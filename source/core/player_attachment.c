#include "player_attachment.h"
#include "game_matrix.h"

#include <string.h>

static PlayerAttachmentStats stats;
void player_attachment_stats(PlayerAttachmentStats *output) {
    if (output) *output = stats;
}

int player_attachment_save(const PlayerAttachmentState *state,
        const PlayerAttachmentArchiveServices *services) {
    uint32_t quantity, tick;
    if (!state || !services || !services->write_word) return 0;
    quantity = state->quantity; tick = state->last_tick;
    return services->write_word(services->context, quantity) &&
        services->write_word(services->context, tick);
}
int player_attachment_load(PlayerAttachmentState *state,
        const PlayerAttachmentArchiveServices *services) {
    return state && services && services->read_word &&
        services->read_word(services->context, &state->quantity) &&
        services->read_word(services->context, &state->last_tick);
}

/* Named fields are not packed to the legacy layout. Each exposed segment
 * explicitly maps a compatibility offset to its native owner. */
static void *attachment_segment(PlayerAttachmentState *s, uint32_t offset,
        uint32_t *begin, size_t *length) {
#define FIELD(at, member) do { if (offset - (at) < sizeof(s->member)) { *begin = (at); \
    *length = sizeof(s->member); return &s->member; } } while (0)
    FIELD(0u, projection_method_table);
    FIELD(4u, parent); FIELD(8u, last_tick); FIELD(12u, quantity);
    FIELD(16u, heading_word); FIELD(20u, motion);
    FIELD(24u, stream_cookie); FIELD(28u, stream_options);
    if (s->kind >= PLAYER_SCENE_POSITION) {
        FIELD(32u, position);
    } else if (s->kind != PLAYER_SCENE_COMMAND) {
        FIELD(32u, kind_flags); FIELD(36u, position);
        FIELD(48u, local_orientation); FIELD(96u, secondary_position);
        FIELD(108u, reverse_orientation);
        FIELD(156u, oriented); FIELD(160u, mode); FIELD(164u, alternate);
        FIELD(168u, repeating); FIELD(172u, repeat_total);
        FIELD(176u, repeat_remaining); FIELD(180u, repeat_delay);
        FIELD(184u, last_repeat);
    }
#undef FIELD
    return NULL;
}

static int attachment_transfer(PlayerAttachmentState *s, uint32_t offset,
        void *bytes, size_t size, int writing) {
    uint8_t *cursor = bytes;
    size_t limit = s->kind == PLAYER_SCENE_COMMAND ? 32u :
        s->kind >= PLAYER_SCENE_POSITION ? 44u : 188u;
    if (!bytes || offset > limit || size > limit - offset) return 0;
    while (size) {
        uint32_t begin;
        size_t length, step;
        uint8_t *field = attachment_segment(s, offset, &begin, &length);
        if (!field) return 0;
        step = length - (offset - begin);
        if (step > size) step = size;
        if (writing) memcpy(field + offset - begin, cursor, step);
        else memcpy(cursor, field + offset - begin, step);
        cursor += step; offset += (uint32_t)step; size -= step;
    }
    return 1;
}

int player_attachment_projection_read(void *context, uint32_t offset,
        void *output, size_t size) {
    return attachment_transfer(context, offset, output, size, 0);
}

int player_attachment_projection_write(void *context, uint32_t offset,
        const void *input, size_t size) {
    return attachment_transfer(context, offset, (void *)input, size, 1);
}

int player_attachment_import(GameMemory *memory, GameAddress object,
        PlayerSceneChildKind kind, PlayerAttachmentState *state) {
    uint8_t projection[188];
    size_t size = kind == PLAYER_SCENE_COMMAND ? 32u :
        kind >= PLAYER_SCENE_POSITION ? 44u : 188u;
    size_t offset;
    if (!memory || !state) return 0;
    /* The CPU compatibility bridge supports scalar accesses, not bulk spans. */
    for (offset = 0u; offset < size; offset += 4u)
        if (!game_memory_read(memory, object + (uint32_t)offset,
                projection + offset, 4u)) return 0;
    memset(state, 0, sizeof(*state)); state->kind = kind;
    return player_attachment_projection_write(state, 0u, projection, size);
}

