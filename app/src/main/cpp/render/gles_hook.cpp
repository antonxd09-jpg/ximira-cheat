#include "render/gles_hook.h"
#include "core/hook.h"
#include "core/il2cpp.h"
#include "util/log.h"

#include <dlfcn.h>
#include <GLES3/gl3.h>

namespace xim::render {

using t_eglSwapBuffers = EGLBoolean (*)(EGLDisplay, EGLSurface);

static t_eglSwapBuffers g_orig_swap = nullptr;
static FrameCallback     g_frame_cb = nullptr;

static EGLBoolean hooked_swap(EGLDisplay dpy, EGLSurface surf) {
    // рисуем свой слой поверх игрового кадра
    if (g_frame_cb) g_frame_cb();

    return g_orig_swap(dpy, surf);
}

bool install() {
    // находим eglSwapBuffers в libEGL.so
    void* libegl = dlopen("libEGL.so", RTLD_NOW | RTLD_NOLOAD);
    if (!libegl) libegl = dlopen("libEGL.so", RTLD_NOW);
    if (!libegl) { XERR("libEGL not found"); return false; }

    void* target = dlsym(libegl, "eglSwapBuffers");
    if (!target) { XERR("eglSwapBuffers not found"); return false; }

    bool ok = hook::install(
        reinterpret_cast<uintptr_t>(target),
        reinterpret_cast<void*>(&hooked_swap),
        reinterpret_cast<void**>(&g_orig_swap));

    if (!ok) XERR("failed to hook eglSwapBuffers");
    else     XLOG("eglSwapBuffers hooked at %p", target);

    return ok;
}

void set_frame_callback(FrameCallback cb) {
    g_frame_cb = cb;
}

} // namespace xim::render