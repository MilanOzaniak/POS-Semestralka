#pragma once
#include <stdint.h>
#include "../shared/messages.h"

typedef struct {
    uint8_t w;
    uint8_t h;
    uint8_t blocks[WORLD_H][WORLD_W];
} map_t;

void map_clear(map_t *m, uint8_t w, uint8_t h);
void map_random_obstacles(map_t *m, uint32_t seed, uint8_t density_pct);
int  map_is_blocked(const map_t *m, pos_t p);
