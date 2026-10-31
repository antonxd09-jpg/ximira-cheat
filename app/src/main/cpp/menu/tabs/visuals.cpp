#include "menu/tabs/tabs.h"
#include "menu/style.h"
#include "menu/widgets.h"
#include "features/esp/config.h"
#include "features/chams/chams.h"
#include "features/glow/glow.h"

namespace xim::menu::tabs {

using namespace xim::features;
using namespace xim::menu::widget;

static const char* kBoxStyles[]     = { "2D", "3D", "Corner", "Rounded" };
static const char* kUnitNames[]     = { "Meters", "Feet" };
static const char* kHpStyles[]      = { "Bar", "Number", "Bar + Number" };
static const char* kOrientNames[]   = { "Horizontal", "Vertical" };
static const char* kWeaponPos[]     = { "Top", "Bottom" };
static const char* kChamsModes[]    = { "Off", "Flat", "Glow", "Wireframe", "Ghost", "Metallic", "Rainbow" };

void draw_visuals() {
    ImGui::TextColored(ImVec4(0.18f, 0.50f, 1.0f, 1.0f), "Visuals");
    ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                       "ESP, chams, glow, world modulation");
    ImGui::Separator();

    // ---------- ESP ----------
    esp::EspConfig& c = esp::config();

    ImGui::BeginChild("##esp_globals", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        toggle("Enable ESP", &c.enabled);
        if (c.enabled) {
            slider_float("Max Distance", &c.max_distance, 10.0f, 500.0f, "%.0fm");
            toggle("Team Check", &c.team_check);
        }
    }
    ImGui::EndChild();

