#include "features/chams/chams_bridge.h"
#include "features/chams/chams.h"
#include "core/memory.h"
#include "core/il2cpp.h"
#include "world/snapshot.h"
#include "world/registry.h"
#include "offsets.hpp"
#include "util/log.h"

#include <atomic>
#include <cstring>

namespace xim::features::chams::bridge {

using namespace xim::mem;
using namespace lemming::offsets;

// -------- il2cpp high-level через наш core/il2cpp --------
// core::il2cpp уже даёт domain_get/assembly_open/assembly_get_image/class_from_name,
// но нам нужны ещё className (имя класса объекта) и field_get_offset.
// className берём через il2cpp_class_get_name + кэш.
using t_class_get_name   = const char* (*)(void*);
using t_class_get_parent = void* (*)(void*);
using t_class_get_fields = void* (*)(void*, void**);
using t_field_get_offset = std::size_t (*)(void*);
using t_field_get_type   = void* (*)(void*);
using t_type_get_name    = char* (*)(void*);
using t_free             = void (*)(void*);

static t_class_get_name   p_class_get_name   = nullptr;
static t_class_get_parent p_class_get_parent = nullptr;
static t_class_get_fields p_class_get_fields = nullptr;
static t_field_get_offset p_field_get_offset = nullptr;
static t_field_get_type   p_field_get_type   = nullptr;
static t_type_get_name    p_type_get_name    = nullptr;
static t_free             p_free             = nullptr;

static bool resolve_helpers() {
    if (p_class_get_name) return true;
    p_class_get_name   = reinterpret_cast<t_class_get_name>  (dlsym(RTLD_DEFAULT, "il2cpp_class_get_name"));
    p_class_get_parent = reinterpret_cast<t_class_get_parent>(dlsym(RTLD_DEFAULT, "il2cpp_class_get_parent"));
    p_class_get_fields = reinterpret_cast<t_class_get_fields>(dlsym(RTLD_DEFAULT, "il2cpp_class_get_fields"));
    p_field_get_offset = reinterpret_cast<t_field_get_offset>(dlsym(RTLD_DEFAULT, "il2cpp_field_get_offset"));
    p_field_get_type   = reinterpret_cast<t_field_get_type>  (dlsym(RTLD_DEFAULT, "il2cpp_field_get_type"));
    p_type_get_name    = reinterpret_cast<t_type_get_name>   (dlsym(RTLD_DEFAULT, "il2cpp_type_get_name"));
    p_free             = reinterpret_cast<t_free>            (dlsym(RTLD_DEFAULT, "il2cpp_free"));
    return p_class_get_name && p_class_get_parent;
}

static const char* class_name_of(void* obj) {
    if (!obj || !p_class_get_name) return nullptr;
    uintptr_t klass = 0;
    if (!read(obj, klass) || !klass) return nullptr;
    return p_class_get_name(reinterpret_cast<void*>(klass));
}

static bool class_chain_contains(void* obj, const char* want) {
    if (!obj || !want || !p_class_get_name || !p_class_get_parent) return false;
    uintptr_t k = 0;
    if (!read(obj, k) || !k) return false;

    void* klass = reinterpret_cast<void*>(k);
    for (int depth = 0; depth < 12 && klass; ++depth) {
        const char* n = p_class_get_name(klass);
        if (n && std::strcmp(n, want) == 0) return true;
        klass = p_class_get_parent(klass);
    }
    return false;
}

// -------- model (jgi) поиск через mfc.parts list --------
// у нас в registry есть ti_model() но проще найти по klass на месте — тот же
// путь, что в snapshot: parts_container → list → items → найти jgi.

static void* find_model_from_player(uintptr_t player) {
    // mfc.parts_container у ake — тот же mfc-layout
    uintptr_t parts = 0;
    if (!read_at(player, field::mfc::kPartsContainer, parts) || !parts) return nullptr;

    uintptr_t list = 0;
    if (!read_at(parts, field::part_container::kPartList, list) || !list) return nullptr;

    uintptr_t items = 0;
    int32_t   size  = 0;
    if (!read_at(list, field::managed_list::kItems, items)) return nullptr;
    if (!read_at(list, field::managed_list::kSize,  size))  return nullptr;
    if (!items || size <= 0 || size > 128) return nullptr;

    for (int32_t i = 0; i < size; ++i) {
        uintptr_t obj = 0;
        if (!read(items + i * sizeof(void*), obj) || !obj) continue;
        const char* n = class_name_of(reinterpret_cast<void*>(obj));
        if (n && std::strcmp(n, "jgi") == 0)
            return reinterpret_cast<void*>(obj);
    }
    return nullptr;
}

// -------- renderers из jgi.kCharacterBindings --------
struct RendererSet {
    void* character = nullptr;
    void* arms      = nullptr;
    void* gloves    = nullptr;
    void* ragdoll   = nullptr;
};

static bool read_renderers(void* model, RendererSet& out) {
    if (!model) return false;
    uintptr_t m = reinterpret_cast<uintptr_t>(model);

    uintptr_t cb = 0;
    if (!read_at(m, field::jgi::kCharacterBindings, cb) || !cb) return false;

    read_at(cb, field::character_bindings::kCharacterRenderer, out.character);
    read_at(cb, field::character_bindings::kArmsRenderer,      out.arms);
    read_at(cb, field::character_bindings::kGlovesRenderer,    out.gloves);

    // ragdoll: если есть — читаем через тот же cb, но поля kRagdoll у нас нет.
    // берём с mfc.kPartsContainer через klass-имя "h…"? — не подтверждено.
    // оставляем nullptr; пользователь настраивает ragdoll-chams позже.
    out.ragdoll = nullptr;

    return out.character || out.arms || out.gloves;
}

// -------- team / local --------
struct TeamInfo {
    int  team  = 0;
    bool enemy = true;
    bool local = false;
};

static bool read_team_info(uintptr_t player, int local_team, TeamInfo& out) {
    uint8_t team = 0;
    if (!read_at(player, field::ake::kTeam, team)) return false;

    uint8_t local_flag = 0;
    read_at(player, field::mfc::kLocalOwned, local_flag);

    out.team  = team;
    out.local = local_flag != 0;
    out.enemy = !out.local && (team != 0 && team != local_team);
    return true;
}

// -------- cached --------
static std::atomic<int> g_last_count{0};

int last_fed_count() {
    return g_last_count.load(std::memory_order_relaxed);
}

void reset() {
    g_last_count.store(0, std::memory_order_relaxed);
}

// -------- feed --------
void feed() {
    begin_frame();

    if (!config().enabled.load(std::memory_order_relaxed)) {
        g_last_count.store(0, std::memory_order_relaxed);
        return;
    }

    if (!resolve_helpers()) {
        g_last_count.store(0, std::memory_order_relaxed);
        return;
    }

    const world::SnapshotBuffer* snap = world::snapshot::get();
    if (!snap || snap->count <= 0) {
        g_last_count.store(0, std::memory_order_relaxed);
        return;
    }

    const world::PlayerSnapshot* loc = world::snapshot::local();
    const int local_team = loc ? loc->team : 0;

    int fed = 0;
    for (int i = 0; i < snap->count; ++i) {
        const world::PlayerSnapshot& s = snap->players[i];
        if (!s.valid || !s.ptr) continue;

        const uintptr_t player = reinterpret_cast<uintptr_t>(s.ptr);

        TeamInfo team_info{};
        if (!read_team_info(player, local_team, team_info)) continue;

        void* model = find_model_from_player(player);
        if (!model) continue;

        RendererSet rs{};
        if (!read_renderers(model, rs)) continue;

        Target t{};
        t.player    = s.ptr;
        t.character = rs.character;
        t.arms      = rs.arms;
        t.gloves    = rs.gloves;
        t.ragdoll   = rs.ragdoll;
        t.team      = team_info.team;
        t.enemy     = team_info.enemy;
        t.local     = team_info.local;

        register_target(t);
        ++fed;
    }

    g_last_count.store(fed, std::memory_order_relaxed);
    if (fed > 0) {
        static std::atomic<uint64_t> last_log_ms{0};
        struct timespec ts{};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now = uint64_t(ts.tv_sec) * 1000ull + ts.tv_nsec / 1000000ull;
        uint64_t prev = last_log_ms.load(std::memory_order_relaxed);
        if (now - prev >= 5000) {
            last_log_ms.store(now, std::memory_order_relaxed);
            XLOG("chams: fed %d targets (snapshot players=%d)", fed, snap->count);
        }
    }
}

} // namespace xim::features::chams::bridge