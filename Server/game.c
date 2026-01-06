#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

static int pos_eq(pos_t a, pos_t b) {
    return a.x == b.x && a.y == b.y;
}

static int any_snake_contains(const player_t players[MAX_PLAYERS], pos_t p) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active || !players[i].alive) continue;
        for (uint16_t k = 0; k < players[i].snake_len; k++) {
            if (pos_eq(players[i].snake[k], p)) return 1;
        }
    }
    return 0;
}

static int fruit_clash(const game_t *G, int skip_idx, pos_t p) {
    for (uint8_t i = 0; i < G->fruit_count; i++) {
        if ((int)i == skip_idx) continue;
        if (pos_eq(G->fruits[i], p)) return 1;
    }
    return 0;
}

static void respawn_one_fruit(game_t *G, const player_t players[MAX_PLAYERS], int idx) {
    for (int tries = 0; tries < 50000; tries++) {
        pos_t p = { (uint8_t)(rand() % G->map.w), (uint8_t)(rand() % G->map.h) };
        if (map_is_blocked(&G->map, p)) continue;
        if (any_snake_contains(players, p)) continue;
        if (fruit_clash(G, idx, p)) continue;
        G->fruits[idx] = p;
        return;
    }
}

static void spawn_all_fruits(game_t *G, const player_t players[MAX_PLAYERS]) {
    G->fruit_count = MAX_FRUITS;
    for (uint8_t i = 0; i < G->fruit_count; i++) respawn_one_fruit(G, players, (int)i);
}

static int collides(const game_t *G, const player_t players[MAX_PLAYERS], const player_t *me, pos_t nh, int grow) {
    if (map_is_blocked(&G->map, nh)) return 1;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        const player_t *pl = &players[i];
        if (!pl->active || !pl->alive) continue;
        for (uint16_t k = 0; k < pl->snake_len; k++) {
            if (pl == me && !grow && k == pl->snake_len - 1) continue;
            if (pos_eq(pl->snake[k], nh)) return 1;
        }
    }
    return 0;
}

void game_init(game_t *G) {
    memset(G, 0, sizeof(*G));
    srand((unsigned)time(NULL));
    G->lobby = 1;

    create_game_t def;
    memset(&def, 0, sizeof(def));
    def.map_type = MAP_EMPTY;
    def.obstacles = 0;
    def.w = WORLD_W;
    def.h = WORLD_H;
    def.mode = MODE_STANDARD;
    def.time_limit = 120;
    def.obstacle_density = 18;
    game_apply_config(G, &def);
}

void game_apply_config(game_t *G, const create_game_t *cfg) {
    G->cfg = *cfg;
    G->cfg_set = 1;

    uint8_t w = cfg->w;
    uint8_t h = cfg->h;
    if (w < 10) w = 10;
    if (h < 10) h = 10;
    if (w > WORLD_W) w = WORLD_W;
    if (h > WORLD_H) h = WORLD_H;

    map_clear(&G->map, w, h);

    if (cfg->obstacles) {
        if (cfg->map_type == MAP_RANDOM) {
            map_random_obstacles(&G->map, (uint32_t)now_ms(), cfg->obstacle_density);
        }
    }

    G->lobby = 1;
    G->start_req = 0;
    G->restart_req = 0;
}

void game_init_round(game_t *G, player_t players[MAX_PLAYERS]) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active) continue;

        players[i].alive = 1;
        players[i].last_dir = DIR_RIGHT;
        players[i].snake_len = 4;

        uint8_t bx = 5, by = 5;
        if (i == 1) { bx = (uint8_t)(G->map.w - 6); by = 5; }
        if (i == 2) { bx = 5; by = (uint8_t)(G->map.h - 6); }
        if (i == 3) { bx = (uint8_t)(G->map.w - 6); by = (uint8_t)(G->map.h - 6); }

        for (uint16_t k = 0; k < players[i].snake_len; k++) {
            players[i].snake[k].x = (uint8_t)(bx - k);
            players[i].snake[k].y = by;
        }
    }

    G->game_start_ms = now_ms();
    if (G->cfg.mode == MODE_TIMED) {
        G->game_end_ms = G->game_start_ms + (uint64_t)G->cfg.time_limit * 1000ULL;
    } else {
        G->game_end_ms = 0;
    }

    spawn_all_fruits(G, players);
}

static void move_one(game_t *G, player_t players[MAX_PLAYERS], player_t *pl) {
    if (!pl->active || !pl->alive) return;

    pos_t nh = pl->snake[0];
    if (pl->last_dir == DIR_UP)    nh.y--;
    if (pl->last_dir == DIR_DOWN)  nh.y++;
    if (pl->last_dir == DIR_LEFT)  nh.x--;
    if (pl->last_dir == DIR_RIGHT) nh.x++;

    if (nh.x >= G->map.w || nh.y >= G->map.h) { pl->alive = 0; return; }

    int ate = -1;
    for (uint8_t i = 0; i < G->fruit_count; i++) {
        if (pos_eq(G->fruits[i], nh)) { ate = (int)i; break; }
    }

    if (collides(G, players, pl, nh, ate >= 0)) { pl->alive = 0; return; }

    if (ate >= 0 && pl->snake_len < MAX_SNAKE) {
        for (int k = (int)pl->snake_len; k > 0; k--) pl->snake[k] = pl->snake[k-1];
        pl->snake[0] = nh;
        pl->snake_len++;
        pl->score += 10;
        respawn_one_fruit(G, players, ate);
    } else {
        for (int k = (int)pl->snake_len - 1; k > 0; k--) pl->snake[k] = pl->snake[k-1];
        pl->snake[0] = nh;
    }
}

void game_move_all(game_t *G, player_t players[MAX_PLAYERS]) {
    if (G->cfg.mode == MODE_TIMED && G->game_end_ms > 0) {
        if (now_ms() >= G->game_end_ms) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].active) players[i].alive = 0;
            }
            return;
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) move_one(G, players, &players[i]);
}

void game_make_state(const game_t *G, const player_t players[MAX_PLAYERS], state_t *out) {
    memset(out, 0, sizeof(*out));
    out->tick = G->tick;
    out->w = G->map.w;
    out->h = G->map.h;
    out->lobby = G->lobby;

    uint8_t pc = 0, ac = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active) pc++;
        if (players[i].active && players[i].alive) ac++;
    }
    out->player_count = pc;
    out->alive_count = ac;
    out->round_over = (!G->lobby && ac == 0) ? 1 : 0;

    out->mode = G->cfg.mode;

    if (G->cfg.mode == MODE_TIMED && G->game_end_ms > 0) {
        uint64_t n = now_ms();
        if (n >= G->game_end_ms) out->time_left = 0;
        else {
            uint64_t left = (G->game_end_ms - n) / 1000ULL;
            if (left > 65535ULL) left = 65535ULL;
            out->time_left = (uint16_t)left;
        }
    } else {
        out->time_left = 0;
    }

    out->fruit_count = G->fruit_count;
    for (uint8_t i = 0; i < G->fruit_count; i++) out->fruits[i] = G->fruits[i];

    for (int i = 0; i < MAX_PLAYERS; i++) {
        out->players[i].id = players[i].id;
        out->players[i].active = players[i].active;
        out->players[i].alive = players[i].alive;
        out->players[i].score = players[i].score;
        out->players[i].len = players[i].snake_len;
        for (uint16_t k = 0; k < players[i].snake_len && k < MAX_SNAKE; k++) {
            out->players[i].seg[k] = players[i].snake[k];
        }
    }
}