    if (c.enabled) {
        ImGui::BeginChild("##box", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        {
            ImGui::TextUnformatted("Box");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
            toggle("##box_en", &c.box.enabled);
            if (c.box.enabled) {
                ImGui::Separator();
                dropdown("Style", &c.box.style, kBoxStyles, 4);
                slider_float("Thickness", &c.box.thickness, 0.5f, 5.0f, "%.1f");
                color_edit("Enemy Color", c.box.color_enemy);
                color_edit("Ally Color",  c.box.color_ally);
                toggle("Fill", &c.box.fill);
                if (c.box.fill)
                    slider_float("Fill Alpha", &c.box.fill_alpha, 0.0f, 1.0f, "%.2f");
            }
        }
        ImGui::EndChild();

        ImGui::BeginChild("##skel", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        {
            ImGui::TextUnformatted("Skeleton");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
            toggle("##skel_en", &c.skeleton.enabled);
            if (c.skeleton.enabled) {
                ImGui::Separator();
                slider_float("Thickness", &c.skeleton.thickness, 0.5f, 5.0f, "%.1f");
                color_edit("Enemy Color", c.skeleton.color_enemy);
                color_edit("Ally Color",  c.skeleton.color_ally);
                toggle("Head Circle", &c.skeleton.head_circle);
                toggle("Show Arms", &c.skeleton.show_arms);
                toggle("Show Legs", &c.skeleton.show_legs);
            }
        }
        ImGui::EndChild();

        ImGui::BeginChild("##name", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        {
            ImGui::TextUnformatted("Name");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
            toggle("##name_en", &c.name.enabled);
            if (c.name.enabled) {
                ImGui::Separator();
                slider_int("Font Size", &c.name.font_size, 8, 28);
                color_edit("Color", c.name.color);
                toggle("Background", &c.name.background);
                if (c.name.background)
                    slider_float("BG Alpha", &c.name.bg_alpha, 0.0f, 1.0f, "%.2f");
            }
        }
        ImGui::EndChild();

        ImGui::BeginChild("##dist", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        {
            ImGui::TextUnformatted("Distance");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
            toggle("##dist_en", &c.distance.enabled);
            if (c.distance.enabled) {
                ImGui::Separator();
                slider_int("Font Size", &c.distance.font_size, 8, 28);
                color_edit("Color", c.distance.color);
                dropdown("Unit", &c.distance.unit, kUnitNames, 2);
            }
        }
        ImGui::EndChild();

        ImGui::BeginChild("##hp", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        {
            ImGui::TextUnformatted("HP Bar");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
            toggle("##hp_en", &c.hp.enabled);
            if (c.hp.enabled) {
                ImGui::Separator();
                dropdown("Style", &c.hp.style, kHpStyles, 3);
                dropdown("Orientation", &c.hp.orientation, kOrientNames, 2);
                slider_float("Width", &c.hp.width, 1.0f, 10.0f, "%.1f");
                color_edit("High Color", c.hp.color_high);
                color_edit("Mid Color",  c.hp.color_mid);
                color_edit("Low Color",  c.hp.color_low);
                slider_int("Mid Threshold", &c.hp.threshold_mid, 0, 100);
                slider_int("Low Threshold", &c.hp.threshold_low, 0, 100);
                toggle("Show Number", &c.hp.show_number);
            }
        }
        ImGui::EndChild();

        ImGui::BeginChild("##armor", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        {
            ImGui::TextUnformatted("Armor Bar");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
            toggle("##armor_en", &c.armor.enabled);
            if (c.armor.enabled) {
                ImGui::Separator();
                slider_float("Width", &c.armor.width, 1.0f, 10.0f, "%.1f");
                color_edit("Color", c.armor.color);
            }
        }
        ImGui::EndChild();

        ImGui::BeginChild("##weapon", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        {
            ImGui::TextUnformatted("Weapon");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
            toggle("##weapon_en", &c.weapon.enabled);
            if (c.weapon.enabled) {
                ImGui::Separator();
                toggle("Show Ammo", &c.weapon.show_ammo);
                slider_int("Font Size", &c.weapon.font_size, 8, 24);
                color_edit("Color", c.weapon.color);
                dropdown("Position", &c.weapon.position, kWeaponPos, 2);
            }
        }
        ImGui::EndChild();
    }

    // ---------- Chams ----------
    chams::Config& ch = chams::config();

    ImGui::BeginChild("##chams", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        ImGui::TextUnformatted("Chams");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        bool en = ch.enabled.load(std::memory_order_relaxed);
        if (toggle("##ch_en", &en)) ch.enabled.store(en);

        if (en) {
            ImGui::Separator();
            int mode = ch.mode.load(std::memory_order_relaxed);
            if (dropdown("Mode", &mode, kChamsModes, 7)) ch.mode.store(mode);

            bool teams = ch.teamsSeparate.load(std::memory_order_relaxed);
            if (toggle("Team Colors", &teams)) ch.teamsSeparate.store(teams);

            bool walls = ch.throughWalls.load(std::memory_order_relaxed);
            if (toggle("Through Walls", &walls)) ch.throughWalls.store(walls);

            if (mode == (int)chams::Mode::Rainbow) {
                float f = ch.rainbowSpeed.load(std::memory_order_relaxed);
                if (slider_float("Rainbow Speed", &f, 0.05f, 3.0f, "%.2f")) ch.rainbowSpeed.store(f);
            }
            if (mode == (int)chams::Mode::Ghost) {
                float f = ch.ghostAlpha.load(std::memory_order_relaxed);
                if (slider_float("Ghost Alpha", &f, 0.05f, 1.0f, "%.2f")) ch.ghostAlpha.store(f);
            }
            if (mode == (int)chams::Mode::Metallic) {
                float r = ch.metallicRoughness.load(std::memory_order_relaxed);
                float m = ch.metallicMetal.load(std::memory_order_relaxed);
                if (slider_float("Roughness", &r, 0.0f, 1.0f, "%.2f")) ch.metallicRoughness.store(r);
                if (slider_float("Metalness", &m, 0.0f, 1.0f, "%.2f")) ch.metallicMetal.store(m);
            }
            if (mode == (int)chams::Mode::Glow) {
                float g = ch.glowIntensity.load(std::memory_order_relaxed);
                if (slider_float("Glow Intensity", &g, 0.5f, 5.0f, "%.2f")) ch.glowIntensity.store(g);
            }

            color_edit("Enemy Color", ch.colors.enemy);
            color_edit("Ally Color",  ch.colors.ally);
            color_edit("Local Color", ch.colors.local);
        }
    }
    ImGui::EndChild();

    // ---------- Glow ----------
    glow::Config& gl = glow::config();

    ImGui::BeginChild("##glow", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        ImGui::TextUnformatted("Glow Outline");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        bool en = gl.enabled.load(std::memory_order_relaxed);
        if (toggle("##gl_en", &en)) gl.enabled.store(en);

        if (en) {
            ImGui::Separator();
            float th = gl.thickness.load(std::memory_order_relaxed);
            if (slider_float("Thickness", &th, 0.5f, 6.0f, "%.1f")) gl.thickness.store(th);

            float it = gl.intensity.load(std::memory_order_relaxed);
            if (slider_float("Intensity", &it, 0.5f, 4.0f, "%.2f")) gl.intensity.store(it);

            float md = gl.maxDistance.load(std::memory_order_relaxed);
            if (slider_float("Max Distance", &md, 10.0f, 500.0f, "%.0fm")) gl.maxDistance.store(md);

            bool pulse = gl.pulse.load(std::memory_order_relaxed);
            if (toggle("Pulse", &pulse)) gl.pulse.store(pulse);
            if (pulse) {
                float ps = gl.pulseSpeed.load(std::memory_order_relaxed);
                if (slider_float("Pulse Speed", &ps, 0.2f, 4.0f, "%.2f")) gl.pulseSpeed.store(ps);
            }

            bool tc = gl.teamColors.load(std::memory_order_relaxed);
            if (toggle("Team Colors", &tc)) gl.teamColors.store(tc);

            bool ds = gl.drawSkeleton.load(std::memory_order_relaxed);
            if (toggle("Skeleton", &ds)) gl.drawSkeleton.store(ds);

            bool db = gl.drawBox.load(std::memory_order_relaxed);
            if (toggle("Box", &db)) gl.drawBox.store(db);

            color_edit("Enemy Color", gl.colors.enemy);
            color_edit("Ally Color",  gl.colors.ally);
            color_edit("Local Color", gl.colors.local);
        }
    }
    ImGui::EndChild();
}

} // namespace xim::menu::tabs