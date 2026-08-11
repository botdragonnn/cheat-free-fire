#pragma once
#include "Includes.hpp"

// ═══════════════════════════════════════════════════════════════════════════════
// Lux - Modern Minimal UI Theme
// Dark Theme with Neutral Accent — #3D3D3D
// ═══════════════════════════════════════════════════════════════════════════════

// Global drag state para children headers
namespace CustomDrag {
    inline bool WantDrag = false;
    inline ImVec2 DragClickPos = ImVec2(0, 0);
}

namespace Theme {
    // ─── Color Palette ───
    namespace Colors {
        // Backgrounds
        constexpr ImU32 Background          = IM_COL32(13, 13, 13, 255);
        constexpr ImU32 BackgroundSecondary = IM_COL32(18, 18, 18, 255);
        constexpr ImU32 BackgroundTertiary  = IM_COL32(24, 24, 24, 255);
        constexpr ImU32 Card                = IM_COL32(22, 22, 22, 255);
        constexpr ImU32 CardHover           = IM_COL32(28, 28, 28, 255);
        
        // Accent Colors (Lux Dark Gray — #3D3D3D)
        constexpr ImU32 Accent              = IM_COL32(200, 0, 0, 255);       // #3D3D3D
        constexpr ImU32 AccentHover         = IM_COL32(85, 85, 85, 255);       // Lighter gray
        constexpr ImU32 AccentDim           = IM_COL32(200, 0, 0, 100);       // Semi-transparent
        constexpr ImU32 AccentGlow          = IM_COL32(200, 0, 0, 50);        // For glow effects
        constexpr ImU32 AccentSubtle        = IM_COL32(200, 0, 0, 25);        // Very subtle
        
        // Text
        constexpr ImU32 TextPrimary         = IM_COL32(245, 245, 245, 255);
        constexpr ImU32 TextSecondary       = IM_COL32(160, 160, 160, 255);
        constexpr ImU32 TextMuted           = IM_COL32(100, 100, 100, 255);
        constexpr ImU32 TextDisabled        = IM_COL32(70, 70, 70, 255);
        
        // Borders
        constexpr ImU32 Border              = IM_COL32(45, 45, 45, 255);
        constexpr ImU32 BorderSubtle        = IM_COL32(35, 35, 35, 255);
        constexpr ImU32 BorderAccent        = IM_COL32(80, 80, 80, 150);
        
        // States
        constexpr ImU32 Success             = IM_COL32(80, 220, 120, 255);
        constexpr ImU32 Warning             = IM_COL32(255, 180, 50, 255);
        constexpr ImU32 Error               = IM_COL32(255, 80, 100, 255);
        
        // Dock
        constexpr ImU32 DockBackground      = IM_COL32(30, 30, 30, 200);
        constexpr ImU32 DockItem            = IM_COL32(40, 40, 40, 255);
        constexpr ImU32 DockItemActive      = IM_COL32(200, 0, 0, 255);
    }
    
    // ─── Sizing Constants ───
    namespace Size {
        constexpr float WindowRounding      = 16.0f;
        constexpr float CardRounding        = 12.0f;
        constexpr float ButtonRounding      = 10.0f;
        constexpr float SmallRounding       = 8.0f;
        constexpr float TinyRounding        = 6.0f;
        
        constexpr float DockHeight          = 70.0f;
        constexpr float DockItemSize        = 50.0f;
        constexpr float DockItemSpacing     = 8.0f;
        constexpr float DockRounding        = 20.0f;
        
        constexpr float GlowSize            = 12.0f;
        constexpr float ShadowOffset        = 4.0f;
    }
    
    // ─── Animation Speeds ───
    namespace Animation {
        constexpr float Fast                = 15.0f;
        constexpr float Normal              = 10.0f;
        constexpr float Slow                = 6.0f;
        constexpr float VerySlow            = 3.0f;
    }
}

namespace Custom {
    // ─── Utility Functions ───
    void DrawGlow(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, float radius, float rounding = 0.0f);
    void DrawShadow(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding = 0.0f, float offset = 4.0f);
    void DrawGradientRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 col1, ImU32 col2, bool horizontal = true, float rounding = 0.0f);
    
    // ─── Profile Bar ───
    void ProfileBar(const char* Username, const char* Role, const char* ExpiryStr);
    
    // ─── Buttons ───
    bool Button(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool GlowButton(const char* label, const ImVec2& size_arg = ImVec2(0, 0));
    bool IconButton(const char* icon, const char* tooltip = nullptr, bool active = false);
    
    // ─── Dock System ───
    bool DockItem(const char* icon, const char* label, bool active, int index);
    void BeginDock();
    void EndDock();
    
    // ─── Cards/Containers ───
    bool CustomChild(const char* str_id, const ImVec2& size);
    void EndCustomChild();
    bool GlassCard(const char* str_id, const ImVec2& size);
    void EndGlassCard();
    
    // ─── Tabs ───
    bool Tab(const char* Label, const char* Icon, bool Enabled);
    bool TabHeader(const char* tabname, int* currentTab, std::vector<const char*> Tabs, int TabIndex);
    bool TabHeaderButton(const char* ButtonName, bool enabled, int direction, int TabIndex);
    bool ModernTab(const char* label, const char* icon, bool* selected);
    
    // ─── Input Controls ───
    bool Checkbox(const char* label, bool* v, float width = 0.0f);
    bool ToggleSwitch(const char* label, bool* v);
    bool KeyBind(const char* label, int* Key, bool IsBlockMouse = false);
    
    // ─── Sliders ───
    bool SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format = NULL, ImGuiSliderFlags flags = 0);
    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
    
    // ─── Color Controls ───
    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);
    bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags = 0, const float* ref_col = NULL);
    
    // ─── Combo Box ───
    bool Combo(const char* label, int* current_item, const char* items_separated_by_zeros);
    
    // ─── Separators/Decorations ───
    void GlowSeparator(float width = 0.0f);
    void SectionHeader(const char* label);
}
