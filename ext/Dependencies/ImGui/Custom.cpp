#include "Custom.hpp"
#include <imgui_internal.h>
#include <Render/Fonts/Fonts.hpp>
#include <Utils/Utils.hpp>
#include <Render/Fonts/Bytes/IconsFontAwesome6.h>
#include <map>
#include <Render/Overlay/Overlay.hpp>
using namespace ImGui;

// ═══════════════════════════════════════════════════════════════════════════════
// Lux - Modern Minimal UI Implementation
// Dark Theme with Neutral Accent -- #3D3D3D
// ═══════════════════════════════════════════════════════════════════════════════

// Variável global para controle de slider ativo (acessível via extern)
ImGuiID activeSlider = 0;

// Variável global para controle de color picker ativo (acessível via extern)
ImGuiID activeColorPicker = 0;

// Variável global para controle de combo aberto (acessível via extern)
ImGuiID activeCombo = 0;

// Variável global para área do popup do combo (acessível via extern)
ImRect activeComboPopupRect = ImRect(0, 0, 0, 0);

namespace Custom {

    // Variável global para bloquear cliques após seleção de combo
    static int g_ComboBlockClickFrames = 0;
    static int g_LastBlockUpdateFrame = -1;
    
    // Função para verificar se cliques estão bloqueados (acessível externamente)
    bool IsClickBlocked() {
        return g_ComboBlockClickFrames > 0;
    }
    
