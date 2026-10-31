#include "world/bones.h"
#include "core/il2cpp.h"
#include "core/memory.h"
#include "offsets.hpp"
#include "util/log.h"

namespace xim::world::bones {

using namespace lemming::offsets;

// Unity Transform layout (IL2CPP, Unity 2019+, ARM64):
//   [0x00] vtable
//   [0x08] monitor
//   [0x10] m_CachedPtr  -> native Transform*
// native Transform:
//   [0x38] localPosition (vec3)
//   [0x44] localRotation (quat)
//   [0x54] localScale
//   [0x80] worldPosition — но не всегда актуально; безопаснее вызывать
//          Component::get_transform + Transform::get_position через движок
//
// Здесь используется самый дешёвый путь: читаем native Transform*
// и берём worldPosition по оффсету 0x90 (для Unity 2019.4 ARM64).
// Если позиции поедут — переключимся на вызов kTransformGetPositionAndRotation.

static constexpr std::size_t kUnityObjectCachedPtr = 0x10;
static constexpr std::size_t kNativeTransformWorldPos = 0x90;

// управляемый Transform::get_position (из kTransformGetPositionAndRotation
// внутри есть get_position, но проще вызвать напрямую native)
using t_transform_get_pos = void (*)(void* self, vec3* out);

bool resolve(void* player, uintptr_t model_ptr, uintptr_t* out_transforms) {
    if (!player || !model_ptr || !out_transforms) return false;

    // jgi.kCharacterBindings (0x30) -> CharacterBindings*
    uintptr_t cb = 0;
    if (!mem::read_at(model_ptr, field::jgi::kCharacterBindings, cb) || !cb)
        return false;

    // CharacterBindings.bones (0x20) -> Dictionary<HumanBodyBones, Transform>
    uintptr_t bones_dict = 0;
    if (!mem::read_at(cb, field::character_bindings::kBones, bones_dict) || !bones_dict)
        return false;

    // Dictionary layout в IL2CPP:
    //   [0x10] buckets
    //   [0x18] entries  (Entry[]: hash, next, key, value)
    //   [0x20] count
    //   [0x28] version
    // Entry: [0x00] hashCode(int), [0x04] next(int), [0x08] key, [0x10] value
    uintptr_t entries = 0;
    int32_t   count   = 0;
    if (!mem::read_at(bones_dict, 0x18, entries)) return false;
    if (!mem::read_at(bones_dict, 0x20, count))   return false;
    if (!entries || count <= 0 || count > 256)    return false;

    // очищаем кэш
    for (int i = 0; i < kBoneCount; ++i) out_transforms[i] = 0;

    for (int i = 0; i < count; ++i) {
        uintptr_t entry = entries + i * 0x18;
        int32_t   hash  = 0;
        int32_t   next  = 0;
        int32_t   key   = -1;
        uintptr_t value = 0;

        if (!mem::read(entry + 0x00, hash)) continue;
        if (hash < 0) continue;                    // пустой слот
        if (!mem::read(entry + 0x08, key))  continue;
        if (!mem::read(entry + 0x10, value)) continue;
        if (!value) continue;

        // HumanBodyBones enum (Unity):
        //   0 = Hips
        //   1 = Spine, 2 = Chest, 3 = UpperChest, 4 = Neck, 5 = Head
        //   6 = LeftShoulder, 7 = LeftUpperArm, 8 = LeftLowerArm, 9 = LeftHand
        //  11 = RightShoulder, 12 = RightUpperArm, 13 = RightLowerArm, 14 = RightHand
        //  15 = LeftUpperLeg, 16 = LeftLowerLeg, 17 = LeftFoot
        //  18 = RightUpperLeg, 19 = RightLowerLeg, 20 = RightFoot

        switch (key) {
            case 5:  out_transforms[kHead]       = value; break;
            case 4:  out_transforms[kNeck]       = value; break;
            case 2:  out_transforms[kSpine]      = value; break;
            case 0:  out_transforms[kHip]        = value; break;

            case 6:  out_transforms[kShoulderL]  = value; break;
            case 8:  out_transforms[kElbowL]     = value; break;
            case 9:  out_transforms[kHandL]      = value; break;

            case 11: out_transforms[kShoulderR]  = value; break;
            case 13: out_transforms[kElbowR]     = value; break;
            case 14: out_transforms[kHandR]      = value; break;

            case 16: out_transforms[kKneeL]      = value; break;
            case 17: out_transforms[kFootL]      = value; break;
            case 19: out_transforms[kKneeR]      = value; break;
            case 20: out_transforms[kFootR]      = value; break;
            default: break;
        }
    }

    return out_transforms[kHead] && out_transforms[kHip];
}

void read_positions(uintptr_t* transforms, BoneSnapshot* out) {
    for (int i = 0; i < kBoneCount; ++i) {
        out[i].valid = false;
        if (!transforms[i]) continue;

        // managed Transform -> native Transform*
        uintptr_t native = 0;
        if (!mem::read_at(transforms[i], kUnityObjectCachedPtr, native) || !native)
            continue;

        vec3 pos{};
        if (!mem::read_at(native, kNativeTransformWorldPos, pos))
            continue;

        // sanity: ноль/мусор отсекаем
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
            continue;
        if (fabsf(pos.x) > 1e5f || fabsf(pos.y) > 1e5f || fabsf(pos.z) > 1e5f)
            continue;

        out[i].pos   = pos;
        out[i].valid = true;
    }
}

} // namespace xim::world::bones