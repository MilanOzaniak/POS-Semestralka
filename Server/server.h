#pragma once
#include <pthread.h>
#include "../shared/messages.h"
#include "game.h"

typedef struct {
    int lfd;
    pthread_mutex_t mtx;

    game_t G;
    player_t players[MAX_PLAYERS];
    uint32_t next_id;

    uint32_t host_id;

    uint8_t shutdown;
} server_t;

int server_listen(uint16_t port);
void server_init(server_t *S, uint16_t port, uint8_t w, uint8_t h);
void server_run(server_t *S);
