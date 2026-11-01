#include "features/hitsound/hitsound.h"
#include "world/snapshot.h"
#include "util/log.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>

#include <jni.h>

extern JavaVM* g_vm;
namespace xim::features::hitsound {

// ---- config ----
static Config g_cfg;
Config& config() { return g_cfg; }

// ---- файлы ----
static std::vector<std::string> g_files;
static std::mutex g_files_mutex;
const std::vector<std::string>& files() { return g_files; }

// ---- JNI ----
// NativeBridge.playSound(String path, float volume) → создаёт MediaPlayer на java-стороне
// NativeBridge.stopSound()                          → останавливает и освобождает
// NativeBridge.ensureFolder(String path)            → создаёт папку
   // определён в ximira.cpp

static JavaVM* vm() { return g_vm; }

static bool call_java_play(const std::string& path, float vol) {
    JavaVM* jvm = vm();
    if (!jvm) return false;

    JNIEnv* env = nullptr;
    bool attached = false;
    if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
        attached = true;
    }

    bool ok = false;
    jclass cls = env->FindClass("com/ximira/loader/NativeBridge");
    if (cls) {
        jmethodID m = env->GetStaticMethodID(cls, "playSound", "(Ljava/lang/String;F)V");
        if (m) {
            jstring js = env->NewStringUTF(path.c_str());
            env->CallStaticVoidMethod(cls, m, js, (jfloat)vol);
            env->DeleteLocalRef(js);
            ok = true;
        }
    }
    if (attached) jvm->DetachCurrentThread();
    return ok;
}

// ---- создание папки (POSIX mkdir -p) ----
static void ensure_folder(const std::string& path) {
    if (path.empty()) return;
    std::string dir;
    dir.reserve(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        dir.push_back(path[i]);
        if (path[i] == '/' && i > 0 && i + 1 < path.size()) {
            // создаём промежуточный компонент
            mkdir(dir.c_str(), 0755);
        }
    }
    mkdir(path.c_str(), 0755);
}

// ---- расширение файла ----
static bool has_ext(const char* name, const char* ext) {
    size_t n = strlen(name);
    size_t e = strlen(ext);
    if (n < e) return false;
    for (size_t i = 0; i < e; ++i)
        if (tolower(name[n - e + i]) != tolower(ext[i])) return false;
    return true;
}

static bool is_sound(const char* name) {
    return has_ext(name, ".wav") || has_ext(name, ".mp3") || has_ext(name, ".ogg");
}

// ---- сканирование ----
void rescan_folder() {
    std::lock_guard<std::mutex> lk(g_files_mutex);
    g_files.clear();

    DIR* d = opendir(g_cfg.folder.c_str());
    if (!d) {
        XLOG("hitsound: folder not found: %s", g_cfg.folder.c_str());
        return;
    }

    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] == '.') continue;
        if (e->d_type != DT_REG && e->d_type != DT_UNKNOWN) continue;
        if (!is_sound(e->d_name)) continue;

        std::string full = g_cfg.folder + e->d_name;
        // фильтр на читаемость
        struct stat st{};
        if (stat(full.c_str(), &st) != 0 || st.st_size < 32) continue;

        g_files.push_back(full);
    }
    closedir(d);

    std::sort(g_files.begin(), g_files.end());
    XLOG("hitsound: found %zu files in %s", g_files.size(), g_cfg.folder.c_str());
}

// ---- set_slot_file ----
void set_slot_file(int slot, const std::string& path) {
    if (slot < 0 || slot > 2) return;
    g_cfg.slots[slot].file = path;
    XLOG("hitsound: slot %d = %s", slot, path.c_str());
}

// ---- cooldown ----
static std::atomic<uint64_t> g_last_play_ms[3] = {};

static uint64_t now_ms() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000ull + ts.tv_nsec / 1000000ull;
}

static std::atomic<uint64_t> g_play_count[3] = {};

