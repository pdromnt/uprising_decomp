#ifndef RECOMP_GAME_SCREEN_CIRCLE_H
#define RECOMP_GAME_SCREEN_CIRCLE_H

#include "game_memory.h"
#include "renderer_host.h"

typedef int (*GameScreenCircleBegin)(void *context);
typedef int (*GameScreenCircleSegment)(void *context,
    const GameAddress coordinates[4], RuntimeRenderVertex vertices[2]);

int game_screen_circle(GameMemory *memory, GameAddress stack,
    int32_t center_x, int32_t center_y, int32_t radius, uint8_t color_index,
    GameScreenCircleBegin begin, GameScreenCircleSegment segment,
    void *context);

#endif
