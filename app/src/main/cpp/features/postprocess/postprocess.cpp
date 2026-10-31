#include "features/postprocess/postprocess.h"
#include "util/log.h"

#include <GLES3/gl3.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

namespace xim::features::postprocess {

static Config g_cfg;
Config& config() { return g_cfg; }

static Stats g_stats;
Stats& stats() { return g_stats; }

// ---------- GL program helpers ----------
struct Program {
    GLuint id      = 0;
    GLint  u_tex   = -1;
    GLint  u_tex2  = -1;
    GLint  u_tex3  = -1;
    GLint  u_time  = -1;
    GLint  u_res   = -1;
    GLint  u_intensity = -1;
    GLint  u_extra1 = -1;
    GLint  u_extra2 = -1;
    GLint  u_extra3 = -1;
    GLint  u_color  = -1;
};

static const char* kVertexSrc = R"(#version 300 es
precision highp float;
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    if (!sh) return 0;
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]{};
        glGetShaderInfoLog(sh, sizeof(log) - 1, nullptr, log);
        XERR("postprocess: shader compile failed: %s", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint link_program(const char* frag_src) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, kVertexSrc);
    if (!vs) return 0;
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!fs) { glDeleteShader(vs); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        char log[512]{};
        glGetProgramInfoLog(prog, sizeof(log) - 1, nullptr, log);
        XERR("postprocess: link failed: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static Program make_program(const char* frag_src) {
    Program p{};
    p.id = link_program(frag_src);
    if (!p.id) return p;
    p.u_tex       = glGetUniformLocation(p.id, "u_tex");
    p.u_tex2      = glGetUniformLocation(p.id, "u_tex2");
    p.u_tex3      = glGetUniformLocation(p.id, "u_tex3");
    p.u_time      = glGetUniformLocation(p.id, "u_time");
    p.u_res       = glGetUniformLocation(p.id, "u_res");
    p.u_intensity = glGetUniformLocation(p.id, "u_intensity");
    p.u_extra1    = glGetUniformLocation(p.id, "u_extra1");
    p.u_extra2    = glGetUniformLocation(p.id, "u_extra2");
    p.u_extra3    = glGetUniformLocation(p.id, "u_extra3");
    p.u_color     = glGetUniformLocation(p.id, "u_color");
    return p;
}

// ---------- shader sources ----------
extern const char* shader_bloom_prefilter;
extern const char* shader_bloom_blur;
extern const char* shader_bloom_combine;
extern const char* shader_dof;
extern const char* shader_lut;
extern const char* shader_vignette;
extern const char* shader_chromatic;
extern const char* shader_scanlines;
extern const char* shader_motion_blur;
extern const char* shader_grain;
extern const char* shader_copy;

// ---------- GL resources ----------
static Program g_p_bloom_pre;
static Program g_p_bloom_blur;
static Program g_p_bloom_comb;
static Program g_p_dof;
static Program g_p_lut;
static Program g_p_vignette;
static Program g_p_chromatic;
static Program g_p_scanlines;
static Program g_p_motion_blur;
static Program g_p_grain;
static Program g_p_copy;

static GLuint g_vao = 0;
static GLuint g_vbo = 0;

static GLuint g_texScene = 0;   // входная сцена
static GLuint g_texA     = 0;   // ping
static GLuint g_texB     = 0;   // pong
static GLuint g_texBloom = 0;   // промежуточный bloom
static GLuint g_fboA     = 0;
static GLuint g_fboB     = 0;
static GLuint g_fboBloom = 0;

static int g_w = 0;
static int g_h = 0;
static bool g_ready = false;

static uint64_t now_ms() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000ull + ts.tv_nsec / 1000000ull;
}
static uint64_t g_start_ms = 0;

// ---------- init ----------
static void create_quad() {
    // два треугольника: [-1..1] x [-1..1], uv [0..1]
    const float verts[] = {
        //  x     y     u     v
        -1.f, -1.f,  0.f,  0.f,
         1.f, -1.f,  1.f,  0.f,
        -1.f,  1.f,  0.f,  1.f,
         1.f, -1.f,  1.f,  0.f,
         1.f,  1.f,  1.f,  1.f,
        -1.f,  1.f,  0.f,  1.f,
    };
    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

static GLuint create_texture(int w, int h) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

static GLuint create_fbo_with_texture(GLuint tex) {
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        XERR("postprocess: fbo incomplete 0x%x", st);
        glDeleteFramebuffers(1, &fbo);
        return 0;
    }
    return fbo;
}