void play_slot(int slot) {
    if (slot < 0 || slot > 2) return;
    if (!g_cfg.enabled.load(std::memory_order_relaxed)) return;
    Slot& s = g_cfg.slots[slot];
    if (!s.enabled.load(std::memory_order_relaxed)) return;
    if (s.file.empty()) return;

    uint64_t now = now_ms();
    uint64_t last = g_last_play_ms[slot].load(std::memory_order_relaxed);
    int cd = g_cfg.cooldown_ms.load(std::memory_order_relaxed);
    if (now - last < (uint64_t)cd) return;

    g_last_play_ms[slot].store(now, std::memory_order_relaxed);

    float vol = s.volume.load(std::memory_order_relaxed)
              * g_cfg.master_volume.load(std::memory_order_relaxed);
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;

    call_java_play(s.file, vol);
    g_play_count[slot].fetch_add(1, std::memory_order_relaxed);
}

uint64_t play_count(int slot) {
    if (slot < 0 || slot > 2) return 0;
    return g_play_count[slot].load(std::memory_order_relaxed);
}

// ---- HP-триггер ----
// на каждом снапшоте смотрим разницу HP у игроков. храним прошлое состояние.
struct PrevHP {
    void* ptr = nullptr;
    float hp = 0.f;
    bool  seen = false;
};
static constexpr int kMaxTracked = 32;
static PrevHP g_prev[kMaxTracked];

// слоты: 0=hit, 1=headshot, 2=kill
static int classify_hit(float prev_hp, float now_hp) {
    if (now_hp <= 0.0f && prev_hp > 0.0f) return 2;       // kill
    float diff = prev_hp - now_hp;
    if (diff >= 50.0f) return 1;                          // headshot
    if (diff >= 1.0f)  return 0;                          // hit
    return -1;
}

void update() {
    if (!g_cfg.enabled.load(std::memory_order_relaxed)) return;

    const world::SnapshotBuffer* snap = world::snapshot::get();
    if (!snap) return;

    // найти существующий PrevHP по ptr или занять новый слот
    auto find_or_alloc = [](void* ptr, bool create) -> PrevHP* {
        for (int i = 0; i < kMaxTracked; ++i) {
            if (g_prev[i].seen && g_prev[i].ptr == ptr) return &g_prev[i];
        }
        if (!create) return nullptr;
        for (int i = 0; i < kMaxTracked; ++i) {
            if (!g_prev[i].seen) {
                g_prev[i].seen = true;
                g_prev[i].ptr = ptr;
                g_prev[i].hp = 0.f;
                return &g_prev[i];
            }
        }
        return nullptr;
    };

    // удалить мёртвых
    for (int i = 0; i < kMaxTracked; ++i) {
        if (!g_prev[i].seen) continue;
        bool alive = false;
        for (int j = 0; j < snap->count; ++j) {
            if (snap->players[j].ptr == g_prev[i].ptr) { alive = true; break; }
        }
        if (!alive) g_prev[i].seen = false;
    }

    // сравнить HP
    for (int i = 0; i < snap->count; ++i) {
        const world::PlayerSnapshot& s = snap->players[i];
        if (!s.valid || !s.ptr) continue;
        if (s.is_local) continue;

        PrevHP* prev = find_or_alloc(s.ptr, true);
        if (!prev) continue;

        if (prev->hp > 0.f) {
            int slot = classify_hit(prev->hp, s.hp);
            if (slot >= 0) {
                // сработал триггер
                play_slot(slot);
            }
        }
        prev->hp = s.hp;
    }
}

// ---- init ----
bool init() {
    ensure_folder(g_cfg.folder);
    rescan_folder();

    // дефолт: включён только hit, чтобы не спамить
    g_cfg.slots[0].enabled.store(true, std::memory_order_relaxed);

    XLOG("hitsound: init ok folder=%s files=%zu",
         g_cfg.folder.c_str(), g_files.size());
    return true;
}

// ---- shutdown ----
void shutdown() {
    for (int i = 0; i < kMaxTracked; ++i) g_prev[i] = {};
    XLOG("hitsound: shutdown");
}

} // namespace xim::features::hitsound