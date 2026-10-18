#ifndef UPRISING_NATIVE_GAMEPLAY_METHODS_H
#define UPRISING_NATIVE_GAMEPLAY_METHODS_H

#include "game_memory.h"

#include <stdint.h>

uint32_t gameplay_method_scalar_u32(uint32_t method_id);
uint16_t gameplay_method_scalar_u16(uint32_t method_id);
void gameplay_method_set_pair(GameMemory *memory, GameAddress self,
    uint32_t first, uint32_t second);
void gameplay_method_set_dimensions(GameMemory *memory, GameAddress self,
    uint16_t first, uint16_t second);
void gameplay_method_publish_launch_value(GameMemory *memory, uint32_t value);
void gameplay_method_set_extent_pair(GameMemory *memory, GameAddress self,
    uint32_t first, uint32_t second);
GameAddress gameplay_method_store_frame_slots(GameMemory *memory,
    GameAddress stack, uint32_t eax, uint32_t ecx, int store_all);
uint32_t gameplay_method_read_global(GameMemory *memory,
    GameAddress address);
typedef struct GameplayMethodVirtualTargetResult {
    GameAddress vtable;
    GameAddress target;
} GameplayMethodVirtualTargetResult;

GameplayMethodVirtualTargetResult gameplay_method_virtual_target(
    GameMemory *memory, GameAddress self, uint32_t slot_offset);

typedef struct GameplayMethodByteCompareResult {
    uint32_t remaining;
    uint8_t left;
    uint8_t right;
    int equal;
} GameplayMethodByteCompareResult;

GameplayMethodByteCompareResult gameplay_method_select_transport(
    GameMemory *memory, GameAddress source);

typedef struct GameplayMethodCallStep {
    GameAddress target;
    GameAddress return_address;
    GameAddress self;
    int set_self;
    GameAddress self_source_address;
    uint32_t arguments[6];
    uint32_t argument_count;
    uint32_t caller_cleanup;
    uint32_t argument0_from_eax;
    GameAddress argument0_source_address;
    uint32_t argument0_addend;
    GameAddress post_write_address;
    uint32_t post_write_value;
    uint32_t post_test_left;
    uint32_t post_test_right;
    uint32_t post_test_width;
} GameplayMethodCallStep;

typedef struct GameplayMethodCallbackPlan {
    GameplayMethodCallStep steps[3];
    uint32_t step_count;
    uint32_t return_value;
    int set_return_value;
    uint32_t initial_eax;
    int set_initial_eax;
    uint32_t initial_ecx;
    int set_initial_ecx;
    uint32_t initial_edx;
    int set_initial_edx;
    uint32_t initial_test_left;
    uint32_t initial_test_right;
    uint32_t initial_test_width;
    GameAddress final_write_address;
    uint32_t final_write_value;
    uint32_t outer_cleanup;
} GameplayMethodCallbackPlan;

uint16_t gameplay_method_adjust_selection_index(GameMemory *memory,
    GameAddress self, int32_t delta);
GameplayMethodCallbackPlan gameplay_method_callback_prepare(
    GameMemory *memory, uint32_t method_id, uint32_t input);
GameplayMethodCallbackPlan gameplay_method_destructor_callback_prepare(
    uint32_t method_id, GameAddress owner, uint32_t flags);
GameplayMethodCallbackPlan gameplay_method_pair_callback_prepare(
    GameMemory *memory, uint32_t method_id, GameAddress owner,
    uint32_t first, uint32_t second);
GameplayMethodCallbackPlan gameplay_method_render_callback_prepare(
    uint32_t first, uint32_t second, uint32_t third, uint32_t fourth,
    uint32_t fifth);

typedef struct GameplayMethodStreamReadResult {
    uint32_t original_count;
    uint32_t decremented_count;
    GameAddress original_cursor;
    GameAddress incremented_cursor;
    uint32_t byte_value;
    int refill;
} GameplayMethodStreamReadResult;

GameplayMethodStreamReadResult gameplay_method_stream_read(
    GameMemory *memory, GameAddress stream);
void gameplay_method_callback_finish(GameMemory *memory, uint32_t method_id);
uint32_t gameplay_method_callback_finish_value(GameMemory *memory,
    uint32_t method_id);
uint32_t gameplay_method_publish_multiplayer_payload(GameMemory *memory,
    GameAddress source, uint32_t *index, uint32_t *value);
GameAddress gameplay_method_store_indexed_vector(GameMemory *memory,
    GameAddress self, uint32_t first, uint32_t second, uint32_t third);
uint16_t gameplay_method_adjust_bounded_angle(GameMemory *memory,
    GameAddress self, int32_t delta, GameAddress *continuation);

#endif
