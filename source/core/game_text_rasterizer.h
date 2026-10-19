#ifndef UPRISING_GAME_TEXT_RASTERIZER_H
#define UPRISING_GAME_TEXT_RASTERIZER_H

#include "game_memory.h"

typedef struct GameTextTarget {
    GameAddress base;
    int32_t stride;
    int rgb565;
    /*Optional borrowed native pixels, valid only during the backend lock.
     * Legacy/oracle targets leave these zero; no host pointer enters base. */
    uint8_t *pixels;
    size_t pixel_bytes;
    /* Optional borrowed [first, end) pixel range for a sparse native lock.
     * The lock owner initializes it to {UINT32_MAX, 0}. */
    uint32_t *written_range;
} GameTextTarget;

uint32_t game_text_rasterize(GameMemory *memory, GameAddress text,
    const GameTextTarget *target, int32_t fallback_x);
uint32_t game_text_width_bytes(GameMemory *,const uint8_t *,size_t);
uint32_t game_text_rasterize_bytes(GameMemory *,const uint8_t *,size_t,
    const GameTextTarget *,int32_t fallback_x);

#endif
