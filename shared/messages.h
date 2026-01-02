#pragma once
#include <stdint.h>


typedef enum {
    MSG_JOIN     = 1,
    MSG_JOIN_OK  = 2,
    MSG_INPUT    = 3,
    MSG_STATE    = 4
} msg_type_t;

typedef enum {
    DIR_UP    = 0,
    DIR_DOWN  = 1,
    DIR_LEFT  = 2,
    DIR_RIGHT = 3
} direction_t;

typedef enum {
    ACT_NONE  = 0,
    ACT_PAUSE = 1,
    ACT_QUIT  = 2,
    ACT_START = 3,
    ACT_RESTART = 4
} action_t;

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t len;
} msg_hdr_t;

typedef struct __attribute__((packed)) {
    uint32_t player_id;
} join_ok_t;

typedef struct __attribute__((packed)) {
    uint8_t dir;    
    uint8_t action;  
} input_t;

#define WORLD_W 40
#define WORLD_H 20

#define MAX_PLAYERS 4
#define MAX_SNAKE   128
#define MAX_FRUITS  3

typedef struct __attribute__((packed)) {
    uint8_t x;
    uint8_t y;
} pos_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint8_t  active;   
    uint8_t  alive;    
    uint16_t len;
    pos_t    seg[MAX_SNAKE]; 
    uint32_t score;
} player_state_t;

typedef struct __attribute__((packed)) {
    uint32_t tick;

    uint8_t w, h;

    uint8_t lobby;      
    uint8_t round_over; 
    uint8_t player_count;
    uint8_t alive_count;

    uint8_t fruit_count;
    pos_t   fruits[MAX_FRUITS];

    player_state_t players[MAX_PLAYERS];
} state_t;