    // Função para atualizar bloqueio (chamar uma vez por frame)
    static void UpdateClickBlock() {
        int currentFrame = ImGui::GetFrameCount();
        if (currentFrame != g_LastBlockUpdateFrame) {
            g_LastBlockUpdateFrame = currentFrame;
            if (g_ComboBlockClickFrames > 0) {
                g_ComboBlockClickFrames--;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // UTILITY FUNCTIONS
    // ═══════════════════════════════════════════════════════════════════════════

    void DrawGlow(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, float radius, float rounding) {
        ImVec4 col = ImGui::ColorConvertU32ToFloat4(color);
        
        for (int i = 0; i < 3; i++) {
            float expand = radius * (i + 1) / 3.0f;
            float alpha = col.w * (1.0f - (float)i / 3.0f) * 0.5f;
            ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, alpha));
            
            dl->AddRectFilled(
                ImVec2(min.x - expand, min.y - expand),
                ImVec2(max.x + expand, max.y + expand),
                glowColor,
                rounding + expand
            );
        }
    }

    void DrawShadow(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, float offset) {
        for (int i = 0; i < 4; i++) {
            float expand = offset * (i + 1) / 4.0f;
            float alpha = 0.15f * (1.0f - (float)i / 4.0f);
            
            dl->AddRectFilled(
                ImVec2(min.x + expand, min.y + expand),
                ImVec2(max.x + expand * 2, max.y + expand * 2),
                IM_COL32(0, 0, 0, (int)(alpha * 255)),
                rounding
            );
        }
    }

    void DrawGradientRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 col1, ImU32 col2, bool horizontal, float rounding) {
        if (horizontal) {
            dl->AddRectFilledMultiColor(min, max, col1, col2, col2, col1);
        } else {
            dl->AddRectFilledMultiColor(min, max, col1, col1, col2, col2);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // PROFILE BAR
    // ═══════════════════════════════════════════════════════════════════════════

    void ProfileBar(const char* Username, const char* Role, const char* ExpiryStr) {
        ImVec2 BasePos = ImGui::GetWindowPos() + ImVec2(24, ImGui::GetWindowSize().y - 24);

        static GLuint s_AvatarTex = 0;

        std::string UsernameString = Username;
        std::string DisplayText = ExpiryStr;

        if (UsernameString.length() > 12) {
            UsernameString = UsernameString.substr(0, 9) + "...";
        }

        auto* dl = ImGui::GetWindowDrawList();
        const float radius = 16.0f;
        ImVec2 imgMin = BasePos - ImVec2(radius, radius);
        ImVec2 imgMax = BasePos + ImVec2(radius, radius);

        // Glow behind avatar
        DrawGlow(dl, imgMin, imgMax, Theme::Colors::AccentGlow, 8.0f, radius);

        if (s_AvatarTex != 0) {
            dl->AddImageRounded(
                (void*)(intptr_t)s_AvatarTex,
                imgMin, imgMax,
                ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32_WHITE,
                radius,
                ImDrawFlags_RoundCornersAll
            );
        } else {
            // Avatar placeholder with gradient
            dl->AddCircleFilled(BasePos, radius, Theme::Colors::BackgroundTertiary, 32);
            
            std::string PrimeiraLetra = UsernameString.substr(0, 1);
            std::transform(PrimeiraLetra.begin(), PrimeiraLetra.end(), PrimeiraLetra.begin(),
                [](unsigned char c) { return std::toupper(c); });

            ImVec2 PrimeiraLetraSize = Utils::CalcTextSize(Fonts::InterBold, 14.f, PrimeiraLetra.c_str());
            ImVec2 ProfileLetra = {
                BasePos.x - PrimeiraLetraSize.x * 0.5f,
                BasePos.y - PrimeiraLetraSize.y * 0.5f
            };

            dl->AddText(Fonts::InterBold, 14.f, ProfileLetra, Theme::Colors::TextPrimary, PrimeiraLetra.c_str());
        }

        // Ring around avatar
        dl->AddCircle(BasePos, radius + 2, Theme::Colors::Accent, 32, 2.0f);

        // Username
        ImVec2 UsernamePos = BasePos + ImVec2(24, -8);
        dl->AddText(Fonts::InterBold, 13.f, UsernamePos, Theme::Colors::TextPrimary, UsernameString.c_str());

        // Expiry text
        ImVec2 DisplayPos = UsernamePos + ImVec2(0, 14);
        dl->AddText(Fonts::InterMedium, 11.f, DisplayPos, Theme::Colors::TextMuted, DisplayText.c_str());
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // BUTTONS - Smooth, rounded, with fluid animations
    // ═══════════════════════════════════════════════════════════════════════════

    bool Button(const char* label, const ImVec2& size_arg, ImGuiButtonFlags flags) {
        struct ButtonAnim {
            ImGuiID id;
            float hover;
            float press;
            float glow;
            float scale;
            bool used;
        };

        static ButtonAnim anims[128];

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        ImGuiIO& io = g.IO;
        const ImGuiID id = window->GetID(label);
        
        ImGui::PushFont(Fonts::InterBold);
        const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
        ImGui::PopFont();
        
        ImDrawList* draw = ImGui::GetWindowDrawList();

        // Find or create animation state
        ButtonAnim* anim = nullptr;
        for (int i = 0; i < IM_ARRAYSIZE(anims); ++i) {
            if (anims[i].used && anims[i].id == id) {
                anim = &anims[i];
                break;
            }
        }
        if (!anim) {
            for (int i = 0; i < IM_ARRAYSIZE(anims); ++i) {
                if (!anims[i].used) {
                    anim = &anims[i];
                    anim->id = id;
                    anim->hover = 0.0f;
                    anim->press = 0.0f;
                    anim->glow = 0.0f;
                    anim->scale = 1.0f;
                    anim->used = true;
                    break;
                }
            }
        }
        if (!anim) return false;

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + 40.0f, 42.0f);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        
        ImGui::ItemSize(size, 0);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered = false, held = false;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
        
        // Ignorar clique se bloqueado por combo
        if (IsClickBlocked()) {
            pressed = false;
        }

        // Smooth animations with easing
        float dt = io.DeltaTime;
        float hoverSpeed = 12.0f;
        float pressSpeed = 20.0f;
        
        anim->hover = anim->hover + (((hovered ? 1.0f : 0.0f) - anim->hover) * dt * hoverSpeed);
        anim->press = anim->press + (((held ? 1.0f : 0.0f) - anim->press) * dt * pressSpeed);
        anim->glow = anim->glow + (((hovered ? 1.0f : 0.0f) - anim->glow) * dt * 8.0f);
        anim->scale = anim->scale + (((held ? 0.97f : 1.0f) - anim->scale) * dt * pressSpeed);

        const float rounding = 12.0f; // More rounded
        float alpha = GetStyle().Alpha;

        // Calculate scaled bounds
        ImVec2 center = bb.GetCenter();
        ImVec2 halfSize = ImVec2(bb.GetWidth() * 0.5f * anim->scale, bb.GetHeight() * 0.5f * anim->scale);
        ImVec2 scaledMin = center - halfSize;
        ImVec2 scaledMax = center + halfSize;

        // Subtle outer glow on hover - usar o MESMO rounding do botão
        if (anim->glow > 0.01f) {
            for (int i = 2; i >= 0; i--) {
                float expand = (2.0f + i * 3.0f) * anim->glow;
                float glowAlpha = 0.06f * (3 - i) * anim->glow * alpha;
                // Usar rounding proporcional para manter forma arredondada
                draw->AddRectFilled(
                    scaledMin - ImVec2(expand, expand),
                    scaledMax + ImVec2(expand, expand),
                    IM_COL32(200, 0, 0, (int)(glowAlpha * 255)),
                    rounding // Mesmo rounding do botão
                );
            }
        }

        // Background - smooth gradient transition
        float t = anim->hover;
        ImU32 bgColor = IM_COL32(
            (int)(28 + 33 * t),
            (int)(28 + 33 * t),
            (int)(28 + 33 * t),
            (int)(200 * alpha)
        );
        
        draw->AddRectFilled(scaledMin, scaledMax, bgColor, rounding);
        
        // Inner highlight at top (subtle)
        if (anim->hover > 0.01f) {
            draw->AddRectFilledMultiColor(
                scaledMin,
                ImVec2(scaledMax.x, scaledMin.y + 20),
                IM_COL32(255, 255, 255, (int)(15 * anim->hover * alpha)),
                IM_COL32(255, 255, 255, (int)(15 * anim->hover * alpha)),
                IM_COL32(255, 255, 255, 0),
                IM_COL32(255, 255, 255, 0)
            );
        }

        // Border
        ImU32 borderCol = IM_COL32(
            (int)(50 + 10 * t),
            (int)(50 + 10 * t),
            (int)(50 + 10 * t),
            (int)((100 + 80 * t) * alpha)
        );
        draw->AddRect(scaledMin, scaledMax, borderCol, rounding, 0, 1.0f);

        // Text with smooth color transition
        ImU32 textCol = IM_COL32(
            (int)(180 + 75 * t),
            (int)(180 + 75 * t),
            (int)(180 + 75 * t),
            (int)(255 * alpha)
        );

        ImGui::PushFont(Fonts::InterBold);
        ImVec2 textPos = ImVec2(
            center.x - label_size.x * 0.5f,
            center.y - label_size.y * 0.5f
        );
        draw->AddText(textPos, textCol, label);
        ImGui::PopFont();

        return pressed;
    }

    bool GlowButton(const char* label, const ImVec2& size_arg) {
        return Button(label, size_arg, 0);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // DOCK SYSTEM (macOS style)
    // ═══════════════════════════════════════════════════════════════════════════

    static int g_DockItemCount = 0;
    static float g_DockStartX = 0;

    void BeginDock() {
        g_DockItemCount = 0;
    }

    bool DockItem(const char* icon, const char* label, bool active, int index) {
        struct DockAnim {
            float hover;
            float bounce;
            float glow;
        };
        
        static std::map<int, DockAnim> anims;
        
        if (anims.find(index) == anims.end()) {
            anims[index] = { 0.0f, 0.0f, 0.0f };
        }
        
        DockAnim& anim = anims[index];
        
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGuiContext& g = *GImGui;
        ImGuiIO& io = g.IO;
        
        const float itemSize = Theme::Size::DockItemSize;
        const float spacing = Theme::Size::DockItemSpacing;
        
        ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + itemSize, pos.y + itemSize));
        const ImGuiID id = window->GetID(label);
        
        ImGui::ItemSize(ImVec2(itemSize + spacing, itemSize));
        if (!ImGui::ItemAdd(bb, id)) return false;
        
        bool hovered = false, held = false;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        
        float dt = io.DeltaTime;
        anim.hover = ImLerp(anim.hover, hovered ? 1.0f : 0.0f, dt * Theme::Animation::Fast);
        anim.glow = ImLerp(anim.glow, active ? 1.0f : 0.0f, dt * Theme::Animation::Normal);
        
        // Bounce animation
        if (pressed) anim.bounce = 1.0f;
        anim.bounce = ImLerp(anim.bounce, 0.0f, dt * Theme::Animation::Fast);
        
        ImDrawList* draw = window->DrawList;
        
        // Calculate bounce offset
        float bounceOffset = sinf(anim.bounce * IM_PI) * 8.0f;
        float hoverOffset = anim.hover * 6.0f;
        float totalOffset = bounceOffset + hoverOffset;
        
        ImVec2 itemMin = ImVec2(bb.Min.x, bb.Min.y - totalOffset);
        ImVec2 itemMax = ImVec2(bb.Max.x, bb.Max.y - totalOffset);
        ImVec2 center = ImVec2((itemMin.x + itemMax.x) * 0.5f, (itemMin.y + itemMax.y) * 0.5f);
        
        // Glow effect
        if (anim.glow > 0.01f || anim.hover > 0.01f) {
            float glowIntensity = ImMax(anim.glow, anim.hover * 0.5f);
            DrawGlow(draw, itemMin, itemMax, IM_COL32(200, 0, 0, (int)(60 * glowIntensity)), 12.0f * glowIntensity, Theme::Size::CardRounding);
        }
        
        // Background
        ImU32 bgColor = active ? Theme::Colors::Accent : IM_COL32(40 + (int)(20 * anim.hover), 40 + (int)(20 * anim.hover), 40 + (int)(20 * anim.hover), 255);
        draw->AddRectFilled(itemMin, itemMax, bgColor, Theme::Size::CardRounding);
        
        // Border
        ImU32 borderCol = active ? Theme::Colors::BorderAccent : Theme::Colors::Border;
        draw->AddRect(itemMin, itemMax, borderCol, Theme::Size::CardRounding, 0, 1.5f);
        
        // Icon
        ImGui::PushFont(Fonts::FontAwesomeSolid);
        ImVec2 iconSize = ImGui::CalcTextSize(icon);
        ImVec2 iconPos = ImVec2(center.x - iconSize.x * 0.5f, center.y - iconSize.y * 0.5f);
        draw->AddText(iconPos, active ? IM_COL32(255, 255, 255, 255) : Theme::Colors::TextSecondary, icon);
        ImGui::PopFont();
        
        // Active indicator dot
        if (active) {
            ImVec2 dotPos = ImVec2(center.x, itemMax.y + 6);
            draw->AddCircleFilled(dotPos, 3.0f, Theme::Colors::Accent, 12);
        }
        
        // Tooltip on hover
        if (hovered && label) {
            ImGui::BeginTooltip();
            ImGui::PushFont(Fonts::InterMedium);
            ImGui::Text("%s", label);
            ImGui::PopFont();
            ImGui::EndTooltip();
        }
        
        g_DockItemCount++;
        return pressed;
    }

    void EndDock() {
        // Nothing needed for now
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // CUSTOM CHILD / CARDS
    // ═══════════════════════════════════════════════════════════════════════════

    bool CustomChild(const char* str_id, const ImVec2& size) {
        ImDrawList* dl = GetWindowDrawList();
        ImVec2 pos = GetWindowPos() + GetCursorPos();
        float alpha = GetStyle().Alpha; // Usar alpha do estilo atual
        
        // Garantir altura mínima para não sobrepor header
        ImVec2 actualSize = size;
        if (actualSize.y < 80) actualSize.y = 80;
        
        // Área do header para drag
        ImRect headerRect(pos, pos + ImVec2(actualSize.x, 36));
        bool headerHovered = ImGui::IsMouseHoveringRect(headerRect.Min, headerRect.Max);
        
        // Se clicar no header, sinalizar que quer drag
        if (headerHovered && ImGui::IsMouseClicked(0)) {
            CustomDrag::WantDrag = true;
            CustomDrag::DragClickPos = ImGui::GetIO().MousePos;
        }
        
        // Draw shadow (só se alpha > 0)
        if (alpha > 0.01f) {
            DrawShadow(dl, pos, pos + actualSize, Theme::Size::CardRounding, 4.0f * alpha);
        }
        
        // PRIMEIRO: Desenhar o fundo completo do child - tom escuro com sutil toque roxo
        dl->AddRectFilled(
            pos,
            pos + actualSize,
            IM_COL32(16, 16, 16, (int)(255 * alpha)),
            Theme::Size::CardRounding
        );
        
        // Header background - levemente mais claro com toque roxo
        dl->AddRectFilled(
            pos,
            pos + ImVec2(actualSize.x, 36),
            IM_COL32(24, 24, 24, (int)(255 * alpha)),
            Theme::Size::CardRounding,
            ImDrawFlags_RoundCornersTop
        );
        
        // Glow muito sutil - linear e bem distribuído
        const int glowLayers = 24;
        const float maxGlowHeight = 28.0f;
        for (int i = 0; i < glowLayers; i++) {
            float t = (float)i / (float)(glowLayers - 1);
            // Curva linear - fade uniforme do topo até embaixo
            float layerAlpha = 1.0f - t;
            float layerHeight = maxGlowHeight * (1.0f - t);
            
            dl->AddRectFilled(
                pos,
                pos + ImVec2(actualSize.x, layerHeight),
                IM_COL32(60, 60, 60, (int)(2 * layerAlpha * alpha)),
                Theme::Size::CardRounding,
                ImDrawFlags_RoundCornersTop
            );
        }
        
        // Header text with glow
        ImGui::PushFont(Fonts::InterBold);
        ImVec2 textSize = ImGui::CalcTextSize(str_id);
        ImVec2 textPos = pos + ImVec2(14, 18 - textSize.y * 0.5f);
        
        // Text glow
        dl->AddText(textPos + ImVec2(0, 1), IM_COL32(200, 0, 0, (int)(30 * alpha)), str_id);
        dl->AddText(textPos, IM_COL32(220, 220, 220, (int)(255 * alpha)), str_id);
        ImGui::PopFont();
        
        // Pink accent line under header
        dl->AddRectFilled(
            pos + ImVec2(0, 35),
            pos + ImVec2(actualSize.x, 37),
            IM_COL32(200, 0, 0, (int)(60 * alpha))
        );
        
        // Border completa - com toque roxo
        dl->AddRect(pos, pos + actualSize, IM_COL32(50, 50, 50, (int)(200 * alpha)), Theme::Size::CardRounding, 0, 1.0f);
        
        // Estilo do child - scrollbar branca minimalista
        PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

        PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
        PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 2.0f);
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        
        SetCursorPosY(GetCursorPosY() + 36);
        
        // Criar ID único para esta child
        char childId[128];
        snprintf(childId, sizeof(childId), "##CustomChild_%s", str_id);
        
        // Permitir scroll vertical - usar AlwaysVerticalScrollbar para garantir área de scroll
        bool ret = BeginChild(childId, actualSize - ImVec2(0, 36), ImGuiChildFlags_None, ImGuiWindowFlags_None);
        SetCursorPos(ImVec2(14, 12));
        BeginGroup();
        
        return ret;
    }

    void EndCustomChild() {
        // Adicionar padding no final se necessário
        SetCursorPosY(GetCursorPosY() + 12);

        EndGroup();
        
        EndChild();
        PopStyleVar(4); // ChildRounding, ScrollbarSize, ScrollbarRounding, WindowPadding
        PopStyleColor(5); // ChildBg, ScrollbarBg, ScrollbarGrab, ScrollbarGrabHovered, ScrollbarGrabActive
        SetCursorPosY(GetCursorPosY() + 14);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // TABS
    // ═══════════════════════════════════════════════════════════════════════════

    bool Tab(const char* Label, const char* Icon, bool Enabled) {
        struct TabAnim {
            ImGuiID id;
            float hover;
            float active;
            float glow;
        };
        
        static std::vector<TabAnim> anims;
        
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(Label);
        
        ImGui::PushFont(Fonts::InterBold);
        const ImVec2 label_size = CalcTextSize(Label, NULL, true);
        ImGui::PopFont();

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb(pos, pos + ImVec2(window->Size.x - 40, 42));
        
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id)) return false;

        // Find or create animation
        auto it = std::find_if(anims.begin(), anims.end(), [id](const TabAnim& a) { return a.id == id; });
        if (it == anims.end()) {
            anims.push_back({ id, 0.0f, Enabled ? 1.0f : 0.0f, 0.0f });
            it = anims.end() - 1;
        }
        
        bool hovered, held;
        bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
        
        // Ignorar clique se bloqueado por combo
        if (IsClickBlocked()) {
            pressed = false;
        }
        
        float dt = g.IO.DeltaTime;
        // Limitar dt para evitar teleporte em frames lentos
        float smoothDt = ImMin(dt, 0.033f); // Max 30fps para animação
        it->hover = ImLerp(it->hover, hovered ? 1.0f : 0.0f, smoothDt * Theme::Animation::Fast);
        it->active = ImLerp(it->active, Enabled ? 1.0f : 0.0f, smoothDt * Theme::Animation::Normal);
        it->glow = ImLerp(it->glow, Enabled ? 1.0f : 0.0f, smoothDt * Theme::Animation::Slow);
        
        ImDrawList* dl = window->DrawList;
        
        // Usar clipping para não vazar fora da área
        dl->PushClipRect(total_bb.Min, total_bb.Max, true);
        
        // Glow effect when active (agora clipped)
        if (it->glow > 0.01f) {
            DrawGlow(dl, total_bb.Min, total_bb.Max, IM_COL32(200, 0, 0, (int)(40 * it->glow)), 8.0f * it->glow, Theme::Size::SmallRounding);
        }
        
        dl->PopClipRect();
        
        // Background
        if (it->active > 0.01f || it->hover > 0.01f) {
            ImU32 bgColor = IM_COL32(
                (int)(61 * it->active + 50 * it->hover * (1 - it->active)),
                (int)(61 * it->active + 50 * it->hover * (1 - it->active)),
                (int)(61 * it->active + 50 * it->hover * (1 - it->active)),
                (int)(255 * it->active + 200 * it->hover * (1 - it->active))
            );
            dl->AddRectFilled(total_bb.Min, total_bb.Max, bgColor, Theme::Size::SmallRounding);
        }
        
        // Icon
        ImGui::PushFont(Fonts::FontAwesomeSolid);
        ImVec2 iconSize = CalcTextSize(Icon);
        float iconAlpha = Enabled ? 1.0f : (0.4f + 0.3f * it->hover);
        dl->AddText(
            total_bb.Min + ImVec2(14, total_bb.GetHeight() * 0.5f - iconSize.y * 0.5f),
            IM_COL32(255, 255, 255, (int)(255 * iconAlpha * GetStyle().Alpha)),
            Icon
        );
        ImGui::PopFont();

        // Label
        ImGui::PushFont(Fonts::InterBold);
        float textAlpha = Enabled ? 1.0f : (0.4f + 0.3f * it->hover);
        dl->AddText(
            total_bb.Min + ImVec2(36, total_bb.GetHeight() * 0.5f - label_size.y * 0.5f),
            IM_COL32(255, 255, 255, (int)(255 * textAlpha * GetStyle().Alpha)),
            Label
        );
        ImGui::PopFont();
        
        // Active indicator
        if (it->active > 0.01f) {
            float lineWidth = 3.0f * it->active;
            dl->AddRectFilled(
                ImVec2(total_bb.Max.x - lineWidth - 2, total_bb.Min.y + 8),
                ImVec2(total_bb.Max.x - 2, total_bb.Max.y - 8),
                IM_COL32(255, 255, 255, (int)(200 * it->active)),
                2.0f
            );
        }

        return pressed;
    }

