#pragma once

#include <atomic>
#include <cstdint>

namespace xim::features::postprocess {

// ---- конфиг всех эффектов ----
struct Config {
    // Bloom
    std::atomic<bool>  bloom_enabled{false};
    std::atomic<float> bloom_intensity{1.0f};    // 0 .. 5
    std::atomic<float> bloom_threshold{0.9f};    // 0 .. 2
    std::atomic<float> bloom_knee{0.5f};         // 0 .. 1
    std::atomic<float> bloom_tint[4] = {1,1,1,1}; // rgba

    // Depth of Field
    std::atomic<bool>  dof_enabled{false};
    std::atomic<float> dof_focus{20.0f};         // 0.1 .. 500
    std::atomic<float> dof_aperture{5.6f};       // 0.5 .. 22
    std::atomic<float> dof_focal{50.0f};         // 10 .. 300

    // LUT / color grading
    std::atomic<bool>  lut_enabled{false};
    std::atomic<int>   lut_preset{0};            // 0 none .. 5
    std::atomic<float> lut_intensity{1.0f};

    // Vignette
    std::atomic<bool>  vignette_enabled{false};
    std::atomic<float> vignette_intensity{0.4f};
    std::atomic<float> vignette_smooth{0.4f};
    std::atomic<float> vignette_color[4] = {0,0,0,1};

    // Chromatic aberration
    std::atomic<bool>  chromatic_enabled{false};
    std::atomic<float> chromatic_intensity{0.3f};

    // Scanlines
    std::atomic<bool>  scanlines_enabled{false};
    std::atomic<float> scanlines_intensity{0.3f};
    std::atomic<float> scanlines_density{2.0f};

    // Motion blur
    std::atomic<bool>  motion_blur_enabled{false};
    std::atomic<float> motion_blur_intensity{0.4f};
    std::atomic<int>   motion_blur_samples{6};

    // Film grain
    std::atomic<bool>  grain_enabled{false};
    std::atomic<float> grain_intensity{0.2f};

    // Общий выключатель — если ни один эффект не включён, вся цепочка
    // пропускается, ноль затрат.
    bool any_enabled() const {
        return bloom_enabled.load(std::memory_order_relaxed)
            || dof_enabled.load(std::memory_order_relaxed)
            || lut_enabled.load(std::memory_order_relaxed)
            || vignette_enabled.load(std::memory_order_relaxed)
            || chromatic_enabled.load(std::memory_order_relaxed)
            || scanlines_enabled.load(std::memory_order_relaxed)
            || motion_blur_enabled.load(std::memory_order_relaxed)
            || grain_enabled.load(std::memory_order_relaxed);
    }
};

Config& config();

// ---- жизненный цикл ----
// init создаёт FBO/VAO/VBO/шейдеры. Вызывается один раз в frame_callback
// (после того как GLES3-контекст жив). Возвращает false, если GLES3 недоступен.
bool init(int screen_width, int screen_height);

// resize при смене размеров viewport
void resize(int screen_width, int screen_height);

// вызывается каждый кадр.
//   scene_tex — GLuint texture, в которой уже отрендерена игровая сцена
//                (или 0 — тогда pass делает копию из текущего framebuffer)
//   target_fbo — куда писать итог. 0 = дефолтный framebuffer.
// Возвращает true если цепочка реально выполнилась.
bool apply(unsigned scene_tex, unsigned target_fbo);

// освобождает ресурсы GLES
void shutdown();

// диагностика
struct Stats {
    std::atomic<int>   passes{0};
    std::atomic<float> last_frame_ms{0.0f};
    std::atomic<bool>  ready{false};
};
Stats& stats();

} // namespace xim::features::postprocess