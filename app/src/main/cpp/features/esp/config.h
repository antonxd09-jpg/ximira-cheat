#pragma once

#include <cstdint>

namespace xim::features::esp {

struct BoxCfg {
    bool  enabled     = true;
    int   style       = 0;        // 0=2D,1=3D,2=corner,3=rounded
    float thickness   = 1.5f;
    float color_enemy[4] = {1.0f, 0.30f, 0.30f, 1.0f};
    float color_ally [4] = {0.30f, 1.0f, 0.45f, 1.0f};
    bool  fill        = false;
    float fill_alpha  = 0.15f;
};

struct SkeletonCfg {
    bool  enabled     = true;
    float thickness   = 1.5f;
    float color_enemy[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float color_ally [4] = {0.30f, 1.0f, 0.45f, 1.0f};
    bool  head_circle = true;
    bool  show_arms   = true;
    bool  show_legs   = true;
};

struct NameCfg {
    bool  enabled     = true;
    int   font_size   = 14;
    float color[4]    = {1.0f, 1.0f, 1.0f, 1.0f};
    bool  background  = false;
    float bg_alpha    = 0.35f;
};

struct DistanceCfg {
    bool  enabled     = true;
    int   font_size   = 12;
    float color[4]    = {0.80f, 0.85f, 0.95f, 1.0f};
    int   unit        = 0;        // 0=m, 1=ft
};

struct HpBarCfg {
    bool  enabled     = true;
    int   style       = 2;        // 0=bar, 1=number, 2=bar+number
    int   orientation = 1;        // 0=horizontal, 1=vertical
    float width       = 3.0f;
    float color_high[4] = {0.30f, 1.0f, 0.45f, 1.0f};
    float color_mid [4] = {1.0f, 0.85f, 0.30f, 1.0f};
    float color_low [4] = {1.0f, 0.30f, 0.30f, 1.0f};
    int   threshold_mid = 60;
    int   threshold_low = 30;
    bool  show_number   = true;
};

struct ArmorBarCfg {
    bool  enabled     = false;
    float width       = 3.0f;
    float color[4]    = {0.30f, 0.65f, 1.0f, 1.0f};
};

struct WeaponCfg {
    bool  enabled     = true;
    bool  show_ammo   = true;
    int   font_size   = 12;
    float color[4]    = {0.85f, 0.85f, 0.90f, 1.0f};
    int   position    = 1;        // 0=top, 1=bottom
};

struct EspConfig {
    bool  enabled       = true;
    float max_distance  = 150.0f;
    bool  team_check    = true;   // не рисовать своих

    BoxCfg      box;
    SkeletonCfg skeleton;
    NameCfg     name;
    DistanceCfg distance;
    HpBarCfg    hp;
    ArmorBarCfg armor;
    WeaponCfg   weapon;
};

EspConfig& config();

} // namespace xim::features::esp