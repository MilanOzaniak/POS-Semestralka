#include "draw.h"
#include <ncurses.h>


// vykreslovanie hlavy hada, lokalny hrac ma @ ostatni maju O
char snake_head_char(int idx, int is_me) {
    (void)idx; // toto tu je lebo bez toho to dava warning
    return is_me ? '@' : 'O';
}

static void put_cell(int oy, int ox, uint8_t x, uint8_t y, char ch) {
    int sx = ox + (int)x * 2; // sirka x bunky ( 2 krat vacsia kvoli tomu ze to islo rycheljsie hore/dole
    int sy = oy + (int)y;     // sirka y bunky
    mvaddch(sy, sx, ch);
    mvaddch(sy, sx + 1, ' ');
}

void draw(const state_t *st, uint32_t my_id) {
    erase();
    // horne menu, kde su vsetci hraci a ich skore a stav
    mvprintw(0, 0, "Players: %u alive: %u  tick:%u  %s%s",
             st->player_count, st->alive_count, st->tick,
             st->lobby ? "LOBBY" : "GAME",
             st->round_over ? " (ROUND OVER)" : "");
    // offset mapy
    int ox = 1, oy = 2;
    // rozmery mapy
    int W = (int)st->w * 2;
    int H = (int)st->h;

    // vykreslenie borderu mapy horny/dolny a pravy/lavy
    for (int x = 0; x < W + 2; x++) {
        mvaddch(oy - 1, ox + x - 1, '#');
        mvaddch(oy + H, ox + x - 1, '#');
    }
    for (int y = 0; y < H; y++) {
        mvaddch(oy + y, ox - 1, '#');
        mvaddch(oy + y, ox + W, '#');
    }

    // dolne menu
    if (st->lobby) {
        mvprintw(oy + H + 1, 0, "Lobby: press 'g' to start. Join more players anytime.");
        mvprintw(oy + H + 2, 0, "Controls: arrows/WASD move | g start | q quit");
    } else if (st->round_over) {
        mvprintw(oy + H + 1, 0, "Round over. Back to lobby.");
        mvprintw(oy + H + 2, 0, "Controls: g start | q quit");
    } else {
        mvprintw(oy + H + 1, 0, "Controls: arrows/WASD move | q quit");
    }

    // vykreslenie prekazok
    for (uint8_t y = 0; y < st->h; y++) {
      for (uint8_t x = 0; x < st->w; x++) {
        if (st->blocks[y][x]) put_cell(oy, ox, x, y, '#');
      }
    }

    // vykreslenie ovocia
    for (uint8_t i = 0; i < st->fruit_count; i++) {
        put_cell(oy, ox, st->fruits[i].x, st->fruits[i].y, 'o');
    }

    // vykreslenie hracov
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const player_state_t *p = &st->players[i];
        if (!p->active) continue;

        int is_me = (p->id == my_id);
        // stav hraca
        mvprintw(1, 0 + i*18, "P%d %s sc=%u", i+1, p->alive ? "ALIVE" : "DEAD ", p->score);

        if (!p->alive) continue;
        // segmenty hada
        for (uint16_t k = 0; k < p->len && k < MAX_SNAKE; k++) {
            char ch = (k == 0) ? snake_head_char(i, is_me) : 'x';
            put_cell(oy, ox, p->seg[k].x, p->seg[k].y, ch);
        }
    }

    refresh();
}
