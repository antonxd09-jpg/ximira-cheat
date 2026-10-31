#include "features/config/config.h"
#include "features/chams/chams.h"
#include "features/glow/glow.h"
#include "features/world_mod/world_mod.h"
#include "features/camera_mod/camera_mod.h"
#include "features/postprocess/postprocess.h"
#include "features/hitsound/hitsound.h"
#include "features/esp/config.h"
#include "util/log.h"

#include <nlohmann/json.hpp>

#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <mutex>

#include <jni.h>

using json = nlohmann::json;

namespace xim::features::config {

static const std::string kFolder = "/storage/emulated/0/Ximira/Configs/";
static const std::string kLastFile = "/storage/emulated/0/Ximira/last_profile.txt";

static std::string g_current = "default";
static std::vector<std::string> g_profiles;
static std::mutex g_mutex;

// ---- JNI (NativeBridge.setClipboard(String), getClipboard() → String) ----
extern JavaVM* g_vm;

static bool jni_set_clipboard(const std::string& text) {
    if (!g_vm) return false;
    JNIEnv* env = nullptr; bool attached = false;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
        attached = true;
    }
    bool ok = false;
    jclass cls = env->FindClass("com/ximira/loader/NativeBridge");
    if (cls) {
        jmethodID m = env->GetStaticMethodID(cls, "setClipboard", "(Ljava/lang/String;)V");
        if (m) {
            jstring s = env->NewStringUTF(text.c_str());
            env->CallStaticVoidMethod(cls, m, s);
            env->DeleteLocalRef(s);
            ok = true;
        }
    }
    if (attached) g_vm->DetachCurrentThread();
    return ok;
}

static std::string jni_get_clipboard() {
    if (!g_vm) return {};
    JNIEnv* env = nullptr; bool attached = false;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return {};
        attached = true;
    }
    std::string result;
    jclass cls = env->FindClass("com/ximira/loader/NativeBridge");
    if (cls) {
        jmethodID m = env->GetStaticMethodID(cls, "getClipboard", "()Ljava/lang/String;");
        if (m) {
            jstring js = (jstring)env->CallStaticObjectMethod(cls, m);
            if (js) {
                const char* raw = env->GetStringUTFChars(js, nullptr);
                if (raw) { result = raw; env->ReleaseStringUTFChars(js, raw); }
                env->DeleteLocalRef(js);
            }
        }
    }
    if (attached) g_vm->DetachCurrentThread();
    return result;
}

// ---- filesystem ----
static void ensure_dir(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur.push_back(path[i]);
        if (path[i] == '/' && i > 0 && i + 1 < path.size())
            mkdir(cur.c_str(), 0755);
    }
    mkdir(path.c_str(), 0755);
}

static bool has_json_ext(const char* n) {
    size_t len = strlen(n);
    return len > 5 && strcmp(n + len - 5, ".json") == 0;
}

void refresh_profiles() {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_profiles.clear();

    DIR* d = opendir(kFolder.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] == '.') continue;
        if (!has_json_ext(e->d_name)) continue;
        std::string name(e->d_name);
        name.resize(name.size() - 5);   // без .json
        g_profiles.push_back(name);
    }
    closedir(d);
    std::sort(g_profiles.begin(), g_profiles.end());
}

std::vector<std::string> list_profiles() {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_profiles;
}

const std::string& current_profile() { return g_current; }
void set_current_profile(const std::string& n) { g_current = n; }

