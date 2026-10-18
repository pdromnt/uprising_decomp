#ifndef UPRISING_GAME_INITIAL_DATA_H
#define UPRISING_GAME_INITIAL_DATA_H

#include "game_memory.h"

/* Creates the game's owned initial records from source-defined data. The
 * fixed-width keys are record/selector identities, never executable pointers. */
int game_initial_data_create(GameMemory *memory);

#endif
