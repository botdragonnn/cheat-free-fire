#pragma once
#include <string>
#include <vector>
#include <Utils/Utils.hpp>
#include <Main/Offsets/Offsets.hpp>
#include <Windows.h>

struct PGMPAGEMAPLOCK
{
	uintptr_t pvPage;
	uintptr_t pvMap;
};

// Log diagnostico de eventos raros (clears de snapshot, restart, CR3 renovada).
// Apenda com timestamp em %TEMP%\HwMon.log. Usado para caçar o bug
// "ESP some no meio da partida" — o usuario pode apagar o arquivo depois.
void DiagLog( const char* fmt, ... );

class Memory;

class PageMapping
{
	public:
	PageMapping( ) = default;
	~PageMapping( )
	{
		Release( );
	}

	PageMapping( const PageMapping& ) = delete;
	PageMapping& operator=( const PageMapping& ) = delete;

	PageMapping( PageMapping&& o ) noexcept
		: m_pageBase( o.m_pageBase ), m_hostPtr( o.m_hostPtr ),
		m_lock( o.m_lock ), m_valid( o.m_valid ), m_writable( o.m_writable )
	{
		o.m_valid = false;
	}

	const void* ResolveRead( uint64_t GCPhys );
	void* ResolveWrite( uint64_t GCPhys );
	void        Release( );

	bool ReadArray( uint64_t GCPhys, void* out, size_t size );
	bool WriteArray( uint64_t GCPhys, const void* src, size_t size );

	template<typename T>
	T Read( uint64_t GCPhys )
	{
		const void* p = ResolveRead( GCPhys );
		if ( !p ) return T{ };
		T val;
		memcpy( &val, p, sizeof( T ) );
		return val;
	}

	template<typename T>
	bool Read( uint64_t GCPhys, T& out )
	{
		const void* p = ResolveRead( GCPhys );
		if ( !p ) return false;
		memcpy( &out, p, sizeof( T ) );
		return true;
	}

	template<typename T>
	bool Write( uint64_t GCPhys, const T& val )
	{
		void* p = ResolveWrite( GCPhys );
		if ( !p ) return false;
		memcpy( p, &val, sizeof( T ) );
		return true;
	}

	private:
	uint64_t        m_pageBase = 0;
	const void* m_hostPtr = nullptr;
	PGMPAGEMAPLOCK  m_lock = { };
	bool            m_valid = false;
	bool            m_writable = false;
};

class Memory
{
	public:
	static bool Initialize( );
	static bool Restart( );
	static bool RestartAsync( );

	// Revalida o CR3 do processo do jogo (task walk leve e periodico).
	// Se o processo reiniciou/mudou de pgd no meio da sessao, recaptura o
	// novo CR3 e invalida os TLBs. Retorna true se mudou.
	static bool RefreshCR3( );

	// Flush thread-local TLB (1x por frame no ESP, 1x por burst no Silent)
	static void FlushTLB( );
	// Flush global — invalida TODAS as threads (Restart)
	static void FlushAllTLB( );

	static bool Read( uintptr_t Address, void* OutValue, size_t Size )
	{
		return ReadBuffer( Address, OutValue, Size );
	}

	template<typename T>
	static bool Read( uintptr_t Address, T& OutValue )
	{
		return ReadBuffer( Address, &OutValue, sizeof( T ) );
	}

	template<typename T>
	static T Read( uintptr_t Address )
	{
		T Value{ };
		ReadBuffer( Address, &Value, sizeof( T ) );
		return Value;
	}

	template<typename T>
	static bool Write( uintptr_t Address, const T& Value )
	{
		return WriteBuffer( Address, &Value, sizeof( T ) );
	}

	static std::string String( uintptr_t Address, int MaxLength = 32 );

	static bool TranslateVA( uintptr_t guestVA, uintptr_t& physOut )
	{
		return CachedCR3Convert( guestVA, physOut );
	}

	static void* GetVM( )
	{
		return VmInstancePtr;
	}

