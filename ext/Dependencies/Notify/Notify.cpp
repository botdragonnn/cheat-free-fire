#include "Notify.hpp"

namespace NotifyManager
{
    std::vector<NotifyClass> NotifyList;
    std::vector<NotifyAnim_t> AnimList;

    void NotifyClass::SetTitle(const char* NewTitle) { this->Title = NewTitle; }
    void NotifyClass::SetDescription(std::string NewDesc) { this->Description = NewDesc; }
    void NotifyClass::SetType(eType NewType) { this->Type = NewType; }
    void NotifyClass::SetState(eState NewState) { this->CurrentState = NewState; }
    void NotifyClass::SetCreationTime(time_t NewCreationTime) { this->CreationTime = NewCreationTime; }

    const char* NotifyClass::GetTitle() { return Title ? Title : ""; }
    std::string NotifyClass::GetDescription() { return Description; }
    time_t NotifyClass::GetExpireTime() { return ExpireTime; }
    time_t NotifyClass::GetCreationTime() { return CreationTime; }
    eType NotifyClass::GetType() { return Type; }
    eState NotifyClass::GetCurrentState() { return CurrentState; }

    time_t NotifyClass::GetNowTime() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    time_t NotifyClass::GetTimeDiff() {
        return (time_t)(GetNowTime() - CreationTime);
    }

    NotifyClass::NotifyClass(eType Type, time_t ExpireTime) {
        this->Type = Type;
        this->ExpireTime = ExpireTime;
        this->CreationTime = GetNowTime();
    }

    void DeleteNotify(int Index) {
        if (Index >= 0 && Index < NotifyList.size()) {
            NotifyList.erase(NotifyList.begin() + Index);
        }
    }

    void Render() {
        const auto DrawList = ImGui::GetForegroundDrawList();
        const auto WindowSize = Overlay::GetTargetWindowSize();
        float NextHeight = 0.f;

        for (auto i = 0; i < NotifyList.size(); i++) {
            auto& Notify = NotifyList.at(i);
            const auto Id = Notify.GetDescription() + std::to_string(Notify.GetCreationTime());

            NotifyAnim_t* NotifyAnim = nullptr;
            for (auto& anim : AnimList) {
                if (anim.Id == Id) {
                    NotifyAnim = &anim;
                    break;
                }
            }

            if (!NotifyAnim) {
                AnimList.push_back({ Id, 0.f, 0.f, 0.f, 0.f });
                NotifyAnim = &AnimList.back();
            }

            if (Notify.GetCurrentState() == eState::Expired) {
                Notify.SetState(eState::In);
                DeleteNotify(i);

                for (auto it = AnimList.begin(); it != AnimList.end(); ++it) {
                    if (it->Id == Id) {
                        AnimList.erase(it);
                        break;
                    }
                }

                i--;
                continue;
            }

            // Use InterMedium font for modern look
            ImGui::PushFont(Fonts::InterMedium);
            auto DescTxtSize = ImGui::CalcTextSize(Notify.GetDescription().c_str());
            ImGui::PopFont();

            const float Padding = 14;
            const float IconSize = 20;
            const float NotifyHeight = 48;
            float NotifyWidth = DescTxtSize.x + Padding * 3 + IconSize + 10;
            NotifyWidth = ImMax(NotifyWidth, 180.0f);
            NotifyWidth = ImMin(NotifyWidth, 550.0f); // Aumentado para textos grandes
            
            ImVec2 NotifySize = ImVec2(NotifyWidth, NotifyHeight);

            NotifyAnim->TargetY = NextHeight;

            float dt = ImGui::GetIO().DeltaTime;

            if (Notify.GetCurrentState() == eState::In || Notify.GetCurrentState() == eState::Current) {
                NotifyAnim->SlideX = NotifyAnim->SlideX + ((NotifySize.x + 20) - NotifyAnim->SlideX) * dt * 10.0f;
                NotifyAnim->Alpha = NotifyAnim->Alpha + (1.f - NotifyAnim->Alpha) * dt * 12.0f;
            }

            if (NotifyAnim->SlideX >= NotifySize.x + 18.f) {
                Notify.SetState(eState::Current);
            }

            if (Notify.GetCurrentState() == eState::Current && Notify.GetTimeDiff() > Notify.GetExpireTime()) {
                Notify.SetState(eState::Out);
            }

            if (Notify.GetCurrentState() == eState::Out) {
                NotifyAnim->SlideX = NotifyAnim->SlideX + (0.f - NotifyAnim->SlideX) * dt * 10.0f;
                NotifyAnim->Alpha = NotifyAnim->Alpha + (0.f - NotifyAnim->Alpha) * dt * 8.0f;
                if (NotifyAnim->SlideX <= 2.f) {
                    Notify.SetState(eState::Expired);
                }
            }

            if (Notify.GetCurrentState() != eState::In) {
                NotifyAnim->YPos = NotifyAnim->YPos + (NotifyAnim->TargetY - NotifyAnim->YPos) * dt * 10.0f;
            }
            else {
                NotifyAnim->YPos = NotifyAnim->TargetY;
            }

            float NotifyStartX = WindowSize.x - NotifySize.x - 20;
            float NotifyStartY = WindowSize.y - NotifySize.y - 20 - NotifyAnim->YPos;

            ImVec2 NotifyMin = ImVec2(NotifyStartX + NotifySize.x + 20 - NotifyAnim->SlideX, NotifyStartY);
            ImVec2 NotifyMax = ImVec2(NotifyMin.x + NotifySize.x, NotifyMin.y + NotifySize.y);

            // Shadow
            DrawList->AddRectFilled(
                NotifyMin + ImVec2(0, 4),
                NotifyMax + ImVec2(0, 4),
                IM_COL32(0, 0, 0, (int)(40 * NotifyAnim->Alpha)),
                12.0f
            );

            // Background
            DrawList->AddRectFilled(
                NotifyMin,
                NotifyMax,
                IM_COL32(22, 22, 22, (int)(250 * NotifyAnim->Alpha)),
                12.0f
            );

            // Border
            DrawList->AddRect(
                NotifyMin,
                NotifyMax,
                IM_COL32(50, 50, 50, (int)(150 * NotifyAnim->Alpha)),
                12.0f,
                0,
                1.0f
            );

            // Pink accent on left - dentro do background arredondado
            // ComeÃ§a apÃ³s o arredondamento (12px) para nÃ£o vazar
            DrawList->AddRectFilled(
                ImVec2(NotifyMin.x + 1, NotifyMin.y + 12),
                ImVec2(NotifyMin.x + 4, NotifyMax.y - 12),
                IM_COL32(200, 0, 0, (int)(255 * NotifyAnim->Alpha)),
                0.0f
            );

            // Icon
            ImGui::PushFont(Fonts::FontAwesomeSolid);
            const char* icon = ICON_FA_CIRCLE_INFO;
            ImVec2 iconSize = ImGui::CalcTextSize(icon);
            ImVec2 iconPos = ImVec2(
                NotifyMin.x + Padding + 4,
                NotifyMin.y + (NotifySize.y - iconSize.y) * 0.5f
            );
            DrawList->AddText(iconPos, IM_COL32(200, 0, 0, (int)(255 * NotifyAnim->Alpha)), icon);
            ImGui::PopFont();

            // Text
            ImGui::PushFont(Fonts::InterMedium);
            ImVec2 textPos = ImVec2(
                iconPos.x + iconSize.x + 10,
                NotifyMin.y + (NotifySize.y - DescTxtSize.y) * 0.5f
            );
            DrawList->AddText(textPos, IM_COL32(220, 220, 225, (int)(255 * NotifyAnim->Alpha)), Notify.GetDescription().c_str());
            ImGui::PopFont();

            // Progress bar at bottom
            float Progress = 1.0f - ((float)Notify.GetTimeDiff() / (float)Notify.GetExpireTime());
            Progress = ImClamp(Progress, 0.0f, 1.0f);
            
            ImVec2 ProgressMin = ImVec2(NotifyMin.x + 4, NotifyMax.y - 3);
            ImVec2 ProgressMax = ImVec2(NotifyMin.x + 4 + (NotifySize.x - 8) * Progress, NotifyMax.y);
            
            DrawList->AddRectFilled(
                ProgressMin,
                ProgressMax,
                IM_COL32(200, 0, 0, (int)(180 * NotifyAnim->Alpha)),
                2.0f
            );

            NextHeight += NotifySize.y + 12.f;
        }
    }

