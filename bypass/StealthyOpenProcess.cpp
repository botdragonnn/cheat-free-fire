#include "StealthyOpenProcess.h"
#include <XorStr.hpp>

// ============================================================
// StealthyOpenProcess v5 — Hardware Breakpoint, zero CRT, thread-safe
//
// Cria thread suspensa em stub de kernel32, arma HWBP (DR0)
// no stub address, redireciona via VEH SINGLE_STEP para
// OpenProcess e ExitThread. Callstack inteira dentro de kernel32.
//
// Vantagens vs v4 (PAGE_GUARD):
//   - HWBP eh per-thread: threads alheias NAO consomem o breakpoint
//   - Elimina race condition de re-guard (reGuardTid removido)
//   - Nao altera protecao de pagina (zero VirtualProtect PAGE_GUARD)
//   - Nenhum SINGLE_STEP "vazado" para outros handlers
//   - Compativel com VehCpuHook e VEHCapture (TID-gated)
//   - Stubs nao precisam estar em paginas diferentes
//
// Nota: DR0 eh usado na thread criada por nos, que eh efemera
// (<5ms). VehCpuHook configura DRs em threads existentes
// apenas durante AddHook/RemoveHook (init/shutdown), portanto
// nao ha conflito com os slots de DR ja em uso.
// ============================================================

namespace StealthyOpenProcess {

    // ============================================================
    // Tipos internos
    // ============================================================

    enum class Stage : int
    {
        Idle = 0,
        WaitingStubA,
        WaitingStubB,
        Completed
    };

    struct HWBPContext
    {
        volatile Stage  stage;
        DWORD           targetThreadId;

        uintptr_t       stubA;
        uintptr_t       stubB;

        DWORD           desiredAccess;
        DWORD           processId;

        uintptr_t       pOpenProcess;
        uintptr_t       pExitThread;

        HANDLE          resultHandle;
        PVOID           vehHandle;
    };

    // ============================================================
    // Estado global (protegido por s_Lock)
    // ============================================================

    static CRITICAL_SECTION s_Lock;
    static bool             s_Initialized = false;
    static HWBPContext      s_Ctx;

    // ============================================================
    // CRT-free helpers
    // ============================================================

    static void ZeroCtx()
    {
        SecureZeroMemory(&s_Ctx, sizeof(s_Ctx));
    }

    // ============================================================
    // DR0 helpers — arma/desarma execution breakpoint no slot 0
    //
    // DR7 layout para slot 0:
    //   Bit 0:     L0  (local enable)
    //   Bits 16-17: R/W0 (00 = execute)
    //   Bits 18-19: LEN0 (00 = 1 byte)
    // ============================================================

    static void ArmDR0(CONTEXT* ctx, uintptr_t addr)
    {
        ctx->Dr0 = addr;
        ctx->Dr7 |= 1ULL;             // L0 = 1
        ctx->Dr7 &= ~(0xFULL << 16);  // R/W0=00, LEN0=00
    }

    static void DisarmDR0(CONTEXT* ctx)
    {
        ctx->Dr0 = 0;
        ctx->Dr7 &= ~1ULL;            // L0 = 0
    }

    // ============================================================
    // VEH Handler — HWBP dois estagios, TID-gated
    //
    // So trata EXCEPTION_SINGLE_STEP da nossa thread.
    // Qualquer outra excecao/thread → EXCEPTION_CONTINUE_SEARCH
    // para VehCpuHook, VEHCapture, etc.
    // ============================================================

