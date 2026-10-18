#ifndef UPRISING_NATIVE_ENTITY_VERTEX_TRANSFORM_H
#define UPRISING_NATIVE_ENTITY_VERTEX_TRANSFORM_H

#include "game_memory.h"
#include "player_rig.h"

int player_transform_vertex_ranges(GameMemory *,PresentationPose *,PlayerRig *,int rendered);

int entity_transform_vertex_ranges(GameMemory *memory, GameAddress object,
    GameAddress scratch);

#endif
