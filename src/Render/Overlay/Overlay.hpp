#pragma once
#include "Includes.hpp"

namespace Overlay
{
    void Setup(HWND TargetHWND);
    void Initialize();
    void ShutDown();

    void UpdateWindowPos();
    void SetupWindowProcHook(std::function<void(HWND, UINT, WPARAM, LPARAM)> Funtion);

    bool IsSettuped();
    bool IsInitialized();
    HWND GetOverlayWindow();
    HWND GetTargetWindow();
    ImVec2 GetTargetWindowSize();

    // OpenGL
    void glInitialize();
    void glRefresh();
    void glShutDown();

    HGLRC glGetContext();
    HDC glGetDeviceContext();
}