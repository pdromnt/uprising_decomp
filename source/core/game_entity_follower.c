#include "game_entity_follower.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static float follower_read_float(GameMemory *memory, GameAddress address) {
    uint32_t bits = game_memory_read_u32(memory, address);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t follower_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void follower_write_float(GameMemory *memory, GameAddress address,
    float value) {
    game_memory_write_u32(memory, address, follower_float_bits(value));
}

static float follower_add(float left, float right) {
    volatile long double value = (long double)left + (long double)right;
    return (float)value;
}

static int follower_is_near(float value, float target, float threshold) {
    long double distance = fabsl((long double)value - (long double)target);
    /* Retail tests only x87 C0, so unordered compares take the near arm. */
    return isnan(distance) || isnan(threshold) ||
        distance < (long double)threshold;
}

int game_entity_follower_update(GameMemory *memory, GameAddress object,
    GameAddress scratch, GameFollowerCall call, void *context) {
    GameAddress vtable = game_memory_read_u32(memory, object);
    uint32_t first_result;
    uint32_t query_result;
    GameAddress manager;
    GameAddress manager_vtable;
    uint32_t arguments[4];
    float threshold;
    float predicted_x;
    float predicted_y;
    float predicted_z;
    float position_x;
    float position_y;
    float position_z;
    float target_x;
    float target_y;
    float target_z;
    uint32_t hop_active;
    uint32_t hop_bits;

    if (memory->fault || !call(context,
            game_memory_read_u32(memory, vtable + 0x60u), object,
            NULL, 0u, &first_result, 0))
        return 0;

    manager = game_memory_read_u32(memory, 0x0058973cu);
    manager_vtable = game_memory_read_u32(memory, manager);
    arguments[0] = (first_result & 0xffffff00u) |
        game_memory_read_u8(memory, object + 0x0cu);
    arguments[1] = 5u;
    arguments[2] = first_result;
    if (memory->fault || !call(context,
            game_memory_read_u32(memory, manager_vtable + 0x44u), manager,
            arguments, 3u, &query_result, 0))
        return 0;
    threshold = (float)((long double)(int32_t)query_result + 1.0L);

    position_x = follower_read_float(memory, object + 0x18u);
    position_y = follower_read_float(memory, object + 0x1cu);
    position_z = follower_read_float(memory, object + 0x20u);
    target_x = follower_read_float(memory, object + 0xf8u);
    target_y = follower_read_float(memory, object + 0xfcu);
    target_z = follower_read_float(memory, object + 0x100u);
    predicted_x = follower_add(position_x,
        follower_read_float(memory, object + 0xb4u));
    predicted_z = follower_add(position_z,
        follower_read_float(memory, object + 0xb8u));
    hop_active = game_memory_read_u32(memory, object + 0x168u);
    hop_bits = game_memory_read_u32(memory, object + 0x164u);

    if (hop_active != 0u || (hop_bits & 0x7fffffffu) != 0u) {
        if (hop_active != 0u) {
            float hop = follower_add(follower_read_float(memory,
                object + 0x164u), 24.0f);
            follower_write_float(memory, object + 0x164u, hop);
            if ((int32_t)follower_float_bits(hop) > (int32_t)0x442f0000u)
                game_memory_write_u32(memory, object + 0x168u, 0u);
            position_y = follower_add(position_y, 24.0f);
        } else {
            float hop = follower_read_float(memory, object + 0x164u);
            if ((follower_float_bits(hop) & 0x7fffffffu) != 0u) {
                follower_write_float(memory, object + 0x164u,
                    follower_add(hop, -24.0f));
                position_y = follower_add(position_y, -24.0f);
            }
        }
        follower_write_float(memory, object + 0x1cu, position_y);

        predicted_y = follower_add(position_y,
            follower_read_float(memory, object + 0x148u));
        follower_write_float(memory, scratch, predicted_x);
        follower_write_float(memory, scratch + 4u, predicted_y);
        follower_write_float(memory, scratch + 8u, predicted_z);
        arguments[0] = object + 0x14cu;
        arguments[1] = object + 0x14eu;
        arguments[2] = scratch;
        arguments[3] = object;
        if (memory->fault || !call(context, 0x00410d10u,
                object + 0x14eu, arguments, 4u, NULL, 1))
            return 0;

        if (follower_is_near(position_y, target_y, threshold)) {
            game_memory_write_u32(memory, object + 0x168u, 0u);
            game_memory_write_u32(memory, object + 0x164u, 0u);
        }
    }

    if (!follower_is_near(position_x, target_x, threshold)) {
        position_x = follower_add(position_x,
            follower_read_float(memory, object + 0xb4u));
        follower_write_float(memory, object + 0x18u, position_x);
    }
    if (!follower_is_near(position_z, target_z, threshold)) {
        position_z = follower_add(position_z,
            follower_read_float(memory, object + 0xb8u));
        follower_write_float(memory, object + 0x20u, position_z);
    }
    if (!follower_is_near(position_y, target_y, threshold)) {
        position_y = follower_add(position_y,
            follower_read_float(memory, object + 0x148u));
        follower_write_float(memory, object + 0x1cu, position_y);
    }

    game_memory_write_u8(memory, object + 0xd8u,
        (uint8_t)(game_memory_read_u8(memory, object + 0xd8u) | 8u));
    if (follower_is_near(position_x, target_x, threshold) &&
        follower_is_near(position_z, target_z, threshold) &&
        follower_is_near(position_y, target_y, threshold))
        game_memory_write_u32(memory, object + 0xe0u, 5u);
    if (memory->fault) return 0;

    vtable = game_memory_read_u32(memory, object);
    return call(context, game_memory_read_u32(memory, vtable + 0x1ccu),
        object, NULL, 0u, NULL, 0);
}
