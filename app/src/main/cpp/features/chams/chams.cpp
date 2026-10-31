#include "features/chams/chams.h"
#include "core/memory.h"
#include "util/log.h"

#include <android/log.h>
#include <array>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <mutex>

namespace xim::features::chams {

using namespace xim::mem;

// ---------- config ----------
static Config g_cfg;
Config& config() { return g_cfg; }

// ---------- material pool ----------
struct MaterialPool {
    void* flat     = nullptr;
    void* glow     = nullptr;
    void* wire     = nullptr;
    void* ghost    = nullptr;
    void* metallic = nullptr;
};

static MaterialPool g_pool;
static bool         g_ready = false;
static std::mutex   g_init_mutex;

// ---- il2cpp runtime fns (резолвим через dlsym при init) ----
using t_string_new        = void* (*)(const char*);
using t_class_get_method  = void* (*)(void*, const char*, int);
using t_runtime_invoke    = void* (*)(void*, void*, void**, void**);
using t_domain_get        = void* (*)();
using t_asm_open          = void* (*)(void*, const char*);
using t_asm_image         = void* (*)(void*);
using t_class_from_name   = void* (*)(void*, const char*, const char*);

static t_string_new       p_string_new       = nullptr;
static t_class_get_method p_class_get_method = nullptr;
static t_runtime_invoke   p_runtime_invoke   = nullptr;
static t_domain_get       p_domain_get       = nullptr;
static t_asm_open         p_asm_open         = nullptr;
static t_asm_image        p_asm_image        = nullptr;
static t_class_from_name  p_class_from_name  = nullptr;

// ---- Unity managed classes ----
static void* g_klass_shader   = nullptr;
static void* g_klass_material = nullptr;

// ---- methodinfo'ы ----
static void* m_shader_find       = nullptr;   // Shader.Find(string)
static void* m_material_ctor     = nullptr;   // Material..ctor(Shader)
static void* m_material_setcolor = nullptr;   // Material.SetColor(int, Color)
static void* m_material_setfloat = nullptr;   // Material.SetFloat(string, float)

// propertyId для "_Color"
static int   g_color_prop_id = 0;

// ---- native renderer offsets (Unity 2019+, ARM64) ----
static constexpr std::size_t kMatList          = 0x80;   // Material** (указатель на массив)
static constexpr std::size_t kMatCount         = 0x88;   // int32
static constexpr std::size_t kSharedMaterial   = 0x98;   // Material*

// ---------- helpers ----------
static void* make_managed_string(const char* utf8) {
    return p_string_new ? p_string_new(utf8) : nullptr;
}

static bool resolve_il2cpp_fns() {
    if (p_string_new && p_class_get_method && p_runtime_invoke &&
        p_domain_get && p_asm_open && p_asm_image && p_class_from_name)
        return true;

    p_string_new       = reinterpret_cast<t_string_new>       (dlsym(RTLD_DEFAULT, "il2cpp_string_new"));
    p_class_get_method = reinterpret_cast<t_class_get_method> (dlsym(RTLD_DEFAULT, "il2cpp_class_get_method_from_name"));
    p_runtime_invoke   = reinterpret_cast<t_runtime_invoke>   (dlsym(RTLD_DEFAULT, "il2cpp_runtime_invoke"));
    p_domain_get       = reinterpret_cast<t_domain_get>       (dlsym(RTLD_DEFAULT, "il2cpp_domain_get"));
    p_asm_open         = reinterpret_cast<t_asm_open>         (dlsym(RTLD_DEFAULT, "il2cpp_domain_assembly_open"));
    p_asm_image        = reinterpret_cast<t_asm_image>        (dlsym(RTLD_DEFAULT, "il2cpp_assembly_get_image"));
    p_class_from_name  = reinterpret_cast<t_class_from_name>  (dlsym(RTLD_DEFAULT, "il2cpp_class_from_name"));

    return p_string_new && p_class_get_method && p_runtime_invoke &&
           p_domain_get && p_asm_open && p_asm_image && p_class_from_name;
}

static void* resolve_unity_class(const char* name) {
    if (!p_domain_get || !p_asm_open || !p_asm_image || !p_class_from_name) return nullptr;
    void* domain = p_domain_get();
    if (!domain) return nullptr;

    const char* candidates[] = {
        "UnityEngine.CoreModule.dll",
        "UnityEngine.CoreModule",
        "UnityEngine.dll",
        "UnityEngine",
    };
    for (const char* a : candidates) {
        void* asm_ = p_asm_open(domain, a);
        if (!asm_) continue;
        void* img = p_asm_image(asm_);
        if (!img) continue;
        void* klass = p_class_from_name(img, "UnityEngine", name);
        if (klass) return klass;
    }
    return nullptr;
}

// ---------- shader methods ----------
static bool resolve_shader_methods() {
    if (!resolve_il2cpp_fns()) {
        XERR("chams: il2cpp fns unresolved");
        return false;
    }

    g_klass_shader   = resolve_unity_class("Shader");
    g_klass_material = resolve_unity_class("Material");
    if (!g_klass_shader || !g_klass_material) {
        XERR("chams: Shader/Material class not found");
        return false;
    }

    m_shader_find       = p_class_get_method(g_klass_shader,   "Find",     1);
    m_material_ctor     = p_class_get_method(g_klass_material, ".ctor",    1);
    m_material_setcolor = p_class_get_method(g_klass_material, "SetColor", 2);
    m_material_setfloat = p_class_get_method(g_klass_material, "SetFloat", 2);

    if (!m_shader_find || !m_material_ctor) {
        XERR("chams: methodinfo resolve failed find=%p ctor=%p",
             m_shader_find, m_material_ctor);
        return false;
    }

    // propertyId = Shader.PropertyToID("_Color")
    void* m_prop = p_class_get_method(g_klass_shader, "PropertyToID", 1);
    if (m_prop) {
        void* s = make_managed_string("_Color");
        if (s) {
            void* args[1] = { s };
            void* exc = nullptr;
            void* result = p_runtime_invoke(m_prop, nullptr, args, &exc);
            if (result && !exc) {
                // int возвращается boxed: [0x10] — значение
                int32_t v = 0;
                read(reinterpret_cast<uintptr_t>(result) + 0x10, v);
                g_color_prop_id = v;
            }
        }
    }

    XLOG("chams: methods ok find=%p ctor=%p setcolor=%p setfloat=%p propId=%d",
         m_shader_find, m_material_ctor, m_material_setcolor, m_material_setfloat,
         g_color_prop_id);
    return true;
}

// ---------- material factory ----------
static void* create_material(const char* shader_name) {
    if (!p_runtime_invoke || !m_shader_find || !m_material_ctor) return nullptr;

    void* exc = nullptr;
    void* str = make_managed_string(shader_name);
    if (!str) return nullptr;

    void* args1[1] = { str };
    void* shader = p_runtime_invoke(m_shader_find, nullptr, args1, &exc);
    if (!shader || exc) return nullptr;

    void* args2[1] = { shader };
    exc = nullptr;
    void* mat = p_runtime_invoke(m_material_ctor, nullptr, args2, &exc);
    if (!mat || exc) return nullptr;

    return mat;
}

static void set_material_color(void* material, const float rgba[4]) {
    if (!p_runtime_invoke || !m_material_setcolor || !material) return;

    struct Color { float r, g, b, a; };
    Color c{rgba[0], rgba[1], rgba[2], rgba[3]};

    void* args[2] = {
        reinterpret_cast<void*>(static_cast<intptr_t>(g_color_prop_id)),
        &c
    };
    void* exc = nullptr;
    p_runtime_invoke(m_material_setcolor, material, args, &exc);
}

// ---------- init ----------
bool init() {
    std::lock_guard<std::mutex> lk(g_init_mutex);
    if (g_ready) return true;

    if (!resolve_shader_methods()) return false;

    g_pool.flat     = create_material("Unlit/Color");
    g_pool.glow     = create_material("Particles/Standard Unlit");
    g_pool.wire     = create_material("VR/SpatialMapping/Wireframe");
    g_pool.ghost    = create_material("Unlit/Transparent");
    g_pool.metallic = create_material("Standard");

    g_ready = g_pool.flat || g_pool.glow || g_pool.wire
           || g_pool.ghost || g_pool.metallic;

    XLOG("chams: init ready=%d flat=%p glow=%p wire=%p ghost=%p metallic=%p",
         g_ready ? 1 : 0, g_pool.flat, g_pool.glow, g_pool.wire,
         g_pool.ghost, g_pool.metallic);
    return g_ready;
}

// ---------- colors ----------
static void pick_color(const Target& t, float out[4]) {
    if (t.local)      std::memcpy(out, g_cfg.colors.local,   16);
    else if (t.enemy) std::memcpy(out, g_cfg.colors.enemy,   16);
    else              std::memcpy(out, g_cfg.colors.ally,    16);
}

static void rainbow_color(float hue01, float out[4]) {
    float h = std::fmod(hue01, 1.0f) * 6.0f;
    int   i = static_cast<int>(h);
    float f = h - i;
    float q = 1.0f - f;
    switch (i % 6) {
        case 0: out[0]=1; out[1]=f; out[2]=0; break;
        case 1: out[0]=q; out[1]=1; out[2]=0; break;
        case 2: out[0]=0; out[1]=1; out[2]=f; break;
        case 3: out[0]=0; out[1]=q; out[2]=1; break;
        case 4: out[0]=f; out[1]=0; out[2]=1; break;
        default: out[0]=1; out[1]=0; out[2]=q; break;
    }
    out[3] = 1.0f;
}

// ---------- apply one target ----------
void apply_to_target(const Target& t) {
    const Mode mode = static_cast<Mode>(g_cfg.mode.load(std::memory_order_relaxed));
    if (mode == Mode::Off) return;

    float color[4]{};
    pick_color(t, color);

    if (mode == Mode::Rainbow)
        rainbow_color(g_cfg.rainbowHue.load(std::memory_order_relaxed), color);
    if (mode == Mode::Ghost)
        color[3] = g_cfg.ghostAlpha.load(std::memory_order_relaxed);

    void* material = nullptr;
    switch (mode) {
        case Mode::Flat:      material = g_pool.flat;     break;
        case Mode::Glow:      material = g_pool.glow;     break;
        case Mode::Wireframe: material = g_pool.wire;     break;
        case Mode::Ghost:     material = g_pool.ghost;    break;
        case Mode::Metallic:  material = g_pool.metallic; break;
        case Mode::Rainbow:   material = g_pool.flat;     break;
        default: return;
    }
    if (!material) return;

    set_material_color(material, color);

    auto patch = [&](void* renderer) {
        if (!renderer) return;
        uintptr_t r = reinterpret_cast<uintptr_t>(renderer);
        // materialCount = 1
        int32_t one = 1;
        write(reinterpret_cast<void*>(r + kMatCount), one);
        // materialList — указатель на массив указателей; пишем [0] = material
        uintptr_t list_ptr = 0;
        if (!read(reinterpret_cast<void*>(r + kMatList), list_ptr)) return;
        if (!list_ptr) return;
        write(reinterpret_cast<void*>(list_ptr), material);
    };

    patch(t.character);
    patch(t.arms);
    patch(t.gloves);
    if (mode != Mode::Ghost) patch(t.ragdoll);
}

// ---------- target registry ----------
static std::array<Target, 64> g_targets{};
static std::atomic<int>       g_target_count{0};
static std::mutex             g_registry_mutex;

void begin_frame() {
    std::lock_guard<std::mutex> lk(g_registry_mutex);
    g_target_count.store(0, std::memory_order_relaxed);
}

void register_target(const Target& t) {
    std::lock_guard<std::mutex> lk(g_registry_mutex);
    int idx = g_target_count.load(std::memory_order_relaxed);
    if (idx >= static_cast<int>(g_targets.size())) return;
    g_targets[idx] = t;
    g_target_count.store(idx + 1, std::memory_order_relaxed);
}

int target_count() {
    return g_target_count.load(std::memory_order_relaxed);
}

// ---------- per-frame update ----------
void update() {
    if (!g_cfg.enabled.load(std::memory_order_relaxed)) return;
    if (!g_ready && !init()) return;

    // rainbow hue advance
    static uint64_t last_ms = 0;
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_ms = uint64_t(ts.tv_sec) * 1000ull + ts.tv_nsec / 1000000ull;
    if (last_ms == 0) last_ms = now_ms;
    uint64_t dt = now_ms - last_ms;
    last_ms = now_ms;

    float speed = g_cfg.rainbowSpeed.load(std::memory_order_relaxed);
    float hue   = g_cfg.rainbowHue.load(std::memory_order_relaxed);
    hue = std::fmod(hue + (float(dt) / 1000.0f) * speed, 1.0f);
    g_cfg.rainbowHue.store(hue, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lk(g_registry_mutex);
    int n = g_target_count.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
        apply_to_target(g_targets[i]);
}

void shutdown() {
    begin_frame();
}

} // namespace xim::features::chams