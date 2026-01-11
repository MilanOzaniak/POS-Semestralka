#define _POSIX_C_SOURCE 200809L
#include "map.h"
#include <string.h>

// vycisti mapu od prekazok a nastavi novy rozmer
void map_clear(map_t *m, uint8_t w, uint8_t h) {
    m->w = w;
    m->h = h;
    memset(m->blocks, 0, sizeof(m->blocks));
}

// random generator -- vygenerovane AI
static uint32_t xorshift32(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

// generovanie prekazok na mape
void map_random_obstacles(map_t *m, uint32_t seed, uint8_t count) {
    uint32_t s = seed ? seed : 2463534242u;
    
    // vymazanie starych prekazok
    for (uint8_t y = 0; y < m->h; y++){
        for (uint8_t x = 0; x < m->w; x++){
            m->blocks[y][x] = 0;
        }
    }
 
    // celkovy pocet buniek v mape
    uint32_t max = (uint32_t)m->w * (uint32_t)m->h;
    if (count > max) count = (uint8_t)max;

    uint32_t placed = 0;
    uint32_t tries = 0;
    
    // poklada prekazky na mapu
    // ak je mapa mala a velky pocet prekazok tak je tam nastaveny tries aby to nespadlo 
    while (placed < count && tries < max * 10) {
        uint32_t r = xorshift32(&s);
        uint8_t x = (uint8_t)(r % m->w);
        uint8_t y = (uint8_t)((r / m->w) % m->h);
        
        // ak uz tam existuje prekazka, skus znova
        if (m->blocks[y][x]) {
            tries++;
            continue;
        }
        
        // poloz prekazku
        m->blocks[y][x] = 1;
        placed++;
    }
    
    // spawn body hracov
    pos_t sp[4] = {
        {5,5},
        {(uint8_t)(m->w - 6), 5},
        {5, (uint8_t)(m->h - 6)},
        {(uint8_t)(m->w - 6), (uint8_t)(m->h - 6)}
    };
    
    // cistenie zony okolo hracov, nech im to nespawne prekazku vedla nich
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

// zistujeme ci nahoodu nieje prekazka mimo mapy
int map_is_blocked(const map_t *m, pos_t p) {
    if (p.x >= m->w || p.y >= m->h) return 1;
    return m->blocks[p.y][p.x] ? 1 : 0;
}
