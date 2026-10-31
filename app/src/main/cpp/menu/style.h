#pragma once

#include "imgui.h"

namespace xim::menu::style {

// ---- палитра (как на референсе XIMIRA) ----
constexpr ImU32 kBgDeep       = IM_COL32(10,  14,  20,  255); // фон окна
constexpr ImU32 kBgPanel      = IM_COL32(15,  20,  28,  255); // карточки
constexpr ImU32 kBgSidebar    = IM_COL32(12,  16,  23,  255); // левая панель
constexpr ImU32 kBorder       = IM_COL32(26,  35,  50,  255); // border 1px
constexpr ImU32 kAccent       = IM_COL32(45,  127, 255, 255); // синий
constexpr ImU32 kAccentDim    = IM_COL32(45,  127, 255, 90);
constexpr ImU32 kText         = IM_COL32(220, 228, 240, 255);
constexpr ImU32 kTextDim      = IM_COL32(130, 145, 165, 255);
constexpr ImU32 kTextMuted    = IM_COL32(90,  102, 120, 255);
constexpr ImU32 kDanger       = IM_COL32(235, 80,  80,  255);
constexpr ImU32 kSuccess      = IM_COL32(80,  210, 130, 255);

// применить стиль к ImGui
void apply();

// шрифты (грузим один раз, разные размеры)
struct Fonts {
    ImFont* regular = nullptr;
    ImFont* small   = nullptr;
    ImFont* title   = nullptr;
    ImFont* logo    = nullptr;
};
Fonts& fonts();
void  load_fonts();

} // namespace xim::menu::style