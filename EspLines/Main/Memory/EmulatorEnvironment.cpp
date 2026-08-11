#include "EmulatorEnvironment.hpp"

#include <windows.h>
#include <cwctype>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <Utils/Utils.hpp>
#include <XorStr.hpp>

#pragma comment(lib, "ntdll.lib")

static auto ToLowerW = [](std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
    };

static auto WContainsI = [](const std::wstring& hay, const std::wstring& needle) {
    return ToLowerW(hay).find(ToLowerW(needle)) != std::wstring::npos;
    };

static auto EndsWithI = [](const std::wstring& s, const std::wstring& suf) {
    if (s.size() < suf.size()) return false;
    return ToLowerW(s.substr(s.size() - suf.size())) == ToLowerW(suf);
    };

static auto StripDevicePrefix = [](std::wstring p) {
    if (p.rfind(L"\\\\?\\", 0) == 0) p.erase(0, 4);
    return p;
    };

extern "C" NTSTATUS NTAPI NtQuerySystemInformation(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

#ifndef SystemExtendedHandleInformation
#define SystemExtendedHandleInformation 64
#endif

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
    PVOID       Object;
    ULONG_PTR   UniqueProcessId;
    ULONG_PTR   HandleValue;
    ULONG       GrantedAccess;
    USHORT      CreatorBackTraceIndex;
    USHORT      ObjectTypeIndex;
    ULONG       HandleAttributes;
    ULONG       Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
    ULONG_PTR NumberOfHandles;
    ULONG_PTR Reserved;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
} SYSTEM_HANDLE_INFORMATION_EX, * PSYSTEM_HANDLE_INFORMATION_EX;

static auto DetectAbiFromFastbootVdiHandle = []() -> ABIType {
    const DWORD selfPid = GetCurrentProcessId();
    ULONG needed = 0;
    std::vector<BYTE> buf(1 << 20);
    NTSTATUS st = 0;

    for (int tries = 0; tries < 6; ++tries) {
        st = NtQuerySystemInformation(SystemExtendedHandleInformation, buf.data(), (ULONG)buf.size(), &needed);
        if (st == 0) break;
        buf.resize(needed ? (size_t)needed + (1 << 16) : buf.size() * 2);
    }

    if (st != 0) return ABIType::X86;

    auto* info = (PSYSTEM_HANDLE_INFORMATION_EX)buf.data();
    wchar_t pathBuf[4096];

    for (ULONG_PTR i = 0; i < info->NumberOfHandles; ++i) {
        const auto& h = info->Handles[i];
        if ((DWORD)(ULONG_PTR)h.UniqueProcessId != selfPid)
            continue;

        HANDLE hFile = (HANDLE)(ULONG_PTR)h.HandleValue;
        DWORD flags = 0;
        if (!GetHandleInformation(hFile, &flags))
            continue;

        DWORD len = GetFinalPathNameByHandleW(hFile, pathBuf, (DWORD)std::size(pathBuf), FILE_NAME_NORMALIZED);
        if (len == 0 || len >= std::size(pathBuf))
            continue;

        std::wstring p = StripDevicePrefix(std::wstring(pathBuf, len));
        if (EndsWithI(p, XorStr(L"\\fastboot.vdi")) || EndsWithI(p, XorStr(L"/fastboot.vdi"))) {
            return WContainsI(p, XorStr(L"pie64")) ? ABIType::X86_64 : ABIType::X86;
        }
    }

    return ABIType::X86;
    };

EmulatorEnvironment& GetEmulatorEnv() {
    static EmulatorEnvironment env;
    return env;
}

const char* EmulatorEnvironment::ABIToString(ABIType abi) {
    // static pra manter os ponteiros vivos apos retorno
    static auto xs_x64 = xorstr("X86_64");
    static auto xs_x86 = xorstr("X86");
    static auto xs_unk = xorstr("Unknown");
    static bool init = false;
    if (!init) { xs_x64.crypt(); xs_x86.crypt(); xs_unk.crypt(); init = true; }

    switch (abi) {
    case ABIType::X86_64: return xs_x64.get();
    case ABIType::X86:    return xs_x86.get();
    default:              return xs_unk.get();
    }
}

std::string EmulatorEnvironment::EmulatorName() const {
    switch (emulator_) {
    case EmulatorType::Msi4:              return XorStr("Msi4");
    case EmulatorType::Msi5:              return XorStr("Msi5");
    case EmulatorType::BlueStacks4:       return XorStr("BlueStacks 4");
    case EmulatorType::BlueStacks5:       return XorStr("BlueStacks 5");
    case EmulatorType::BlueStacks5Beta:   return XorStr("BlueStacks 5 Beta");
    case EmulatorType::Bluestacks5China:  return XorStr("BlueStacks 5 China");
    default:                              return XorStr("Unknown");
    }
}

