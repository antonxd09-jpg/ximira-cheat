#pragma once

#include "util/math.h"

#include <atomic>
#include <cstdint>

namespace xim::features::chams {

enum class Mode : int {
    Off = 0,
    Flat,
    Glow,
    Wireframe,
    Ghost,
    Metallic,
    Rainbow,
};

struct Colors {
    float enemy[4]   = {1.00f, 0.20f, 0.20f, 1.0f};
    float ally[4]    = {0.20f, 1.00f, 0.40f, 1.0f};
    float local[4]   = {0.30f, 0.60f, 1.00f, 1.0f};
    float ragdoll[4] = {0.55f, 0.55f, 0.55f, 1.0f};
};

struct Config {
    std::atomic<bool>  enabled{false};
    std::atomic<int>   mode{static_cast<int>(Mode::Flat)};
    std::atomic<bool>  teamsSeparate{true};
    std::atomic<bool>  throughWalls{true};
    std::atomic<bool>  glowPulse{false};
    std::atomic<float> pulseSpeed{1.5f};
    std::atomic<float> rainbowSpeed{0.5f};
    std::atomic<float> ghostAlpha{0.35f};
    std::atomic<float> metallicRoughness{0.15f};
    std::atomic<float> metallicMetal{0.9f};
    std::atomic<float> glowIntensity{2.0f};
    std::atomic<float> rainbowHue{0.0f};   // 0..1, выставляется из update()
    Colors colors;
};

Config& config();

// резолвит UnityEngine.Shader / Material методы и создаёт пул материалов.
// вызывается один раз в frame_callback, до первого update().
bool init();

// покадровый апдейт: проходит по целям, которые зарегистрировал bridge,
// и пишет override-material в native Renderer.
void update();

// снять все override (когда enabled=false).
void shutdown();

// ---- реестр целей (заполняется chams_bridge.cpp) ----
struct Target {
    void* player    = nullptr;   // ake*
    void* character = nullptr;   // Renderer*
    void* arms      = nullptr;   // Renderer*
    void* gloves    = nullptr;   // Renderer*
    void* ragdoll   = nullptr;   // Renderer* (может быть null)
    int   team      = 0;
    bool  enemy     = true;
    bool  local     = false;
};

void begin_frame();                       // очистить список перед заполнением
void register_target(const Target& t);    // добавить одну цель
int  target_count();

// хелпер: применить текущий режим к одной цели (используется внутри update())
void apply_to_target(const Target& t);

} // namespace xim::features::chams