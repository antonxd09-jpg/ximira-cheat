#include "world/snapshot.h"
#include "world/registry.h"
#include "core/memory.h"
#include "core/il2cpp.h"
#include "offsets.hpp"
#include "util/log.h"

#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <sys/resource.h>

namespace xim::world::snapshot {

using namespace xim::mem;
using namespace lemming::offsets;

static SnapshotBuffer   g_buf[2];
static std::atomic<int> g_front{0};
static std::atomic<int> g_back{1};

static pthread_t         g_thread;
static std::atomic<bool> g_running{false};
static std::atomic<int>  g_interval_ms{33};

struct BoneCache {
    void*     player = nullptr;
    uintptr_t model  = 0;
    uintptr_t transforms[kBoneCount] = {};
    uint64_t  last_resolve = 0;
};
static BoneCache g_cache[kMaxPlayers];

static uint64_t now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + ts.tv_nsec / 1000000ull;
}

// ---- stats (ylw) ----
static void read_stats(uintptr_t stats, PlayerSnapshot& s) {
    if (!stats) return;

    int16_t hp = 0, max_hp = 0, armor = 0, max_armor = 0;
    uint8_t helmet = 0;

    read_at(stats, field::ylw::kHp,       hp);
    read_at(stats, field::ylw::kMaxHp,    max_hp);
    read_at(stats, field::ylw::kArmor,    armor);
    read_at(stats, field::ylw::kMaxArmor, max_armor);
    read_at(stats, field::ylw::kHelmet,   helmet);

    s.hp        = (float)hp;
    s.max_hp    = (float)(max_hp > 0 ? max_hp : 100);
    s.armor     = (float)armor;
    s.max_armor = (float)(max_armor > 0 ? max_armor : 100);
    s.helmet    = helmet != 0;
    s.alive     = hp > 0;
}

// ---- weapon (nrk → qjf) ----
static void read_weapon(uintptr_t player, PlayerSnapshot& s) {
    // weapon part ищем в part-контейнере player через reg::find_component
    uintptr_t wp = reg::find_component(player, reg::ti_weapon_part());
    if (!wp) return;

    uintptr_t wi = 0;   // qjf WeaponInstance
    read_at(wp, field::nrk::kActiveWeapon, wi);
    if (!wi) return;

    int16_t wid = 0, mag = 0, reserve = 0;
    read_at(wi, field::qjf::kWeaponId,     wid);
    read_at(wi, field::qjf::kMagazineAmmo, mag);
    read_at(wi, field::qjf::kReserveAmmo,  reserve);

    s.weapon_id    = wid;
    s.mag_ammo     = mag;
    s.reserve_ammo = reserve;

    // имя оружия — по id. Пока "weapon", подставим таблицу после
    // подтверждения enum WeaponId в dump.cs
    snprintf(s.weapon, kWeaponLen, "weapon");
}

// ---- bones ----
static bool resolve_bones(uintptr_t player, BoneCache& bc) {
    uint64_t now = now_ms();
    if (bc.player == reinterpret_cast<void*>(player)
        && bc.transforms[kHead] && (now - bc.last_resolve) < 1000)
        return true;

    bc.player = reinterpret_cast<void*>(player);
    bc.last_resolve = now;
    for (int i = 0; i < kBoneCount; ++i) bc.transforms[i] = 0;

    // модель jgi
    uintptr_t model = reg::find_component(player, reg::ti_model());
    if (!model) return false;
    bc.model = model;

    // jgi.kCharacterBindings (0x30) -> CharacterBindings
    uintptr_t cb = 0;
    if (!read_at(model, field::jgi::kCharacterBindings, cb) || !cb)
        return false;

    // CharacterBindings.bones (0x20) -> Dictionary<int, Transform>
    uintptr_t dict = 0;
    if (!read_at(cb, field::character_bindings::kBones, dict) || !dict)
        return false;

    uintptr_t entries = 0;
    int32_t   count   = 0;
    if (!read_at(dict, 0x18, entries)) return false;
    if (!read_at(dict, 0x20, count))   return false;
    if (!entries || count <= 0 || count > 512) return false;

    // Entry: [0x00] hashCode, [0x04] next, [0x08] key, [0x10] value
    for (int i = 0; i < count; ++i) {
        uintptr_t e = entries + i * 0x18;
        int32_t hash = 0;
        if (!read(e + 0x00, hash)) continue;
        if (hash < 0) continue;

        int32_t   key = -1;
        uintptr_t val = 0;
        if (!read(e + 0x08, key))  continue;
        if (!read(e + 0x10, val))  continue;
        if (!val) continue;

        switch (key) {
            case 5:  bc.transforms[kHead]      = val; break;
            case 4:  bc.transforms[kNeck]      = val; break;
            case 2:  bc.transforms[kSpine]     = val; break;
            case 0:  bc.transforms[kHip]       = val; break;
            case 6:  bc.transforms[kShoulderL] = val; break;
            case 8:  bc.transforms[kElbowL]    = val; break;
            case 9:  bc.transforms[kHandL]     = val; break;
            case 11: bc.transforms[kShoulderR] = val; break;
            case 13: bc.transforms[kElbowR]    = val; break;
            case 14: bc.transforms[kHandR]     = val; break;
            case 16: bc.transforms[kKneeL]     = val; break;
            case 17: bc.transforms[kFootL]     = val; break;
            case 19: bc.transforms[kKneeR]     = val; break;
            case 20: bc.transforms[kFootR]     = val; break;
            default: break;
        }
    }
    return bc.transforms[kHead] && bc.transforms[kHip];
}

