#include "menu.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

static uint16_t clamp_u16(long v, uint16_t lo, uint16_t hi, uint16_t defv) {
    if (v < (long)lo || v > (long)hi) return defv;
    return (uint16_t)v;
}

static uint8_t clamp_u8(long v, uint8_t lo, uint8_t hi, uint8_t defv) {
    if (v < (long)lo || v > (long)hi) return defv;
    return (uint8_t)v;
}

static long read_num_at(int y, int x, long defv) {
    char buf[32];

    int old = is_nodelay(stdscr);
    nodelay(stdscr, FALSE);

    move(y, x);
    clrtoeol();
    refresh();

    echo();
    curs_set(1);

    buf[0] = '\0';
    if (mvgetnstr(y, x, buf, (int)sizeof(buf) - 1) == ERR) {
        buf[0] = '\0';
    }

    noecho();
    curs_set(0);

    nodelay(stdscr, old);

    if (buf[0] == '\0') return defv;
    return strtol(buf, NULL, 10);
}

static void draw_menu(const create_game_t *cfg, int sel) {
    erase();

    mvprintw(1, 2, "New Game Setup (host)");

    mvprintw(3, 2, "%c Width:  %u",  (sel==0?'>':' '), (unsigned)cfg->w);
    mvprintw(4, 2, "%c Height: %u",  (sel==1?'>':' '), (unsigned)cfg->h);
    mvprintw(5, 2, "%c Obstacles: %s", (sel==2?'>':' '), cfg->obstacles ? "ON" : "OFF");
    mvprintw(6, 2, "%c Map type: %s", (sel==3?'>':' '), (cfg->map_type==MAP_RANDOM) ? "RANDOM" : "EMPTY");
    mvprintw(7, 2, "%c Mode: %s", (sel==4?'>':' '), (cfg->mode==MODE_TIMED) ? "TIMED" : "STANDARD");

    mvprintw(8, 2, "%c Time limit (sec): ", (sel==5?'>':' '));
    if (cfg->mode == MODE_TIMED) {
        printw("%u", (unsigned)cfg->time_limit);
    } else {
        printw("-");
    }

    mvprintw(9, 2, "%c Obstacle density (pct): %u", (sel==6?'>':' '), (unsigned)cfg->obstacle_density);

    mvprintw(11, 2, "Enter=create  q=quit  arrows=move  space=toggle  e=edit number");
    refresh();
}

int menu_run(menu_result_t *out, uint8_t is_host) {
    memset(out, 0, sizeof(*out));

    create_game_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.map_type = MAP_EMPTY;
    cfg.obstacles = 0;
    cfg.w = WORLD_W;
    cfg.h = WORLD_H;
    cfg.mode = MODE_STANDARD;
    cfg.time_limit = 120;
    cfg.obstacle_density = 18;

    if (!is_host) {
        out->cfg = cfg;
        return 1;
    }

    int sel = 0;
    const int max_sel = 6;

    for (;;) {
        if (cfg.w < 10) cfg.w = 10;
        if (cfg.h < 10) cfg.h = 10;
        if (cfg.w > WORLD_W) cfg.w = WORLD_W;
        if (cfg.h > WORLD_H) cfg.h = WORLD_H;
        if (cfg.time_limit < 5) cfg.time_limit = 5;
        if (cfg.time_limit > 3600) cfg.time_limit = 3600;
        if (cfg.obstacle_density > 60) cfg.obstacle_density = 60;

        draw_menu(&cfg, sel);

        int ch = getch();
        if (ch == 'q' || ch == 'Q') return 0;

        if (ch == KEY_UP) {
            if (sel > 0) sel--;
        } else if (ch == KEY_DOWN) {
            if (sel < max_sel) sel++;
        } else if (ch == ' ' || ch == KEY_LEFT || ch == KEY_RIGHT) {
            if (sel == 2) cfg.obstacles = (uint8_t)!cfg.obstacles;
            else if (sel == 3) cfg.map_type = (cfg.map_type == MAP_EMPTY) ? MAP_RANDOM : MAP_EMPTY;
            else if (sel == 4) cfg.mode = (cfg.mode == MODE_STANDARD) ? MODE_TIMED : MODE_STANDARD;
        } else if (ch == 'e' || ch == 'E') {
            if (sel == 0) {
                long v = read_num_at(3, 14, cfg.w);
                cfg.w = clamp_u8(v, 10, WORLD_W, cfg.w);
            } else if (sel == 1) {
                long v = read_num_at(4, 14, cfg.h);
                cfg.h = clamp_u8(v, 10, WORLD_H, cfg.h);
            } else if (sel == 5 && cfg.mode == MODE_TIMED) {
                long v = read_num_at(8, 22, cfg.time_limit);
                cfg.time_limit = clamp_u16(v, 5, 3600, cfg.time_limit);
            } else if (sel == 6) {
                long v = read_num_at(9, 28, cfg.obstacle_density);
                cfg.obstacle_density = clamp_u8(v, 0, 60, cfg.obstacle_density);
            }
        } else if (ch == '\n' || ch == KEY_ENTER) {
            out->cfg = cfg;
            return 1;
        }
    }
}

