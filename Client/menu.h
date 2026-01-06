#pragma once
#include <stdint.h>
#include "../shared/messages.h"

typedef struct {
    create_game_t cfg;
} menu_result_t;

int menu_run(menu_result_t *out, uint8_t is_host);
