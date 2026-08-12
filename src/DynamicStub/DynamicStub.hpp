#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace DynamicStub
{
    struct StubInfo
    {
        HMODULE hModule;
        LPVOID stubAddress;
        SIZE_T stubSize;
        BYTE originalBytes[64];
        std::wstring moduleName;
        bool isActive;
    };

    struct DllAnalysis
    {
        HMODULE hModule;
        std::wstring name;
        LPBYTE baseAddress;
        SIZE_T moduleSize;
        DWORD threadCount;
        DWORD totalCycles;
    };

    bool Initialize();
    void Shutdown();
    DllAnalysis* FindBestDllForStub();
    size_t GetLoadedModulesAnalysis(std::vector<DllAnalysis>& outList);
    HANDLE CreateThreadWithDynamicStub(
        LPTHREAD_START_ROUTINE lpStartAddress,
        LPVOID lpParameter,
        DWORD dwCreationFlags = 0,
        LPDWORD lpThreadId = nullptr
    );
    bool GetStubInfo(StubInfo& info);
    void CleanupStub();
}