#include "menu/tabs/tabs.h"
#include "menu/style.h"
#include "menu/widgets.h"
#include "features/world_mod/world_mod.h"
#include "features/postprocess/postprocess.h"

namespace xim::menu::tabs {

using namespace xim::features;
using namespace xim::menu::widget;

static const char* kSkyboxModes[] = { "Solid", "Gradient", "Cubemap", "Custom" };
static const char* kFogModes[]    = { "Linear", "Exp", "Exp2" };
static const char* kLutPresets[]  = { "None", "Warm", "Cold", "Night", "Thermal", "NVG" };

void draw_world() {
    ImGui::TextColored(ImVec4(0.18f, 0.50f, 1.0f, 1.0f), "World");
    ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                       "Skybox, fog, ambient, post-processing");
    ImGui::Separator();

    world_mod::WorldCfg& w = world_mod::config();

    // ---- Skybox ----
    ImGui::BeginChild("##sky", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        ImGui::TextUnformatted("Skybox");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        toggle("##sky_en", &w.skybox.enabled);
        if (w.skybox.enabled) {
            ImGui::Separator();
            dropdown("Mode", &w.skybox.mode, kSkyboxModes, 4);
            color_edit("Top",     w.skybox.top);
            color_edit("Horizon", w.skybox.horiz);
            color_edit("Bottom",  w.skybox.bot);
            slider_float("Rotation",   &w.skybox.rotation,   0.0f, 360.0f, "%.0f");
            slider_float("Brightness", &w.skybox.brightness, 0.0f, 3.0f,   "%.2f");
        }
    }
    ImGui::EndChild();

    // ---- Fog ----
    ImGui::BeginChild("##fog", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        ImGui::TextUnformatted("Fog");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        toggle("##fog_en", &w.fog.enabled);
        if (w.fog.enabled) {
            ImGui::Separator();
            dropdown("Mode", &w.fog.mode, kFogModes, 3);
            slider_float("Density", &w.fog.density, 0.0f, 1.0f, "%.2f");
            if (w.fog.mode == 0) {
                slider_float("Start", &w.fog.start, 0.0f, 500.0f, "%.0f");
                slider_float("End",   &w.fog.end,  10.0f, 1000.0f, "%.0f");
            }
            color_edit("Color", w.fog.color);
        }
    }
    ImGui::EndChild();

    // ---- Ambient ----
    ImGui::BeginChild("##amb", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        ImGui::TextUnformatted("Ambient Light");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        toggle("##amb_en", &w.ambient.enabled);
        if (w.ambient.enabled) {
            ImGui::Separator();
            color_edit("Color", w.ambient.color);
            slider_float("Intensity", &w.ambient.intensity, 0.0f, 5.0f, "%.2f");
        }
    }
    ImGui::EndChild();

