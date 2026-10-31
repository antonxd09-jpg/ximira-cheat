#pragma once

#include "util/math.h"

namespace xim::render {

// инициализирует w2s: резолвит native Camera::GetMain и достаёт матрицы
bool init();

// пересчитывает view/projection матрицы (раз в кадр)
void update_matrices();

// world -> screen. false если точка за камерой или вне frustum
bool world_to_screen(const vec3& world, vec2& out);

// размер viewport в пикселях
vec2 viewport();

} // namespace xim::render