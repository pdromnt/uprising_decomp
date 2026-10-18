#ifndef UPRISING_NATIVE_STATE_HELPER_PLANS_H
#define UPRISING_NATIVE_STATE_HELPER_PLANS_H

#include "game_memory.h"

#include <stdint.h>

typedef struct StateHelperResourcePlan {
    GameAddress group;
    GameAddress table;
    GameAddress resource;
    uint32_t weight;
    GameAddress vtable;
    GameAddress prepare_target;
    GameAddress activate_target;
} StateHelperResourcePlan;

typedef struct StateHelperPresentationAdvance {
    uint32_t state;
    uint32_t gated;
    GameAddress active;
    uint32_t call_eax;
    int publish;
} StateHelperPresentationAdvance;

typedef struct StateHelperKeyedNode {
    GameAddress node;
    uint32_t last_value;
} StateHelperKeyedNode;

typedef struct StateHelperPacketQueueEndpoints {
    GameAddress head;
    GameAddress tail;
} StateHelperPacketQueueEndpoints;

uint32_t state_helper_flagged_member_offset(uint32_t flags);
uint32_t state_helper_aligned_descriptor_size(uint32_t size);
uint32_t state_helper_presentation_buffer_bytes(int32_t count);
StateHelperResourcePlan state_helper_resource_plan(GameMemory *memory,
    GameAddress owner, int32_t group_index, uint32_t slot_index);
StateHelperPresentationAdvance state_helper_advance_presentation(
    GameMemory *memory, GameAddress object);
void state_helper_controller_store_argument(GameMemory *memory,
    GameAddress self, uint32_t argument);
void state_helper_initialize_presentation_buffer(GameMemory *memory,
    GameAddress object);
void state_helper_begin_attachment(GameMemory *memory, GameAddress object,
    GameAddress source, uint16_t channel);
void state_helper_finish_attachment(GameMemory *memory, GameAddress object,
    uint16_t type, GameAddress binding);
void state_helper_finish_type23_owner(GameMemory *memory,
    GameAddress object);
void state_helper_finish_type26_runtime(GameMemory *memory,
    GameAddress object);
void state_helper_reset_presentation_children(GameMemory *memory,
    GameAddress object);
void state_helper_finish_presentation_reset(GameMemory *memory,
    GameAddress object);
void state_helper_initialize_keyed_record(GameMemory *memory,
    GameAddress record, uint32_t key, GameAddress next,
    GameAddress previous);
void state_helper_begin_small_list_destructor(GameMemory *memory,
    GameAddress object);
StateHelperKeyedNode state_helper_find_keyed_node(GameMemory *memory,
    GameAddress owner, uint32_t key);
void state_helper_initialize_list_root(GameMemory *memory,
    GameAddress root);
void state_helper_publish_list_root(GameMemory *memory, GameAddress root);
void state_helper_initialize_compact_payload(GameMemory *memory,
    GameAddress payload, uint32_t first, uint32_t second);
void state_helper_initialize_full_payload(GameMemory *memory,
    GameAddress payload, const uint32_t values[5]);
GameAddress state_helper_active_view(GameMemory *memory);
GameAddress state_helper_presentation_handle(GameMemory *memory,
    GameAddress object);
GameAddress state_helper_presentation_manager(GameMemory *memory);
uint32_t state_helper_begin_scalar_sample(GameMemory *memory,
    uint32_t value_bits);
void state_helper_finish_scalar_sample(GameMemory *memory,
    uint32_t reference, uint32_t converted);
uint32_t state_helper_guard_head(GameMemory *memory, GameAddress fs_base);
void state_helper_initialize_guarded_traversal(GameMemory *memory,
    GameAddress fs_base, GameAddress frame, GameAddress guard_head);
uint32_t state_helper_advance_guarded_record(GameMemory *memory,
    GameAddress frame, uint32_t stride);
void state_helper_finish_guarded_traversal(GameMemory *memory,
    GameAddress frame);
uint32_t state_helper_restore_guarded_traversal(GameMemory *memory,
    GameAddress fs_base, GameAddress frame);
void state_helper_construct_packet(GameMemory *memory, GameAddress packet,
    uint32_t argument);
GameAddress state_helper_packet_payload(GameMemory *memory,
    GameAddress packet);
void state_helper_clear_packet_payload(GameMemory *memory,
    GameAddress packet);
GameAddress state_helper_packet_owner_service(GameMemory *memory);
StateHelperPacketQueueEndpoints state_helper_packet_queue_endpoints(
    GameMemory *memory, GameAddress endpoint);
void state_helper_packet_queue_initialize(GameMemory *memory,
    GameAddress endpoint, GameAddress packet);
void state_helper_packet_queue_append(GameMemory *memory,
    GameAddress endpoint, GameAddress tail, GameAddress packet);
void state_helper_packet_queue_prepend(GameMemory *memory,
    GameAddress endpoint, GameAddress head, GameAddress packet);
GameAddress state_helper_packet_next(GameMemory *memory,
    GameAddress packet);
void state_helper_packet_queue_insert_after(GameMemory *memory,
    GameAddress previous, GameAddress next, GameAddress packet);

#endif
