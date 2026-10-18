#ifndef UPRISING_NATIVE_GAME_STATE_OPS_H
#define UPRISING_NATIVE_GAME_STATE_OPS_H

#include "game_memory.h"

#include <stdint.h>

typedef struct GameRectangleBounds {
    int32_t left, top, right, bottom;
} GameRectangleBounds;

typedef struct GameOccupancyWindow {
    uint32_t minimum_x, minimum_y, maximum_y, maximum_x;
} GameOccupancyWindow;

typedef struct GameStateRecordConfig {
    uint8_t type;
    uint32_t second;
    uint32_t third;
    int32_t fourth;
    uint32_t fifth;
    uint32_t sixth;
    uint32_t seventh;
    uint32_t eighth;
    uint16_t ninth;
    uint32_t tenth;
    uint32_t eleventh;
    uint16_t twelfth;
    int32_t thirteenth;
    uint32_t fourteenth;
} GameStateRecordConfig;

typedef struct GameStateCallbackBinding {
    GameAddress enabled;
    GameAddress disabled;
} GameStateCallbackBinding;

typedef struct GameStateStringSearchResult {
    uint32_t index;
    int32_t comparison;
} GameStateStringSearchResult;

typedef struct GameSelectorAdvanceResult {
    uint32_t selector;
    uint32_t mask;
    uint32_t final_eax;
} GameSelectorAdvanceResult;

void game_state_initialize_word_table(GameMemory *memory, GameAddress object);
uint32_t game_state_accumulate_field(GameMemory *memory, GameAddress object,
    uint32_t source_offset, uint32_t destination_offset, uint32_t amount);
void game_state_reset_slot_tables(GameMemory *memory);
void game_state_copy_48(GameMemory *memory, GameAddress destination,
    GameAddress source);
void game_state_reset_runtime_vectors(GameMemory *memory);
GameRectangleBounds game_state_clamp_rectangle(GameRectangleBounds input,
    int32_t minimum_x, int32_t maximum_x, int32_t minimum_y,
    int32_t maximum_y);
int game_state_assign_related(GameMemory *memory, GameAddress owner,
    GameAddress candidate, uint32_t destination_offset);
void game_state_initialize_descriptor(GameMemory *memory, GameAddress object);
uint32_t game_state_resolve_event_category(GameMemory *memory,
    uint32_t identifier);
int game_state_bitset_suffix_clear(GameMemory *memory, GameAddress bitset,
    int32_t bit_index);
int game_state_timed_child_ready(GameMemory *memory, GameAddress object);

typedef struct GameAssetSelection {
    uint32_t field_offset;
    GameAddress table_base;
    int valid;
} GameAssetSelection;

GameAssetSelection game_state_asset_selection(uint8_t type);
GameAssetSelection game_state_secondary_asset_selection(uint8_t type,
    uint8_t subtype);
uint32_t game_state_asset_record_index(uint32_t field, uint32_t caller_index);
uint32_t game_state_asset_owner_field(GameMemory *memory, GameAddress owner,
    GameAssetSelection selection);
uint32_t game_state_asset_record(GameMemory *memory,
    GameAssetSelection selection, uint32_t field, uint32_t caller_index);
uint32_t game_state_read_global(GameMemory *memory, GameAddress address);
void game_state_write_global(GameMemory *memory, GameAddress address,
    uint32_t value);
void game_state_copy_global_pair(GameMemory *memory, GameAddress first_source,
    GameAddress second_source, GameAddress first_output,
    GameAddress second_output);
int game_state_occupancy_query(GameMemory *memory, GameAddress object,
    uint32_t original_x, uint32_t original_y, uint32_t x_offset,
    uint32_t y_offset);
GameOccupancyWindow game_state_set_occupancy_window(GameMemory *memory,
    GameAddress object, uint32_t first_x, uint32_t first_y,
    uint32_t second_x, uint32_t second_y);
uint32_t game_state_grid_mask(GameMemory *memory, GameAddress object,
    int32_t x, int32_t y);
void game_state_write_fields(GameMemory *memory, GameAddress object,
    const uint32_t *offsets, const uint32_t *values, unsigned count);
uint32_t game_state_scaled_indexed_value(uint32_t index, uint32_t value,
    uint32_t scaled_mask);
void game_state_reset_pair_array(GameMemory *memory, GameAddress object,
    unsigned count, uint32_t second_offset);
