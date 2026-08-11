#include "FullScreenFix.hpp"
#include "Cheat.hpp"
#include <Main/Draw/Draw.hpp>

#include <Windows.h>
#include <d3d9.h>
#include <VehCpuHook/VehCpuHook.hpp>
#include <XorStr.hpp>

#pragma comment(lib, "d3d9.lib")

// =====================================================================
// Typedefs
// =====================================================================

typedef BOOL(WINAPI* FN_SetWindowPos)(
    HWND hWnd, HWND hWndInsertAfter,
    int X, int Y, int cx, int cy, UINT uFlags);

typedef HRESULT(STDMETHODCALLTYPE* FN_D3D9Reset)(
    IDirect3DDevice9* pDevice,
    D3DPRESENT_PARAMETERS* pPresentationParameters);

// =====================================================================
// Originals
// =====================================================================

static FN_SetWindowPos g_oSetWindowPos = nullptr;
static FN_D3D9Reset    g_oD3D9Reset = nullptr;

// =====================================================================
// Slot IDs (HWBP — 2 DR slots)
// =====================================================================

static int g_slotSetWindowPos = -1;
static int g_slotD3D9Reset = -1;

static bool g_hooksInitialized = false;

// =====================================================================
// HOOKS
// =====================================================================

static BOOL WINAPI hkSetWindowPos(
    HWND hWnd, HWND hWndInsertAfter,
    int X, int Y, int cx, int cy, UINT uFlags)
{
    if (!g_oSetWindowPos)
        return FALSE;

    HWND overlayWnd = Overlay::GetOverlayWindow();

    if (overlayWnd && hWnd == overlayWnd)
    {
        VehCpuHook::CallOriginal(g_slotSetWindowPos);
        return g_oSetWindowPos(hWnd, HWND_TOPMOST, X, Y, cx, cy, uFlags);
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);
    if (pid == GetCurrentProcessId())
    {
        if (hWndInsertAfter == HWND_TOPMOST)
            hWndInsertAfter = HWND_NOTOPMOST;
    }

    VehCpuHook::CallOriginal(g_slotSetWindowPos);
    return g_oSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

static HRESULT STDMETHODCALLTYPE hkD3D9Reset(
    IDirect3DDevice9* pDevice,
    D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    if (pPresentationParameters)
    {
        pPresentationParameters->Windowed = TRUE;
        pPresentationParameters->FullScreen_RefreshRateInHz = 0;
        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        if (pPresentationParameters->BackBufferFormat != D3DFMT_UNKNOWN)
            pPresentationParameters->BackBufferFormat = D3DFMT_UNKNOWN;
    }

    if (g_oD3D9Reset)
    {
        VehCpuHook::CallOriginal(g_slotD3D9Reset);
        return g_oD3D9Reset(pDevice, pPresentationParameters);
    }

    return D3D_OK;
}

// =====================================================================
// D3D9 vtable discovery + hook
// =====================================================================

static void SetupD3D9FullscreenHook()
{
    if (g_slotD3D9Reset >= 0)
        return;

    HMODULE hD3D9 = xorstr(L"d3d9.dll").use([](const wchar_t* s) {
        return GetModuleHandleW(s);
        });
    if (!hD3D9)
        return;

    using FN_Direct3DCreate9 = IDirect3D9 * (WINAPI*)(UINT);
    auto pDirect3DCreate9 = xorstr("Direct3DCreate9").use([&](const char* s) {
        return reinterpret_cast<FN_Direct3DCreate9>(GetProcAddress(hD3D9, s));
        });
    if (!pDirect3DCreate9)
        return;

    IDirect3D9* pD3D = pDirect3DCreate9(D3D_SDK_VERSION);
    if (!pD3D)
        return;

    D3DPRESENT_PARAMETERS d3dpp{};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = GetDesktopWindow();
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;

    IDirect3DDevice9* pDevice = nullptr;
    HRESULT hr = pD3D->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
        d3dpp.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp, &pDevice);

    if (SUCCEEDED(hr) && pDevice)
    {
        void** vtbl = *reinterpret_cast<void***>(pDevice);
        void* pReset = vtbl[16];

        if (pReset)
        {
            g_slotD3D9Reset = VehCpuHook::AddHook(
                pReset,
                &hkD3D9Reset,
                reinterpret_cast<void**>(&g_oD3D9Reset));
        }

        pDevice->Release();
    }

    pD3D->Release();
}

// =====================================================================
// PUBLIC
// =====================================================================

void FullScreenFixHooks()
{
    if (g_hooksInitialized)
        return;

    HMODULE hUser32 = xorstr(L"user32.dll").use([](const wchar_t* s) {
        return GetModuleHandleW(s);
        });
    if (!hUser32) {
        hUser32 = xorstr(L"user32.dll").use([](const wchar_t* s) {
            return LoadLibraryW(s);
            });
    }
    if (!hUser32)
        return;

    LPVOID pSetWindowPos = xorstr("SetWindowPos").use([&](const char* s) {
        return reinterpret_cast<LPVOID>(GetProcAddress(hUser32, s));
        });
    if (pSetWindowPos)
    {
        g_slotSetWindowPos = VehCpuHook::AddHook(
            pSetWindowPos,
            &hkSetWindowPos,
            reinterpret_cast<void**>(&g_oSetWindowPos));
    }

    SetupD3D9FullscreenHook();

    g_hooksInitialized = true;
}

void RemoveFullScreenFixHooks()
{
    if (!g_hooksInitialized)
        return;

    if (g_slotSetWindowPos >= 0)
    {
        VehCpuHook::RemoveHook(g_slotSetWindowPos);
        g_slotSetWindowPos = -1;
    }

    if (g_slotD3D9Reset >= 0)
    {
        VehCpuHook::RemoveHook(g_slotD3D9Reset);
        g_slotD3D9Reset = -1;
    }

    g_hooksInitialized = false;
}