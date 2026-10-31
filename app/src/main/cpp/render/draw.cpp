#include "render/draw.h"

namespace xim::render::draw {

ImU32 col_from_vec4(const float c[4]) {
    return IM_COL32(
        (int)(c[0]*255.0f),
        (int)(c[1]*255.0f),
        (int)(c[2]*255.0f),
        (int)(c[3]*255.0f));
}

void rect(ImDrawList* dl, vec2 a, vec2 b, ImU32 col, float th) {
    dl->AddRect(ImVec2(a.x, a.y), ImVec2(b.x, b.y), col, 0.0f, 0, th);
}

void rect_filled(ImDrawList* dl, vec2 a, vec2 b, ImU32 col) {
    dl->AddRectFilled(ImVec2(a.x, a.y), ImVec2(b.x, b.y), col);
}

void rect_corner(ImDrawList* dl, vec2 a, vec2 b, ImU32 col, float th, float len) {
    // 4 угла: каждый — два отрезка
    // top-left
    dl->AddLine({a.x, a.y}, {a.x + len, a.y}, col, th);
    dl->AddLine({a.x, a.y}, {a.x, a.y + len}, col, th);
    // top-right
    dl->AddLine({b.x - len, a.y}, {b.x, a.y}, col, th);
    dl->AddLine({b.x, a.y}, {b.x, a.y + len}, col, th);
    // bottom-left
    dl->AddLine({a.x, b.y - len}, {a.x, b.y}, col, th);
    dl->AddLine({a.x, b.y}, {a.x + len, b.y}, col, th);
    // bottom-right
    dl->AddLine({b.x - len, b.y}, {b.x, b.y}, col, th);
    dl->AddLine({b.x, b.y - len}, {b.x, b.y}, col, th);
}

void line(ImDrawList* dl, vec2 a, vec2 b, ImU32 col, float th) {
    dl->AddLine({a.x, a.y}, {b.x, b.y}, col, th);
}

void circle(ImDrawList* dl, vec2 c, float r, ImU32 col, float th, int seg) {
    dl->AddCircle({c.x, c.y}, r, col, seg, th);
}

void text(ImDrawList* dl, vec2 pos, ImU32 col, const char* txt,
          bool centered, bool outlined) {
    ImVec2 sz = ImGui::CalcTextSize(txt);
    ImVec2 p(pos.x, pos.y);
    if (centered) p.x -= sz.x * 0.5f;

    if (outlined) {
        ImU32 shadow = IM_COL32(0, 0, 0, 200);
        dl->AddText({p.x - 1, p.y}, shadow, txt);
        dl->AddText({p.x + 1, p.y}, shadow, txt);
        dl->AddText({p.x, p.y - 1}, shadow, txt);
        dl->AddText({p.x, p.y + 1}, shadow, txt);
    }
    dl->AddText(p, col, txt);
}

void bar(ImDrawList* dl, vec2 a, vec2 b, float frac, ImU32 col, ImU32 bg) {
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    rect_filled(dl, a, b, bg);
    vec2 f = {a.x, a.y + (b.y - a.y) * (1.0f - frac)};
    rect_filled(dl, f, b, col);
}

} // namespace xim::render::draw