void game_state_apply_default_pair_array(GameMemory *memory,
    GameAddress object, GameAddress source, unsigned count,
    uint32_t second_offset, GameAddress default_source);
uint32_t game_state_next_owner_sequence(GameMemory *memory,
    GameAddress object);
int game_state_append_bounded(GameMemory *memory, GameAddress object,
    uint32_t count_offset, uint32_t values_offset, unsigned stride,
    unsigned capacity, uint32_t value, uint32_t *old_count);
uint32_t game_state_bounce_counter(GameMemory *memory, GameAddress total,
    GameAddress direction, uint32_t maximum);
uint32_t game_state_initialize_texture_cache(GameMemory *memory, int fullscreen);
void game_state_add_pair(GameMemory *memory, GameAddress first_address,
    GameAddress second_address, uint32_t first, uint32_t second);
void game_state_write_byte(GameMemory *memory, GameAddress address, uint8_t value);
uint32_t game_state_read_indexed(GameMemory *memory, GameAddress table,
    uint32_t index);
void game_state_add_word_pair(GameMemory *memory, GameAddress first_address,
    GameAddress second_address, uint16_t first, uint16_t second);
float game_state_approximate_distance(float first_x, float first_y,
    float second_x, float second_y, float half);
void game_state_configure_record(GameMemory *memory, GameAddress object,
    const GameStateRecordConfig *config);
int game_state_append_typed_record(GameMemory *memory, GameAddress object,
    uint8_t type, uint32_t value, uint32_t *old_count);
GameStateStringSearchResult game_state_find_string(GameMemory *memory, GameAddress table,
    int32_t count, GameAddress query);
GameStateCallbackBinding game_state_bind_callbacks(GameMemory *memory,
    GameAddress object, uint32_t first, uint32_t second,
    GameAddress enabled_first, GameAddress enabled_second);
void game_state_initialize_transform(GameMemory *memory, GameAddress object,
    GameAddress vtable);
GameSelectorAdvanceResult game_state_advance_selector(GameMemory *memory,
    GameAddress object);
void game_state_build_span_table(GameMemory *memory, int primary_mode,
    int secondary_mode);
void game_state_initialize_gameplay_grid(GameMemory *memory,
    GameAddress object, uint32_t mode, int wide_variant,
    const uint32_t mode_seven_bounds[4]);
void game_state_initialize_gameplay_order(GameMemory *memory,
    GameAddress object);
typedef struct GameStateOptionApplyResult {
    uint32_t masked_flags;
    uint32_t final_value;
} GameStateOptionApplyResult;
typedef struct GameStateGridPathResult {
    uint32_t value;
    uint32_t branch;
    uint32_t column;
    uint32_t row;
    uint32_t target_index;
    uint32_t target_bits;
    uint32_t target_mask;
    uint32_t first_index;
    uint32_t first_bits;
    uint32_t final_index;
    uint32_t final_bits;
    uint32_t final_mask;
} GameStateGridPathResult;
typedef struct GameStateLocalizedRefreshResult {
    uint32_t limit;
    uint32_t final_eax;
    uint32_t final_ecx;
    uint32_t final_ebx;
    int skipped;
} GameStateLocalizedRefreshResult;
typedef enum GameStateRelinkFlagKind {
    GAME_STATE_RELINK_TEST_BYTE = 0,
    GAME_STATE_RELINK_TEST_WORD
} GameStateRelinkFlagKind;
typedef struct GameStateRelinkResult {
    uint32_t final_eax;
    uint32_t final_ecx;
    uint32_t final_edx;
    uint32_t flag_value;
    uint32_t seed_left;
    uint32_t seed_right;
    GameStateRelinkFlagKind flag_kind;
    int seed_compare;
    int modified;
} GameStateRelinkResult;
GameStateOptionApplyResult game_state_apply_object_options(
    GameMemory *memory, GameAddress target, uint32_t flags,
    GameAddress object, GameAddress first_option, GameAddress second_option);
GameStateGridPathResult game_state_query_grid_path(GameMemory *memory,
    GameAddress grid, uint32_t original_x, uint32_t original_y,
    uint32_t x_offset, uint32_t y_offset);
GameStateLocalizedRefreshResult game_state_refresh_localized_name_slots(
    GameMemory *memory, uint32_t start, uint32_t count, int backward);
GameStateRelinkResult game_state_relink_intrusive_node(GameMemory *memory,
    GameAddress node);

#endif
