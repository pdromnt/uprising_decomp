#include "state_helper_plans.h"

uint32_t state_helper_flagged_member_offset(uint32_t flags) {
    if ((flags & 1u) == 0u) return 0xe4u;
    return (flags & 0x40u) != 0u ? 0x80u : 0x1cu;
}

uint32_t state_helper_aligned_descriptor_size(uint32_t size) {
    return (size + 3u) & ~3u;
}

uint32_t state_helper_presentation_buffer_bytes(int32_t count) {
    return count > 0 ? (uint32_t)count * 12u : 0u;
}

StateHelperResourcePlan state_helper_resource_plan(GameMemory *memory,
        GameAddress owner, int32_t group_index, uint32_t slot_index) {
    StateHelperResourcePlan plan = {0};
    plan.group = game_memory_read_u32(memory,
        owner + (uint32_t)(group_index * 4) + 0x0cu);
    if (!memory->fault) plan.table = game_memory_read_u32(memory,
        plan.group + 0x0cu);
    if (!memory->fault) plan.weight = game_memory_read_u32(memory,
        plan.group + 0x10u);
    if (!memory->fault) plan.resource = game_memory_read_u32(memory,
        plan.table + slot_index * 4u);
    if (!memory->fault) {
        plan.vtable = game_memory_read_u32(memory, plan.resource);
        if (!memory->fault) plan.prepare_target = game_memory_read_u32(memory,
            plan.vtable + 0x48u);
        if (!memory->fault) plan.activate_target = game_memory_read_u32(memory,
            plan.vtable + 0x34u);
    }
    return plan;
}

StateHelperPresentationAdvance state_helper_advance_presentation(
        GameMemory *memory, GameAddress object) {
    StateHelperPresentationAdvance result = {0};
    uint32_t state = game_memory_read_u32(memory, object + 0x44u);
    result.gated = game_memory_read_u32(memory, 0x0059d4ccu);
    result.active = game_memory_read_u32(memory, 0x00627ab8u);
    if (memory->fault || state == 3u ||
            (result.gated != 0u && result.active != object))
        return result;
    result.state = result.gated != 0u && result.active == object ? 2u : 1u;
    result.call_eax = result.gated != 0u ? result.active : result.gated;
    game_memory_write_u32(memory, object + 0x44u, result.state);
    if (!memory->fault) game_memory_write_u32(memory, 0x00627ab8u, 0u);
    result.publish = !memory->fault;
    return result;
}

void state_helper_controller_store_argument(GameMemory *memory,
        GameAddress self, uint32_t argument) {
    game_memory_write_u32(memory, self + 0x14u, argument);
}

void state_helper_initialize_presentation_buffer(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object, 0x005764f8u);
}

void state_helper_begin_attachment(GameMemory *memory, GameAddress object,
        GameAddress source, uint16_t channel) {
    game_memory_write_u32(memory, object, 0x00576bc8u);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, source);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x14u, channel);
}

void state_helper_finish_attachment(GameMemory *memory, GameAddress object,
        uint16_t type, GameAddress binding) {
    game_memory_write_u16(memory, object + 8u, type);
    if (!memory->fault) game_memory_write_u32(memory, object, 0x00576e08u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x48u, binding);
}

void state_helper_finish_type23_owner(GameMemory *memory,
        GameAddress object) {
    uint32_t flags = game_memory_read_u32(memory, object + 0x10u);
    if (memory->fault) return;
    game_memory_write_u32(memory, object, 0x005776d8u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x264u,
        0x005776b8u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x294u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x10u,
        flags | 0x8100u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x298u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x33cu, 1u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x2c8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x2b8u, 0u);
    if (!memory->fault) game_memory_write_u8(memory, object + 0x0cu, 0x23u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x258u,
        0x00639af0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x25cu,
        0x00639ae8u);
}

