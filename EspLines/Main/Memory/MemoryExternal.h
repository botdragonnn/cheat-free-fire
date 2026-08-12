#pragma once
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <cstdint>
#include "../../../bypass/StealthyOpenProcess.h"

#pragma comment(lib, "psapi.lib")

#ifndef _NTDEF_
typedef LONG NTSTATUS;
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

// typedefs de syscalls
typedef NTSTATUS(NTAPI* NtWriteVirtualMemory_t)(
    HANDLE ProcessHandle,
    PVOID  BaseAddress,
    PVOID  Buffer,
    ULONG  NumberOfBytesToWrite,
    PULONG NumberOfBytesWritten
    );

typedef NTSTATUS(NTAPI* NtReadVirtualMemory_t)(
    HANDLE ProcessHandle,
    PVOID  BaseAddress,
    PVOID  Buffer,
    ULONG  NumberOfBytesToRead,
    PULONG NumberOfBytesRead
    );

//======================================================
// Vetor simples, sem STL (usa HeapAlloc/HeapFree)
//======================================================
template <typename T>
class SimpleVector
{
public:
    SimpleVector()
        : _data(NULL), _size(0), _capacity(0)
    {
    }

    ~SimpleVector()
    {
        Free();
    }

    SimpleVector(const SimpleVector&) = delete;
    SimpleVector& operator=(const SimpleVector&) = delete;

    SimpleVector(SimpleVector&& other) noexcept
        : _data(other._data), _size(other._size), _capacity(other._capacity)
    {
        other._data = NULL;
        other._size = 0;
        other._capacity = 0;
    }

    SimpleVector& operator=(SimpleVector&& other) noexcept
    {
        if (this != &other)
        {
            Free();
            _data = other._data;
            _size = other._size;
            _capacity = other._capacity;

            other._data = NULL;
            other._size = 0;
            other._capacity = 0;
        }
        return *this;
    }

    bool Reserve(SIZE_T newCapacity)
    {
        if (newCapacity <= _capacity)
            return true;

        HANDLE heap = GetProcessHeap();
        if (!heap)
            return false;

        void* mem = NULL;
        if (_data)
            mem = HeapReAlloc(heap, 0, _data, newCapacity * sizeof(T));
        else
            mem = HeapAlloc(heap, 0, newCapacity * sizeof(T));

        if (!mem)
            return false;

        _data = (T*)mem;
        _capacity = newCapacity;
        if (_size > _capacity)
            _size = _capacity;

        return true;
    }

    bool PushBack(const T& value)
    {
        if (_size + 1 > _capacity)
        {
            SIZE_T newCap = (_capacity == 0) ? 16 : (_capacity * 2);
            if (newCap < _size + 1)
                newCap = _size + 1;
            if (!Reserve(newCap))
                return false;
        }

        _data[_size++] = value;
        return true;
    }

    void Clear()
    {
        _size = 0;
    }

    void Free()
    {
        if (_data)
        {
            HANDLE heap = GetProcessHeap();
            if (heap)
                HeapFree(heap, 0, _data);
            _data = NULL;
        }
        _size = 0;
        _capacity = 0;
    }

    void Resize(SIZE_T newSize)
    {
        if (newSize > _capacity)
        {
            if (!Reserve(newSize))
                return;
        }
        _size = newSize;
    }

    T* data() { return _data; }
    const T* data() const { return _data; }

    SIZE_T size()  const { return _size; }
    bool   empty() const { return _size == 0; }

    T& operator[](SIZE_T i) { return _data[i]; }
    const T& operator[](SIZE_T i) const { return _data[i]; }

    T* begin() { return _data; }
    T* end() { return _data + _size; }
    const T* begin() const { return _data; }
    const T* end()   const { return _data + _size; }

private:
    T* _data;
    SIZE_T _size;
    SIZE_T _capacity;
};

//======================================================
// MemoryExternal CRT-free
//======================================================
class MemoryExternal
{
public:

    //void Attach(HANDLE hProcess, DWORD pid)
    //{
    //    _hProc = hProcess;
    //    _pid = pid;
    //}

