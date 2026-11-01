#include "menu/style.h"
#include "util/log.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

namespace xim::menu::style {

static Fonts g_fonts;

Fonts& fonts() { return g_fonts; }

void apply() {
    ImGuiStyle& s = ImGui::GetStyle();

    // ---- геометрия ----
    s.WindowRounding    = 10.0f;
    s.ChildRounding     = 8.0f;
    s.FrameRounding     = 6.0f;
    s.PopupRounding     = 8.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding      = 6.0f;
    s.TabRounding       = 6.0f;

    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.FrameBorderSize   = 1.0f;
    s.PopupBorderSize   = 1.0f;

    s.WindowPadding     = ImVec2(16, 16);
    s.FramePadding      = ImVec2(12, 8);
    s.ItemSpacing       = ImVec2(10, 10);
    s.ItemInnerSpacing  = ImVec2(8, 6);
    s.ScrollbarSize     = 8.0f;
    s.GrabMinSize       = 14.0f;

    // ---- цвета ----
    ImVec4* c = s.Colors;

    c[ImGuiCol_WindowBg]             = ImVec4(0.04f, 0.055f, 0.08f, 1.0f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.06f, 0.08f, 0.11f, 1.0f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.05f, 0.07f, 0.10f, 1.0f);
    c[ImGuiCol_Border]               = ImVec4(0.10f, 0.14f, 0.20f, 1.0f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_Text]                 = ImVec4(0.86f, 0.89f, 0.94f, 1.0f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.35f, 0.40f, 0.47f, 1.0f);

    c[ImGuiCol_FrameBg]              = ImVec4(0.08f, 0.10f, 0.14f, 1.0f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.11f, 0.14f, 0.19f, 1.0f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.13f, 0.17f, 0.23f, 1.0f);

    c[ImGuiCol_TitleBg]              = ImVec4(0.04f, 0.055f, 0.08f, 1.0f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.04f, 0.055f, 0.08f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.04f, 0.055f, 0.08f, 1.0f);

    c[ImGuiCol_MenuBarBg]            = ImVec4(0.05f, 0.07f, 0.10f, 1.0f);

    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.14f, 0.18f, 0.24f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.23f, 0.30f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.18f, 0.50f, 1.00f, 1.0f);

    c[ImGuiCol_CheckMark]            = ImVec4(0.18f, 0.50f, 1.00f, 1.0f);

    c[ImGuiCol_SliderGrab]           = ImVec4(0.18f, 0.50f, 1.00f, 1.0f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.25f, 0.58f, 1.00f, 1.0f);

    c[ImGuiCol_Button]               = ImVec4(0.09f, 0.12f, 0.16f, 1.0f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.18f, 0.50f, 1.00f, 0.55f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.18f, 0.50f, 1.00f, 0.85f);

    c[ImGuiCol_Header]               = ImVec4(0.18f, 0.50f, 1.00f, 0.20f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.18f, 0.50f, 1.00f, 0.35f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.18f, 0.50f, 1.00f, 0.55f);

    c[ImGuiCol_Separator]            = ImVec4(0.10f, 0.14f, 0.20f, 1.0f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.18f, 0.50f, 1.00f, 0.60f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.18f, 0.50f, 1.00f, 1.00f);

    c[ImGuiCol_Tab]                  = ImVec4(0.07f, 0.09f, 0.12f, 1.0f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.18f, 0.50f, 1.00f, 0.45f);
    c[ImGuiCol_TabSelected]            = ImVec4(0.18f, 0.50f, 1.00f, 0.80f);
    c[ImGuiCol_TabDimmed]         = ImVec4(0.07f, 0.09f, 0.12f, 1.0f);
    c[ImGuiCol_TabDimmedSelected]   = ImVec4(0.11f, 0.14f, 0.19f, 1.0f);

    c[ImGuiCol_NavHighlight]         = ImVec4(0.18f, 0.50f, 1.00f, 1.00f);
}

// ---- шрифты ----
// положи их в app/src/main/assets/fonts/
// regular.ttf, small.ttf (опционально), bold.ttf (для заголовка и лого)
void load_fonts() {
    ImGuiIO& io = ImGui::GetIO();

    // базовый — Inter Regular 15px
    g_fonts.regular = io.Fonts->AddFontFromFileTTF(
            "/system/fonts/Roboto-Regular.ttf", 15.0f);

    // fallback, если Roboto нет
    if (!g_fonts.regular)
        g_fonts.regular = io.Fonts->AddFontDefault();

    g_fonts.small = g_fonts.regular;
    g_fonts.title = g_fonts.regular;
    g_fonts.logo  = g_fonts.regular;

    XLOG("fonts loaded");
}

} // namespace xim::menu::style