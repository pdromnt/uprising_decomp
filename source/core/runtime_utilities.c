#include "runtime_utilities.h"

static int mode_is_enabled(uint16_t mode) {
    int16_t signed_mode = (int16_t)mode;
    return signed_mode <= 0x0100 ||
        (signed_mode >= 0x0300 && signed_mode <= 0x0500) ||
        signed_mode >= 0x0700;
}

int runtime_set_mode_enabled(GameMemory *memory, uint16_t mode) {
    int enabled = mode_is_enabled(mode);
    game_memory_write_u32(memory, 0x0059d438u, enabled ? 1u : 0u);
    return enabled;
}

uint32_t runtime_register_pointer(GameMemory *memory, GameAddress value) {
    uint32_t index = game_memory_read_u32(memory, 0x00639828u) + 1u;
    if (!memory->fault) game_memory_write_u32(memory, 0x00639828u, index);
    if (!memory->fault)
        game_memory_write_u32(memory, 0x00631b24u + index * 4u, value);
    return index;
}

void runtime_reset_registration_array(GameMemory *memory, int backwards) {
    uint32_t was_initialized = game_memory_read_u32(memory, 0x0059d938u);
    uint32_t index;
    if (memory->fault) return;
    game_memory_write_u32(memory, 0x0059d934u, 1u);
    if (was_initialized != 0u) {
        for (index = 0u; index < 8000u && !memory->fault; ++index)
            game_memory_write_u32(memory, backwards ?
                0x00631b28u - index * 4u : 0x00631b28u + index * 4u, 0u);
    }
    if (!memory->fault) game_memory_write_u32(memory, 0x00639828u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x0059d938u, 0u);
}

static int32_t arithmetic_shift_right(uint32_t value, unsigned amount) {
    return ((int32_t)value) >> amount;
}

void runtime_update_viewport_bounds(GameMemory *memory, int quarter_lower) {
    int32_t width = (int32_t)game_memory_read_u32(memory, 0x005ac6a0u);
    int32_t height = (int32_t)game_memory_read_u32(memory, 0x005ac6a4u);
    int32_t inset;
    int32_t horizontal;
    int32_t vertical;
    if (memory->fault) return;
    inset = arithmetic_shift_right((uint32_t)height, 3u);
    horizontal = arithmetic_shift_right((uint32_t)width, 1u);
    vertical = arithmetic_shift_right((uint32_t)height,
        quarter_lower ? 2u : 1u);
    game_memory_write_u32(memory, 0x005acce0u,
        (uint32_t)(horizontal + inset));
    if (!memory->fault) game_memory_write_u32(memory, 0x005ac920u,
        (uint32_t)(horizontal - inset));
    if (!memory->fault) game_memory_write_u32(memory, 0x005acce4u,
        (uint32_t)(arithmetic_shift_right((uint32_t)height, 1u) + inset));
    if (!memory->fault) game_memory_write_u32(memory, 0x005ac924u,
        (uint32_t)(vertical - inset));
}

void runtime_reset_presentation_pair(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00604258u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x0060425cu, 0u);
}

RuntimeResourceDescriptor runtime_resource_descriptor(GameMemory *memory,
        unsigned index) {
    RuntimeResourceDescriptor descriptor = {0u, 0u, 0u};
    static const GameAddress owners[2] = {0x0058abf4u, 0x0058abf8u};
    static const GameAddress values[2] = {0x005ac910u, 0x005acbecu};
    static const GameAddress formats[2] = {0x0058bae8u, 0x0058baecu};
    if (index >= 2u) return descriptor;
    descriptor.owner = game_memory_read_u32(memory, owners[index]);
    if (!memory->fault && descriptor.owner != 0u)
        descriptor.value = game_memory_read_u32(memory, values[index]);
    descriptor.format = formats[index];
    return descriptor;
}

GameAddress runtime_display_descriptor(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005980acu);
}

void runtime_publish_display_descriptor(GameMemory *memory,
        GameAddress descriptor) {
    game_memory_write_u32(memory, 0x005980acu, descriptor);
}

void runtime_store_display_descriptor(GameMemory *memory,
        GameAddress descriptor, uint16_t width, uint16_t height,
        GameAddress handle) {
    game_memory_write_u16(memory, descriptor, width);
    if (!memory->fault)
        game_memory_write_u16(memory, descriptor + 2u, height);
    if (!memory->fault)
        game_memory_write_u32(memory, descriptor + 4u, handle);
}
