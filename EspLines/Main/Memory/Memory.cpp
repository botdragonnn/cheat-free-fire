#include "Memory.hpp"
#include <Notify/Notify.hpp>
#include "EmulatorEnvironment.hpp"
#include <Cheat/Globals.hpp>
#include <Main/Unity/UTF/UTF8.hpp>
#include <Main/Draw/Draw.hpp>
#include <thread>
#include <cstdarg>
#include <XorStr.hpp>
#include <Lazyimporter.hpp>
#include <VehPGDHook/Vehpageguardhook.hpp>

#define LI_RESOLVE(name, type, mod) \
    FrameWork::LazyImporter::li::detail::lazy_function< \
        LAZY_IMPORTER_KHASH(#name), void(*)()>().template in_safe<type>(mod)

bool CanCheckHook = false;

// Anti-tamper NUNCA pode derrubar o jogo. Antes, um byte de prologo divergente
// (ex.: emulador atualizado / build nova do VBox com prologo diferente) fazia
// __ud2() — instrucao ilegal que nenhum VEH trata (só SINGLE_STEP) → crash do
// processo inteiro ao injetar / no meio da partida. Agora loga 1x e segue em
// modo degradado: se os ponteiros forem realmente invalidos, as leituras
// falham sozinhas (ESP congela) sem derrubar o jogo.
#define VALIDATE()                                          \
    if (CanCheckHook) {                                     \
        static bool s_HookMismatchLogged = false;           \
        if (!s_HookMismatchLogged) {                        \
            BYTE b1 = ( PGMR3PhysRead ) ? *( BYTE* )PGMR3PhysRead : 0xFF; \
            BYTE b2 = ( PGMPhysGCPtr ) ? *( BYTE* )PGMPhysGCPtr : 0xFF; \
            BYTE b3 = ( GCPhys2CCPtrRO ) ? *( BYTE* )GCPhys2CCPtrRO : 0xFF; \
            BYTE b4 = ( GCPhys2CCPtr ) ? *( BYTE* )GCPhys2CCPtr : 0xFF; \
            if ( b1 != 0x48 || b2 != 0x48 || b3 != 0x48 || b4 != 0x48 ) { \
                s_HookMismatchLogged = true;                \
                CanCheckHook = false;                       \
                DiagLog( "[diag] VALIDATE: prologos VMM divergentes (%02x %02x %02x %02x) — seguindo degradado", b1, b2, b3, b4 ); \
            }                                               \
        }                                                   \
    }

// ============================================================
// Static members
// ============================================================
void* Memory::VmInstancePtr = nullptr;
uintptr_t Memory::GuestCR3 = 0;

Memory::PGMR3PhysReadExternalType  Memory::PGMR3PhysRead = nullptr;
Memory::PGMR3PhysWriteExternalType Memory::PGMR3PhysWrite = nullptr;
Memory::PGMR3PhysTlbGCPhys2PtrType Memory::PhysTlbToPtr = nullptr;
Memory::PGMPhysGCPtr2GCPhysType    Memory::PGMPhysGCPtr = nullptr;
Memory::VMMGetCpuByIdType          Memory::VMMGetCpu = nullptr;
Memory::GCPhys2CCPtrROExternalType Memory::GCPhys2CCPtrRO = nullptr;
Memory::GCPhys2CCPtrExternalType   Memory::GCPhys2CCPtr = nullptr;
Memory::ReleasePageMappingLockType Memory::ReleaseLock = nullptr;

volatile LONG Memory::s_GlobalGen = 0;
volatile LONG Memory::s_RestartPending = 0;
volatile LONGLONG Memory::s_LastRestartTick = 0;
DWORD         Memory::s_TlsIndex = TLS_OUT_OF_INDEXES;

// ============================================================
// DiagLog — log diagnostico em %TEMP%\HwMon.log
// ============================================================
void DiagLog( const char* fmt, ... )
{
	char buf [ 512 ];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, args );
	va_end( args );

	char tmp [ MAX_PATH ];
	if ( !GetTempPathA( MAX_PATH, tmp ) ) return;

	char path [ MAX_PATH ];
	sprintf_s( path, "%sHwMon.log", tmp );

	HANDLE h = CreateFileA( path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( h == INVALID_HANDLE_VALUE ) return;

	if ( GetFileSize( h, nullptr ) > 256 * 1024 )
	{
		// Mantem o arquivo pequeno: trunca no primeiro 128KB.
		CloseHandle( h );
		h = CreateFileA( path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
		if ( h == INVALID_HANDLE_VALUE ) return;
	}

	char line [ 640 ];
	int n = sprintf_s( line, "[%llu] %s\r\n", ( unsigned long long )GetTickCount64( ), buf );
	DWORD written = 0;
	if ( n > 0 ) WriteFile( h, line, ( DWORD )n, &written, nullptr );
	CloseHandle( h );
}

// ============================================================
// SoftTLB — dynamic TLS (manual mapper safe)
// ============================================================

Memory::ThreadTLB* Memory::GetThreadTLB( )
{
	// s_TlsIndex deve ter sido alocado no Initialize
	if ( s_TlsIndex == TLS_OUT_OF_INDEXES )
		return nullptr;

	auto* tlb = ( ThreadTLB* )TlsGetValue( s_TlsIndex );
	if ( !tlb )
	{
		// Primeiro acesso nessa thread — aloca via HeapAlloc (sem CRT)
		tlb = ( ThreadTLB* )HeapAlloc( GetProcessHeap( ), HEAP_ZERO_MEMORY, sizeof( ThreadTLB ) );
		if ( !tlb ) return nullptr;
		TlsSetValue( s_TlsIndex, tlb );
	}
	return tlb;
}

void Memory::FlushTLB( )
{
	ThreadTLB* tlb = GetThreadTLB( );
	if ( !tlb ) return;
	memset( tlb->entries, 0, sizeof( tlb->entries ) );
	tlb->localGen = InterlockedCompareExchange( &s_GlobalGen, 0, 0 );
}

void Memory::FlushAllTLB( )
{
	InterlockedIncrement( &s_GlobalGen );
}

// ============================================================
// Linux kernel offsets
// ============================================================
static uintptr_t KVA_INIT_TASK_64 = 0x0;
static uintptr_t OFFSET_TASKS_64 = 0x470;
static uintptr_t OFFSET_COMM_64 = 0x720;
static uintptr_t OFFSET_MM_64 = 0x4C0;
static uintptr_t OFFSET_MM_PGD_64 = 0x50;
static uintptr_t OFFSET_VMA_END_64 = 0x8;
static uintptr_t OFFSET_VMA_NEXT_64 = 0x10;
static uintptr_t OFFSET_VMA_FILE_64 = 0xA0;
static uintptr_t OFFSET_FILE_DENTRY_64 = 0x18;
static uintptr_t OFFSET_DENTRY_NAME_64 = 0x38;

static uint32_t KVA_INIT_TASK_32 = 0x0;
static uint32_t OFFSET_TASKS_32 = 0x2F4;
static uint32_t OFFSET_COMM_32 = 0x478;
static uint32_t OFFSET_MM_32 = 0x31C;
static uint32_t OFFSET_MM_PGD_32 = 0x20;
static uint32_t OFFSET_VMA_END_32 = 0x4;
static uint32_t OFFSET_VMA_NEXT_32 = 0x8;
static uint32_t OFFSET_VMA_FILE_32 = 0x50;
static uint32_t OFFSET_FILE_DENTRY_32 = 0x0C;
static uint32_t OFFSET_DENTRY_NAME_32 = 0x1C;

// ============================================================
// PageMapping
// ============================================================

const void* PageMapping::ResolveRead( uint64_t GCPhys )
{
	uint64_t page = GCPhys & ~0xFFFULL;
	uint64_t off = GCPhys & 0xFFF;
	if ( m_valid && page == m_pageBase && !m_writable )
		return ( const uint8_t* )m_hostPtr + off;
	Release( );
	void* pVM = Memory::GetVM( );
	if ( !pVM || !Memory::GCPhys2CCPtrRO ) return nullptr;
	const void* ptr = nullptr;
	int rc = Memory::GCPhys2CCPtrRO( pVM, page, &ptr, &m_lock );
	if ( rc < 0 || !ptr ) return nullptr;
	m_pageBase = page;
	m_hostPtr = ptr;
	m_valid = true;
	m_writable = false;
	return ( const uint8_t* )ptr + off;
}

void* PageMapping::ResolveWrite( uint64_t GCPhys )
{
	uint64_t page = GCPhys & ~0xFFFULL;
	uint64_t off = GCPhys & 0xFFF;
	if ( m_valid && page == m_pageBase && m_writable )
		return ( uint8_t* )const_cast< void* >( m_hostPtr ) + off;
	Release( );
	void* pVM = Memory::GetVM( );
	if ( !pVM || !Memory::GCPhys2CCPtr ) return nullptr;
	void* ptr = nullptr;
	int rc = Memory::GCPhys2CCPtr( pVM, page, &ptr, &m_lock );
	if ( rc < 0 || !ptr ) return nullptr;
	m_pageBase = page;
	m_hostPtr = ptr;
	m_valid = true;
	m_writable = true;
	return ( uint8_t* )ptr + off;
}

void PageMapping::Release( )
{
	if ( !m_valid ) return;
	void* pVM = Memory::GetVM( );
	if ( pVM && Memory::ReleaseLock )
		Memory::ReleaseLock( pVM, &m_lock );
	m_valid = false;
	m_hostPtr = nullptr;
	m_pageBase = 0;
	m_writable = false;
	m_lock = { };
}

bool PageMapping::ReadArray( uint64_t GCPhys, void* out, size_t size )
{
	uint8_t* dst = ( uint8_t* )out;
	while ( size > 0 )
	{
		size_t chunkSize = 0x1000 - ( size_t )( GCPhys & 0xFFF );
		if ( chunkSize > size ) chunkSize = size;
		const void* src = ResolveRead( GCPhys );
		if ( !src ) return false;
		memcpy( dst, src, chunkSize );
		dst += chunkSize;
		GCPhys += chunkSize;
		size -= chunkSize;
	}
	return true;
}

bool PageMapping::WriteArray( uint64_t GCPhys, const void* src, size_t size )
{
	const uint8_t* s = ( const uint8_t* )src;
	while ( size > 0 )
	{
		size_t chunkSize = 0x1000 - ( size_t )( GCPhys & 0xFFF );
		if ( chunkSize > size ) chunkSize = size;
		void* dst = ResolveWrite( GCPhys );
		if ( !dst ) return false;
		memcpy( dst, s, chunkSize );
		s += chunkSize;
		GCPhys += chunkSize;
		size -= chunkSize;
	}
	return true;
}

// ============================================================
// ReadBuffer — fallback chain: CCPtr → TlbToPtr → PGMR3PhysRead
// ============================================================
bool Memory::ReadBuffer( uintptr_t address, void* buffer, size_t size )
{
	if ( !VmInstancePtr || !PGMR3PhysRead || !buffer || size == 0 )
		return false;

	VALIDATE( );

	uint8_t* out = static_cast< uint8_t* >( buffer );
	size_t remaining = size;
	PageMapping map;

	while ( remaining > 0 )
	{
		uintptr_t physAddr;

		if ( GuestCR3 != 0 )
		{
			if ( !CachedCR3Convert( address, physAddr ) )
				return false;
		}
		else
		{
			if ( !ConvertGCPtr( address, physAddr ) )
				return false;
		}

		size_t pageOffset = address & 0xFFF;
		size_t toRead = min( remaining, 0x1000 - pageOffset );

		if ( GCPhys2CCPtrRO )
		{
			const void* hostPtr = map.ResolveRead( physAddr );
			if ( hostPtr )
			{
				memcpy( out, hostPtr, toRead );
			}
			else if ( PGMR3PhysRead( VmInstancePtr, physAddr, out, toRead ) != 0 )
			{
				return false;
			}
		}
		else if ( PhysTlbToPtr )
		{
			void* hostPtr = nullptr;
			int rc = PhysTlbToPtr( VmInstancePtr, physAddr, false, &hostPtr );
			if ( rc >= 0 && hostPtr )
				memcpy( out, hostPtr, toRead );
			else if ( PGMR3PhysRead( VmInstancePtr, physAddr, out, toRead ) != 0 )
				return false;
		}
		else
		{
			if ( PGMR3PhysRead( VmInstancePtr, physAddr, out, toRead ) != 0 )
				return false;
		}

		remaining -= toRead;
		address += toRead;
		out += toRead;
	}
	return true;
}

// ============================================================
// WriteBuffer — fallback chain: CCPtr → TlbToPtr → PGMR3PhysWrite
// ============================================================
bool Memory::WriteBuffer( uintptr_t address, const void* buffer, size_t size )
{
	if ( !VmInstancePtr || !PGMR3PhysWrite || !buffer || size == 0 )
		return false;

	VALIDATE( );

	const uint8_t* in = static_cast< const uint8_t* >( buffer );
	size_t remaining = size;
	PageMapping map;

	while ( remaining > 0 )
	{
		uintptr_t physAddr;

		if ( GuestCR3 != 0 )
		{
			if ( !CachedCR3Convert( address, physAddr ) )
				return false;
		}
		else
		{
			if ( !ConvertGCPtr( address, physAddr ) )
				return false;
		}

		size_t pageOffset = address & 0xFFF;
		size_t toWrite = min( remaining, 0x1000 - pageOffset );

		if ( GCPhys2CCPtr )
		{
			void* hostPtr = map.ResolveWrite( physAddr );
			if ( hostPtr )
			{
				memcpy( hostPtr, in, toWrite );
			}
			else if ( PGMR3PhysWrite( VmInstancePtr, physAddr, ( void* )in, toWrite ) != 0 )
			{
				return false;
			}
		}
		else if ( PhysTlbToPtr )
		{
			void* hostPtr = nullptr;
			int rc = PhysTlbToPtr( VmInstancePtr, physAddr, true, &hostPtr );
			if ( rc >= 0 && hostPtr )
				memcpy( hostPtr, in, toWrite );
			else if ( PGMR3PhysWrite( VmInstancePtr, physAddr, ( void* )in, toWrite ) != 0 )
				return false;
		}
		else
		{
			if ( PGMR3PhysWrite( VmInstancePtr, physAddr, ( void* )in, toWrite ) != 0 )
				return false;
		}

		remaining -= toWrite;
		address += toWrite;
		in += toWrite;
	}
	return true;
}

// ============================================================
// Page table walks
// ============================================================

bool Memory::Translate32( uintptr_t GuestVA, uintptr_t CR3, uintptr_t& GuestPA )
{
	GuestPA = 0;
	uint32_t PD_index = ( GuestVA >> 22 ) & 0x3FF;
	uint32_t PT_index = ( GuestVA >> 12 ) & 0x3FF;
	uint32_t page_offset = GuestVA & 0xFFF;

	uint32_t pd_entry = 0;
	if ( PGMR3PhysRead( VmInstancePtr, CR3 + PD_index * 4, &pd_entry, sizeof( pd_entry ) ) != 0 ) return false;
	if ( !( pd_entry & 1 ) ) return false;

	if ( pd_entry & ( 1 << 7 ) )
	{
		GuestPA = ( pd_entry & 0xFFC00000U ) + ( GuestVA & 0x3FFFFF );
		return true;
	}

	uint32_t pt_entry = 0;
	uintptr_t pt_base = static_cast< uintptr_t >( pd_entry & 0xFFFFF000U );
	if ( PGMR3PhysRead( VmInstancePtr, pt_base + PT_index * 4, &pt_entry, sizeof( pt_entry ) ) != 0 ) return false;
	if ( !( pt_entry & 1 ) ) return false;

	GuestPA = static_cast< uintptr_t >( pt_entry & 0xFFFFF000U ) + page_offset;
	return true;
}

bool Memory::Translate64( uintptr_t GuestVA, uintptr_t CR3, uintptr_t& GuestPA )
{
	GuestPA = 0;
	if ( CR3 < 0x1000 ) return false;

	const uintptr_t pml4_base = CR3 & 0xFFFFFFFFFFFFF000ULL;
	uint64_t PML4_index = ( GuestVA >> 39 ) & 0x1FF;
	uint64_t PDPT_index = ( GuestVA >> 30 ) & 0x1FF;
	uint64_t PD_index = ( GuestVA >> 21 ) & 0x1FF;
	uint64_t PT_index = ( GuestVA >> 12 ) & 0x1FF;
	uint64_t page_off = GuestVA & 0xFFF;
	uint64_t entry = 0;
	uintptr_t table = 0;

	if ( PGMR3PhysRead( VmInstancePtr, pml4_base + PML4_index * 8, &entry, sizeof( entry ) ) != 0 ) return false;
	if ( !( entry & 1 ) ) return false;
	table = static_cast< uintptr_t >( entry & 0xFFFFFFFFF000ULL );

	if ( PGMR3PhysRead( VmInstancePtr, table + PDPT_index * 8, &entry, sizeof( entry ) ) != 0 ) return false;
	if ( !( entry & 1 ) ) return false;
	if ( entry & ( 1ULL << 7 ) )
	{
		GuestPA = static_cast< uintptr_t >( entry & 0xFFFFFC0000000ULL ) + ( GuestVA & 0x3FFFFFFFULL );
		return true;
	}
	table = static_cast< uintptr_t >( entry & 0xFFFFFFFFF000ULL );

	if ( PGMR3PhysRead( VmInstancePtr, table + PD_index * 8, &entry, sizeof( entry ) ) != 0 ) return false;
	if ( !( entry & 1 ) ) return false;
	if ( entry & ( 1ULL << 7 ) )
	{
		GuestPA = static_cast< uintptr_t >( entry & 0xFFFFFFE00000ULL ) + ( GuestVA & 0x1FFFFFULL );
		return true;
	}
	table = static_cast< uintptr_t >( entry & 0xFFFFFFFFF000ULL );

	if ( PGMR3PhysRead( VmInstancePtr, table + PT_index * 8, &entry, sizeof( entry ) ) != 0 ) return false;
	if ( !( entry & 1 ) ) return false;

	GuestPA = static_cast< uintptr_t >( entry & 0x000FFFFFFFFFF000ULL ) + page_off;
	return true;
}

// Dispatch por ABI
bool Memory::ConvertCR3( uintptr_t GuestVA, uintptr_t CR3, uintptr_t& GuestPA )
{
	GuestPA = 0;
	if ( !VmInstancePtr || GuestVA <= 0x1000 )
		return false;

	ABIType abi = GetEmulatorEnv( ).Abi( );
	switch ( abi )
	{
		case ABIType::X86: 
			return Translate32( GuestVA, CR3, GuestPA );
		case ABIType::X86_64:
			return Translate64( GuestVA, CR3, GuestPA );
		default: return false;
	}
}

bool Memory::ConvertGCPtr( uintptr_t GuestVA, uintptr_t& GuestPA )
{
	GuestPA = 0;
	if ( !VmInstancePtr || !VMMGetCpu || !PGMPhysGCPtr || GuestVA <= 0x1000 )
		return false;

	for ( int i = 0; i < 4; ++i )
	{
		void* cpu = VMMGetCpu( VmInstancePtr, i );
		if ( !cpu ) continue;
		uintptr_t temp = 0;
		if ( PGMPhysGCPtr( cpu, GuestVA, &temp ) == 0 && temp != 0 )
		{
			GuestPA = temp;
			return true;
		}
	}
	return false;
}

// ============================================================
// CachedCR3Convert — SoftTLB via TlsAlloc (manual mapper safe)
// ============================================================
bool Memory::CachedCR3Convert( uintptr_t GuestVA, uintptr_t& GuestPA )
{
	GuestPA = 0;
	if ( GuestVA <= 0x1000 || GuestCR3 == 0 )
		return false;

	ThreadTLB* tlb = GetThreadTLB( );

	// Se TLS falhou (TlsAlloc não rodou, ou HeapAlloc falhou)
	// cai direto pro page table walk sem cache
	if ( !tlb )
		return ConvertCR3( GuestVA, GuestCR3, GuestPA );

	// Check geração global → auto-flush se divergiu
	LONG curGen = InterlockedCompareExchange( &s_GlobalGen, 0, 0 );
	if ( tlb->localGen != curGen )
	{
		memset( tlb->entries, 0, sizeof( tlb->entries ) );
		tlb->localGen = curGen;
	}

	uintptr_t vpn = GuestVA >> 12;
	uint32_t slot = static_cast< uint32_t >( vpn % TLB_SIZE );

	// TLB hit
	if ( tlb->entries [ slot ].vpn == vpn && tlb->entries [ slot ].ppn != 0 && tlb->entries [ slot ].gen == curGen )
	{
		GuestPA = tlb->entries [ slot ].ppn | ( GuestVA & 0xFFF );
		return true;
	}

	// TLB miss → full page table walk
	if ( !ConvertCR3( GuestVA, GuestCR3, GuestPA ) )
		return false;

	// Insere no TLB
	tlb->entries [ slot ].vpn = vpn;
	tlb->entries [ slot ].ppn = GuestPA & ~0xFFFULL;
	tlb->entries [ slot ].gen = curGen;

	return true;
}

// ============================================================
// String
// ============================================================
std::string Memory::String( uintptr_t Address, int MaxLength )
{
	if ( !Address || MaxLength <= 0 ) return "";

	constexpr int BLOCK_SIZE = 128;
	std::string Output;
	Output.reserve( MaxLength );
	int Remaining = MaxLength;
	uintptr_t Current = Address;

	while ( Remaining > 0 )
	{
		char block [ BLOCK_SIZE ];
		int toRead = min( Remaining, BLOCK_SIZE );

		if ( !ReadBuffer( Current, block, toRead ) )
			break;

		for ( int i = 0; i < toRead; i++ )
		{
			if ( block [ i ] == '\0' ) return Output;
			Output.push_back( block [ i ] );
		}

		Remaining -= toRead;
		Current += toRead;
	}
	return Output;
}

// ============================================================
// RefreshCR3 — revalidacao periodica do CR3 (task walk leve)
// ============================================================
bool Memory::RefreshCR3( )
{
	if ( !VmInstancePtr ) return false;

	auto& env = GetEmulatorEnv( );
	bool N32 = ( env.Abi( ) == ABIType::X86 );

	auto xs_proc = xorstr( "com.dts.freefir" );
	const char* processBaseName = xs_proc.crypt_get( );

	const auto ProcessName = [ ] ( const char* comm, const char* pattern ) -> bool
	{
		if ( !comm || !pattern ) return false;
		if ( std::strstr( comm, pattern ) != nullptr ) return true;
		const char* core = pattern;
		if ( std::strncmp( core, "com.", 4 ) == 0 ) core += 4;
		const char* c2 = comm;
		if ( *c2 == '.' ) ++c2;
		return ( std::strstr( c2, core ) != nullptr );
	};

	// Mesmo padrao do GetModuleAddress — chain de task_structs em memoria
	// fisica, sem depender do CR3 (que e exatamente o que estamos revalidando).
	auto ReadPtr = [ N32 ] ( uintptr_t a ) -> uintptr_t {
		return N32 ? ReadPA<uint32_t>( a ) : ReadPA<uint64_t>( a );
	};
	auto BadPtr = [ N32 ] ( uintptr_t v ) -> bool {
		return !v || ( N32 ? ( uint32_t )v == UINT32_MAX : v == UINT64_MAX );
	};

	size_t OFF_TASKS = N32 ? OFFSET_TASKS_32 : OFFSET_TASKS_64;
	size_t OFF_COMM = N32 ? OFFSET_COMM_32 : OFFSET_COMM_64;
	size_t OFF_MM = N32 ? OFFSET_MM_32 : OFFSET_MM_64;
	size_t OFF_MM_PGD = N32 ? OFFSET_MM_PGD_32 : OFFSET_MM_PGD_64;
	uintptr_t KVA_INIT = N32 ? ( uintptr_t )KVA_INIT_TASK_32 : KVA_INIT_TASK_64;

	uintptr_t init_task_phys = 0;
	if ( !ConvertGCPtr( KVA_INIT, init_task_phys ) || init_task_phys == 0 )
		return false;

	uintptr_t current_phys = init_task_phys;
	do
	{
		char comm [ 16 ] = { 0 };
		ReadPhysical( current_phys + OFF_COMM, comm, 15 );
		comm [ 15 ] = '\0';
		if ( ProcessName( comm, processBaseName ) )
		{
			uintptr_t mm = ReadPtr( current_phys + OFF_MM );
			if ( BadPtr( mm ) ) return false;

			uintptr_t pgd_va = N32 ? ( uintptr_t )ReadVirtual<uint32_t>( mm + OFF_MM_PGD )
			                       : ReadVirtual<uintptr_t>( mm + OFF_MM_PGD );
			if ( BadPtr( pgd_va ) ) return false;

			uintptr_t pgd_phys = 0;
			if ( !ConvertGCPtr( pgd_va, pgd_phys ) || pgd_phys == 0 )
				return false;

			if ( pgd_phys == GuestCR3 )
				return false;

			// CR3 mudou (processo reiniciou / trocou de pgd): recaptura e
			// invalida os TLBs de todas as threads — a proxima leitura em
			// qualquer thread re-caminha na tabela nova.
			GuestCR3 = pgd_phys;
			FlushAllTLB( );
			Console::LogHex( XorStr( "[Memory] CR3 renovada: " ), pgd_phys );
			DiagLog( "[diag] CR3 renovada: %llx", ( unsigned long long )pgd_phys );
			return true;
		}

		uintptr_t next_kva = ReadPtr( current_phys + OFF_TASKS );
		uintptr_t next_list_phys = 0;
		if ( !ConvertGCPtr( next_kva, next_list_phys ) || next_list_phys == 0 )
			return false;
		current_phys = next_list_phys - OFF_TASKS;
	}
	while ( current_phys != init_task_phys );

	return false;
}

// ============================================================
// GetModuleAddress (bootstrap — usa PhysRead direto, ternary N32)
// ============================================================
std::vector<uintptr_t> Memory::GetModuleAddress( bool N32 )
{
	std::vector<uintptr_t> results;

	if ( !VmInstancePtr )
	{
		Console::Log( XorStr( "[FindLibraryBase] Memory nao inicializado (VmInstancePtr nulo)." ) );
		return results;
	}

	auto xs_proc = xorstr( "com.dts.freefir" );
	const char* processBaseName = xs_proc.crypt_get( );
	auto xs_module = xorstr( "libil2cpp.so" );
	const char* ModuleName = xs_module.crypt_get( );

	const auto ProcessName = [ ] ( const char* comm, const char* pattern ) -> bool
	{
		if ( !comm || !pattern ) return false;
		if ( std::strstr( comm, pattern ) != nullptr ) return true;
		const char* core = pattern;
		if ( std::strncmp( core, "com.", 4 ) == 0 ) core += 4;
		const char* c2 = comm;
		if ( *c2 == '.' ) ++c2;
		return ( std::strstr( c2, core ) != nullptr );
	};

	// Ternary lambdas — um unico bloco para 32/64
	auto ReadPtr = [ N32 ] ( uintptr_t a ) -> uintptr_t {
		return N32 ? ReadPA<uint32_t>( a ) : ReadPA<uint64_t>( a );
	};
	auto BadPtr = [ N32 ] ( uintptr_t v ) -> bool {
		return !v || ( N32 ? ( uint32_t )v == UINT32_MAX : v == UINT64_MAX );
	};

	size_t OFF_TASKS = N32 ? OFFSET_TASKS_32 : OFFSET_TASKS_64;
	size_t OFF_COMM = N32 ? OFFSET_COMM_32 : OFFSET_COMM_64;
	size_t OFF_MM = N32 ? OFFSET_MM_32 : OFFSET_MM_64;
	size_t OFF_MM_PGD  = N32 ? OFFSET_MM_PGD_32 : OFFSET_MM_PGD_64;
	size_t OFF_VMA_END  = N32 ? OFFSET_VMA_END_32 : OFFSET_VMA_END_64;
	size_t OFF_VMA_NEXT = N32 ? OFFSET_VMA_NEXT_32 : OFFSET_VMA_NEXT_64;
	size_t OFF_VMA_FILE = N32 ? OFFSET_VMA_FILE_32 : OFFSET_VMA_FILE_64;
	size_t OFF_FILE_DENTRY = N32 ? OFFSET_FILE_DENTRY_32 : OFFSET_FILE_DENTRY_64;
	size_t OFF_DENTRY_NAME = N32 ? OFFSET_DENTRY_NAME_32 : OFFSET_DENTRY_NAME_64;
	uintptr_t KVA_INIT = N32 ? ( uintptr_t )KVA_INIT_TASK_32 : KVA_INIT_TASK_64;

	uintptr_t init_task_phys = 0;
	ConvertGCPtr( KVA_INIT, init_task_phys );
	uintptr_t current_phys = init_task_phys;
	uintptr_t target_task = 0;
	do
	{
		char comm [ 16 ] = { 0 };
		ReadPhysical( current_phys + OFF_COMM, comm, 15 );
		comm [ 15 ] = '\0';
		if ( ProcessName( comm, processBaseName ) )
		{
			target_task = current_phys; break;
		}
		uintptr_t next_kva = ReadPtr( current_phys + OFF_TASKS );
		uintptr_t next_list_phys = 0;
		ConvertGCPtr( next_kva, next_list_phys );
		current_phys = next_list_phys - OFF_TASKS;
	}
	while ( current_phys != init_task_phys );

	if ( !target_task )
	{
		std::printf( XorStr( "[GetModuleAddress] process not found\n" ) );
		return results;
	}

	uintptr_t mm = ReadPtr( target_task + OFF_MM );
	uintptr_t pgd_va = N32 ? ( uintptr_t )ReadVirtual<uint32_t>( mm + OFF_MM_PGD ) : ReadVirtual<uintptr_t>( mm + OFF_MM_PGD );
	if ( !BadPtr( pgd_va ) )
	{
		uintptr_t pgd_phys = 0;
		if ( ConvertGCPtr( pgd_va, pgd_phys ) )
		{
			GuestCR3 = pgd_phys;
			std::printf( XorStr( "[GetModuleAddress] pgd_va=%p pgd_phys=%p\n" ), ( void* )pgd_va, ( void* )pgd_phys );
		}
	}

	uintptr_t mmap = N32 ? ( uintptr_t )ReadVirtual<uint32_t>( mm ) : ReadVirtual<uintptr_t>( mm );
	uintptr_t mmap_phys = 0;
	ConvertGCPtr( mmap, mmap_phys );
	uintptr_t vma_phys = mmap_phys;

	while ( vma_phys )
	{
		uintptr_t start = ReadPtr( vma_phys );
		uintptr_t next  = ReadPtr( vma_phys + OFF_VMA_NEXT );
		uintptr_t vm_file_va = ReadPtr( vma_phys + OFF_VMA_FILE );

		if ( !BadPtr( vm_file_va ) )
		{
			uintptr_t dentry = N32 ? ( uintptr_t )ReadVirtual<uint32_t>( vm_file_va + OFF_FILE_DENTRY ) : ReadVirtual<uintptr_t>( vm_file_va + OFF_FILE_DENTRY );
			if ( !BadPtr( dentry ) )
			{
				char name [ 64 ] = { 0 };
				if ( N32 )
				{
					uint32_t Name_va = ReadVirtual<uint32_t>( dentry + OFF_DENTRY_NAME );
					if ( Name_va && Name_va != 0xFFFFFFFF )
						ReadVirtualBuffer( Name_va, name, 48 );
				}
				else
				{
					ReadVirtualBuffer( dentry + OFF_DENTRY_NAME, name, 48 );
				}

				if ( std::strstr( name, ModuleName ) != nullptr )
				{
					std::printf( XorStr( "[GetModuleAddress] FOUND %s at %p\n" ), ModuleName, ( void* )start );
					results.push_back( start );
				}
			}
		}
		if ( !next ) break;
		ConvertGCPtr( next, vma_phys );
	}

	if ( results.empty( ) )
		std::printf( XorStr( "[GetModuleAddress] module '%s' not found\n" ), ModuleName );

	return results;
}

// ============================================================
// CaptureVmInstancePtr — VEH page guard hook
// ============================================================
bool Memory::CaptureVmInstancePtr( )
{
	if ( VmInstancePtr != nullptr )
		return true;

	if ( !VEHCapture::Install( ( uintptr_t )PGMR3PhysRead ) )
	{
		Console::Log( XorStr( "[Memory] Failed to install VEH capture on PGMR3PhysReadExternal" ) );
		return false;
	}

	Console::Log( XorStr( "[Memory] VEH capture installed, waiting for PGMR3PhysReadExternal call..." ) );

	while ( !VEHCapture::WasCaptured( ) )
		Sleep( 1 );

	VEHCapture::Remove( );

	VmInstancePtr = VEHCapture::GetCaptured( );
	Console::LogHex( XorStr( "[Memory] VmInstancePtr captured via VEH: " ), ( uintptr_t )VmInstancePtr );
	return VmInstancePtr != nullptr;
}

// ============================================================
// Initialize
// ============================================================
bool Memory::Initialize( )
{
	HMODULE bstkVMM = xorstr( "BstkVMM.dll" ).use( [ ] ( const char* s )
	{
		return lzimpLI_FN( GetModuleHandleA ).get<decltype( &GetModuleHandleA )>( )( s );
	} );
	if ( !bstkVMM )
	{
		Console::Log( XorStr( "[Memory] Error: BstkVMM.dll not found" ) );
		return false;
	}

	// Aloca TLS slot ANTES de qualquer coisa — precisa estar pronto
	// quando as threads começarem a usar CachedCR3Convert
	s_TlsIndex = TlsAlloc( );
	if ( s_TlsIndex == TLS_OUT_OF_INDEXES )
	{
		Console::Log( XorStr( "[Memory] Error: TlsAlloc failed" ) );
		return false;
	}

	PGMR3PhysRead = LI_RESOLVE( PGMR3PhysReadExternal, PGMR3PhysReadExternalType, bstkVMM );
	PGMR3PhysWrite = LI_RESOLVE( PGMR3PhysWriteExternal, PGMR3PhysWriteExternalType, bstkVMM );
	VMMGetCpu = LI_RESOLVE( VMMGetCpuById, VMMGetCpuByIdType, bstkVMM );
	PGMPhysGCPtr = LI_RESOLVE( PGMPhysGCPtr2GCPhys, PGMPhysGCPtr2GCPhysType, bstkVMM );
	PhysTlbToPtr = LI_RESOLVE( PGMR3PhysTlbGCPhys2Ptr, PGMR3PhysTlbGCPhys2PtrType, bstkVMM );

	if ( !PGMR3PhysRead || !PGMR3PhysWrite || !VMMGetCpu || !PGMPhysGCPtr )
	{
		Console::Log( XorStr( "[Memory] Error: Failed to resolve base exports" ) );
		if ( !PGMR3PhysRead )  Console::Log( XorStr( "[Memory]   -> PGMR3PhysReadExternal is NULL" ) );
		if ( !PGMR3PhysWrite ) Console::Log( XorStr( "[Memory]   -> PGMR3PhysWriteExternal is NULL" ) );
		if ( !VMMGetCpu )      Console::Log( XorStr( "[Memory]   -> VMMGetCpuById is NULL" ) );
		if ( !PGMPhysGCPtr )   Console::Log( XorStr( "[Memory]   -> PGMPhysGCPtr2GCPhys is NULL" ) );
		return false;
	}

	GCPhys2CCPtrRO = LI_RESOLVE( PGMR3PhysGCPhys2CCPtrReadOnlyExternal, GCPhys2CCPtrROExternalType, bstkVMM );
	GCPhys2CCPtr = LI_RESOLVE( PGMR3PhysGCPhys2CCPtrExternal, GCPhys2CCPtrExternalType, bstkVMM );
	ReleaseLock = LI_RESOLVE( PGMPhysReleasePageMappingLock, ReleasePageMappingLockType, bstkVMM );

	if ( !GCPhys2CCPtrRO || !GCPhys2CCPtr || !ReleaseLock )
	{
		Console::Log( XorStr( "[Memory] Warning: CCPtr exports not found, fallback to TLB/PhysRead mode" ) );
	}
	else
	{
		Console::Log( XorStr( "[Memory] CCPtr exports resolved" ) );
		Console::LogHex( XorStr( "[Memory] GCPhys2CCPtrRO @ " ), ( uintptr_t )GCPhys2CCPtrRO );
		Console::LogHex( XorStr( "[Memory] GCPhys2CCPtr   @ " ), ( uintptr_t )GCPhys2CCPtr );
		Console::LogHex( XorStr( "[Memory] ReleaseLock    @ " ), ( uintptr_t )ReleaseLock );
	}

	Console::Log( XorStr( "[Memory] Functions resolved successfully" ) );
	Console::LogHex( XorStr( "[Memory] PGMR3PhysRead  @ " ), ( uintptr_t )PGMR3PhysRead );
	Console::LogHex( XorStr( "[Memory] PGMR3PhysWrite @ " ), ( uintptr_t )PGMR3PhysWrite );
	Console::LogHex( XorStr( "[Memory] PhysTlbToPtr   @ " ), ( uintptr_t )PhysTlbToPtr );

	if ( !CaptureVmInstancePtr( ) )
	{
		Console::Log( XorStr( "[Memory] Error: Failed to capture VmInstancePtr" ) );
		return false;
	}

	GetEmulatorEnv( ).Refresh( );

	if ( !KernelOffsetSelector::ApplyInitTaskOffsets( GetEmulatorEnv( ), KVA_INIT_TASK_32, KVA_INIT_TASK_64 ) )
		Console::Log( XorStr( "[Offsets] Nao foi possivel selecionar KVA_INIT_TASK para essa versao." ) );
	else
	{
		Console::LogHex( XorStr( "[Offsets] KVA_INIT_TASK_32 = " ), ( uintptr_t )KVA_INIT_TASK_32 );
		if ( KVA_INIT_TASK_64 )
			Console::LogHex( XorStr( "[Offsets] KVA_INIT_TASK_64 = " ), ( uintptr_t )KVA_INIT_TASK_64 );
	}

	Console::Log( std::string( XorStr( "[ENV] Emulator=" ) ) + GetEmulatorEnv( ).EmulatorName( ) +
		" Version=" + GetEmulatorEnv( ).Version( ) +
		" ABI=" + EmulatorEnvironment::ABIToString( GetEmulatorEnv( ).Abi( ) ) );

	CanCheckHook = true;

	auto& env = GetEmulatorEnv( );
	bool N32 = ( env.Abi( ) == ABIType::X86 );
	auto modules = GetModuleAddress( N32 );
	uintptr_t il2cppBase = modules.empty( ) ? 0 : modules [ 0 ];

	// Passa todas as candidates para o Offsets (GameConfig itera sobre elas)
	Offsets::LibIl2CppCandidates = modules;

	if ( il2cppBase )
	{
		Offsets::LibIl2Cpp = il2cppBase;
		Console::LogHex( XorStr( "[Memory] Il2Cpp located at " ), il2cppBase );
	}
	else
	{
		Console::Log( XorStr( "[Memory] Failed to locate libil2cpp.so by any method." ) );
	}

	if ( il2cppBase != 0 && GuestCR3 != 0 )
	{
		uintptr_t physAddr = 0;
		if ( ConvertCR3( il2cppBase, GuestCR3, physAddr ) )
		{
			int ELFHeader = 0;
			if ( PGMR3PhysRead( VmInstancePtr, physAddr, &ELFHeader, sizeof( ELFHeader ) ) == 0 )
			{
				Console::LogHex( XorStr( "[Memory] CR3 from mm->pgd OK, ELFHeader = 0x" ), ELFHeader );
				Offsets::GameConfig( );
				Data::StartReadThread( );
			}
			else Console::Log( XorStr( "[Memory] PGMR3PhysRead failed when checking ELF with GuestCR3" ) );
		}
		else Console::Log( XorStr( "[Memory] ConvertCR3(il2cppBase, GuestCR3) failed" ) );
	}
	else
	{
		if ( GuestCR3 == 0 ) Console::Log( XorStr( "[Memory] Warning: GuestCR3 is 0" ) );
	}

	g_Globals.General.EnableFuncs = true;
	std::thread( [ & ] ( )
	{
		NotifyManager::Send( XorStr( "Started" ), 4000 );
	} ).detach( );
	return ( VmInstancePtr != nullptr && Offsets::LibIl2Cpp != 0 );
}

bool Memory::Restart( )
{
	Data::StopReadThread( );
	Offsets::LibIl2Cpp = 0;
	GuestCR3 = 0;
	FlushTLB( );
	FlushAllTLB( );

	auto& env = GetEmulatorEnv( );
	bool N32 = ( env.Abi( ) == ABIType::X86 );
	auto modules = GetModuleAddress( N32 );
	uintptr_t il2cppBase = modules.empty( ) ? 0 : modules [ 0 ];

	Offsets::LibIl2CppCandidates = modules;

	if ( il2cppBase )
	{
		Offsets::LibIl2Cpp = il2cppBase;
		Console::LogHex( XorStr( "[Memory] Il2Cpp located at " ), il2cppBase );
	}
	else Console::Log( XorStr( "[Memory] Failed to locate libil2cpp.so by any method." ) );

	if ( il2cppBase != 0 && GuestCR3 != 0 )
	{
		uintptr_t physAddr = 0;
		if ( ConvertCR3( il2cppBase, GuestCR3, physAddr ) )
		{
			int ELFHeader = 0;
			if ( PGMR3PhysRead( VmInstancePtr, physAddr, &ELFHeader, sizeof( ELFHeader ) ) == 0 )
			{
				Console::LogHex( XorStr( "[Memory] CR3 from mm->pgd OK, ELFHeader = 0x" ), ELFHeader );
				Offsets::GameConfig( );
				Data::StartReadThread( );
			}
			else Console::Log( XorStr( "[Memory] PGMR3PhysRead failed when checking ELF with GuestCR3" ) );
		}
		else Console::Log( XorStr( "[Memory] ConvertCR3(il2cppBase, GuestCR3) failed" ) );
	}
	else
	{
		if ( GuestCR3 == 0 ) Console::Log( XorStr( "[Memory] Warning: GuestCR3 is 0" ) );
	}
	return ( VmInstancePtr != nullptr && Offsets::LibIl2Cpp != 0 );
}

bool Memory::RestartAsync()
{
	// Single-flight: se ja existe um restart em andamento, nao empilha outro.
	if ( InterlockedCompareExchange( &s_RestartPending, 1, 0 ) != 0 )
		return false;

	// Cooldown: evita tempestade de rescans durante loading screens
	// (GameFacade legitima 0 por alguns segundos entre partidas).
	if ( GetTickCount64( ) - InterlockedCompareExchange64( &s_LastRestartTick, 0, 0 ) < 2000 )
	{
		InterlockedExchange( &s_RestartPending, 0 );
		return false;
	}

	std::thread( [ ] ( )
	{
		InterlockedExchange64( &s_LastRestartTick, GetTickCount64( ) );

		int delayMs = 250;
		while ( !g_Globals.General.ShutDown )
		{
			// Relocaliza libil2cpp + CR3. Se falhar (processo do jogo ainda
			// reiniciando), tenta de novo com backoff crescente ate conseguir.
			if ( g_FreeFireMemory.Restart( ) )
				break;

			std::this_thread::sleep_for( std::chrono::milliseconds( delayMs ) );
			if ( delayMs < 5000 ) delayMs *= 2;
		}
		InterlockedExchange( &s_RestartPending, 0 );
	} ).detach( );

	return true;
}

Memory g_FreeFireMemory;