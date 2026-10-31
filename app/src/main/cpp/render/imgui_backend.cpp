#include "render/imgui_backend.h"
#include "util/log.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_android.h"

#include <GLES3/gl3.h>

namespace xim::render {

static bool g_initialized = false;

bool init_imgui() {
    if (g_initialized) return true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;          // не сохраняем imgui.ini
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // GLES3 backend
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        XERR("ImGui_ImplOpenGL3_Init failed");
        return false;
    }

    // Android platform backend (тач, время, дисплей)
    if (!ImGui_ImplAndroid_Init(nullptr)) {
        XERR("ImGui_ImplAndroid_Init failed");
        return false;
    }

    g_initialized = true;
    XLOG("ImGui initialized");
    return true;
}

void imgui_new_frame() {
    if (!g_initialized) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
}

void imgui_render() {
    if (!g_initialized) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void shutdown_imgui() {
    if (!g_initialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();
    g_initialized = false;
}

} // namespace xim::render