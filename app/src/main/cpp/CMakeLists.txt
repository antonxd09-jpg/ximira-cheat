cmake_minimum_required(VERSION 3.22.1)
project(ximira CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-O3 -ffast-math -fvisibility=hidden -fomit-frame-pointer)
set(TP ${CMAKE_SOURCE_DIR}/../../../third_party)

# imgui
add_library(imgui STATIC
    ${TP}/imgui/imgui.cpp ${TP}/imgui/imgui_draw.cpp
    ${TP}/imgui/imgui_tables.cpp ${TP}/imgui/imgui_widgets.cpp
    ${TP}/imgui/backends/imgui_impl_opengl3.cpp
    ${TP}/imgui/backends/imgui_impl_android.cpp)
target_include_directories(imgui PUBLIC ${TP}/imgui ${TP}/imgui/backends)


# nlohmann
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${TP}/nlohmann/single_include)

# ximira
file(GLOB_RECURSE XIM_SRC CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/core/*.cpp ${CMAKE_SOURCE_DIR}/render/*.cpp
    ${CMAKE_SOURCE_DIR}/world/*.cpp ${CMAKE_SOURCE_DIR}/features/*.cpp
    ${CMAKE_SOURCE_DIR}/menu/*.cpp ${CMAKE_SOURCE_DIR}/menu/tabs/*.cpp
    ${CMAKE_SOURCE_DIR}/util/*.cpp)
add_library(ximira SHARED ${XIM_SRC} ximira.cpp)
target_include_directories(ximira PRIVATE ${CMAKE_SOURCE_DIR} ${TP}/imgui ${TP}/imgui/backends)
target_compile_definitions(ximira PRIVATE XIM_ARM64=1 XIM_VERSION="1.1.0")
