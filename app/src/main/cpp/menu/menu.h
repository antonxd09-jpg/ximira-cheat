#pragma once

namespace xim::menu {

// вызывается один раз после init_imgui
void init();

// вызывается каждый кадр из frame_callback (после imgui_new_frame)
void draw();

// открыто ли меню (влияет на passthrough тача в игру)
bool is_open();
void set_open(bool v);

// состояние меню: scale, позиция — сохраняется в config
struct State {
    bool  open      = true;
    float scale     = 1.0f;    // 0.5 .. 2.0
    int   tab       = 0;       // активный таб
    int   pos_x     = -1;      // -1 = автоцентр
    int   pos_y     = -1;
};
State& state();

} // namespace xim::menu