void state_helper_finish_type26_runtime(GameMemory *memory,
        GameAddress object) {
    uint32_t flags = game_memory_read_u32(memory, object + 0x10u);
    if (memory->fault) return;
    game_memory_write_u32(memory, object, 0x00577a90u);
    if (!memory->fault) game_memory_write_u8(memory, object + 0x0cu, 0x26u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x10u,
        flags | 0x7008u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x194u, 0u);
}

void state_helper_reset_presentation_children(GameMemory *memory,
        GameAddress object) {
    static const uint32_t offsets[5] = {0x5cu, 0x98u, 0x9cu, 0x58u, 0x60u};
    unsigned index;
    for (index = 0u; index < 5u && !memory->fault; ++index)
        game_memory_write_u32(memory, object + offsets[index], 0u);
}

void state_helper_finish_presentation_reset(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object + 0x18u, 1u);
}

void state_helper_initialize_keyed_record(GameMemory *memory,
        GameAddress record, uint32_t key, GameAddress next,
        GameAddress previous) {
    game_memory_write_u32(memory, record, key);
    if (!memory->fault) game_memory_write_u32(memory, record + 4u, next);
    if (!memory->fault) game_memory_write_u32(memory, record + 8u, previous);
}

void state_helper_begin_small_list_destructor(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object, 0x00575b08u);
}

StateHelperKeyedNode state_helper_find_keyed_node(GameMemory *memory,
        GameAddress owner, uint32_t key) {
    StateHelperKeyedNode result = {0};
    GameAddress node = game_memory_read_u32(memory, owner);
    while (!memory->fault && node != 0u) {
        result.last_value = game_memory_read_u32(memory, node);
        if (result.last_value == key) {
            result.node = node;
            return result;
        }
        if (!memory->fault) node = game_memory_read_u32(memory, node + 4u);
    }
    return result;
}

void state_helper_initialize_list_root(GameMemory *memory,
        GameAddress root) {
    game_memory_write_u32(memory, root, 0u);
    if (!memory->fault) game_memory_write_u32(memory, root + 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, root + 8u, 0u);
}

void state_helper_publish_list_root(GameMemory *memory, GameAddress root) {
    game_memory_write_u32(memory, 0x005980c4u, root);
}

void state_helper_initialize_compact_payload(GameMemory *memory,
        GameAddress payload, uint32_t first, uint32_t second) {
    game_memory_write_u32(memory, payload, 0u);
    if (!memory->fault) game_memory_write_u32(memory, payload + 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, payload + 8u, first);
    if (!memory->fault) game_memory_write_u32(memory, payload + 0x0cu, second);
}

void state_helper_initialize_full_payload(GameMemory *memory,
        GameAddress payload, const uint32_t values[5]) {
    unsigned index;
    for (index = 0u; index < 5u && !memory->fault; ++index)
        game_memory_write_u32(memory, payload + index * 4u, values[index]);
    if (!memory->fault) game_memory_write_u32(memory, payload + 0x14u, 0u);
}

GameAddress state_helper_active_view(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00604f08u);
}

GameAddress state_helper_presentation_handle(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object + 0x50u);
}

GameAddress state_helper_presentation_manager(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00628a90u);
}

uint32_t state_helper_begin_scalar_sample(GameMemory *memory,
        uint32_t value_bits) {
    uint32_t reference = game_memory_read_u32(memory, 0x0062792cu);
    if (!memory->fault) game_memory_write_u32(memory, 0x00626e4cu,
        value_bits);
    return reference;
}

void state_helper_finish_scalar_sample(GameMemory *memory,
        uint32_t reference, uint32_t converted) {
    game_memory_write_u32(memory, 0x00597f5cu,
        reference - converted);
}

uint32_t state_helper_guard_head(GameMemory *memory, GameAddress fs_base) {
    return game_memory_read_u32(memory, fs_base);
}