    // ---- Culling ----
    ImGui::BeginChild("##cul", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        ImGui::TextUnformatted("Render Culling");
        ImGui::Separator();
        toggle("Fullbright",   &w.culling.fullbright);
        toggle("Wireframe",    &w.culling.wireframe);
        toggle("No Shadows",   &w.culling.no_shadows);
        toggle("No Grass",     &w.culling.no_grass);
        toggle("No Particles", &w.culling.no_particles);
        toggle("No Smoke",     &w.culling.no_smoke);
        toggle("No Flash",     &w.culling.no_flash);
        toggle("No Hands",     &w.culling.no_hands);
    }
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0, 4));
    ImGui::TextColored(ImVec4(0.18f, 0.50f, 1.0f, 1.0f), "Post-processing");

    postprocess::Config& pp = postprocess::config();

    // ---- Bloom ----
    ImGui::BeginChild("##bloom", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = pp.bloom_enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("Bloom");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##bl_en", &en)) pp.bloom_enabled.store(en);
        if (en) {
            ImGui::Separator();
            float i = pp.bloom_intensity.load(std::memory_order_relaxed);
            float t = pp.bloom_threshold.load(std::memory_order_relaxed);
            float k = pp.bloom_knee.load(std::memory_order_relaxed);
            if (slider_float("Intensity", &i, 0.0f, 5.0f, "%.2f")) pp.bloom_intensity.store(i);
            if (slider_float("Threshold", &t, 0.0f, 2.0f, "%.2f")) pp.bloom_threshold.store(t);
            if (slider_float("Soft Knee", &k, 0.0f, 1.0f, "%.2f")) pp.bloom_knee.store(k);
        }
    }
    ImGui::EndChild();

    // ---- DoF ----
    ImGui::BeginChild("##dof", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = pp.dof_enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("Depth of Field");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##dof_en", &en)) pp.dof_enabled.store(en);
        if (en) {
            ImGui::Separator();
            float f  = pp.dof_focus.load(std::memory_order_relaxed);
            float a  = pp.dof_aperture.load(std::memory_order_relaxed);
            float fl = pp.dof_focal.load(std::memory_order_relaxed);
            if (slider_float("Focus", &f, 0.1f, 500.0f, "%.1f")) pp.dof_focus.store(f);
            if (slider_float("Aperture", &a, 0.5f, 22.0f, "%.1f")) pp.dof_aperture.store(a);
            if (slider_float("Focal Length", &fl, 10.0f, 300.0f, "%.0f")) pp.dof_focal.store(fl);
        }
    }
    ImGui::EndChild();

    // ---- LUT ----
    ImGui::BeginChild("##lut", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = pp.lut_enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("Color Grading");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##lut_en", &en)) pp.lut_enabled.store(en);
        if (en) {
            ImGui::Separator();
            int p = pp.lut_preset.load(std::memory_order_relaxed);
            if (dropdown("Preset", &p, kLutPresets, 6)) pp.lut_preset.store(p);
            float i = pp.lut_intensity.load(std::memory_order_relaxed);
            if (slider_float("Intensity", &i, 0.0f, 1.0f, "%.2f")) pp.lut_intensity.store(i);
        }
    }
    ImGui::EndChild();

    // ---- Vignette ----
    ImGui::BeginChild("##vig", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = pp.vignette_enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("Vignette");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##vig_en", &en)) pp.vignette_enabled.store(en);
        if (en) {
            ImGui::Separator();
            float i = pp.vignette_intensity.load(std::memory_order_relaxed);
            float s = pp.vignette_smooth.load(std::memory_order_relaxed);
            if (slider_float("Intensity", &i, 0.0f, 1.0f, "%.2f")) pp.vignette_intensity.store(i);
            if (slider_float("Smoothness", &s, 0.0f, 1.0f, "%.2f")) pp.vignette_smooth.store(s);
            color_edit("Color", reinterpret_cast<float*>(&pp.vignette_color));
        }
    }
    ImGui::EndChild();

    // ---- Chromatic ----
    ImGui::BeginChild("##chr", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = pp.chromatic_enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("Chromatic Aberration");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##chr_en", &en)) pp.chromatic_enabled.store(en);
        if (en) {
            ImGui::Separator();
            float i = pp.chromatic_intensity.load(std::memory_order_relaxed);
            if (slider_float("Intensity", &i, 0.0f, 1.0f, "%.2f")) pp.chromatic_intensity.store(i);
        }
    }
    ImGui::EndChild();

    // ---- Scanlines ----
    ImGui::BeginChild("##scn", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = pp.scanlines_enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("Scanlines");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##scn_en", &en)) pp.scanlines_enabled.store(en);
        if (en) {
            ImGui::Separator();
            float i = pp.scanlines_intensity.load(std::memory_order_relaxed);
            float d = pp.scanlines_density.load(std::memory_order_relaxed);
            if (slider_float("Intensity", &i, 0.0f, 1.0f, "%.2f")) pp.scanlines_intensity.store(i);
            if (slider_float("Density", &d, 1.0f, 8.0f, "%.1f")) pp.scanlines_density.store(d);
        }
    }
    ImGui::EndChild();

    // ---- Motion Blur ----
    ImGui::BeginChild("##mb", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = pp.motion_blur_enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("Motion Blur");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##mb_en", &en)) pp.motion_blur_enabled.store(en);
        if (en) {
            ImGui::Separator();
            float i = pp.motion_blur_intensity.load(std::memory_order_relaxed);
            if (slider_float("Intensity", &i, 0.0f, 1.0f, "%.2f")) pp.motion_blur_intensity.store(i);
        }
    }
    ImGui::EndChild();

    // ---- Grain ----
    ImGui::BeginChild("##grn", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = pp.grain_enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("Film Grain");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##grn_en", &en)) pp.grain_enabled.store(en);
        if (en) {
            ImGui::Separator();
            float i = pp.grain_intensity.load(std::memory_order_relaxed);
            if (slider_float("Intensity", &i, 0.0f, 1.0f, "%.2f")) pp.grain_intensity.store(i);
        }
    }
    ImGui::EndChild();
}

} // namespace xim::menu::tabs