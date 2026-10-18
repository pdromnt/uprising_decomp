#ifndef UPRISING_NATIVE_RUNTIME_UTILITIES_H
#define UPRISING_NATIVE_RUNTIME_UTILITIES_H

#include "game_memory.h"

#include <stdint.h>

void runtime_store_vector3(GameMemory *memory, GameAddress destination,
    uint32_t x, uint32_t y, uint32_t z);
GameAddress runtime_follow_payload(GameMemory *memory, GameAddress owner);
void runtime_publish_event_value(GameMemory *memory, uint32_t value);
uint32_t runtime_consume_u32(GameMemory *memory, GameAddress cursor_slot);
int runtime_three_slots_empty(GameMemory *memory, GameAddress slots);
void runtime_copy_words(GameMemory *memory, GameAddress destination,
    GameAddress source, uint32_t count);
uint32_t runtime_add_offsets(uint32_t left, uint32_t right, int *wrapped);
void runtime_shift_u96_left(GameMemory *memory, GameAddress value);
void runtime_shift_u96_right(GameMemory *memory, GameAddress value);
typedef struct RuntimeShiftU96Result {
    int32_t word_shift;
    int32_t bit_shift;
    uint32_t mask;
    uint8_t inverse_shift;
    uint32_t final_selected;
    uint32_t final_eax;
} RuntimeShiftU96Result;
RuntimeShiftU96Result runtime_shift_u96(GameMemory *memory,
    GameAddress value, int32_t count);
typedef struct RuntimeDivideU64Result {
    uint32_t quotient_low;
    uint32_t quotient_high;
    uint32_t remainder_low;
    uint32_t remainder_high;
} RuntimeDivideU64Result;
int runtime_divide_u64(uint32_t dividend_low, uint32_t dividend_high,
    uint32_t divisor_low, uint32_t divisor_high,
    RuntimeDivideU64Result *result);
int runtime_set_mode_enabled(GameMemory *memory, uint16_t mode);
uint32_t runtime_register_pointer(GameMemory *memory, GameAddress value);
void runtime_reset_registration_array(GameMemory *memory, int backwards);
void runtime_install_callback_table(GameMemory *memory);
void runtime_decode_bit_masks(GameMemory *memory, GameAddress record,
    uint16_t masks[2]);
GameAddress runtime_resolve_handle(GameMemory *memory, uint32_t index);
int runtime_byte_class_matches(GameMemory *memory, uint8_t code,
    uint32_t class_mask, uint32_t requested_mask);
uint32_t runtime_pack_file_mode(uint16_t mode);
uint16_t runtime_unpack_file_mode(uint32_t packed);
void runtime_update_viewport_bounds(GameMemory *memory, int quarter_lower);
GameAddress runtime_probe_stack(GameMemory *memory, GameAddress stack,
    uint32_t amount);
GameAddress runtime_optional_gameplay_service(GameMemory *memory);
int runtime_character_should_submit(uint32_t character);
uint32_t runtime_parser_success_predicate(uint32_t result);
uint32_t runtime_locale_class_bit2(GameMemory *memory, int32_t character);
int runtime_service_needs_initialization(GameMemory *memory);
uint32_t runtime_service_commit_initialization(GameMemory *memory);
void runtime_fullscreen_rectangle(GameMemory *memory, uint32_t rectangle[4]);
uint32_t runtime_forward_table(unsigned variant);
uint32_t runtime_element_width(void);
uint32_t runtime_conversion_width(void);

typedef struct RuntimeShutdownRegistration {
    GameAddress object;
    GameAddress constructor;
    GameAddress callback;
} RuntimeShutdownRegistration;

void runtime_platform_set_input_active(GameMemory *memory, int active);
GameAddress runtime_platform_input_callback(GameMemory *memory,
    unsigned stage);
void runtime_reset_subsystem_token(GameMemory *memory);
GameAddress runtime_active_session_owner(GameMemory *memory);
void runtime_reset_input_queues(GameMemory *memory);
void runtime_reset_presentation_pair(GameMemory *memory);
RuntimeShutdownRegistration runtime_shutdown_registration(unsigned kind);
GameAddress runtime_exception_registration_target(GameMemory *memory);
void runtime_publish_exception_handle(GameMemory *memory,
    GameAddress handle);
void runtime_reset_environment_state(GameMemory *memory);
GameAddress runtime_platform_region_target(GameMemory *memory,
    unsigned operation);
void runtime_publish_platform_region(GameMemory *memory, GameAddress handle);
GameAddress runtime_platform_region(GameMemory *memory);
void runtime_publish_environment_mode(GameMemory *memory, uint32_t mode);
GameAddress runtime_pretermination_callback(GameMemory *memory);
void runtime_termination_range(unsigned index, GameAddress *begin,
    GameAddress *end);
uint32_t runtime_default_locale_selector(void);
void runtime_default_control_masks(uint32_t *requested, uint32_t *mask);
uint32_t runtime_supported_control_mask(void);
GameAddress runtime_platform_close_target(GameMemory *memory);
void runtime_publish_platform_error(GameMemory *memory, uint32_t error);
GameAddress runtime_platform_probe_target(GameMemory *memory,
    unsigned stage);
