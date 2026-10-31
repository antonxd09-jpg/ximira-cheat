#pragma once

#include "util/math.h"
#include "imgui.h"

namespace xim::render::draw {

// примитивы — батчатся через ImGui draw list, поверх GLES сцены
void rect(ImDrawList* dl, vec2 a, vec2 b, ImU32 col, float thickness = 1.0f);
void rect_filled(ImDrawList* dl, vec2 a, vec2 b, ImU32 col);
void rect_corner(ImDrawList* dl, vec2 a, vec2 b, ImU32 col, float th, float len);
void line(ImDrawList* dl, vec2 a, vec2 b, ImU32 col, float th = 1.0f);
void circle(ImDrawList* dl, vec2 c, float r, ImU32 col, float th = 1.0f, int seg = 16);
void text(ImDrawList* dl, vec2 pos, ImU32 col, const char* txt,
          bool centered = false, bool outlined = true);
void bar(ImDrawList* dl, vec2 a, vec2 b, float frac, ImU32 col,
         ImU32 bg = IM_COL32(0,0,0,180));

// хелпер: ARGB float[4] -> ImU32 (для color picker'ов)
ImU32 col_from_vec4(const float c[4]);

} // namespace xim::render::draw