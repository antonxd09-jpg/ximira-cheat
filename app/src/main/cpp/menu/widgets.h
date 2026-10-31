#pragma once

#include "imgui.h"

namespace xim::menu::widget {

bool toggle(const char* label, bool* v);

bool slider_float(const char* label, float* v, float lo, float hi,
                  const char* fmt = "%.2f");
bool slider_int(const char* label, int* v, int lo, int hi);

bool dropdown(const char* label, int* current, const char* const items[],
              int items_count);

bool color_edit(const char* label, float col[4]);

void section_begin(const char* title, const char* subtitle = nullptr);
void section_end();

void hint(const char* fmt, ...);
bool link(const char* label, const char* url);

} // namespace xim::menu::widget

// JNI-мосты, реализованы в ximira.cpp
extern "C" void jni_open_url(const char* url);
extern "C" void jni_open_path(const char* path);