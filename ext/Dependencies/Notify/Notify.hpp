#pragma once
#include <Windows.h>
#include <vector>
#include <chrono>
#include <imgui.h>
#include <Utils/Utils.hpp>
#include <Render/Fonts/Fonts.hpp>
#include <Render/Overlay/Overlay.hpp>
#include <Render/Fonts/Bytes/IconsFontAwesome6.h>

namespace NotifyManager
{
    enum eType {
        None,
        Info,
        Warning
    };

    enum eState {
        In,
        Current,
        Out,
        Expired
    };

    struct NotifyAnim_t {
        std::string Id;
        float SlideX = 0.f;
        float YPos = 0.f;
        float Alpha = 0.f;
        float TargetY = 0.f;
    };

    class NotifyClass {
    private:
        const char* Title = nullptr;  // Ponteiro para literal - não aloca no heap!
        std::string Description;
        time_t ExpireTime = 0;
        time_t CreationTime = 0;
        eType Type = eType::None;
        eState CurrentState = eState::In;

    public:
        void SetTitle(const char* NewTitle);
        void SetDescription(std::string NewDesc);
        void SetType(eType NewType);
        void SetState(eState NewState);
        void SetCreationTime(time_t NewCreationTime);

        const char* GetTitle();
        std::string GetDescription();
        time_t GetExpireTime();
        time_t GetCreationTime();
        eType GetType();
        eState GetCurrentState();

        time_t GetNowTime();
        time_t GetTimeDiff();

        NotifyClass(eType Type, time_t ExpireTime = 4000);
    };

    extern std::vector<NotifyClass> NotifyList;
    extern std::vector<NotifyAnim_t> AnimList;

    void DeleteNotify(int Index);
    void Render();
    void Send(std::string Description, time_t ExpireTime = 4000);
    void Cleanup();  // Limpar todos os buffers
}