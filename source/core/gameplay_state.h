#ifndef UPRISING_NATIVE_GAMEPLAY_STATE_H
#define UPRISING_NATIVE_GAMEPLAY_STATE_H

#include "game_memory.h"
#include "gameplay_catalog.h"
#include "tagged_scalar_stream.h"
#include "recyclable_records.h"
#include "player_feedback.h"

typedef struct GameplayEntityContactServices {
    void *context;
    int (*damage)(void *,GameAddress object,GameAddress other,int32_t *amount);
    int (*killed)(void *,GameAddress object,GameAddress other);
    int (*reset)(void *,GameAddress object,GameAddress method);
    GamePresentationFeedbackServices feedback;
} GameplayEntityContactServices;
int gameplay_group_note_attack(GameMemory *,GameAddress controller,GameAddress member,GameAddress other);
int gameplay_entity_contact(GameMemory *,GameAddress object,GameAddress other,const GameplayEntityContactServices *);
typedef int (*GameplayKillCredit)(void *,GameAddress catalog,uint32_t amount);
int gameplay_statistics_note(GameMemory *,GameAddress statistics,uint8_t type,int loss,uint32_t *value);
uint32_t gameplay_statistics_reward(GameMemory *,uint8_t type);
int gameplay_entity_note_kill(GameMemory *,GameAddress,GameAddress,void *,GameplayKillCredit);
typedef struct GameplayContainerArchiveServices {
    const TaggedRecordStreamServices *records;
    const RecyclableRecordServices *storage;
    void *context;
    int (*reset_foreign)(void *,GameAddress);
    int (*append_foreign)(void *,GameAddress,GameAddress);
} GameplayContainerArchiveServices;
int gameplay_state_stream_outer(GameMemory *,int reading,uint32_t *marker,
    const GameplayContainerArchiveServices *,GameAddress);
int gameplay_state_stream_container(GameMemory *,int reading,uint32_t *marker,
    const GameplayContainerArchiveServices *,GameAddress);
int gameplay_state_stream_grid(GameMemory *,int reading,int alternate,uint32_t *marker,
    const GameplayContainerArchiveServices *,GameAddress);
int gameplay_state_load_settings(GameMemory *,uint32_t *marker,
    const TaggedRecordStreamServices *);
int gameplay_state_stream_presentation(GameMemory *,int reading,
    uint32_t *last_elements,const TaggedRecordStreamServices *);
typedef int (*GameplayArchiveResolve)(void *,uint32_t identity,GameAddress *);
/* Root/entity ownership stays with its existing factory. These ports are only
 * for distinct member classes and catalog variants still being reconstructed;
 * record transport and linked-list reconstruction execute native C. */
typedef struct GameplayRootArchiveServices {
    const TaggedRecordStreamServices *records;
    void *context;
    int (*member)(void *,GameAddress,unsigned operation);
    int (*variant)(void *,GameAddress root,uint8_t kind,uint32_t *);
    int (*create)(void *,uint8_t kind,uint16_t variant,GameAddress *);
    int (*bind_group)(void *,GameAddress group,GameAddress catalog,uint32_t enabled);
    int (*bind_group_member)(void *,GameAddress group,uint16_t identity,unsigned slot);
    GameplayArchiveResolve resolve;
} GameplayRootArchiveServices;
int gameplay_state_stream_root(GameMemory *,int reading,uint32_t *marker,
    const GameplayRootArchiveServices *,GameAddress);
int gameplay_state_fix_root(GameMemory *,const GameplayRootArchiveServices *,GameAddress);
int gameplay_state_stream_group(GameMemory *,int reading,uint32_t *marker,
    const GameplayRootArchiveServices *,GameAddress);
int gameplay_state_fix_group(GameMemory *,const GameplayRootArchiveServices *,GameAddress);
int gameplay_state_stream_player_root(GameMemory *,int reading,uint32_t *marker,
    const GameplayRootArchiveServices *,GameAddress);
int gameplay_state_save_player_root_base(GameMemory *,uint32_t *marker,
    const GameplayRootArchiveServices *,GameAddress);
int gameplay_state_stream_player_root_base(GameMemory *,int reading,uint32_t *marker,
    const GameplayRootArchiveServices *,GameAddress);
int gameplay_state_fix_player_root(GameMemory *,const GameplayRootArchiveServices *,GameAddress);
int gameplay_state_stream_group_member(GameMemory *,int reading,unsigned family,uint32_t *marker,
    const GameplayRootArchiveServices *,GameAddress);
