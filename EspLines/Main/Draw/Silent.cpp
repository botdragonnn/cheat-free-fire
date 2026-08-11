#include "Silent.hpp"
#include <intrin.h>
#include <Cheat/Globals.hpp>
#include <DynamicStub/DynamicStub.hpp>

namespace Silent {

    static volatile LONGLONG g_LocalPlayer = 0;
    static volatile LONGLONG g_TargetEntity = 0;
    volatile LONG g_Running = 0;

    static CRITICAL_SECTION g_MatrixCS;
    static Matrix4x4 g_ViewMatrix = {};
    static bool g_CSInitialized = false;

    static void EnsureCSInit() {
        if (!g_CSInitialized) {
            InitializeCriticalSection(&g_MatrixCS);
            g_CSInitialized = true;
        }
    }

    static __forceinline uintptr_t ReadPtr(uintptr_t addr, bool N32) {
        return N32 ? g_FreeFireMemory.Read<uint32_t>(addr) : g_FreeFireMemory.Read<uint64_t>(addr);
    }

    static void ThreadProc()
    {
        UINT32 rngState = (UINT32)__rdtsc();
        if (!rngState) rngState = 0xDEADBEEF;

        auto RngNext = [&]() -> UINT32 {
            rngState ^= rngState << 13;
            rngState ^= rngState >> 17;
            rngState ^= rngState << 5;
            return rngState;
            };
        auto RngFloat = [&]() -> float {
            return (float)(RngNext() & 0x00FFFFFF) / (float)0x01000000;
            };

        while (InterlockedCompareExchange(&g_Running, 0, 0) != 0 && !g_Globals.General.ShutDown)
        {
            if (!g_Globals.Silent.Enabled || !(GetAsyncKeyState(g_Globals.Silent.KeyBind) & 0x8000))
            {
                Sleep(3);
                continue;
            }

            const bool N32 = g_Globals.General.N32;

            const uintptr_t localPlayer = (uintptr_t)InterlockedCompareExchange64(&g_LocalPlayer, 0, 0);
            const uintptr_t targetEntity = (uintptr_t)InterlockedCompareExchange64(&g_TargetEntity, 0, 0);

            if (localPlayer == 0 || targetEntity == 0)
            {
                SwitchToThread();
                continue;
            }

            Memory::FlushTLB();

            const uintptr_t weaponPtr = ReadPtr(localPlayer + Offsets::Player::m_LastAimingInfoFromWeapon, N32);
            if (weaponPtr == 0)
            {
                SwitchToThread();
                continue;
            }

            const uintptr_t directionVA = weaponPtr + Offsets::HitObjectInfo::RayDir;
            const uintptr_t startPosVA = weaponPtr + Offsets::HitObjectInfo::StartPosition;

            uintptr_t dirPhys = 0;
            if (!g_FreeFireMemory.TranslateVA(directionVA, dirPhys))
            {
                SwitchToThread();
                continue;
            }

            uintptr_t startPhys = 0;
            g_FreeFireMemory.TranslateVA(startPosVA, startPhys);

            PageMapping writeMap;
            void* dirHostPtr = writeMap.ResolveWrite(dirPhys);
            if (!dirHostPtr)
            {
                SwitchToThread();
                continue;
            }

            Matrix4x4 cachedMatrix;
            EnterCriticalSection(&g_MatrixCS);
            cachedMatrix = g_ViewMatrix;
            LeaveCriticalSection(&g_MatrixCS);

            Vector3 cachedAimPos = {};
            Vector3 cachedShootOrigin = {};
            bool posValid = false;

            for (int i = 0; i < WRITE_LOOP_COUNT; ++i)
            {
                if ((i & 1023) == 0)
                {
                    if (InterlockedCompareExchange(&g_Running, 0, 0) == 0 || g_Globals.General.ShutDown)
                        break;

                    const uintptr_t curTarget = (uintptr_t)InterlockedCompareExchange64(&g_TargetEntity, 0, 0);
                    if (curTarget != targetEntity || curTarget == 0)
                        break;

                    if (!(GetAsyncKeyState(g_Globals.Silent.KeyBind) & 0x8000))
                        break;
                }

                if ((i & 127) == 0)
                {
                    Vector3 newAimPos = Transform::GetHeadPosition(targetEntity, N32);
                    if (newAimPos.X == 0.0f && newAimPos.Y == 0.0f && newAimPos.Z == 0.0f)
                    {
                        posValid = false;
                        continue;
                    }

                    if (!W2S::FovCheck(cachedMatrix, newAimPos, (float)g_Globals.Silent.Fov))
                        break;

                    newAimPos.Y += 0.05f;
                    cachedAimPos = newAimPos;

                    if (startPhys != 0)
                    {
                        PageMapping readMap;
                        readMap.Read<Vector3>(startPhys, cachedShootOrigin);
                    }
                    else
                    {
                        cachedShootOrigin = g_FreeFireMemory.Read<Vector3>(startPosVA);
                    }

                    if (cachedShootOrigin.X == 0.0f && cachedShootOrigin.Y == 0.0f && cachedShootOrigin.Z == 0.0f)
                    {
                        posValid = false;
                        continue;
                    }

                    posValid = true;

                    dirHostPtr = writeMap.ResolveWrite(dirPhys);
                    if (!dirHostPtr)
                        break;
                }

                if (!posValid)
                    continue;

                Vector3 dir;
                dir.X = cachedAimPos.X - cachedShootOrigin.X;
                dir.Y = cachedAimPos.Y - cachedShootOrigin.Y;
                dir.Z = cachedAimPos.Z - cachedShootOrigin.Z;

                if (dir.X == 0.0f && dir.Y == 0.0f && dir.Z == 0.0f)
                    continue;

                const float jitter = (RngFloat() * (SMOOTH_MAX - SMOOTH_MIN)) + SMOOTH_MIN;
                dir.X += (RngFloat() * jitter) - (jitter * 0.5f);
                dir.Y += (RngFloat() * jitter) - (jitter * 0.5f);
                dir.Z += (RngFloat() * jitter) - (jitter * 0.5f);

                memcpy(dirHostPtr, &dir, sizeof(Vector3));

                if ((i & 2047) == 0 && i > 0)
                {
                    writeMap.Release();
                    SwitchToThread();
                    dirHostPtr = writeMap.ResolveWrite(dirPhys);
                    if (!dirHostPtr)
                        break;
                }
            }
        }

        InterlockedExchange(&g_Running, 0);
    }

    static DWORD WINAPI ThreadProcWrapper(LPVOID)
    {
        ThreadProc();
        return 0;
    }

    void Start()
    {
        EnsureCSInit();
        if (InterlockedExchange(&g_Running, 1) != 0)
            return;
        DynamicStub::CreateThreadWithDynamicStub(ThreadProcWrapper, nullptr);
    }

    void Stop()
    {
        InterlockedExchange(&g_Running, 0);
        InterlockedExchange64(&g_LocalPlayer, 0);
        InterlockedExchange64(&g_TargetEntity, 0);
    }

    void UpdateViewMatrix(const Matrix4x4& matrix)
    {
        EnsureCSInit();
        EnterCriticalSection(&g_MatrixCS);
        g_ViewMatrix = matrix;
        LeaveCriticalSection(&g_MatrixCS);
    }

    void SetTarget(uintptr_t localPlayer, uintptr_t targetEntity)
    {
        InterlockedExchange64(&g_LocalPlayer, (LONGLONG)localPlayer);
        InterlockedExchange64(&g_TargetEntity, (LONGLONG)targetEntity);
    }

    void ClearTarget()
    {
        InterlockedExchange64(&g_TargetEntity, 0);
    }
}