    static __declspec(noinline) LONG WINAPI VEHHandler(EXCEPTION_POINTERS* ep)
    {
        if (ep->ExceptionRecord->ExceptionCode != STATUS_SINGLE_STEP)
            return EXCEPTION_CONTINUE_SEARCH;

        DWORD tid = GetCurrentThreadId();
        if (tid != s_Ctx.targetThreadId)
            return EXCEPTION_CONTINUE_SEARCH;

        uintptr_t rip = (uintptr_t)ep->ContextRecord->Rip;

        // ---- Stage A: thread atingiu stubA → redireciona para OpenProcess ----
        if (s_Ctx.stage == Stage::WaitingStubA && rip == s_Ctx.stubA)
        {
            // Alinha RSP (ABI x64: 16-byte aligned antes do CALL)
            ep->ContextRecord->Rsp &= ~0xFull;
            ep->ContextRecord->Rsp -= 8;
            *(uintptr_t*)(ep->ContextRecord->Rsp) = s_Ctx.stubB;

            // OpenProcess(dwDesiredAccess, FALSE, dwProcessId)
            ep->ContextRecord->Rcx = (DWORD64)s_Ctx.desiredAccess;
            ep->ContextRecord->Rdx = (DWORD64)FALSE;
            ep->ContextRecord->R8  = (DWORD64)s_Ctx.processId;
            ep->ContextRecord->Rip = s_Ctx.pOpenProcess;

            // Move DR0 para stubB (Stage B)
            ArmDR0(ep->ContextRecord, s_Ctx.stubB);

            // RF (Resume Flag): previne re-fire no novo RIP
            ep->ContextRecord->EFlags |= 0x10000;

            s_Ctx.stage = Stage::WaitingStubB;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // ---- Stage B: OpenProcess retornou → captura resultado, ExitThread ----
        if (s_Ctx.stage == Stage::WaitingStubB && rip == s_Ctx.stubB)
        {
            s_Ctx.resultHandle = (HANDLE)ep->ContextRecord->Rax;

            ep->ContextRecord->Rsp &= ~0xFull;
            ep->ContextRecord->Rsp -= 8;
            *(uintptr_t*)(ep->ContextRecord->Rsp) = 0;

            // ExitThread(0)
            ep->ContextRecord->Rcx = 0;
            ep->ContextRecord->Rip = s_Ctx.pExitThread;

            // Desarma DR0 — nao precisamos mais
            DisarmDR0(ep->ContextRecord);

            // RF
            ep->ContextRecord->EFlags |= 0x10000;

            s_Ctx.stage = Stage::Completed;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // Nao eh nosso breakpoint esperado — passa para outros handlers
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // ============================================================
    // Busca par de stubs em kernel32 (executable, enderecos distintos)
    //
    // Com HWBP nao precisam estar em paginas diferentes.
    // Os stubs nunca executam — o breakpoint dispara ANTES.
    // ============================================================

    static bool TryPair(HMODULE hK32,
        const char* nameA, const char* nameB,
        uintptr_t& outA, uintptr_t& outB)
    {
        void* pA = GetProcAddress(hK32, nameA);
        void* pB = GetProcAddress(hK32, nameB);
        if (!pA || !pB)    return false;
        if (pA == pB)      return false;

        MEMORY_BASIC_INFORMATION mbiA, mbiB;
        SecureZeroMemory(&mbiA, sizeof(mbiA));
        SecureZeroMemory(&mbiB, sizeof(mbiB));

        if (!VirtualQuery(pA, &mbiA, sizeof(mbiA))) return false;
        if (!VirtualQuery(pB, &mbiB, sizeof(mbiB))) return false;

        DWORD execMask = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE;
        if (!(mbiA.Protect & execMask) || !(mbiB.Protect & execMask))
            return false;

        outA = (uintptr_t)pA;
        outB = (uintptr_t)pB;
        return true;
    }

    static bool FindStubPair(HMODULE hK32, uintptr_t& outA, uintptr_t& outB)
    {
        // Pares de funcoes nao sensiveis em kernel32.
        // Com HWBP nao precisam estar em paginas diferentes.
        // Ordem: mais comuns/estaveis primeiro, fallback para locales.

        if (TryPair(hK32, XorStr("GetProfileIntA"), XorStr("lstrcpyW"), outA, outB)) return true;
        if (TryPair(hK32, XorStr("GetProfileStringA"), XorStr("lstrcatA"), outA, outB)) return true;
        if (TryPair(hK32, XorStr("WriteProfileStringA"), XorStr("GetPrivateProfileIntA"), outA, outB)) return true;
        if (TryPair(hK32, XorStr("GetProfileIntW"), XorStr("lstrcpyA"), outA, outB)) return true;
        if (TryPair(hK32, XorStr("GetLocaleInfoA"), XorStr("IsDBCSLeadByte"), outA, outB)) return true;
        if (TryPair(hK32, XorStr("GetDateFormatA"), XorStr("GetTimeFormatA"), outA, outB)) return true;
        if (TryPair(hK32, XorStr("CompareStringA"), XorStr("LCMapStringA"), outA, outB)) return true;
        if (TryPair(hK32, XorStr("GetSystemDefaultLangID"), XorStr("GetOEMCP"), outA, outB)) return true;

        return false;
    }

    // ============================================================
    // Initialize / Shutdown
    // ============================================================

    bool Initialize()
    {
        if (s_Initialized) return true;
        InitializeCriticalSection(&s_Lock);
        ZeroCtx();
        s_Initialized = true;
        return true;
    }

    void Shutdown()
    {
        if (!s_Initialized) return;

        EnterCriticalSection(&s_Lock);

        if (s_Ctx.vehHandle)
        {
            s_Ctx.stage = Stage::Idle;
            RemoveVectoredExceptionHandler(s_Ctx.vehHandle);
            s_Ctx.vehHandle = nullptr;
        }

        ZeroCtx();
        LeaveCriticalSection(&s_Lock);

        DeleteCriticalSection(&s_Lock);
        s_Initialized = false;
    }

    // ============================================================
    // Execute — abre handle para processo alvo
    //
    // Thread-safe: chamadas concorrentes serializadas por
    // CRITICAL_SECTION. Cada chamada cria uma thread efemera
    // com HWBP proprio — sem interferencia com threads alheias.
    // ============================================================

    __declspec(noinline) HANDLE Execute(DWORD dwProcessId, DWORD dwDesiredAccess)
    {
        // Lazy init
        if (!s_Initialized)
            Initialize();

        EnterCriticalSection(&s_Lock);

        // ---- Resolve kernel32 ----
        HMODULE hK32 = xorstr("kernel32.dll").use([](const char* s) {
            return GetModuleHandleA(s);
            });

        if (!hK32)
        {
            LeaveCriticalSection(&s_Lock);
            return nullptr;
        }

        // ---- Par de stubs ----
        uintptr_t stubA = 0, stubB = 0;
        if (!FindStubPair(hK32, stubA, stubB))
        {
            LeaveCriticalSection(&s_Lock);
            return nullptr;
        }

        // ---- Resolve APIs ----
        void* pOpenProcess = xorstr("OpenProcess").use([&](const char* s) -> void* {
            return (void*)GetProcAddress(hK32, s);
            });
        void* pExitThread = xorstr("ExitThread").use([&](const char* s) -> void* {
            return (void*)GetProcAddress(hK32, s);
            });

        if (!pOpenProcess || !pExitThread)
        {
            LeaveCriticalSection(&s_Lock);
            return nullptr;
        }

        // ---- Contexto ----
        ZeroCtx();
        s_Ctx.stubA         = stubA;
        s_Ctx.stubB         = stubB;
        s_Ctx.desiredAccess = dwDesiredAccess;
        s_Ctx.processId     = dwProcessId;
        s_Ctx.pOpenProcess  = (uintptr_t)pOpenProcess;
        s_Ctx.pExitThread   = (uintptr_t)pExitThread;
        s_Ctx.stage         = Stage::Idle;

        // ---- Registra VEH (prioridade maxima) ----
        s_Ctx.vehHandle = AddVectoredExceptionHandler(TRUE, VEHHandler);
        if (!s_Ctx.vehHandle)
        {
            ZeroCtx();
            LeaveCriticalSection(&s_Lock);
            return nullptr;
        }

        // ---- Cria thread suspensa no stub de kernel32 ----
        DWORD threadId = 0;
        HANDLE hThread = CreateThread(
            nullptr, 0,
            (LPTHREAD_START_ROUTINE)stubA,
            nullptr,
            CREATE_SUSPENDED,
            &threadId);

        if (!hThread)
        {
            RemoveVectoredExceptionHandler(s_Ctx.vehHandle);
            ZeroCtx();
            LeaveCriticalSection(&s_Lock);
            return nullptr;
        }

        s_Ctx.targetThreadId = threadId;

        // ---- Arma HWBP (DR0) na thread suspensa ----
        // DR0 = stubA, tipo = execucao, tamanho = 1 byte
        // Thread nova tem DRs zerados — sem conflito com VehCpuHook
        // que so configura DRs de threads existentes em AddHook/RemoveHook.
        CONTEXT ctx;
        SecureZeroMemory(&ctx, sizeof(ctx));
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

        if (!GetThreadContext(hThread, &ctx))
        {
            TerminateThread(hThread, 0);
            CloseHandle(hThread);
            RemoveVectoredExceptionHandler(s_Ctx.vehHandle);
            ZeroCtx();
            LeaveCriticalSection(&s_Lock);
            return nullptr;
        }

        ArmDR0(&ctx, stubA);

        if (!SetThreadContext(hThread, &ctx))
        {
            TerminateThread(hThread, 0);
            CloseHandle(hThread);
            RemoveVectoredExceptionHandler(s_Ctx.vehHandle);
            ZeroCtx();
            LeaveCriticalSection(&s_Lock);
            return nullptr;
        }

        // ---- Resume e espera ----
        s_Ctx.stage = Stage::WaitingStubA;
        ResumeThread(hThread);

        // OpenProcess eh quase instantaneo — 5s eh mais que suficiente.
        DWORD wait = WaitForSingleObject(hThread, 5000);

        if (wait == WAIT_TIMEOUT)
        {
            wait = WaitForSingleObject(hThread, 5000);

            if (wait == WAIT_TIMEOUT)
            {
                // Thread travou. DR morrem junto com a thread.
                TerminateThread(hThread, 0);
                WaitForSingleObject(hThread, 2000);
            }
        }

        // ---- Thread morta — seguro limpar tudo ----
        CloseHandle(hThread);

        Stage finalStage = s_Ctx.stage;
        HANDLE result = nullptr;

        if (finalStage == Stage::Completed)
        {
            result = s_Ctx.resultHandle;
        }
        else
        {
            if (s_Ctx.resultHandle)
            {
                CloseHandle(s_Ctx.resultHandle);
                s_Ctx.resultHandle = nullptr;
            }
        }

        // ---- Remove VEH ----
        s_Ctx.stage = Stage::Idle;
        RemoveVectoredExceptionHandler(s_Ctx.vehHandle);
        s_Ctx.vehHandle = nullptr;

        ZeroCtx();
        LeaveCriticalSection(&s_Lock);
        return result;
    }

} // namespace StealthyOpenProcess