int gameplay_state_fix_group_member(GameMemory *,unsigned family,const GameplayRootArchiveServices *,GameAddress);
int gameplay_state_stream_factions(GameMemory *,int reading,const GameplayRootArchiveServices *);
typedef struct GameplayGroupConstruction {
    uint32_t identity,price;
    GameAddress bounds,positions,metadata,rectangles;
    uint16_t secondary_count,rectangle_count;
} GameplayGroupConstruction;
typedef struct GameplayGroupServices {
    void *context;
    GameAddress (*allocate)(void *,size_t);
    int (*release)(void *,GameAddress);
    /* Reset and destroy are distinct child methods still being recovered. */
    int (*member_reset)(void *,GameAddress);
    int (*member_destroy)(void *,GameAddress);
    int (*catalog_value)(void *,GameAddress,uint8_t kind,unsigned property,uint32_t *);
    int (*budget)(void *,GameAddress,uint32_t price,int consume,uint32_t *);
    int (*create)(void *,uint8_t kind,uint16_t variant,GameAddress *);
    int (*member_activate)(void *,GameAddress,uint32_t tick,int enabled);
    int (*member_disable)(void *,GameAddress);
} GameplayGroupServices;
int gameplay_group_construct(GameMemory *,const GameplayGroupServices *,GameAddress,const GameplayGroupConstruction *);
int gameplay_group_destroy(GameMemory *,const GameplayGroupServices *,GameAddress);
int gameplay_group_reset(GameMemory *,const GameplayGroupServices *,GameAddress);
typedef struct GameplayGroupDetachServices {
    void *context;
    int (*disable)(void *,GameAddress,int enabled);
    int (*stop)(void *,GameAddress);
    int (*retire)(void *,GameAddress);
} GameplayGroupDetachServices;
int gameplay_group_remove_member(GameMemory *,const GameplayGroupDetachServices *,GameAddress,int32_t slot);
int gameplay_descriptor_detach(GameMemory *,const GameplayGroupDetachServices *,GameAddress,int auxiliaries);
typedef int (*GameplayPayloadPropertyQuery)(void *,GameAddress,GameAddress method,uint32_t *);
int gameplay_payload_set_disabled(GameMemory *,GameAddress,int articulated,uint32_t enabled,void *,GameplayPayloadPropertyQuery);
int gameplay_group_activate(GameMemory *,const GameplayGroupServices *,GameAddress,GameAddress catalog,uint32_t enabled,uint16_t *status);
int gameplay_group_primary_construct(GameMemory *,const GameplayGroupServices *,GameAddress,GameAddress descriptor);
int gameplay_group_member_construct(GameMemory *,GameAddress,GameAddress descriptor,uint16_t slot,GameAddress payload,uint16_t identity,unsigned family);
int gameplay_group_place_member(GameMemory *,const GameplayGroupServices *,GameAddress,GameAddress payload,uint32_t slot);
int gameplay_group_bind_member(GameMemory *,const GameplayGroupServices *,const GameplayUnitServices *,GameAddress,int16_t identity,uint32_t slot,uint16_t *status);
/* The descriptor owns its payload reference; factory scene objects keep their
 * existing scene-list owners. Family 0 is primary, 1/2 the secondary classes.
 * Ports below describe distinct payload behaviors still being reconstructed. */
typedef struct GameplayDescriptorServices {
    void *context;
    int (*catalog_value)(void *,GameAddress,uint8_t,unsigned,uint32_t,uint32_t *);
    int (*create)(void *,uint8_t,uint16_t,GameAddress *);
    int (*next_sequence)(void *,GameAddress,uint32_t *);
    int (*publish)(void *,GameAddress,GameAddress);
    int (*attach)(void *,GameAddress,uint32_t,const float[3],uint16_t);
    int (*bind_payload)(void *,GameAddress,unsigned,uint32_t,const float[3],uint16_t,GameAddress,int);
    int (*duration)(void *,GameAddress,int32_t *);
    int (*token)(void *,GameAddress,uint32_t *);
    int (*stop)(void *,GameAddress);
    int (*retire)(void *,GameAddress,unsigned,int auxiliary);
    int (*transition)(void *,GameAddress,uint32_t,GameAddress);
    int (*notify)(void *,GameAddress);
    int (*unit_value)(void *,GameAddress,uint8_t,int32_t *);
    int (*credit)(void *,GameAddress,int32_t);
} GameplayDescriptorServices;
int gameplay_descriptor_bind(GameMemory *,const GameplayDescriptorServices *,GameAddress,unsigned family,uint32_t tick,uint32_t restored);
int gameplay_descriptor_destroy(GameMemory *,const GameplayDescriptorServices *,GameAddress,unsigned family,uint32_t mode);
typedef struct GameplayDescriptorUpdateServices {
    const GameplayDescriptorServices *lifecycle;
    void *context;
    int (*sound)(void *,GameAddress position);
    int (*phase)(void *,GameAddress,uint32_t);
    int (*relocate)(void *,GameAddress,GameAddress position,uint16_t);
    int (*refresh)(void *,GameAddress,unsigned method_slot,GameAddress argument);
    int (*settled)(void *,GameAddress);
    int (*activity)(void *,GameAddress,int *);
    int (*intersect)(void *,GameAddress,GameAddress,float[3],int *);
    int (*contact)(void *,GameAddress,GameAddress,const uint32_t[3],const float[3],uint32_t *);
    GameAddress (*allocate_contact)(void *);
    int (*release_contacts)(void *,GameAddress);
} GameplayDescriptorUpdateServices;
int gameplay_descriptor_motion(GameMemory *,GameAddress,float height,const float position[3]);
int gameplay_descriptor_contacts(GameMemory *,const GameplayDescriptorUpdateServices *,GameAddress,GameAddress *);
int gameplay_descriptor_update(GameMemory *,const GameplayDescriptorUpdateServices *,GameAddress,unsigned family);
typedef struct GameplayEntityArchiveResult {int resolved_link;GameAddress last_resolution;} GameplayEntityArchiveResult;
int gameplay_state_stream_entities(GameMemory *,int reading,uint32_t *marker,
    const TaggedRecordStreamServices *,GameplayArchiveResolve,void *,GameplayEntityArchiveResult *);

typedef int (*GameplayRadarReveal)(void *context,GameAddress position,float radius);
int gameplay_selector_refresh(GameMemory *memory,int alternate,
    GameplayRadarReveal reveal,void *context);

#include <stdint.h>
int gameplay_state_queue_packet(GameMemory *,GameAddress packet);

typedef struct GameplayCollectionUpdateResult {
    uint32_t processed;
    uint32_t final_count;
} GameplayCollectionUpdateResult;

typedef struct GameplayRotatingTextResult {
    uint32_t destination;
    uint32_t current_index;
    uint32_t current_primary;
    uint32_t previous_tertiary;
    int changed;
} GameplayRotatingTextResult;

typedef enum GameplaySlotReplacementKind {
    GAMEPLAY_SLOT_NOT_FOUND = 0,
    GAMEPLAY_SLOT_PRIMARY,
    GAMEPLAY_SLOT_PRIMARY_CLEARED,
    GAMEPLAY_SLOT_FALLBACK
} GameplaySlotReplacementKind;

