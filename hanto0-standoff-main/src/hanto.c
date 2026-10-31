#include "hanto.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static entity_t sim[8];
static int      sim_count = 0;
static float    tick      = 0.f;

static void build_skeleton(entity_t *e) {
    vec3_t o = e->origin;
    e->bones[BONE_HEAD]       = (vec3_t){ o.x,       o.y+1.8f, o.z };
    e->bones[BONE_NECK]       = (vec3_t){ o.x,       o.y+1.5f, o.z };
    e->bones[BONE_CHEST]      = (vec3_t){ o.x,       o.y+1.2f, o.z };
    e->bones[BONE_WAIST]      = (vec3_t){ o.x,       o.y+0.9f, o.z };
    e->bones[BONE_L_SHOULDER] = (vec3_t){ o.x-0.4f,  o.y+1.4f, o.z };
    e->bones[BONE_L_ELBOW]    = (vec3_t){ o.x-0.4f,  o.y+1.1f, o.z };
    e->bones[BONE_L_WRIST]    = (vec3_t){ o.x-0.4f,  o.y+0.8f, o.z };
    e->bones[BONE_R_SHOULDER] = (vec3_t){ o.x+0.4f,  o.y+1.4f, o.z };
    e->bones[BONE_R_ELBOW]    = (vec3_t){ o.x+0.4f,  o.y+1.1f, o.z };
    e->bones[BONE_R_WRIST]    = (vec3_t){ o.x+0.4f,  o.y+0.8f, o.z };
    e->bones[BONE_L_HIP]      = (vec3_t){ o.x-0.2f,  o.y+0.8f, o.z };
    e->bones[BONE_L_KNEE]     = (vec3_t){ o.x-0.2f,  o.y+0.45f,o.z };
    e->bones[BONE_L_ANKLE]    = (vec3_t){ o.x-0.2f,  o.y,      o.z };
    e->bones[BONE_R_HIP]      = (vec3_t){ o.x+0.2f,  o.y+0.8f, o.z };
    e->bones[BONE_R_KNEE]     = (vec3_t){ o.x+0.2f,  o.y+0.45f,o.z };
    e->bones[BONE_R_ANKLE]    = (vec3_t){ o.x+0.2f,  o.y,      o.z };
}

int hanto_init(void) {
    srand((unsigned)time(NULL));
    const char *names[] = {"alpha","bravo","charlie","delta","echo","foxtrot","golf","hotel"};
    sim_count = 5;
    for (int i = 0; i < sim_count; i++) {
        sim[i].origin = (vec3_t){
            (float)(rand() % 200 - 100),
            0.f,
            (float)(rand() % 200 - 100)
        };
        sim[i].health = 100.f;
        sim[i].team   = (i % 2) + 1;
        sim[i].alive  = 1;
        strncpy(sim[i].name, names[i], 31);
        build_skeleton(&sim[i]);
    }
    return 0;
}

void hanto_shutdown(void) { sim_count = 0; }

int hanto_get_entities(entity_t *buf, int max) {
    tick += 0.016f;
    for (int i = 0; i < sim_count; i++) {
        sim[i].origin.x += sinf(tick + i * 1.3f) * 0.3f;
        sim[i].origin.z += cosf(tick + i * 0.9f) * 0.3f;
        build_skeleton(&sim[i]);
    }
    int n = (sim_count < max) ? sim_count : max;
    for (int i = 0; i < n; i++) buf[i] = sim[i];
    return n;
}

view_t hanto_get_view(void) {
    return (view_t){
        .pos = { 0.f, 1.7f, 0.f },
        .yaw = tick * 10.f,
        .fov = 90.f,
        .w   = 120,
        .h   = 40
    };
}