int player_attachment_submit(PlayerAttachmentState *state,
        const float position[3], const PlayerAttachmentFrame *frame,
        const PlayerAttachmentServices *services) {
    PlayerAttachmentEmission emission;
    PlayerAttachmentEnvironment environment;
    int32_t quantity;
    if (!state || !frame || !services || !services->emit) return 0;
    if (state->kind == PLAYER_SCENE_COMMAND) return 1;
    ++stats.submissions;
    emission.kind = state->kind;
    emission.parent = state->parent; emission.motion = state->motion;
    memcpy(emission.angles, frame->angles, sizeof(emission.angles));
    game_matrix_transform_point_values(emission.position,
        frame->world_matrix, position);
    emission.strength = 1.0f;
    if (state->kind == PLAYER_SCENE_DERIVED_SPATIAL) {
        int32_t total = (int32_t)state->repeat_total;
        int32_t half = total >= 0 ? total / 2 :
            (int32_t)((state->repeat_total >> 1u) | 0x80000000u);
        int32_t remaining = (int32_t)(state->repeat_remaining - 1u);
        if (remaining <= half)
            emission.strength = (float)((long double)remaining / half);
        else
            emission.strength = (float)((long double)(int32_t)(
                (uint32_t)half - state->repeat_remaining + 1u) / half + 1.0L);
    }
    if (!services->emit(services->context, &emission)) return 0;
    ++stats.emitted;
    if (!services->environment || !services->environment(services->context,
            state->parent, state->kind, &environment)) return 0;
    if (environment.feedback_enabled && (environment.parent_key == 0x20u ||
            (environment.parent_key == 0x23u && environment.linked_world_matches)))
        if (!services->feedback || !services->feedback(services->context)) return 0;
    quantity = (int32_t)state->quantity;
    if (quantity > 0) {
        if (quantity == 1 && environment.current_world_is_local &&
                environment.tick - environment.previous_notice > 125u)
            if (!services->last_round || !services->last_round(services->context,
                    state->kind, environment.tick)) return 0;
        --state->quantity;
        ++stats.consumed;
    }
    return 1;
}