    bool TabHeader(const char* tabname, int* currentTab, std::vector<const char*> Tabs, int TabIndex) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& Style = g.Style;
        const ImGuiID Id = window->GetID(tabname);
        float alpha = Style.Alpha;

        SetCursorPos(ImVec2(20, 10));

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = ImVec2(window->Size.x - 40, 50);
        const ImRect bb(pos, pos + size);

        ItemSize(size, Style.FramePadding.y);
        if (!ItemAdd(bb, Id)) return false;

        int WidhtMax = 0;
        ImGui::PushFont(Fonts::InterMedium);
        for (size_t i = 0; i < Tabs.size(); i++) {
            WidhtMax = WidhtMax + 24 + CalcTextSize(Tabs.at(i)).x;
        }
        ImGui::PopFont();

        SetCursorPos(ImVec2(20, 10) + ImVec2(bb.GetWidth() / 2 - WidhtMax / 2, 10));

        ImDrawList* dl = GetWindowDrawList();
        ImVec2 tabBgPos = GetWindowPos() + GetCursorPos();
        
        // Tab container background - usar alpha
        dl->AddRectFilled(
            tabBgPos,
            tabBgPos + ImVec2(WidhtMax, 36),
            IM_COL32(35, 35, 35, (int)(255 * alpha)),
            Theme::Size::SmallRounding
        );
        dl->AddRect(
            tabBgPos,
            tabBgPos + ImVec2(WidhtMax, 36),
            IM_COL32(50, 50, 50, (int)(200 * alpha)),
            Theme::Size::SmallRounding
        );

        for (int i = 0; i < Tabs.size(); i++) {
            if (Tabs.size() != 1) {
                if (i == 0) {
                    if (TabHeaderButton(Tabs.at(i), *currentTab == i, 1, TabIndex))
                        *currentTab = i;
                }
                else if (i == Tabs.size() - 1) {
                    ImGui::SameLine(0, 0);
                    if (TabHeaderButton(Tabs.at(i), *currentTab == i, 3, TabIndex))
                        *currentTab = i;
                }
                else {
                    ImGui::SameLine(0, 0);
                    if (TabHeaderButton(Tabs.at(i), *currentTab == i, 2, TabIndex))
                        *currentTab = i;
                }
            }
            else {
                if (TabHeaderButton(Tabs.at(i), *currentTab == i, 0, TabIndex))
                    *currentTab = i;
            }
        }

