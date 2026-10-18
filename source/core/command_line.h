#ifndef UPRISING_NATIVE_COMMAND_LINE_H
#define UPRISING_NATIVE_COMMAND_LINE_H

#include "game_memory.h"

typedef struct GameCommandLineResult {
    GameAddress output_cursor;
    GameAddress command_cursor;
} GameCommandLineResult;

int game_command_line_parse(GameMemory *memory, GameAddress argument_slot,
    GameAddress command, GameAddress argv, GameAddress output,
    GameAddress argc_address, GameAddress chars_address,
    GameAddress lead_byte_table, GameCommandLineResult *result);

#endif
