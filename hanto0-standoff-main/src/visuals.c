#include "visuals.h"
#include <math.h>
#include <stdio.h>

static void bres(WINDOW *w, int x0, int y0, int x1, int y1, chtype ch) {
    int dx=abs(x1-x0), dy=abs(y1-y0);
    int sx=(x0<x1)?1:-1, sy=(y0<y1)?1:-1;
    int err=dx-dy, e2, mw, mh;
    getmaxyx(w, mh, mw);
    for(;;) {
        if (x0>=0 && x0<mw && y0>=0 && y0<mh) mvwaddch(w,y0,x0,ch);
        if (x0==x1 && y0==y1) break;
        e2=2*err;
        if (e2>-dy){err-=dy;x0+=sx;}
        if (e2< dx){err+=dx;y0+=sy;}
    }
}

void draw_box(WINDOW *w, screen_pos_t head, screen_pos_t feet, int team) {
    if (!head.visible || !feet.visible) return;
    int h=abs(feet.sy-head.sy);
    int wd=h/3; if(wd<2) wd=2;
    int x1=head.sx-wd, x2=head.sx+wd, y1=head.sy, y2=feet.sy;
    wattron(w, COLOR_PAIR(team));
    bres(w,x1,y1,x2,y1,'-'); bres(w,x1,y2,x2,y2,'-');
    bres(w,x1,y1,x1,y2,'|'); bres(w,x2,y1,x2,y2,'|');
    mvwaddch(w,y1,x1,'+'); mvwaddch(w,y1,x2,'+');
    mvwaddch(w,y2,x1,'+'); mvwaddch(w,y2,x2,'+');
    wattroff(w, COLOR_PAIR(team));
}

void draw_skeleton(WINDOW *w, entity_t *e, view_t *v) {
    screen_pos_t sp[BONE_COUNT];
    for (int i=0;i<BONE_COUNT;i++) sp[i]=world_to_screen(e->bones[i],v);
    wattron(w, COLOR_PAIR(3));
#define S(a,b,c) bres(w,sp[a].sx,sp[a].sy,sp[b].sx,sp[b].sy,(c))
    S(BONE_HEAD,BONE_NECK,'o');
    S(BONE_NECK,BONE_CHEST,'|');
    S(BONE_CHEST,BONE_WAIST,'|');
    S(BONE_NECK,BONE_L_SHOULDER,'\\');
    S(BONE_L_SHOULDER,BONE_L_ELBOW,'\\');
    S(BONE_L_ELBOW,BONE_L_WRIST,'\\');
    S(BONE_NECK,BONE_R_SHOULDER,'/');
    S(BONE_R_SHOULDER,BONE_R_ELBOW,'/');
    S(BONE_R_ELBOW,BONE_R_WRIST,'/');
    S(BONE_WAIST,BONE_L_HIP,'\\');
    S(BONE_L_HIP,BONE_L_KNEE,'|');
    S(BONE_L_KNEE,BONE_L_ANKLE,'|');
    S(BONE_WAIST,BONE_R_HIP,'/');
    S(BONE_R_HIP,BONE_R_KNEE,'|');
    S(BONE_R_KNEE,BONE_R_ANKLE,'|');
#undef S
    wattroff(w, COLOR_PAIR(3));
}

void draw_line(WINDOW *w, int sw, int sh, screen_pos_t tgt, int team) {
    if (!tgt.visible) return;
    wattron(w, COLOR_PAIR(team));
    bres(w, sw/2, sh-1, tgt.sx, tgt.sy, '.');
    wattroff(w, COLOR_PAIR(team));
}

void draw_esp_frame(WINDOW *w, entity_t *ents, int count,
                    view_t *v, esp_flags_t *flags) {
    int sh, sw;
    getmaxyx(w, sh, sw);
    for (int i=0;i<count;i++) {
        entity_t *e=&ents[i];
        if (!e->alive) continue;
        screen_pos_t head  = world_to_screen(e->bones[BONE_HEAD],    v);
        screen_pos_t feet  = world_to_screen(e->bones[BONE_L_ANKLE], v);
        screen_pos_t chest = world_to_screen(e->bones[BONE_CHEST],   v);
        if (!chest.visible) continue;
        int tc=(e->team==1)?1:2;
        if (flags->esp_lines)    draw_line(w,sw,sh,chest,tc);
        if (flags->esp_box)      draw_box(w,head,feet,tc);
        if (flags->esp_skeleton) draw_skeleton(w,e,v);
        if (flags->esp_name && chest.sx>=0 && chest.sx<sw-10
                             && chest.sy>=0 && chest.sy<sh) {
            wattron(w,COLOR_PAIR(tc));
            mvwprintw(w,chest.sy,chest.sx,"[%s]",e->name);
            wattroff(w,COLOR_PAIR(tc));
        }
        if (flags->esp_dist && chest.sx>=0 && chest.sx<sw-8
                             && chest.sy+1<sh) {
            wattron(w,COLOR_PAIR(tc));
            mvwprintw(w,chest.sy+1,chest.sx,"%.0fm",chest.dist);
            wattroff(w,COLOR_PAIR(tc));
        }
        if (flags->esp_health && head.sx>=0 && head.sx<sw-8
                               && head.sy-1>=0 && head.sy-1<sh) {
            int hp=(int)e->health;
            int hcp=hp>60?3:hp>30?5:1;
            wattron(w,COLOR_PAIR(hcp));
            mvwprintw(w,head.sy-1,head.sx,"HP:%d",hp);
            wattroff(w,COLOR_PAIR(hcp));
        }
    }
}
