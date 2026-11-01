#include "world/registry.h"
#include "core/il2cpp.h"
#include "core/memory.h"
#include "offsets.hpp"
#include "util/log.h"

#include <cstring>
#include <cstdio>

namespace xim::world::reg {

using namespace lemming::offsets;

static void* g_image             = nullptr;
static uintptr_t g_base          = 0;

static void* g_ti_client_world   = nullptr;
static void* g_ti_registry       = nullptr;
static void* g_ti_store          = nullptr;
static void* g_ti_player         = nullptr;
static void* g_ti_netobject      = nullptr;
static void* g_ti_model          = nullptr;
static void* g_ti_stats          = nullptr;
static void* g_ti_visibility     = nullptr;
static void* g_ti_weapon_part    = nullptr;
static void* g_ti_weapon_inst    = nullptr;
static void* g_ti_camera_comp    = nullptr;
static void* g_klass_player      = nullptr;
static void* g_klass_model       = nullptr;
static void* g_klass_stats       = nullptr;
static void* g_klass_weapon_part = nullptr;

static void* load_typeinfo(uintptr_t slot) {
    void* p = nullptr;
    if (!mem::read(slot, p)) return nullptr;
    return p;
}

static uintptr_t find_libil2cpp_base() {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "libil2cpp.so") && strstr(line, "r--p")) {
            sscanf(line, "%lx-", &base);
            break;
        }
    }
    fclose(f);
    return base;
}

bool init() {
    if (g_image && g_base) return true;

    if (!il2cpp::init()) return false;

    void* domain = il2cpp::domain_get();
    if (!domain) return false;

    g_image = il2cpp::assembly_get_image(
            il2cpp::domain_assembly_open("Assembly-CSharp.dll"));
    if (!g_image)
        g_image = il2cpp::assembly_get_image(
                il2cpp::domain_assembly_open("Assembly-CSharp"));
    if (!g_image) { XERR("no Assembly-CSharp"); return false; }

    g_base = find_libil2cpp_base();
    if (!g_base) { XERR("no libil2cpp base"); return false; }

    g_ti_client_world = load_typeinfo(g_base + typeinfo::kClientWorld);
    g_ti_registry     = load_typeinfo(g_base + typeinfo::kRegistry);
    g_ti_store        = load_typeinfo(g_base + typeinfo::kStore);
    g_ti_player       = load_typeinfo(g_base + typeinfo::kPlayer);
    g_ti_netobject    = load_typeinfo(g_base + typeinfo::kNetObject);
    g_ti_model        = load_typeinfo(g_base + typeinfo::kModel);
    g_ti_stats        = load_typeinfo(g_base + typeinfo::kStats);
    g_ti_visibility   = load_typeinfo(g_base + typeinfo::kVisibility);
    g_ti_weapon_part  = load_typeinfo(g_base + typeinfo::kWeaponPart);
    g_ti_weapon_inst  = load_typeinfo(g_base + typeinfo::kWeaponInstance);
    g_ti_camera_comp  = load_typeinfo(g_base + typeinfo::kCameraComponent);

    g_klass_player      = il2cpp::class_from_name(g_image, "", "ake");
    g_klass_model       = il2cpp::class_from_name(g_image, "", "jgi");
    g_klass_stats       = il2cpp::class_from_name(g_image, "", "ylw");
    g_klass_weapon_part = il2cpp::class_from_name(g_image, "", "nrk");

    if (!g_klass_player) { XERR("ake class not found"); return false; }

    XLOG("registry ready: player=%p model=%p stats=%p wp=%p",
         g_klass_player, g_klass_model, g_klass_stats, g_klass_weapon_part);
    return true;
}

void* ti_client_world()   { return g_ti_client_world; }
void* ti_registry()       { return g_ti_registry; }
void* ti_store()          { return g_ti_store; }
void* ti_player()         { return g_ti_player; }
void* ti_netobject()      { return g_ti_netobject; }
void* ti_model()          { return g_ti_model; }
void* ti_stats()          { return g_ti_stats; }
void* ti_visibility()     { return g_ti_visibility; }
void* ti_weapon_part()    { return g_ti_weapon_part; }
void* ti_weapon_instance(){ return g_ti_weapon_inst; }
void* ti_camera_comp()    { return g_ti_camera_comp; }
void* klass_player()      { return g_klass_player; }

// ---- статический ClientWorld.instance через klass->static_fields ----
static uintptr_t get_world_instance() {
    // Il2CppClass.static_fields лежит по offset 0xB8 (il2cpp 2020+, ARM64)
    constexpr std::size_t kStaticFields = 0xB8;
    uintptr_t sf = 0;
    if (!mem::read(reinterpret_cast<uintptr_t>(g_ti_client_world)
                   + kStaticFields, sf)) return 0;
    if (!sf) return 0;

    // instance — первое поле static_fields
    uintptr_t inst = 0;
    mem::read(sf, inst);
    return inst;
}

// пройти по part-контейнеру net-объекта и вернуть компонент с заданным klass
uintptr_t find_component(uintptr_t net, void* klass) {
    if (!net || !klass) return 0;

    uintptr_t parts = 0;
    if (!mem::read_at(net, field::mfc::kPartsContainer, parts) || !parts)
        return 0;

    uintptr_t list = 0;
    if (!mem::read_at(parts, field::part_container::kPartList, list) || !list)
        return 0;

    uintptr_t items = 0;
    int32_t   size  = 0;
    if (!mem::read_at(list, field::managed_list::kItems, items)) return 0;
    if (!mem::read_at(list, field::managed_list::kSize,  size))  return 0;
    if (!items || size <= 0 || size > 256) return 0;

    for (int i = 0; i < size; ++i) {
        uintptr_t obj = 0;
        if (!mem::read(items + i * sizeof(void*), obj) || !obj) continue;

        uintptr_t k = 0;
        if (!mem::read(obj, k)) continue;
        if (reinterpret_cast<void*>(k) == klass)
            return obj;
    }
    return 0;
}

int collect_players(void** out, int max) {
    if (!out || max <= 0) return 0;

    uintptr_t world = get_world_instance();
    if (!world) return 0;

    uintptr_t registry = 0;
    if (!mem::read_at(world, field::njh::kRegistry, registry) || !registry)
        return 0;

    uintptr_t store = 0;
    if (!mem::read_at(registry, field::gat::kStore, store) || !store)
        return 0;

    uintptr_t yjk = 0;
    if (!mem::read_at(store, field::gzx::kObjects, yjk) || !yjk)
        return 0;

    uintptr_t items = 0;
    int32_t   count = 0;
    if (!mem::read_at(yjk, field::yjk::kItems, items)) return 0;
    if (!mem::read_at(yjk, field::yjk::kCount, count)) return 0;
    if (!items || count <= 0) return 0;
    if (count > 512) count = 512;

    int found = 0;
    for (int i = 0; i < count; ++i) {
        uintptr_t net = 0;
        if (!mem::read(items + i * sizeof(void*), net) || !net) continue;

        // ищем компонент ake (Player) в part-контейнере
        uintptr_t player = find_component(net, g_klass_player);
        if (!player) continue;

        out[found++] = reinterpret_cast<void*>(player);
        if (found >= max) break;
    }
    return found;
}

uintptr_t model_from_net(uintptr_t net) {
    return find_component(net, g_klass_model);
}

uintptr_t stats_from_player(uintptr_t player) {
    return find_component(player, g_klass_stats);
}

} // namespace xim::world::reg