static void destroy_targets() {
    if (g_fboA)     { glDeleteFramebuffers(1, &g_fboA);     g_fboA = 0; }
    if (g_fboB)     { glDeleteFramebuffers(1, &g_fboB);     g_fboB = 0; }
    if (g_fboBloom) { glDeleteFramebuffers(1, &g_fboBloom); g_fboBloom = 0; }
    if (g_texA)     { glDeleteTextures(1, &g_texA);         g_texA = 0; }
    if (g_texB)     { glDeleteTextures(1, &g_texB);         g_texB = 0; }
    if (g_texBloom) { glDeleteTextures(1, &g_texBloom);     g_texBloom = 0; }
    if (g_texScene) { glDeleteTextures(1, &g_texScene);     g_texScene = 0; }
}

static void create_targets(int w, int h) {
    destroy_targets();
    g_texScene = create_texture(w, h);
    g_texA     = create_texture(w, h);
    g_texB     = create_texture(w, h);
    // bloom в половинном разрешении для скорости
    g_texBloom = create_texture(w / 2, h / 2);

    g_fboA     = create_fbo_with_texture(g_texA);
    g_fboB     = create_fbo_with_texture(g_texB);
    g_fboBloom = create_fbo_with_texture(g_texBloom);
}

bool init(int w, int h) {
    if (g_ready) return true;
    if (w <= 0 || h <= 0) return false;

    g_w = w; g_h = h;
    g_start_ms = now_ms();

    g_p_copy        = make_program(shader_copy);
    g_p_bloom_pre   = make_program(shader_bloom_prefilter);
    g_p_bloom_blur  = make_program(shader_bloom_blur);
    g_p_bloom_comb  = make_program(shader_bloom_combine);
    g_p_dof         = make_program(shader_dof);
    g_p_lut         = make_program(shader_lut);
    g_p_vignette    = make_program(shader_vignette);
    g_p_chromatic   = make_program(shader_chromatic);
    g_p_scanlines   = make_program(shader_scanlines);
    g_p_motion_blur = make_program(shader_motion_blur);
    g_p_grain       = make_program(shader_grain);

    if (!g_p_copy.id || !g_p_bloom_pre.id || !g_p_bloom_blur.id || !g_p_bloom_comb.id
        || !g_p_dof.id || !g_p_lut.id || !g_p_vignette.id || !g_p_chromatic.id
        || !g_p_scanlines.id || !g_p_motion_blur.id || !g_p_grain.id) {
        XERR("postprocess: program link failed");
        return false;
    }

    create_quad();
    create_targets(w, h);

    g_ready = true;
    g_stats.ready.store(true, std::memory_order_relaxed);
    XLOG("postprocess: ready %dx%d", w, h);
    return true;
}

void resize(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (w == g_w && h == g_h) return;
    g_w = w; g_h = h;
    if (g_ready) create_targets(w, h);
    XLOG("postprocess: resized to %dx%d", w, h);
}

// ---------- draw helpers ----------
static void bind_program(const Program& p) {
    glUseProgram(p.id);
    glBindVertexArray(g_vao);
}

