#include "menu/menu.h"
#include "menu/style.h"
#include "menu/widgets.h"
#include "menu/tabs/tabs.h"
#include "render/imgui_backend.h"
#include "util/log.h"

#include <GLES3/gl3.h>
#include <android/log.h>

namespace xim::menu {

static State g_state;
State& state() { return g_state; }

bool is_open()      { return g_state.open; }
void set_open(bool v){ g_state.open = v; }

// ссылка на разработчика
static constexpr const char* kDevUrl = "https://t.me/skeetcod";

void init() {
    style::apply();
    style::load_fonts();
    XLOG("menu initialized");
}

// ---- sidebar ----
static const char* kTabs[] = { "Visuals", "World", "Camera", "Misc", "Configs" };
static constexpr int kTabCount = IM_ARRAYSIZE(kTabs);

static void draw_sidebar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.047f, 0.063f, 0.09f, 1.0f));
    ImGui::BeginChild("##sidebar", ImVec2(180, 0), ImGuiChildFlags_Border);

    ImGui::Dummy(ImVec2(0, 8));

    // лого X + название
    ImGui::PushFont(style::fonts().logo);
    ImGui::TextColored(ImVec4(0.18f, 0.50f, 1.0f, 1.0f), "  X");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.86f, 0.89f, 0.94f, 1.0f), " XIMIRA");
    ImGui::PopFont();
    ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                       "  PREMIUM CHEAT INTERFACE");

    ImGui::Dummy(ImVec2(0, 16));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    // табы
    for (int i = 0; i < kTabCount; ++i) {
        bool active = (g_state.tab == i);

        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Header,
                ImVec4(0.18f, 0.50f, 1.0f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.18f, 0.50f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.51f, 0.57f, 0.65f, 1.0f));
        }

        // синяя полоса слева у активного
        ImVec2 p = ImGui::GetCursorScreenPos();
        if (active) {
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x, p.y + 4),
                ImVec2(p.x + 3, p.y + ImGui::GetTextLineHeight() + 12),
                style::kAccent, 2.0f);
        }

        if (ImGui::Selectable(
                (std::string("   ") + kTabs[i]).c_str(),
                active,
                ImGuiSelectableFlags_None,
                ImVec2(0, 36)))
            g_state.tab = i;

        ImGui::PopStyleColor(2);
    }

    // ---- футер ----
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 110);

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));

    ImGui::TextColored(ImVec4(0.86f, 0.89f, 0.94f, 1.0f), "  XIMIRA");
    ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                       "  v" XIM_VERSION);
    ImGui::Dummy(ImVec2(0, 4));

    // кликабельная ссылка на телегу
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
    link("@skeetcod", kDevUrl);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ---- main window ----
void draw() {
    if (!g_state.open) return;

    // размер: фиксированный базовый × scale
    float base_w = 900.0f;
    float base_h = 620.0f;
    float w = base_w * g_state.scale;
    float h = base_h * g_state.scale;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 size(w, h);

    // позиция
    ImVec2 pos;
    if (g_state.pos_x < 0 || g_state.pos_y < 0) {
        pos = ImVec2((io.DisplaySize.x - size.x) * 0.5f,
                     (io.DisplaySize.y - size.y) * 0.5f);
    } else {
        pos = ImVec2((float)g_state.pos_x, (float)g_state.pos_y);
    }

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##ximira_main", nullptr, flags);

    // ---- шапка ----
    ImGui::BeginChild("##header", ImVec2(0, 56), ImGuiChildFlags_None);
    {
        ImGui::SetCursorPos(ImVec2(16, 12));
        ImGui::PushFont(style::fonts().logo);
        ImGui::TextColored(ImVec4(0.18f, 0.50f, 1.0f, 1.0f), "X");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::PushFont(style::fonts().title);
        ImGui::TextColored(ImVec4(0.86f, 0.89f, 0.94f, 1.0f), "XIMIRA");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                           "  PREMIUM CHEAT INTERFACE");

        // статус + кнопки свернуть/закрыть (справа)
        float right = ImGui::GetWindowWidth() - 16;
        ImGui::SetCursorPos(ImVec2(right - 200, 14));
        ImGui::TextColored(ImVec4(0.18f, 0.50f, 1.0f, 0.8f), "● UI Active");

        ImGui::SetCursorPos(ImVec2(right - 40, 12));
        if (ImGui::Button("—", ImVec2(28, 28))) g_state.open = false;

        ImGui::SetCursorPos(ImVec2(right - 8, 12));
        if (ImGui::Button("X", ImVec2(28, 28))) g_state.open = false;
    }
    ImGui::EndChild();

    ImGui::Separator();

    // ---- тело: sidebar + content ----
    ImGui::BeginChild("##body", ImVec2(0, 0), ImGuiChildFlags_None);
    {
        draw_sidebar();
        ImGui::SameLine();

        ImGui::BeginChild("##content", ImVec2(0, 0), ImGuiChildFlags_None);
        ImGui::SetCursorPos(ImVec2(16, 12));

        switch (g_state.tab) {
            case 0: tabs::draw_visuals(); break;
            case 1: tabs::draw_world();   break;
            case 2: tabs::draw_camera();  break;
            case 3: tabs::draw_misc();    break;
            case 4: tabs::draw_configs(); break;
        }

        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace xim::menu