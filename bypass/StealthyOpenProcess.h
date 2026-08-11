#pragma once
#include <Windows.h>

// ============================================================
// StealthyOpenProcess v5
//   - Zero CRT
//   - Thread-safe (CRITICAL_SECTION)
//   - HWBP (DR0) per-thread — sem race condition com threads alheias
//   - VEH SINGLE_STEP TID-gated — nao interfere com VehCpuHook
//   - Callstack limpa dentro de kernel32
// ============================================================

namespace StealthyOpenProcess {

    bool   Initialize();
    void   Shutdown();
    HANDLE Execute(DWORD dwProcessId, DWORD dwDesiredAccess = PROCESS_ALL_ACCESS);

} // namespace StealthyOpenProcess