        return false;
    }

    bool TabHeaderButton(const char* ButtonName, bool enabled, int direction, int TabIndex) {
        struct TabHeaderAnim {
            int tab_index;
            float posx;
            float selectedpox;
            float hover;
        };

        static std::vector<TabHeaderAnim> Values;

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGui::PushFont(Fonts::InterMedium);
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(ButtonName);
        const ImVec2 label_size = CalcTextSize(ButtonName, NULL, true);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = ImVec2(label_size.x + 24, 36);
        ImGui::PopFont();

        const ImRect bb(pos, pos + size);
        ItemSize(size, 0);
        if (!ItemAdd(bb, id)) return false;

        auto Value = std::find_if(Values.begin(), Values.end(),
            [TabIndex](const TabHeaderAnim& anim) { return anim.tab_index == TabIndex; });

        if (Value == Values.end()) {
            TabHeaderAnim newAnim;
            newAnim.tab_index = TabIndex;
            newAnim.posx = pos.x;
            newAnim.selectedpox = enabled ? pos.x : 0;
            newAnim.hover = 0.0f;
            Values.push_back(newAnim);
            Value = Values.end() - 1;
        }

        if (enabled) {
            Value->selectedpox = pos.x;
        }

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);
        
        float dt = g.IO.DeltaTime;
        Value->hover = ImLerp(Value->hover, hovered ? 1.0f : 0.0f, dt * Theme::Animation::Fast);
        Value->posx = ImLerp(Value->posx, Value->selectedpox - window->Pos.x, dt * Theme::Animation::Normal);

        ImDrawFlags Corner;
        if (direction == 0) Corner = ImDrawFlags_RoundCornersAll;
        else if (direction == 1) Corner = ImDrawFlags_RoundCornersLeft;
        else if (direction == 2) Corner = ImDrawFlags_RoundCornersNone;
        else Corner = ImDrawFlags_RoundCornersRight;

        ImDrawList* dl = window->DrawList;
        float alpha = style.Alpha;

        // Active indicator with glow
        if (enabled) {
            ImVec2 activeMin = ImVec2(window->Pos.x + Value->posx, bb.Min.y);
            ImVec2 activeMax = ImVec2(window->Pos.x + Value->posx + size.x, bb.Max.y);
            
            // Glow
            DrawGlow(dl, activeMin, activeMax, IM_COL32(200, 0, 0, (int)(60 * alpha)), 6.0f * alpha, Theme::Size::TinyRounding);
            
            // Background
            dl->AddRectFilled(activeMin, activeMax, IM_COL32(200, 0, 0, (int)(255 * alpha)), Theme::Size::TinyRounding, Corner);
        }

        // Text
        ImGui::PushFont(Fonts::InterMedium);
        ImU32 textColor = enabled ? IM_COL32(255, 255, 255, (int)(255 * alpha)) :
                          IM_COL32(180 + (int)(40 * Value->hover), 180 + (int)(40 * Value->hover), 190 + (int)(40 * Value->hover), (int)(255 * alpha));
        dl->AddText(
            ImVec2(bb.Min.x + 12, bb.GetCenter().y - label_size.y * 0.5f),
            textColor,
            ButtonName
        );
        ImGui::PopFont();

        return pressed;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // CHECKBOX
    // ═══════════════════════════════════════════════════════════════════════════

    bool Checkbox(const char* label, bool* v, float width) {
        struct CheckAnim {
            ImGuiID id;
            float check;
            float hover;
            float glow;
        };
        
        static std::vector<CheckAnim> anims;

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        
        ImGui::PushFont(Fonts::InterMedium);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        ImGui::PopFont();

        const float square_sz = 22;
        const ImVec2 pos = window->DC.CursorPos;
        float actual_width = width > 0.0f ? width : (GetWindowSize().x - 28);
        const ImRect total_bb(pos, pos + ImVec2(actual_width, 28));

        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id)) return false;

        // Find or create animation
        auto it = std::find_if(anims.begin(), anims.end(), [id](const CheckAnim& a) { return a.id == id; });
        if (it == anims.end()) {
            anims.push_back({ id, *v ? 1.0f : 0.0f, 0.0f, 0.0f });
            it = anims.end() - 1;
        }

        bool hovered, held;
        bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
        
        // Ignorar clique se bloqueado por combo
        if (pressed && !IsClickBlocked()) {
            *v = !(*v);
            MarkItemEdited(id);
        }

        float dt = g.IO.DeltaTime;
        it->check = ImLerp(it->check, *v ? 1.0f : 0.0f, dt * Theme::Animation::Fast);
        it->hover = ImLerp(it->hover, hovered ? 1.0f : 0.0f, dt * Theme::Animation::Fast);
        it->glow = ImLerp(it->glow, *v ? 1.0f : 0.0f, dt * Theme::Animation::Normal);

        const ImRect check_bb(total_bb.Max - ImVec2(square_sz, square_sz + 3), total_bb.Max - ImVec2(0, 3));
        ImDrawList* dl = window->DrawList;

        // Glow when checked (clipped para não vazar)
        if (it->glow > 0.01f) {
            dl->PushClipRect(check_bb.Min - ImVec2(2, 2), check_bb.Max + ImVec2(2, 2), true);
            DrawGlow(dl, check_bb.Min, check_bb.Max, IM_COL32(200, 0, 0, (int)(50 * it->glow)), 6.0f * it->glow, Theme::Size::TinyRounding);
            dl->PopClipRect();
        }

        // Background
        ImU32 bgColor = IM_COL32(
            (int)ImLerp(24.0f, 200.0f, it->check),
            (int)ImLerp(24.0f, 0.0f, it->check),
            (int)ImLerp(24.0f, 0.0f, it->check),
            255
        );
        dl->AddRectFilled(check_bb.Min, check_bb.Max, bgColor, Theme::Size::TinyRounding);

        // Border
        ImU32 borderCol = IM_COL32(
            (int)ImLerp(55.0f + 20.0f * it->hover, 200.0f, it->check),
            (int)ImLerp(55.0f + 20.0f * it->hover, 0.0f, it->check),
            (int)ImLerp(55.0f + 20.0f * it->hover, 0.0f, it->check),
            (int)ImLerp(180.0f, 200.0f, it->check)
        );
        dl->AddRect(check_bb.Min, check_bb.Max, borderCol, Theme::Size::TinyRounding, 0, 1.5f);

        // Checkmark
        if (it->check > 0.01f) {
            float checkSize = 10.0f * it->check;
            ImVec2 center = check_bb.GetCenter();
            
            // Animated checkmark path
            ImVec2 p1 = center + ImVec2(-4.0f, 0.0f) * it->check;
            ImVec2 p2 = center + ImVec2(-1.0f, 3.0f) * it->check;
            ImVec2 p3 = center + ImVec2(4.0f, -3.0f) * it->check;
            
            dl->AddLine(p1, p2, IM_COL32(255, 255, 255, (int)(255 * it->check)), 2.0f);
            dl->AddLine(p2, p3, IM_COL32(255, 255, 255, (int)(255 * it->check)), 2.0f);
        }

        // Label
        ImGui::PushFont(Fonts::InterMedium);
        ImU32 textCol = IM_COL32(
            (int)(200 + 55 * it->hover),
            (int)(200 + 55 * it->hover),
            (int)(200 + 55 * it->hover),
            255
        );
        dl->AddText(
            ImVec2(total_bb.Min.x, total_bb.GetCenter().y - label_size.y * 0.5f),
            textCol,
            label
        );
        ImGui::PopFont();

        return pressed;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // KEYBIND - with animated width on key change
    // ═══════════════════════════════════════════════════════════════════════════

    const char* GetKeyNameFromSystem(int vkCode) {
        switch (vkCode) {
            case 0: return "None";
            case VK_LBUTTON: return "LMB";
            case VK_RBUTTON: return "RMB";
            case VK_MBUTTON: return "MMB";
            case VK_XBUTTON1: return "X1";
            case VK_XBUTTON2: return "X2";
        }

        UINT scanCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC);
        switch (vkCode) {
            case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END:
            case VK_PRIOR: case VK_NEXT: case VK_LEFT: case VK_RIGHT:
            case VK_UP: case VK_DOWN: case VK_NUMLOCK: case VK_DIVIDE:
                scanCode |= 0x100;
                break;
        }

        static char keyName[64];
        LONG lParam = scanCode << 16;
        if (GetKeyNameTextA(lParam, keyName, sizeof(keyName)))
            return keyName;

        return "???";
    }

    bool KeyBind(const char* label, int* Key, bool IsBlockMouse) {
        struct KeyBindAnim {
            ImGuiID id;
            float hover;
            float active;
            float width;
            float targetWidth;
            bool listening;
            bool mouseWasReleased; // Flag para saber se o mouse foi solto desde que começou a escutar
        };
        
        static std::vector<KeyBindAnim> anims;
        static ImGuiID activeKeyBind = 0;

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiID id = window->GetID(label);

        ImGui::PushFont(Fonts::InterMedium);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        ImGui::PopFont();

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb(pos, pos + ImVec2(GetWindowSize().x - 28, 28));

        ItemSize(total_bb, g.Style.FramePadding.y);
        if (!ItemAdd(total_bb, id)) return false;

        // Find or create animation
        auto it = std::find_if(anims.begin(), anims.end(), [id](const KeyBindAnim& a) { return a.id == id; });
        if (it == anims.end()) {
            KeyBindAnim newAnim;
            newAnim.id = id;
            newAnim.hover = 0.0f;
            newAnim.active = 0.0f;
            newAnim.width = 50.0f;
            newAnim.targetWidth = 50.0f;
            newAnim.listening = false;
            newAnim.mouseWasReleased = false;
            anims.push_back(newAnim);
            it = anims.end() - 1;
        }

        // Get key text and calculate target width
        const char* keyText = it->listening ? "..." : GetKeyNameFromSystem(*Key);
        ImGui::PushFont(Fonts::InterMedium);
        ImVec2 keySize = CalcTextSize(keyText);
        ImGui::PopFont();
        
        it->targetWidth = ImMax(keySize.x + 20.0f, 45.0f);

        // Animate width smoothly
        float dt = g.IO.DeltaTime;
        it->width = it->width + ((it->targetWidth - it->width) * dt * 15.0f);

        float boxWidth = it->width;
        ImRect key_bb(
            ImVec2(total_bb.Max.x - boxWidth, total_bb.Min.y),
            ImVec2(total_bb.Max.x, total_bb.Max.y)
        );

        bool hovered = ImGui::IsMouseHoveringRect(key_bb.Min, key_bb.Max);
        
        // Registrar hover para toda a área (evita arrasto da janela)
        bool totalHovered = ImGui::IsMouseHoveringRect(total_bb.Min, total_bb.Max);
        if (totalHovered) {
            g.HoveredId = id;
        }
        
        bool pressed = hovered && ImGui::IsMouseClicked(0) && !IsClickBlocked();

        // Sync com estado global
        if (activeKeyBind != 0 && activeKeyBind != id) {
            it->listening = false;
        }

        // Iniciar listening quando clicado
        if (pressed && !it->listening) {
            // Desativar outros
            for (auto& a : anims) {
                a.listening = false;
            }
            it->listening = true;
            it->mouseWasReleased = false; // Reset flag
            activeKeyBind = id;
        }

        it->hover = it->hover + (((hovered ? 1.0f : 0.0f) - it->hover) * dt * 15.0f);
        it->active = it->active + (((it->listening ? 1.0f : 0.0f) - it->active) * dt * 12.0f);

        bool changed = false;

        // Key detection
        if (it->listening) {
            // Atualizar flag de mouse solto
            if (!ImGui::IsMouseDown(0)) {
                it->mouseWasReleased = true;
            }
            
            // ESC cancela
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                it->listening = false;
                activeKeyBind = 0;
            }
            else {
                // Verificar todas as teclas
                for (int vk = 1; vk < 255; vk++) {
                    // Skip ESC (já tratado)
                    if (vk == VK_ESCAPE) continue;
                    
                    // Bloquear mouse se necessário
                    if (IsBlockMouse) {
                        if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
                        if (vk == VK_XBUTTON1 || vk == VK_XBUTTON2) continue;
                    }
                    
                    // Para botões do mouse, só aceitar depois que o mouse foi solto uma vez
                    if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
                        vk == VK_XBUTTON1 || vk == VK_XBUTTON2) {
                        if (!it->mouseWasReleased) continue;
                    }

                    // Verificar se tecla está pressionada
                    if (GetAsyncKeyState(vk) & 0x8000) {
                        *Key = vk;
                        it->listening = false;
                        activeKeyBind = 0;
                        changed = true;
                        break;
                    }
                }
            }
        }

        ImDrawList* dl = window->DrawList;

        // Background with smooth transition
        ImU32 bgColor = IM_COL32(
            (int)(28 + 25 * it->hover),
            (int)(28 + 25 * it->hover),
            (int)(28 + 25 * it->hover),
            255
        );
        dl->AddRectFilled(key_bb.Min, key_bb.Max, bgColor, 8.0f);

        // Border with glow when active
        if (it->active > 0.01f) {
            // Subtle glow
            for (int i = 1; i >= 0; i--) {
                float expand = (2.0f + i * 3.0f) * it->active;
                float alpha = 0.15f * (2 - i) * it->active;
                dl->AddRect(
                    key_bb.Min - ImVec2(expand, expand),
                    key_bb.Max + ImVec2(expand, expand),
                    IM_COL32(200, 0, 0, (int)(alpha * 255)),
                    8.0f + expand * 0.5f,
                    0, 1.0f
                );
            }
        }

        ImU32 borderCol = IM_COL32(
            (int)(50 + 255 * it->active),
            (int)(50 + 255 * it->active),
            (int)(50 + 255 * it->active),
            (int)(120 + 135 * it->active)
        );
        dl->AddRect(key_bb.Min, key_bb.Max, borderCol, 8.0f, 0, 1.0f);

        // Key text centered
        ImGui::PushFont(Fonts::InterMedium);
        ImVec2 textPos = ImVec2(
            key_bb.GetCenter().x - keySize.x * 0.5f,
            key_bb.GetCenter().y - keySize.y * 0.5f
        );
        ImU32 textCol = it->listening ? Theme::Colors::Accent :  IM_COL32(160 + (int)(60 * it->hover), 160 + (int)(60 * it->hover), 160 + (int)(60 * it->hover), 255);
        dl->AddText(textPos, textCol, keyText);
        ImGui::PopFont();

        // Label
        ImGui::PushFont(Fonts::InterMedium);
        dl->AddText(
            ImVec2(total_bb.Min.x, total_bb.GetCenter().y - label_size.y * 0.5f),
            IM_COL32(180 + (int)(40 * it->hover), 180 + (int)(40 * it->hover), 180 + (int)(40 * it->hover), 255),
            label
        );
        ImGui::PopFont();

        return pressed;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // SLIDERS
    // ═══════════════════════════════════════════════════════════════════════════

    // Helper: Convert scalar value to normalized position [0,1]
    float ScalarToNormalized(ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max) {
        switch (data_type) {
            case ImGuiDataType_S32: {
                int v = *(int*)p_data;
                int v_min = *(const int*)p_min;
                int v_max = *(const int*)p_max;
                return (v_max > v_min) ? (float)(v - v_min) / (float)(v_max - v_min) : 0.0f;
            }
            case ImGuiDataType_Float: {
                float v = *(float*)p_data;
                float v_min = *(const float*)p_min;
                float v_max = *(const float*)p_max;
                return (v_max > v_min) ? (v - v_min) / (v_max - v_min) : 0.0f;
            }
            default:
                return 0.0f;
        }
    }

    // Helper: Convert normalized position [0,1] to scalar value
    void NormalizedToScalar(ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, float t) {
        t = ImClamp(t, 0.0f, 1.0f);
        switch (data_type) {
            case ImGuiDataType_S32: {
                int v_min = *(const int*)p_min;
                int v_max = *(const int*)p_max;
                *(int*)p_data = v_min + (int)(t * (float)(v_max - v_min) + 0.5f);
                break;
            }
            case ImGuiDataType_Float: {
                float v_min = *(const float*)p_min;
                float v_max = *(const float*)p_max;
                *(float*)p_data = v_min + t * (v_max - v_min);
                break;
            }
        }
    }

    bool SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
        struct SliderAnim {
            ImGuiID id;
            float hover;
            float grab;
            float glow;
            float visualT; // Posição visual animada
        };
        
        static std::vector<SliderAnim> anims;
        // activeSlider agora é global (declarado no topo do arquivo)

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        ImGui::PushFont(Fonts::InterMedium);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        ImGui::PopFont();

        const ImVec2 pos = window->DC.CursorPos;
        const float slider_height = 6.0f;
        const float total_height = 42.0f;
        const ImRect frame_bb(pos, pos + ImVec2(GetWindowSize().x - 28, total_height));

        ItemSize(frame_bb, style.FramePadding.y);
        if (!ItemAdd(frame_bb, id)) return false;

        // Find or create animation
        auto it = std::find_if(anims.begin(), anims.end(), [id](const SliderAnim& a) { return a.id == id; });
        if (it == anims.end()) {
            float initialT = ScalarToNormalized(data_type, p_data, p_min, p_max);
            anims.push_back({ id, 0.0f, 0.0f, 0.0f, initialT });
            it = anims.end() - 1;
        }

        // Slider track area (visual)
        const float track_y = frame_bb.Max.y - slider_height - 8;
        const ImRect track_bb(
            ImVec2(frame_bb.Min.x, track_y),
            ImVec2(frame_bb.Max.x, track_y + slider_height)
        );
        
        // Área de clique expandida (toda a área abaixo do label)
        const ImRect click_bb(
            ImVec2(frame_bb.Min.x, track_y - 12),
            ImVec2(frame_bb.Max.x, track_y + slider_height + 12)
        );

        bool hovered = ImGui::IsMouseHoveringRect(click_bb.Min, click_bb.Max);
        bool held = false;
        
        // Registrar hover para toda a área do slider (evita arrasto da janela)
        bool frameHovered = ImGui::IsMouseHoveringRect(frame_bb.Min, frame_bb.Max);
        if (frameHovered || hovered) {
            g.HoveredId = id;
        }
        
        // Iniciar drag apenas se nenhum outro slider está ativo e cliques não estão bloqueados
        if (hovered && ImGui::IsMouseClicked(0) && activeSlider == 0 && !IsClickBlocked()) {
            activeSlider = id;
        }
        
        // Verificar se ESTE slider está sendo arrastado
        if (activeSlider == id) {
            if (ImGui::IsMouseDown(0)) {
                held = true;
            } else {
                activeSlider = 0; // Soltar
            }
        }

        float dt = g.IO.DeltaTime;
        it->hover = ImLerp(it->hover, hovered ? 1.0f : 0.0f, dt * Theme::Animation::Fast);
        it->grab = ImLerp(it->grab, held ? 1.0f : 0.0f, dt * Theme::Animation::Fast);
        it->glow = ImLerp(it->glow, (hovered || held) ? 1.0f : 0.0f, dt * Theme::Animation::Normal);

        // Calculate grab position from current value
        float target_t = ScalarToNormalized(data_type, p_data, p_min, p_max);
        
        // Handle dragging - apenas se ESTE slider está ativo
        bool value_changed = false;
        if (held && activeSlider == id) {
            float mouse_t = ImClamp((g.IO.MousePos.x - track_bb.Min.x) / track_bb.GetWidth(), 0.0f, 1.0f);
            NormalizedToScalar(data_type, p_data, p_min, p_max, mouse_t);
            target_t = mouse_t;
            MarkItemEdited(id);
            value_changed = true;
        }
        
        // Smooth animation para posição visual
        float smoothSpeed = held ? 25.0f : 15.0f; // Mais rápido quando arrastando
        it->visualT = it->visualT + (target_t - it->visualT) * dt * smoothSpeed;
        
        // Clamp para evitar overshoot
        if (fabsf(it->visualT - target_t) < 0.001f) {
            it->visualT = target_t;
        }

        ImDrawList* dl = window->DrawList;

        // Clipar para não vazar
        dl->PushClipRect(frame_bb.Min, frame_bb.Max, true);

        // Track background
        dl->AddRectFilled(track_bb.Min, track_bb.Max, Theme::Colors::BackgroundTertiary, slider_height * 0.5f);

        // Filled track with glow (usar visualT para smooth)
        float grab_radius = 8.0f + 2.0f * it->grab;
        // Clamp grab_x para não vazar para fora (considerar raio do grab)
        float grab_x = ImLerp(track_bb.Min.x + grab_radius, track_bb.Max.x - grab_radius, it->visualT);
        ImRect filled_bb(track_bb.Min, ImVec2(grab_x, track_bb.Max.y));
        
        if (it->glow > 0.01f) {
            DrawGlow(dl, filled_bb.Min, filled_bb.Max, IM_COL32(220, 0, 0, (int)(40 * it->glow)), 4.0f, slider_height * 0.5f);
        }
        
        // Gradient fill
        dl->AddRectFilledMultiColor(
            filled_bb.Min,
            filled_bb.Max,
            IM_COL32(200, 0, 0, 255),
            Theme::Colors::Accent,
            Theme::Colors::Accent,
            IM_COL32(200, 0, 0, 255)
        );

        // Grab handle
        ImVec2 grab_center = ImVec2(grab_x, track_bb.GetCenter().y);
        
        // Handle glow
        if (it->glow > 0.01f) {
            DrawGlow(dl, 
                grab_center - ImVec2(grab_radius, grab_radius),
                grab_center + ImVec2(grab_radius, grab_radius),
                IM_COL32(200, 0, 0, (int)(60 * it->glow)),
                8.0f * it->glow,
                grab_radius
            );
        }
        
        dl->AddCircleFilled(grab_center, grab_radius, Theme::Colors::Accent, 24);
        dl->AddCircle(grab_center, grab_radius, IM_COL32(80, 80, 80, 100), 24, 2.0f);
        
        dl->PopClipRect();

        // Format value text
        char value_buf[64];
        ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
        
        ImGui::PushFont(Fonts::InterMedium);
        ImVec2 value_size = CalcTextSize(value_buf);
        
        // Label
        dl->AddText(
            ImVec2(frame_bb.Min.x, frame_bb.Min.y),
            Theme::Colors::TextSecondary,
            label
        );
        
        // Value
        dl->AddText(
            ImVec2(frame_bb.Max.x - value_size.x, frame_bb.Min.y),
            Theme::Colors::TextPrimary,
            value_buf
        );
        ImGui::PopFont();

        return value_changed;
    }

    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
        return SliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
    }

    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
        return SliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format, flags);
    }

    bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags, const float* ref_col) {
        return ImGui::ColorPicker4(label, col, flags, ref_col);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // SEPARATORS & DECORATIONS
    // ═══════════════════════════════════════════════════════════════════════════

    void GlowSeparator(float width) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return;

        ImDrawList* dl = window->DrawList;
        ImVec2 pos = window->DC.CursorPos;
        float w = width > 0 ? width : window->Size.x - 40;

        // Glow line
        ImVec2 lineStart = pos + ImVec2(0, 4);
        ImVec2 lineEnd = pos + ImVec2(w, 4);
        
        // Multiple glow layers
        for (int i = 3; i >= 0; i--) {
            float thickness = 1.0f + i * 2.0f;
            float alpha = 0.15f * (4 - i);
            dl->AddLine(lineStart, lineEnd, IM_COL32(200, 0, 0, (int)(alpha * 255)), thickness);
        }

        // Core line
        dl->AddLine(lineStart, lineEnd, Theme::Colors::Accent, 1.0f);

        SetCursorPosY(GetCursorPosY() + 12);
    }

    void SectionHeader(const char* label) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return;

        ImDrawList* dl = window->DrawList;
        ImVec2 pos = window->DC.CursorPos;

        ImGui::PushFont(Fonts::InterBold);
        ImVec2 textSize = CalcTextSize(label);
        
        // Text with subtle glow
        dl->AddText(pos + ImVec2(0, 1), IM_COL32(200, 0, 0, 30), label);
        dl->AddText(pos, Theme::Colors::TextMuted, label);
        ImGui::PopFont();

        SetCursorPosY(GetCursorPosY() + textSize.y + 8);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // MODERN COLOR PICKER - Custom implementation
    // ═══════════════════════════════════════════════════════════════════════════

    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags) {
        struct ColorPickerState {
            ImGuiID id;
            bool isOpen;
            float openAnim;
            float hoverAnim;
            bool draggingSV;
            bool draggingHue;
            bool draggingAlpha;
            // Armazenar HSV para evitar perda de precisão
            float cachedH;
            float cachedS;
            float cachedV;
            bool hsvInitialized;
            // Posição fixa do popup (para não mudar com scroll)
            ImVec2 fixedPopupPos;
            bool positionSet;
        };
        
        static std::vector<ColorPickerState> states;
        // activeColorPicker agora é global (declarado no topo do arquivo)

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiID id = window->GetID(label);
        float dt = g.IO.DeltaTime;

        // Find or create state
        auto it = std::find_if(states.begin(), states.end(), [id](const ColorPickerState& s) { return s.id == id; });
        if (it == states.end()) {
            ColorPickerState newState;
            newState.id = id;
            newState.isOpen = false;
            newState.openAnim = 0.0f;
            newState.hoverAnim = 0.0f;
            newState.draggingSV = false;
            newState.draggingHue = false;
            newState.draggingAlpha = false;
            newState.hsvInitialized = false;
            newState.cachedH = 0.0f;
            newState.cachedS = 1.0f;
            newState.cachedV = 1.0f;
            newState.positionSet = false;
            newState.fixedPopupPos = ImVec2(0, 0);
            states.push_back(newState);
            it = states.end() - 1;
        }

        ImGui::PushFont(Fonts::InterMedium);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        ImGui::PopFont();

        const ImVec2 pos = window->DC.CursorPos;
        const float previewSize = 24.0f;
        const ImRect total_bb(pos, pos + ImVec2(GetWindowSize().x - 28, 28));
        const ImRect preview_bb(
            ImVec2(total_bb.Max.x - previewSize, total_bb.GetCenter().y - previewSize * 0.5f),
            ImVec2(total_bb.Max.x, total_bb.GetCenter().y + previewSize * 0.5f)
        );

        ItemSize(total_bb, g.Style.FramePadding.y);
        if (!ItemAdd(total_bb, id)) return false;

        bool hovered = ImGui::IsMouseHoveringRect(preview_bb.Min, preview_bb.Max);
        
        // Registrar hover para toda a área (evita arrasto da janela)
        bool totalHovered = ImGui::IsMouseHoveringRect(total_bb.Min, total_bb.Max);
        if (totalHovered) {
            g.HoveredId = id;
        }
        
        bool clicked = hovered && ImGui::IsMouseClicked(0) && !IsClickBlocked();
        
        // Reset drag states quando mouse é solto
        if (!ImGui::IsMouseDown(0)) {
            it->draggingSV = false;
            it->draggingHue = false;
            it->draggingAlpha = false;
        }

        // Se outro picker está aberto, não permitir abrir este
        if (clicked) {
            if (activeColorPicker == 0) {
                // Nenhum picker aberto, abrir este
                it->isOpen = true;
                it->hsvInitialized = false; // Reinicializar HSV cache
                it->positionSet = false; // Vai setar posição no próximo frame
                activeColorPicker = id;
            } else if (activeColorPicker == id) {
                // Este picker está aberto, fechar
                it->isOpen = false;
                it->positionSet = false;
                activeColorPicker = 0;
            }
            // Se outro picker está aberto, ignorar clique (não fazer nada)
        }
        
        // Sincronizar estado com activeColorPicker
        if (activeColorPicker != 0 && activeColorPicker != id) {
            it->isOpen = false;
            it->positionSet = false;
        }

        it->hoverAnim += ((hovered ? 1.0f : 0.0f) - it->hoverAnim) * dt * 12.0f;
        it->openAnim += ((it->isOpen ? 1.0f : 0.0f) - it->openAnim) * dt * 15.0f;

        ImDrawList* dl = window->DrawList;

        // Label
        ImGui::PushFont(Fonts::InterMedium);
        dl->AddText(
            ImVec2(total_bb.Min.x, total_bb.GetCenter().y - label_size.y * 0.5f),
            IM_COL32(180 + (int)(40 * it->hoverAnim), 180 + (int)(40 * it->hoverAnim), 180 + (int)(40 * it->hoverAnim), 255),
            label
        );
        ImGui::PopFont();

        // Color preview with border
        ImU32 previewCol = IM_COL32(
            (int)(col[0] * 255),
            (int)(col[1] * 255),
            (int)(col[2] * 255),
            (int)(col[3] * 255)
        );
        
        // Checkerboard for alpha
        const float checkSize = 4.0f;
        for (float cy = preview_bb.Min.y; cy < preview_bb.Max.y; cy += checkSize) {
            for (float cx = preview_bb.Min.x; cx < preview_bb.Max.x; cx += checkSize) {
                bool isLight = ((int)((cx - preview_bb.Min.x) / checkSize) + (int)((cy - preview_bb.Min.y) / checkSize)) % 2 == 0;
                ImU32 checkCol = isLight ? IM_COL32(60, 60, 60, 255) : IM_COL32(40, 40, 40, 255);
                float endX = cx + checkSize;
                float endY = cy + checkSize;
                if (endX > preview_bb.Max.x) endX = preview_bb.Max.x;
                if (endY > preview_bb.Max.y) endY = preview_bb.Max.y;
                dl->AddRectFilled(ImVec2(cx, cy), ImVec2(endX, endY), checkCol);
            }
        }
        
        dl->AddRectFilled(preview_bb.Min, preview_bb.Max, previewCol, 6.0f);
        
        ImU32 borderCol = IM_COL32(
            (int)(60 + 195 * it->hoverAnim),
            (int)(60 + 0 * it->hoverAnim),
            (int)(60 + 85 * it->hoverAnim),
            255
        );
        dl->AddRect(preview_bb.Min, preview_bb.Max, borderCol, 6.0f, 0, 1.5f);

        bool changed = false;

        // Popup picker
        if (it->openAnim > 0.01f) {
            float svSize = 150.0f;
            float hueWidth = 20.0f;
            float alphaHeight = 16.0f;
            float buttonHeight = 24.0f;
            float padding = 12.0f;
            float gap = 8.0f;
            
            float popupWidth = padding * 2 + svSize + gap + hueWidth;
            float popupHeight = padding * 2 + svSize + gap + alphaHeight + gap + buttonHeight;
            
            // Salvar posição fixa quando abre (apenas uma vez)
            if (!it->positionSet) {
                it->fixedPopupPos = ImVec2(preview_bb.Max.x - popupWidth, preview_bb.Max.y + 8);
                it->positionSet = true;
            }
            
            ImVec2 popupPos = it->fixedPopupPos;
            ImVec2 popupSize = ImVec2(popupWidth, popupHeight);
            
            // Scale animation
            float scale = 0.9f + 0.1f * it->openAnim;
            ImVec2 scaledSize = popupSize * scale;
            ImVec2 offset = (popupSize - scaledSize) * 0.5f;
            ImVec2 popupMin = popupPos + offset;
            
            ImGui::SetNextWindowPos(popupMin);
            ImGui::SetNextWindowSize(scaledSize);
            ImGui::SetNextWindowFocus(); // Trazer para frente
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, it->openAnim);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.10f, 0.98f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
            
            ImGui::Begin("##ColorPicker", nullptr, 
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_AlwaysAutoResize);
            
            // Trazer janela para frente manualmente
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            {
                // Usar cache de HSV para evitar perda de precisão (especialmente quando V=0 ou S=0)
                float h, s, v;
                if (!it->hsvInitialized) {
                    ImGui::ColorConvertRGBtoHSV(col[0], col[1], col[2], h, s, v);
                    it->cachedH = h;
                    it->cachedS = s;
                    it->cachedV = v;
                    it->hsvInitialized = true;
                } else {
                    h = it->cachedH;
                    s = it->cachedS;
                    v = it->cachedV;
                }
                
                // Scaled dimensions
                float scaledSvSize = svSize * scale;
                float scaledHueWidth = hueWidth * scale;
                float scaledAlphaHeight = alphaHeight * scale;
                float scaledGap = gap * scale;
                float scaledButtonHeight = buttonHeight * scale;
                
                // Color square (SV picker)
                ImVec2 svPos = ImGui::GetCursorScreenPos();
                
                // Draw SV square
                ImDrawList* popupDl = ImGui::GetWindowDrawList();
                
                // Base color
                float hueR, hueG, hueB;
                ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, hueR, hueG, hueB);
                ImU32 hueCol = IM_COL32((int)(hueR*255), (int)(hueG*255), (int)(hueB*255), 255);
                
                float svRounding = 6.0f;
                ImVec2 svMax = ImVec2(svPos.x + scaledSvSize, svPos.y + scaledSvSize);
                
                // Desenhar fundo branco arredondado primeiro
                popupDl->AddRectFilled(svPos, svMax, IM_COL32(255, 255, 255, 255), svRounding);
                
                // Agora desenhar os gradientes COM arredondamento usando PathRect
                // Gradiente horizontal (branco -> cor da hue)
                // Usaremos a técnica de desenhar várias linhas horizontais
                int steps = (int)scaledSvSize;
                for (int i = 0; i < steps; i++) {
                    float t = (float)i / (float)(steps - 1);
                    float y = svPos.y + i;
                    
                    // Calcular quanto recortar dos cantos nessa linha
                    float cornerDist = 0.0f;
                    if (i < svRounding) {
                        float dy = svRounding - i;
                        cornerDist = svRounding - sqrtf(svRounding * svRounding - dy * dy);
                    } else if (i > scaledSvSize - svRounding) {
                        float dy = i - (scaledSvSize - svRounding);
                        cornerDist = svRounding - sqrtf(svRounding * svRounding - dy * dy);
                    }
                    
                    float x1 = svPos.x + cornerDist;
                    float x2 = svMax.x - cornerDist;
                    
                    // Cor do gradiente vertical (branco no topo, preto embaixo)
                    float blackT = t; // 0 no topo, 1 embaixo
                    
                    // Cor da esquerda (branco misturado com preto)
                    int leftR = (int)(255 * (1.0f - blackT));
                    int leftG = (int)(255 * (1.0f - blackT));
                    int leftB = (int)(255 * (1.0f - blackT));
                    
                    // Cor da direita (hue misturada com preto)
                    int rightR = (int)(hueR * 255 * (1.0f - blackT));
                    int rightG = (int)(hueG * 255 * (1.0f - blackT));
                    int rightB = (int)(hueB * 255 * (1.0f - blackT));
                    
                    popupDl->AddRectFilledMultiColor(
                        ImVec2(x1, y), ImVec2(x2, y + 1),
                        IM_COL32(leftR, leftG, leftB, 255),
                        IM_COL32(rightR, rightG, rightB, 255),
                        IM_COL32(rightR, rightG, rightB, 255),
                        IM_COL32(leftR, leftG, leftB, 255)
                    );
                }
                
                // Borda
                popupDl->AddRect(svPos, svMax, IM_COL32(50, 50, 50, 255), svRounding);
                
                // SV selector circle - clipar dentro
                float svSelectorX = svPos.x + s * scaledSvSize;
                float svSelectorY = svPos.y + (1.0f - v) * scaledSvSize;
                svSelectorX = ImClamp(svSelectorX, svPos.x + 3.0f, svMax.x - 3.0f);
                svSelectorY = ImClamp(svSelectorY, svPos.y + 3.0f, svMax.y - 3.0f);
                popupDl->AddCircleFilled(ImVec2(svSelectorX, svSelectorY), 5.0f, IM_COL32(255, 255, 255, 255), 16);
                popupDl->AddCircle(ImVec2(svSelectorX, svSelectorY), 5.0f, IM_COL32(0, 0, 0, 180), 16, 1.5f);
                
                // Handle SV drag
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                ImRect svRect(svPos, ImVec2(svPos.x + scaledSvSize, svPos.y + scaledSvSize));
                bool svHovered = ImGui::IsMouseHoveringRect(svRect.Min, svRect.Max);
                if (svHovered && ImGui::IsMouseClicked(0)) {
                    it->draggingSV = true;
                }
                if (it->draggingSV && ImGui::IsMouseDown(0)) {
                    float newS = (mousePos.x - svPos.x) / scaledSvSize;
                    float newV = 1.0f - (mousePos.y - svPos.y) / scaledSvSize;
                    s = ImClamp(newS, 0.0f, 1.0f);
                    v = ImClamp(newV, 0.0f, 1.0f);
                    // Atualizar cache
                    it->cachedS = s;
                    it->cachedV = v;
                    ImGui::ColorConvertHSVtoRGB(h, s, v, col[0], col[1], col[2]);
                    changed = true;
                }
                
                // Hue bar
                float hueX = svPos.x + scaledSvSize + scaledGap;
                float hueHeight = scaledSvSize;
                
                for (int i = 0; i < 6; i++) {
                    float h1 = i / 6.0f;
                    float h2 = (i + 1) / 6.0f;
                    float r1, g1, b1, r2, g2, b2;
                    ImGui::ColorConvertHSVtoRGB(h1, 1.0f, 1.0f, r1, g1, b1);
                    ImGui::ColorConvertHSVtoRGB(h2, 1.0f, 1.0f, r2, g2, b2);
                    
                    popupDl->AddRectFilledMultiColor(
                        ImVec2(hueX, svPos.y + hueHeight * h1),
                        ImVec2(hueX + scaledHueWidth, svPos.y + hueHeight * h2),
                        IM_COL32((int)(r1*255), (int)(g1*255), (int)(b1*255), 255),
                        IM_COL32((int)(r1*255), (int)(g1*255), (int)(b1*255), 255),
                        IM_COL32((int)(r2*255), (int)(g2*255), (int)(b2*255), 255),
                        IM_COL32((int)(r2*255), (int)(g2*255), (int)(b2*255), 255)
                    );
                }
                popupDl->AddRect(ImVec2(hueX, svPos.y), ImVec2(hueX + scaledHueWidth, svPos.y + hueHeight), IM_COL32(60, 60, 60, 255), 4.0f);
                
                // Hue selector - clamp position
                float hueSelectorY = svPos.y + ImClamp(h, 0.0f, 0.999f) * hueHeight;
                popupDl->AddRectFilled(
                    ImVec2(hueX - 2, hueSelectorY - 3),
                    ImVec2(hueX + scaledHueWidth + 2, hueSelectorY + 3),
                    IM_COL32(255, 255, 255, 255), 2.0f
                );
                
                // Handle Hue drag
                ImRect hueRect(ImVec2(hueX - 5, svPos.y - 5), ImVec2(hueX + scaledHueWidth + 5, svPos.y + hueHeight + 5));
                bool hueHovered = ImGui::IsMouseHoveringRect(hueRect.Min, hueRect.Max);
                if (hueHovered && ImGui::IsMouseClicked(0)) {
                    it->draggingHue = true;
                }
                if (it->draggingHue && ImGui::IsMouseDown(0)) {
                    float newH = (mousePos.y - svPos.y) / hueHeight;
                    h = ImClamp(newH, 0.0f, 0.999f);
                    // Atualizar cache
                    it->cachedH = h;
                    ImGui::ColorConvertHSVtoRGB(h, s, v, col[0], col[1], col[2]);
                    changed = true;
                }
                
                // Alpha bar - mesmo tamanho do SV square
                float alphaY = svPos.y + scaledSvSize + scaledGap;
                float alphaWidth = scaledSvSize; // Mesmo tamanho do SV
                
                // Alpha gradient
                ImU32 colNoAlpha = IM_COL32((int)(col[0]*255), (int)(col[1]*255), (int)(col[2]*255), 0);
                ImU32 colFullAlpha = IM_COL32((int)(col[0]*255), (int)(col[1]*255), (int)(col[2]*255), 255);
                popupDl->AddRectFilledMultiColor(
                    ImVec2(svPos.x, alphaY), 
                    ImVec2(svPos.x + alphaWidth, alphaY + scaledAlphaHeight), 
                    colNoAlpha, colFullAlpha, colFullAlpha, colNoAlpha
                );
                popupDl->AddRect(ImVec2(svPos.x, alphaY), ImVec2(svPos.x + alphaWidth, alphaY + scaledAlphaHeight), IM_COL32(60, 60, 60, 255), 4.0f);
                
                // Alpha selector - clamp position
                float alphaSelectorX = svPos.x + ImClamp(col[3], 0.0f, 1.0f) * alphaWidth;
                popupDl->AddRectFilled(
                    ImVec2(alphaSelectorX - 3, alphaY - 2),
                    ImVec2(alphaSelectorX + 3, alphaY + scaledAlphaHeight + 2),
                    IM_COL32(255, 255, 255, 255), 2.0f
                );
                
                // Handle Alpha drag
                ImRect alphaRect(ImVec2(svPos.x - 5, alphaY - 5), ImVec2(svPos.x + alphaWidth + 5, alphaY + scaledAlphaHeight + 5));
                bool alphaHovered = ImGui::IsMouseHoveringRect(alphaRect.Min, alphaRect.Max);
                if (alphaHovered && ImGui::IsMouseClicked(0)) {
                    it->draggingAlpha = true;
                }
                if (it->draggingAlpha && ImGui::IsMouseDown(0)) {
                    float newA = (mousePos.x - svPos.x) / alphaWidth;
                    col[3] = ImClamp(newA, 0.0f, 1.0f);
                    changed = true;
                }
                
                // Botão fechar - alinhado com alpha bar
                float buttonY = alphaY + scaledAlphaHeight + scaledGap;
                float buttonWidth = alphaWidth;
                
                ImRect closeBtn(
                    ImVec2(svPos.x, buttonY),
                    ImVec2(svPos.x + buttonWidth, buttonY + scaledButtonHeight)
                );
                
                bool closeBtnHovered = ImGui::IsMouseHoveringRect(closeBtn.Min, closeBtn.Max);
                bool closeBtnClicked = closeBtnHovered && ImGui::IsMouseClicked(0);
                
                // Background do botão
                ImU32 btnBg = closeBtnHovered ? IM_COL32(200, 0, 0, 200) : IM_COL32(60, 60, 60, 255);
                popupDl->AddRectFilled(closeBtn.Min, closeBtn.Max, btnBg, 4.0f);
                
                // Texto "OK"
                ImGui::PushFont(Fonts::InterMedium);
                const char* btnText = "OK";
                ImVec2 btnTextSize = ImGui::CalcTextSize(btnText);
                ImVec2 btnTextPos = ImVec2(
                    closeBtn.GetCenter().x - btnTextSize.x * 0.5f,
                    closeBtn.GetCenter().y - btnTextSize.y * 0.5f
                );
                popupDl->AddText(btnTextPos, IM_COL32(255, 255, 255, 255), btnText);
                ImGui::PopFont();
                
                if (closeBtnClicked) {
                    it->isOpen = false;
                    activeColorPicker = 0;
                }
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
        }

        return changed;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // MODERN COMBO BOX
    // ═══════════════════════════════════════════════════════════════════════════

    bool Combo(const char* label, int* current_item, const char* items_separated_by_zeros) {
        struct ComboState {
            ImGuiID id;
            bool isOpen;
            float openAnim;
            float hoverAnim;
            std::vector<float> itemHoverAnims;
        };
        
        static std::vector<ComboState> states;
        // activeCombo agora é global (declarado no topo do arquivo)
        
        // Atualizar bloqueio global
        UpdateClickBlock();

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiID id = window->GetID(label);
        float dt = g.IO.DeltaTime;
        
        // Extrair display label (remover ## e tudo depois)
        std::string displayLabel = label;
        size_t hashPos = displayLabel.find("##");
        if (hashPos != std::string::npos) {
            displayLabel = displayLabel.substr(0, hashPos);
        }

        // Parse items
        std::vector<std::string> items;
        const char* p = items_separated_by_zeros;
        while (*p) {
            items.push_back(p);
            p += strlen(p) + 1;
        }

        // Find or create state
        auto it = std::find_if(states.begin(), states.end(), [id](const ComboState& s) { return s.id == id; });
        if (it == states.end()) {
            ComboState newState;
            newState.id = id;
            newState.isOpen = false;
            newState.openAnim = 0.0f;
            newState.hoverAnim = 0.0f;
            newState.itemHoverAnims.resize(items.size(), 0.0f);
            states.push_back(newState);
            it = states.end() - 1;
        }
        
        if (it->itemHoverAnims.size() != items.size()) {
            it->itemHoverAnims.resize(items.size(), 0.0f);
        }

        ImGui::PushFont(Fonts::InterMedium);
        const ImVec2 label_size = CalcTextSize(displayLabel.c_str(), NULL, true);
        ImGui::PopFont();

        const ImVec2 pos = window->DC.CursorPos;
        const float comboWidth = 100.0f;
        const ImRect total_bb(pos, pos + ImVec2(GetWindowSize().x - 28, 28));
        const ImRect combo_bb(
            ImVec2(total_bb.Max.x - comboWidth, total_bb.Min.y),
            ImVec2(total_bb.Max.x, total_bb.Max.y)
        );

        ItemSize(total_bb, g.Style.FramePadding.y);
        if (!ItemAdd(total_bb, id)) return false;

        // Se cliques estão bloqueados, não processar
        bool clicksBlocked = IsClickBlocked();
        
        bool hovered = ImGui::IsMouseHoveringRect(combo_bb.Min, combo_bb.Max);
        
        // Registrar hover para toda a área (evita arrasto da janela)
        bool totalHovered = ImGui::IsMouseHoveringRect(total_bb.Min, total_bb.Max);
        if (totalHovered) {
            g.HoveredId = id;
        }
        
        bool clicked = hovered && ImGui::IsMouseClicked(0) && !clicksBlocked;

        // Toggle ao clicar no combo
        if (clicked) {
            // Close other combos
            if (activeCombo != 0 && activeCombo != id) {
                for (auto& s : states) {
                    if (s.id == activeCombo) {
                        s.isOpen = false;
                        break;
                    }
                }
                activeComboPopupRect = ImRect(0, 0, 0, 0);
            }
            it->isOpen = !it->isOpen;
            activeCombo = it->isOpen ? id : 0;
            if (!it->isOpen) {
                activeComboPopupRect = ImRect(0, 0, 0, 0);
            }
        }

        it->hoverAnim += ((hovered ? 1.0f : 0.0f) - it->hoverAnim) * dt * 12.0f;
        // Animação de fechar MUITO mais rápida que abrir
        float openSpeed = it->isOpen ? 18.0f : 100.0f; // 100 = muito rápido para fechar
        it->openAnim += ((it->isOpen ? 1.0f : 0.0f) - it->openAnim) * dt * openSpeed;
        
        // Forçar zero se muito pequeno (fechar rápido)
        if (!it->isOpen && it->openAnim < 0.1f) {
            it->openAnim = 0.0f;
        }

        ImDrawList* dl = window->DrawList;

        // Label (usar displayLabel sem ##)
        ImGui::PushFont(Fonts::InterMedium);
        dl->AddText(
            ImVec2(total_bb.Min.x, total_bb.GetCenter().y - label_size.y * 0.5f),
            IM_COL32(180 + (int)(40 * it->hoverAnim), 180 + (int)(40 * it->hoverAnim), 180 + (int)(30 * it->hoverAnim), 255),
            displayLabel.c_str()
        );
        ImGui::PopFont();

        // Combo box background
        ImU32 bgColor = IM_COL32(
            (int)(28 + 20 * it->hoverAnim),
            (int)(28 + 20 * it->hoverAnim),
            (int)(28 + 20 * it->hoverAnim),
            255
        );
        dl->AddRectFilled(combo_bb.Min, combo_bb.Max, bgColor, 8.0f);

        ImU32 borderCol = IM_COL32(
            (int)(50 + 255 * it->openAnim * 0.5f),
            (int)(50 + 255 * it->openAnim * 0.5f),
            (int)(50 + 255 * it->openAnim * 0.5f),
            (int)(120 + 80 * it->hoverAnim)
        );
        dl->AddRect(combo_bb.Min, combo_bb.Max, borderCol, 8.0f, 0, 1.0f);

        // Current item text
        const char* currentText = (*current_item >= 0 && *current_item < (int)items.size()) ? items[*current_item].c_str() : "";
        ImGui::PushFont(Fonts::InterMedium);
        ImVec2 textSize = CalcTextSize(currentText);
        dl->AddText(
            ImVec2(combo_bb.Min.x + 10, combo_bb.GetCenter().y - textSize.y * 0.5f),
            IM_COL32(200 + (int)(55 * it->hoverAnim), 200 + (int)(55 * it->hoverAnim), 200 + (int)(45 * it->hoverAnim), 255),
            currentText
        );
        ImGui::PopFont();

        // Dropdown arrow - usar FontAwesome para ícone consistente
        ImGui::PushFont(Fonts::FontAwesomeSolid);
        const char* arrowIcon = it->openAnim > 0.5f ? ICON_FA_CHEVRON_UP : ICON_FA_CHEVRON_DOWN;
        ImVec2 arrowSize = CalcTextSize(arrowIcon);
        ImVec2 arrowPos = ImVec2(combo_bb.Max.x - arrowSize.x - 8, combo_bb.GetCenter().y - arrowSize.y * 0.5f);
        dl->AddText(arrowPos, IM_COL32(150 + (int)(105 * it->hoverAnim), 150 + (int)(105 * it->hoverAnim), 150 + (int)(95 * it->hoverAnim), 255), arrowIcon);
        ImGui::PopFont();

        bool changed = false;

        // Dropdown popup - usar janela real para bloquear input atrás
        if (it->openAnim > 0.01f) {
            float itemHeight = 28.0f;
            float popupHeight = items.size() * itemHeight + 8;
            ImVec2 popupPos = ImVec2(combo_bb.Min.x, combo_bb.Max.y + 4);
            ImVec2 popupSize = ImVec2(comboWidth, popupHeight);
            
            // Atualizar área global do popup
            activeComboPopupRect = ImRect(popupPos, popupPos + popupSize);
            
            // Criar janela popup real para capturar input
            ImGui::SetNextWindowPos(popupPos);
            ImGui::SetNextWindowSize(popupSize);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, it->openAnim);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.118f, 0.118f, 0.118f, 0.98f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.216f, 0.216f, 0.216f, 0.8f));
            
            char popupName[128];
            snprintf(popupName, sizeof(popupName), "##ComboPopup_%s", label);
            
            ImGui::Begin(popupName, nullptr, 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoSavedSettings);
            
            ImDrawList* popupDl = ImGui::GetWindowDrawList();
            
            // Borda
            popupDl->AddRect(popupPos, popupPos + popupSize, 
                IM_COL32(55, 55, 55, (int)(200 * it->openAnim)), 8.0f, 0, 1.0f);
            
            // Items
            ImVec2 itemPos = popupPos + ImVec2(4, 4);
            
            for (int i = 0; i < (int)items.size(); i++) {
                ImRect itemRect(itemPos, itemPos + ImVec2(comboWidth - 8, itemHeight));
                
                // Verificar hover manualmente (não usa IsMouseHoveringRect que pode ser afetado por ordem de janelas)
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                bool itemHovered = mousePos.x >= itemRect.Min.x && mousePos.x <= itemRect.Max.x &&
                                   mousePos.y >= itemRect.Min.y && mousePos.y <= itemRect.Max.y;
                bool itemClicked = itemHovered && ImGui::IsMouseClicked(0);
                bool isSelected = (i == *current_item);
                
                it->itemHoverAnims[i] += ((itemHovered ? 1.0f : 0.0f) - it->itemHoverAnims[i]) * dt * 15.0f;
                
                if (itemClicked && it->openAnim > 0.5f) {
                    *current_item = i;
                    it->isOpen = false;
                    activeCombo = 0;
                    activeComboPopupRect = ImRect(0, 0, 0, 0);
                    changed = true;
                    g_ComboBlockClickFrames = 3; // Bloquear cliques por 3 frames
                }
                
                // Item background
                float itemBgAlpha = it->itemHoverAnims[i] * 0.3f + (isSelected ? 0.4f : 0.0f);
                if (itemBgAlpha > 0.01f) {
                    popupDl->AddRectFilled(itemRect.Min, itemRect.Max, 
                        IM_COL32(200, 0, 0, (int)(itemBgAlpha * 255 * it->openAnim)), 6.0f);
                }
                
                // Item text
                ImGui::PushFont(Fonts::InterMedium);
                ImVec2 itemTextSize = CalcTextSize(items[i].c_str());
                ImVec2 itemTextPos = ImVec2(
                    itemRect.Min.x + 8,
                    itemRect.GetCenter().y - itemTextSize.y * 0.5f
                );
                
                ImU32 itemTextCol = IM_COL32(
                    (int)(170 + 85 * (it->itemHoverAnims[i] + (isSelected ? 0.5f : 0.0f))),
                    (int)(170 + 85 * (it->itemHoverAnims[i] + (isSelected ? 0.5f : 0.0f))),
                    (int)(170 + 85 * (it->itemHoverAnims[i] + (isSelected ? 0.5f : 0.0f))),
                    (int)(255 * it->openAnim)
                );
                popupDl->AddText(itemTextPos, itemTextCol, items[i].c_str());
                ImGui::PopFont();
                
                itemPos.y += itemHeight;
            }
            
            ImGui::End();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
        }
        
        // Fechar se clicar fora (DEPOIS de processar items para dar prioridade à seleção)
        if (it->isOpen && !changed && ImGui::IsMouseClicked(0) && !hovered) {
            float itemHeight = 28.0f;
            float popupHeight = items.size() * itemHeight + 8;
            ImVec2 popupPos = ImVec2(combo_bb.Min.x, combo_bb.Max.y + 4);
            ImVec2 popupMax = popupPos + ImVec2(comboWidth, popupHeight);
            
            // Verificar manualmente se o mouse está sobre o popup (não usa IsMouseHoveringRect que pode ser afetado por ordem de janelas)
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            bool mouseInPopup = mousePos.x >= popupPos.x && mousePos.x <= popupMax.x &&
                               mousePos.y >= popupPos.y && mousePos.y <= popupMax.y;
            
            if (!mouseInPopup) {
                it->isOpen = false;
                if (activeCombo == id) {
                    activeCombo = 0;
                    activeComboPopupRect = ImRect(0, 0, 0, 0);
                }
                g_ComboBlockClickFrames = 2;
            }
        }

        return changed;
    }

} // namespace Custom