typedef struct GameplaySlotReplacementResult {
    GameplaySlotReplacementKind kind;
    uint32_t replacement;
    uint32_t primary_table;
    uint32_t empty_map;
    uint32_t index;
} GameplaySlotReplacementResult;

typedef struct GameplayEntityResetPlan {
    GameAddress owner;
    GameAddress target;
    uint32_t selector;
} GameplayEntityResetPlan;

typedef struct GameplayEntityRelinkResult {
    GameAddress head;
    GameAddress old_first;
} GameplayEntityRelinkResult;

typedef struct GameplayWeightedSlotResult {
    uint32_t slot;
    uint32_t final_remainder;
    uint32_t increment;
} GameplayWeightedSlotResult;

typedef struct GameplayEntityBucketRefreshResult {
    uint32_t tick;
    uint32_t timer;
    int32_t count;
    int refreshed;
} GameplayEntityBucketRefreshResult;

typedef struct GameplayCalendarExpansionResult {
    uint32_t year;
    uint32_t day_of_year;
    uint32_t month;
    uint32_t day_of_month;
    uint32_t weekday;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
    uint32_t seconds_within_hour;
} GameplayCalendarExpansionResult;

typedef struct GameplayPackedCalendarResult {
    uint32_t calendar_index;
    uint32_t raw_packed;
    uint32_t packed;
    uint32_t packed_scale_125;
    uint32_t local_value;
    int wrote_local;
} GameplayPackedCalendarResult;

typedef struct GameplaySubmissionSample {
    uint32_t original_cursor;
    uint32_t cursor;
    uint32_t next_cursor;
    GameAddress scalar;
    GameAddress table;
    int scaled;
} GameplaySubmissionSample;

typedef struct GameplayVirtualCallPlan {
    GameAddress vtable;
    GameAddress target;
} GameplayVirtualCallPlan;

typedef struct GameplayReleasePlan {
    uint32_t active;
    uint32_t handle;
    GameAddress target;
} GameplayReleasePlan;

typedef struct GameplayModeTransitionPlan {
    uint32_t secondary;
    uint32_t interface_handle;
    GameAddress virtual_target;
} GameplayModeTransitionPlan;

typedef struct GameplayInterfaceTeardownPlan {
    uint32_t interface_handle;
    uint32_t secondary;
    GameAddress release_target;
} GameplayInterfaceTeardownPlan;

typedef struct GameplayPairReplacementPlan {
    GameAddress first_peer;
    GameAddress second_peer;
    GameAddress forwarded;
    GameAddress vtable;
    GameAddress target;
    unsigned branch;
} GameplayPairReplacementPlan;

typedef struct GameplayInterfaceSlotPlan {
    GameAddress slot;
    GameAddress current;
    GameAddress first_table;
    GameAddress second_table;
} GameplayInterfaceSlotPlan;

typedef struct GameplayRecordDelegationPlan {
    uint32_t global_state;
    uint32_t slot;
    uint32_t group;
    GameAddress owner;
    GameAddress table_value;
    uint8_t inhibit;
} GameplayRecordDelegationPlan;

typedef struct GameplayPresentationBufferResult {
    uint32_t index;
    uint32_t next_index;
    uint16_t type;
} GameplayPresentationBufferResult;

typedef struct GameplayPresentationModePlan {
    GameAddress source;
    GameAddress nested;
    GameAddress table_object;
    uint32_t nested_state;
    uint32_t source_mode;
    uint32_t shared_state;
    uint32_t selector;
} GameplayPresentationModePlan;

typedef struct GameplayTypedPresentationPlan {
    GameAddress runtime;
    uint32_t record_state;
    uint32_t owner_type;
    uint32_t first;
    uint32_t second;
    uint32_t third;
} GameplayTypedPresentationPlan;

typedef struct GameplayIndexedDeliveryNode {
    GameAddress next;
    GameAddress payload;
    GameAddress nested;
    uint32_t key;
    GameAddress callback;
} GameplayIndexedDeliveryNode;

typedef struct GameplayActiveObjectPlan {
    GameAddress active;
    GameAddress first_previous;
    GameAddress second_previous;
} GameplayActiveObjectPlan;

typedef struct GameplayLinkedStatePlan {
    uint32_t previous;
    uint32_t preflight_mode;
    uint32_t final_mode;
} GameplayLinkedStatePlan;

typedef struct GameplayConfigurationSnapshot {
    uint32_t first;
    uint32_t second;
    uint32_t third;
    uint16_t packed_first;
    uint16_t packed_second;
    uint16_t packed_third;
    GameAddress vtable;
    GameAddress first_target;
    GameAddress second_target;
} GameplayConfigurationSnapshot;

typedef struct GameplayCallbackReplacementPlan {
    GameAddress active;
    GameAddress callback;
    GameAddress fallback;
    GameAddress context;
} GameplayCallbackReplacementPlan;

typedef struct GameplayComponentLookupPlan {
    uint32_t raw_group;
    uint32_t group;
    uint8_t type;
    GameAddress provider;
    GameplayVirtualCallPlan lookup;
} GameplayComponentLookupPlan;

typedef struct GameplayViewportLayoutPlan {
    uint32_t selector;
    uint32_t width;
    uint32_t enabled;
    uint32_t lower;
    uint32_t upper;
    uint32_t span;
    uint32_t end;
    int update_span;
    int early_zero_layout;
} GameplayViewportLayoutPlan;

typedef struct GameplayViewportClampResult {
    uint32_t x;
    uint32_t y;
    uint32_t compared_y;
    uint32_t maximum_y;
} GameplayViewportClampResult;

