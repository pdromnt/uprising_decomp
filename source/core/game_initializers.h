#ifndef UPRISING_NATIVE_GAME_INITIALIZERS_H
#define UPRISING_NATIVE_GAME_INITIALIZERS_H

#include "game_memory.h"

#include <stdint.h>

typedef struct GamePresentationDescriptorEntry {
    GameAddress slot;
    GameAddress current;
    GameAddress descriptor;
    GameAddress primary;
    GameAddress payload;
    uint8_t subtype;
    uint8_t type;
} GamePresentationDescriptorEntry;

typedef struct GamePresentationResourceBucket {
    GameAddress slot;
    GameAddress bucket;
    GameAddress array;
    uint32_t count;
    uint8_t type;
} GamePresentationResourceBucket;

void game_initializer_004129b0(GameMemory *memory, GameAddress object);
void game_initializer_00519330(GameMemory *memory, uint32_t value);
void game_initializer_004e9660(GameMemory *memory);
uint32_t game_initializer_004ac1a0(GameMemory *memory);
void game_initializer_00413690(GameMemory *memory, GameAddress object,
    uint32_t value);
void game_initializer_0049a6c0(GameMemory *memory, GameAddress object);
void game_initializer_0048fab0(GameMemory *memory, GameAddress object);
void game_initializer_00548a90(GameMemory *memory);
void game_initializer_004aa490(GameMemory *memory, GameAddress object);
void game_initializer_004d7660(GameMemory *memory);
void game_initializer_004a3980(GameMemory *memory, GameAddress object);
void game_initializer_004b2d80(GameMemory *memory);
void game_initializer_004cc490(GameMemory *memory);
void game_initializer_00452730(GameMemory *memory, GameAddress object);
void game_initializer_00519ee0(GameMemory *memory);
void game_initializer_00453eb0(GameMemory *memory, GameAddress object,
    uint8_t kind, uint8_t subtype, uint32_t payload);
void game_initializer_004d8ae0(GameMemory *memory);
void game_initializer_004f0190(GameMemory *memory, int clear_buffer);
uint32_t game_initializer_00518d10(GameMemory *memory);
void game_initializer_00551490(GameMemory *memory, GameAddress object);
GameAddress game_initializer_005514f0(GameMemory *memory, GameAddress object);
void game_initializer_00549710(GameMemory *memory, GameAddress object);
GameAddress game_initializer_00519230(GameMemory *memory);
void game_initializer_00518d50(GameMemory *memory, GameAddress destination);
GameAddress game_initializer_00518c00(GameMemory *memory);
void game_initializer_0051bec0(GameMemory *memory, GameAddress object,
    uint32_t first, uint32_t second, uint32_t third, uint32_t fourth);
void game_initializer_0042cfb0(GameMemory *memory, GameAddress object);
void game_initializer_0042d180(GameMemory *memory, GameAddress object,
    GameAddress vector, uint32_t second, uint32_t third, uint32_t fourth,
    uint32_t fifth);
void game_initializer_five_field_record(GameMemory *memory,
    GameAddress object, uint32_t vtable, const uint32_t values[5],
    int first_layout);
void game_initializer_0048f320(GameMemory *memory, GameAddress object);
void game_initializer_004dbca0(GameMemory *memory, GameAddress object,
    uint32_t value);
void game_initializer_004df740(GameMemory *memory, GameAddress object,
    uint32_t value);
void game_initializer_0051bf40(GameMemory *memory, GameAddress object,
    uint32_t first, uint32_t second);
void game_initializer_0043ca30(GameMemory *memory, GameAddress object,
    uint32_t first, GameAddress source);
void game_initializer_004a6d90(GameMemory *memory);
void game_initializer_004a6df0(GameMemory *memory);
void game_initializer_004b33a0(GameMemory *memory);
void game_initializer_004b3960(GameMemory *memory);
void game_initializer_0047afd0(GameMemory *memory, GameAddress object,
    uint32_t selection, GameAddress first, GameAddress second);
void game_initializer_004dc380(GameMemory *memory, GameAddress object,
    uint32_t first, uint32_t second, uint32_t third, uint32_t provider_index,
    uint32_t fallback_index);
void game_initializer_004dfbb0(GameMemory *memory, GameAddress object,
    uint32_t provider_index, uint32_t fallback_index, uint32_t first,
    uint32_t second, GameAddress name);
uint32_t game_initializer_clear_mask(GameMemory *memory, GameAddress object,
    uint32_t mask);
void game_initializer_004cec80(GameMemory *memory, GameAddress object);
uint32_t game_initializer_object_type(GameMemory *memory, GameAddress object,
    GameAddress *state);
