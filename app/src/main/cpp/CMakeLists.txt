cmake_minimum_required(VERSION 3.22.1)
project(ximira CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- общие флаги (без -fno-exceptions / -fno-rtti — их хотят imgui и nlohmann) ----
set(XIM_COMMON_FLAGS
        -O3
        -ffast-math
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -fomit-frame-pointer
        -funroll-loops
        )
add_compile_options(${XIM_COMMON_FLAGS})
add_link_options(-Wl,-z,max-page-size=16384 -Wl,--gc-sections)

# ============================================================
# imgui
# ============================================================
add_library(imgui STATIC
        ${CMAKE_SOURCE_DIR}/../../../third_party/imgui/imgui.cpp
        ${CMAKE_SOURCE_DIR}/../../../third_party/imgui/imgui_draw.cpp
        ${CMAKE_SOURCE_DIR}/../../../third_party/imgui/imgui_tables.cpp
        ${CMAKE_SOURCE_DIR}/../../../third_party/imgui/imgui_widgets.cpp
        ${CMAKE_SOURCE_DIR}/../../../third_party/imgui/backends/imgui_impl_opengl3.cpp
        ${CMAKE_SOURCE_DIR}/../../../third_party/imgui/backends/imgui_impl_android.cpp
        )
target_include_directories(imgui PUBLIC
        ${CMAKE_SOURCE_DIR}/../../../third_party/imgui
        ${CMAKE_SOURCE_DIR}/../../../third_party/imgui/backends)

# ============================================================
# dobby
# ============================================================
add_library(dobby STATIC
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/arch/arm64/assembler-arm64.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/arch/arm64/decoder-arm64.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/arch/arm64/decoder-arm64-inl.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/arch/arm64/instruction-decoder-arm64.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/arch/arm64/register-arm64.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/arch/arm64/utils-arm64.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/assembler.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/codegen.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/constants.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/emulator.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/interceptor.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/khook.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/memory.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/core/trampoline.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/TrampolineBridge/ClosureTrampolineBridge/arm64/helper-arm64.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/TrampolineBridge/ClosureTrampolineBridge/closure-trampoline-common-handler.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/Backend/UserMode/PlatformUtil/Linux/ProcessRuntime.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/Backend/UserMode/Thread/PlatformThread.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/Backend/UserMode/UnifiedInterface/platform-posix.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/Backend/UserMode/MultiThreadSupport/ThreadSupport.cpp
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/Backend/UserMode/ExecMemory/code-patch-tool-posix.cc
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/source/Backend/UserMode/ExecMemory/clear-cache-tool-all.c
        )
target_include_directories(dobby PUBLIC
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby/include
        ${CMAKE_SOURCE_DIR}/../../../third_party/dobby)
target_compile_definitions(dobby PRIVATE __arm64__ TARGET_ARCH_ARM64)

# ============================================================
# nlohmann (header-only)
# ============================================================
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
        ${CMAKE_SOURCE_DIR}/../../../third_party/nlohmann
        ${CMAKE_SOURCE_DIR}/../../../third_party/nlohmann/nlohmann)

# ============================================================
# ximira
# ============================================================
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

target_link_libraries(ximira
        imgui
        dobby
        nlohmann_json
        android
        log
        GLESv3
        EGL)

target_compile_definitions(ximira PRIVATE
        XIM_ARM64=1
        XIM_VERSION="1.0.0"
        XIM_BUILD="release")