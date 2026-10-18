#include "bitset_span.h"

GameBitsetPosition game_bitset_position(uint32_t base, int32_t bit_index) {
    GameBitsetPosition position;
    position.word_index = bit_index / 32;
    position.remainder = bit_index % 32;
    position.sign = bit_index < 0 ? UINT32_MAX : 0u;
    position.shift = 31u - (uint32_t)position.remainder;
    position.mask = 1u << (position.shift & 31u);
    position.word_address = base + (uint32_t)position.word_index * 4u;
    return position;
}
