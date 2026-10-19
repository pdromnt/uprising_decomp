#ifndef UPRISING_GAMEPLAY_TEARDOWN_H
#define UPRISING_GAMEPLAY_TEARDOWN_H
#include "game_memory.h"

typedef struct GameplaySceneObjectMembers {
    GameAddress buffer;
    GameAddress auxiliary;
} GameplaySceneObjectMembers;

GameAddress gameplay_teardown_begin_scene_object(GameMemory *memory,
    GameAddress object);
GameplaySceneObjectMembers gameplay_teardown_scene_object_members(
    GameMemory *memory, GameAddress object);
void gameplay_teardown_finish_scene_object(GameMemory *memory,
    GameAddress object);
void gameplay_teardown_reset_entity_auxiliary(GameMemory *memory,
    GameAddress owner, unsigned stage);
#endif