void game_initializer_game_defaults_before_callback(GameMemory *memory);
void game_initializer_game_defaults_after_callback(GameMemory *memory);
void game_initializer_character_alphabet(GameMemory *memory);
uint8_t game_initializer_character_noise_value(uint32_t random_value,
    uint32_t multiplier, uint8_t base);
void game_initializer_record_catalog_table(GameMemory *memory,
    GameAddress self, uint32_t offset, GameAddress table);
void game_initializer_record_catalog_records(GameMemory *memory,
    GameAddress records);
void game_initializer_resource_owner_begin(GameMemory *memory,
    GameAddress self, uint32_t first_selector, uint32_t second_selector,
    uint32_t first_index, uint32_t second_index, uint32_t trailing_value);
void game_initializer_resource_owner_reference(GameMemory *memory,
    GameAddress self, uint32_t offset, GameAddress reference);
void game_initializer_presentation_registry_begin(GameMemory *memory);
void game_initializer_presentation_registry_primary(GameMemory *memory,
    GameAddress primary);
void game_initializer_presentation_registry_finish(GameMemory *memory,
    GameAddress primary, GameAddress secondary);
uint32_t game_initializer_presentation_controller(GameMemory *memory,
    GameAddress self);
void game_initializer_presentation_variant(GameMemory *memory,
    GameAddress self, GameAddress vtable, uint8_t kind,
    GameAddress first_callback, GameAddress second_callback);
void game_initializer_presentation_node(GameMemory *memory,
    GameAddress self, uint32_t flags, GameAddress vtable, uint8_t kind,
    GameAddress first_callback, GameAddress second_callback);
void game_initializer_presentation_runtime(GameMemory *memory,
    GameAddress self, uint32_t flags);
void game_initializer_type_004528_runtime(GameMemory *memory);
void game_initializer_runtime_descriptor(GameMemory *memory,
    GameAddress descriptor);
void game_initializer_large_work_area(GameMemory *memory, GameAddress self);
int game_initializer_effect_random_table(GameMemory *); /*53cc40*/
void game_initializer_type_004fd2_service(GameMemory *memory);
void game_initializer_render_resource_holder(GameMemory *memory,
    GameAddress self);
void game_initializer_flagged_object(GameMemory *memory, GameAddress self,
    GameAddress vtable, uint32_t flags, uint8_t type, int set_type,
    uint32_t extra_byte_offset, uint8_t extra_byte_value);
void game_initializer_type45_runtime(GameMemory *memory, GameAddress self);
void game_initializer_object_identity(GameMemory *memory, GameAddress self,
    GameAddress vtable, uint8_t type, int set_type);
void game_initializer_type22_identity(GameMemory *memory, GameAddress self,
    GameAddress vtable, uint8_t type, GameAddress primary_store,
    GameAddress secondary_store);
void game_initializer_linked_object(GameMemory *memory, GameAddress self,
    GameAddress vtable, uint8_t type, uint32_t flags,
    uint32_t first_clear_offset, uint32_t second_clear_offset,
    uint32_t byte_offset, uint8_t byte_value);
void game_initializer_type40_object(GameMemory *memory, GameAddress self,
    GameAddress vtable, uint8_t type, uint32_t flags);
void game_initializer_multistage_object(GameMemory *memory, GameAddress self,
    GameAddress first_vtable, GameAddress second_vtable,
    GameAddress final_vtable, uint8_t type, uint32_t flags);
void game_initializer_base_gameplay_record(GameMemory *memory,
    GameAddress self, uint32_t flags);
void game_initializer_presentation_object(GameMemory *memory,
    GameAddress self, uint32_t flags);
void game_initializer_source_copy_object(GameMemory *memory,
    GameAddress self, GameAddress source, uint32_t flags);
void game_initializer_gameplay_controller(GameMemory *memory,
    GameAddress self);
void game_initializer_inline_owner_object(GameMemory *memory,
    GameAddress self, GameAddress vtable, uint8_t type, uint32_t flags,
    GameAddress owner);
void game_initializer_type0c_runtime(GameMemory *memory, GameAddress self);
void game_initializer_type28_runtime(GameMemory *memory, GameAddress self,
    uint32_t flags);
void game_initializer_runtime_descriptor_object(GameMemory *memory,
    GameAddress self);
void game_initializer_pointer_ring(GameMemory *memory, GameAddress self);
void game_initializer_pointer_ring_finish(GameMemory *memory,
    GameAddress self);
void game_initializer_type00416x(GameMemory *memory, GameAddress self,
    GameAddress vtable, unsigned variant, uint32_t value);
void game_initializer_presentation_buffer_controller(GameMemory *memory,
    GameAddress self);
void game_initializer_store_presentation_buffer(GameMemory *memory,
    GameAddress self, unsigned index, GameAddress buffer);
