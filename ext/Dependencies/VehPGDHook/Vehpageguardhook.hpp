#pragma once
#include <Windows.h>

#ifdef _WIN64
#define XIP Rip
#define XRCX Rcx
#else
#define XIP Eip
#define XRCX Ecx
#endif

// VEH Hook para capturar VmInstancePtr (baseado no LeoHook)
// Sem mutex, sem STL complexa
class VEHCapture {
public:
    static bool Install(uintptr_t targetFunc);
    static bool Remove();
    static void* GetCaptured() { return (void*)s_CapturedValue; }
    static bool WasCaptured() { return s_Captured; }

private:
    static LONG WINAPI Handler(EXCEPTION_POINTERS* pExceptionInfo);
    static bool AreInSamePage(uintptr_t addr1, uintptr_t addr2);

    static uintptr_t s_TargetFunc;
    static uintptr_t s_CapturedValue;
    static PVOID s_VehHandle;
    static DWORD s_OldProtect;
    static volatile bool s_Captured;
    static volatile bool s_Active;
};
