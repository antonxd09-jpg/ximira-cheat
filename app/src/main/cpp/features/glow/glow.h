#pragma once

#include "util/math.h"
#include "imgui.h"

#include <atomic>

namespace xim::features::glow {

struct Colors {
    float enemy[4] = {1.00f, 0.30f, 0.30f, 1.0f};
    float ally[4]  = {0.30f, 1.00f, 0.50f, 1.0f};
    float local[4] = {0.40f, 0.70f, 1.00f, 1.0f};
};

struct Config {
    std::atomic<bool>  enabled{false};
    std::atomic<float> thickness{2.0f};      // 0.5 .. 6
    std::atomic<float> intensity{1.6f};      // 0.5 .. 4
    std::atomic<bool>  pulse{false};
    std::atomic<float> pulseSpeed{1.5f};
    std::atomic<bool>  teamColors{true};
    std::atomic<bool>  drawSkeleton{true};   // рисовать линии скелета внутри glow
    std::atomic<bool>  drawBox{true};        // рисовать bounding-рамку
    std::atomic<float> maxDistance{200.0f};  // метров
    Colors colors;
};

Config& config();

// init — не нужен, glow рисуется через ImGui напрямую.
// интерфейс симметричен chams для однообразия.
bool init();
void update(ImDrawList* dl, int screen_w, int screen_h);
void shutdown();

} // namespace xim::features::glow