typedef struct GameplayObjectMessageSpec {
    uint32_t guard_offset;
    uint32_t message;
    GameAddress return_address;
    int guarded;
} GameplayObjectMessageSpec;

typedef struct GameplayWorldQuerySnapshot {
    uint32_t world[4];
    uint32_t object_first;
    uint32_t object_second;
} GameplayWorldQuerySnapshot;

typedef struct GameplayObjectStatusSnapshot {
    uint32_t global_active;
    uint32_t handle;
    uint32_t global_owner;
    uint32_t handle_owner;
    uint8_t inhibited;
    uint32_t threshold;
    GameAddress virtual_target;
} GameplayObjectStatusSnapshot;

typedef struct GameplayStateRangeSnapshot {
    int32_t count;
    int32_t base;
} GameplayStateRangeSnapshot;

typedef struct GameplayScalarTableSpec {
    GameAddress destination;
    uint32_t count;
    uint32_t scalar_bits;
} GameplayScalarTableSpec;

typedef struct GameplayNumericTableSpec {
    GameAddress output;
    GameAddress end;
    GameAddress scalar;
    uint32_t count;
} GameplayNumericTableSpec;

typedef struct GameplayRecordGroupSpec {
    uint32_t count;
    uint32_t stride;
    GameAddress output;
    GameAddress sources[4];
    uint32_t flag;
    uint32_t allocate_return;
    uint32_t construct_return;
} GameplayRecordGroupSpec;

typedef struct GameplayRuntimeViewCopyResult {
    uint32_t final_eax;
    GameAddress final_ecx;
    GameAddress final_edx;
} GameplayRuntimeViewCopyResult;

typedef struct GameplayRuntimeViewSourceResult {
    GameAddress final_ecx;
    uint32_t final_edx;
} GameplayRuntimeViewSourceResult;

typedef struct GameplayStatusDispatchPlan {
    GameAddress state;
    uint32_t kind;
    GameAddress active;
} GameplayStatusDispatchPlan;

typedef struct GameplayObjectCallPlan {
    GameAddress object;
    GameAddress vtable;
    GameAddress target;
} GameplayObjectCallPlan;

uint32_t gameplay_state_store_counter(GameMemory *memory, GameAddress self,
    uint32_t value);
uint32_t gameplay_state_copy_global(GameMemory *memory, GameAddress self);
uint32_t gameplay_state_increment_indexed(GameMemory *memory, GameAddress self,
    int16_t index);
int gameplay_state_counter_at_least(GameMemory *memory, GameAddress self,
    uint32_t value);
uint32_t gameplay_state_subtract_counter(GameMemory *memory, GameAddress self,
    uint32_t amount);
void gameplay_state_add_counters(GameMemory *memory, GameAddress self,
    uint32_t amount, uint32_t *primary, uint32_t *secondary);
int gameplay_state_entry_matches(GameMemory *memory, GameAddress self);
void gameplay_state_publish_value(GameMemory *memory, GameAddress self);
int gameplay_state_flag_set(GameMemory *memory, GameAddress self, uint8_t mask);
uint32_t gameplay_state_class_slot(GameMemory *memory, GameAddress self,
    uint32_t slot_offset, int honor_disabled);
void gameplay_state_clear_words(GameMemory *memory, GameAddress self,
    uint32_t offset, uint32_t count);
uint16_t gameplay_state_update_angle(GameMemory *memory, GameAddress self,
    int *updated);
void gameplay_state_reset_actor(GameMemory *memory, GameAddress self);
uint32_t gameplay_state_increment_command(GameMemory *memory, GameAddress self,
    uint8_t command, int *matched);
void gameplay_state_apply_palette(GameMemory *memory, GameAddress self,
    uint16_t palette_index);
void gameplay_state_reset_large_object(GameMemory *memory, GameAddress self);
GameplayCollectionUpdateResult gameplay_state_offset_collection(
    GameMemory *memory, GameAddress collection, uint32_t first_delta,
    uint32_t second_delta);
GameplayCollectionUpdateResult gameplay_state_assign_collection(
    GameMemory *memory, GameAddress collection, uint32_t first_value,
    uint32_t second_value);
uint32_t gameplay_state_advance_ping_pong(GameMemory *memory,
    GameAddress object);
GameplayRotatingTextResult gameplay_state_update_rotating_text(
    GameMemory *memory, GameAddress object, GameAddress destination,
    int backwards);
GameplaySlotReplacementResult gameplay_state_replace_entity_slot(
    GameMemory *memory, GameAddress manager, GameAddress target);
GameplayEntityResetPlan gameplay_state_prepare_entity_reset(
    GameMemory *memory, GameAddress self);
GameplayEntityRelinkResult gameplay_state_finish_entity_reset(
    GameMemory *memory, GameAddress self);
GameplayWeightedSlotResult gameplay_state_apply_weighted_slot(
    GameMemory *memory, GameAddress object, uint32_t slot,
    uint32_t candidate, uint32_t first_weight, uint32_t second_weight,
    uint32_t increment);
GameplayEntityBucketRefreshResult gameplay_state_refresh_entity_buckets(
    GameMemory *memory, GameAddress object, int forced, int backwards);
GameplayCalendarExpansionResult gameplay_state_expand_calendar(
    GameMemory *memory, uint32_t packed);
GameplayPackedCalendarResult gameplay_state_pack_calendar(
    GameMemory *memory, const uint32_t arguments[11]);
GameplaySubmissionSample gameplay_state_prepare_submission_sample(
    GameMemory *memory, unsigned mode);
void gameplay_state_commit_submission_sample(GameMemory *memory,
    const GameplaySubmissionSample *sample);
uint32_t gameplay_state_submission_table_value(GameMemory *memory,
    unsigned mode, uint32_t index);
