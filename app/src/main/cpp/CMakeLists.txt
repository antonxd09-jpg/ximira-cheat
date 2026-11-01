cmake_minimum_required(VERSION 3.22.1)
project(ximira CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-O3 -ffast-math -fvisibility=hidden -fvisibility-inlines-hidden -fomit-frame-pointer -funroll-loops)
add_link_options(-Wl,--gc-sections)
set(TP ${CMAKE_SOURCE_DIR}/../../../third_party)

# imgui
add_library(imgui STATIC
    ${TP}/imgui/imgui.cpp ${TP}/imgui/imgui_draw.cpp
    ${TP}/imgui/imgui_tables.cpp ${TP}/imgui/imgui_widgets.cpp
    ${TP}/imgui/backends/imgui_impl_opengl3.cpp
    ${TP}/imgui/backends/imgui_impl_android.cpp)
target_include_directories(imgui PUBLIC ${TP}/imgui ${TP}/imgui/backends)

# shadowhook (inline hook for Android)
set(SH_ROOT ${TP}/shadowhook/shadowhook)
add_library(shadowhook STATIC
    ${SH_ROOT}/src/main/cpp/shadowhook.c)
target_include_directories(shadowhook PUBLIC ${SH_ROOT}/src/main/cpp/include)
target_compile_definitions(shadowhook PRIVATE SHADOWHOOK_EXPORT)
target_link_libraries(shadowhook android log)

# nlohmann
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${TP}/nlohmann/single_include)

# ximira
file(GLOB_RECURSE XIM_SRC CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/core/*.cpp
    ${CMAKE_SOURCE_DIR}/render/*.cpp
    ${CMAKE_SOURCE_DIR}/world/*.cpp
    ${CMAKE_SOURCE_DIR}/features/*.cpp
    ${CMAKE_SOURCE_DIR}/menu/*.cpp
    ${CMAKE_SOURCE_DIR}/menu/tabs/*.cpp
    ${CMAKE_SOURCE_DIR}/util/*.cpp)
add_library(ximira SHARED ${XIM_SRC} ximira.cpp)
target_include_directories(ximira PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(ximira imgui shadowhook nlohmann_json android log GLESv3 EGL)
target_compile_definitions(ximira PRIVATE XIM_ARM64=1 XIM_VERSION="1.1.0")
