#include "draw.h"
#include <ncurses.h>

char snake_head_char(int idx, int is_me) {
    (void)idx;
    return is_me ? '@' : 'O';
}

static void put_cell(int oy, int ox, uint8_t x, uint8_t y, char ch) {
    int sx = ox + (int)x * 2;
    int sy = oy + (int)y;
    mvaddch(sy, sx, ch);
    mvaddch(sy, sx + 1, ' ');
}

void draw(const state_t *st, uint32_t my_id) {
    erase();

    mvprintw(0, 0, "Players: %u alive: %u  tick:%u  %s%s",
             st->player_count, st->alive_count, st->tick,
             st->lobby ? "LOBBY" : "GAME",
             st->round_over ? " (ROUND OVER)" : "");

    int ox = 1, oy = 2;
    int W = (int)st->w * 2;
    int H = (int)st->h;

    for (int x = 0; x < W + 2; x++) {
        mvaddch(oy - 1, ox + x - 1, '#');
        mvaddch(oy + H, ox + x - 1, '#');
    }
    for (int y = 0; y < H; y++) {
        mvaddch(oy + y, ox - 1, '#');
        mvaddch(oy + y, ox + W, '#');
    }

    if (st->lobby) {
        mvprintw(oy + H + 1, 0, "Lobby: press 'g' to start. Join more players anytime.");
        mvprintw(oy + H + 2, 0, "Controls: arrows/WASD move | g start | r restart | q quit");
    } else if (st->round_over) {
        mvprintw(oy + H + 1, 0, "Round over. Back to lobby.");
        mvprintw(oy + H + 2, 0, "Controls: g start | q quit");
    } else {
        mvprintw(oy + H + 1, 0, "Controls: arrows/WASD move | q quit");
    }

    for (uint8_t i = 0; i < st->fruit_count; i++) {
        put_cell(oy, ox, st->fruits[i].x, st->fruits[i].y, 'o');
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        const player_state_t *p = &st->players[i];
        if (!p->active) continue;

        int is_me = (p->id == my_id);

        mvprintw(1, 0 + i*18, "P%d id=%u %s sc=%u",
                 i+1, p->id, p->alive ? "ALIVE" : "DEAD ", p->score);

        if (!p->alive) continue;

        for (uint16_t k = 0; k < p->len && k < MAX_SNAKE; k++) {
            char ch = (k == 0) ? snake_head_char(i, is_me) : 'x';
            put_cell(oy, ox, p->seg[k].x, p->seg[k].y, ch);
        }
    }

    refresh();
}
