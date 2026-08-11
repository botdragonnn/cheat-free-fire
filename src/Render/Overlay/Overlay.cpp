#include "Overlay.hpp"
#include <Utils/Utils.hpp>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

//struct WndRECT : public RECT {
//    int Width() { return right - left; }
//    int Height() { return bottom - top; }
//};
//
//static std::function<void(HWND, UINT, WPARAM, LPARAM)> pWindowProc;
//
//bool bSettuped = false;
//bool bInitialized = false;
//bool bGLInitialized = false;
//
//HWND hWindow = nullptr;
//WNDCLASSEX WindowClass{};
//HWND hTargetWindow = nullptr;
//WndRECT wTargetWindowRect{};
//
//HDC  hDeviceContext = nullptr;
//HGLRC hRenderContext = nullptr;
//
//// --- PFD igual ao original (RGBA 32, alpha 8, depth 24, stencil 8) ---
//static bool SetMyPixelFormat(HDC hDC) {
//    PIXELFORMATDESCRIPTOR pfd = {
//        sizeof(PIXELFORMATDESCRIPTOR), 1,
//        (WORD)(PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER),
//        PFD_TYPE_RGBA,
//        32, 0,0,0,0,0,0,
//        8,  // alpha
//        0,
//        0, 0,0,0,0,
//        24, // depth
//        8,  // stencil
//        0
//    };
//    int pf = ChoosePixelFormat(hDC, &pfd);
//    if (pf == 0) { Console::Log("[SetPixelFormat] ChoosePixelFormat -> Error: ", (DWORD)GetLastError()); return false; }
//    if (!SetPixelFormat(hDC, pf, &pfd)) { Console::Log("[SetPixelFormat] SetPixelFormat -> Error: ", (DWORD)GetLastError()); return false; }
//    return true;
//}
//
//static bool CreateGLContext(HDC hDC) {
//    hRenderContext = wglCreateContext(hDC);
//    if (!hRenderContext) { Console::Log("[CreateGLContext] wglCreateContext -> Error: ", (DWORD)GetLastError()); return false; }
//    if (!wglMakeCurrent(hDC, hRenderContext)) { Console::Log("[CreateGLContext] wglMakeCurrent -> Error: ", (DWORD)GetLastError()); return false; }
//    return true;
//}
//
//static void InitializeOpenGL() {
//    // estado que garante alpha por-pixel
//    glEnable(GL_BLEND);
//    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//    glDisable(GL_DEPTH_TEST);
//    glDisable(GL_CULL_FACE);
//    // se você não usa GL fixed pipeline, isso não afeta o ImGui moderno, mas não atrapalha:
//    glDisable(GL_LIGHTING);
//    glDisable(GL_TEXTURE_2D);
//
//    glClearColor(0.f, 0.f, 0.f, 0.f); // fundo totalmente transparente
//    glClear(GL_COLOR_BUFFER_BIT);
//    bGLInitialized = true;
//}
//
//static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
//    if (uMsg == WM_ERASEBKGND) return 1; // evita GDI pintar qualquer fundo
//    if (pWindowProc) pWindowProc(hWnd, uMsg, wParam, lParam);
//    return DefWindowProc(hWnd, uMsg, wParam, lParam);
//}
//
//namespace Overlay {
//    std::wstring WindowClassName;
//
//    void Setup(HWND TargetHWND) {
//        if (!TargetHWND) {
//            Console::Log("[Overlay] Setup -> Invalid target window handle");
//            return;
//        }
//        hTargetWindow = TargetHWND;
//        if (GetClientRect(hTargetWindow, &wTargetWindowRect)) {
//            MapWindowPoints(hTargetWindow, nullptr, reinterpret_cast<LPPOINT>(&wTargetWindowRect), 2);
//            bSettuped = true;
//        }
//        else {
//            Console::Log("[Overlay] Setup -> Failed to get target window rect");
//        }
//    }
//
//    void Initialize() {
//        if (!bSettuped) {
//            Console::Log("[Overlay] Initialize -> Overlay not setup");
//            return;
//        }
//
//        WindowClassName = Utils::RandomString(5); // volta pro tamanho original
//        if (!WindowClassName.empty())
//            UnregisterClass(WindowClassName.c_str(), GetModuleHandle(NULL));
//
//        ZeroMemory(&WindowClass, sizeof(WNDCLASSEX));
//        WindowClass.cbSize = sizeof(WindowClass);
//        WindowClass.style = CS_HREDRAW | CS_VREDRAW;
//        WindowClass.lpfnWndProc = WindowProc;
//        WindowClass.hInstance = GetModuleHandle(NULL);
//        WindowClass.hIcon = NULL;
//        WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
//        WindowClass.hbrBackground = NULL;                // << voltou ao original (sem brush)
//        WindowClass.lpszMenuName = NULL;
//        WindowClass.lpszClassName = WindowClassName.c_str();
//        WindowClass.hIconSm = NULL;
//
//        if (!RegisterClassEx(&WindowClass)) {
//            Console::Log("[Overlay] Initialize -> RegisterClassEx Error: ", (DWORD)GetLastError());
//            return;
//        }
//
//        // mesma flag da sua base + LAYERED
//        hWindow = CreateWindowEx(
//            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED,
//            WindowClassName.c_str(), L"Overlay",
//            WS_POPUP | WS_VISIBLE,
//            wTargetWindowRect.left, wTargetWindowRect.top,
//            wTargetWindowRect.Width(), wTargetWindowRect.Height(),
//            nullptr, nullptr, GetModuleHandle(nullptr), nullptr
//        );
//        if (!hWindow) {
//            Console::Log("[Overlay] Initialize -> CreateWindowEx Error: ", (DWORD)GetLastError());
//            return;
//        }
//
//        // Alpha global 0 => janela base invisível; o que aparece é o que você desenhar com alpha > 0
//        SetLayeredWindowAttributes(hWindow, 0, 0, LWA_ALPHA);
//
//        // Vidro total quando o DWM está ativo (melhor caminho no Optimus)
//        BOOL compositionEnabled = FALSE;
//        if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)) && compositionEnabled) {
//            MARGINS margins = { -1, -1, -1, -1 };
//            DwmExtendFrameIntoClientArea(hWindow, &margins);
//        }
//
//        ShowWindow(hWindow, SW_SHOW);
//        UpdateWindow(hWindow);
//
//        bInitialized = true;
//
//        // --- OpenGL ---
//        hDeviceContext = GetDC(hWindow);
//        if (!hDeviceContext) { Console::Log("[glInitialize] GetDC Error: ", (DWORD)GetLastError()); return; }
//        if (!SetMyPixelFormat(hDeviceContext)) { Console::Log("[glInitialize] SetPixelFormat failed", 0u); return; }
//        if (!CreateGLContext(hDeviceContext)) { Console::Log("[glInitialize] CreateGLContext failed", 0u); return; }
//        InitializeOpenGL();
//
//        // clear inicial transparente visualizado
//        if (bGLInitialized) {
//            wglMakeCurrent(hDeviceContext, hRenderContext);
//            glClearColor(0.f, 0.f, 0.f, 0.f);
//            glClear(GL_COLOR_BUFFER_BIT);
//            SwapBuffers(hDeviceContext);
//        }
//    }
//
//    void ShutDown() {
//        // OpenGL
//        if (hRenderContext) {
//            wglMakeCurrent(nullptr, nullptr);
//            wglDeleteContext(hRenderContext);
//            hRenderContext = nullptr;
//        }
//        if (hDeviceContext) {
//            ReleaseDC(hWindow, hDeviceContext);
//            hDeviceContext = nullptr;
//        }
//        bGLInitialized = false;
//
//        // Window
//        if (hWindow) {
//            DestroyWindow(hWindow);
//            hWindow = nullptr;
//        }
//        if (!WindowClassName.empty()) {
//            UnregisterClass(WindowClassName.c_str(), GetModuleHandle(NULL));
//            WindowClassName.clear();
//        }
//
//        bInitialized = false;
//        bSettuped = false;
//    }
//
//    void UpdateWindowPos() {
//        if (!hTargetWindow || !hWindow) return;
//        WndRECT r{};
//        GetClientRect(hTargetWindow, &r);
//        MapWindowPoints(hTargetWindow, nullptr, reinterpret_cast<LPPOINT>(&r), 2);
//        MoveWindow(hWindow, r.left, r.top, r.Width(), r.Height(), FALSE); // igual ao original
//    }
//
//    void SetupWindowProcHook(std::function<void(HWND, UINT, WPARAM, LPARAM)> Funtion) {
//        pWindowProc = Funtion;
//    }
//
//    void glInitialize() {}
//    void glRefresh() {
//        if (hDeviceContext && hRenderContext) {
//            wglMakeCurrent(hDeviceContext, hRenderContext);
//            glClear(GL_COLOR_BUFFER_BIT);
//            SwapBuffers(hDeviceContext);
//        }
//    }
//
//    ImVec2 GetTargetWindowSize() {
//        if (!hTargetWindow) return ImVec2(0, 0);
//        RECT rect{};
//        if (GetClientRect(hTargetWindow, &rect)) {
//            return ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
//        }
//        return ImVec2(0, 0);
//    }
//
//    HGLRC glGetContext() { return hRenderContext; }
//    HDC   glGetDeviceContext() { return hDeviceContext; }
//    bool  IsSettuped() { return bSettuped; }
//    bool  IsInitialized() { return bInitialized; }
//    HWND  GetOverlayWindow() { return hWindow; }
//    HWND  GetTargetWindow() { return hTargetWindow; }
//}