    //// ======================================================
    //// GetRemoteModuleBase COM FALLBACK para base local
    //// ======================================================
    //uintptr_t GetRemoteModuleBase(const wchar_t* moduleName)
    //{
    //    if (!_hProc || !moduleName)
    //        return 0;

    //    // Tenta via CreateToolhelp32Snapshot primeiro
    //    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, _pid);
    //    if (hSnapshot == INVALID_HANDLE_VALUE)
    //    {
    //        // Se falhar, tenta sem SNAPMODULE32
    //        hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, _pid);
    //    }

    //    if (hSnapshot != INVALID_HANDLE_VALUE)
    //    {
    //        MODULEENTRY32W me{};
    //        me.dwSize = sizeof(me);

    //        if (Module32FirstW(hSnapshot, &me))
    //        {
    //            do
    //            {
    //                if (_wcsicmp(me.szModule, moduleName) == 0)
    //                {
    //                    uintptr_t result = reinterpret_cast<uintptr_t>(me.modBaseAddr);
    //                    CloseHandle(hSnapshot);
    //                    return result;
    //                }
    //            } while (Module32NextW(hSnapshot, &me));
    //        }

    //        CloseHandle(hSnapshot);
    //    }

    //    // FALLBACK: Para ntdll.dll, usa base local (sempre é a mesma)
    //    if (_wcsicmp(moduleName, L"KernelBase.dll") == 0)
    //    {
    //        HMODULE hLocal = GetModuleHandleW(L"KernelBase.dll");
    //        if (hLocal)
    //        {
    //            return (uintptr_t)hLocal;
    //        }
    //    }

    //    return 0;
    //}

    bool HasReadWritePermissions() const
    {
        return HasReadWritePermissions(_hProc);
    }

    bool HasReadWritePermissions(HANDLE handle) const
    {
        if (!handle)
            return false;

        BYTE testBuffer[16] = {};
        BYTE verifyBuffer[16] = {};

        LPVOID remoteMem = VirtualAllocEx(handle, NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remoteMem)
        {
            BYTE dummy = 0;
            SIZE_T written = 0;
            NTSTATUS status = _NtWriteVirtualMemory
                ? _NtWriteVirtualMemory(handle, (PVOID)0x7FFFFFFE0000, &dummy, sizeof(dummy), (PULONG)&written)
                : (NTSTATUS)0xC0000001;
            return NT_SUCCESS(status);
        }

        for (int i = 0; i < sizeof(testBuffer); ++i)
            testBuffer[i] = (BYTE)(i * 13 + 7);

        ULONG written = 0;
        NTSTATUS wstatus = _NtWriteVirtualMemory
            ? _NtWriteVirtualMemory(handle, remoteMem, testBuffer, sizeof(testBuffer), &written)
            : (NTSTATUS)0xC0000001;

        ULONG read = 0;
        NTSTATUS rstatus = _NtReadVirtualMemory
            ? _NtReadVirtualMemory(handle, remoteMem, verifyBuffer, sizeof(verifyBuffer), &read)
            : (NTSTATUS)0xC0000001;

        bool ok = NT_SUCCESS(wstatus) && NT_SUCCESS(rstatus)
            && written == sizeof(testBuffer)
            && read == sizeof(verifyBuffer);

        if (ok)
        {
            for (int i = 0; i < sizeof(testBuffer); ++i)
            {
                if (testBuffer[i] != verifyBuffer[i])
                {
                    ok = false;
                    break;
                }
            }
        }

        VirtualFreeEx(handle, remoteMem, 0, MEM_RELEASE);
        return ok;
    }