// ---- serialize / deserialize ----
// собираем json из всех модулей
static json serialize() {
    json j;

    // ---- chams ----
    {
        auto& c = chams::config();
        j["chams"] = {
            {"enabled",         c.enabled.load()},
            {"mode",            c.mode.load()},
            {"teamsSeparate",   c.teamsSeparate.load()},
            {"throughWalls",    c.throughWalls.load()},
            {"glowPulse",       c.glowPulse.load()},
            {"pulseSpeed",      c.pulseSpeed.load()},
            {"rainbowSpeed",    c.rainbowSpeed.load()},
            {"ghostAlpha",      c.ghostAlpha.load()},
            {"metallicRough",   c.metallicRoughness.load()},
            {"metallicMetal",   c.metallicMetal.load()},
            {"glowIntensity",   c.glowIntensity.load()},
            {"color_enemy",  {c.colors.enemy[0], c.colors.enemy[1], c.colors.enemy[2], c.colors.enemy[3]}},
            {"color_ally",   {c.colors.ally [0], c.colors.ally [1], c.colors.ally [2], c.colors.ally [3]}},
            {"color_local",  {c.colors.local[0], c.colors.local[1], c.colors.local[2], c.colors.local[3]}},
            {"color_ragdoll",{c.colors.ragdoll[0], c.colors.ragdoll[1], c.colors.ragdoll[2], c.colors.ragdoll[3]}},
        };
    }

    // ---- glow ----
    {
        auto& c = glow::config();
        j["glow"] = {
            {"enabled",       c.enabled.load()},
            {"thickness",     c.thickness.load()},
            {"intensity",     c.intensity.load()},
            {"pulse",         c.pulse.load()},
            {"pulseSpeed",    c.pulseSpeed.load()},
            {"teamColors",    c.teamColors.load()},
            {"drawSkeleton",  c.drawSkeleton.load()},
            {"drawBox",       c.drawBox.load()},
            {"maxDistance",   c.maxDistance.load()},
            {"color_enemy", {c.colors.enemy[0], c.colors.enemy[1], c.colors.enemy[2], c.colors.enemy[3]}},
            {"color_ally",  {c.colors.ally [0], c.colors.ally [1], c.colors.ally [2], c.colors.ally [3]}},
            {"color_local", {c.colors.local[0], c.colors.local[1], c.colors.local[2], c.colors.local[3]}},
        };
    }

    // ---- world_mod ----
    {
        auto& c = world_mod::config();
        j["world"] = {
            {"skybox_en", c.skybox.enabled}, {"skybox_mode", c.skybox.mode},
            {"skybox_bright", c.skybox.brightness}, {"skybox_rot", c.skybox.rotation},
            {"fog_en", c.fog.enabled}, {"fog_mode", c.fog.mode},
            {"fog_density", c.fog.density}, {"fog_start", c.fog.start}, {"fog_end", c.fog.end},
            {"fog_color", {c.fog.color[0], c.fog.color[1], c.fog.color[2], c.fog.color[3]}},
            {"amb_en", c.ambient.enabled}, {"amb_int", c.ambient.intensity},
            {"amb_color", {c.ambient.color[0], c.ambient.color[1], c.ambient.color[2], c.ambient.color[3]}},
            {"no_shadows", c.culling.no_shadows},
            {"no_grass",   c.culling.no_grass},
            {"no_particles", c.culling.no_particles},
            {"no_smoke",   c.culling.no_smoke},
            {"no_flash",   c.culling.no_flash},
            {"no_hands",   c.culling.no_hands},
            {"fullbright", c.culling.fullbright},
            {"wireframe",  c.culling.wireframe},
        };
    }

    // ---- camera_mod ----
    {
        auto& c = camera_mod::config();
        j["camera"] = {
            {"fov_en",   c.fov_enabled.load()},
            {"fov_val",  c.fov_value.load()},
            {"tp_en",    c.tp_enabled.load()},
            {"tp_dist",  c.tp_distance.load()},
            {"tp_height",c.tp_height.load()},
            {"tp_side",  c.tp_side.load()},
            {"no_shake", c.no_shake.load()},
        };
    }

    // ---- postprocess ----
    {
        auto& c = postprocess::config();
        j["pp"] = {
            {"bloom_en", c.bloom_enabled.load()}, {"bloom_i", c.bloom_intensity.load()},
            {"bloom_t", c.bloom_threshold.load()}, {"bloom_k", c.bloom_knee.load()},
            {"dof_en", c.dof_enabled.load()}, {"dof_f", c.dof_focus.load()},
            {"dof_a", c.dof_aperture.load()}, {"dof_fl", c.dof_focal.load()},
            {"lut_en", c.lut_enabled.load()}, {"lut_p", c.lut_preset.load()}, {"lut_i", c.lut_intensity.load()},
            {"vig_en", c.vignette_enabled.load()}, {"vig_i", c.vignette_intensity.load()}, {"vig_s", c.vignette_smooth.load()},
            {"chr_en", c.chromatic_enabled.load()}, {"chr_i", c.chromatic_intensity.load()},
            {"scn_en", c.scanlines_enabled.load()}, {"scn_i", c.scanlines_intensity.load()}, {"scn_d", c.scanlines_density.load()},
            {"mb_en", c.motion_blur_enabled.load()}, {"mb_i", c.motion_blur_intensity.load()}, {"mb_s", c.motion_blur_samples.load()},
            {"gr_en", c.grain_enabled.load()}, {"gr_i", c.grain_intensity.load()},
        };
    }

    // ---- hitsound ----
    {
        auto& c = hitsound::config();
        j["hitsound"] = {
            {"enabled", c.enabled.load()},
            {"master_volume", c.master_volume.load()},
            {"cooldown_ms", c.cooldown_ms.load()},
            {"folder", c.folder},
            {"slots", json::array({
                {{"en", c.slots[0].enabled.load()}, {"vol", c.slots[0].volume.load()}, {"file", c.slots[0].file}},
                {{"en", c.slots[1].enabled.load()}, {"vol", c.slots[1].volume.load()}, {"file", c.slots[1].file}},
                {{"en", c.slots[2].enabled.load()}, {"vol", c.slots[2].volume.load()}, {"file", c.slots[2].file}},
            })},
        };
    }

    // ---- esp ----
    {
        auto& c = esp::config();
        j["esp"] = {
            {"enabled", c.enabled},
            {"max_distance", c.max_distance},
            {"team_check", c.team_check},
            {"box_en", c.box.enabled}, {"box_style", c.box.style}, {"box_thick", c.box.thickness},
            {"box_fill", c.box.fill}, {"box_fill_a", c.box.fill_alpha},
            {"box_en_col", {c.box.color_enemy[0], c.box.color_enemy[1], c.box.color_enemy[2], c.box.color_enemy[3]}},
            {"box_al_col", {c.box.color_ally[0],  c.box.color_ally[1],  c.box.color_ally[2],  c.box.color_ally[3]}},
            {"skel_en", c.skeleton.enabled}, {"skel_thick", c.skeleton.thickness},
            {"skel_head", c.skeleton.head_circle}, {"skel_arms", c.skeleton.show_arms}, {"skel_legs", c.skeleton.show_legs},
            {"name_en", c.name.enabled}, {"name_size", c.name.font_size}, {"name_bg", c.name.background}, {"name_bg_a", c.name.bg_alpha},
            {"dist_en", c.distance.enabled}, {"dist_unit", c.distance.unit},
            {"hp_en", c.hp.enabled}, {"hp_style", c.hp.style}, {"hp_orient", c.hp.orientation}, {"hp_w", c.hp.width},
            {"hp_mid", c.hp.threshold_mid}, {"hp_low", c.hp.threshold_low}, {"hp_num", c.hp.show_number},
            {"armor_en", c.armor.enabled}, {"armor_w", c.armor.width},
            {"weap_en", c.weapon.enabled}, {"weap_ammo", c.weapon.show_ammo}, {"weap_pos", c.weapon.position},
        };
    }

    return j;
}

