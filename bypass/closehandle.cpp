#include <windows.h>
#include <new>
#include <chrono>
#include <Cheat/Globals.hpp>
#include <DynamicStub/DynamicStub.hpp>

static inline bool IsLikelyUnknownExitStatus(DWORD code)
{
    if (code == 0 || code == 259 /* STILL_ACTIVE */) return false;

    switch (code) {
    case 0xC0000005: // STATUS_ACCESS_VIOLATION
    case 0xC000013A: // STATUS_CONTROL_C_EXIT
    case 0x40000015: // STATUS_FATAL_APP_EXIT
        return false;
    }
    return (code & 0xFFFF0000u) != 0;
}

static size_t CloseUnknownExitThreadHandles()
{
    typedef LONG NTSTATUS;
    typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);

    const ULONG SystemExtendedHandleInformationClass = 64;
    const NTSTATUS STATUS_INFO_LENGTH_MISMATCH = (NTSTATUS)0xC0000004;

    typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
        PVOID      Object;
        ULONG_PTR  UniqueProcessId;
        ULONG_PTR  HandleValue;
        ULONG      GrantedAccess;
        USHORT     CreatorBackTraceIndex;
        USHORT     ObjectTypeIndex;
        ULONG      HandleAttributes;
        ULONG      Reserved;
    } SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, * PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

    typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
        ULONG_PTR NumberOfHandles;
        ULONG_PTR Reserved;
        SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
    } SYSTEM_HANDLE_INFORMATION_EX, * PSYSTEM_HANDLE_INFORMATION_EX;

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return 0;

    auto NtQuerySystemInformation =
        reinterpret_cast<NtQuerySystemInformation_t>(GetProcAddress(ntdll, "NtQuerySystemInformation"));
    if (!NtQuerySystemInformation) return 0;

    DWORD selfPid = GetCurrentProcessId();
    HANDLE self = GetCurrentProcess();

    size_t closed = 0;
    ULONG bufSize = 1u << 20;
    BYTE* buffer = nullptr;

    for (;;) {
        delete[] buffer;
        buffer = new (std::nothrow) BYTE[bufSize];
        if (!buffer) return closed;

        ULONG needed = 0;
        NTSTATUS st = NtQuerySystemInformation(SystemExtendedHandleInformationClass, buffer, bufSize, &needed);

        if (st == STATUS_INFO_LENGTH_MISMATCH) {
            bufSize = (needed > bufSize) ? needed : (bufSize << 1);
            continue;
        }

        if (st < 0) {
            delete[] buffer;
            return closed;
        }
        break;
    }

    auto info = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION_EX>(buffer);

    for (ULONG_PTR i = 0; i < info->NumberOfHandles; ++i) {
        const auto& e = info->Handles[i];
        if ((DWORD)(ULONG_PTR)e.UniqueProcessId != selfPid)
            continue;

        HANDLE hSource = (HANDLE)(ULONG_PTR)e.HandleValue;
        HANDLE hDup = nullptr;

        if (!DuplicateHandle(self, hSource, self, &hDup, THREAD_QUERY_LIMITED_INFORMATION, FALSE, 0)) {
            if (!DuplicateHandle(self, hSource, self, &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS))
                continue;
        }

        DWORD tid = GetThreadId(hDup);
        if (tid == 0) {
            CloseHandle(hDup);
            continue;
        }

        DWORD exitCode = 0;
        if (!GetExitCodeThread(hDup, &exitCode)) {
            CloseHandle(hDup);
            continue;
        }

        CloseHandle(hDup);

        if (!IsLikelyUnknownExitStatus(exitCode))
            continue;

        DWORD flags = 0;
        if (GetHandleInformation(hSource, &flags) && (flags & HANDLE_FLAG_PROTECT_FROM_CLOSE)) {
            SetHandleInformation(hSource, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0);
        }

        if (CloseHandle(hSource)) {
            ++closed;
        }
    }

    delete[] buffer;
    return closed;
}

// 🧵 Thread que monitora e fecha continuamente handles "Unknown"
static DWORD WINAPI MonitorUnknownExitHandlesThread(LPVOID lpParameter)
{
    int emptyChecks = 0;

    while (true)
    {
        // ✅ Se o sistema estiver sendo desligado, encerra imediatamente
        if (g_Globals.General.ShutDown)
            break;

        size_t count = CloseUnknownExitThreadHandles();

        Sleep(1);

        if (count == 0)
        {
            emptyChecks++;

            // se não encontrou nada em 3 verificações seguidas (~30s), encerra
            if (emptyChecks >= 3)
                break;

            Sleep(10000); // 10 segundos
        }
        else
        {
            // reset contador e dorme rápido pra continuar varrendo
            emptyChecks = 0;
            Sleep(500);
        }
    }

    return 0;
}

// Handle da thread de monitoramento para shutdown seguro
static HANDLE g_MonitorThreadHandle = nullptr;

// 🚀 Inicia o monitoramento automático usando DynamicStub
static void StartHandleMonitor()
{
    // Criar thread com stub - Start Address será ucrtbase.dll ou msvcr100.dll
    g_MonitorThreadHandle = DynamicStub::CreateThreadWithDynamicStub(MonitorUnknownExitHandlesThread, nullptr, 0, nullptr );
}

// Para a thread de monitoramento e aguarda sua finalização
static void StopHandleMonitor()
{
    if ( !g_MonitorThreadHandle )
        return;

    // ShutDown já deve estar true, aguarda a thread sair (máx 3s)
    if ( WaitForSingleObject( g_MonitorThreadHandle, 3000 ) == WAIT_TIMEOUT )
    {
        TerminateThread( g_MonitorThreadHandle, 0 );
    }

    CloseHandle( g_MonitorThreadHandle );
    g_MonitorThreadHandle = nullptr;
}