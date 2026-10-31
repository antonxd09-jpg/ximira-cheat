#pragma once

namespace xim::render {

// init — инициализирует ImGui + GLES3 backend
// вызывается ОДИН РАЗ, внутри frame callback при первом кадре,
// когда GL-контекст уже валиден
bool init_imgui();

// new_frame + render — вызываются из frame callback
void imgui_new_frame();
void imgui_render();

// shutdown
void shutdown_imgui();

} // namespace xim::render