	// Function pointer types
	using PGMR3PhysReadExternalType = int( __cdecl* )( void* pVM, uintptr_t GCPhys, void* pvBuf, size_t cbRead );
	using PGMR3PhysWriteExternalType = int( __cdecl* )( void* pVM, uintptr_t GCPhys, void* pvBuf, size_t cbWrite );
	using PGMR3PhysTlbGCPhys2PtrType = int( __cdecl* )( void* pVM, uintptr_t GCPhys, bool fWritable, void** ppv );
	using PGMPhysGCPtr2GCPhysType = int( __cdecl* )( void* pVCpu, uintptr_t GCPtr, uintptr_t* pGCPhys );
	using VMMGetCpuByIdType = void* ( __cdecl* )( void* pVM, int idCpu );
	using GCPhys2CCPtrROExternalType = int( __cdecl* )( void* pVM, uint64_t GCPhys, const void** ppv, PGMPAGEMAPLOCK* pLock );
	using GCPhys2CCPtrExternalType = int( __cdecl* )( void* pVM, uint64_t GCPhys, void** ppv, PGMPAGEMAPLOCK* pLock );
	using ReleasePageMappingLockType = void( __cdecl* )( void* pVM, PGMPAGEMAPLOCK* pLock );

	static PGMR3PhysReadExternalType  PGMR3PhysRead;
	static PGMR3PhysWriteExternalType PGMR3PhysWrite;
	static GCPhys2CCPtrROExternalType GCPhys2CCPtrRO;
	static GCPhys2CCPtrExternalType   GCPhys2CCPtr;
	static ReleasePageMappingLockType ReleaseLock;

	private:
	static PGMR3PhysTlbGCPhys2PtrType PhysTlbToPtr;
	static PGMPhysGCPtr2GCPhysType    PGMPhysGCPtr;
	static VMMGetCpuByIdType          VMMGetCpu;

	// ============================================================
	// SoftTLB — per-thread VA→PA cache via TlsAlloc (manual mapper safe)
	//
	// thread_local crasheia com manual mapper porque o CRT não
	// inicializa os TLS slots sem DllMain DLL_PROCESS_ATTACH.
	// TlsAlloc/TlsGetValue/TlsSetValue são Win32 API puras,
	// funcionam independente de como a DLL foi carregada.
	// ============================================================
	static constexpr size_t TLB_SIZE = 128;

	struct TLBEntry
	{
		uintptr_t vpn;
		uintptr_t ppn;
		LONG      gen;
	};

	// Per-thread data — alocado via HeapAlloc no primeiro acesso
	struct ThreadTLB
	{
		TLBEntry entries [ TLB_SIZE ];
		LONGLONG localGen;
	};

	static volatile LONG s_GlobalGen;
	static volatile LONG s_RestartPending;
	static volatile LONGLONG s_LastRestartTick;
	static DWORD         s_TlsIndex;     // TlsAlloc index

	// Retorna o TLB da thread atual (aloca se necessário)
	static ThreadTLB* GetThreadTLB( );

	static void* VmInstancePtr;
	static uintptr_t GuestCR3;

	static bool CaptureVmInstancePtr( );

	static bool ReadBuffer( uintptr_t Address, void* Buffer, size_t Size );
	static bool WriteBuffer( uintptr_t Address, const void* Buffer, size_t Size );

	static bool Translate32( uintptr_t GuestVA, uintptr_t CR3, uintptr_t& GuestPA );
	static bool Translate64( uintptr_t GuestVA, uintptr_t CR3, uintptr_t& GuestPA );
	static bool ConvertCR3( uintptr_t GuestVA, uintptr_t CR3, uintptr_t& GuestPA );
	static bool ConvertGCPtr( uintptr_t GuestVA, uintptr_t& GuestPA );
	static bool CachedCR3Convert( uintptr_t GuestVA, uintptr_t& GuestPA );

	static std::vector<uintptr_t> GetModuleAddress( bool N32 );

	template<typename T>
	static T ReadPA( uintptr_t address )
	{
		T result{ };
		PGMR3PhysRead( VmInstancePtr, address, &result, sizeof( T ) );
		return result;
	}

	static void ReadPhysical( uintptr_t addr, void* out, size_t size )
	{
		PGMR3PhysRead( VmInstancePtr, addr, out, size );
	}

	template<typename T>
	static T ReadVirtual( uintptr_t address )
	{
		T result{ };
		uintptr_t physAddress;
		if ( ConvertGCPtr( address, physAddress ) )
			PGMR3PhysRead( VmInstancePtr, physAddress, &result, sizeof( T ) );
		return result;
	}

	static void ReadVirtualBuffer( uintptr_t addr, void* out, size_t size )
	{
		uintptr_t physAddress;
		if ( ConvertGCPtr( addr, physAddress ) )
			PGMR3PhysRead( VmInstancePtr, physAddress, out, size );
	}
};

extern Memory g_FreeFireMemory;