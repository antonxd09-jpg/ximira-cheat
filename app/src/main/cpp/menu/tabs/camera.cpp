#include "menu/tabs/tabs.h"
#include "menu/style.h"
#include "menu/widgets.h"
#include "features/camera_mod/camera_mod.h"

namespace xim::menu::tabs {

using namespace xim::features;
using namespace xim::menu::widget;

void draw_camera() {
    ImGui::TextColored(ImVec4(0.18f, 0.50f, 1.0f, 1.0f), "Camera");
    ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                       "FOV, third person, shake");
    ImGui::Separator();

    camera_mod::Config& c = camera_mod::config();

    // ---- FOV ----
    ImGui::BeginChild("##fov", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = c.fov_enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("FOV Changer");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##fov_en", &en)) c.fov_enabled.store(en);
        if (en) {
            ImGui::Separator();
            float v = c.fov_value.load(std::memory_order_relaxed);
            if (slider_float("FOV", &v, 60.0f, 160.0f, "%.0f"))
                c.fov_value.store(v);
        }
    }
    ImGui::EndChild();

    // ---- Third person ----
    ImGui::BeginChild("##tp", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = c.tp_enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("Third Person");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##tp_en", &en)) c.tp_enabled.store(en);
        if (en) {
            ImGui::Separator();
            float d = c.tp_distance.load(std::memory_order_relaxed);
            float h = c.tp_height.load(std::memory_order_relaxed);
            float s = c.tp_side.load(std::memory_order_relaxed);
            if (slider_float("Distance", &d, 0.5f, 8.0f, "%.1f")) c.tp_distance.store(d);
            if (slider_float("Height",   &h, -2.0f, 2.0f, "%.2f")) c.tp_height.store(h);
            if (slider_float("Side",     &s, -2.0f, 2.0f, "%.2f")) c.tp_side.store(s);
        }
    }
    ImGui::EndChild();

    // ---- No shake ----
    ImGui::BeginChild("##shake", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = c.no_shake.load(std::memory_order_relaxed);
        if (toggle("Disable Camera Shake", &en)) c.no_shake.store(en);
    }
    ImGui::EndChild();

    // ---- Info ----
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                       "Camera hook: %s", 
                       c.hook_ready.load(std::memory_order_relaxed) ? "ready" : "waiting");
}

} // namespace xim::menu::tabs