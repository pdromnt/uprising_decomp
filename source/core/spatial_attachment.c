#include "spatial_attachment.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
    ATTACHMENT_ATAN_TABLE = 0x0062e8b0u,
    ATTACHMENT_TRIG_TABLE = 0x006298a8u,
    ATTACHMENT_ATAN_SCALE = 0x00572dacu
};

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float read_float(GameMemory *memory, GameAddress address) {
    return bits_float(game_memory_read_u32(memory, address));
}

static void write_float(GameMemory *memory, GameAddress address,
        float value) {
    game_memory_write_u32(memory, address, float_bits(value));
}

static uint32_t trunc_low(long double value) {
    /* Retail's ftol helper truncates ST(0) to a signed 64-bit integer and
     * consumes its low dword. Ordinary attachment ratios are table indices. */
    if (!isfinite(value) || value < (long double)INT64_MIN ||
            value >= (long double)INT64_MAX) return 0x80000000u;
    return (uint32_t)(int64_t)value;
}

static uint16_t motion_heading(GameMemory *memory,
        const GameSpatialAttachmentInit *init) {
    float x = init->position[0];
    int32_t count = (int32_t)game_memory_read_u32(memory,
        init->motion + 0x24u);
    float scale = read_float(memory, init->motion + 0x1cu);
    volatile long double extended_z =
        (long double)count * (long double)scale;
    volatile float z = (float)extended_z;
    float atan_scale = read_float(memory, ATTACHMENT_ATAN_SCALE);
    uint32_t index;
    uint16_t table;
    uint16_t heading;
    volatile long double ratio;
    if (memory->fault) return 0;
    if (fabsl((long double)x) < fabsl(extended_z)) {
        ratio = (long double)x * (long double)atan_scale;
        ratio /= z;
        ratio += atan_scale;
        index = trunc_low(ratio);
        table = game_memory_read_u16(memory,
            ATTACHMENT_ATAN_TABLE + index * 2u);
        heading = (int32_t)float_bits(z) > 0 ?
            (uint16_t)(table & 0x07ffu) : (uint16_t)(table + 0x0400u);
    } else {
        ratio = (long double)z * (long double)atan_scale;
        ratio /= x;
        ratio += atan_scale;
        index = trunc_low(ratio);
        table = game_memory_read_u16(memory,
            ATTACHMENT_ATAN_TABLE + index * 2u);
        heading = (int32_t)float_bits(x) > 0 ?
            (uint16_t)(0x0200u - table) :
            (uint16_t)(0x0600u - table);
    }
    return heading;
}

static void write_orientation(GameMemory *memory, GameAddress object,
        uint16_t heading) {
    uint16_t inverse = (uint16_t)(-((uint16_t)(heading * 2u)));
    uint16_t forward = (uint16_t)-inverse;
    uint32_t inverse_sine = game_memory_read_u32(memory,
        ATTACHMENT_TRIG_TABLE + ((uint32_t)inverse & 0x07ffu) * 4u);
    uint32_t inverse_cosine = game_memory_read_u32(memory,
        ATTACHMENT_TRIG_TABLE +
        (((uint32_t)(int32_t)(int16_t)inverse + 0x0200u) & 0x07ffu) * 4u);
    uint32_t forward_sine = game_memory_read_u32(memory,
        ATTACHMENT_TRIG_TABLE + ((uint32_t)forward & 0x07ffu) * 4u);
    uint32_t forward_cosine = game_memory_read_u32(memory,
        ATTACHMENT_TRIG_TABLE +
        (((uint32_t)(int32_t)(int16_t)forward + 0x0200u) & 0x07ffu) * 4u);
    if (memory->fault) return;
    game_memory_write_u32(memory, object + 0x30u, inverse_cosine);
    game_memory_write_u32(memory, object + 0x34u, 0u);
    game_memory_write_u32(memory, object + 0x38u, inverse_sine);
    game_memory_write_u32(memory, object + 0x3cu, 0u);
    game_memory_write_u32(memory, object + 0x40u, 0u);
    write_float(memory, object + 0x44u, 1.0f);
    game_memory_write_u32(memory, object + 0x48u, 0u);
    game_memory_write_u32(memory, object + 0x4cu, 0u);
    write_float(memory, object + 0x50u, -bits_float(inverse_sine));
    game_memory_write_u32(memory, object + 0x54u, 0u);
    game_memory_write_u32(memory, object + 0x58u, inverse_cosine);
    game_memory_write_u32(memory, object + 0x5cu, 0u);
    game_memory_write_u32(memory, object + 0x6cu, forward_cosine);
    game_memory_write_u32(memory, object + 0x70u, 0u);
    game_memory_write_u32(memory, object + 0x74u, forward_sine);
    game_memory_write_u32(memory, object + 0x78u, 0u);
    game_memory_write_u32(memory, object + 0x7cu, 0u);
    write_float(memory, object + 0x80u, 1.0f);
    game_memory_write_u32(memory, object + 0x84u, 0u);
    game_memory_write_u32(memory, object + 0x88u, 0u);
    write_float(memory, object + 0x8cu, -bits_float(forward_sine));
    game_memory_write_u32(memory, object + 0x90u, 0u);
    game_memory_write_u32(memory, object + 0x94u, forward_cosine);
    game_memory_write_u32(memory, object + 0x98u, 0u);
}

int game_spatial_attachment_construct(GameMemory *memory,
        GameAddress object, const GameSpatialAttachmentInit *init) {
    uint32_t motion_link;
    unsigned axis;
    if (!memory || !object || !init) return 0;
    motion_link = game_memory_read_u32(memory, init->motion + 0x28u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, object, 0x00572e08u);
    game_memory_write_u32(memory, object + 4u, init->parent);
    game_memory_write_u32(memory, object + 8u, 0u);
    game_memory_write_u32(memory, object + 0x0cu, motion_link);
    game_memory_write_u16(memory, object + 0x10u, init->heading);
    game_memory_write_u32(memory, object + 0x14u, init->motion);
    game_memory_write_u32(memory, object + 0x18u, 0u);
    game_memory_write_u32(memory, object + 0x1cu, 0u);
    game_memory_write_u32(memory, object + 0x20u, init->kind);
    for (axis = 0u; axis < 3u; ++axis) {
        write_float(memory, object + 0x24u + axis * 4u,
            init->position[axis]);
        write_float(memory, object + 0x60u + axis * 4u,
            init->secondary_position[axis]);
    }
    game_memory_write_u32(memory, object + 0x9cu, init->orient_to_motion);
    game_memory_write_u32(memory, object + 0xa0u, init->mode);
    game_memory_write_u32(memory, object + 0xa4u, 0u);
    game_memory_write_u32(memory, object + 0xa8u, 1u);
    game_memory_write_u32(memory, object + 0xacu, init->resource);
    game_memory_write_u32(memory, object + 0xb0u, init->resource);
    game_memory_write_u32(memory, object + 0xb4u, init->slot);
    game_memory_write_u32(memory, object + 0xb8u, 0u);
    if (!memory->fault && init->orient_to_motion)
        write_orientation(memory, object, motion_heading(memory, init));
    return !memory->fault;
}