GameAddress runtime_initializer_target(GameMemory *memory,
    GameAddress cursor);
uint32_t runtime_control_preserved(uint32_t current, uint32_t mask);
uint32_t runtime_control_selected(uint32_t requested, uint32_t mask);
void runtime_multibyte_classification_state(GameMemory *memory,
    uint32_t *locale, GameAddress *table);
uint32_t runtime_multibyte_locale(GameMemory *memory);
uint16_t runtime_decode_folded_multibyte(GameMemory *memory,
    GameAddress *cursor);
uint32_t runtime_ascii_lower(uint32_t value);
uint32_t runtime_character_locale_active(GameMemory *memory);
uint32_t runtime_locale_table_count(GameMemory *memory);
GameAddress runtime_character_table(GameMemory *memory);
uint8_t runtime_character_flags(GameMemory *memory, uint8_t value);
typedef struct RuntimeResourceDescriptor {
    GameAddress owner;
    uint32_t value;
    GameAddress format;
} RuntimeResourceDescriptor;
RuntimeResourceDescriptor runtime_resource_descriptor(GameMemory *memory,
    unsigned index);
GameAddress runtime_resource_formatter(GameMemory *memory);
void runtime_publish_stream_buffer_counter(GameMemory *memory,
    uint32_t value);
void runtime_configure_stream_buffer(GameMemory *memory,
    GameAddress stream, GameAddress buffer, uint32_t flags, uint32_t size);
void runtime_reset_stream_record(GameMemory *memory, GameAddress stream);
GameAddress runtime_publish_locale_state(GameMemory *memory, uint32_t flags,
    uint32_t length, uint32_t trail_state, uint32_t lead_state);

typedef struct RuntimeWindowBounds {
    uint32_t left;
    uint32_t top;
    uint32_t right;
    uint32_t bottom;
} RuntimeWindowBounds;
RuntimeWindowBounds runtime_window_bounds(GameMemory *memory,
    GameAddress object);
void runtime_reposition_window(GameMemory *memory, GameAddress object,
    uint32_t left, uint32_t top, uint32_t tag, uint32_t width,
    uint32_t height);
typedef struct RuntimeCallbackTable {
    GameAddress begin;
    GameAddress end;
} RuntimeCallbackTable;
RuntimeCallbackTable runtime_callback_table(GameMemory *memory);
void runtime_rebase_callback_table(GameMemory *memory, GameAddress begin,
    uint32_t offset);
void runtime_append_callback(GameMemory *memory, GameAddress callback);
void runtime_set_callback_block(GameMemory *memory, GameAddress block);
void runtime_publish_callback_block(GameMemory *memory, GameAddress block);
typedef struct RuntimeArrayCleanupPlan {
    uint32_t completed;
    uint32_t arguments[4];
} RuntimeArrayCleanupPlan;
RuntimeArrayCleanupPlan runtime_array_cleanup_plan(GameMemory *memory,
    GameAddress frame);
GameAddress runtime_display_descriptor(GameMemory *memory);
void runtime_publish_display_descriptor(GameMemory *memory,
    GameAddress descriptor);
void runtime_store_display_descriptor(GameMemory *memory,
    GameAddress descriptor, uint16_t width, uint16_t height,
    GameAddress handle);
typedef struct RuntimeEnvironmentState {
    GameAddress table;
    uint32_t initialized;
} RuntimeEnvironmentState;
RuntimeEnvironmentState runtime_environment_state(GameMemory *memory);
GameAddress runtime_environment_entry(GameMemory *memory,
    GameAddress cursor);
typedef struct RuntimeCallArguments3 {
    uint32_t first;
    uint32_t second;
    uint32_t third;
} RuntimeCallArguments3;
RuntimeCallArguments3 runtime_call_arguments3(GameMemory *memory,
    GameAddress stack);
RuntimeCallArguments3 runtime_scanner_arguments(GameMemory *memory,
    GameAddress stack);
uint32_t runtime_descriptor_value(GameMemory *memory, GameAddress stack);
GameAddress runtime_sync_handle(GameMemory *memory);
void runtime_publish_sync_handle(GameMemory *memory, GameAddress handle);
GameAddress runtime_sync_target(GameMemory *memory, unsigned stage);
GameAddress runtime_default_sync_name(void);
uint32_t runtime_stream_flags(GameMemory *memory, GameAddress stream);
GameAddress runtime_stream_cursor(GameMemory *memory, GameAddress stream);
GameAddress runtime_stream_buffer(GameMemory *memory, GameAddress stream);
uint32_t runtime_stream_count(GameMemory *memory, GameAddress stream);
GameAddress runtime_stream_handle(GameMemory *memory, GameAddress stream);
void runtime_publish_stream_cursor(GameMemory *memory, GameAddress stream,
    GameAddress cursor);
void runtime_publish_stream_byte_state(GameMemory *memory,
    GameAddress stream, uint32_t count, uint32_t flags);
void runtime_store_stream_flags(GameMemory *memory, GameAddress stream,
    uint32_t flags);
