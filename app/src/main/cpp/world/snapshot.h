#pragma once

#include "util/math.h"

#include <cstdint>
#include <atomic>

namespace xim::world {

constexpr int kMaxPlayers = 24;
constexpr int kMaxBones   = 20;
constexpr int kNameLen    = 32;
constexpr int kWeaponLen  = 24;

enum Bone : int {
    kHead = 0, kNeck, kSpine, kHip,
    kShoulderL, kElbowL, kHandL,
    kShoulderR, kElbowR, kHandR,
    kKneeL, kFootL,
    kKneeR, kFootR,
    kBoneCount
};

struct BoneSnapshot {
    vec3 pos;
    bool valid = false;
};

struct PlayerSnapshot {
    void*   ptr         = nullptr;
    bool    valid       = false;
    bool    is_local    = false;
    int     team        = 0;
    bool    visible     = false;
    bool    alive       = true;

    float   hp          = 0.f;
    float   max_hp      = 100.f;
    float   armor       = 0.f;
    float   max_armor   = 100.f;
    bool    helmet      = false;

    vec3    origin;
    vec3    head_pos;

    BoneSnapshot bones[kBoneCount];

    char    name[kNameLen]      = {};
    char    weapon[kWeaponLen]  = {};
    int     weapon_id           = 0;
    int     mag_ammo            = 0;
    int     reserve_ammo        = 0;

    float   distance            = 0.f;
};

struct SnapshotBuffer {
    PlayerSnapshot players[kMaxPlayers];
    vec3           local_origin;
    vec3           local_head;
    int            count = 0;
    uint64_t       frame = 0;
};

namespace snapshot {
bool start();
void stop();
void set_interval_ms(int ms);
const SnapshotBuffer* get();
const PlayerSnapshot* local();
}

} // namespace xim::world