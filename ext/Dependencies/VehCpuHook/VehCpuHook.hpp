#pragma once
#include <Windows.h>

#ifdef _WIN64
#define VEH_XIP Rip
#else
#define VEH_XIP Eip
#endif

class VehCpuHook
{
public:
    static bool Initialize();
    static void Shutdown();

    static int  AddHook(void* pTarget, void* pDetour, void** ppOriginal);
    static bool RemoveHook(int slot);
    static void CallOriginal(int slot);

private:
    struct Slot
    {
        uintptr_t target;
        uintptr_t detour;
        bool active;
    };

    static Slot s_Slots[4];
    static PVOID s_Veh;
    static volatile LONG s_Skip[4];

    static LONG WINAPI Handler(EXCEPTION_POINTERS* ep);
    static void SetDROnAllThreads(int slot, uintptr_t addr, bool enable);
};