void runtime_clear_stream_buffer(GameMemory *memory, GameAddress stream);
void runtime_publish_text_cursor(GameMemory *memory, uint32_t x, uint32_t y);
GameAddress runtime_text_rasterizer_target(void);
GameAddress runtime_text_formatter_target(void);
typedef struct RuntimeBufferedByte {
    GameAddress cursor;
    uint8_t value;
    int available;
} RuntimeBufferedByte;
typedef struct RuntimeStreamFlushState {
    uint32_t flags;
    GameAddress cursor;
    GameAddress base;
    GameAddress handle;
} RuntimeStreamFlushState;
RuntimeBufferedByte runtime_stream_commit_count_and_take(GameMemory *memory,
    GameAddress stream, int32_t count);
uint32_t runtime_locale_mode(GameMemory *memory);
uint16_t runtime_locale_class_value(GameMemory *memory, int32_t character,
    GameAddress *table);
uint32_t runtime_increment_counter(GameMemory *memory, GameAddress counter);
uint32_t runtime_pointer_value(GameMemory *memory, GameAddress pointer);
RuntimeStreamFlushState runtime_stream_flush_state(GameMemory *memory,
    GameAddress stream);
void runtime_stream_finish_flush(GameMemory *memory, GameAddress stream,
    uint32_t flags, GameAddress base, int reset_count);
void runtime_stream_reset_write_cursor(GameMemory *memory,
    GameAddress stream, GameAddress base);
void runtime_stream_finish_close(GameMemory *memory, GameAddress stream,
    uint32_t flags);
GameAddress runtime_stream_single_output_target(void);
void runtime_initialize_format_descriptor(GameMemory *memory,
    GameAddress descriptor, GameAddress cursor, uint32_t remaining,
    uint32_t flags);
GameAddress runtime_terminate_format_output(GameMemory *memory,
    GameAddress cursor);
uint32_t runtime_calendar_month_offset(GameMemory *memory, uint32_t month);
uint32_t runtime_calendar_epoch_base(GameMemory *memory);
uint32_t runtime_calendar_auto_daylight(GameMemory *memory);
uint32_t runtime_calendar_daylight_offset(GameMemory *memory);
uint32_t runtime_handle_limit(GameMemory *memory);
GameAddress runtime_handle_table_block(GameMemory *memory,
    uint32_t block_index);
uint32_t runtime_standard_handles_owned(GameMemory *memory);
uint32_t runtime_standard_handle(uint32_t handle);
GameAddress runtime_standard_handle_target(GameMemory *memory);
void runtime_invalidate_handle_record(GameMemory *memory,
    GameAddress block_slot, uint32_t record_offset);
void runtime_publish_invalid_handle_error(GameMemory *memory);
void runtime_publish_handle_block(GameMemory *memory, uint32_t block_index,
    GameAddress block, uint32_t new_limit);
void runtime_reserve_handle_record(GameMemory *memory, GameAddress record);
typedef struct RuntimeHandleEntry {
    GameAddress block_slot;
    GameAddress block;
    uint32_t record_offset;
    uint32_t value;
    uint8_t flags;
    int valid;
} RuntimeHandleEntry;
RuntimeHandleEntry runtime_handle_entry(GameMemory *memory,
    uint32_t descriptor);
void runtime_store_handle_entry(GameMemory *memory,
    const RuntimeHandleEntry *entry, uint32_t value);
void runtime_clear_handle_entry_flags(GameMemory *memory,
    const RuntimeHandleEntry *entry, uint8_t mask);
GameAddress runtime_address_service_target(GameMemory *memory,
    unsigned stage);
uint32_t runtime_floating_frame_initialized(GameMemory *memory);
void runtime_mark_floating_refresh(GameMemory *memory);
typedef struct RuntimeThreeWordCombine {
    uint32_t destination[3];
    uint32_t source[3];
} RuntimeThreeWordCombine;
RuntimeThreeWordCombine runtime_three_word_combine(GameMemory *memory,
    GameAddress destination, GameAddress source);
void runtime_store_three_word_carry(GameMemory *memory,
    GameAddress destination, uint32_t value);
uint32_t runtime_three_word_destination(GameMemory *memory,
    GameAddress destination, unsigned index);

typedef struct RuntimeWindowStatePlan {
    uint32_t arguments[5];
    GameAddress target;
    uint32_t current_state;
    int invoke;
} RuntimeWindowStatePlan;

RuntimeWindowStatePlan runtime_window_state_plan(GameMemory *memory,
    GameAddress object, int active);
void runtime_window_state_commit(GameMemory *memory, GameAddress object,
    int active);

typedef struct RuntimePseudoHandleResult {
    uint32_t value;
    GameAddress tail_target;
    int tail_dispatch;
} RuntimePseudoHandleResult;

int runtime_resolve_pseudo_handle(GameMemory *memory, uint32_t value,
    RuntimePseudoHandleResult *result);
int runtime_resolve_punctuation(GameMemory *memory, uint8_t code,
    uint32_t *selector, GameAddress *target);

#endif
