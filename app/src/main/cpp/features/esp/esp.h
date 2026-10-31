#pragma once

#include "imgui.h"

namespace xim::features::esp {

// резолвит всё что нужно для ESP (уже сделано в registry/w2s, тут — проверка)
bool init();

// рисует ESP поверх сцены. вызывается каждый кадр из frame_callback,
// использует ImGui background draw list — не создаёт окно, рисует поверх всего
void draw(ImDrawList* dl);

} // namespace xim::features::esp