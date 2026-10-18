#ifndef UPRISING_GAMEPLAY_CONSTRUCTORS_H
#define UPRISING_GAMEPLAY_CONSTRUCTORS_H

#include "game_memory.h"
#include "presentation_state_copy.h"

#include <stdint.h>

/* Complete startup manager constructors and shared command-state teardown. */
int gameplay_manager_construct(GameMemory *,GameAddress,unsigned identifier);
int gameplay_manager_destroy(GameMemory *,GameAddress,const GamePresentationStorageServices *);

void gameplay_constructor_initialize_command_owner(GameMemory *memory,
    GameAddress object, uint16_t identifier);
void gameplay_constructor_copy_type89_fields(GameMemory *memory,
    GameAddress object, GameAddress source);
void gameplay_constructor_initialize_triple_list_owner(GameMemory *memory,
    GameAddress object);
void gameplay_constructor_initialize_list_node(GameMemory *memory,
    GameAddress node);
void gameplay_constructor_publish_triple_list(GameMemory *memory,
    GameAddress object, unsigned index, GameAddress node);
void gameplay_constructor_initialize_layered_stage(GameMemory *memory,
    GameAddress object, unsigned stage, uint32_t first, uint32_t second,
    uint32_t third);
void gameplay_constructor_initialize_indexed_buffer(GameMemory *memory,
    GameAddress object, uint32_t count);
uint32_t gameplay_constructor_indexed_buffer_size(uint32_t count,
    unsigned index);
void gameplay_constructor_publish_indexed_buffer(GameMemory *memory,
    GameAddress object, unsigned index, GameAddress allocation);
void gameplay_constructor_initialize_presentation_controller_base(
    GameMemory *memory, GameAddress object);
void gameplay_constructor_finish_presentation_controller(GameMemory *memory,
    GameAddress object);

typedef struct GameplayOwnedSlotSpec {
    uint32_t offset;
    GameAddress destructor;
    GameAddress destructor_return;
    GameAddress delete_return;
} GameplayOwnedSlotSpec;

typedef struct GameplayCallbackSpec {
    uint32_t code;
    GameAddress callback;
    GameAddress return_address;
} GameplayCallbackSpec;

GameplayOwnedSlotSpec gameplay_constructor_owned_slot_spec(unsigned index);
GameplayCallbackSpec gameplay_constructor_presentation_callback_spec(
    unsigned index);
void gameplay_constructor_initialize_text_provider(GameMemory *memory,
    GameAddress object);
void gameplay_constructor_initialize_reference_pair(GameMemory *memory,
    GameAddress object, uint32_t first_selector, uint32_t first_index,
    uint32_t second_selector, uint32_t second_index, uint32_t value);
void gameplay_constructor_publish_reference_pair(GameMemory *memory,
    GameAddress object, GameAddress first, GameAddress second);

#endif
