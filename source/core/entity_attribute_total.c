#include "entity_attribute_total.h"

GameEntityAttributeTotal game_entity_attribute_total(GameMemory *memory,
    GameAddress entity, uint32_t base) {
    static const uint32_t offsets[] = {
        0xd8u, 0xb8u, 0xb4u, 0xd4u, 0xd0u, 0xbcu
    };
    GameEntityAttributeTotal result = {0u, 0u, 0u, 0u};
    uint32_t sum = base;
    unsigned i;
    if (!memory) return result;
    for (i = 0u; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        sum += game_memory_read_u32(memory, entity + offsets[i]);
        if (memory->fault) return result;
    }
    result.signed_delta = (uint32_t)(int32_t)(int16_t)
        game_memory_read_u16(memory, entity + 0xc8u);
    if (memory->fault) return result;
    sum += game_memory_read_u32(memory, entity + 0xc0u);
    if (memory->fault) return result;
    result.final_field = game_memory_read_u32(memory, entity + 0xc4u);
    if (memory->fault) return result;
    result.before_signed = sum + result.final_field;
    result.total = result.before_signed + result.signed_delta;
    return result;
}
