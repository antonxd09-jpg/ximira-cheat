#include "render/w2s.h"
#include "core/memory.h"
#include "offsets.hpp"
#include "util/log.h"

#include <cstring>
#include <cstdio>

namespace xim::render {

using namespace lemming::offsets;

using t_camera_get_main = void* (*)();

static t_camera_get_main g_cam_get_main = nullptr;
static uintptr_t         g_unity_base   = 0;

static mat4 g_view{};
static mat4 g_proj{};
static vec2 g_viewport{1920, 1080};
static bool g_ok = false;

static constexpr std::size_t kCamWorldToCamera = 0x70;
static constexpr std::size_t kCamProjection    = 0xB0;
static constexpr std::size_t kCamPixelWidth    = 0x38;
static constexpr std::size_t kCamPixelHeight   = 0x3C;

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
    if (g_ok) return true;
    g_unity_base = find_unity_base();
    if (!g_unity_base) return false;

    g_cam_get_main = reinterpret_cast<t_camera_get_main>(
            g_unity_base + unity::kNativeCameraGetMain);
    g_ok = true;
    return true;
}

void update_matrices() {
    if (!g_ok) return;
    void* cam = g_cam_get_main ? g_cam_get_main() : nullptr;
    if (!cam) return;

    uintptr_t c = reinterpret_cast<uintptr_t>(cam);
    if (!mem::is_readable(reinterpret_cast<void*>(c), 0x100)) return;

    mem::read_at(c, kCamWorldToCamera, g_view);
    mem::read_at(c, kCamProjection,    g_proj);

    int w = 0, h = 0;
    mem::read_at(c, kCamPixelWidth,  w);
    mem::read_at(c, kCamPixelHeight, h);
    if (w > 0 && h > 0) g_viewport = vec2((float)w, (float)h);
}

bool world_to_screen(const vec3& world, vec2& out) {
    if (!g_ok) return false;

    vec4 wp{world.x, world.y, world.z, 1.0f};
    vec4 view = g_view.mul_vec4(wp);
    if (view.w < 0.01f) return false;

    vec4 clip = g_proj.mul_vec4(view);
    if (clip.w <= 0.0f) return false;

    float ndc_x = clip.x / clip.w;
    float ndc_y = clip.y / clip.w;

    out.x = (ndc_x * 0.5f + 0.5f) * g_viewport.x;
    out.y = (1.0f - (ndc_y * 0.5f + 0.5f)) * g_viewport.y;

    return out.x >= -200 && out.x <= g_viewport.x + 200
        && out.y >= -200 && out.y <= g_viewport.y + 200;
}

vec2 viewport() { return g_viewport; }

} // namespace xim::render