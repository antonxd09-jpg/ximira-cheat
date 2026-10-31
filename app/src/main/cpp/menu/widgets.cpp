#include "menu/widgets.h"
#include "menu/style.h"

#include <cstdarg>
#include <cstdio>

namespace xim::menu::widget {

bool toggle(const char* label, bool* v) {
    ImGui::PushID(label);
    float h = ImGui::GetFrameHeight();
    float w = h * 1.8f;

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("##t", ImVec2(w, h));
    bool changed = false;
    if (ImGui::IsItemClicked()) { *v = !*v; changed = true; }

    ImU32 bg = *v ? style::kAccent : IM_COL32(40, 48, 62, 255);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, h * 0.5f);

    float cx = *v ? (p.x + w - h * 0.5f) : (p.x + h * 0.5f);
    dl->AddCircleFilled(ImVec2(cx, p.y + h * 0.5f), h * 0.4f,
                        IM_COL32(255, 255, 255, 255), 24);

    ImGui::SameLine();
    ImGui::TextUnformatted(label);

    ImGui::PopID();
    return changed;
}

bool slider_float(const char* label, float* v, float lo, float hi,
                  const char* fmt) {
    ImGui::PushID(label);
    float avail = ImGui::GetContentRegionAvail().x;
    float box_w = 60.0f;

    ImGui::SetNextItemWidth(avail - box_w - 8.0f);
    bool changed = ImGui::SliderFloat("##s", v, lo, hi, "");

    ImGui::SameLine();
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, *v);
    ImGui::PushItemWidth(box_w);
    ImGui::BeginDisabled();
    ImGui::InputText("##v", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    ImGui::PopItemWidth();

    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
}

bool slider_int(const char* label, int* v, int lo, int hi) {
    ImGui::PushID(label);
    float avail = ImGui::GetContentRegionAvail().x;
    float box_w = 60.0f;

    ImGui::SetNextItemWidth(avail - box_w - 8.0f);
    bool changed = ImGui::SliderInt("##s", v, lo, hi, "");

    ImGui::SameLine();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", *v);
    ImGui::PushItemWidth(box_w);
    ImGui::BeginDisabled();
    ImGui::InputText("##v", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    ImGui::PopItemWidth();

    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
}

bool dropdown(const char* label, int* current, const char* const items[],
              int items_count) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    bool changed = ImGui::Combo("##c", current, items, items_count);
    ImGui::PopID();
    return changed;
}

bool color_edit(const char* label, float col[4]) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 48.0f);
    bool changed = ImGui::ColorEdit4("##c", col,
            ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_AlphaPreviewHalf);
    ImGui::PopID();
    return changed;
}

void section_begin(const char* title, const char* subtitle) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.08f, 0.11f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.10f, 0.14f, 0.20f, 1.0f));

    ImGui::BeginChild(title, ImVec2(0, 0),
                      ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);

    ImGui::TextColored(ImVec4(0.86f, 0.89f, 0.94f, 1.0f), "%s", title);
    if (subtitle)
        ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.47f, 1.0f), "%s", subtitle);
    ImGui::Separator();
}

void section_end() {
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void hint(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    ImGui::TextColoredV(ImVec4(0.35f, 0.40f, 0.47f, 1.0f), fmt, ap);
    va_end(ap);
}

bool link(const char* label, const char* url) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.18f, 0.50f, 1.0f, 1.0f));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    bool clicked = ImGui::IsItemClicked();
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("%s", url);
    }
    if (clicked) jni_open_url(url);
    return clicked;
}

} // namespace xim::menu::widget