static void draw_fullscreen() {
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void set_tex(GLint loc, GLuint tex, int unit) {
    if (loc < 0) return;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(loc, unit);
}

// ============================================================
// E2: apply() — цепочка проходов
// ============================================================

// какие проходы реально активны
static bool want_bloom()      { return g_cfg.bloom_enabled.load(std::memory_order_relaxed); }
static bool want_dof()        { return g_cfg.dof_enabled.load(std::memory_order_relaxed); }
static bool want_lut()        { return g_cfg.lut_enabled.load(std::memory_order_relaxed); }
static bool want_vignette()   { return g_cfg.vignette_enabled.load(std::memory_order_relaxed); }
static bool want_chromatic()  { return g_cfg.chromatic_enabled.load(std::memory_order_relaxed); }
static bool want_scanlines()  { return g_cfg.scanlines_enabled.load(std::memory_order_relaxed); }
static bool want_motion()     { return g_cfg.motion_blur_enabled.load(std::memory_order_relaxed); }
static bool want_grain()      { return g_cfg.grain_enabled.load(std::memory_order_relaxed); }

// ping/pong — два FBO. результат каждого пасса пишется в "dst", вход читается из "src",
// потом dst и src меняются местами.
struct PingPong {
    GLuint fbo_a;
    GLuint fbo_b;
    GLuint tex_a;
    GLuint tex_b;
    bool   use_a_as_src = true;

    GLuint src_fbo() const { return use_a_as_src ? fbo_a : fbo_b; }
    GLuint dst_fbo() const { return use_a_as_src ? fbo_b : fbo_a; }
    GLuint src_tex() const { return use_a_as_src ? tex_a : tex_b; }
    GLuint dst_tex() const { return use_a_as_src ? tex_b : tex_a; }
    void   swap()          { use_a_as_src = !use_a_as_src; }
};

// -------- общий вызов одного пасса --------
// читает tex_src, пишет в target_fbo, с заданным program+uniform'ами.
static void pass_texture(const Program& prog,
                         GLuint tex_src,
                         GLuint target_fbo,
                         int w, int h,
                         float intensity,
                         float e1 = 0.f, float e2 = 0.f, float e3 = 0.f,
                         const float color[4] = nullptr) {
    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glViewport(0, 0, w, h);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    bind_program(prog);
    set_tex(prog.u_tex, tex_src, 0);

    if (prog.u_res >= 0)       glUniform2f(prog.u_res, (float)w, (float)h);
    if (prog.u_time >= 0)      glUniform1f(prog.u_time, (float)(now_ms() - g_start_ms) * 0.001f);
    if (prog.u_intensity >= 0) glUniform1f(prog.u_intensity, intensity);
    if (prog.u_extra1 >= 0)    glUniform1f(prog.u_extra1, e1);
    if (prog.u_extra2 >= 0)    glUniform1f(prog.u_extra2, e2);
    if (prog.u_extra3 >= 0)    glUniform1f(prog.u_extra3, e3);
    if (color && prog.u_color >= 0) glUniform4fv(prog.u_color, 1, color);

    draw_fullscreen();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

// -------- bloom (3 пасса) --------
static void run_bloom(PingPong& pp) {
    const int bw = g_w / 2;
    const int bh = g_h / 2;

    const float intensity = g_cfg.bloom_intensity.load(std::memory_order_relaxed);
    const float threshold = g_cfg.bloom_threshold.load(std::memory_order_relaxed);
    const float knee      = g_cfg.bloom_knee.load(std::memory_order_relaxed);

    // 1) prefilter: яркие пиксели → g_texBloom
    {
        glBindFramebuffer(GL_FRAMEBUFFER, g_fboBloom);
        glViewport(0, 0, bw, bh);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        bind_program(g_p_bloom_pre);
        set_tex(g_p_bloom_pre.u_tex, pp.src_tex(), 0);
        if (g_p_bloom_pre.u_res >= 0)
            glUniform2f(g_p_bloom_pre.u_res, (float)bw, (float)bh);
        if (g_p_bloom_pre.u_intensity >= 0)
            glUniform1f(g_p_bloom_pre.u_intensity, intensity);
        if (g_p_bloom_pre.u_extra1 >= 0)
            glUniform1f(g_p_bloom_pre.u_extra1, threshold);
        if (g_p_bloom_pre.u_extra2 >= 0)
            glUniform1f(g_p_bloom_pre.u_extra2, knee);
        draw_fullscreen();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // 2) blur по X → g_texA (переиспользуем pp.dst как временный? лучше отдельный)
    //    используем g_fboA/g_fboB как scratch на половинном разрешении.
    //    внимание: g_fboA/g_fboB — полного размера, но viewport половинный —
    //    остальное не заполняется, но это нормально.
    const int bw2 = bw, bh2 = bh;

    // X-blur: g_texBloom → g_fboA
    {
        glBindFramebuffer(GL_FRAMEBUFFER, g_fboA);
        glViewport(0, 0, bw2, bh2);
        glDisable(GL_BLEND);
        bind_program(g_p_bloom_blur);
        set_tex(g_p_bloom_blur.u_tex, g_texBloom, 0);
        if (g_p_bloom_blur.u_res >= 0)
            glUniform2f(g_p_bloom_blur.u_res, (float)bw2, (float)bh2);
        if (g_p_bloom_blur.u_extra1 >= 0) glUniform1f(g_p_bloom_blur.u_extra1, 1.0f); // dir.x
        if (g_p_bloom_blur.u_extra2 >= 0) glUniform1f(g_p_bloom_blur.u_extra2, 0.0f); // dir.y
        if (g_p_bloom_blur.u_intensity >= 0) glUniform1f(g_p_bloom_blur.u_intensity, 1.0f);
        draw_fullscreen();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Y-blur: g_texA → g_texBloom
    {
        glBindFramebuffer(GL_FRAMEBUFFER, g_fboBloom);
        glViewport(0, 0, bw2, bh2);
        glDisable(GL_BLEND);
        bind_program(g_p_bloom_blur);
        set_tex(g_p_bloom_blur.u_tex, g_texA, 0);
        if (g_p_bloom_blur.u_res >= 0)
            glUniform2f(g_p_bloom_blur.u_res, (float)bw2, (float)bh2);
        if (g_p_bloom_blur.u_extra1 >= 0) glUniform1f(g_p_bloom_blur.u_extra1, 0.0f);
        if (g_p_bloom_blur.u_extra2 >= 0) glUniform1f(g_p_bloom_blur.u_extra2, 1.0f);
        if (g_p_bloom_blur.u_intensity >= 0) glUniform1f(g_p_bloom_blur.u_intensity, 1.0f);
        draw_fullscreen();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // 3) combine: scene + bloom → pp.dst
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pp.dst_fbo());
        glViewport(0, 0, g_w, g_h);
        glDisable(GL_BLEND);
        bind_program(g_p_bloom_comb);
        set_tex(g_p_bloom_comb.u_tex,  pp.src_tex(), 0);
        set_tex(g_p_bloom_comb.u_tex2, g_texBloom,   1);
        if (g_p_bloom_comb.u_res >= 0)
            glUniform2f(g_p_bloom_comb.u_res, (float)g_w, (float)g_h);
        if (g_p_bloom_comb.u_intensity >= 0)
            glUniform1f(g_p_bloom_comb.u_intensity, intensity);
        if (g_p_bloom_comb.u_color >= 0) {
            float tint[4] = {
                g_cfg.bloom_tint[0].load(std::memory_order_relaxed),
                g_cfg.bloom_tint[1].load(std::memory_order_relaxed),
                g_cfg.bloom_tint[2].load(std::memory_order_relaxed),
                g_cfg.bloom_tint[3].load(std::memory_order_relaxed),
            };
            glUniform4fv(g_p_bloom_comb.u_color, 1, tint);
        }
        draw_fullscreen();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    pp.swap();
}

// -------- dof (1 пасс) --------
// без depth-текстуры — приближение через focus/aperture.
// честный DoF требует depth, движок его отдаёт неохотно; это визуально
// даёт мягкость на дальних и ближних объектах.
static void run_dof(PingPong& pp) {
    const float focus    = g_cfg.dof_focus.load(std::memory_order_relaxed);
    const float aperture = g_cfg.dof_aperture.load(std::memory_order_relaxed);
    const float focal    = g_cfg.dof_focal.load(std::memory_order_relaxed);

    pass_texture(g_p_dof, pp.src_tex(), pp.dst_fbo(), g_w, g_h,
                 1.0f,
                 focus, aperture, focal);
    pp.swap();
}

// -------- lut (1 пасс) --------
static void run_lut(PingPong& pp) {
    const int   preset    = g_cfg.lut_preset.load(std::memory_order_relaxed);
    const float intensity = g_cfg.lut_intensity.load(std::memory_order_relaxed);

    pass_texture(g_p_lut, pp.src_tex(), pp.dst_fbo(), g_w, g_h,
                 intensity, (float)preset);
    pp.swap();
}

// -------- chromatic (1 пасс) --------
static void run_chromatic(PingPong& pp) {
    const float intensity = g_cfg.chromatic_intensity.load(std::memory_order_relaxed);
    pass_texture(g_p_chromatic, pp.src_tex(), pp.dst_fbo(), g_w, g_h, intensity);
    pp.swap();
}

// -------- scanlines (1 пасс) --------
static void run_scanlines(PingPong& pp) {
    const float intensity = g_cfg.scanlines_intensity.load(std::memory_order_relaxed);
    const float density   = g_cfg.scanlines_density.load(std::memory_order_relaxed);
    pass_texture(g_p_scanlines, pp.src_tex(), pp.dst_fbo(), g_w, g_h,
                 intensity, density);
    pp.swap();
}

// -------- motion blur (1 пасс, приближённый) --------
static void run_motion_blur(PingPong& pp) {
    const float intensity = g_cfg.motion_blur_intensity.load(std::memory_order_relaxed);
    const int   samples   = g_cfg.motion_blur_samples.load(std::memory_order_relaxed);
    pass_texture(g_p_motion_blur, pp.src_tex(), pp.dst_fbo(), g_w, g_h,
                 intensity, (float)samples);
    pp.swap();
}

// -------- grain (1 пасс) --------
static void run_grain(PingPong& pp) {
    const float intensity = g_cfg.grain_intensity.load(std::memory_order_relaxed);
    pass_texture(g_p_grain, pp.src_tex(), pp.dst_fbo(), g_w, g_h, intensity);
    pp.swap();
}

// -------- vignette (последним, пишет прямо в target_fbo) --------
static void run_vignette(GLuint src_tex, GLuint target_fbo, int w, int h) {
    const float intensity = g_cfg.vignette_intensity.load(std::memory_order_relaxed);
    const float smooth    = g_cfg.vignette_smooth.load(std::memory_order_relaxed);
    const float color[4]  = {
        g_cfg.vignette_color[0].load(std::memory_order_relaxed),
        g_cfg.vignette_color[1].load(std::memory_order_relaxed),
        g_cfg.vignette_color[2].load(std::memory_order_relaxed),
        g_cfg.vignette_color[3].load(std::memory_order_relaxed),
    };
    pass_texture(g_p_vignette, src_tex, target_fbo, w, h,
                 intensity, smooth, 0, 0, color);
}

// -------- базовая копия сцены в ping-pong --------
static void copy_scene_to_pp(GLuint scene_tex, PingPong& pp) {
    pass_texture(g_p_copy, scene_tex, pp.dst_fbo(), g_w, g_h, 1.0f);
    pp.swap();
}

// -------- главный apply --------
bool apply(unsigned scene_tex, unsigned target_fbo) {
    if (!g_ready || g_w <= 0 || g_h <= 0) return false;
    if (!g_cfg.any_enabled()) return false;

    const uint64_t t0 = now_ms();

    // 1) если передана внешняя сцена — копируем её в ping
    //    если scene_tex == 0 — читаем из текущего дефолтного framebuffer'а
    //    (используется, когда чит рендерит уже поверх сцены и хочет
    //     применить постпроцесс к собственному слою).
    PingPong pp{ g_fboA, g_fboB, g_texA, g_texB, true };

    GLuint source_tex = (GLuint)scene_tex;
    if (source_tex == 0) {
        // scene_tex == 0: постпроцесс работает поверх уже отрисованной
        // игровой сцены в дефолтном framebuffer'е. Копируем его.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pp.dst_fbo());
        glBlitFramebuffer(0, 0, g_w, g_h, 0, 0, g_w, g_h,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        pp.swap();
    } else {
        copy_scene_to_pp(source_tex, pp);
    }

    // 2) bloom (внутри сам решает писать ли в pp)
    if (want_bloom())      run_bloom(pp);

    // 3) dof
    if (want_dof())        run_dof(pp);

    // 4) lut / color grading
    if (want_lut())        run_lut(pp);

    // 5) chromatic
    if (want_chromatic())  run_chromatic(pp);

    // 6) scanlines
    if (want_scanlines())  run_scanlines(pp);

    // 7) motion blur
    if (want_motion())     run_motion_blur(pp);

    // 8) grain
    if (want_grain())      run_grain(pp);

    // 9) vignette пишет уже напрямую в target_fbo (он финальный)
    if (want_vignette()) {
        run_vignette(pp.src_tex(), target_fbo, g_w, g_h);
    } else {
        // последний шаг — скопировать результат в target_fbo,
        // если ещё не сделан. Используем copy-pass.
        pass_texture(g_p_copy, pp.src_tex(), target_fbo, g_w, g_h, 1.0f);
    }

    g_stats.passes.fetch_add(1, std::memory_order_relaxed);
    g_stats.last_frame_ms.store((float)(now_ms() - t0),
                                std::memory_order_relaxed);
    return true;
}

// -------- shutdown --------
void shutdown() {
    if (!g_ready) return;

    // снимаем привязку
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    auto drop = [](Program& p) {
        if (p.id) { glDeleteProgram(p.id); p.id = 0; }
    };
    drop(g_p_copy);
    drop(g_p_bloom_pre);
    drop(g_p_bloom_blur);
    drop(g_p_bloom_comb);
    drop(g_p_dof);
    drop(g_p_lut);
    drop(g_p_vignette);
    drop(g_p_chromatic);
    drop(g_p_scanlines);
    drop(g_p_motion_blur);
    drop(g_p_grain);

    destroy_targets();

    if (g_vbo) { glDeleteBuffers(1, &g_vbo); g_vbo = 0; }
    if (g_vao) { glDeleteVertexArrays(1, &g_vao); g_vao = 0; }

    g_ready = false;
    g_stats.ready.store(false, std::memory_order_relaxed);
    XLOG("postprocess: shutdown");
}

} // namespace xim::features::postprocess