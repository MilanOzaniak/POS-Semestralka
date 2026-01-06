#define _POSIX_C_SOURCE 200809L
#include "map.h"
#include <string.h>


void map_clear(map_t *m, uint8_t w, uint8_t h) {
    m->w = w;
    m->h = h;
    memset(m->blocks, 0, sizeof(m->blocks));
}

static uint32_t xorshift32(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

void map_random_obstacles(map_t *m, uint32_t seed, uint8_t density_pct) {
    if (density_pct > 60) density_pct = 60;
    uint32_t s = seed ? seed : 2463534242u;

    for (uint8_t y = 0; y < m->h; y++) {
        for (uint8_t x = 0; x < m->w; x++) {
            uint32_t r = xorshift32(&s) % 100u;
            m->blocks[y][x] = (r < density_pct) ? 1 : 0;
        }
    }

    pos_t sp[4] = {
        {5,5},
        {(uint8_t)(m->w - 6), 5},
        {5, (uint8_t)(m->h - 6)},
        {(uint8_t)(m->w - 6), (uint8_t)(m->h - 6)}
    };

    for (int i = 0; i < 4; i++) {
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                int xx = (int)sp[i].x + dx;
                int yy = (int)sp[i].y + dy;
                if (xx >= 0 && yy >= 0 && xx < (int)m->w && yy < (int)m->h) {
                    m->blocks[yy][xx] = 0;
                }
            }
        }
    }
}

int map_is_blocked(const map_t *m, pos_t p) {
    if (p.x >= m->w || p.y >= m->h) return 1;
    return m->blocks[p.y][p.x] ? 1 : 0;
}
