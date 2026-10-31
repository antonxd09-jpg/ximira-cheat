#pragma once

#include <atomic>

namespace xim::features::camera_mod {

struct Config {
    // FOV
    std::atomic<bool>  fov_enabled{false};
    std::atomic<float> fov_value{90.0f};         // 60 .. 160
    std::atomic<bool>  fov_per_weapon{false};    // игнорируется при выкл. снайперки

    // Third person
    std::atomic<bool>  tp_enabled{false};
    std::atomic<float> tp_distance{2.5f};        // 0.5 .. 8
    std::atomic<float> tp_height{-0.3f};         // -2 .. 2
    std::atomic<float> tp_side{0.0f};            // -2 .. 2

    // Camera shake
    std::atomic<bool>  no_shake{false};

    // Camera hook state (для отладки в UI)
    std::atomic<bool>  hook_ready{false};
    std::atomic<int>   fov_last_written{0};
    std::atomic<int>   applied_frames{0};
};

Config& config();

// резолвит Camera::GetMain, читает текущий native camera*, кэширует.
// true если камера доступна. вызывается в frame_callback.
bool init();

// применяет конфиг к камере. вызывается каждый кадр.
void update();

// сброс к оригинальным значениям (при выходе из матча / выключении фичи)
void reset();

} // namespace xim::features::camera_mod