GameplayVirtualCallPlan gameplay_state_virtual_call_plan(GameMemory *memory,
    GameAddress object, uint32_t slot);
int gameplay_state_low_flag_set(uint32_t value);
int gameplay_state_refresh_due(GameMemory *memory, GameAddress object,
    uint32_t interval);
GameAddress gameplay_state_link_next(GameMemory *memory, GameAddress node,
    uint32_t offset);
uint32_t gameplay_state_merge_entity_flags(GameMemory *memory,
    GameAddress entity, uint32_t flags);
void gameplay_state_publish_entity_grid(GameMemory *memory,
    GameAddress output, int16_t x, int16_t y, GameAddress entity);
GameplayReleasePlan gameplay_state_release_plan(GameMemory *memory,
    GameAddress object);
int gameplay_state_cached_link_matches(GameMemory *memory,
    GameAddress cached, uint32_t generation);
GameplayComponentLookupPlan gameplay_state_component_lookup_plan(
    GameMemory *memory, GameAddress object);
GameplayVirtualCallPlan gameplay_state_component_check_plan(
    GameMemory *memory, GameAddress object);
uint16_t gameplay_state_component_word(GameMemory *memory,
    GameAddress component, uint32_t live_offset, uint32_t fallback_offset,
    int fallback);
uint32_t gameplay_state_component_value(GameMemory *memory,
    GameAddress component, uint32_t live_offset, uint32_t fallback_offset,
    int fallback);
GameplayVirtualCallPlan gameplay_state_status_forward_plan(
    GameMemory *memory, GameAddress object, uint32_t required_state,
    uint32_t member_offset, uint32_t slot);
GameplayViewportLayoutPlan gameplay_state_viewport_layout_plan(
    GameMemory *memory, uint32_t selector, int sibling);
void gameplay_state_apply_viewport_layout(GameMemory *memory,
    const GameplayViewportLayoutPlan *plan, int sibling);
GameplayViewportClampResult gameplay_state_clamp_viewport_point(
    GameMemory *memory, GameAddress point);
GameplayViewportClampResult gameplay_state_clamp_viewport_values(
    GameMemory *memory,uint32_t x,uint32_t y);
int gameplay_state_timed_update_due(uint32_t timer);
GameAddress gameplay_state_saved_exception_handler(GameMemory *memory,
    GameAddress frame);
GameplayModeTransitionPlan gameplay_state_apply_mode(GameMemory *memory,
    GameAddress object, uint32_t mode);
void gameplay_state_begin_interface_teardown(GameMemory *memory,
    GameAddress object);
GameplayInterfaceTeardownPlan gameplay_state_interface_teardown_plan(
    GameMemory *memory, GameAddress object);
void gameplay_state_clear_optional_interface(GameMemory *memory,
    GameAddress object);
int gameplay_state_object_message_allowed(uint32_t message);
GameplayObjectMessageSpec gameplay_state_object_message_spec(unsigned event);
uint32_t gameplay_state_object_message_guard(GameMemory *memory,
    GameAddress object, uint32_t guard_offset);
void gameplay_state_store_presentation_mode(GameMemory *memory,
    GameAddress object, uint32_t mode);
void gameplay_state_prepare_presentation_mode(GameMemory *memory,
    GameAddress object);
GameplayPresentationModePlan gameplay_state_presentation_mode_plan(
    GameMemory *memory, GameAddress object);
void gameplay_state_store_presentation_runtime(GameMemory *memory,
    GameAddress object, uint32_t mode, uint32_t shared_state);
void gameplay_state_finish_presentation_runtime(GameMemory *memory,
    GameAddress object, GameAddress handle);
GameAddress gameplay_state_typed_presentation_table(GameMemory *memory,
    uint32_t type);
GameAddress gameplay_state_owner_presentation_runtime(GameMemory *memory,
    GameAddress owner);
void gameplay_state_store_owner_presentation_runtime(GameMemory *memory,
    GameAddress owner, GameAddress runtime);
GameplayTypedPresentationPlan gameplay_state_attach_typed_presentation(
    GameMemory *memory, GameAddress owner, GameAddress record);
void gameplay_state_copy_typed_presentation_transform(GameMemory *memory,
    const GameplayTypedPresentationPlan *plan);
GameAddress gameplay_state_indexed_delivery_head(GameMemory *memory,
    GameAddress owner);
GameplayIndexedDeliveryNode gameplay_state_indexed_delivery_outer(
    GameMemory *memory, GameAddress node);
GameAddress gameplay_state_indexed_delivery_inner_list(GameMemory *memory,
    GameAddress payload);
GameplayIndexedDeliveryNode gameplay_state_indexed_delivery_inner(
    GameMemory *memory, GameAddress node);
GameAddress gameplay_state_owned_payload_list(GameMemory *memory,
    GameAddress owner);
GameplayIndexedDeliveryNode gameplay_state_owned_payload_node(
    GameMemory *memory, GameAddress node);
void gameplay_state_prepare_gameplay_record(GameMemory *memory,
    GameAddress object);
void gameplay_state_finish_gameplay_record(GameMemory *memory,
    GameAddress object, uint32_t owner);
GameplayPairReplacementPlan gameplay_state_pair_replacement_plan(
    GameMemory *memory, GameAddress owner, GameAddress argument);
typedef struct GameplayPeerServices {
    void *context;
    int (*call)(void *,GameAddress owner,GameAddress method,GameAddress argument);
} GameplayPeerServices;
/* Stable method IDs select ordinary peer behavior; unknown classes use the
 * explicit external callback, never an emulated calling convention. */
int gameplay_state_peer_method(GameMemory *,GameAddress owner,GameAddress argument,
    GameAddress method,const GameplayPeerServices *);
GameplayInterfaceSlotPlan gameplay_state_interface_slot_plan(
    GameMemory *memory, uint32_t index);