static uint32_t record_word(GameMemory *m, GameAddress p) {
    return game_memory_read_u32(m, p);
}
static int32_t record_integer(uint32_t value) {
    int32_t result; memcpy(&result, &value, 4); return result;
}
static void record_floats(GameMemory *m, GameAddress p, float *out, unsigned count) {
    unsigned i;
    for (i = 0; i < count; ++i) {
        uint32_t bits = record_word(m, p + i * 4u);
        memcpy(out + i, &bits, 4);
    }
}
int player_attachment_record_submit(GameMemory *m, GameAddress child,
        const float position[3], const PlayerAttachmentFrame *frame,
        const PlayerAttachmentServices *s) {
    PlayerAttachmentEmission emission;
    PlayerAttachmentEnvironment environment;
    uint32_t table, quantity;
    if (!m || !frame || !s || !s->emit) return 0;
    table = record_word(m, child);
    if (table != 0x572e08u && table != 0x572e20u) return 0;
    emission.kind = table == 0x572e20u ? PLAYER_SCENE_DERIVED_SPATIAL : PLAYER_SCENE_SPATIAL;
    emission.parent = record_word(m, child + 4u);
    emission.motion = record_word(m, child + 20u);
    memcpy(emission.angles, frame->angles, sizeof(emission.angles));
    game_matrix_transform_point_values(emission.position, frame->world_matrix, position);
    emission.strength = 1.0f;
    if (emission.kind == PLAYER_SCENE_DERIVED_SPATIAL) {
        uint32_t total = record_word(m, child + 0xacu);
        int32_t half = record_integer((total >> 1u) | (total & 0x80000000u));
        uint32_t remaining = record_word(m, child + 0xb0u);
        int32_t next = record_integer(remaining - 1u);
        emission.strength = next <= half ? (float)((long double)next / half) :
            (float)((long double)record_integer((uint32_t)half - remaining + 1u) / half + 1.0L);
    }
    if (m->fault || !s->emit(s->context, &emission) || !s->environment ||
            !s->environment(s->context, emission.parent, emission.kind, &environment)) return 0;
    if (environment.feedback_enabled && (environment.parent_key == 0x20u ||
            (environment.parent_key == 0x23u && environment.linked_world_matches)))
        if (!s->feedback || !s->feedback(s->context)) return 0;
    quantity = record_word(m, child + 12u);
    if (record_integer(quantity) > 0) {
        if (quantity == 1u && environment.current_world_is_local &&
                environment.tick - environment.previous_notice > 125u)
            if (!s->last_round || !s->last_round(s->context, emission.kind, environment.tick)) return 0;
        game_memory_write_u32(m, child + 12u, quantity - 1u);
    }
    return !m->fault;
}
static int record_emit_pose(GameMemory *m, GameAddress child, GameAddress position,
        const PlayerAttachmentFrame *frame, const uint16_t angles[3],
        const PlayerAttachmentServices *s) {
    PlayerAttachmentFrame oriented = *frame;
    float point[3];
    record_floats(m, position, point, 3);
    memcpy(oriented.angles, angles, sizeof(oriented.angles));
    return !m->fault && player_attachment_record_submit(m, child, point, &oriented, s);
}
int player_attachment_record_update(GameMemory *m, GameAddress child,
        const PlayerAttachmentFrame *frame, const PlayerAttachmentServices *s) {
    uint32_t table, tick, remaining, alternate;
    float orientation[12], local[12], composed[12];
    uint16_t angles[3];
    if (!m || !frame || !s) return -1;
    table = record_word(m, child);
    if (table != 0x572e08u && table != 0x572e20u) return -1;
    if (!record_word(m, child + 12u)) return m->fault ? -1 : 0;
    tick = frame->tick;
    if (record_integer(record_word(m, child + 8u) + frame->motion_delay) > record_integer(tick) ||
            (frame->parent_flags & 0x200000u)) {
        game_memory_write_u32(m, child + 0xb0u, record_word(m, child + 0xacu));
        return m->fault ? -1 : 1;
    }
    if (record_word(m, child + 0xa8u) &&
            record_integer(record_word(m, child + 0xb4u) + record_word(m, child + 0xb8u)) > record_integer(tick))
        return m->fault ? -1 : 1;
    if (!record_word(m, child + 0x9cu)) {
        if (!record_emit_pose(m, child, child + 0x24u, frame, frame->angles, s)) return -1;
    } else {
        game_matrix_attachment_orientation(m, orientation, frame->angles);
        if (!record_word(m, child + 0xa0u)) {
            record_floats(m, child + 0x30u, local, 12);
            game_matrix_compose_values(composed, orientation, local);
            if (m->fault || !game_matrix_extract_angles(m, composed, angles) ||
                    !record_emit_pose(m, child, child + 0x24u, frame, angles, s)) return -1;
            record_floats(m, child + 0x6cu, local, 12);
            game_matrix_compose_values(orientation, composed, local);
            if (m->fault || !game_matrix_extract_angles(m, orientation, angles) ||
                    !record_emit_pose(m, child, child + 0x60u, frame, angles, s)) return -1;
        } else {
            alternate = record_word(m, child + 0xa4u);
            record_floats(m, child + (alternate ? 0x6cu : 0x30u), local, 12);
            game_matrix_compose_values(composed, orientation, local);
            if (m->fault || !game_matrix_extract_angles(m, composed, angles) ||
                    !record_emit_pose(m, child, child + (alternate ? 0x60u : 0x24u), frame, angles, s)) return -1;
            game_memory_write_u32(m, child + 0xa4u, !alternate);
        }
    }
    remaining = record_word(m, child + 0xb0u) - 1u;
    game_memory_write_u32(m, child + 0xb0u, remaining);
    if (!record_word(m, child + 0xa8u) || record_integer(remaining) <= 0)
        game_memory_write_u32(m, child + 8u, record_word(m, 0x5ac6acu));
    game_memory_write_u32(m, child + 0xb8u, record_word(m, 0x5ac6acu));
    if (m->fault || !s->notify || !s->notify(s->context, record_word(m, child + 4u),
            game_memory_read_u16(m, child + 16u))) return -1;
    return m->fault ? -1 : 3;
}

