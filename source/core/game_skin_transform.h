#ifndef RECOMP_GAME_SKIN_TRANSFORM_H
#define RECOMP_GAME_SKIN_TRANSFORM_H

#include "game_memory.h"

void game_skin_transform(GameMemory *memory, GameAddress model);
int game_presentation_transform_vertices(GameMemory *memory, GameAddress object);
int game_presentation_transform(GameMemory *memory, GameAddress object);

#endif
