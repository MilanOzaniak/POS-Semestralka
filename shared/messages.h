#pragma once
#include <stdint.h>


typedef enum {
    MSG_JOIN     = 1,
    MSG_JOIN_OK  = 2,
    MSG_PING     = 3,
    MSG_PONG     = 4,
    MSG_INPUT    = 5,
    MSG_STATE    = 10
} msg_type_t;

typedef enum {
    DIR_UP = 0,
    DIR_DOWN = 1,
    DIR_LEFT = 2,
    DIR_RIGHT = 3
} direction_t;

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t len;   
} msg_hdr_t;

typedef struct __attribute__((packed)) {
    uint32_t player_id;
} join_ok_t;

typedef struct __attribute__((packed)) {
    uint32_t tick;
} state_t;

typedef struct __attribute__((packed)) {
    uint8_t dir;
} input_t;
