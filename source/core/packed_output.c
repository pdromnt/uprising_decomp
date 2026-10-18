#include "packed_output.h"

GamePackedOutputCursor game_packed_output_begin(int32_t value) {
    GamePackedOutputCursor cursor;
    cursor.magnitude = value < 0 ? 0u - (uint32_t)value : (uint32_t)value;
    cursor.table = value < 0 ? 0x005a36a0u : 0x005a3540u;
    return cursor;
}

GamePackedOutputDigit game_packed_output_next(GamePackedOutputCursor *cursor) {
    GamePackedOutputDigit digit;
    uint32_t value = cursor->magnitude;
    cursor->table += 0x54u;
    digit.value = value & 7u;
    digit.remaining = value & 0x80000000u ?
        (value >> 3u) | 0xe0000000u : value >> 3u;
    digit.entry = cursor->table + digit.value * 12u;
    cursor->magnitude = digit.remaining;
    return digit;
}
