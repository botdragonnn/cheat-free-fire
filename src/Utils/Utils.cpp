#include "Utils.hpp"
#include <ranges>
#include <TlHelp32.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

// Classe usada pelo NtQuerySystemInformation
#define SystemHandleInformation 0x10

// Estruturas necess�rias
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO {
    USHORT UniqueProcessId;
    USHORT CreatorBackTraceIndex;
    UCHAR  ObjectTypeIndex;
    UCHAR  HandleAttributes;
    USHORT HandleValue;
    PVOID  Object;
    ULONG  GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, * PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

typedef struct _SYSTEM_HANDLE_INFORMATION {
    ULONG NumberOfHandles;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

std::wstring Utils::RandomString(size_t Length) {
    auto Randchar = []() -> char {
        const char* Charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        const size_t MaxIndex = (sizeof(Charset) - 1);
        return Charset[rand() % MaxIndex];
        };

    std::wstring Str(Length, 0);
    std::generate_n(Str.begin(), Length, Randchar);
    return Str;
}

ImVec2 Utils::CalcTextSize(ImFont* Font, int Size, const char* Label) {
    return Font->CalcTextSizeA(Size, FLT_MAX, 0, Label);
}

FILE* fDummy = nullptr;

bool Utils::IsProcessElevated()
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return false;

    TOKEN_ELEVATION elevation = {};
    DWORD dwSize = 0;

    BOOL ok = GetTokenInformation(
        hToken,
        TokenElevation,
        &elevation,
        sizeof(elevation),
        &dwSize
    );

    CloseHandle(hToken);

    if (!ok)
        return false;

    return elevation.TokenIsElevated != 0;
}

void Console::InitConsole()
{
    if (!(AllocConsole)())
        return;

    freopen_s(&fDummy, ("CONIN$"), ("r"), stdin);
    std::cin.clear();
    freopen_s(&fDummy, ("CONOUT$"), ("w"), stdout);
    std::cout.clear();
    freopen_s(&fDummy, ("CONOUT$"), ("w"), stderr);
    std::cerr.clear();
    std::clog.clear();

    (SetConsoleOutputCP)(CP_UTF8);
    (SetConsoleCP)(CP_UTF8);
    (SetConsoleTitleA)(("DLL Debug Console"));
}

void Console::ShutdownConsole()
{
    if (fDummy) fclose(fDummy);
    (FreeConsole)();
}

bool Utils::EnableDebugPrivilege()
{
    HANDLE hToken = nullptr;

    if (!OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
        &hToken))
    {
        return false;
    }

    LUID luid;
    if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid))
    {
        CloseHandle(hToken);
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr))
    {
        CloseHandle(hToken);
        return false;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}

bool Utils::DisableDebugPrivilege()
{
    HANDLE hToken = nullptr;

    if (!OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
        &hToken))
    {
        return false;
    }

    LUID luid;
    if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid))
    {
        CloseHandle(hToken);
        return false;
    }

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = 0; // <<< REMOVE / DESATIVA

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr))
    {
        CloseHandle(hToken);
        return false;
    }

    // IMPORTANTE: sempre checar isso
    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}

