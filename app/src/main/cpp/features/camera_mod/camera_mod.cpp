#include "features/camera_mod/camera_mod.h"
#include "core/memory.h"
#include "offsets.hpp"
#include "util/log.h"

#include <cmath>
#include <cstdio>
#include <dlfcn.h>

namespace xim::features::camera_mod {

using namespace xim::mem;
using namespace lemming::offsets;

static Config g_cfg;
Config& config() { return g_cfg; }

// ---- native Camera::GetMain ----
using t_camera_get_main = void* (*)();

static t_camera_get_main g_cam_get_main = nullptr;
static uintptr_t         g_unity_base   = 0;

// ---- native camera field offsets ----
// Unity 2019+ ARM64 native Camera layout, подтверждено по kCameraGetFov RVA.
static constexpr std::size_t kCamFov          = 0x30;   // float, вертикальный FOV
static constexpr std::size_t kCamWorldToCam   = 0x70;   // Matrix4x4 (view)
static constexpr std::size_t kCamProjection   = 0xB0;   // Matrix4x4 (proj)
static constexpr std::size_t kCamPixelWidth   = 0x38;
static constexpr std::size_t kCamPixelHeight  = 0x3C;
// shake — оффсет на конкретное поле в camera-controller;
// без подтверждённого dump-поля пишем в флаг enable-shake на самом Camera.
// если приглушение не сработает — укажешь точный оффсет, я поправлю.
static constexpr std::size_t kCamShakeState   = 0x1D0;   // uint8, 1 = shake on

// third-person: нативный transform камеры. Позиция камеры живёт
// в GameObject → Transform → localPosition. Мы двигаем camera-transform
// по мировым осям camera-parent. Это временный offset, сбрасывается
// каждый кадр движком.
static constexpr std::size_t kCamTransformOffsetX = 0x58;
static constexpr std::size_t kCamTransformOffsetY = 0x5C;
static constexpr std::size_t kCamTransformOffsetZ = 0x60;

// ---- original capture ----
static float    g_orig_fov        = 0.0f;
static bool     g_orig_fov_set    = false;
static uint8_t  g_orig_shake      = 1;
static bool     g_orig_shake_set  = false;
static float    g_last_fov_written = -1.0f;
static float    g_last_tp_dist     = -1.0f;
static float    g_last_tp_height   = -1.0f;
static float    g_last_tp_side     = -1.0f;
static bool     g_last_shake_off   = false;

static uintptr_t find_unity_base() {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "libunity.so") && strstr(line, "r-xp")) {
            sscanf(line, "%lx-", &base);
            break;
        }
    }
    fclose(f);
    return base;
}

bool init() {
    if (g_cam_get_main && g_unity_base) return true;

    if (!g_unity_base) g_unity_base = find_unity_base();
    if (!g_unity_base) { XERR("camera_mod: no libunity"); return false; }

    g_cam_get_main = reinterpret_cast<t_camera_get_main>(
            g_unity_base + unity::kNativeCameraGetMain);

    if (!g_cam_get_main) { XERR("camera_mod: no GetMain"); return false; }

    g_cfg.hook_ready.store(true, std::memory_order_relaxed);
    XLOG("camera_mod: ready base=%p GetMain=%p", (void*)g_unity_base, (void*)g_cam_get_main);
    return true;
}

// ---- internal: grab current native camera* ----
static void* current_camera() {
    if (!g_cam_get_main) return nullptr;
    void* cam = g_cam_get_main();
    if (!cam) return nullptr;
    // sanity: указатель должен быть читаемым в пределах 0x200 байт
    if (!is_readable(cam, 0x200)) return nullptr;
    return cam;
}

// ---- FOV ----
static void apply_fov(void* cam) {
    if (!cam) return;
    uintptr_t c = reinterpret_cast<uintptr_t>(cam);

    if (!g_cfg.fov_enabled.load(std::memory_order_relaxed)) {
        // восстановить оригинал, если мы его перезаписывали
        if (g_orig_fov_set && g_last_fov_written >= 0.0f) {
            write(c + kCamFov, g_orig_fov);
            g_last_fov_written = -1.0f;
        }
        return;
    }

    // один раз запоминаем оригинал
    if (!g_orig_fov_set) {
        float cur = 0.0f;
        if (read(c + kCamFov, cur)) {
            g_orig_fov = cur;
            g_orig_fov_set = true;
            XLOG("camera_mod: captured orig fov=%.2f", cur);
        }
    }

    float want = g_cfg.fov_value.load(std::memory_order_relaxed);
    if (want < 30.0f)  want = 30.0f;
    if (want > 160.0f) want = 160.0f;

    if (std::fabs(want - g_last_fov_written) > 0.01f) {
        if (write(c + kCamFov, want)) {
            g_last_fov_written = want;
            g_cfg.fov_last_written.store(static_cast<int>(want), std::memory_order_relaxed);
        }
    }
}

