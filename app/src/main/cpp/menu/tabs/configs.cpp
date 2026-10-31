#include "menu/tabs/tabs.h"
#include "menu/style.h"
#include "menu/widgets.h"
#include "menu/menu.h"
#include "features/config/config.h"

#include <cstring>
#include <string>
#include <vector>

namespace xim::menu::tabs {

using namespace xim::features;
using namespace xim::menu::widget;

static char g_profile_name[64] = "default";
static std::vector<std::string> g_profiles_cache;
static int  g_selected_profile = 0;
static bool g_profiles_dirty = true;

static void refresh_local_profiles() {
    g_profiles_cache = config::list_profiles();
    if (g_profiles_cache.empty()) g_profiles_cache.push_back("default");
    if (g_selected_profile >= (int)g_profiles_cache.size())
        g_selected_profile = 0;
    g_profiles_dirty = false;
}

void draw_configs() {
    ImGui::TextColored(ImVec4(0.18f, 0.50f, 1.0f, 1.0f), "Configs");
    ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                       "Profiles and settings");
    ImGui::Separator();

    if (g_profiles_dirty) refresh_local_profiles();

    // ---- список профилей ----
    ImGui::BeginChild("##cfg_list", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        std::vector<const char*> names;
        for (auto& s : g_profiles_cache) names.push_back(s.c_str());

        if (ImGui::Combo("Profile", &g_selected_profile,
                         names.data(), (int)names.size())) {
            std::strncpy(g_profile_name,
                         g_profiles_cache[g_selected_profile].c_str(),
                         sizeof(g_profile_name) - 1);
            g_profile_name[sizeof(g_profile_name) - 1] = '\0';
        }

        ImGui::InputText("Name", g_profile_name, sizeof(g_profile_name));
        ImGui::Dummy(ImVec2(0, 8));

        // Save
        if (ImGui::Button("Save", ImVec2(100, 32))) {
            if (config::save(g_profile_name)) {
                g_profiles_dirty = true;
                config::save_last_used();
            }
        }
        ImGui::SameLine();
        // Load
        if (ImGui::Button("Load", ImVec2(100, 32))) {
            if (config::load(g_profile_name)) {
                config::save_last_used();
            }
        }
        ImGui::SameLine();
        // Delete
        if (ImGui::Button("Delete", ImVec2(100, 32))) {
            if (config::remove(g_profile_name)) {
                g_profiles_dirty = true;
            }
        }

        ImGui::Dummy(ImVec2(0, 8));
        if (ImGui::Button("Export to clipboard", ImVec2(200, 32))) {
            config::export_to_clipboard();
        }
        ImGui::SameLine();
        if (ImGui::Button("Import from clipboard", ImVec2(200, 32))) {
            config::import_from_clipboard();
        }

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                           "current: %s", config::current_profile().c_str());
    }
    ImGui::EndChild();

    // ---- UI settings ----
    ImGui::BeginChild("##ui", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        ImGui::TextUnformatted("Menu");
        ImGui::Separator();
        menu::State& st = menu::state();
        widget::slider_float("Menu Scale", &st.scale, 0.5f, 2.0f, "%.2f");

        const char* snap[] = { "Center", "Top Left", "Top Right",
                               "Bottom Left", "Bottom Right" };
        static int snap_idx = 0;
        if (widget::dropdown("Position", &snap_idx, snap, 5)) {
            ImGuiIO& io = ImGui::GetIO();
            float w = 900.0f * st.scale;
            float h = 620.0f * st.scale;
            switch (snap_idx) {
                case 0: st.pos_x = (int)((io.DisplaySize.x - w) * 0.5f);
                        st.pos_y = (int)((io.DisplaySize.y - h) * 0.5f); break;
                case 1: st.pos_x = 20; st.pos_y = 20; break;
                case 2: st.pos_x = (int)(io.DisplaySize.x - w - 20); st.pos_y = 20; break;
                case 3: st.pos_x = 20; st.pos_y = (int)(io.DisplaySize.y - h - 20); break;
                case 4: st.pos_x = (int)(io.DisplaySize.x - w - 20);
                        st.pos_y = (int)(io.DisplaySize.y - h - 20); break;
            }
        }
    }
    ImGui::EndChild();

    // ---- developer link ----
    ImGui::BeginChild("##dev", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        ImGui::TextUnformatted("Developer");
        ImGui::Separator();
        widget::link("@skeetcod", "https://t.me/skeetcod");
    }
    ImGui::EndChild();
}

} // namespace xim::menu::tabs