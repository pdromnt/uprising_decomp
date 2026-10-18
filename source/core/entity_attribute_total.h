#ifndef UPRISING_NATIVE_ENTITY_ATTRIBUTE_TOTAL_H
#define UPRISING_NATIVE_ENTITY_ATTRIBUTE_TOTAL_H

#include "game_memory.h"

typedef struct GameEntityAttributeTotal {
    uint32_t total;
    uint32_t before_signed;
    uint32_t signed_delta;
    uint32_t final_field;
} GameEntityAttributeTotal;

GameEntityAttributeTotal game_entity_attribute_total(GameMemory *memory,
    GameAddress entity, uint32_t base);

#endif
