cmake_minimum_required(VERSION 3.22.1)
project(ximira CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(XIM_COMMON_FLAGS -O3 -ffast-math -fvisibility=hidden -fvisibility-inlines-hidden -fomit-frame-pointer -funroll-loops)
add_compile_options(${XIM_COMMON_FLAGS})
add_link_options(-Wl,-z,max-page-size=16384 -Wl,--gc-sections)

set(THIRD_PARTY ${CMAKE_SOURCE_DIR}/../../../third_party)

# imgui
add_library(imgui STATIC
    ${THIRD_PARTY}/imgui/imgui.cpp
    ${THIRD_PARTY}/imgui/imgui_draw.cpp
    ${THIRD_PARTY}/imgui/imgui_tables.cpp
    ${THIRD_PARTY}/imgui/imgui_widgets.cpp
    ${THIRD_PARTY}/imgui/backends/imgui_impl_opengl3.cpp
    ${THIRD_PARTY}/imgui/backends/imgui_impl_android.cpp
)
target_include_directories(imgui PUBLIC
    ${THIRD_PARTY}/imgui
    ${THIRD_PARTY}/imgui/backends)

# dobby via add_subdirectory
set(Plugin_PlatformUnifiedInterface OFF CACHE BOOL "" FORCE)
set(Plugin_SymbolResolver OFF CACHE BOOL "" FORCE)
set(Plugin_Darwin_Dyld OFF CACHE BOOL "" FORCE)
set(DobbyHome ${THIRD_PARTY}/dobby)
add_subdirectory(${DobbyHome} ${CMAKE_BINARY_DIR}/dobby_build)

# nlohmann (header-only)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${THIRD_PARTY}/nlohmann/single_include)

# ximira
file(GLOB_RECURSE XIM_SRC CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/core/*.cpp
    ${CMAKE_SOURCE_DIR}/render/*.cpp
    ${CMAKE_SOURCE_DIR}/world/*.cpp
    ${CMAKE_SOURCE_DIR}/features/*.cpp
    ${CMAKE_SOURCE_DIR}/menu/*.cpp
    ${CMAKE_SOURCE_DIR}/menu/tabs/*.cpp
    ${CMAKE_SOURCE_DIR}/util/*.cpp
)

add_library(ximira SHARED ${XIM_SRC} ximira.cpp)
target_include_directories(ximira PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(ximira imgui dobby nlohmann_json android log GLESv3 EGL)
target_compile_definitions(ximira PRIVATE XIM_ARM64=1 XIM_VERSION="1.1.0" XIM_BUILD="release")
