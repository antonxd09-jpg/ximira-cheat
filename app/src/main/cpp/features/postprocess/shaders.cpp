#include <cstddef>

namespace xim::features::postprocess {

// ---------------- copy ----------------
const char* shader_copy = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 fragColor;
void main() {
    fragColor = texture(u_tex, v_uv);
}
)";

// ---------------- bloom: prefilter ----------------
// Выбирает яркие пиксели и уводит их в яркость, с порогом и soft knee.
const char* shader_bloom_prefilter = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec2  u_res;
uniform float u_extra1;   // threshold
uniform float u_extra2;   // knee
out vec4 fragColor;

void main() {
    vec3 c = texture(u_tex, v_uv).rgb;
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float th   = u_extra1;
    float knee = u_extra2 * th + 1e-5;
    float soft = clamp(luma - th + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float contrib = max(soft, luma - th) / max(luma, 1e-5);
    fragColor = vec4(c * contrib, 1.0);
}
)";

// ---------------- bloom: blur (separable) ----------------
// 9-тапный гаусс, направление задаётся u_extra1 (x), u_extra2 (y).
const char* shader_bloom_blur = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec2  u_res;
uniform float u_extra1;   // dir.x
uniform float u_extra2;   // dir.y
out vec4 fragColor;

void main() {
    vec2 dir = vec2(u_extra1, u_extra2) / u_res;
    float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 sum = texture(u_tex, v_uv).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        vec2 off = dir * float(i) * 1.4;
        sum += texture(u_tex, v_uv + off).rgb * w[i];
        sum += texture(u_tex, v_uv - off).rgb * w[i];
    }
    fragColor = vec4(sum, 1.0);
}
)";

// ---------------- bloom: combine ----------------
const char* shader_bloom_combine = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;    // scene
uniform sampler2D u_tex2;   // bloom
uniform float u_intensity;
uniform vec4  u_color;      // tint
out vec4 fragColor;

void main() {
    vec3 scene = texture(u_tex,  v_uv).rgb;
    vec3 bloom = texture(u_tex2, v_uv).rgb;
    bloom *= u_color.rgb;
    fragColor = vec4(scene + bloom * u_intensity, 1.0);
}
)";

// ---------------- dof (без depth, приближение) ----------------
// мягкость на всём кадре, сила определяется focus/aperture.
// e1=focus, e2=aperture, e3=focal.
const char* shader_dof = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec2  u_res;
uniform float u_extra1;   // focus
uniform float u_extra2;   // aperture
uniform float u_extra3;   // focal
out vec4 fragColor;

void main() {
    // приближённый круг размытия: 8 тапов по окружности
    float radius = clamp(u_extra1 / max(u_extra2, 0.5) / max(u_extra3, 1.0), 0.0, 1.0);
    radius = 1.0 - radius;             // ближе фокус → меньше размытие
    radius = clamp(radius, 0.0, 0.8);
    vec2 texel = 1.0 / u_res;

    vec3 sum = vec3(0.0);
    int  n = 8;
    for (int i = 0; i < n; ++i) {
        float a = 6.2831853 * float(i) / float(n);
        vec2  o = vec2(cos(a), sin(a)) * texel * radius * 6.0;
        sum += texture(u_tex, v_uv + o).rgb;
    }
    sum /= float(n);
    fragColor = vec4(sum, 1.0);
}
)";

// ---------------- lut / color grading ----------------
// u_extra1 = preset (0..5), u_intensity = mix.
// 0=none, 1=warm, 2=cold, 3=night, 4=thermal, 5=nvg
const char* shader_lut = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform float u_intensity;
uniform float u_extra1;
out vec4 fragColor;