uint32_t gameplay_state_callback_registration_state(GameMemory *memory,
    GameAddress owner);
void gameplay_state_begin_callback_registration(GameMemory *memory,
    GameAddress owner);
GameAddress gameplay_state_rectangle_service(GameMemory *memory);
GameplayRecordDelegationPlan gameplay_state_record_delegation_plan(
    GameMemory *memory, GameAddress object);
GameAddress gameplay_state_presentation_sink(GameMemory *memory,
    GameAddress object, unsigned index);
GameplayPresentationBufferResult gameplay_state_publish_presentation_buffer(
    GameMemory *memory, GameAddress object, GameAddress buffer);
GameplayConfigurationSnapshot gameplay_state_publish_configuration_snapshot(
    GameMemory *memory, GameAddress object);
void gameplay_state_reset_request_words(GameMemory *memory,
    GameAddress object);
uint32_t gameplay_state_request_entry_count(GameMemory *memory,
    GameAddress object);
void gameplay_state_mark_request_entry(GameMemory *memory,
    GameAddress object, uint32_t index);
void gameplay_state_finish_request_reset(GameMemory *memory,
    GameAddress object);
GameplayCallbackReplacementPlan gameplay_state_begin_callback_replacement(
    GameMemory *memory, GameAddress object);
uint32_t gameplay_state_callback_status(GameMemory *memory,
    GameAddress object);
void gameplay_state_mark_callback_fallback(GameMemory *memory,
    GameAddress object);
void gameplay_state_finish_callback_replacement(GameMemory *memory,
    GameAddress object, GameAddress fallback, GameAddress context);
GameAddress gameplay_state_begin_presentation_refresh(GameMemory *memory,
    GameAddress object);
GameAddress gameplay_state_presentation_collection(GameMemory *memory,
    GameAddress object);
GameAddress gameplay_state_presentation_buffer(GameMemory *memory,
    GameAddress object);
GameAddress gameplay_state_subordinate(GameMemory *memory,
    GameAddress object);
uint32_t gameplay_state_subordinate_capacity(GameMemory *memory,
    GameAddress subordinate);
void gameplay_state_store_subordinate(GameMemory *memory,
    GameAddress object, GameAddress subordinate);
uint32_t gameplay_state_interpolation_count(GameMemory *memory,
    GameAddress object);
void gameplay_state_publish_launch_result(GameMemory *memory,
    GameAddress object, uint32_t result);
uint32_t gameplay_state_launch_mode(GameMemory *memory);
uint32_t gameplay_state_clock(GameMemory *memory);
void gameplay_state_store_launch_expiration(GameMemory *memory,
    GameAddress object, uint32_t expiration);
void gameplay_state_begin_source_scalar(GameMemory *memory,
    GameAddress object, uint8_t mode);
uint32_t gameplay_state_publish_source_scalar(GameMemory *memory,
    GameAddress object, uint32_t value);
void gameplay_state_prepare_optional_source(GameMemory *memory,
    GameAddress object, uint32_t value);
void gameplay_state_publish_object_payload(GameMemory *memory,
    GameAddress owner, GameAddress payload);
void gameplay_state_clear_object_payload(GameMemory *memory,
    GameAddress owner);
GameAddress gameplay_state_object_peer(GameMemory *memory,
    GameAddress owner, uint32_t offset);
uint32_t gameplay_state_object_pool_count(GameMemory *memory);
GameAddress gameplay_state_object_pool_slot(GameMemory *memory,
    uint32_t index);
void gameplay_state_clear_object_pool_slot(GameMemory *memory,
    uint32_t index);
GameAddress gameplay_state_global_object_head(GameMemory *memory);
GameAddress gameplay_state_pop_global_object(GameMemory *memory,
    GameAddress object);
void gameplay_state_store_collection_mode(GameMemory *memory,
    GameAddress object, uint32_t mode);
uint32_t gameplay_state_collection_count(GameMemory *memory,
    GameAddress object);
GameAddress gameplay_state_collection_handle(GameMemory *memory,
    GameAddress object, uint32_t index);
void gameplay_state_clear_collection_handle(GameMemory *memory,
    GameAddress object, uint32_t index);
GameAddress gameplay_state_begin_owned_field_destructor(GameMemory *memory,
    GameAddress object, GameAddress vtable, uint32_t field_offset);
void gameplay_state_clear_owned_field(GameMemory *memory,
    GameAddress object, uint32_t field_offset);
int16_t gameplay_state_owned_interface_count(GameMemory *memory,
    GameAddress object);
GameAddress gameplay_state_owned_interface(GameMemory *memory,
    GameAddress object, uint32_t index);
GameAddress gameplay_state_object_selection(GameMemory *memory,
    GameAddress object);
GameAddress gameplay_state_shared_presentation(GameMemory *memory);
void gameplay_state_publish_source_descriptor(GameMemory *memory,
    GameAddress output, uint32_t first, uint32_t second, uint32_t third,
    uint32_t aligned_span);
GameAddress gameplay_state_object_label(GameAddress object);
void gameplay_state_store_object_label_key(GameMemory *memory,
    GameAddress object, uint32_t key);
uint8_t gameplay_state_prepare_object_action(GameMemory *memory,
    GameAddress object);
GameAddress gameplay_state_object_action_target(GameMemory *memory,
    GameAddress object);
GameplayActiveObjectPlan gameplay_state_active_object_plan(GameMemory *memory);
void gameplay_state_archive_active_object(GameMemory *memory,
    GameAddress object);
void gameplay_state_publish_active_object(GameMemory *memory,
    GameAddress object);
uint16_t gameplay_state_active_object_mode(GameMemory *memory,
    GameAddress *object);
GameAddress gameplay_state_clear_active_object_request(GameMemory *memory,
    GameAddress object, uint16_t mode);