// хелпер: достать из json безопасно
template <typename T>
static void get(const json& j, const char* key, T& out) {
    if (j.contains(key) && !j[key].is_null()) {
        try { out = j[key].get<T>(); } catch (...) {}
    }
}

static void get_arr4(const json& j, const char* key, float out[4]) {
    if (j.contains(key) && j[key].is_array() && j[key].size() == 4) {
        for (int i = 0; i < 4; ++i) out[i] = j[key][i].get<float>();
    }
}

// десериализация — пишем всё в конфиги модулей
static void deserialize(const json& j) {
    // chams
    if (j.contains("chams")) {
        auto& c = chams::config();
        auto& s = j["chams"];
        bool b; int i; float f;
        get(s, "enabled", b);       c.enabled.store(b);
        get(s, "mode", i);          c.mode.store(i);
        get(s, "teamsSeparate", b); c.teamsSeparate.store(b);
        get(s, "throughWalls", b);  c.throughWalls.store(b);
        get(s, "glowPulse", b);     c.glowPulse.store(b);
        get(s, "pulseSpeed", f);    c.pulseSpeed.store(f);
        get(s, "rainbowSpeed", f);  c.rainbowSpeed.store(f);
        get(s, "ghostAlpha", f);    c.ghostAlpha.store(f);
        get(s, "metallicRough", f); c.metallicRoughness.store(f);
        get(s, "metallicMetal", f); c.metallicMetal.store(f);
        get(s, "glowIntensity", f); c.glowIntensity.store(f);
        get_arr4(s, "color_enemy",  c.colors.enemy);
        get_arr4(s, "color_ally",   c.colors.ally);
        get_arr4(s, "color_local",  c.colors.local);
        get_arr4(s, "color_ragdoll",c.colors.ragdoll);
    }

    // glow
    if (j.contains("glow")) {
        auto& c = glow::config();
        auto& s = j["glow"];
        bool b; float f;
        get(s, "enabled", b);      c.enabled.store(b);
        get(s, "thickness", f);    c.thickness.store(f);
        get(s, "intensity", f);    c.intensity.store(f);
        get(s, "pulse", b);        c.pulse.store(b);
        get(s, "pulseSpeed", f);   c.pulseSpeed.store(f);
        get(s, "teamColors", b);   c.teamColors.store(b);
        get(s, "drawSkeleton", b); c.drawSkeleton.store(b);
        get(s, "drawBox", b);      c.drawBox.store(b);
        get(s, "maxDistance", f);  c.maxDistance.store(f);
        get_arr4(s, "color_enemy", c.colors.enemy);
        get_arr4(s, "color_ally",  c.colors.ally);
        get_arr4(s, "color_local", c.colors.local);
    }

    // world_mod
    if (j.contains("world")) {
        auto& c = world_mod::config();
        auto& s = j["world"];
        get(s, "skybox_en",     c.skybox.enabled);
        get(s, "skybox_mode",   c.skybox.mode);
        get(s, "skybox_bright", c.skybox.brightness);
        get(s, "skybox_rot",    c.skybox.rotation);
        get(s, "fog_en",        c.fog.enabled);
        get(s, "fog_mode",      c.fog.mode);
        get(s, "fog_density",   c.fog.density);
        get(s, "fog_start",     c.fog.start);
        get(s, "fog_end",       c.fog.end);
        get_arr4(s, "fog_color", c.fog.color);
        get(s, "amb_en",        c.ambient.enabled);
        get(s, "amb_int",       c.ambient.intensity);
        get_arr4(s, "amb_color", c.ambient.color);
        get(s, "no_shadows",    c.culling.no_shadows);
        get(s, "no_grass",      c.culling.no_grass);
        get(s, "no_particles",  c.culling.no_particles);
        get(s, "no_smoke",      c.culling.no_smoke);
        get(s, "no_flash",      c.culling.no_flash);
        get(s, "no_hands",      c.culling.no_hands);
        get(s, "fullbright",    c.culling.fullbright);
        get(s, "wireframe",     c.culling.wireframe);
    }

    // camera
    if (j.contains("camera")) {
        auto& c = camera_mod::config();
        auto& s = j["camera"];
        bool b; float f;
        get(s, "fov_en", b);    c.fov_enabled.store(b);
        get(s, "fov_val", f);   c.fov_value.store(f);
        get(s, "tp_en", b);     c.tp_enabled.store(b);
        get(s, "tp_dist", f);   c.tp_distance.store(f);
        get(s, "tp_height", f); c.tp_height.store(f);
        get(s, "tp_side", f);   c.tp_side.store(f);
        get(s, "no_shake", b);  c.no_shake.store(b);
    }

    // postprocess
    if (j.contains("pp")) {
        auto& c = postprocess::config();
        auto& s = j["pp"];
        bool b; int i; float f;
        get(s, "bloom_en", b); c.bloom_enabled.store(b);
        get(s, "bloom_i", f);  c.bloom_intensity.store(f);
        get(s, "bloom_t", f);  c.bloom_threshold.store(f);
        get(s, "bloom_k", f);  c.bloom_knee.store(f);
        get(s, "dof_en", b);   c.dof_enabled.store(b);
        get(s, "dof_f", f);    c.dof_focus.store(f);
        get(s, "dof_a", f);    c.dof_aperture.store(f);
        get(s, "dof_fl", f);   c.dof_focal.store(f);
        get(s, "lut_en", b);   c.lut_enabled.store(b);
        get(s, "lut_p", i);    c.lut_preset.store(i);
        get(s, "lut_i", f);    c.lut_intensity.store(f);
        get(s, "vig_en", b);   c.vignette_enabled.store(b);
        get(s, "vig_i", f);    c.vignette_intensity.store(f);
        get(s, "vig_s", f);    c.vignette_smooth.store(f);
        get(s, "chr_en", b);   c.chromatic_enabled.store(b);
        get(s, "chr_i", f);    c.chromatic_intensity.store(f);
        get(s, "scn_en", b);   c.scanlines_enabled.store(b);
        get(s, "scn_i", f);    c.scanlines_intensity.store(f);
        get(s, "scn_d", f);    c.scanlines_density.store(f);
        get(s, "mb_en", b);    c.motion_blur_enabled.store(b);
        get(s, "mb_i", f);     c.motion_blur_intensity.store(f);
        get(s, "mb_s", i);     c.motion_blur_samples.store(i);
        get(s, "gr_en", b);    c.grain_enabled.store(b);
        get(s, "gr_i", f);     c.grain_intensity.store(f);
    }

    // hitsound
    if (j.contains("hitsound")) {
        auto& c = hitsound::config();
        auto& s = j["hitsound"];
        bool b; float f; int i;
        get(s, "enabled", b); c.enabled.store(b);
        get(s, "master_volume", f); c.master_volume.store(f);
        get(s, "cooldown_ms", i); c.cooldown_ms.store(i);
        if (s.contains("folder") && s["folder"].is_string()) c.folder = s["folder"].get<std::string>();
        if (s.contains("slots") && s["slots"].is_array() && s["slots"].size() == 3) {
            for (int k = 0; k < 3; ++k) {
                auto& sl = s["slots"][k];
                bool e; float v;
                get(sl, "en", e); c.slots[k].enabled.store(e);
                get(sl, "vol", v); c.slots[k].volume.store(v);
                if (sl.contains("file") && sl["file"].is_string())
                    c.slots[k].file = sl["file"].get<std::string>();
            }
        }
    }

    // esp
    if (j.contains("esp")) {
        auto& c = esp::config();
        auto& s = j["esp"];
        bool b; int i; float f;
        get(s, "enabled", b); c.enabled = b;
        get(s, "max_distance", f); c.max_distance = f;
        get(s, "team_check", b); c.team_check = b;
        get(s, "box_en", b);    c.box.enabled = b;
        get(s, "box_style", i); c.box.style = i;
        get(s, "box_thick", f); c.box.thickness = f;
        get(s, "box_fill", b);  c.box.fill = b;
        get(s, "box_fill_a", f); c.box.fill_alpha = f;
        get_arr4(s, "box_en_col", c.box.color_enemy);
        get_arr4(s, "box_al_col", c.box.color_ally);
        get(s, "skel_en", b);    c.skeleton.enabled = b;
        get(s, "skel_thick", f); c.skeleton.thickness = f;
        get(s, "skel_head", b);  c.skeleton.head_circle = b;
        get(s, "skel_arms", b);  c.skeleton.show_arms = b;
        get(s, "skel_legs", b);  c.skeleton.show_legs = b;
        get(s, "name_en", b);    c.name.enabled = b;
        get(s, "name_size", i);  c.name.font_size = i;
        get(s, "name_bg", b);    c.name.background = b;
        get(s, "name_bg_a", f);  c.name.bg_alpha = f;
        get(s, "dist_en", b);    c.distance.enabled = b;
        get(s, "dist_unit", i);  c.distance.unit = i;
        get(s, "hp_en", b);      c.hp.enabled = b;
        get(s, "hp_style", i);   c.hp.style = i;
        get(s, "hp_orient", i);  c.hp.orientation = i;
        get(s, "hp_w", f);       c.hp.width = f;
        get(s, "hp_mid", i);     c.hp.threshold_mid = i;
        get(s, "hp_low", i);     c.hp.threshold_low = i;
        get(s, "hp_num", b);     c.hp.show_number = b;
        get(s, "armor_en", b);   c.armor.enabled = b;
        get(s, "armor_w", f);    c.armor.width = f;
        get(s, "weap_en", b);    c.weapon.enabled = b;
        get(s, "weap_ammo", b);  c.weapon.show_ammo = b;
        get(s, "weap_pos", i);   c.weapon.position = i;
    }
}

