#ifndef UPRISING_NATIVE_BITSET_SPAN_H
#define UPRISING_NATIVE_BITSET_SPAN_H

#include <stdint.h>

typedef struct GameBitsetPosition {
    int32_t word_index;
    int32_t remainder;
    uint32_t sign;
    uint32_t shift;
    uint32_t mask;
    uint32_t word_address;
} GameBitsetPosition;

GameBitsetPosition game_bitset_position(uint32_t base, int32_t bit_index);

#endif