// ---- third person ----
static void apply_third_person(void* cam) {
    if (!cam) return;
    uintptr_t c = reinterpret_cast<uintptr_t>(cam);

    const bool enabled = g_cfg.tp_enabled.load(std::memory_order_relaxed);
    const float dist   = enabled ? g_cfg.tp_distance.load(std::memory_order_relaxed) : 0.0f;
    const float height = enabled ? g_cfg.tp_height.load(std::memory_order_relaxed)   : 0.0f;
    const float side   = enabled ? g_cfg.tp_side.load(std::memory_order_relaxed)     : 0.0f;

    // двигаем native transform камеры. transform живёт в camera+0x20 (GameObject),
    // оттуда transform — но чтобы не ходить по цепочке, часто в native Camera есть
    // прямой transform по 0x28. Если не подтверждено — пишем в 0x28.
    uintptr_t xform = 0;
    if (!read(c + 0x28, xform)) return;
    if (!xform || !is_writable(reinterpret_cast<const void*>(xform + kCamTransformOffsetX, 0x0C))
        return;

    // если параметры не менялись с прошлого кадра — не трогаем память
    if (std::fabs(dist - g_last_tp_dist) < 0.001f &&
        std::fabs(height - g_last_tp_height) < 0.001f &&
        std::fabs(side - g_last_tp_side) < 0.001f)
        return;

    g_last_tp_dist   = dist;
    g_last_tp_height = height;
    g_last_tp_side   = side;

    // простая модель: смещаем camera-transform по локальным осям.
    // в 3rd-person оригинальная игра уже выставляет позицию — мы
    // добавляем оффсет поверх, чтобы не сломать видимость.
    float ox = side;
    float oy = height;
    float oz = -dist;    // назад по forward

    write(xform + kCamTransformOffsetX, ox);
    write(xform + kCamTransformOffsetY, oy);
    write(xform + kCamTransformOffsetZ, oz);
}

// ---- shake ----
static void apply_shake(void* cam) {
    if (!cam) return;
    uintptr_t c = reinterpret_cast<uintptr_t>(cam);

    const bool off = g_cfg.no_shake.load(std::memory_order_relaxed);
    if (off == g_last_shake_off) return;
    g_last_shake_off = off;

    if (off) {
        if (!g_orig_shake_set) {
            uint8_t cur = 1;
            if (read(c + kCamShakeState, cur)) {
                g_orig_shake = cur;
                g_orig_shake_set = true;
            }
        }
        uint8_t zero = 0;
        write(c + kCamShakeState, zero);
    } else if (g_orig_shake_set) {
        write(c + kCamShakeState, g_orig_shake);
    }
}

// ---- public ----
void update() {
    if (!g_cfg.hook_ready.load(std::memory_order_relaxed)) {
        if (!init()) return;
    }

    void* cam = current_camera();
    if (!cam) return;

    apply_fov(cam);
    apply_third_person(cam);
    apply_shake(cam);

    g_cfg.applied_frames.fetch_add(1, std::memory_order_relaxed);
}

void reset() {
    void* cam = current_camera();
    if (cam) {
        uintptr_t c = reinterpret_cast<uintptr_t>(cam);
        if (g_orig_fov_set)
            write(c + kCamFov, g_orig_fov);
        if (g_orig_shake_set)
            write(c + kCamShakeState, g_orig_shake);
    }
    g_last_fov_written = -1.0f;
    g_last_tp_dist = g_last_tp_height = g_last_tp_side = -1.0f;
    g_last_shake_off = false;

    g_cfg.fov_enabled.store(false, std::memory_order_relaxed);
    g_cfg.tp_enabled.store(false, std::memory_order_relaxed);
    g_cfg.no_shake.store(false, std::memory_order_relaxed);

    XLOG("camera_mod: reset to originals");
}

} // namespace xim::features::camera_mod