vec3 preset_warm(vec3 c)    { return clamp(c * vec3(1.15, 1.02, 0.85) + vec3(0.03, 0.00, -0.02), 0.0, 1.0); }
vec3 preset_cold(vec3 c)    { return clamp(c * vec3(0.85, 0.98, 1.18) + vec3(-0.02, 0.00, 0.03), 0.0, 1.0); }
vec3 preset_night(vec3 c)   {
    float l = dot(c, vec3(0.2126,0.7152,0.0722));
    return vec3(pow(l, 1.15) * 0.35, pow(l, 1.05) * 0.55, pow(l, 0.95) * 0.95);
}
vec3 preset_thermal(vec3 c) {
    float l = dot(c, vec3(0.2126,0.7152,0.0722));
    return clamp(vec3(l * 1.6, l * 0.4, (1.0 - l) * 1.4), 0.0, 1.0);
}
vec3 preset_nvg(vec3 c) {
    float l = dot(c, vec3(0.2126,0.7152,0.0722));
    return vec3(l * 0.15, l * 1.25, l * 0.15);
}

void main() {
    vec3 c = texture(u_tex, v_uv).rgb;
    vec3 g = c;
    int p = int(u_extra1 + 0.5);
    if (p == 1) g = preset_warm(c);
    else if (p == 2) g = preset_cold(c);
    else if (p == 3) g = preset_night(c);
    else if (p == 4) g = preset_thermal(c);
    else if (p == 5) g = preset_nvg(c);
    fragColor = vec4(mix(c, g, clamp(u_intensity, 0.0, 1.0)), 1.0);
}
)";

// ---------------- vignette ----------------
const char* shader_vignette = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform float u_intensity;
uniform float u_extra1;   // smoothness
uniform vec4  u_color;
out vec4 fragColor;

void main() {
    vec3 c = texture(u_tex, v_uv).rgb;
    vec2 d = v_uv - 0.5;
    float r = length(d) * 1.41421356;
    float s = mix(1.0, r, u_intensity);
    float v = smoothstep(0.75, 0.75 + u_extra1 * 0.6, s);
    c = mix(c, u_color.rgb, v * u_color.a);
    fragColor = vec4(c, 1.0);
}
)";

// ---------------- chromatic aberration ----------------
const char* shader_chromatic = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform float u_intensity;
out vec4 fragColor;

void main() {
    vec2 d = v_uv - 0.5;
    float r2 = dot(d, d);
    float k = u_intensity * 0.02;
    vec2 r_uv = v_uv + d * r2 * k;
    vec2 b_uv = v_uv - d * r2 * k;
    float R = texture(u_tex, r_uv).r;
    float G = texture(u_tex, v_uv).g;
    float B = texture(u_tex, b_uv).b;
    fragColor = vec4(R, G, B, 1.0);
}
)";

// ---------------- scanlines ----------------
const char* shader_scanlines = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec2  u_res;
uniform float u_intensity;
uniform float u_extra1;   // density
out vec4 fragColor;

void main() {
    vec3 c = texture(u_tex, v_uv).rgb;
    float y = v_uv.y * u_res.y / max(u_extra1, 1.0);
    float s = 0.85 + 0.15 * sin(y * 3.14159265 * 2.0);
    c *= mix(1.0, s, clamp(u_intensity, 0.0, 1.0));
    fragColor = vec4(c, 1.0);
}
)";

// ---------------- motion blur (приближённый) ----------------
// u_extra1 = samples, u_intensity = сила размытия.
const char* shader_motion_blur = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform float u_intensity;
uniform float u_extra1;
out vec4 fragColor;

void main() {
    int n = int(clamp(u_extra1, 2.0, 16.0));
    float strength = clamp(u_intensity, 0.0, 1.0) * 0.02;
    vec3 sum = vec3(0.0);
    for (int i = 0; i < 16; ++i) {
        if (i >= n) break;
        float t = float(i) / float(n - 1);
        vec2 off = (v_uv - 0.5) * strength * t;
        sum += texture(u_tex, v_uv + off).rgb;
    }
    sum /= float(n);
    fragColor = vec4(sum, 1.0);
}
)";

// ---------------- film grain ----------------
const char* shader_grain = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec2  u_res;
uniform float u_time;
uniform float u_intensity;
out vec4 fragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec3 c = texture(u_tex, v_uv).rgb;
    vec2 p = v_uv * u_res + vec2(u_time * 137.0);
    float g = hash(p) - 0.5;
    c += g * u_intensity * 0.35;
    fragColor = vec4(c, 1.0);
}
)";

} // namespace xim::features::postprocess