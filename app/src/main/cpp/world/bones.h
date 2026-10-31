#pragma once

#include "util/math.h"
#include "world/snapshot.h"

#include <cstdint>

namespace xim::world::bones {

// резолвит Transform* для костей игрока один раз и кэширует на ptr
// модель jgi лежит в ake через player-цепочку, у модели есть animator
// и CharacterBindings.bones — словарь HumanBodyBones -> Transform
bool resolve(void* player, uintptr_t model_ptr, uintptr_t* out_transforms);

// читает мировые позиции костей из закэшированных Transform*
// через Transform::get_position (по умолчанию kTransformGetPositionAndRotation,
// но для чистых позиций используем transform->localToWorldMatrix или
// прямой путь: NativeTransform->worldPos по смещению 0x38)
void read_positions(uintptr_t* transforms, BoneSnapshot* out);

} // namespace xim::world::bones