#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>

#include <Cheat/Cheat.hpp>
#include <Cheat/Globals.hpp>
#include <Utils/Utils.hpp>
#include <bypass/closehandle.cpp>
#include <DynamicStub/DynamicStub.hpp>

#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
#define IMGUI_DISABLE_GAMEPAD_SUPPORT
#define DISCORD_DISABLE_IO_THREAD
#define JM_XORSTR_DISABLE_AVX_INTRINSICS

HMODULE g_hModule = nullptr;
HANDLE g_MainFinishedEvent = nullptr;
HANDLE g_MainThreadHandle = nullptr;

static DWORD WINAPI MainThread(LPVOID /*param*/)
{
#ifdef _DEBUG
    Console::InitConsole();
#endif
    Cheat::Initialize();

    if (g_MainFinishedEvent)
        SetEvent(g_MainFinishedEvent);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID address)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);

        if (address) {
            __try {
                char* src = (char*)address;
                size_t len = strnlen_s(src, 1024);
                if (len > 0 && len < 1024) {
                    char* copy = (char*)malloc(len + 1);
                    if (copy) {
                        memcpy(copy, src, len);
                        copy[len] = '\0';
                        g_Globals.General.Local = copy;

                        __try {
                            ZeroMemory(address, len);
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER) {}
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                g_Globals.General.Local = nullptr;
            }
        }
        else {
            g_Globals.General.Local = nullptr;
        }

        g_MainFinishedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        DynamicStub::Initialize();
        g_MainThreadHandle = DynamicStub::CreateThreadWithDynamicStub(MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