static bool read_world_pos(uintptr_t managed_transform, vec3& out) {
    if (!managed_transform) return false;

    uintptr_t native = 0;
    if (!read_at(managed_transform, managed::kUnityObjectCachedPtr, native))
        return false;
    if (!native) return false;

    vec3 p{};
    if (!read_at(native, 0x90, p)) return false;

    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        return false;
    if (fabsf(p.x) > 1e5f || fabsf(p.y) > 1e5f || fabsf(p.z) > 1e5f)
        return false;

    out = p;
    return true;
}

static void read_bones(BoneCache& bc, BoneSnapshot* out) {
    for (int i = 0; i < kBoneCount; ++i) {
        out[i].valid = false;
        vec3 p{};
        if (read_world_pos(bc.transforms[i], p)) {
            out[i].pos   = p;
            out[i].valid = true;
        }
    }
}

// ---- build snapshot ----
static void build(SnapshotBuffer& out) {
    out.count = 0;
    out.frame++;

    void* players[kMaxPlayers] = {};
    int n = reg::collect_players(players, kMaxPlayers);
    if (n <= 0) return;

    // 1st pass: find local player + origin
    uintptr_t local_ptr = 0;
    vec3      local_origin{};
    bool      local_found = false;

    for (int i = 0; i < n; ++i) {
        uintptr_t p = reinterpret_cast<uintptr_t>(players[i]);
        if (!p) continue;

        uint8_t local_flag = 0;
        read_at(p, field::mfc::kLocalOwned, local_flag);
        if (!local_flag) continue;

        local_ptr = p;
        // origin локального — берём из его костей (позже), или пока из позиции
        // Если bones уже закэшированы — используем hip
        BoneCache& bc = g_cache[0];
        if (resolve_bones(p, bc)) {
            vec3 hip{};
            if (read_world_pos(bc.transforms[kHip], hip)) {
                local_origin = hip;
                local_found = true;
            }
        }
        break;
    }

    // 2nd pass: fill all players
    for (int i = 0; i < n && out.count < kMaxPlayers; ++i) {
        uintptr_t p = reinterpret_cast<uintptr_t>(players[i]);
        if (!p) continue;

        PlayerSnapshot s{};
        s.ptr   = players[i];
        s.valid = true;
        s.is_local = (local_ptr == p);

        uint8_t team = 0, vis = 0;
        read_at(p, field::ake::kTeam,       team);
        read_at(p, field::ake::kVisibility, vis);
        s.team    = team;
        s.visible = vis != 0;

        uintptr_t stats = reg::find_component(p, reg::ti_stats());
        read_stats(stats, s);

        // имя — пока "player". Подставим поле, как только подтвердим.
        snprintf(s.name, kNameLen, s.is_local ? "you" : "player");

        read_weapon(p, s);

        // bones
        BoneCache& bc = g_cache[out.count];
        if (resolve_bones(p, bc)) {
            read_bones(bc, s.bones);

            if (s.bones[kHead].valid) s.head_pos = s.bones[kHead].pos;
            if (s.bones[kHip].valid)  s.origin   = s.bones[kHip].pos;
        }

        // distance — от локального origin к origin игрока
        if (local_found && !s.is_local) {
            vec3 d = s.origin - local_origin;
            s.distance = d.length();
        } else {
            s.distance = 0.0f;
        }

        out.players[out.count++] = s;
    }

    if (local_found) {
        out.local_origin = local_origin;
        out.local_head   = local_origin; // head отдельно не считаем — используем hip
    }
}

static void* worker(void*) {
    setpriority(PRIO_PROCESS, 0, 10);
    while (g_running.load(std::memory_order_relaxed)) {
        int back  = g_back.load(std::memory_order_relaxed);
        int front = g_front.load(std::memory_order_relaxed);
        if (back == front) back = 1 - front;

        build(g_buf[back]);

        g_front.store(back, std::memory_order_release);
        g_back.store(1 - back, std::memory_order_release);

        usleep(g_interval_ms.load(std::memory_order_relaxed) * 1000);
    }
    return nullptr;
}

bool start() {
    if (g_running.exchange(true)) return true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&g_thread, &attr, worker, nullptr) != 0) {
        pthread_attr_destroy(&attr);
        g_running = false;
        XERR("snapshot worker failed");
        return false;
    }
    pthread_attr_destroy(&attr);
    XLOG("snapshot worker started");
    return true;
}

void stop() { g_running = false; }

void set_interval_ms(int ms) {
    if (ms < 1) ms = 1;
    if (ms > 200) ms = 200;
    g_interval_ms.store(ms);
}

const SnapshotBuffer* get() {
    int f = g_front.load(std::memory_order_acquire);
    return &g_buf[f];
}

const PlayerSnapshot* local() {
    const SnapshotBuffer* s = get();
    for (int i = 0; i < s->count; ++i)
        if (s->players[i].is_local) return &s->players[i];
    return nullptr;
}

} // namespace xim::world::snapshot