void state_helper_initialize_guarded_traversal(GameMemory *memory,
        GameAddress fs_base, GameAddress frame, GameAddress guard_head) {
    game_memory_write_u32(memory, fs_base, guard_head);
    if (!memory->fault) game_memory_write_u32(memory, frame - 0x20u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, frame - 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, frame - 0x1cu, 0u);
}

uint32_t state_helper_advance_guarded_record(GameMemory *memory,
        GameAddress frame, uint32_t stride) {
    uint32_t record = game_memory_read_u32(memory, frame + 8u);
    uint32_t completed = game_memory_read_u32(memory, frame - 0x1cu);
    if (!memory->fault)
        game_memory_write_u32(memory, frame + 8u, record + stride);
    if (!memory->fault)
        game_memory_write_u32(memory, frame - 0x1cu, ++completed);
    return completed;
}

void state_helper_finish_guarded_traversal(GameMemory *memory,
        GameAddress frame) {
    game_memory_write_u32(memory, frame - 0x20u, 1u);
    if (!memory->fault)
        game_memory_write_u32(memory, frame - 4u, 0xffffffffu);
}

uint32_t state_helper_restore_guarded_traversal(GameMemory *memory,
        GameAddress fs_base, GameAddress frame) {
    uint32_t previous = game_memory_read_u32(memory, frame - 0x10u);
    if (!memory->fault) game_memory_write_u32(memory, fs_base, previous);
    return previous;
}

void state_helper_construct_packet(GameMemory *memory, GameAddress packet,
        uint32_t argument) {
    game_memory_write_u32(memory, packet, 0x00573180u);
    if (!memory->fault) game_memory_write_u32(memory, packet + 4u, argument);
    if (!memory->fault) game_memory_write_u32(memory, packet + 0x10u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, packet, 0x00577a58u);
    if (!memory->fault) game_memory_write_u32(memory, packet + 8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, packet + 0x0cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, packet + 0x14u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, packet + 0x18u, 0u);
}

GameAddress state_helper_packet_payload(GameMemory *memory,
        GameAddress packet) {
    return game_memory_read_u32(memory, packet + 0x14u);
}

void state_helper_clear_packet_payload(GameMemory *memory,
        GameAddress packet) {
    game_memory_write_u32(memory, packet + 0x14u, 0u);
}

GameAddress state_helper_packet_owner_service(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0059d4bcu);
}

StateHelperPacketQueueEndpoints state_helper_packet_queue_endpoints(
        GameMemory *memory, GameAddress endpoint) {
    StateHelperPacketQueueEndpoints result;
    result.head = game_memory_read_u32(memory, endpoint + 0x28u);
    result.tail = game_memory_read_u32(memory, endpoint + 0x2cu);
    return result;
}

void state_helper_packet_queue_initialize(GameMemory *memory,
        GameAddress endpoint, GameAddress packet) {
    game_memory_write_u32(memory, endpoint + 0x2cu, packet);
    if (!memory->fault) game_memory_write_u32(memory, endpoint + 0x28u,
        packet);
}

void state_helper_packet_queue_append(GameMemory *memory,
        GameAddress endpoint, GameAddress tail, GameAddress packet) {
    game_memory_write_u32(memory, tail + 4u, packet);
    if (!memory->fault) game_memory_write_u32(memory, endpoint + 0x2cu,
        packet);
}

void state_helper_packet_queue_prepend(GameMemory *memory,
        GameAddress endpoint, GameAddress head, GameAddress packet) {
    game_memory_write_u32(memory, packet + 4u, head);
    if (!memory->fault) game_memory_write_u32(memory, endpoint + 0x28u,
        packet);
}

GameAddress state_helper_packet_next(GameMemory *memory,
        GameAddress packet) {
    return game_memory_read_u32(memory, packet + 4u);
}

void state_helper_packet_queue_insert_after(GameMemory *memory,
        GameAddress previous, GameAddress next, GameAddress packet) {
    game_memory_write_u32(memory, packet + 4u, next);
    if (!memory->fault) game_memory_write_u32(memory, previous + 4u, packet);
}
