#ifndef UPRISING_NATIVE_PALETTE_PUBLISH_H
#define UPRISING_NATIVE_PALETTE_PUBLISH_H

#include "game_memory.h"

typedef struct GamePalettePublication {
    GameAddress source_end;
    GameAddress destination_end;
    uint8_t final_component;
} GamePalettePublication;

int game_palette_publish_rgb(GameMemory *memory, GameAddress source,
    GameAddress destination, GamePalettePublication *publication);
int game_palette_initialize_332(GameMemory *memory, GameAddress destination);

#endif
