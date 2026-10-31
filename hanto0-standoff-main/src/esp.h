#pragma once
#include "hanto.h"

typedef struct {
    int   sx, sy;
    float dist;
    int   visible;
} screen_pos_t;

screen_pos_t world_to_screen(vec3_t world, view_t *v);
float        vec3_dist(vec3_t a, vec3_t b);
