#include "features/glow/glow.h"
#include "render/draw.h"
#include "render/w2s.h"
#include "world/snapshot.h"
#include "util/log.h"

#include <cmath>
#include <chrono>

namespace w2s = xim::render;

namespace xim::features::glow {

using namespace xim::render;
using namespace xim::world;

static Config g_cfg;
Config& config() { return g_cfg; }

bool init() {
    XLOG("glow: ready (imgui drawlist backend)");
    return true;
}

void shutdown() {}

// ---- helpers ----

static ImU32 to_u32(const float c[4], float alpha_mul) {
    float a = c[3] * alpha_mul;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    return IM_COL32(
        static_cast<int>(c[0] * 255.0f),
        static_cast<int>(c[1] * 255.0f),
        static_cast<int>(c[2] * 255.0f),
        static_cast<int>(a * 255.0f));
}

static void pick_color(const PlayerSnapshot& s, const PlayerSnapshot* loc,
                       bool team_colors, const Colors& pal, float out[4]) {
    if (loc && s.ptr == loc->ptr) {
        std::memcpy(out, pal.local, sizeof(float) * 4);
        return;
    }
    if (!team_colors) {
        std::memcpy(out, pal.enemy, sizeof(float) * 4);
        return;
    }
    if (loc && s.team != 0 && s.team == loc->team) {
        std::memcpy(out, pal.ally, sizeof(float) * 4);
        return;
    }
    std::memcpy(out, pal.enemy, sizeof(float) * 4);
}

// ---- skeleton projection ----
// возвращает true если хотя бы 2 кости спроецировались
static bool project_bones(const PlayerSnapshot& s,
                          vec2 out[kBoneCount],
                          bool ok[kBoneCount]) {
    int valid = 0;
    for (int i = 0; i < kBoneCount; ++i) {
        ok[i] = false;
        if (!s.bones[i].valid) continue;
        vec2 sp{};
        if (!w2s::world_to_screen(s.bones[i].pos, sp)) continue;
        out[i] = sp;
        ok[i]  = true;
        ++valid;
    }
    return valid >= 2;
}

// ---- skeleton edges ----
// какие индексы соединяем линиями
struct Edge { int a; int b; };
static constexpr Edge kEdges[] = {
    {kHead, kNeck}, {kNeck, kSpine}, {kSpine, kHip},

    {kNeck, kShoulderL}, {kShoulderL, kElbowL}, {kElbowL, kHandL},
    {kNeck, kShoulderR}, {kShoulderR, kElbowR}, {kElbowR, kHandR},

    {kHip, kKneeL}, {kKneeL, kFootL},
    {kHip, kKneeR}, {kKneeR, kFootR},
};

// ---- one player ----
static void draw_player(ImDrawList* dl,
                        const PlayerSnapshot& s,
                        const PlayerSnapshot* loc,
                        float now_sec) {
    const Config& cfg = g_cfg;

    // team / color
    float color[4]{};
    pick_color(s, loc, cfg.teamColors.load(std::memory_order_relaxed),
               cfg.colors, color);

    // pulse
    float pulse = 1.0f;
    if (cfg.pulse.load(std::memory_order_relaxed)) {
        float speed = cfg.pulseSpeed.load(std::memory_order_relaxed);
        pulse = 0.65f + 0.35f * (0.5f + 0.5f * std::sin(now_sec * speed * 6.28318f));
    }

    const float intensity = cfg.intensity.load(std::memory_order_relaxed);
    const float thickness = cfg.thickness.load(std::memory_order_relaxed);

    // ---- bounding box из костей ----
    vec2 head2d{}, feet2d{};
    bool ok_head = w2s::world_to_screen(s.head_pos, head2d);
    bool ok_feet = w2s::world_to_screen(s.origin,  feet2d);

    // fallback: если нет head/origin — построим bbox из костей
    if (!ok_head || !ok_feet) {
        vec2 minp{1e9f, 1e9f}, maxp{-1e9f, -1e9f};
        int count = 0;
        for (int i = 0; i < kBoneCount; ++i) {
            if (!s.bones[i].valid) continue;
            vec2 sp{};
            if (!w2s::world_to_screen(s.bones[i].pos, sp)) continue;
            minp.x = std::fmin(minp.x, sp.x); minp.y = std::fmin(minp.y, sp.y);
            maxp.x = std::fmax(maxp.x, sp.x); maxp.y = std::fmax(maxp.y, sp.y);
            ++count;
        }
        if (count >= 2) {
            head2d = minp;
            feet2d = maxp;
            ok_head = ok_feet = true;
        }
    }
    if (!ok_head || !ok_feet) return;

    // ---- 1) если включён скелет — рисуем его (внешний + внутренний проход) ----
    if (cfg.drawSkeleton.load(std::memory_order_relaxed)) {
        vec2 bpts[kBoneCount]{};
        bool bok [kBoneCount]{};
        if (project_bones(s, bpts, bok)) {
            // внешний проход
            ImU32 outer = to_u32(color, 0.35f * intensity * pulse);
            float outer_th = thickness + 4.0f;
            for (const Edge& e : kEdges) {
                if (!bok[e.a] || !bok[e.b]) continue;
                dl->AddLine({bpts[e.a].x, bpts[e.a].y},
                            {bpts[e.b].x, bpts[e.b].y}, outer, outer_th);
            }
            // внутренний яркий
            ImU32 inner = to_u32(color, 1.0f * pulse);
            for (const Edge& e : kEdges) {
                if (!bok[e.a] || !bok[e.b]) continue;
                dl->AddLine({bpts[e.a].x, bpts[e.a].y},
                            {bpts[e.b].x, bpts[e.b].y}, inner, thickness);
            }
            // голова кружком
            if (bok[kHead] && bok[kNeck]) {
                float r = std::fabs(bpts[kHead].y - bpts[kNeck].y) * 0.65f;
                if (r < 3.0f) r = 3.0f;
                if (r > 32.0f) r = 32.0f;
                dl->AddCircle({bpts[kHead].x, bpts[kHead].y}, r + 3.0f, outer, 28, thickness + 3.0f);
                dl->AddCircle({bpts[kHead].x, bpts[kHead].y}, r, inner, 28, thickness);
            }
        }
    }

    // ---- 2) bounding box glow ----
    if (cfg.drawBox.load(std::memory_order_relaxed)) {
        // бокс от head до feet с боковым паддингом
        float top    = std::fmin(head2d.y, feet2d.y) - 12.0f;
        float bottom = std::fmax(head2d.y, feet2d.y) + 4.0f;
        float cx     = (head2d.x + feet2d.x) * 0.5f;
        float half   = (bottom - top) * 0.30f;   // стандартное соотношение бокса
        float left   = cx - half;
        float right  = cx + half;

        ImU32 outer = to_u32(color, 0.35f * intensity * pulse);
        ImU32 inner = to_u32(color, 0.90f * pulse);

        // внешний (толстый, полупрозрачный)
        dl->AddRect({left, top}, {right, bottom}, outer,
                    0.0f, 0, thickness + 5.0f);
        // средний (полупро)
        dl->AddRect({left, top}, {right, bottom}, to_u32(color, 0.60f * pulse),
                    0.0f, 0, thickness + 2.0f);
        // внутренний яркий
        dl->AddRect({left, top}, {right, bottom}, inner,
                    0.0f, 0, thickness);
    }
}

// ---- public update ----
void update(ImDrawList* dl, int screen_w, int screen_h) {
    if (!dl) return;
    if (!g_cfg.enabled.load(std::memory_order_relaxed)) return;

    const SnapshotBuffer* snap = snapshot::get();
    if (!snap || snap->count <= 0) return;

    const PlayerSnapshot* loc = snapshot::local();

    // время для пульса
    static auto start = std::chrono::steady_clock::now();
    float now_sec = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - start).count();

    const float max_dist = g_cfg.maxDistance.load(std::memory_order_relaxed);

    for (int i = 0; i < snap->count; ++i) {
        const PlayerSnapshot& s = snap->players[i];
        if (!s.valid || !s.alive) continue;

        if (max_dist > 0.0f && s.distance > max_dist) continue;

        draw_player(dl, s, loc, now_sec);
    }
}

} // namespace xim::features::glow