GameplayLinkedStatePlan gameplay_state_linked_state_plan(GameMemory *memory,
    GameAddress object);
void gameplay_state_activate_linked_state(GameMemory *memory,
    GameAddress object, GameAddress next);
void gameplay_state_mark_linked_state_refresh(GameMemory *memory,
    GameAddress object);
void gameplay_state_prepare_linked_state_clear(GameMemory *memory,
    GameAddress object, uint32_t previous);
void gameplay_state_finish_linked_state_clear(GameMemory *memory,
    GameAddress object);
GameplayWorldQuerySnapshot gameplay_state_prepare_world_query(
    GameMemory *memory, GameAddress object);
void gameplay_state_world_query_object(GameMemory *memory,
    GameAddress object, uint32_t *first, uint32_t *second);
GameplayObjectStatusSnapshot gameplay_state_object_status_snapshot(
    GameMemory *memory, GameAddress object);
uint32_t gameplay_state_status_global_active(GameMemory *memory);
uint32_t gameplay_state_status_handle(GameMemory *memory,
    GameAddress object);
uint32_t gameplay_state_status_global_owner(GameMemory *memory);
uint32_t gameplay_state_status_handle_owner(GameMemory *memory,
    uint32_t handle);
uint8_t gameplay_state_status_inhibited(GameMemory *memory,
    GameAddress object);
uint32_t gameplay_state_status_threshold(GameMemory *memory,
    GameAddress object);
GameAddress gameplay_state_status_virtual_target(GameMemory *memory,
    GameAddress object);
GameplayVirtualCallPlan gameplay_state_status_virtual_call(
    GameMemory *memory, GameAddress object);
uint32_t gameplay_state_increment_optional_counter(GameMemory *memory,
    GameAddress counter);
uint8_t gameplay_state_text_byte(GameMemory *memory, GameAddress text,
    uint32_t index);
GameplayStateRangeSnapshot gameplay_state_range_snapshot(GameMemory *memory);
GameAddress gameplay_state_range_object(GameMemory *memory,
    int alternate, uint32_t index);
uint32_t gameplay_state_runtime_views_initialized(GameMemory *memory);
void gameplay_state_copy_runtime_view(GameMemory *memory, uint32_t index);
GameplayRuntimeViewCopyResult gameplay_state_copy_runtime_views(
    GameMemory *memory);
void gameplay_state_copy_runtime_view_seed(GameMemory *memory);
GameplayRuntimeViewSourceResult gameplay_state_copy_runtime_view_sources(
    GameMemory *memory);
GameplayScalarTableSpec gameplay_state_scalar_table_spec(unsigned index);
GameAddress gameplay_state_value_submit_target(GameMemory *memory,
    GameAddress object);
GameplayVirtualCallPlan gameplay_state_value_submit_call(GameMemory *memory,
    GameAddress object);
void gameplay_state_publish_value_pair(GameMemory *memory,
    GameAddress output, uint32_t value);
GameplayStatusDispatchPlan gameplay_state_status_dispatch_plan(
    GameMemory *memory);
GameplayObjectCallPlan gameplay_state_object_tail_call(GameMemory *memory,
    GameAddress owner);
void gameplay_state_initialize_descriptor_binding(GameMemory *memory,
    GameAddress object, uint32_t state, int extended);
void gameplay_state_attach_descriptor_owner(GameMemory *memory,
    GameAddress attached, GameAddress object);
uint32_t gameplay_state_finish_descriptor_binding(GameMemory *memory,
    GameAddress object, GameAddress attached, uint32_t *binding);
void gameplay_state_initialize_presentation_packet(GameMemory *memory,
    GameAddress object);
void gameplay_state_initialize_container_header(GameMemory *memory,
    GameAddress container, uint8_t kind, uint8_t subtype, uint32_t value,
    int alternate);
void gameplay_state_initialize_surface_container(GameMemory *memory,
    GameAddress container);
void gameplay_state_initialize_surface_rows(GameMemory *memory,
    GameAddress container, int32_t height);
GameAddress gameplay_state_seed_primary_interface(GameMemory *memory);
GameAddress gameplay_state_finish_primary_interface(GameMemory *memory);
GameAddress gameplay_state_resource_slot(GameMemory *memory,
    uint32_t index);
void gameplay_state_clear_resource_slot(GameMemory *memory,
    uint32_t index);
void gameplay_state_initialize_entity_runtime(GameMemory *memory,
    GameAddress object);
void gameplay_state_store_entity_integer_property(GameMemory *memory,
    GameAddress object, uint32_t index, uint32_t value);
GameplayNumericTableSpec gameplay_state_numeric_table_spec(unsigned index);
void gameplay_state_fill_u32(GameMemory *memory, GameAddress destination,
    uint32_t count, uint32_t value, int reverse);
void gameplay_state_initialize_scaled_runtime_header(GameMemory *memory,
    GameAddress object);
void gameplay_state_clear_scaled_runtime_fields(GameMemory *memory,
    GameAddress object, GameAddress embedded);
GameplayRecordGroupSpec gameplay_state_record_group_spec(unsigned index,
    int alternate_first_group);

typedef struct GameplayBufferedOwnerPlan {
    GameAddress base_target;
    GameAddress exception_handler;
    GameAddress base_return;
} GameplayBufferedOwnerPlan;

typedef struct GameplayTimedEffectAttachPlan {
    uint32_t attach_slot;
    GameAddress helper_return;
    GameAddress attach_return;
    GameAddress first_tick_return;
    GameAddress second_tick_return;
    int set_default_height;
} GameplayTimedEffectAttachPlan;

typedef struct GameplaySelectionTablePlan {
    GameAddress selected;
    GameAddress count;
    GameAddress objects;
    GameAddress left;
    GameAddress top;
    GameAddress right;
    GameAddress short_return;
    GameAddress tall_return;
} GameplaySelectionTablePlan;

