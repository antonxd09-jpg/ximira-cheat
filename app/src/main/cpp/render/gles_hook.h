#pragma once

#include <EGL/egl.h>

namespace xim::render {

// install — вешает хук на eglSwapBuffers
// callback вызывается каждый кадр ПЕРЕД swap (после того как игра отрисовала)
// там рисуем ImGui и ESP
bool install();

// frame callback — регистрируется из меню/фич
using FrameCallback = void (*)();
void set_frame_callback(FrameCallback cb);

} // namespace xim::render