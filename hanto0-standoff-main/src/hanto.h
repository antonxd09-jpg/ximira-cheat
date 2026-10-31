#pragma once
#include <stdint.h>

typedef struct { float x, y, z; } vec3_t;

typedef enum {
    BONE_HEAD = 0, BONE_NECK, BONE_CHEST, BONE_WAIST,
    BONE_L_SHOULDER, BONE_L_ELBOW, BONE_L_WRIST,
    BONE_R_SHOULDER, BONE_R_ELBOW, BONE_R_WRIST,
    BONE_L_HIP, BONE_L_KNEE, BONE_L_ANKLE,
    BONE_R_HIP, BONE_R_KNEE, BONE_R_ANKLE,
    BONE_COUNT
} bone_id_t;

typedef struct {
    vec3_t origin;
    vec3_t bones[BONE_COUNT];
    float  health;
    int    team;
    int    alive;
    char   name[32];
} entity_t;

typedef struct {
    vec3_t pos;
    float  yaw;
    float  fov;
    int    w, h;
} view_t;

int    hanto_init(void);
void   hanto_shutdown(void);
int    hanto_get_entities(entity_t *buf, int max);
view_t hanto_get_view(void);