typedef struct GameplayTailCallPlan {
    GameAddress call_target;
    GameAddress call_return;
    GameAddress tail_target;
} GameplayTailCallPlan;

GameplayBufferedOwnerPlan gameplay_state_buffered_owner_plan(unsigned variant);
GameplayTimedEffectAttachPlan gameplay_state_timed_effect_attach_plan(
    unsigned variant);
GameplaySelectionTablePlan gameplay_state_selection_table_plan(
    unsigned variant);
GameplayTailCallPlan gameplay_state_tail_call_plan(unsigned variant);
uint32_t gameplay_state_smacker_channel_mask(unsigned index);

typedef struct GameplaySmackerChannelOps {
    void *context;
    int (*apply)(void *context, unsigned channel, uint32_t handle,
        uint32_t mask, uint32_t volume, uint32_t pan);
} GameplaySmackerChannelOps;

int gameplay_state_apply_smacker_channels(GameMemory *memory,
    GameAddress owner, uint32_t volume, const GameplaySmackerChannelOps *ops);

typedef struct GameplayAllocationSpec {
    uint32_t size;
    GameAddress destination;
    GameAddress return_address;
} GameplayAllocationSpec;

typedef struct GameplayDisplayTransitionPlan {
    uint32_t scalar_bits;
    GameAddress call_return;
    int apply;
} GameplayDisplayTransitionPlan;

typedef struct GameplayIndexedSlotMethod {
    uint32_t vtable_offset;
    GameAddress return_address;
    int invoke;
} GameplayIndexedSlotMethod;

typedef struct GameplayOptionalLookupSpec {
    GameAddress name;
    GameAddress import_slot;
    GameAddress return_address;
} GameplayOptionalLookupSpec;

GameplayAllocationSpec gameplay_state_world_buffer_spec(unsigned index);
GameplayAllocationSpec gameplay_state_runtime_pool_spec(unsigned index);
GameplayAllocationSpec gameplay_state_object_array_spec(unsigned index);
void gameplay_state_initialize_world_down_vector(GameMemory *memory);
void gameplay_state_reset_runtime_pool_tables(GameMemory *memory);
uint32_t gameplay_state_prepare_runtime_mode(GameMemory *memory);
GameAddress gameplay_state_indexed_slot_target(GameMemory *memory,
    GameAddress object, int *dispatch);
GameplayDisplayTransitionPlan gameplay_state_display_transition_plan(
    GameMemory *memory);
void gameplay_state_begin_presentation_bootstrap(GameMemory *memory);
void gameplay_state_publish_presentation_dimensions(GameMemory *memory);
void gameplay_state_finish_presentation_bootstrap(GameMemory *memory);
GameplayIndexedSlotMethod gameplay_state_indexed_slot_method(
    unsigned variant, uint8_t state);
GameplayOptionalLookupSpec gameplay_state_optional_lookup_spec(unsigned index);
void gameplay_state_initialize_controller_fields(GameMemory *memory,
    GameAddress object);
void gameplay_state_initialize_descriptor_defaults(GameMemory *memory);

typedef struct GameplayControllerServices {
    void *context;
    int (*detach_external)(void *,GameAddress child,GameAddress method);
    int (*stop_sound)(void *,int16_t sound);
    int (*play_sound)(void *,int16_t sound,float volume,uint32_t flags);
    int (*reset_application)(void *);
    int (*change_mode)(void *,uint32_t mode);
    int (*set_control_mode)(void *,uint32_t mode);
    int (*show_selection_message)(void *);
} GameplayControllerServices;
/* Embedded attachment and controller retain their existing owned records. */
int gameplay_controller_reset_attachment(GameMemory *,GameAddress child,
    const GameplayControllerServices *);
int gameplay_controller_activate_attachment(GameMemory *,GameAddress child,
    const GamePresentationFeedbackServices *);
int gameplay_controller_refresh_selection(GameMemory *,GameAddress controller,
    const GameplayControllerServices *);

typedef struct GameplayEffectRingServices {
    void *context;
    int (*visible)(void *,GameAddress,int *);
    int (*duration)(void *,GameAddress,int32_t *);
    int (*retire_effect)(void *,GameAddress,int immediate);
    int (*create_effect)(void *,GameAddress,const float position[3],GameAddress *);
    int (*emit)(void *,GameAddress,const float position[3]);
    int (*retire_catalog)(void *,GameAddress,uint8_t);
    int (*phase)(void *,GameAddress,uint32_t);
    int (*detach_controller)(void *,GameAddress,GameAddress);
    int (*token)(void *,GameAddress,uint32_t *);
    int (*notify)(void *,GameAddress,uint32_t,int remove);
} GameplayEffectRingServices;
int gameplay_effect_ring_update(GameMemory *,const GameplayEffectRingServices *,GameAddress);
int gameplay_effect_ring_reset(GameMemory *,const GameplayEffectRingServices *,GameAddress,int detach_controller);
int gameplay_descriptor_payload_update(GameMemory *,const GameplayEffectRingServices *,GameAddress);
int gameplay_descriptor_payload_retire(GameMemory *,const GameplayEffectRingServices *,GameAddress,int detach_controller);

int gameplay_feedback_request(GameMemory *,const uint32_t values[3]);
/* Selector controls keep their existing panel/widget records and owners.
 * Unknown widget kinds use a typed query port; known derived classes are native. */
typedef int (*GameplayWidgetKindQuery)(void *,GameAddress,uint32_t *);
int gameplay_widget_kind(GameMemory *,GameAddress,uint32_t *);
int gameplay_panel_settle(GameMemory *,GameAddress,GameplayWidgetKindQuery,void *);
int gameplay_selector_ranges(GameMemory *,int alternate);
#endif
