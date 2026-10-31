#pragma once
#include <ncurses.h>
#include "visuals.h"

typedef struct {
    int open;
    int selected;
} menu_state_t;

void menu_init      (menu_state_t *m);
void menu_draw      (WINDOW *w, menu_state_t *m, esp_flags_t *flags);
void menu_handle_key(menu_state_t *m, esp_flags_t *flags, int key);
