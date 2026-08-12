#include "DynamicStub.hpp"
#include <TlHelp32.h>
#include <vector>
#include <algorithm>
#include <Psapi.h>
#include <XorStr.hpp>

extern HMODULE g_hModule;

// ============================================================
// DynamicStub v2 - VEH PAGE_GUARD based (zero CoW)
// ============================================================

typedef LONG NTSTATUS;

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID, * PCLIENT_ID;

typedef struct _THREAD_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PVOID    TebBaseAddress;
    CLIENT_ID ClientId;
    KAFFINITY AffinityMask;
    LONG     Priority;
    LONG     BasePriority;
} THREAD_BASIC_INFORMATION, * PTHREAD_BASIC_INFORMATION;

typedef enum _THREADINFOCLASS {
    ThreadBasicInformation = 0,
    ThreadQuerySetWin32StartAddress = 9
} THREADINFOCLASS;

typedef NTSTATUS(NTAPI* pNtQueryInformationThread)(
    HANDLE ThreadHandle,
    THREADINFOCLASS ThreadInformationClass,
    PVOID ThreadInformation,
    ULONG ThreadInformationLength,
    PULONG ReturnLength
    );

typedef struct _PEB_LDR_DATA {
    ULONG      Length;
    BOOLEAN    Initialized;
    PVOID      SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA, * PPEB_LDR_DATA;

typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID      DllBase;
    PVOID      EntryPoint;
    ULONG      SizeOfImage;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

typedef struct _PEB {
    BYTE           Reserved1[2];
    BYTE           BeingDebugged;
    BYTE           Reserved2[1];
    PVOID          Reserved3[2];
    PPEB_LDR_DATA  Ldr;
} PEB, * PPEB;

struct ThreadInfo
{
    PVOID         startAddress;
    HMODULE       hModule;
    ULONG64       cycles;
    std::wstring  moduleName;
};

namespace DynamicStub
{
    struct VEHStubContext
    {
        volatile bool  active;
        volatile bool  redirected;
        DWORD          targetThreadId;
        uintptr_t      stubAddress;
        uintptr_t      realFunction;
        uintptr_t      realParameter;
        DWORD          oldProtect;
        PVOID          vehHandle;
    };

    static VEHStubContext   g_VehCtx = {};
    static CRITICAL_SECTION g_CriticalSection = {};
    static bool             g_Initialized = false;

    static pNtQueryInformationThread g_NtQueryInformationThread = nullptr;

    static LONG WINAPI StubVEHHandler(EXCEPTION_POINTERS* ep)
    {
        if (ep->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION)
        {
            if (g_VehCtx.active && !g_VehCtx.redirected)
            {
                if (GetCurrentThreadId() == g_VehCtx.targetThreadId)
                {
                    uintptr_t rip = (uintptr_t)ep->ContextRecord->Rip;

                    if (rip == g_VehCtx.stubAddress)
                    {
                        ep->ContextRecord->Rip = g_VehCtx.realFunction;
                        ep->ContextRecord->Rcx = g_VehCtx.realParameter;
                        g_VehCtx.redirected = true;
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
            }

            ep->ContextRecord->EFlags |= 0x100;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        if (ep->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
        {
            if (g_VehCtx.active && !g_VehCtx.redirected)
            {
                DWORD dwOld;
                VirtualProtect(
                    (LPVOID)(g_VehCtx.stubAddress),
                    1,
                    PAGE_EXECUTE_READ | PAGE_GUARD,
                    &dwOld
                );
            }
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }

    static bool IsModuleInPEB(HMODULE hModule)
    {
        __try
        {
#ifdef _WIN64
            PEB* peb = (PEB*)__readgsqword(0x60);
#else
            PEB* peb = (PEB*)__readfsdword(0x30);
#endif
            if (!peb || !peb->Ldr) return false;

            PLIST_ENTRY listHead = &peb->Ldr->InLoadOrderModuleList;
            PLIST_ENTRY listEntry = listHead->Flink;

            while (listEntry != listHead)
            {
                PLDR_DATA_TABLE_ENTRY entry =
                    CONTAINING_RECORD(listEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
                if (entry->DllBase == hModule) return true;
                listEntry = listEntry->Flink;
            }
            return false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    static PVOID GetThreadStartAddress(HANDLE hThread)
    {
        if (!g_NtQueryInformationThread) return nullptr;

        PVOID startAddress = nullptr;
        NTSTATUS status = g_NtQueryInformationThread(
            hThread,
            ThreadQuerySetWin32StartAddress,
            &startAddress,
            sizeof(startAddress),
            nullptr
        );
        return (status == 0) ? startAddress : nullptr;
    }

    static bool IsAddressInModule(PVOID address, HMODULE hModule, SIZE_T moduleSize)
    {
        if (!address || !hModule) return false;
        LPBYTE base = (LPBYTE)hModule;
        LPBYTE addr = (LPBYTE)address;
        return (addr >= base && addr < (base + moduleSize));
    }

    // ============================================================
    // FindPriorityAddress - acha endereco em ucrtbase/msvcr100
    // Agora usando XorStr em vez de XOR manual
    // ============================================================
    static PVOID FindPriorityAddress()
    {
        // XorStr wide — descriptografa no stack, wipe no fim do scope
        auto xs_ucrt = xorstr(L"ucrtbase.dll");
        auto xs_msvcr = xorstr(L"msvcr100.dll");

        HMODULE hUcrt = GetModuleHandleW(xs_ucrt.crypt_get());
        HMODULE hMsvcr = GetModuleHandleW(xs_msvcr.crypt_get());
        if (!hUcrt && !hMsvcr) return nullptr;

        std::vector<ThreadInfo> targetThreads;

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return nullptr;

        THREADENTRY32 te32 = {};
        te32.dwSize = sizeof(THREADENTRY32);
        DWORD currentPid = GetCurrentProcessId();

        if (Thread32First(hSnapshot, &te32))
        {
            do
            {
                if (te32.th32OwnerProcessID != currentPid) continue;

                HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
                if (!hThread) continue;

                PVOID startAddr = GetThreadStartAddress(hThread);
                ULONG64 cycles = 0;
                QueryThreadCycleTime(hThread, &cycles);
                CloseHandle(hThread);

                if (!startAddr) continue;

                auto checkModule = [&](HMODULE hMod, const wchar_t* name)
                    {
                        if (!hMod) return false;
                        MODULEINFO modInfo = {};
                        if (!GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo)))
                            return false;
                        if (!IsAddressInModule(startAddr, hMod, modInfo.SizeOfImage))
                            return false;

                        ThreadInfo info = {};
                        info.startAddress = startAddr;
                        info.hModule = hMod;
                        info.cycles = cycles;
                        info.moduleName = name;
                        targetThreads.push_back(info);
                        return true;
                    };

                if (checkModule(hUcrt, xs_ucrt.get()))   continue;
                checkModule(hMsvcr, xs_msvcr.get());

            } while (Thread32Next(hSnapshot, &te32));
        }
        CloseHandle(hSnapshot);

        if (targetThreads.empty()) return nullptr;

        std::sort(targetThreads.begin(), targetThreads.end(),
            [](const ThreadInfo& a, const ThreadInfo& b) { return a.cycles > b.cycles; });

        return targetThreads[0].startAddress;
    }

    // ============================================================
    // Inicializacao / Shutdown
    // ============================================================
    bool Initialize()
    {
        if (g_Initialized) return true;
        InitializeCriticalSection(&g_CriticalSection);

        HMODULE hNtdll = xorstr(L"ntdll.dll").use([](const wchar_t* s) {
            return GetModuleHandleW(s);
            });
        if (hNtdll)
        {
            g_NtQueryInformationThread = xorstr("NtQueryInformationThread").use([&](const char* s) {
                return (pNtQueryInformationThread)GetProcAddress(hNtdll, s);
                });
        }

        ZeroMemory(&g_VehCtx, sizeof(g_VehCtx));
        g_Initialized = true;
        return true;
    }

    void Shutdown()
    {
        if (!g_Initialized) return;
        CleanupStub();
        DeleteCriticalSection(&g_CriticalSection);
        g_Initialized = false;
    }

    void CleanupStub()
    {
        if (g_VehCtx.vehHandle)
        {
            g_VehCtx.active = false;

            if (g_VehCtx.stubAddress)
            {
                DWORD dwOld;
                VirtualProtect(
                    (LPVOID)g_VehCtx.stubAddress,
                    1,
                    g_VehCtx.oldProtect,
                    &dwOld
                );
            }

            RemoveVectoredExceptionHandler(g_VehCtx.vehHandle);
        }

        ZeroMemory(&g_VehCtx, sizeof(g_VehCtx));
    }

    HANDLE CreateThreadWithDynamicStub(
        LPTHREAD_START_ROUTINE lpStartAddress,
        LPVOID lpParameter,
        DWORD dwCreationFlags,
        LPDWORD lpThreadId)
    {
        if (!g_Initialized) Initialize();

        if (IsModuleInPEB(g_hModule))
        {
            return CreateThread(
                nullptr, 0, lpStartAddress, lpParameter,
                dwCreationFlags, lpThreadId
            );
        }

        EnterCriticalSection(&g_CriticalSection);

        if (g_VehCtx.active || g_VehCtx.vehHandle)
        {
            CleanupStub();
        }

        PVOID stubLocation = FindPriorityAddress();
        if (!stubLocation)
        {
            LeaveCriticalSection(&g_CriticalSection);
            return CreateThread(
                nullptr, 0, lpStartAddress, lpParameter,
                dwCreationFlags, lpThreadId
            );
        }

        ZeroMemory(&g_VehCtx, sizeof(g_VehCtx));
        g_VehCtx.stubAddress = (uintptr_t)stubLocation;
        g_VehCtx.realFunction = (uintptr_t)lpStartAddress;
        g_VehCtx.realParameter = (uintptr_t)lpParameter;
        g_VehCtx.active = false;
        g_VehCtx.redirected = false;

        g_VehCtx.vehHandle = AddVectoredExceptionHandler(
            TRUE,
            StubVEHHandler
        );

        if (!g_VehCtx.vehHandle)
        {
            LeaveCriticalSection(&g_CriticalSection);
            return CreateThread(
                nullptr, 0, lpStartAddress, lpParameter,
                dwCreationFlags, lpThreadId
            );
        }

        DWORD threadId = 0;
        HANDLE hThread = CreateThread(
            nullptr,
            0,
            (LPTHREAD_START_ROUTINE)stubLocation,
            nullptr,
            CREATE_SUSPENDED,
            &threadId
        );

        if (!hThread)
        {
            RemoveVectoredExceptionHandler(g_VehCtx.vehHandle);
            ZeroMemory(&g_VehCtx, sizeof(g_VehCtx));
            LeaveCriticalSection(&g_CriticalSection);
            return nullptr;
        }

        g_VehCtx.targetThreadId = threadId;
        if (lpThreadId) *lpThreadId = threadId;

        if (!VirtualProtect(
            (LPVOID)stubLocation,
            1,
            PAGE_EXECUTE_READ | PAGE_GUARD,
            &g_VehCtx.oldProtect))
        {
            TerminateThread(hThread, 0);
            CloseHandle(hThread);
            RemoveVectoredExceptionHandler(g_VehCtx.vehHandle);
            ZeroMemory(&g_VehCtx, sizeof(g_VehCtx));
            LeaveCriticalSection(&g_CriticalSection);
            return nullptr;
        }

        g_VehCtx.active = true;

        bool callerWantsSuspended = (dwCreationFlags & CREATE_SUSPENDED) != 0;

        ResumeThread(hThread);

        DWORD waitStart = GetTickCount();
        while (!g_VehCtx.redirected)
        {
            if ((GetTickCount() - waitStart) > 2000)
            {
                g_VehCtx.active = false;
                TerminateThread(hThread, 0);
                CloseHandle(hThread);
                CleanupStub();
                LeaveCriticalSection(&g_CriticalSection);
                return nullptr;
            }
            Sleep(1);
        }

        g_VehCtx.active = false;

        DWORD dwOld;
        VirtualProtect(
            (LPVOID)stubLocation,
            1,
            g_VehCtx.oldProtect,
            &dwOld
        );

        RemoveVectoredExceptionHandler(g_VehCtx.vehHandle);
        g_VehCtx.vehHandle = nullptr;

        if (callerWantsSuspended)
        {
            SuspendThread(hThread);
        }

        LeaveCriticalSection(&g_CriticalSection);
        return hThread;
    }

    bool GetStubInfo(StubInfo& info)
    {
        if (!g_VehCtx.active) return false;
        EnterCriticalSection(&g_CriticalSection);
        info.stubAddress = (LPVOID)g_VehCtx.stubAddress;
        info.stubSize = 0;
        info.isActive = g_VehCtx.active;
        ZeroMemory(info.originalBytes, sizeof(info.originalBytes));
        LeaveCriticalSection(&g_CriticalSection);
        return true;
    }

    DllAnalysis* FindBestDllForStub() { return nullptr; }
    size_t GetLoadedModulesAnalysis(std::vector<DllAnalysis>& outList) { return 0; }
}