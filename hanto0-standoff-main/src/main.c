#include <ncurses.h>
#include <unistd.h>
#include "hanto.h"
#include "esp.h"
#include "visuals.h"
#include "menu.h"

int main(void) {
    if (hanto_init() != 0) return 1;
    initscr(); cbreak(); noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_RED,     -1);
    init_pair(2, COLOR_CYAN,    -1);
    init_pair(3, COLOR_GREEN,   -1);
    init_pair(4, COLOR_GREEN,   -1);
    init_pair(5, COLOR_YELLOW,  -1);
    init_pair(6, COLOR_WHITE,   -1);
    init_pair(7, COLOR_MAGENTA, -1);

    esp_flags_t  flags = {1,1,1,1,1,1};
    menu_state_t menu;
    menu_init(&menu);

    entity_t entities[32];
    int count = 0;

    while (1) {
        int ch = getch();
        if (ch=='q'||ch=='Q') break;
        if (ch=='i'||ch=='I'||ch==27) menu_handle_key(&menu,&flags,'i');
        else if (menu.open)           menu_handle_key(&menu,&flags,ch);

        count   = hanto_get_entities(entities, 32);
        view_t v = hanto_get_view();

        erase();
        int sh, sw;
        getmaxyx(stdscr, sh, sw);

        wattron(stdscr, COLOR_PAIR(7)|A_BOLD);
        mvprintw(0,0," ESP-DEMO  [I] menu  [SPACE] toggle  [Q] quit  ents:%d",count);
        wattroff(stdscr, COLOR_PAIR(7)|A_BOLD);

        draw_esp_frame(stdscr, entities, count, &v, &flags);
        menu_draw(stdscr, &menu, &flags);

        wattron(stdscr, COLOR_PAIR(6));
        mvaddch(sh/2, sw/2, '+');
        wattroff(stdscr, COLOR_PAIR(6));

        refresh();
        usleep(16000);
    }
    endwin();
    hanto_shutdown();
    return 0;
}
