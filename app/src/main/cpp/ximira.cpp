#include <jni.h>
#include <pthread.h>
#include <unistd.h>

#include "core/il2cpp.h"
#include "core/hook.h"
#include "render/gles_hook.h"
#include "render/imgui_backend.h"
#include "world/registry.h"
#include "world/snapshot.h"
#include "features/esp/esp.h"
#include "features/esp/config.h"
#include "features/world_mod/world_mod.h"
#include "features/chams/chams.h"
#include "features/chams/chams_bridge.h"
#include "features/glow/glow.h"
#include "features/camera_mod/camera_mod.h"
#include "features/postprocess/postprocess.h"
#include "features/hitsound/hitsound.h"
#include "features/config/config.h"
#include "menu/menu.h"
#include "util/log.h"

#include <imgui.h>
#include <GLES3/gl3.h>

// ---- глобальный VM, доступен в hitsound.cpp и config.cpp через extern ----
JavaVM* g_vm = nullptr;

// ---- JNI-мосты для menu/widgets и features ----
extern "C" void jni_open_url(const char* url) {
    if (!g_vm) return;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }
    jclass cls = env->FindClass("com/ximira/loader/NativeBridge");
    if (cls) {
        jmethodID m = env->GetStaticMethodID(cls, "openUrl", "(Ljava/lang/String;)V");
        if (m) {
            jstring s = env->NewStringUTF(url);
            env->CallStaticVoidMethod(cls, m, s);
            env->DeleteLocalRef(s);
        }
    }
    if (attached) g_vm->DetachCurrentThread();
}

extern "C" void jni_open_path(const char* path) {
    if (!g_vm) return;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }
    jclass cls = env->FindClass("com/ximira/loader/NativeBridge");
    if (cls) {
        jmethodID m = env->GetStaticMethodID(cls, "openPath", "(Ljava/lang/String;)V");
        if (m) {
            jstring s = env->NewStringUTF(path);
            env->CallStaticVoidMethod(cls, m, s);
            env->DeleteLocalRef(s);
        }
    }
    if (attached) g_vm->DetachCurrentThread();
}

// ---- boot state ----
static bool g_init_done = false;
static int  g_frame_count = 0;

// ---- postprocess throttling: если FPS падает — временно режем тяжёлые эффекты ----
static uint64_t g_last_fps_check_ms = 0;
static int      g_frames_since_check = 0;
static bool     g_pp_auto_downgraded = false;

static uint64_t now_ms() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000ull + ts.tv_nsec / 1000000ull;
}

// ---- frame callback, вызывается каждый eglSwapBuffers ----
static void frame_callback() {
    // ленивая инициализация ImGui при первом кадре (уже с валидным GL-контекстом)
    if (!g_init_done) {
        if (!xim::render::init_imgui()) return;
        xim::menu::init();
        xim::features::esp::init();
        xim::features::chams::init();
        xim::features::glow::init();
        xim::features::world_mod::init();
        xim::features::camera_mod::init();
        // postprocess инициализируется отдельно, при первом кадре валидного GLES
        const int w = static_cast<int>(ImGui::GetIO().DisplaySize.x);
        const int h = static_cast<int>(ImGui::GetIO().DisplaySize.y);
        if (w > 0 && h > 0) {
            xim::features::postprocess::init(w, h);
        }
        g_init_done = true;
        g_last_fps_check_ms = now_ms();
        XLOG("ximira: all modules init");
    }

    xim::render::imgui_new_frame();

    // --- camera_mod ДО postprocess, чтобы FOV успел примениться ---
    xim::features::camera_mod::update();

    // --- world_mod (fog/ambient/shadows/tint) ---
    xim::features::world_mod::update();

    // --- chams ---
    if (xim::features::chams::config().enabled.load(std::memory_order_relaxed)) {
        xim::features::chams::bridge::feed();
        xim::features::chams::update();
    }

    // --- ESP ---
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    xim::features::esp::draw(bg);

    // --- glow (поверх ESP) ---
    if (xim::features::glow::config().enabled.load(std::memory_order_relaxed)) {
        xim::features::glow::update(bg,
            static_cast<int>(ImGui::GetIO().DisplaySize.x),
            static_cast<int>(ImGui::GetIO().DisplaySize.y));
    }

    // --- menu ---
    xim::menu::draw();

    // --- ImGui render ---
    xim::render::imgui_render();

    // --- hitsound (HP-триггер, раз в кадр) ---
    xim::features::hitsound::update();

    // --- postprocess auto-throttle ---
    // считаем FPS раз в секунду; если <50 — временно отключаем тяжёлые эффекты
    ++g_frames_since_check;
    uint64_t now = now_ms();
    if (now - g_last_fps_check_ms >= 1000) {
        int fps = g_frames_since_check;
        if (fps < 50 && !g_pp_auto_downgraded) {
            // режем тяжёлые: scanlines, chromatic, grain, motion_blur
            auto& pp = xim::features::postprocess::config();
            pp.scanlines_enabled.store(false, std::memory_order_relaxed);
            pp.chromatic_enabled.store(false, std::memory_order_relaxed);
            pp.grain_enabled.store(false, std::memory_order_relaxed);
            pp.motion_blur_enabled.store(false, std::memory_order_relaxed);
            g_pp_auto_downgraded = true;
            XLOG("postprocess: auto-downgrade, fps=%d", fps);
        }
        g_frames_since_check = 0;
        g_last_fps_check_ms = now;
    }

    ++g_frame_count;
}

// ---- boot thread ----
static void* boot_thread(void*) {
    // 1) ждём il2cpp
    for (int i = 0; i < 60; ++i) {
        if (xim::il2cpp::init()) break;
        sleep(1);
    }

    // 2) ждём пока игра поднимет registry
    for (int i = 0; i < 120; ++i) {
        if (xim::world::reg::init()) break;
        sleep(1);
    }

    // 3) стартуем snapshot worker
    xim::world::snapshot::start();

    // 4) hitsound + config init (не требуют GL)
    xim::features::hitsound::init();
    xim::features::config::init();
    xim::features::config::load_last_used();   // авто-загрузка последнего профиля

    // 5) вешаем GLES-хук
    if (!xim::render::install()) {
        XERR("ximira: gles hook install failed");
        return nullptr;
    }
    xim::render::set_frame_callback(&frame_callback);

    XLOG("ximira: boot complete");
    return nullptr;
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    XLOG("ximira v" XIM_VERSION " loaded");

    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t, &attr, boot_thread, nullptr);
    pthread_attr_destroy(&attr);

    return JNI_VERSION_1_6;
}