#include "Vehpageguardhook.hpp"

// Inicialização das variáveis estáticas
uintptr_t VEHCapture::s_TargetFunc = 0;
uintptr_t VEHCapture::s_CapturedValue = 0;
PVOID VEHCapture::s_VehHandle = nullptr;
DWORD VEHCapture::s_OldProtect = 0;
volatile bool VEHCapture::s_Captured = false;
volatile bool VEHCapture::s_Active = false;

LONG WINAPI VEHCapture::Handler(EXCEPTION_POINTERS* pExceptionInfo)
{
    // Tratar STATUS_GUARD_PAGE_VIOLATION
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION)
    {
        // Verificar se estamos no endereço alvo
        if (pExceptionInfo->ContextRecord->XIP == s_TargetFunc)
        {
            // Capturar RCX (primeiro argumento = VmInstancePtr)
            if (!s_Captured && s_Active)
            {
                s_CapturedValue = pExceptionInfo->ContextRecord->XRCX;
                if (s_CapturedValue != 0)
                {
                    s_Captured = true;
                }
            }
        }

        // Setar Trap Flag para gerar SINGLE_STEP após próxima instrução
        pExceptionInfo->ContextRecord->EFlags |= 0x100;

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // Tratar STATUS_SINGLE_STEP (re-aplicar PAGE_GUARD)
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
    {
        // Re-aplicar PAGE_GUARD se ainda estamos ativos
        if (s_Active && !s_Captured)
        {
            DWORD dwOld;
            VirtualProtect((LPVOID)s_TargetFunc, 1, PAGE_EXECUTE_READ | PAGE_GUARD, &dwOld);
        }

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

bool VEHCapture::AreInSamePage(uintptr_t addr1, uintptr_t addr2)
{
    MEMORY_BASIC_INFORMATION mbi1;
    if (!VirtualQuery((LPCVOID)addr1, &mbi1, sizeof(mbi1)))
        return true;

    MEMORY_BASIC_INFORMATION mbi2;
    if (!VirtualQuery((LPCVOID)addr2, &mbi2, sizeof(mbi2)))
        return true;

    return (mbi1.BaseAddress == mbi2.BaseAddress);
}

bool VEHCapture::Install(uintptr_t targetFunc)
{
    if (s_VehHandle != nullptr)
        return false;

    s_TargetFunc = targetFunc;
    s_CapturedValue = 0;
    s_Captured = false;
    s_Active = true;

    // Registrar VEH handler
    s_VehHandle = AddVectoredExceptionHandler(TRUE, (PVECTORED_EXCEPTION_HANDLER)Handler);
    if (!s_VehHandle)
        return false;

    // Aplicar PAGE_GUARD
    if (!VirtualProtect((LPVOID)targetFunc, 1, PAGE_EXECUTE_READ | PAGE_GUARD, &s_OldProtect))
    {
        RemoveVectoredExceptionHandler(s_VehHandle);
        s_VehHandle = nullptr;
        return false;
    }

    return true;
}

bool VEHCapture::Remove()
{
    s_Active = false;

    if (!s_VehHandle)
        return false;

    // Restaurar proteção original
    DWORD dwOld;
    VirtualProtect((LPVOID)s_TargetFunc, 1, s_OldProtect, &dwOld);

    // Remover VEH handler
    RemoveVectoredExceptionHandler(s_VehHandle);
    s_VehHandle = nullptr;

    return true;
}