// ---- save / load / remove ----
static std::string profile_path(const std::string& name) {
    return kFolder + name + ".json";
}

bool save(const std::string& name) {
    ensure_dir(kFolder);
    std::ofstream f(profile_path(name));
    if (!f.is_open()) {
        XERR("config: cannot write %s", profile_path(name).c_str());
        return false;
    }
    json j = serialize();
    f << j.dump(2);
    f.close();
    g_current = name;
    refresh_profiles();
    XLOG("config: saved %s", name.c_str());
    return true;
}

bool load(const std::string& name) {
    std::ifstream f(profile_path(name));
    if (!f.is_open()) {
        XERR("config: cannot read %s", profile_path(name).c_str());
        return false;
    }
    try {
        json j;
        f >> j;
        deserialize(j);
        g_current = name;
        XLOG("config: loaded %s", name.c_str());
        return true;
    } catch (const std::exception& e) {
        XERR("config: parse error %s: %s", name.c_str(), e.what());
        return false;
    }
}

bool remove(const std::string& name) {
    if (::remove(profile_path(name).c_str()) == 0) {
        refresh_profiles();
        XLOG("config: removed %s", name.c_str());
        return true;
    }
    return false;
}

// ---- clipboard ----
bool export_to_clipboard() {
    json j = serialize();
    std::string text = j.dump();
    return jni_set_clipboard(text);
}

bool import_from_clipboard() {
    std::string text = jni_get_clipboard();
    if (text.empty()) return false;
    try {
        json j = json::parse(text);
        deserialize(j);
        XLOG("config: imported from clipboard");
        return true;
    } catch (const std::exception& e) {
        XERR("config: clipboard parse error: %s", e.what());
        return false;
    }
}

// ---- last used ----
bool save_last_used() {
    ensure_dir("/storage/emulated/0/Ximira/");
    std::ofstream f(kLastFile);
    if (!f.is_open()) return false;
    f << g_current;
    return true;
}

bool load_last_used() {
    std::ifstream f(kLastFile);
    if (!f.is_open()) return false;
    std::string name;
    std::getline(f, name);
    if (name.empty()) return false;
    return load(name);
}

// ---- init ----
bool init() {
    ensure_dir(kFolder);
    ensure_dir("/storage/emulated/0/Ximira/");
    refresh_profiles();
    XLOG("config: init ok folder=%s profiles=%zu", kFolder.c_str(), g_profiles.size());
    return true;
}

} // namespace xim::features::config