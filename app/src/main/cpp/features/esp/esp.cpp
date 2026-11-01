#include "features/esp/esp.h"
#include "features/esp/config.h"
#include "render/draw.h"
#include "render/w2s.h"
#include "world/snapshot.h"
#include "util/log.h"

#include <cmath>
#include <cstdio>

namespace w2s = xim::render;

namespace xim::features::esp {

using namespace xim::render;

bool init() {
    if (!w2s::init()) { XERR("esp: w2s init failed"); return false; }
    XLOG("esp initialized");
    return true;
}

static void draw_box(ImDrawList* dl, const BoxCfg& cfg,
                     vec2 tl, vec2 br, bool enemy) {
    ImU32 col = draw::col_from_vec4(enemy ? cfg.color_enemy : cfg.color_ally);
    switch (cfg.style) {
        case 0:
        case 1:
            draw::rect(dl, tl, br, col, cfg.thickness);
            break;
        case 2:
            draw::rect_corner(dl, tl, br, col, cfg.thickness,
                              (br.y - tl.y) * 0.20f);
            break;
        case 3:
            dl->AddRect({tl.x, tl.y}, {br.x, br.y}, col, 4.0f, 0, cfg.thickness);
            break;
    }
    if (cfg.fill) {
        ImU32 fcol = (col & 0x00FFFFFF) |
                     ((ImU32)(cfg.fill_alpha * 255.0f) << 24);
        draw::rect_filled(dl, tl, br, fcol);
    }
}

static void draw_skeleton(ImDrawList* dl, const SkeletonCfg& cfg,
                          const world::PlayerSnapshot& s, bool enemy) {
    ImU32 col = draw::col_from_vec4(enemy ? cfg.color_enemy : cfg.color_ally);
    float th  = cfg.thickness;

    auto P = [&](int b, vec2& out) -> bool {
        if (b < 0 || b >= world::kBoneCount) return false;
        if (!s.bones[b].valid) return false;
        return w2s::world_to_screen(s.bones[b].pos, out);
    };

    vec2 head, neck, spine, hip;
    bool ok_h = P(world::kHead,  head);
    bool ok_n = P(world::kNeck,  neck);
    bool ok_s = P(world::kSpine, spine);
    bool ok_p = P(world::kHip,   hip);

    if (ok_h && ok_n) draw::line(dl, head, neck, col, th);
    if (ok_n && ok_s) draw::line(dl, neck, spine, col, th);
    if (ok_s && ok_p) draw::line(dl, spine, hip, col, th);

    if (cfg.show_arms) {
        vec2 a,b,c,d,e,f;
        bool aa = P(world::kShoulderL, a);
        bool bb = P(world::kElbowL,    b);
        bool cc = P(world::kHandL,     c);
        bool dd = P(world::kShoulderR, d);
        bool ee = P(world::kElbowR,    e);
        bool ff = P(world::kHandR,     f);
        if (aa && bb) draw::line(dl, a, b, col, th);
        if (bb && cc) draw::line(dl, b, c, col, th);
        if (dd && ee) draw::line(dl, d, e, col, th);
        if (ee && ff) draw::line(dl, e, f, col, th);
        if (ok_n && aa) draw::line(dl, neck, a, col, th);
        if (ok_n && dd) draw::line(dl, neck, d, col, th);
    }

    if (cfg.show_legs) {
        vec2 a,b,c,d;
        bool aa = P(world::kKneeL, a);
        bool bb = P(world::kFootL, b);
        bool cc = P(world::kKneeR, c);
        bool dd = P(world::kFootR, d);
        if (ok_p && aa) draw::line(dl, hip, a, col, th);
        if (aa && bb)   draw::line(dl, a, b, col, th);
        if (ok_p && cc) draw::line(dl, hip, c, col, th);
        if (cc && dd)   draw::line(dl, c, d, col, th);
    }

    if (cfg.head_circle && ok_h && ok_n) {
        float r = std::fabs(head.y - neck.y) * 0.65f;
        if (r < 4.0f) r = 4.0f;
        if (r > 40.0f) r = 40.0f;
        draw::circle(dl, head, r, col, th, 24);
    }
}

static void draw_name(ImDrawList* dl, const NameCfg& cfg,
                      vec2 anchor, const char* txt) {
    ImU32 col = draw::col_from_vec4(cfg.color);
    if (cfg.background) {
        ImVec2 sz = ImGui::CalcTextSize(txt);
        vec2 tl = {anchor.x - sz.x * 0.5f - 4, anchor.y - 2};
        vec2 br = {anchor.x + sz.x * 0.5f + 4, anchor.y + sz.y + 2};
        draw::rect_filled(dl, tl, br,
                          IM_COL32(0,0,0,(int)(cfg.bg_alpha*255)));
    }
    draw::text(dl, anchor, col, txt, true, true);
}

static void draw_distance(ImDrawList* dl, const DistanceCfg& cfg,
                          vec2 anchor, float meters) {
    char buf[32];
    if (cfg.unit == 0) snprintf(buf, sizeof(buf), "%.0fm", meters);
    else               snprintf(buf, sizeof(buf), "%.0fft", meters * 3.28084f);
    draw::text(dl, anchor, draw::col_from_vec4(cfg.color), buf, true, true);
}

static ImU32 hp_color(const HpBarCfg& cfg, float frac) {
    int pct = (int)(frac * 100.0f);
    if (pct <= cfg.threshold_low) return draw::col_from_vec4(cfg.color_low);
    if (pct <= cfg.threshold_mid) return draw::col_from_vec4(cfg.color_mid);
    return draw::col_from_vec4(cfg.color_high);
}

static void draw_hp(ImDrawList* dl, const HpBarCfg& cfg,
                    vec2 tl, vec2 br, float hp, float max_hp) {
    float frac = max_hp > 0 ? hp / max_hp : 0;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    ImU32 col = hp_color(cfg, frac);

    if (cfg.style == 0 || cfg.style == 2) {
        if (cfg.orientation == 1) {
            vec2 a = {tl.x - 6.0f, tl.y};
            vec2 b = {tl.x - 6.0f + cfg.width, br.y};
            draw::bar(dl, a, b, frac, col, IM_COL32(0,0,0,180));
        } else {
            vec2 a = {tl.x, br.y + 4.0f};
            vec2 b = {br.x, br.y + 4.0f + cfg.width};
            draw::bar(dl, a, b, frac, col, IM_COL32(0,0,0,180));
        }
    }
    if ((cfg.style == 1 || cfg.style == 2) && cfg.show_number) {
        char buf[16]; snprintf(buf, sizeof(buf), "%d", (int)hp);
        vec2 p = {(tl.x + br.x) * 0.5f, tl.y - 18.0f};
        draw::text(dl, p, col, buf, true, true);
    }
}

static void draw_armor(ImDrawList* dl, const ArmorBarCfg& cfg,
                       vec2 tl, vec2 br, float armor, float max_armor) {
    float frac = max_armor > 0 ? armor / max_armor : 0;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    vec2 a = {tl.x - 6.0f - cfg.width - 2.0f, tl.y};
    vec2 b = {tl.x - 6.0f - 2.0f, br.y};
    draw::bar(dl, a, b, frac, draw::col_from_vec4(cfg.color),
              IM_COL32(0,0,0,180));
}

static void draw_weapon(ImDrawList* dl, const WeaponCfg& cfg,
                        vec2 tl, vec2 br, const char* weapon,
                        int mag, int reserve) {
    char buf[48];
    if (cfg.show_ammo && (mag > 0 || reserve > 0))
        snprintf(buf, sizeof(buf), "%s  [%d/%d]", weapon, mag, reserve);
    else
        snprintf(buf, sizeof(buf), "%s", weapon);

    vec2 p;
    if (cfg.position == 0) p = {(tl.x + br.x) * 0.5f, tl.y - 34.0f};
    else                   p = {(tl.x + br.x) * 0.5f, br.y + 6.0f};
    draw::text(dl, p, draw::col_from_vec4(cfg.color), buf, true, true);
}

void draw(ImDrawList* dl) {
    EspConfig& cfg = config();
    if (!cfg.enabled) return;

    w2s::update_matrices();

    const world::SnapshotBuffer* snap = world::snapshot::get();
    if (!snap || snap->count == 0) return;

    const world::PlayerSnapshot* loc = world::snapshot::local();

    for (int i = 0; i < snap->count; ++i) {
        const world::PlayerSnapshot& s = snap->players[i];
        if (!s.valid || !s.alive) continue;
        if (cfg.team_check && loc && s.team == loc->team) continue;
        if (cfg.max_distance > 0.0f && s.distance > cfg.max_distance) continue;

        vec2 head2d, feet2d;
        bool ok_head = w2s::world_to_screen(s.head_pos, head2d);
        bool ok_feet = w2s::world_to_screen(s.origin,  feet2d);
        if (!ok_head || !ok_feet) continue;

        vec2 tl = { std::fmin(head2d.x, feet2d.x) - 22.0f, head2d.y - 12.0f };
        vec2 br = { std::fmax(head2d.x, feet2d.x) + 22.0f, feet2d.y + 4.0f };

        for (int b = 0; b < world::kBoneCount; ++b) {
            if (!s.bones[b].valid) continue;
            vec2 sp;
            if (!w2s::world_to_screen(s.bones[b].pos, sp)) continue;
            if (sp.x - 22.0f < tl.x) tl.x = sp.x - 22.0f;
            if (sp.x + 22.0f > br.x) br.x = sp.x + 22.0f;
            if (sp.y - 12.0f < tl.y) tl.y = sp.y - 12.0f;
            if (sp.y + 4.0f  > br.y) br.y = sp.y + 4.0f;
        }

        bool enemy = true;

        if (cfg.box.enabled)      draw_box(dl, cfg.box, tl, br, enemy);
        if (cfg.skeleton.enabled) draw_skeleton(dl, cfg.skeleton, s, enemy);
        if (cfg.hp.enabled)       draw_hp(dl, cfg.hp, tl, br, s.hp, s.max_hp);
        if (cfg.armor.enabled)    draw_armor(dl, cfg.armor, tl, br, s.armor, s.max_armor);
        if (cfg.name.enabled)     draw_name(dl, cfg.name,
                                            {(tl.x+br.x)*0.5f, tl.y-14.0f}, s.name);
        if (cfg.distance.enabled) draw_distance(dl, cfg.distance,
                                                {(tl.x+br.x)*0.5f, br.y+20.0f}, s.distance);
        if (cfg.weapon.enabled && s.weapon[0])
            draw_weapon(dl, cfg.weapon, tl, br, s.weapon, s.mag_ammo, s.reserve_ammo);
    }
}

} // namespace xim::features::esp