uint32_t game_initializer_global_backing_size(unsigned index);
void game_initializer_store_global_backing(GameMemory *memory,
    unsigned index, GameAddress buffer);
void game_initializer_type0045fd(GameMemory *memory, GameAddress self,
        uint32_t flags, GameAddress link);
/* Copy the shared scene prototype and finish either level-owned teleporter
 * class. Buffer allocation and resource cloning are supplied by the caller. */
void game_initializer_teleporter_scene_object(GameMemory *memory,
    GameAddress object, GameAddress prototype, GameAddress buffer,
    GameAddress resource, int destination);
void game_initializer_effect_object_begin(GameMemory *memory,
    GameAddress self, GameAddress source);
void game_initializer_effect_object_finish(GameMemory *memory,
    GameAddress self);
void game_initializer_store_type0049_runtime(GameMemory *memory,
    uint32_t index, GameAddress object);
void game_initializer_type0049ad(GameMemory *memory, GameAddress self,
    uint32_t first, uint16_t second);
void game_initializer_reciprocal_table(GameMemory *memory,
    GameAddress table);
void game_initializer_scratch_record(GameMemory *memory, uint32_t index,
    GameAddress record);
void game_initializer_scratch_records_finish(GameMemory *memory);
void game_initializer_shared_presentation_header(GameMemory *memory,
    GameAddress self);
GamePresentationDescriptorEntry game_initializer_presentation_descriptor_entry(
    GameMemory *memory, GameAddress self, unsigned index, int payload);
void game_initializer_store_presentation_descriptor(GameMemory *memory,
    const GamePresentationDescriptorEntry *entry, GameAddress object);
uint32_t game_initializer_shared_presentation_active(GameMemory *memory,
    GameAddress self);
void game_initializer_publish_shared_presentation(GameMemory *memory,
    GameAddress object);
void game_initializer_set_shared_presentation_active(GameMemory *memory,
    GameAddress self, uint32_t active);
void game_initializer_store_shared_presentation_interface(GameMemory *memory,
    GameAddress self, GameAddress interface_object);
GamePresentationResourceBucket game_initializer_presentation_resource_bucket(
    GameMemory *memory, GameAddress self, unsigned index);
GameAddress game_initializer_presentation_bucket_object(GameMemory *memory,
    const GamePresentationResourceBucket *bucket, unsigned index);
void game_initializer_clear_presentation_bucket_object(GameMemory *memory,
    const GamePresentationResourceBucket *bucket, unsigned index);
void game_initializer_clear_presentation_bucket(GameMemory *memory,
    const GamePresentationResourceBucket *bucket);
void game_initializer_type004793_finish(GameMemory *memory,
    GameAddress self);
void game_initializer_gameplay_presentation_controller(GameMemory *memory,
    GameAddress self);
void game_initializer_clear_gameplay_presentation_link(GameMemory *memory,
    GameAddress self);
GameAddress game_initializer_shared_presentation_resource(GameMemory *memory);
void game_initializer_store_shared_presentation_resource(GameMemory *memory,
    GameAddress resource);
GameAddress game_initializer_shared_presentation_source(GameMemory *memory);
GameAddress game_initializer_shared_presentation_owner(GameMemory *memory);
GameAddress game_initializer_descriptor_resource_begin(GameMemory *memory,
    GameAddress self, GameAddress descriptor);
void game_initializer_descriptor_resource_activate(GameMemory *memory,
    GameAddress self);
void game_initializer_store_descriptor_resource_handle(GameMemory *memory,
    GameAddress self, GameAddress handle);
GameAddress game_initializer_presentation_selector_record(GameMemory *memory,
    uint32_t selector);
GameAddress game_initializer_presentation_descriptor_payload(
    GameMemory *memory, GameAddress descriptor);
void game_initializer_store_presentation_object(GameMemory *memory,
    GameAddress self, GameAddress object);
void game_initializer_resource_stream_begin(GameMemory *memory);
GameAddress game_initializer_resource_stream(GameMemory *memory);
GameAddress game_initializer_resource_stream_path(GameMemory *memory);
void game_initializer_resource_stream_opening(GameMemory *memory);
void game_initializer_store_resource_stream(GameMemory *memory,
    GameAddress stream);
void game_initializer_store_resource_record_size(GameMemory *memory,
    GameAddress destination, uint32_t size);
void game_initializer_named_record_begin(GameMemory *memory,
    GameAddress object);
void game_initializer_store_named_record_label(GameMemory *memory,
    GameAddress object, GameAddress label);
void game_initializer_named_record_finish(GameMemory *memory,
    GameAddress object);
void game_initializer_gameplay_service_tables(GameMemory *memory,
    int backwards);

#endif
