#ifndef UPRISING_GAMEPLAY_TEARDOWN_H
#define UPRISING_GAMEPLAY_TEARDOWN_H

#include "game_memory.h"

#include <stdint.h>

typedef struct GameplayTeardownSlotSpec {
    uint32_t offset;
    GameAddress return_address;
} GameplayTeardownSlotSpec;

typedef struct GameplayTeardownGlobalSpec {
    GameAddress slot;
    GameAddress return_address;
} GameplayTeardownGlobalSpec;

typedef struct GameplayTeardownListSpec {
    uint32_t offset;
    GameAddress first_return;
    GameAddress destroy_return;
    GameAddress next_return;
} GameplayTeardownListSpec;


typedef struct GameplayRuntimeTeardownSpec {
    uint32_t queue_limit;
    uint32_t request_flags;
    GameAddress queue_owner_slot;
    GameAddress response_owner_slot;
    GameAddress queue_count_slot;
} GameplayRuntimeTeardownSpec;

typedef struct GameplaySceneObjectMembers {
    GameAddress buffer;
    GameAddress auxiliary;
} GameplaySceneObjectMembers;

GameplayTeardownSlotSpec gameplay_teardown_resource_owner_slot(unsigned index);
GameplayTeardownGlobalSpec gameplay_teardown_service_global(unsigned index);
GameplayTeardownListSpec gameplay_teardown_owner_list(unsigned index);
GameplayRuntimeTeardownSpec gameplay_teardown_runtime_service_spec(void);
uint32_t gameplay_teardown_component_slot_offset(unsigned index);

/* State half of the shared scene-object destructor. The external resource
 * destructor runs between begin and members, before the final base vtable. */
GameAddress gameplay_teardown_begin_scene_object(GameMemory *memory,
    GameAddress object);
GameplaySceneObjectMembers gameplay_teardown_scene_object_members(
    GameMemory *memory, GameAddress object);
void gameplay_teardown_finish_scene_object(GameMemory *memory,
    GameAddress object);

void gameplay_teardown_clear_entity_binding(GameMemory *memory,
    GameAddress owner);
void gameplay_teardown_reset_entity_auxiliary(GameMemory *memory,
    GameAddress owner, unsigned stage);
void gameplay_teardown_mark_entity_auxiliary_empty(GameMemory *memory,
    GameAddress owner);
void gameplay_teardown_detach_entity_auxiliary(GameMemory *memory,
    GameAddress owner);
void gameplay_teardown_clear_presentation_surface(GameMemory *memory,
    GameAddress owner, unsigned slot);
void gameplay_teardown_reset_presentation_owner(GameMemory *memory,
    GameAddress owner);
void gameplay_teardown_mark_presentation_owner_inactive(GameMemory *memory,
    GameAddress owner);
void gameplay_teardown_clear_presentation_tail(GameMemory *memory,
    GameAddress owner, unsigned slot);
void gameplay_teardown_clear_service_global(GameMemory *memory,
    unsigned index);

#endif
