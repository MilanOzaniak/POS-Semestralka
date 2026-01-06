#pragma once
#pragma once
#include <stdint.h>

#define MAX_PLAYERS 4
#define MAX_SNAKE   128
#define MAX_FRUITS  8

#define WORLD_W 40
#define WORLD_H 20

typedef struct __attribute__((packed)) {
    uint8_t x;
    uint8_t y;
} pos_t;

typedef enum {
    MSG_JOIN      = 1,
    MSG_JOIN_OK   = 2,
    MSG_INPUT     = 3,
    MSG_STATE     = 4,
    MSG_CREATE    = 5
} msg_type_t;

typedef enum {
    DIR_UP    = 0,
    DIR_DOWN  = 1,
    DIR_LEFT  = 2,
    DIR_RIGHT = 3
} direction_t;

typedef enum {
    ACT_NONE    = 0,
    ACT_QUIT    = 1,
    ACT_START   = 2,
    ACT_RESTART = 3
} action_t;

typedef enum {
    MAP_EMPTY  = 0,
    MAP_RANDOM = 1
} map_type_t;

typedef enum {
    MODE_STANDARD = 0,
    MODE_TIMED    = 1
} game_mode_t;

typedef struct __attribute__((packed)) {
    uint8_t map_type;
    uint8_t obstacles;
    uint8_t w;
    uint8_t h;
    uint8_t mode;
    uint16_t time_limit;
    uint8_t obstacle_density;
} create_game_t;

typedef struct __attribute__((packed)) {
    uint8_t dir;
    uint8_t action;
} input_t;

typedef struct __attribute__((packed)) {
    uint32_t player_id;
    uint8_t  is_host;
} join_ok_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint8_t  active;
    uint8_t  alive;
    uint32_t score;
    uint16_t len;
    pos_t    seg[MAX_SNAKE];
} player_state_t;

typedef struct __attribute__((packed)) {
    uint32_t tick;
    uint8_t  w;
    uint8_t  h;

    uint8_t  lobby;
    uint8_t  round_over;

    uint8_t  player_count;
    uint8_t  alive_count;

    uint8_t  mode;
    uint16_t time_left;

    uint8_t  fruit_count;
    pos_t    fruits[MAX_FRUITS];

    player_state_t players[MAX_PLAYERS];
} state_t;