void EmulatorEnvironment::DetectEmulator() {
    wchar_t pathW[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, pathW, MAX_PATH)) {
        emulator_ = EmulatorType::Unknown;
        return;
    }

    const std::wstring exePath(pathW);

    if (exePath.find(XorStr(L"BlueStacks_msi2")) != std::wstring::npos) {
        emulator_ = EmulatorType::Msi4;
    }
    else if (exePath.find(XorStr(L"BlueStacks_msi5")) != std::wstring::npos) {
        emulator_ = EmulatorType::Msi5;
    }
    else if (exePath.find(XorStr(L"BlueStacks_nxt_cn")) != std::wstring::npos) {
        emulator_ = EmulatorType::Bluestacks5China;
    }
    else if (exePath.find(XorStr(L"BlueStacks_nxt")) != std::wstring::npos) {
        emulator_ = EmulatorType::BlueStacks5;
    }
    else if (exePath.find(XorStr(L"BlueStacks_arabica")) != std::wstring::npos) {
        emulator_ = EmulatorType::BlueStacks5;
    }
    else if (exePath.find(XorStr(L"BlueStacks")) != std::wstring::npos) {
        emulator_ = EmulatorType::BlueStacks4;
    }
    else {
        emulator_ = EmulatorType::Unknown;
    }

    Console::Log(std::string(XorStr("Emulador Detectado: ")) + EmulatorName());
}