    MemoryExternal()
        : _hProc(NULL),
        _pid(0),
        _NtWriteVirtualMemory(NULL),
        _NtReadVirtualMemory(NULL),
        _NtSuspendProcess(NULL),
        _NtResumeProcess(NULL)
    {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll)
        {
            _NtWriteVirtualMemory = (NtWriteVirtualMemory_t)GetProcAddress(ntdll, "NtWriteVirtualMemory");
            _NtReadVirtualMemory = (NtReadVirtualMemory_t)GetProcAddress(ntdll, "NtReadVirtualMemory");

            _NtSuspendProcess = (NtSuspendProcess_t)GetProcAddress(ntdll, "NtSuspendProcess");
            _NtResumeProcess = (NtResumeProcess_t)GetProcAddress(ntdll, "NtResumeProcess");
        }
    }

    ~MemoryExternal()
    {
        Reset();
    }

    //bool SetProcess(DWORD pid)
    //{
    //    Reset();
    //    _pid = pid;
    //    _hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    //    return (_hProc != NULL);
    //}

    bool SetProcess(DWORD pid)
    {
        Reset();
        _pid = pid;

        // Usa StealthyOpenProcess em vez de OpenProcess normal
        _hProc = StealthyOpenProcess::Execute(pid, 0x0430);


        return (_hProc != NULL);
    }

    bool SetHandle(HANDLE h, DWORD pid)
    {
        Reset();
        _hProc = h;
        _pid = pid;
        return (_hProc != NULL);
    }

    bool   HasValidHandle() const { return _hProc != NULL; }
    HANDLE GetHandle()      const { return _hProc; }

    void Reset()
    {
        if (_hProc)
        {
            CloseHandle(_hProc);
            _hProc = NULL;
        }
        _pid = 0;
    }

    typedef NTSTATUS(NTAPI* NtSuspendProcess_t)(HANDLE);
    typedef NTSTATUS(NTAPI* NtResumeProcess_t)(HANDLE);

    void SuspendProcess() const
    {
        if (_NtSuspendProcess && _hProc)
            _NtSuspendProcess(_hProc);
    }

    void ResumeProcess() const
    {
        if (_NtResumeProcess && _hProc)
            _NtResumeProcess(_hProc);
    }

    template<typename T>
    T Read(ULONG_PTR addr) const
    {
        T val;
        ZeroMemory(&val, sizeof(T));

        if (!_NtReadVirtualMemory || !_hProc)
            return val;

        ULONG bytesRead = 0;
        NTSTATUS status = _NtReadVirtualMemory(
            _hProc,
            (PVOID)addr,
            &val,
            (ULONG)sizeof(T),
            &bytesRead
        );

        if (!NT_SUCCESS(status) || bytesRead != sizeof(T))
            ZeroMemory(&val, sizeof(T));

        return val;
    }

    template<typename T>
    bool Write(ULONG_PTR addr, const T& val) const
    {
        if (!_NtWriteVirtualMemory || !_hProc)
            return false;

        ULONG written = 0;
        NTSTATUS status = _NtWriteVirtualMemory(
            _hProc,
            (PVOID)addr,
            (PVOID)&val,
            (ULONG)sizeof(T),
            &written
        );

        return NT_SUCCESS(status) && written == sizeof(T);
    }

    bool WriteBlock(ULONG_PTR addr, const void* data, SIZE_T size) const
    {
        if (!_NtWriteVirtualMemory || !_hProc || !data || size == 0)
            return false;

        const BYTE* src = (const BYTE*)data;
        SIZE_T totalWritten = 0;
        const SIZE_T CHUNK = 0x1000;

        while (totalWritten < size)
        {
            SIZE_T remaining = size - totalWritten;
            SIZE_T toWrite = (remaining < CHUNK) ? remaining : CHUNK;

            ULONG written = 0;
            NTSTATUS status = _NtWriteVirtualMemory(
                _hProc,
                (PVOID)(addr + totalWritten),
                (PVOID)(src + totalWritten),
                (ULONG)toWrite,
                &written
            );

            if (!NT_SUCCESS(status) || written == 0)
                return false;

            totalWritten += written;
        }

        return true;
    }

    bool ReadBlock(ULONG_PTR addr, void* buffer, SIZE_T size, SIZE_T* outRead = NULL) const
    {
        if (!_NtReadVirtualMemory || !_hProc || !buffer || size == 0)
            return false;

        ULONG bytesRead = 0;
        NTSTATUS status = _NtReadVirtualMemory(
            _hProc,
            (PVOID)addr,
            buffer,
            (ULONG)size,
            &bytesRead
        );

        if (outRead)
            *outRead = (SIZE_T)bytesRead;

        return NT_SUCCESS(status) && bytesRead > 0;
    }

    void WriteMem(ULONG_PTR addr, const void* data, SIZE_T size) const
    {
        WriteBlock(addr, data, size);
    }

    uintptr_t FindRemoteExport(uintptr_t remoteModuleBase, const char* exportName)
    {
        if (!remoteModuleBase || !exportName)
            return 0;

        IMAGE_DOS_HEADER dosHeader{};
        if (!ReadBlock(remoteModuleBase, &dosHeader, sizeof(dosHeader)))
            return 0;

        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
            return 0;

        IMAGE_NT_HEADERS64 ntHeaders{};
        if (!ReadBlock(remoteModuleBase + dosHeader.e_lfanew, &ntHeaders, sizeof(ntHeaders)))
            return 0;

        if (ntHeaders.Signature != IMAGE_NT_SIGNATURE)
            return 0;

        IMAGE_DATA_DIRECTORY exportDir = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!exportDir.VirtualAddress || !exportDir.Size)
            return 0;

        IMAGE_EXPORT_DIRECTORY exportTable{};
        if (!ReadBlock(remoteModuleBase + exportDir.VirtualAddress, &exportTable, sizeof(exportTable)))
            return 0;

        SimpleVector<uint32_t> nameRvas;
        nameRvas.Resize(exportTable.NumberOfNames);
        if (!ReadBlock(remoteModuleBase + exportTable.AddressOfNames, nameRvas.data(),
            sizeof(uint32_t) * exportTable.NumberOfNames))
            return 0;

        SimpleVector<uint16_t> ordinals;
        ordinals.Resize(exportTable.NumberOfNames);
        if (!ReadBlock(remoteModuleBase + exportTable.AddressOfNameOrdinals, ordinals.data(),
            sizeof(uint16_t) * exportTable.NumberOfNames))
            return 0;

        SimpleVector<uint32_t> functions;
        functions.Resize(exportTable.NumberOfFunctions);
        if (!ReadBlock(remoteModuleBase + exportTable.AddressOfFunctions, functions.data(),
            sizeof(uint32_t) * exportTable.NumberOfFunctions))
            return 0;

        char nameBuffer[128]{};

        for (DWORD i = 0; i < exportTable.NumberOfNames; ++i)
        {
            uintptr_t nameAddr = remoteModuleBase + nameRvas[i];
            ZeroMemory(nameBuffer, sizeof(nameBuffer));

            if (!ReadBlock(nameAddr, nameBuffer, sizeof(nameBuffer) - 1))
                continue;

            nameBuffer[sizeof(nameBuffer) - 1] = '\0';

            if (_stricmp(nameBuffer, exportName) == 0)
            {
                WORD ordinal = ordinals[i];
                if (ordinal >= functions.size())
                    return 0;

                uint32_t rva = functions[ordinal];
                uintptr_t funcAddr = remoteModuleBase + rva;

                if (funcAddr < remoteModuleBase || funcAddr >(remoteModuleBase + 0x10000000))
                {
                    return 0;
                }

                return funcAddr;
            }
        }

        return 0;
    }

    typedef SimpleVector<ULONG_PTR> AddrVector;

    AddrVector AobScan(const char* pattern) const
    {
        AddrVector hits;

        if (!_hProc || !_NtReadVirtualMemory || !pattern)
            return hits;

        PatternData pd;
        if (!ParsePattern(pattern, pd))
            return hits;

        SimpleVector<MemRegion> regions;
        QueryMemoryRegions(regions);

        if (pd.pat.size() == 0 || regions.size() == 0)
            return hits;

        const SIZE_T patternSize = pd.pat.size();
        const SIZE_T overlap = (patternSize > 0) ? (patternSize - 1) : 0;
        const SIZE_T CHUNK_SIZE = 16 * 1024 * 1024;

        BYTE* buffer = (BYTE*)VirtualAlloc(NULL, CHUNK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!buffer)
            return hits;

        for (SIZE_T r = 0; r < regions.size(); ++r)
        {
            ULONG_PTR regionBase = (ULONG_PTR)regions[r].base;
            SIZE_T    regionSize = regions[r].size;

            if (regionSize < patternSize)
                continue;

            SIZE_T offset = 0;
            while (offset < regionSize)
            {
                SIZE_T remaining = regionSize - offset;
                SIZE_T toRead = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;

                ULONG bytesRead = 0;
                NTSTATUS status = _NtReadVirtualMemory(
                    _hProc,
                    (PVOID)(regionBase + offset),
                    buffer,
                    (ULONG)toRead,
                    &bytesRead
                );

                if (!NT_SUCCESS(status) || bytesRead < patternSize)
                {
                    offset += toRead;
                    continue;
                }

                SIZE_T maxPos = (SIZE_T)bytesRead - patternSize;

                for (SIZE_T pos = 0; pos <= maxPos; ++pos)
                {
                    if (MatchPattern(buffer + pos, pd, patternSize))
                    {
                        hits.PushBack(regionBase + offset + pos);
                    }
                }

                if (toRead > overlap)
                    offset += (toRead - overlap);
                else
                    offset += toRead;

                Sleep(2);
            }
        }

        VirtualFree(buffer, 0, MEM_RELEASE);
        return hits;
    }

    bool CheckPattern(ULONG_PTR address, const char* pattern) const
    {
        if (!_hProc || !_NtReadVirtualMemory || !pattern)
            return false;

        PatternData pd;
        if (!ParsePattern(pattern, pd) || pd.pat.size() == 0)
            return false;

        SIZE_T patternSize = pd.pat.size();
        BYTE* buffer = (BYTE*)VirtualAlloc(NULL, patternSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!buffer)
            return false;

        ULONG bytesRead = 0;
        NTSTATUS status = _NtReadVirtualMemory(
            _hProc,
            (PVOID)address,
            buffer,
            (ULONG)patternSize,
            &bytesRead
        );

        bool result = false;
        if (NT_SUCCESS(status) && bytesRead == patternSize)
        {
            result = MatchPattern(buffer, pd, patternSize);
        }

        VirtualFree(buffer, 0, MEM_RELEASE);
        return result;
    }

    inline static DWORD FindProcessId(const WCHAR* nameW)
    {
        if (!nameW || !nameW[0])
            return 0;

        WCHAR target[260];
        ToLowerCopyW(nameW, target, 260);

        DWORD pid = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
            return 0;

        PROCESSENTRY32W pe;
        ZeroMemory(&pe, sizeof(pe));
        pe.dwSize = sizeof(pe);

        if (Process32FirstW(snap, &pe))
        {
            do
            {
                WCHAR exeLower[260];
                ToLowerCopyW(pe.szExeFile, exeLower, 260);

                if (StringEqualsW(exeLower, target))
                {
                    pid = pe.th32ProcessID;
                    break;
                }

            } while (Process32NextW(snap, &pe));
        }

        CloseHandle(snap);
        return pid;
    }

    inline static DWORD FindProcessId(const char* nameA)
    {
        if (!nameA || !nameA[0])
            return 0;

        int lenW = MultiByteToWideChar(CP_ACP, 0, nameA, -1, NULL, 0);
        if (lenW <= 1)
            return 0;

        if (lenW > 260)
            lenW = 260;

        WCHAR buf[260];
        ZeroMemory(buf, sizeof(buf));

        MultiByteToWideChar(CP_ACP, 0, nameA, -1, buf, lenW);

        return FindProcessId((const WCHAR*)buf);
    }