struct WndRECT : public RECT {
    int Width() { return right - left; }
    int Height() { return bottom - top; }
};

static std::function<void(HWND, UINT, WPARAM, LPARAM)> pWindowProc;

bool bSettuped = false;
bool bInitialized = false;
bool bGLInitialized = false;

HWND hWindow = nullptr;
WNDCLASSEX WindowClass;
HWND hTargetWindow = nullptr;
WndRECT wTargetWindowRect{};

HDC hDeviceContext = nullptr;
HGLRC hRenderContext = nullptr;

std::wstring g_ClassName{};

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (pWindowProc) {
        pWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

namespace Overlay {

    void Setup(HWND TargetHWND) {
        if (!TargetHWND) {
            Console::Log("[Overlay::Setup] Invalid target window handle");
            return;
        }

        hTargetWindow = TargetHWND;
        if (GetClientRect(hTargetWindow, &wTargetWindowRect)) {
            MapWindowPoints(hTargetWindow, nullptr, reinterpret_cast<LPPOINT>(&wTargetWindowRect), 2);
            bSettuped = true;
        }
        else {
            Console::Log("[Overlay::Setup] Failed to get target window rect. Error: ", GetLastError());
        }
    }

    void Initialize() {
        if (!bSettuped) {
            Console::Log("[Overlay::Initialize] Overlay not setup");
            return;
        }

        srand(static_cast<unsigned int>(time(0)));
        g_ClassName = (L"Window_") + Utils::RandomString(2);
        if (g_ClassName.c_str() != nullptr) {
            if ((UnregisterClass)(g_ClassName.c_str(), GetModuleHandle(NULL))) {
                Console::Log("[Overlay::Initialize] Successfully unregistered existing window class.");
            }
        }
        else {
            Console::Log("[Overlay::Initialize] Failed to unregister existing window class. Error code: ", GetLastError());
        }

        ZeroMemory(&WindowClass, sizeof(WNDCLASSEX));
        WindowClass.cbSize = sizeof(WindowClass);
        WindowClass.style = CS_HREDRAW | CS_VREDRAW;
        WindowClass.lpfnWndProc = WindowProc;
        WindowClass.cbClsExtra = 0;
        WindowClass.cbWndExtra = 0;
        WindowClass.hInstance = GetModuleHandle(NULL);
        WindowClass.hIcon = NULL;
        WindowClass.hCursor = NULL;
        WindowClass.hbrBackground = NULL;
        WindowClass.lpszMenuName = NULL;
        WindowClass.lpszClassName = g_ClassName.c_str();
        WindowClass.hIconSm = NULL;

        if (!RegisterClassEx(&WindowClass)) {
            Console::Log("[Overlay::Initialize] RegisterClassEx Error: ", GetLastError());
            return;
        }

        hWindow = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED,
            WindowClass.lpszClassName, WindowClass.lpszMenuName, WS_POPUP | WS_VISIBLE,
            wTargetWindowRect.left, wTargetWindowRect.top, wTargetWindowRect.Width(), wTargetWindowRect.Height(),
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr
        );

        if (!hWindow) {
            Console::Log("[Overlay::Initialize] CreateWindowEx Error: ", GetLastError());
            return;
        }

        if (!SetLayeredWindowAttributes(hWindow, RGB(0, 0, 0), 255, LWA_ALPHA)) {
            Console::Log("[Overlay::Initialize] SetLayeredWindowAttributes Error: ", GetLastError());
        }

        BOOL compositionEnabled = FALSE;
        if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)) && compositionEnabled) {
            MARGINS Margins = { wTargetWindowRect.left, wTargetWindowRect.top, wTargetWindowRect.Width(), wTargetWindowRect.Height() };
            HRESULT hr = DwmExtendFrameIntoClientArea(hWindow, &Margins);
            if (FAILED(hr)) {
                Console::Log("[Overlay::Initialize] DwmExtendFrameIntoClientArea Failed. HR: ", (uint64_t)hr);
            }
        }

        ShowWindow(hWindow, SW_SHOW);
        UpdateWindow(hWindow);

        glInitialize();
        if (bGLInitialized) {
            wglMakeCurrent(hDeviceContext, hRenderContext);
            glClearColor(0.f, 0.f, 0.f, 0.f);
            glClear(GL_COLOR_BUFFER_BIT);
            SwapBuffers(hDeviceContext);
        }
        bInitialized = true;
    }

    bool SetPixelFormat(HDC hDC) {
        if (!hDC) {
            Console::Log("[Overlay::SetPixelFormat] Invalid HDC");
            return false;
        }

        PIXELFORMATDESCRIPTOR pfd = {
            sizeof(PIXELFORMATDESCRIPTOR),
            1,
            (WORD)(PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER),
            PFD_TYPE_RGBA,
            32,0,0,0,0,0,0,
            8,
            0,
            0,0,0,0,/*0,0,0,*/
            24,
            8,
            0
        };

        int pixelFormat = ChoosePixelFormat(hDC, &pfd);
        if (pixelFormat == 0) {
            Console::Log("[Overlay::SetPixelFormat] ChoosePixelFormat Error: ", GetLastError());
            return false;
        }

        if (!::SetPixelFormat(hDC, pixelFormat, &pfd)) {
            Console::Log("[Overlay::SetPixelFormat] SetPixelFormat Error: ", GetLastError());
            return false;
        }

        return true;
    }

    bool CreateGLContext(HDC hDC) {
        if (!hDC) {
            Console::Log("[Overlay::CreateGLContext] Invalid HDC");
            return false;
        }

        hRenderContext = wglCreateContext(hDC);
        if (!hRenderContext) {
            Console::Log("[Overlay::CreateGLContext] wglCreateContext Error: ", GetLastError());
            return false;
        }

        if (!wglMakeCurrent(hDC, hRenderContext)) {
            Console::Log("[Overlay::CreateGLContext] wglMakeCurrent Error: ", GetLastError());
            wglDeleteContext(hRenderContext);
            hRenderContext = nullptr;
            return false;
        }

        return true;
    }

    void ShutDown() {
        glShutDown();

        if (hWindow) {
            if (!DestroyWindow(hWindow)) {
                Console::Log("[Overlay::ShutDown] DestroyWindow Error: ", GetLastError());
            }
            hWindow = nullptr;
        }

        if (!g_ClassName.empty() && WindowClass.hInstance) {
            if (!UnregisterClassW(g_ClassName.c_str(), WindowClass.hInstance)) {
                Console::Log("[OVERLAY] UnregisterClassW failed: ", GetLastError());
            }
        }

        WindowClass = WNDCLASSEX{};
        g_ClassName.clear();

        bInitialized = false;
        bSettuped = false;
    }

    void UpdateWindowPos() {
        if (!hTargetWindow || !hWindow) {
            Console::Log("[Overlay::UpdateWindowPos] Invalid window handle(s)");
            return;
        }

        WndRECT TargetWindowRect{};
        if (!GetClientRect(hTargetWindow, &TargetWindowRect)) {
            Console::Log("[Overlay::UpdateWindowPos] GetClientRect Error: ", GetLastError());
            return;
        }

        MapWindowPoints(hTargetWindow, nullptr, reinterpret_cast<LPPOINT>(&TargetWindowRect), 2);
        if (!MoveWindow(hWindow, TargetWindowRect.left, TargetWindowRect.top,
            TargetWindowRect.Width(), TargetWindowRect.Height(), false)) {
            Console::Log("[Overlay::UpdateWindowPos] MoveWindow Error: ", GetLastError());
        }
    }

    void SetupWindowProcHook(std::function<void(HWND, UINT, WPARAM, LPARAM)> Funtion) {
        pWindowProc = Funtion;
    }

    void glInitialize() {
        if (!hWindow) {
            Console::Log("[Overlay::glInitialize] hWindow is null");
            return;
        }

        hDeviceContext = GetDC(hWindow);
        if (!hDeviceContext) {
            Console::Log("[Overlay::glInitialize] GetDC Error: ", GetLastError());
            return;
        }

        if (!SetPixelFormat(hDeviceContext)) {
            Console::Log("[Overlay::glInitialize] SetPixelFormat failed");
            return;
        }

        if (!CreateGLContext(hDeviceContext)) {
            Console::Log("[Overlay::glInitialize] CreateGLContext failed");
            return;
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);

        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);

        bGLInitialized = true;
        Console::Log("[Overlay] OpenGL initialized successfully");
    }

    void glRefresh() {
        if (!hDeviceContext || !hRenderContext) {
            Console::Log("[Overlay::glRefresh] Context not ready");
            return;
        }

        if (!wglMakeCurrent(hDeviceContext, hRenderContext)) {
            Console::Log("[Overlay::glRefresh] wglMakeCurrent Error: ", GetLastError());
            return;
        }

        if (!SwapBuffers(hDeviceContext)) {
            Console::Log("[Overlay::glRefresh] SwapBuffers Error: ", GetLastError());
        }
    }

    void glShutDown() {
        if (hRenderContext) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(hRenderContext);
            hRenderContext = nullptr;
        }

        if (hDeviceContext) {
            ReleaseDC(hWindow, hDeviceContext);
            hDeviceContext = nullptr;
        }

        bGLInitialized = false;
    }

    ImVec2 GetTargetWindowSize() {
        if (!hTargetWindow) {
            Console::Log("[Overlay::GetTargetWindowSize] hTargetWindow is null");
            return ImVec2(0, 0);
        }

        RECT rect{};
        if (GetClientRect(hTargetWindow, &rect)) {
            return ImVec2(static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top));
        }

        Console::Log("[Overlay::GetTargetWindowSize] GetClientRect Error: ", GetLastError());
        return ImVec2(0, 0);
    }

    HGLRC glGetContext() { return hRenderContext; }
    HDC glGetDeviceContext() { return hDeviceContext; }
    bool IsSettuped() { return bSettuped; }
    bool IsInitialized() { return bInitialized; }
    HWND GetOverlayWindow() { return hWindow; }
    HWND GetTargetWindow() { return hTargetWindow; }
}