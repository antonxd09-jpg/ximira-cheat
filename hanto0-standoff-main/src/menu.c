#include "menu.h"

#define ITEM_COUNT 6

static const char *labels[ITEM_COUNT] = {
    "ESP Box","ESP Skeleton","ESP Lines",
    "ESP Name","ESP Distance","ESP Health"
};

static int *get_toggle(esp_flags_t *f, int i) {
    int *arr[ITEM_COUNT] = {
        &f->esp_box,&f->esp_skeleton,&f->esp_lines,
        &f->esp_name,&f->esp_dist,&f->esp_health
    };
    return arr[i];
}

void menu_init(menu_state_t *m) { m->open=0; m->selected=0; }

void menu_draw(WINDOW *w, menu_state_t *m, esp_flags_t *f) {
    if (!m->open) return;
    int sh, sw;
    getmaxyx(w, sh, sw); (void)sh;
    int mw=24, mh=ITEM_COUNT+4;
    int mx=sw-mw-2, my=2;
    wattron(w, COLOR_PAIR(6));
    for (int r=0;r<mh;r++) {
        mvwprintw(w,my+r,mx,"|");
        mvwprintw(w,my+r,mx+mw-1,"|");
    }
    for (int c=0;c<mw;c++) {
        mvwprintw(w,my,mx+c,"-");
        mvwprintw(w,my+mh-1,mx+c,"-");
    }
    mvwprintw(w,my,mx,"+"); mvwprintw(w,my,mx+mw-1,"+");
    mvwprintw(w,my+mh-1,mx,"+"); mvwprintw(w,my+mh-1,mx+mw-1,"+");
    wattroff(w, COLOR_PAIR(6));
    wattron(w, COLOR_PAIR(3)|A_BOLD);
    mvwprintw(w,my+1,mx+4,"--- ESP MENU ---");
    wattroff(w, COLOR_PAIR(3)|A_BOLD);
    for (int i=0;i<ITEM_COUNT;i++) {
        int sel=(i==m->selected);
        if (sel) wattron(w,A_REVERSE|COLOR_PAIR(3));
        else     wattron(w,COLOR_PAIR(6));
        mvwprintw(w,my+2+i,mx+2," [%c] %-16s",
                  *get_toggle(f,i)?'X':' ',labels[i]);
        if (sel) wattroff(w,A_REVERSE|COLOR_PAIR(3));
        else     wattroff(w,COLOR_PAIR(6));
    }
}

void menu_handle_key(menu_state_t *m, esp_flags_t *f, int key) {
    switch(key) {
        case KEY_UP:   m->selected=(m->selected-1+ITEM_COUNT)%ITEM_COUNT; break;
        case KEY_DOWN: m->selected=(m->selected+1)%ITEM_COUNT;             break;
        case ' ': case '\n': case KEY_ENTER:
            *get_toggle(f,m->selected)^=1;                                 break;
        case 'i': case 'I': case 27: m->open^=1;                          break;
        default: break;
    }
}