void EmulatorEnvironment::QueryVersion() {
    auto WideToUtf8 = [](const std::wstring& w) -> std::string {
        if (w.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string result(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &result[0], len, nullptr, nullptr);
        return result;
        };

    if (emulator_ == EmulatorType::Unknown) {
        version_.clear();
        return;
    }

    // Registry subkeys — std::wstring copia antes do temporário morrer
    std::wstring subKey;

    switch (emulator_) {
    case EmulatorType::BlueStacks4:      subKey = XorStr(L"SOFTWARE\\BlueStacks");        break;
    case EmulatorType::BlueStacks5:      subKey = XorStr(L"SOFTWARE\\BlueStacks_nxt");    break;
    case EmulatorType::Bluestacks5China: subKey = XorStr(L"SOFTWARE\\BlueStacks_nxt_cn"); break;
    case EmulatorType::Msi4:             subKey = XorStr(L"SOFTWARE\\BlueStacks_msi2");   break;
    case EmulatorType::Msi5:             subKey = XorStr(L"SOFTWARE\\BlueStacks_msi5");   break;
    default:
        version_.clear();
        return;
    }

    HKEY hKey = nullptr;
    LONG status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
    if (status != ERROR_SUCCESS) {
        version_.clear();
        return;
    }

    DWORD type = 0;
    DWORD size = 0;

    status = RegQueryValueExW(hKey, XorStr(L"Version"), nullptr, &type, nullptr, &size);
    if (status != ERROR_SUCCESS || type != REG_SZ || size == 0) {
        RegCloseKey(hKey);
        version_.clear();
        return;
    }

    std::wstring verW(size / sizeof(wchar_t), L'\0');

    status = RegQueryValueExW(hKey, XorStr(L"Version"), nullptr, &type, reinterpret_cast<LPBYTE>(&verW[0]), &size);

    RegCloseKey(hKey);

    if (status != ERROR_SUCCESS) {
        version_.clear();
        return;
    }

    const size_t nulPos = verW.find(L'\0');
    if (nulPos != std::wstring::npos)
        verW.resize(nulPos);

    version_ = WideToUtf8(verW);
    Console::Log(std::string(XorStr("Versao do Emulador: ")) + version_);
}

void EmulatorEnvironment::DetectABI()
{
    abi_ = ABIType::Unknown;

    if (emulator_ == EmulatorType::Msi4 || emulator_ == EmulatorType::BlueStacks4)
    {
        // Inline comparisons — cada XorStr temporário vive durante a comparação
        bool isX64 =
            version_ == XorStr("4.280.4.4002") ||
            version_ == XorStr("4.260.25.4001") ||
            version_ == XorStr("4.240.30.4004") ||
            version_ == XorStr("4.230.10.4001") ||
            version_ == XorStr("4.220.0.4001") ||
            version_ == XorStr("4.210.0.4009") ||
            version_ == XorStr("4.200.0.4012") ||
            version_ == XorStr("4.180.0.4004") ||
            version_ == XorStr("4.150.13.4102");

        abi_ = isX64 ? ABIType::X86_64 : ABIType::X86;
        Console::Log(std::string(XorStr("[ABI] BS4/MSI4 version=")) + version_ + " -> " + ABIToString(abi_));
        return;
    }

    if (emulator_ == EmulatorType::BlueStacks5 || emulator_ == EmulatorType::Msi5 || emulator_ == EmulatorType::BlueStacks5Beta || emulator_ == EmulatorType::Bluestacks5China)
    {
        static bool cached = false;
        static ABIType cachedAbi = ABIType::X86;

        if (!cached) {
            cachedAbi = DetectAbiFromFastbootVdiHandle();
            cached = true;
        }

        abi_ = cachedAbi;
        Console::Log(std::string(XorStr("[ABI] BS5/MSI5 detectado via handle fastboot.vdi -> ")) + ABIToString(abi_));
        return;
    }

    abi_ = ABIType::X86;
    Console::Log(std::string(XorStr("[ABI] Emulator desconhecido, assumindo X86")));
}

void EmulatorEnvironment::Refresh() {
    DetectEmulator();
    QueryVersion();
    DetectABI();
}

bool KernelOffsetSelector::ApplyInitTaskOffsets(const EmulatorEnvironment& env, uint32_t& kva_init_task_32, uint64_t& kva_init_task_64)
{
    auto SplitVersion = [](const std::string& ver) -> std::vector<int> {
        std::vector<int> parts;
        std::stringstream ss(ver);
        std::string item;
        while (std::getline(ss, item, '.')) {
            try { parts.push_back(std::stoi(item)); }
            catch (...) { parts.push_back(0); }
        }
        return parts;
        };

    auto VersionGreaterOrEqual = [&](const std::string& v1, const std::string& v2) -> bool {
        auto a = SplitVersion(v1);
        auto b = SplitVersion(v2);

        const size_t n = max(a.size(), b.size());
        a.resize(n, 0);
        b.resize(n, 0);

        for (size_t i = 0; i < n; ++i) {
            if (a[i] > b[i]) return true;
            if (a[i] < b[i]) return false;
        }
        return true;
        };

    const auto emu = env.Emulator();
    const auto& ver = env.Version();

    if (emu == EmulatorType::BlueStacks5Beta) {
        if (VersionGreaterOrEqual(ver, XorStr("5.0.0.7129"))) {
            kva_init_task_32 = 0xE19bbb00u;
            return true;
        }
        return false;
    }

    if (emu == EmulatorType::BlueStacks5 || emu == EmulatorType::Msi5 || emu == EmulatorType::Bluestacks5China) {
        if (VersionGreaterOrEqual(ver, XorStr("5.22.125.1001"))) {
            kva_init_task_32 = 0xE19C5B00u;
            kva_init_task_64 = 0xffffffff822147c0ULL;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("5.21.120.1025"))) {
            kva_init_task_32 = 0xE19c5b00u;
            kva_init_task_64 = 0xffffffff822147c0ULL;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("5.10.250.1004"))) {
            kva_init_task_32 = 0xE19c3b00u;
            kva_init_task_64 = 0xffffffff822147c0ULL;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("5.8.101.1001"))) {
            kva_init_task_32 = 0xE19b5b00u;
            kva_init_task_64 = 0xffffffff822147c0ULL;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("5.4.0.1063"))) {
            kva_init_task_32 = 0xE19b5b00u;
            kva_init_task_64 = 0xffffffff82014740ULL;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("5.2.100.1047"))) {
            kva_init_task_32 = 0xE19b3b00u;
            kva_init_task_64 = 0xffffffff82014740ULL;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("5.2.0.1052"))) {
            kva_init_task_32 = 0xE19b3b00u;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("5.1.110.1005"))) {
            kva_init_task_32 = 0xE19b5b00u;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("5.0.100.1002"))) {
            kva_init_task_32 = 0xE19bdb00u;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("5.0.50.1030"))) {
            kva_init_task_32 = 0xE19bbb00u;
            return true;
        }
        return false;
    }

    if (emu == EmulatorType::BlueStacks4 || emu == EmulatorType::Msi4) {
        if (VersionGreaterOrEqual(ver, XorStr("4.240.30.1002"))) {
            kva_init_task_32 = 0xE19bbb00u;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("4.230.20.1001"))) {
            kva_init_task_32 = 0xC19bbb00u;
            return true;
        }
        if (VersionGreaterOrEqual(ver, XorStr("4.80.0.1060"))) {
            kva_init_task_32 = 0xC17ecb00u;
            return true;
        }
        return false;
    }

    return false;
}