#pragma once
#include <stdint.h>
#include "../shared/messages.h"

char snake_head_char(int idx, int is_me);
void draw(const state_t *st, uint32_t my_id);
