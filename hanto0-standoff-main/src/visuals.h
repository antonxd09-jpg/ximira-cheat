#pragma once
#include <ncurses.h>
#include "hanto.h"
#include "esp.h"

typedef struct {
    int esp_box;
    int esp_skeleton;
    int esp_lines;
    int esp_name;
    int esp_dist;
    int esp_health;
} esp_flags_t;

void draw_box      (WINDOW *w, screen_pos_t head, screen_pos_t feet, int team);
void draw_skeleton (WINDOW *w, entity_t *e, view_t *v);
void draw_line     (WINDOW *w, int sw, int sh, screen_pos_t target, int team);
void draw_esp_frame(WINDOW *w, entity_t *ents, int count,
                    view_t *v, esp_flags_t *flags);
