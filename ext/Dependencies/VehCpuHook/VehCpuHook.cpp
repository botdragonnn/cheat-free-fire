#include "VehCpuHook.hpp"
#include <TlHelp32.h>

VehCpuHook::Slot VehCpuHook::s_Slots[4] = {};
PVOID VehCpuHook::s_Veh = nullptr;
volatile LONG VehCpuHook::s_Skip[4] = {};

bool VehCpuHook::Initialize()
{
    if (s_Veh) return true;
    ZeroMemory(s_Slots, sizeof(s_Slots));
    for (int i = 0; i < 4; i++) s_Skip[i] = 0;
    s_Veh = AddVectoredExceptionHandler(1, Handler);
    return s_Veh != nullptr;
}

void VehCpuHook::Shutdown()
{
    for (int i = 0; i < 4; i++)
    {
        if (s_Slots[i].active)
            RemoveHook(i);
    }
    if (s_Veh)
    {
        RemoveVectoredExceptionHandler(s_Veh);
        s_Veh = nullptr;
    }
}

int VehCpuHook::AddHook(void* pTarget, void* pDetour, void** ppOriginal)
{
    int slot = -1;
    for (int i = 0; i < 4; i++)
    {
        if (!s_Slots[i].active) { slot = i; break; }
    }
    if (slot == -1) return -1;

    s_Slots[slot].target = (uintptr_t)pTarget;
    s_Slots[slot].detour = (uintptr_t)pDetour;
    s_Slots[slot].active = true;
    InterlockedExchange(&s_Skip[slot], 0);

    if (ppOriginal)
        *ppOriginal = pTarget;

    SetDROnAllThreads(slot, (uintptr_t)pTarget, true);
    return slot;
}

bool VehCpuHook::RemoveHook(int slot)
{
    if (slot < 0 || slot > 3 || !s_Slots[slot].active)
        return false;

    SetDROnAllThreads(slot, 0, false);

    s_Slots[slot].active = false;
    s_Slots[slot].target = 0;
    s_Slots[slot].detour = 0;
    InterlockedExchange(&s_Skip[slot], 0);
    return true;
}

void VehCpuHook::CallOriginal(int slot)
{
    if (slot >= 0 && slot < 4)
        InterlockedExchange(&s_Skip[slot], 1);
}

LONG WINAPI VehCpuHook::Handler(EXCEPTION_POINTERS* ep)
{
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
        return EXCEPTION_CONTINUE_SEARCH;

    uintptr_t ip = ep->ContextRecord->VEH_XIP;

    for (int i = 0; i < 4; i++)
    {
        Slot slot = s_Slots[i];
        if (!slot.active) continue;
        if (ip != slot.target) continue;

        if (InterlockedCompareExchange(&s_Skip[i], 0, 1) == 1)
        {
            ep->ContextRecord->EFlags |= (1 << 16);
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        ep->ContextRecord->VEH_XIP = slot.detour;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void VehCpuHook::SetDROnAllThreads(int slot, uintptr_t addr, bool enable)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();

    THREADENTRY32 te = {};
    te.dwSize = sizeof(te);

    if (Thread32First(snap, &te))
    {
        do
        {
            if (te.th32OwnerProcessID != pid) continue;

            HANDLE hThread = OpenThread(
                THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                FALSE, te.th32ThreadID);
            if (!hThread) continue;

            bool isSelf = (te.th32ThreadID == tid);
            if (!isSelf) SuspendThread(hThread);

            CONTEXT ctx = {};
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

            if (GetThreadContext(hThread, &ctx))
            {
                uintptr_t* dr = &ctx.Dr0;
                dr[slot] = enable ? addr : 0;

                if (enable)
                {
                    ctx.Dr7 |= (1ULL << (slot * 2));
                    ctx.Dr7 &= ~(0xFULL << (16 + slot * 4));
                }
                else
                {
                    ctx.Dr7 &= ~(1ULL << (slot * 2));
                }

                ctx.Dr6 = 0;
                SetThreadContext(hThread, &ctx);
            }

            if (!isSelf) ResumeThread(hThread);
            CloseHandle(hThread);

        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}