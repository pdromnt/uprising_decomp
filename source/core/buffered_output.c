#include "buffered_output.h"

GameBufferedByte game_buffered_byte_emit(GameMemory *memory,
        GameAddress stream, GameAddress input_byte) {
    GameBufferedByte result = {0u, 0u, 0u, 0u, 0};
    result.previous_count = game_memory_read_u32(memory, stream + 4u);
    if (memory->fault) return result;
    result.count = result.previous_count - 1u;
    game_memory_write_u32(memory, stream + 4u, result.count);
    if (memory->fault || (int32_t)result.count < 0) return result;
    result.direct = 1;
    result.cursor = game_memory_read_u32(memory, stream);
    if (memory->fault) return result;
    result.value = game_memory_read_u8(memory, input_byte);
    if (memory->fault) return result;
    game_memory_write_u8(memory, result.cursor, result.value);
    if (memory->fault) return result;
    result.cursor = game_memory_read_u32(memory, stream);
    if (memory->fault) return result;
    result.value = game_memory_read_u8(memory, result.cursor);
    if (memory->fault) return result;
    game_memory_write_u32(memory, stream, result.cursor + 1u);
    return result;
}

uint32_t game_buffered_output_record(GameMemory *memory,
        GameAddress count_pointer, int failed) {
    uint32_t previous;
    if (failed) {
        game_memory_write_u32(memory, count_pointer, UINT32_MAX);
        return 0u;
    }
    previous = game_memory_read_u32(memory, count_pointer);
    if (!memory->fault)
        game_memory_write_u32(memory, count_pointer, previous + 1u);
    return previous;
}
