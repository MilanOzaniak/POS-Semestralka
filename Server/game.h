#pragma once
#include <stdint.h>
#include <pthread.h>
#include "../shared/messages.h"
#include "map.h"

typedef struct {
    uint8_t  active;
    uint8_t  alive;
    uint8_t  last_dir;
    uint32_t id;
    uint32_t score;
    uint16_t snake_len;
    pos_t    snake[MAX_SNAKE];
    int      fd;
    pthread_t th;
} player_t;

typedef struct {
    map_t map;

    uint8_t lobby;
    uint8_t start_req;
    uint8_t restart_req;

    uint8_t cfg_set;
    create_game_t cfg;

    uint64_t game_start_ms;
    uint64_t game_end_ms;

    uint32_t tick;

    uint8_t fruit_count;
    pos_t fruits[MAX_FRUITS];
} game_t;

uint64_t now_ms(void);

void game_init(game_t *G);
void game_apply_config(game_t *G, const create_game_t *cfg);
void game_init_round(game_t *G, player_t players[MAX_PLAYERS]);
void game_move_all(game_t *G, player_t players[MAX_PLAYERS]);
void game_make_state(const game_t *G, const player_t players[MAX_PLAYERS], state_t *out);