    static std::wstring Utf8ToUtf16(const std::string& s) {
        if (s.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), len);
        return w;
    }

    static std::string Utf16ToUtf8(const std::wstring& w) {
        if (w.empty()) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string s(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), len, nullptr, nullptr);
        return s;
    }

    static std::string NormalizeUtf8ToNFC(const std::string& utf8) {
        std::wstring w = Utf8ToUtf16(utf8);
        int needed = NormalizeString(NormalizationC, w.c_str(), (int)w.size(), nullptr, 0);
        if (needed <= 0) return utf8;
        std::wstring out(needed, L'\0');
        NormalizeString(NormalizationC, w.c_str(), (int)w.size(), out.data(), needed);
        return Utf16ToUtf8(out);
    }

    void Send(std::string Description, time_t ExpireTime) {
        Description = NormalizeUtf8ToNFC(Description);
        NotifyManager::NotifyClass Notify(NotifyManager::eType::Info, ExpireTime);
        Notify.SetTitle("Storm Cheats");  // Literal - fica na seção .rdata, não no heap!
        Notify.SetDescription(Description);
        NotifyList.push_back(Notify);
    }

    // Função para zerar string na memória antes de liberar
    static void SecureZeroString(std::string& str) {
        if (!str.empty()) {
            volatile char* p = const_cast<volatile char*>(str.data());
            for (size_t i = 0; i < str.size(); i++) {
                p[i] = 0;
            }
            str.clear();
            str.shrink_to_fit();
        }
    }

    void Cleanup() {
        // Limpar descrições das notificações (Title é const char* - não aloca no heap)
        for (auto& notify : NotifyList) {
            std::string desc = notify.GetDescription();
            SecureZeroString(desc);
            notify.SetDescription("");
        }
        NotifyList.clear();
        NotifyList.shrink_to_fit();

        // Limpar animações
        for (auto& anim : AnimList) {
            SecureZeroString(anim.Id);
        }
        AnimList.clear();
        AnimList.shrink_to_fit();
    }

}