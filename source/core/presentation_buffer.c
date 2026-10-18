#include "presentation_buffer.h"

void presentation_buffer_flip_and_compact(GameMemory *memory,
        GameAddress descriptor, GameAddress scratch) {
    uint32_t width = game_memory_read_u32(memory, descriptor + 4u);
    uint32_t height = game_memory_read_u32(memory, descriptor + 8u);
    GameAddress data = game_memory_read_u32(memory, descriptor + 0x14u);
    uint32_t stride;
    GameAddress front;
    GameAddress back;
    uint32_t half;
    uint32_t row;
    uint32_t column;
    if (memory->fault) return;
    stride = (width + 3u) & ~3u;
    front = data;
    back = data + stride * height;
    half = height >> 1u;
    game_memory_write_u32(memory, scratch + 0x14u, stride);
    game_memory_write_u32(memory, scratch + 0x18u, descriptor);
    game_memory_write_u32(memory, scratch + 0x20u, back);
    game_memory_write_u32(memory, scratch + 0x24u, front);
    game_memory_write_u32(memory, scratch + 0x28u, half);
    for (row = 0u; row < half && !memory->fault; ++row) {
        back -= stride;
        for (column = 0u; column < width && !memory->fault; ++column) {
            uint8_t upper = game_memory_read_u8(memory, front + column);
            uint8_t lower = game_memory_read_u8(memory, back + column);
            if (!memory->fault)
                game_memory_write_u8(memory, back + column, upper);
            if (!memory->fault)
                game_memory_write_u8(memory, front + column, lower);
        }
        front += stride;
        game_memory_write_u32(memory, scratch + 0x20u, back);
        game_memory_write_u32(memory, scratch + 0x24u, front);
        game_memory_write_u32(memory, scratch + 0x28u, half - row - 1u);
    }
    if (!memory->fault && stride != width && height > 1u) {
        GameAddress source = data + stride;
        GameAddress destination = data + width;
        for (row = 1u; row < height && !memory->fault; ++row) {
            for (column = 0u; column < width && !memory->fault; ++column)
                game_memory_write_u8(memory, destination + column,
                    game_memory_read_u8(memory, source + column));
            source += stride;
            destination += width;
        }
    }
}
