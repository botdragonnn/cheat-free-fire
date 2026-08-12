#pragma once
#include "Includes.hpp"
#include <Cheat/Globals.hpp>
#include <Render/Overlay/Overlay.hpp>
#include "Render/Fonts/Fonts.hpp"
#include <Render/Fonts/Bytes/IconsFontAwesome6.h>
#include <Custom.hpp>
#include <Utils/Utils.hpp>

class Interface
{
public:
    Interface(HWND Window, HWND TargetWindow, HDC DeviceContext, HGLRC RenderContext) {
        Initialize(Window, TargetWindow, DeviceContext, RenderContext);
    }
    ~Interface() {
        ShutDown();
    }

    void Initialize(HWND Window, HWND TargetWindow, HDC DeviceContext, HGLRC RenderContext);
    void InitializeMenu();
    void UpdateStyle();
    void RenderGui();
    void WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void HandleMenuKey();
    void ShutDown();
    bool GetMenuOpen() const { return bIsMenuOpen; }
    int  GetCurrentTab() const { return CurrentTab; }

private:
    HWND hWindow;
    HWND hTargetWindow;
    HDC hDeviceContext;
    HGLRC hRenderContext;
    bool bIsMenuOpen = false;
    int CurrentTab = 0;
    bool isLoading = false;
    char loadingMessage[64] = "";
public:
    UINT ResizeWidht;
    UINT ResizeHeight;
};
inline Interface* g_Interface = nullptr;