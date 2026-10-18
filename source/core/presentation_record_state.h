#ifndef UPRISING_NATIVE_PRESENTATION_RECORD_STATE_H
#define UPRISING_NATIVE_PRESENTATION_RECORD_STATE_H

#include "game_memory.h"

typedef enum GamePresentationRecordFlags {
    GAME_PRESENTATION_RECORD_COMPARE,
    GAME_PRESENTATION_RECORD_ADD,
    GAME_PRESENTATION_RECORD_ZERO
} GamePresentationRecordFlags;

typedef struct GamePresentationRecordResult {
    uint32_t record;
    uint32_t type;
    uint32_t primary;
    uint32_t secondary;
    uint32_t add_left;
    uint32_t add_right;
    GamePresentationRecordFlags flags;
} GamePresentationRecordResult;

int game_presentation_publish_record(GameMemory *memory, GameAddress owner,
    GamePresentationRecordResult *result);

#endif
