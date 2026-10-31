#include "esp.h"
#include <math.h>

#define DEG2RAD (3.14159265f / 180.f)

screen_pos_t world_to_screen(vec3_t world, view_t *v) {
    screen_pos_t r = { 0, 0, 0.f, 0 };
    float yr = v->yaw * DEG2RAD;
    float dx = world.x - v->pos.x;
    float dy = world.y - v->pos.y;
    float dz = world.z - v->pos.z;
    float cam_x =  dx * sinf(yr) + dz * (-cosf(yr));
    float cam_y =  dy;
    float cam_z =  dx * cosf(yr) + dz *   sinf(yr);
    if (cam_z < 0.1f) return r;
    float half_fov = (v->fov * 0.5f) * DEG2RAD;
    float scale    = (v->w  * 0.5f) / tanf(half_fov);
    r.sx      = (int)(v->w * 0.5f + (cam_x / cam_z) * scale);
    r.sy      = (int)(v->h * 0.5f - (cam_y / cam_z) * scale * 2.f);
    r.dist    = sqrtf(dx*dx + dy*dy + dz*dz);
    r.visible = 1;
    return r;
}

float vec3_dist(vec3_t a, vec3_t b) {
    float dx=a.x-b.x, dy=a.y-b.y, dz=a.z-b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}
