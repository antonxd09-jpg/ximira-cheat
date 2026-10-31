#include "menu/tabs/tabs.h"
#include "menu/style.h"
#include "menu/widgets.h"
#include "features/hitsound/hitsound.h"

#include <string>
#include <vector>

namespace xim::menu::tabs {

using namespace xim::features;
using namespace xim::menu::widget;

static const char* kSoundSlots[] = { "Hit", "Headshot", "Kill" };

// текущий выбранный слот в UI
static int g_current_slot = 0;

// имена файлов без пути — для дропдауна
static std::vector<std::string> g_display_names;
static std::vector<std::string> g_full_paths;
static int g_selected_index = -1;
static bool g_dropdown_open_prev = false;

void draw_misc() {
    ImGui::TextColored(ImVec4(0.18f, 0.50f, 1.0f, 1.0f), "Misc");
    ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                       "Hit sound, indicators, watermark");
    ImGui::Separator();

    hitsound::Config& hs = hitsound::config();

    // ---- Hit Sound ----
    ImGui::BeginChild("##hs", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        bool en = hs.enabled.load(std::memory_order_relaxed);
        ImGui::TextUnformatted("Hit Sound");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (toggle("##hs_en", &en)) hs.enabled.store(en);

        if (en) {
            ImGui::Separator();

            // папка
            ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f), "Folder:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.86f, 0.89f, 0.94f, 1.0f),
                               "%s", hs.folder.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Open"))
                jni_open_path(hs.folder.c_str());

            // master volume
            float mv = hs.master_volume.load(std::memory_order_relaxed);
            if (slider_float("Master Volume", &mv, 0.0f, 1.0f, "%.2f"))
                hs.master_volume.store(mv);

            int cd = hs.cooldown_ms.load(std::memory_order_relaxed);
            if (slider_int("Cooldown (ms)", &cd, 10, 500))
                hs.cooldown_ms.store(cd);

            // слот
            if (dropdown("Slot", &g_current_slot, kSoundSlots, 3)) {
                // при смене слота — пересобрать список файлов + выбрать текущий
                hitsound::rescan_folder();
                g_display_names.clear();
                g_full_paths.clear();
                g_selected_index = -1;
                for (const auto& p : hitsound::files()) {
                    size_t slash = p.find_last_of('/');
                    g_display_names.push_back(slash == std::string::npos ? p : p.substr(slash + 1));
                    g_full_paths.push_back(p);
                }
                // подсветить текущий выбранный
                const std::string& cur = hs.slots[g_current_slot].file;
                for (size_t i = 0; i < g_full_paths.size(); ++i) {
                    if (g_full_paths[i] == cur) { g_selected_index = (int)i; break; }
                }
            }

            // статус файла в слоте
            ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f), "File:");
            ImGui::SameLine();
            const std::string& file = hs.slots[g_current_slot].file;
            ImGui::TextColored(ImVec4(0.86f, 0.89f, 0.94f, 1.0f),
                               "%s", file.empty() ? "(none)" : file.c_str());

            // дропдаун файлов
            if (!g_display_names.empty()) {
                std::vector<const char*> cstr;
                cstr.reserve(g_display_names.size());
                for (auto& s : g_display_names) cstr.push_back(s.c_str());

                if (ImGui::Combo("Sound file", &g_selected_index,
                                 cstr.data(), (int)cstr.size())) {
                    if (g_selected_index >= 0 &&
                        g_selected_index < (int)g_full_paths.size()) {
                        hitsound::set_slot_file(g_current_slot, g_full_paths[g_selected_index]);
                    }
                }
            } else {
                hint("put .wav / .mp3 / .ogg into the folder, then reopen this tab");
            }

            // per-slot enable + volume
            bool slot_en = hs.slots[g_current_slot].enabled.load(std::memory_order_relaxed);
            if (toggle("Slot Enabled", &slot_en))
                hs.slots[g_current_slot].enabled.store(slot_en);

            float sv = hs.slots[g_current_slot].volume.load(std::memory_order_relaxed);
            if (slider_float("Slot Volume", &sv, 0.0f, 1.0f, "%.2f"))
                hs.slots[g_current_slot].volume.store(sv);

            if (ImGui::Button("Preview", ImVec2(120, 30))) {
                hitsound::play_slot(g_current_slot);
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f),
                               "played %llu times",
                               (unsigned long long)hitsound::play_count(g_current_slot));
        }
    }
    ImGui::EndChild();
}

} // namespace xim::menu::tabs