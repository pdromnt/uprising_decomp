#ifndef UPRISING_NATIVE_CHARACTER_CLASSIFICATION_H
#define UPRISING_NATIVE_CHARACTER_CLASSIFICATION_H

#include "game_memory.h"

uint16_t game_character_classification_table(GameMemory *memory,
    GameAddress table, uint16_t character);
uint16_t game_character_classification_mask(uint16_t classes,
    uint16_t requested);
int game_character_classification_clear(GameMemory *memory,
    GameAddress table, int reverse);

#endif
