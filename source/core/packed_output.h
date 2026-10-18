#ifndef UPRISING_NATIVE_PACKED_OUTPUT_H
#define UPRISING_NATIVE_PACKED_OUTPUT_H

#include <stdint.h>

typedef struct GamePackedOutputCursor {
    uint32_t magnitude;
    uint32_t table;
} GamePackedOutputCursor;

typedef struct GamePackedOutputDigit {
    uint32_t value;
    uint32_t remaining;
    uint32_t entry;
} GamePackedOutputDigit;

GamePackedOutputCursor game_packed_output_begin(int32_t value);
GamePackedOutputDigit game_packed_output_next(GamePackedOutputCursor *cursor);

#endif