static int emit_pose(PlayerAttachmentState *state, const float position[3],
        const PlayerAttachmentFrame *frame, const uint16_t angles[3],
        const PlayerAttachmentServices *services) {
    PlayerAttachmentFrame oriented_frame = *frame;
    memcpy(oriented_frame.angles, angles, sizeof(oriented_frame.angles));
    return player_attachment_submit(state, position, &oriented_frame, services);
}

int player_attachment_update(GameMemory *memory, PlayerAttachmentState *state,
        const PlayerAttachmentFrame *frame, const PlayerAttachmentServices *s) {
    float orientation[12], composed[12];
    uint16_t angles[3];
    if (!memory || !state || !frame || !s) return -1;
    ++stats.updates;
    if (state->kind == PLAYER_SCENE_COMMAND || !state->quantity) return 0;
    if ((int32_t)(state->last_tick + frame->motion_delay) > (int32_t)frame->tick ||
        (frame->parent_flags & 0x200000u)) {
        if (state->kind <= PLAYER_SCENE_DERIVED_SPATIAL)
            state->repeat_remaining = state->repeat_total;
        ++stats.waiting;
        return 1;
    }
    if (state->kind >= PLAYER_SCENE_POSITION) {
        int available;
        if (!s->available || !s->available(s->context, state->kind, &available))
            return -1;
        if (!available) { ++stats.unavailable; return 2; }
        if (!player_attachment_submit(state, state->position, frame, s)) return -1;
        state->last_tick = frame->tick;
    } else {
        if (state->repeating && (int32_t)(state->repeat_delay + state->last_repeat) >
                (int32_t)frame->tick) { ++stats.waiting; return 1; }
        if (!state->oriented) {
            if (!player_attachment_submit(state, state->position, frame, s)) return -1;
        } else {
            game_matrix_attachment_orientation(memory, orientation, frame->angles);
            if (memory->fault) return -1;
            if (!state->mode) {
                ++stats.paired;
                game_matrix_compose_values(composed, orientation, state->local_orientation);
                if (!game_matrix_extract_angles(memory, composed, angles) ||
                    !emit_pose(state, state->position, frame, angles, s)) return -1;
                game_matrix_compose_values(orientation, composed, state->reverse_orientation);
                if (!game_matrix_extract_angles(memory, orientation, angles) ||
                    !emit_pose(state, state->secondary_position, frame, angles, s)) return -1;
            } else {
                ++stats.alternating;
                game_matrix_compose_values(composed, orientation, state->alternate ?
                    state->reverse_orientation : state->local_orientation);
                if (!game_matrix_extract_angles(memory, composed, angles) ||
                    !emit_pose(state, state->alternate ? state->secondary_position :
                        state->position, frame, angles, s)) return -1;
                state->alternate = !state->alternate;
            }
        }
        --state->repeat_remaining;
        if (!state->repeating || (int32_t)state->repeat_remaining <= 0)
            state->last_tick = frame->tick;
        state->last_repeat = frame->tick;
    }
    if (!s->notify || !s->notify(s->context, state->parent,
            (uint16_t)state->heading_word)) return -1;
    return 3;
}
