#ifndef UPRISING_NATIVE_BUFFERED_OUTPUT_H
#define UPRISING_NATIVE_BUFFERED_OUTPUT_H

#include "game_memory.h"

typedef struct GameBufferedByte {
    uint32_t previous_count;
    uint32_t count;
    GameAddress cursor;
    uint8_t value;
    int direct;
} GameBufferedByte;

GameBufferedByte game_buffered_byte_emit(GameMemory *memory,
    GameAddress stream, GameAddress input_byte);
uint32_t game_buffered_output_record(GameMemory *memory,
    GameAddress count_pointer, int failed);

#endif