private:
    HANDLE _hProc;
    DWORD  _pid;
    NtWriteVirtualMemory_t _NtWriteVirtualMemory;
    NtReadVirtualMemory_t  _NtReadVirtualMemory;

    NtSuspendProcess_t _NtSuspendProcess;
    NtResumeProcess_t  _NtResumeProcess;

    struct PatternData
    {
        SimpleVector<BYTE> pat;
        SimpleVector<BYTE> mask;
    };

    struct MemRegion
    {
        LPCVOID base;
        SIZE_T  size;
    };

    static BYTE HexCharToVal(char c)
    {
        if (c >= '0' && c <= '9') return (BYTE)(c - '0');
        if (c >= 'A' && c <= 'F') return (BYTE)(c - 'A' + 10);
        if (c >= 'a' && c <= 'f') return (BYTE)(c - 'a' + 10);
        return 0xFF;
    }

    static bool ParsePattern(const char* p, PatternData& out)
    {
        out.pat.Clear();
        out.mask.Clear();

        const char* s = p;
        while (*s)
        {
            while (*s == ' ' || *s == '\t')
                ++s;

            if (!*s)
                break;

            char token[3];
            int  tlen = 0;

            while (*s && *s != ' ' && *s != '\t' && tlen < 2)
            {
                token[tlen++] = *s;
                ++s;
            }

            while (*s && *s != ' ' && *s != '\t')
                ++s;

            if (tlen == 0)
                continue;

            token[tlen] = '\0';

            if (token[0] == '?')
            {
                out.pat.PushBack(0);
                out.mask.PushBack(0);
            }
            else
            {
                BYTE hi = HexCharToVal(token[0]);
                BYTE lo = 0;
                if (tlen > 1)
                    lo = HexCharToVal(token[1]);

                if (hi == 0xFF || (tlen > 1 && lo == 0xFF))
                {
                    out.pat.Clear();
                    out.mask.Clear();
                    return false;
                }

                BYTE b = (BYTE)((hi << 4) | (lo & 0x0F));
                out.pat.PushBack(b);
                out.mask.PushBack(0xFF);
            }
        }

        return (out.pat.size() > 0);
    }

    static BOOL StringEqualsW(const WCHAR* a, const WCHAR* b)
    {
        if (!a || !b)
            return FALSE;

        while (*a && *b)
        {
            if (*a != *b)
                return FALSE;
            ++a;
            ++b;
        }
        return (*a == 0 && *b == 0);
    }

    static void ToLowerCopyW(const WCHAR* src, WCHAR* dst, int dstLen)
    {
        if (!src || !dst || dstLen <= 0)
            return;

        int i = 0;
        for (; i < dstLen - 1 && src[i] != 0; ++i)
        {
            WCHAR c = src[i];
            if (c >= L'A' && c <= L'Z')
                c = (WCHAR)(c - L'A' + L'a');
            dst[i] = c;
        }
        dst[i] = 0;
    }

    static bool MatchPattern(const BYTE* data, const PatternData& pd, SIZE_T patternSize)
    {
        const BYTE* pPat = pd.pat.data();
        const BYTE* pMask = pd.mask.data();

        for (SIZE_T i = 0; i < patternSize; ++i)
        {
            BYTE m = pMask[i];
            if (m != 0)
            {
                BYTE v = (BYTE)(data[i] & m);
                if (v != pPat[i])
                    return false;
            }
        }
        return true;
    }

    void QueryMemoryRegions(SimpleVector<MemRegion>& out) const
    {
        out.Clear();

        if (!_hProc)
            return;

        LPCBYTE addr = 0;
        MEMORY_BASIC_INFORMATION mbi;
        DWORD allowedProtections =
            PAGE_READONLY |
            PAGE_READWRITE |
            PAGE_EXECUTE_READ |
            PAGE_EXECUTE_READWRITE;

        while (VirtualQueryEx(_hProc, addr, &mbi, sizeof(mbi)) == sizeof(mbi))
        {
            if (mbi.State == MEM_COMMIT &&
                !(mbi.Protect & PAGE_GUARD) &&
                (mbi.Protect & allowedProtections))
            {
                MemRegion mr;
                mr.base = mbi.BaseAddress;
                mr.size = mbi.RegionSize;
                out.PushBack(mr);
            }

            addr = (LPCBYTE)mbi.BaseAddress + mbi.RegionSize;
        }
    }
};