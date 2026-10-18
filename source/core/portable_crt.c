#include "portable_crt.h"

GameAddress portable_crt_strrchr(GameMemory *memory, GameAddress string,
    uint8_t target) {
    GameAddress cursor = string;
    GameAddress result = 0u;
    for (;;) {
        uint8_t value = game_memory_read_u8(memory, cursor);
        if (memory->fault) return 0u;
        if (value == target) result = cursor;
        if (value == 0u) return result;
        ++cursor;
    }
}

int32_t portable_crt_memcmp(GameMemory *memory, GameAddress left,
    GameAddress right, uint32_t count) {
    while (count-- != 0u) {
        uint8_t a = game_memory_read_u8(memory, left++);
        uint8_t b = game_memory_read_u8(memory, right++);
        if (memory->fault) return 0;
        if (a != b) return a < b ? -1 : 1;
        if (a == 0u) break;
    }
    return 0;
}

/*560de0: one CRT stream, shared by level initialization and menu consumers. */
void portable_crt_rand_seed(GameMemory *memory,uint32_t seed) {
    game_memory_write_u32(memory,0x005a1cd0u,seed);
}
uint32_t portable_crt_rand_step(GameMemory *memory) {
    uint32_t seed = game_memory_read_u32(memory, 0x005a1cd0u);
    seed = seed * 0x343fdu + 0x269ec3u;
    if (!memory->fault) game_memory_write_u32(memory, 0x005a1cd0u, seed);
    return (seed >> 16u) & 0x7fffu;
}

void portable_crt_strncpy(GameMemory *memory, GameAddress destination,
    GameAddress source, uint32_t count) {
    uint32_t index;
    int padding = 0;
    for (index = 0u; index < count && !memory->fault; ++index) {
        uint8_t value = 0u;
        if (!padding) {
            value = game_memory_read_u8(memory, source + index);
            if (value == 0u) padding = 1;
        }
        if (!memory->fault)
            game_memory_write_u8(memory, destination + index, value);
    }
}

uint32_t portable_crt_strnlen(GameMemory *memory, GameAddress string,
    uint32_t limit) {
    uint32_t length = 0u;
    while (length < limit &&
        game_memory_read_u8(memory, string + length) != 0u && !memory->fault)
        ++length;
    return length;
}

GameAddress portable_crt_strpbrk(GameMemory *memory, GameAddress string,
    GameAddress accept) {
    GameAddress cursor;
    for (; !memory->fault; ++string) {
        uint8_t value = game_memory_read_u8(memory, string);
        if (value == 0u || memory->fault) return 0u;
        for (cursor = accept; !memory->fault; ++cursor) {
            uint8_t accepted = game_memory_read_u8(memory, cursor);
            if (accepted == 0u) break;
            if (accepted == value) return string;
        }
    }
    return 0u;
}

GameAddress portable_crt_mbsrchr(GameMemory *memory, GameAddress string,
    uint32_t target) {
    GameAddress result = 0u;
    GameAddress cursor = string;
    for (;;) {
        uint8_t first = game_memory_read_u8(memory, cursor);
        uint32_t current = first;
        if (memory->fault) return 0u;
        if ((game_memory_read_u8(memory, 0x005a30a1u + first) & 4u) != 0u) {
            uint8_t second = game_memory_read_u8(memory, cursor + 1u);
            if (memory->fault) return 0u;
            if (second != 0u) current = ((uint32_t)first << 8u) | second;
            else {
                if (result == 0u) result = cursor + 1u;
                return result;
            }
        }
        if (current == target) result = cursor;
        if (first == 0u) return result;
        cursor += current > 0xffu ? 2u : 1u;
    }
}
