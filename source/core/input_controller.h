#ifndef UPRISING_NATIVE_INPUT_CONTROLLER_H
#define UPRISING_NATIVE_INPUT_CONTROLLER_H

#include "game_memory.h"

int game_input_controller_clear_initial(GameMemory *memory,
    GameAddress object);
int game_input_controller_clear_matrix(GameMemory *memory,
    GameAddress object);

#endif
