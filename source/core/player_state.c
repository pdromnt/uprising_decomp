#include "gameplay_spatial.h"
#include "presentation_instance.h"
#include "terrain_asset.h"
#include "player_state.h"
#include "gameplay_geometry.h"
#include "presentation_instance.h"

#include <limits.h>
#include <math.h>
#include <string.h>

enum {
    PLAYER_POSITION_X = 0x018u,
    PLAYER_POSITION_Z = 0x020u,
    PLAYER_ACTION_FLAGS = 0x140u,
    PLAYER_SELECTED_LEFT = 0x1ccu,
    PLAYER_SELECTED_RIGHT = 0x1d0u,
    PLAYER_INVENTORY_FIRST = 0x1d4u,
    PLAYER_DEFAULT_PRIMARY = 0x1d4u,
    PLAYER_DEFAULT_SECONDARY = 0x1d8u,
    PLAYER_LEFT_MODE = 0x214u,
    PLAYER_RIGHT_MODE = 0x218u,
    PLAYER_TYPE3_CHILD = 0x21cu,
    PLAYER_TYPE15_CHILD = 0x220u,
    WEAPON_COUNT = 0x0cu,
    WEAPON_DESCRIPTOR = 0x14u,
    DESCRIPTOR_TYPE = 0x0cu,
    DESCRIPTOR_MAX_COUNT = 0x28u,
    INVENTORY_COUNT = 15u,
    FIRE_LEFT = 0x00010000u,
    FIRE_RIGHT = 0x00020000u,
    WEAPON_TYPE_PERSISTENT_LEFT = 3u,
    WEAPON_TYPE_PERSISTENT_RIGHT = 15u,
    PERSISTENT_CHILD_OWNER = 0xe4u,
    RECHARGE_ZONE_HEAD = 0x294u,
    RECHARGE_ZONE_MIN_X = 0x04u,
    RECHARGE_ZONE_MIN_Z = 0x08u,
    RECHARGE_ZONE_MAX_X = 0x0cu,
    RECHARGE_ZONE_MAX_Z = 0x10u,
    RECHARGE_ZONE_NEXT = 0x64u
};

static int32_t player_signed_bits(uint32_t bits) {
    int32_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float player_read_f32(GamePlayerRef player, uint32_t offset) {
    uint32_t bits = game_memory_read_u32(player.memory,
        player.address + offset);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int player_signed_less(uint32_t left, uint32_t right) {
    return player_signed_bits(left) < player_signed_bits(right);
}

static int player_increment_clamped(GamePlayerRef player,
    GameAddress weapon, uint32_t amount, int count_before_maximum) {
    GameAddress descriptor = game_memory_read_u32(player.memory,
        weapon + WEAPON_DESCRIPTOR);
    uint32_t current;
    uint32_t maximum;
    uint32_t candidate;
    if (player.memory->fault) return 0;
    if (count_before_maximum) {
        current = game_memory_read_u32(player.memory, weapon + WEAPON_COUNT);
        maximum = game_memory_read_u32(player.memory,
            descriptor + DESCRIPTOR_MAX_COUNT);
    } else {
        maximum = game_memory_read_u32(player.memory,
            descriptor + DESCRIPTOR_MAX_COUNT);
        current = game_memory_read_u32(player.memory, weapon + WEAPON_COUNT);
    }
    if (player.memory->fault) return 0;
    if (player_signed_less(maximum, current)) return 1;
    candidate = current + amount;
    if (!player_signed_less(candidate, maximum)) candidate = maximum;
    game_memory_write_u32(player.memory, weapon + WEAPON_COUNT, candidate);
    return !player.memory->fault;
}

GamePlayerRef game_player_ref(GameMemory *memory, GameAddress address) {
    GamePlayerRef result;
    result.memory = memory;
    result.address = address;
    return result;
}

int game_player_refill_inventory_to_max(GamePlayerRef player) {
    uint32_t index = INVENTORY_COUNT;
    while (index != 0u) {
        GameAddress weapon;
        GameAddress descriptor;
        uint32_t maximum;
        --index;
        weapon = game_memory_read_u32(player.memory, player.address +
            PLAYER_INVENTORY_FIRST + index * 4u);
        if (player.memory->fault) return 0;
        if (weapon == 0u) continue;
        descriptor = game_memory_read_u32(player.memory,
            weapon + WEAPON_DESCRIPTOR);
        maximum = game_memory_read_u32(player.memory,
            descriptor + DESCRIPTOR_MAX_COUNT);
        if (player.memory->fault) return 0;
        game_memory_write_u32(player.memory, weapon + WEAPON_COUNT, maximum);
        if (player.memory->fault) return 0;
    }
    return 1;
}

static int lower_bound_passes(float lower, float value) {
    return isnan(lower) || isnan(value) || lower <= value;
}

static int upper_bound_passes(float upper, float value) {
    return !isnan(upper) && !isnan(value) && upper >= value;
}

static int player_inside_recharge_zone(GamePlayerRef player,
    GameAddress zone) {
    uint32_t bits;
    float bound;
    float value;
    bits = game_memory_read_u32(player.memory, zone + RECHARGE_ZONE_MIN_X);
    memcpy(&bound, &bits, sizeof(bound));
    value = player_read_f32(player, PLAYER_POSITION_X);
    if (player.memory->fault || !lower_bound_passes(bound, value)) return 0;
    bits = game_memory_read_u32(player.memory, zone + RECHARGE_ZONE_MAX_X);
    memcpy(&bound, &bits, sizeof(bound));
    value = player_read_f32(player, PLAYER_POSITION_X);
    if (player.memory->fault || !upper_bound_passes(bound, value)) return 0;
    bits = game_memory_read_u32(player.memory, zone + RECHARGE_ZONE_MIN_Z);
    memcpy(&bound, &bits, sizeof(bound));
    value = player_read_f32(player, PLAYER_POSITION_Z);
    if (player.memory->fault || !lower_bound_passes(bound, value)) return 0;
    bits = game_memory_read_u32(player.memory, zone + RECHARGE_ZONE_MAX_Z);
    memcpy(&bound, &bits, sizeof(bound));
    value = player_read_f32(player, PLAYER_POSITION_Z);
    return !player.memory->fault && upper_bound_passes(bound, value);
}

int game_player_update_recharge(GamePlayerRef player, uint32_t tick,
    GameAddress world_manager, GamePlayerRechargeResult *result) {
    GameAddress zone;
    if (result) *result = GAME_PLAYER_RECHARGE_NONE;
    if ((tick & 3u) != 0u) return 1;
    zone = game_memory_read_u32(player.memory,
        world_manager + RECHARGE_ZONE_HEAD);
    if (player.memory->fault) return 0;
    while (zone != 0u) {
        if (player_inside_recharge_zone(player, zone)) {
            GameAddress primary = game_memory_read_u32(player.memory,
                player.address + PLAYER_DEFAULT_PRIMARY);
            GameAddress secondary;
            if (player.memory->fault ||
                    !player_increment_clamped(player, primary, 2u, 0))
                return 0;
            secondary = game_memory_read_u32(player.memory,
                player.address + PLAYER_DEFAULT_SECONDARY);
            if (player.memory->fault ||
                    !player_increment_clamped(player, secondary, 2u, 1))
                return 0;
            if (result) *result = GAME_PLAYER_RECHARGE_ZONE;
            return 1;
        }
        if (player.memory->fault) return 0;
        zone = game_memory_read_u32(player.memory, zone + RECHARGE_ZONE_NEXT);
        if (player.memory->fault) return 0;
    }
    if ((tick & 7u) == 0u) {
        GameAddress primary = game_memory_read_u32(player.memory,
            player.address + PLAYER_DEFAULT_PRIMARY);
        if (player.memory->fault ||
                !player_increment_clamped(player, primary, 1u, 1)) return 0;
        if (result) *result = GAME_PLAYER_RECHARGE_PASSIVE;
    }
    return 1;
}

static int player_weapon_type(GamePlayerRef player, uint32_t selected_offset,
    uint8_t *type) {
    GameAddress weapon = game_memory_read_u32(player.memory,
        player.address + selected_offset);
    GameAddress descriptor = game_memory_read_u32(player.memory,
        weapon + WEAPON_DESCRIPTOR);
    if (player.memory->fault) return 0;
    *type = game_memory_read_u8(player.memory, descriptor + DESCRIPTOR_TYPE);
    return !player.memory->fault;
}

static int player_release_persistent_child(GamePlayerRef player,
    uint32_t selected_offset, uint8_t weapon_type, uint32_t child_offset) {
    uint8_t selected_type;
    GameAddress child;
    if (!player_weapon_type(player, selected_offset, &selected_type)) return 0;
    if (selected_type != weapon_type) return 1;
    child = game_memory_read_u32(player.memory, player.address + child_offset);
    if (player.memory->fault) return 0;
    if (child == 0u) return 1;
    game_memory_write_u32(player.memory, child + PERSISTENT_CHILD_OWNER, 0u);
    game_memory_write_u32(player.memory, player.address + child_offset, 0u);
    return !player.memory->fault;
}

int game_player_repair_empty_weapon_selections(GamePlayerRef player) {
    GameAddress selected;
    GameAddress fallback;
    uint32_t count;
    uint32_t flags;
    selected = game_memory_read_u32(player.memory,
        player.address + PLAYER_SELECTED_LEFT);
    count = game_memory_read_u32(player.memory, selected + WEAPON_COUNT);
    if (player.memory->fault) return 0;
    if (count == 0u) {
        flags = game_memory_read_u32(player.memory,
            player.address + PLAYER_ACTION_FLAGS);
        game_memory_write_u32(player.memory, player.address +
            PLAYER_ACTION_FLAGS, flags & ~FIRE_LEFT);
        if (player.memory->fault ||
                !player_release_persistent_child(player,
                    PLAYER_SELECTED_LEFT, WEAPON_TYPE_PERSISTENT_LEFT,
                    PLAYER_TYPE3_CHILD) ||
                !player_release_persistent_child(player,
                    PLAYER_SELECTED_LEFT, WEAPON_TYPE_PERSISTENT_RIGHT,
                    PLAYER_TYPE15_CHILD)) return 0;
        game_memory_write_u32(player.memory,
            player.address + PLAYER_LEFT_MODE, 0u);
        fallback = game_memory_read_u32(player.memory,
            player.address + PLAYER_DEFAULT_PRIMARY);
        game_memory_write_u32(player.memory,
            player.address + PLAYER_SELECTED_LEFT, fallback);
        if (player.memory->fault) return 0;
    }
    selected = game_memory_read_u32(player.memory,
        player.address + PLAYER_SELECTED_RIGHT);
    count = game_memory_read_u32(player.memory, selected + WEAPON_COUNT);
    if (player.memory->fault) return 0;
    if (count == 0u) {
        flags = game_memory_read_u32(player.memory,
            player.address + PLAYER_ACTION_FLAGS);
        game_memory_write_u32(player.memory, player.address +
            PLAYER_ACTION_FLAGS, flags & ~FIRE_RIGHT);
        if (player.memory->fault ||
                !player_release_persistent_child(player,
                    PLAYER_SELECTED_RIGHT, WEAPON_TYPE_PERSISTENT_LEFT,
                    PLAYER_TYPE3_CHILD) ||
                !player_release_persistent_child(player,
                    PLAYER_SELECTED_RIGHT, WEAPON_TYPE_PERSISTENT_RIGHT,
                    PLAYER_TYPE15_CHILD)) return 0;
        fallback = game_memory_read_u32(player.memory,
            player.address + PLAYER_DEFAULT_SECONDARY);
        count = game_memory_read_u32(player.memory, fallback + WEAPON_COUNT);
        if (player.memory->fault) return 0;
        if (count == 0u) {
            game_memory_write_u32(player.memory,
                player.address + PLAYER_RIGHT_MODE, 0u);
            fallback = game_memory_read_u32(player.memory,
                player.address + PLAYER_DEFAULT_PRIMARY);
        } else {
            game_memory_write_u32(player.memory,
                player.address + PLAYER_RIGHT_MODE, 1u);
        }
        game_memory_write_u32(player.memory,
            player.address + PLAYER_SELECTED_RIGHT, fallback);
        if (player.memory->fault) return 0;
    }
    return 1;
}

static int player_collision_cache_subtype(uint32_t subtype) {
    return subtype <= 11u && !(subtype >= 2u && subtype <= 4u);
}

static int game_player_ignore_type_b0(GamePlayerRef player,
        GameAddress candidate) {
    uint32_t subtype = game_memory_read_u32(player.memory, candidate + 0x64u);
    uint32_t expires;
    uint32_t tick;
    if (player.memory->fault || !player_collision_cache_subtype(subtype))
        return 0;
    if (game_memory_read_u32(player.memory, candidate + 0x14u) !=
            game_memory_read_u32(player.memory, player.address + 0x48cu))
        return 0;
    expires = game_memory_read_u32(player.memory, player.address + 0x490u) + 20u;
    tick = game_memory_read_u32(player.memory, 0x005acbe8u);
    if (player.memory->fault || (int32_t)expires <= (int32_t)tick) return 0;
    return (int32_t)game_memory_read_u32(player.memory,
        player.address + 0x494u) > 3;
}

int game_player_should_ignore_collider(GamePlayerRef player,
        GameAddress candidate) {
    uint8_t type;
    if ((game_memory_read_u32(player.memory, candidate + 0x10u) &
            0x400u) != 0u)
        return !player.memory->fault;
    type = game_memory_read_u8(player.memory, candidate + 0x0cu);
    if (player.memory->fault) return 0;
    switch (type) {
        case 10u:
            return (game_memory_read_u8(player.memory,
                candidate + 0xe5u) & 0x40u) != 0u;
        case 17u:
            return ((game_memory_read_u32(player.memory,
                player.address + 0x14u) ^ game_memory_read_u32(player.memory,
                candidate + 0x14u)) & 0x3fu) == 0u;
        case 64u: {
            uint32_t player_slot = game_memory_read_u32(player.memory,
                player.address + 0x14u) & 0x3fu;
            uint32_t candidate_slot = game_memory_read_u32(player.memory,
                candidate + 0x14u) & 0x3fu;
            uint32_t relationship = game_memory_read_u32(player.memory,
                0x005f0160u + player_slot * 24u + candidate_slot * 4u);
            uint32_t flags = game_memory_read_u32(player.memory,
                player.address + 0x2f4u);
            return !player.memory->fault &&
                (relationship == 0u || (flags & 0x20u) == 0u);
        }
        case 162u:
            return game_memory_read_u32(player.memory,
                candidate + 0xb8u) == 1u;
        case 176u:
            return game_player_ignore_type_b0(player, candidate);
        default:
            return 0;
    }
}

static void game_player_remember_collision(GamePlayerRef player,
        GameAddress candidate) {
    uint32_t owner;
    uint32_t tick;
    if (game_memory_read_u8(player.memory, candidate + 0x0cu) != 0xb0u ||
            !player_collision_cache_subtype(game_memory_read_u32(
                player.memory, candidate + 0x64u)))
        return;
    owner = game_memory_read_u32(player.memory, candidate + 0x14u);
    tick = game_memory_read_u32(player.memory, 0x005acbe8u);
    if (player.memory->fault) return;
    if (owner == game_memory_read_u32(player.memory,
            player.address + 0x48cu) &&
            (int32_t)(game_memory_read_u32(player.memory,
                player.address + 0x490u) + 20u) > (int32_t)tick) {
        uint32_t count = game_memory_read_u32(player.memory,
            player.address + 0x494u);
        if (!player.memory->fault)
            game_memory_write_u32(player.memory,
                player.address + 0x494u, count + 1u);
        return;
    }
    game_memory_write_u32(player.memory, player.address + 0x48cu, owner);
    game_memory_write_u32(player.memory, player.address + 0x494u, 1u);
    game_memory_write_u32(player.memory, player.address + 0x490u, tick);
}

int game_player_find_collision(GamePlayerRef player, uint32_t first,
        uint32_t second, GameAddress excluded, int remember_hit,
        const GamePlayerCollisionOps *ops, GameAddress *found) {
    GameAddress buckets=0u,heads[9];
    const PresentationPose *pose=presentation_instance_pose(player.address);
    int native_heads=0;
    unsigned bucket_index;
    if (!found || !ops || !ops->probe || !ops->query_buckets) return 0;
    *found = 0u;
    if (excluded != 0u) {
        uint32_t collides;
        if (!ops->probe(ops->context, excluded, player.address,
                first, second, &collides)) return 0;
        if (collides != 0u) { *found = excluded; return 1; }
    }
    if(pose)native_heads=gameplay_spatial_query_heads(gameplay_spatial_grid(0x005f0800u),pose->position[0],pose->position[2],heads);
    if (!native_heads && (!ops->query_buckets(ops->context, player.address,
            game_memory_read_u32(player.memory, player.address + 0x18u),
            game_memory_read_u32(player.memory, player.address + 0x20u),
            &buckets) || player.memory->fault))
        return 0;
    for (bucket_index = 0u; bucket_index < 9u; ++bucket_index) {
        GameAddress head = native_heads?0u:game_memory_read_u32(player.memory,
            buckets + bucket_index * 4u);
        GameAddress candidate = native_heads?heads[bucket_index]:game_memory_read_u32(player.memory, head + 4u);
        if (player.memory->fault) return 0;
        while (candidate != 0u) {
            if (candidate != player.address && candidate != excluded &&
                    !game_player_should_ignore_collider(player, candidate)) {
                uint32_t collides;
                if (player.memory->fault || !ops->probe(ops->context,
                        candidate, player.address, first, second, &collides))
                    return 0;
                if (collides != 0u) {
                    if (remember_hit)
                        game_player_remember_collision(player, candidate);
                    if (player.memory->fault) return 0;
                    *found = candidate;
                    return 1;
                }
            }
            if (player.memory->fault) return 0;
            candidate = game_memory_read_u32(player.memory, candidate + 4u);
        }
    }
    return !player.memory->fault;
}

GamePlayerAimResult game_player_composite_aim(GamePlayerRef player,
        uint32_t base_angle) {
    GamePlayerAimResult result = {0u, 0u, 0u};
    result.partial = (uint32_t)(int32_t)(int16_t)game_memory_read_u16(
        player.memory, player.address + 0x26cu);
    result.partial += base_angle;
    result.direction = game_memory_read_u32(player.memory,
        player.address + 0x1acu);
    result.partial += (uint32_t)(int32_t)(int16_t)game_memory_read_u16(
        player.memory, player.address + 0x224u);
    result.partial += game_memory_read_u32(player.memory,
        player.address + 0x498u);
    result.tail = (uint32_t)(int32_t)(int16_t)game_memory_read_u16(
        player.memory, player.address + 0x1b8u);
    result.partial += result.direction;
    result.partial += game_memory_read_u32(player.memory,
        player.address + 0x1b0u);
    result.partial += game_memory_read_u32(player.memory,
        player.address + 0x1b4u);
    return result;
}

int game_player_sample_animation(GamePlayerRef player,
        const GamePlayerAnimationOps *ops) {
    static const uint16_t channels[8] = {
        0x308u, 0x324u, 0x340u, 0x35cu,
        0x378u, 0x394u, 0x3b0u, 0x3ccu};
    static const uint16_t destinations[8] = {
        0x018u, 0x01cu, 0x020u, 0x024u,
        0x026u, 0x028u, 0x26cu, 0x224u};
    static const uint8_t kinds[8] = {
        GAME_PLAYER_SAMPLE_FLOAT, GAME_PLAYER_SAMPLE_FLOAT,
        GAME_PLAYER_SAMPLE_FLOAT, GAME_PLAYER_SAMPLE_UNSIGNED_ANGLE,
        GAME_PLAYER_SAMPLE_SIGNED_ANGLE, GAME_PLAYER_SAMPLE_SIGNED_ANGLE,
        GAME_PLAYER_SAMPLE_UNSIGNED_ANGLE, GAME_PLAYER_SAMPLE_SIGNED_ANGLE};
    unsigned index;
    if (!ops || !ops->sample) return 0;
    for (index = 0u; index < 8u; ++index) {
        if (!ops->sample(ops->context,
                player.address + channels[index],
                player.address + destinations[index],
                (GamePlayerAnimationSampleKind)kinds[index]))
            return 0;
    }
    return !player.memory->fault;
}

GamePlayerPresentationPlan game_player_prepare_presentation(
        GamePlayerRef player, GameAddress source) {
    GamePlayerPresentationPlan plan = {0};
    plan.owner_index = game_memory_read_u32(player.memory,
        player.address + 0x14u) & 0x3fu;
    plan.active_world = game_memory_read_u32(player.memory, 0x005f0208u);
    plan.owner = game_memory_read_u32(player.memory,
        0x005f0210u + plan.owner_index * 4u);
    if (player.memory->fault || plan.owner != plan.active_world) return plan;
    plan.active = 1;
    plan.position_x = game_memory_read_u32(player.memory, source + 0u);
    plan.position_y = game_memory_read_u32(player.memory, source + 4u);
    plan.position_z = game_memory_read_u32(player.memory, source + 8u);
    game_memory_write_u32(player.memory, player.address + 0x64u,
        plan.position_x);
    game_memory_write_u32(player.memory, player.address + 0x68u,
        plan.position_y);
    game_memory_write_u32(player.memory, player.address + 0x6cu,
        plan.position_z);
    plan.vtable = game_memory_read_u32(player.memory, player.address);
    plan.first_target = game_memory_read_u32(player.memory,
        plan.vtable + 0x90u);
    plan.second_target = game_memory_read_u32(player.memory,
        plan.vtable + 0x94u);
    return plan;
}

uint32_t game_player_finish_presentation(GamePlayerRef player) {
    uint32_t flags = game_memory_read_u32(player.memory,
        player.address + 0x2f4u);
    if (!player.memory->fault)
        game_memory_write_u32(player.memory, player.address + 0x2f4u,
            flags | 2u);
    return flags;
}

GamePlayerTurretDisplayPlan game_player_turret_display_plan(
        GamePlayerRef player) {
    GamePlayerTurretDisplayPlan plan = {0};
    plan.pitch = (int16_t)game_memory_read_u16(player.memory,
        player.address + 0x226u);
    if (player.memory->fault) return plan;
    if (plan.pitch > -113) {
        plan.mode = GAME_PLAYER_TURRET_DISPLAY_CHANNEL;
    } else if (plan.pitch < -284) {
        plan.mode = GAME_PLAYER_TURRET_DISPLAY_ZERO;
        return plan;
    } else {
        plan.mode = GAME_PLAYER_TURRET_DISPLAY_SCALED;
        plan.delta = -113 - (int32_t)plan.pitch;
    }
    plan.channel = (int16_t)game_memory_read_u16(player.memory,
        player.address + 0x2ccu);
    return plan;
}

int game_player_record_history(GamePlayerRef player,
        GamePlayerHistoryMode mode, const GamePlayerHistoryOps *ops) {
    static const uint16_t sources[8] = {
        0x018u, 0x01cu, 0x020u, 0x024u,
        0x026u, 0x028u, 0x26cu, 0x224u};
    static const uint16_t channels[8] = {
        0x308u, 0x324u, 0x340u, 0x35cu,
        0x378u, 0x394u, 0x3b0u, 0x3ccu};
    static const uint8_t kinds[8] = {
        GAME_PLAYER_HISTORY_SCALAR, GAME_PLAYER_HISTORY_SCALAR,
        GAME_PLAYER_HISTORY_SCALAR, GAME_PLAYER_HISTORY_UNSIGNED_ANGLE,
        GAME_PLAYER_HISTORY_SIGNED_ANGLE, GAME_PLAYER_HISTORY_SIGNED_ANGLE,
        GAME_PLAYER_HISTORY_UNSIGNED_ANGLE, GAME_PLAYER_HISTORY_SIGNED_ANGLE};
    unsigned index;
    if (!ops || !ops->sample) return 0;
    for (index = 0u; index < 8u; ++index) {
        if (!ops->sample(ops->context, player, mode, index,
                player.address + sources[index],
                player.address + channels[index],
                (GamePlayerHistorySampleKind)kinds[index]))
            return 0;
    }
    return !player.memory->fault;
}

GamePlayerDeathBeginPlan game_player_begin_death(GamePlayerRef player) {
    GamePlayerDeathBeginPlan plan = {0};
    plan.tick = game_memory_read_u32(player.memory, 0x005ac6acu);
    if (player.memory->fault) return plan;
    game_memory_write_u32(player.memory, player.address + 0x498u, plan.tick);
    if (player.memory->fault) return plan;
    plan.owner_index = game_memory_read_u32(player.memory,
        player.address + 0x14u) & 0x3fu;
    if (player.memory->fault) return plan;
    plan.owner = game_memory_read_u32(player.memory,
        0x005f0210u + plan.owner_index * 4u);
    if (player.memory->fault) return plan;
    game_memory_write_u32(player.memory, plan.owner + 0x5a0u, 0u);
    if (player.memory->fault) return plan;
    plan.local_player = game_memory_read_u32(player.memory, 0x005f0208u);
    if (player.memory->fault) return plan;
    plan.is_local = plan.local_player == plan.owner;
    if (plan.is_local)
        plan.game_mode = game_memory_read_u32(player.memory, 0x00604760u);
    return plan;
}

GamePlayerDeathCameraPlan game_player_prepare_death_camera(
        GamePlayerRef player) {
    GamePlayerDeathCameraPlan plan = {0};
    plan.primary = game_memory_read_u32(player.memory, 0x0058f890u);
    if (player.memory->fault) return plan;
    plan.current = game_memory_read_u32(player.memory, 0x0058f898u);
    if (player.memory->fault) return plan;
    if (plan.current == plan.primary) {
        plan.path = GAME_PLAYER_DEATH_CAMERA_CURRENT;
        plan.angle = game_memory_read_u16(player.memory, plan.current + 6u);
        if (player.memory->fault) return plan;
        plan.distance = game_memory_read_u32(player.memory,
            player.address + 0x30u);
        plan.camera_source = plan.current + 0x0cu;
        plan.pose_source = player.address + 0x18u;
        plan.working_player = player.address + 0x18u;
        return plan;
    }
    plan.secondary = game_memory_read_u32(player.memory, 0x0058f89cu);
    if (player.memory->fault) return plan;
    plan.primary = game_memory_read_u32(player.memory, 0x0058f890u);
    if (player.memory->fault) return plan;
    plan.distance = game_memory_read_u32(player.memory,
        player.address + 0x30u);
    if (player.memory->fault) return plan;
    if (plan.primary == plan.secondary) {
        plan.path = GAME_PLAYER_DEATH_CAMERA_SECONDARY;
        plan.secondary = game_memory_read_u32(player.memory, 0x0058f89cu);
        if (player.memory->fault) return plan;
        plan.angle = game_memory_read_u16(player.memory,
            plan.secondary + 6u);
        plan.camera_source = plan.secondary + 0x0cu;
        plan.pose_source = player.address + 0x18u;
        plan.working_player = player.address + 0x18u;
        return plan;
    }
    plan.path = GAME_PLAYER_DEATH_CAMERA_FALLBACK;
    plan.angle = game_memory_read_u16(player.memory, player.address + 0x78u);
    plan.camera_source = player.address + 0x64u;
    plan.pose_source = player.address + 0x18u;
    plan.working_player = player.address + 0x64u;
    return plan;
}

GameAddress game_player_death_camera_controller(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0058f8b0u);
}

static int game_player_walk_slots(GamePlayerRef player, uint32_t argument,
        uint32_t vtable_offset, const GamePlayerSlotOps *ops) {
    unsigned index;
    for (index = 0u; index < 15u; ++index) {
        GameAddress slot = game_memory_read_u32(player.memory,
            player.address + 0x1d4u + index * 4u);
        if (player.memory->fault) return 0;
        if (slot != 0u) {
            GameAddress vtable = game_memory_read_u32(player.memory, slot);
            GameAddress target;
            if (player.memory->fault) return 0;
            target = game_memory_read_u32(player.memory,
                vtable + vtable_offset);
            if (player.memory->fault ||
                    !ops->visit(ops->context, index, slot, vtable, target,
                        argument))
                return 0;
        }
        if (!ops->advance(ops->context, index)) return 0;
    }
    return 1;
}

int game_player_configure_slots(GamePlayerRef player, uint32_t argument,
        const GamePlayerSlotOps *ops) {
    if (!ops || !ops->configure || !ops->visit || !ops->advance ||
            !ops->select)
        return 0;
    if (!ops->configure(ops->context, player.address, argument)) return 0;
    if (!game_player_walk_slots(player, argument, 0x0cu, ops)) return 0;
    return ops->select(ops->context, player.address, argument) &&
        !player.memory->fault;
}

int game_player_refresh_slots(GamePlayerRef player, uint32_t argument,
        const GamePlayerSlotOps *ops, GamePlayerSlotSnapshot *snapshot) {
    GamePlayerSlotSnapshot result = {0};
    if (!ops || !snapshot || !ops->unlink || !ops->refresh ||
            !ops->visit || !ops->advance || !ops->select)
        return 0;
    if (!ops->unlink(ops->context, player.address) ||
            !ops->refresh(ops->context, player.address, argument) ||
            !game_player_walk_slots(player, argument, 0x10u, ops) ||
            !ops->select(ops->context, player.address, argument))
        return 0;
    result.first_index = game_memory_read_u32(player.memory,
        player.address + 0x214u);
    if (player.memory->fault) return 0;
    result.second_index = game_memory_read_u32(player.memory,
        player.address + 0x218u);
    if (player.memory->fault) return 0;
    result.first_slot = game_memory_read_u32(player.memory,
        player.address + 0x1d4u + result.first_index * 4u);
    if (player.memory->fault) return 0;
    game_memory_write_u32(player.memory, player.address + 0x1ccu,
        result.first_slot);
    if (player.memory->fault) return 0;
    result.position_x = game_memory_read_u32(player.memory,
        player.address + 0x18u);
    if (player.memory->fault) return 0;
    result.second_slot = game_memory_read_u32(player.memory,
        player.address + 0x1d4u + result.second_index * 4u);
    if (player.memory->fault) return 0;
    result.position_y = game_memory_read_u32(player.memory,
        player.address + 0x1cu);
    if (player.memory->fault) return 0;
    game_memory_write_u32(player.memory, player.address + 0x1d0u,
        result.second_slot);
    if (player.memory->fault) return 0;
    game_memory_write_u32(player.memory, player.address + 0x1bcu,
        result.position_x);
    if (player.memory->fault) return 0;
    result.angle = game_memory_read_u16(player.memory,
        player.address + 0x24u);
    if (player.memory->fault) return 0;
    result.position_z = game_memory_read_u32(player.memory,
        player.address + 0x20u);
    if (player.memory->fault) return 0;
    game_memory_write_u16(player.memory, player.address + 0x1c8u,
        result.angle);
    if (player.memory->fault) return 0;
    game_memory_write_u32(player.memory, player.address + 0x1c0u,
        result.position_y);
    if (player.memory->fault) return 0;
    game_memory_write_u32(player.memory, player.address + 0x1c4u,
        result.position_z);
    if (player.memory->fault) return 0;
    *snapshot = result;
    return 1;
}

GamePlayerCallSquareSearch game_player_find_call_square(
        GamePlayerRef player, const GamePlayerCallSquareOps *ops) {
    GamePlayerCallSquareSearch result = {0};
    if (!ops || !ops->contains) return result;
    result.count = (int16_t)game_memory_read_u16(player.memory, 0x00639a74u);
    if (player.memory->fault) return result;
    if (ops->compare_count &&
            !ops->compare_count(ops->context, result.count))
        return result;
    if (result.count <= 0) return result;
    result.table = game_memory_read_u32(player.memory, 0x00639a70u);
    if (player.memory->fault) return result;
    result.cursor = result.table;
    for (;;) {
        int inside = 0;
        if (ops->prepare_candidate &&
                !ops->prepare_candidate(ops->context, result.cursor,
                    result.index))
            return result;
        result.candidate = game_memory_read_u32(player.memory, result.cursor);
        if (player.memory->fault ||
                !ops->contains(ops->context, result.candidate,
                    player.address, &inside))
            return result;
        if (inside) {
            result.table = game_memory_read_u32(player.memory, 0x00639a70u);
            if (player.memory->fault) return result;
            if (ops->select &&
                    !ops->select(ops->context, result.table, result.index))
                return result;
            result.square = game_memory_read_u32(player.memory,
                result.table + result.index * 4u);
            result.found = !player.memory->fault && result.square != 0u;
            return result;
        }
        result.cursor += 4u;
        result.index++;
        result.count = (int16_t)game_memory_read_u16(player.memory,
            0x00639a74u);
        if (player.memory->fault) return result;
        if (ops->advance_table &&
                !ops->advance_table(ops->context, result.cursor,
                    result.index, result.count))
            return result;
        if (result.count <= (int32_t)result.index) return result;
    }
}

GamePlayerOwnedSquareSearch game_player_find_owned_call_square(
        GamePlayerRef player, uint32_t owner_index,
        const GamePlayerCallSquareOps *ops) {
    GamePlayerOwnedSquareSearch result = {0};
    if (!ops || !ops->contains) return result;
    result.owner = game_memory_read_u32(player.memory,
        0x005f0210u + (owner_index & 0x3fu) * 4u);
    if (player.memory->fault) return result;
    if (ops->prepare_chain &&
            !ops->prepare_chain(ops->context, result.owner))
        return result;
    result.square = game_memory_read_u32(player.memory,
        result.owner + 0x294u);
    if (player.memory->fault) return result;
    if (ops->advance_chain &&
            !ops->advance_chain(ops->context, result.square))
        return result;
    while (result.square != 0u) {
        int inside = 0;
        if (!ops->contains(ops->context, result.square,
                player.address, &inside))
            return result;
        if (inside) {
            result.found = 1;
            return result;
        }
        result.square = game_memory_read_u32(player.memory,
            result.square + 0x64u);
        if (player.memory->fault) return result;
        if (ops->advance_chain &&
                !ops->advance_chain(ops->context, result.square))
            return result;
    }
    return result;
}

static uint64_t game_player_truncate_extended(long double value);
int game_player_build_network_delta(GameMemory *m,GameAddress queue,GamePlayerNetworkDelta *out){
    static const int16_t live[7]={-0xd8,-0xd4,-0xd0,-0xc4,-0xc6,-0xc8,0x132};
    static const int16_t snapshot[7]={-0x124,-0x120,-0x11c,-0x118,-0x116,-0x114,0x130};
    uint32_t health;int32_t signed_health;unsigned i;
    if(!m || !out)return 0;
    health=game_memory_read_u32(m,queue-0xe4u);
    memcpy(&signed_health,&health,4u);memset(out,0,sizeof(*out));
    if(signed_health<=0)return !m->fault;
    for(i=0;i<7u;i++){
        uint32_t a,b;uint16_t value;
        if(i<3u){
            float x,y;uint32_t xb=game_memory_read_u32(m,queue+live[i]),yb=game_memory_read_u32(m,queue+snapshot[i]);
            memcpy(&x,&xb,4u);memcpy(&y,&yb,4u);
            a=(uint32_t)game_player_truncate_extended(x);b=(uint32_t)game_player_truncate_extended(y);
            if(!((a^b)&0xfffffffeu))continue;
            value=(uint16_t)((i==1u)?a:((a>>1u)|(a&0x80000000u)));
        }else{
            a=game_memory_read_u16(m,queue+live[i]);b=game_memory_read_u16(m,queue+snapshot[i]);
            if(a==b)continue;
            value=(uint16_t)a;
        }
        out->values[out->count++]=value;out->mask|=(uint8_t)(1u<<i);
    }
    {
        uint16_t a=game_memory_read_u16(m,queue+0x184u),b=game_memory_read_u16(m,queue+0xeau),c=game_memory_read_u16(m,queue+0xe8u);
        int16_t sa,sb,sc;memcpy(&sa,&a,2u);memcpy(&sb,&b,2u);memcpy(&sc,&c,2u);
        if((int32_t)sa+sb!=sc){out->values[out->count++]=(uint16_t)(a+b);out->mask|=0x80u;}
    }
    if(m->fault || !out->mask)return !m->fault;
    out->route=(game_memory_read_u32(m,queue+0x1b8u)&2u)!=0u;
    out->float_words[0]=game_memory_read_u32(m,0x604114u);
    out->float_words[1]=game_memory_read_u32(m,0x6040a4u);
    for(i=0;i<2u;i++){
        uint32_t bits=game_memory_read_u32(m,queue+(i?0x1a0u:0x1a4u));float a,b;
        memcpy(&a,&bits,4u);memcpy(&b,&out->float_words[i],4u);
        if(!isnan(a) && !isnan(b) && a!=b)out->float_update=1u;
    }
    out->ready=1u;return !m->fault;
}
int game_player_emit_network_delta(GamePlayerRef player,
        const GamePlayerNetworkDeltaOps *ops) {
    static const int16_t quantized_live[3] = {-0xd8, -0xd4, -0xd0};
    static const int16_t quantized_snapshot[3] = {-0x124, -0x120, -0x11c};
    static const uint8_t quantized_bits[3] = {0x01u, 0x02u, 0x04u};
    static const uint8_t quantized_halve[3] = {1u, 0u, 1u};
    static const int16_t word_live[4] = {-0xc4, -0xc6, -0xc8, 0x132};
    static const int16_t word_snapshot[4] = {-0x118, -0x116, -0x114, 0x130};
    static const uint8_t word_bits[4] = {0x08u, 0x10u, 0x20u, 0x40u};
    unsigned index;
    if (!ops || !ops->quantized || !ops->word ||
            !ops->combined_angle || !ops->finish)
        return 0;
    for (index = 0u; index < 3u; ++index) {
        if (!ops->quantized(ops->context, index,
                player.address + quantized_live[index],
                player.address + quantized_snapshot[index],
                quantized_bits[index], quantized_halve[index]))
            return 0;
    }
    for (index = 0u; index < 4u; ++index) {
        if (!ops->word(ops->context, index,
                player.address + word_live[index],
                player.address + word_snapshot[index], word_bits[index]))
            return 0;
    }
    if (!ops->combined_angle(ops->context, player.address + 0x184u,
            player.address + 0xeau, player.address + 0xe8u))
        return 0;
    return ops->finish(ops->context, player) && !player.memory->fault;
}

static uint32_t game_player_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

GamePlayerNetworkPacketResult game_player_apply_network_packet(
        GamePlayerRef player, GameAddress packet) {
    GamePlayerNetworkPacketResult result = {0};
    unsigned bit;
    result.mask = game_memory_read_u8(player.memory, packet + 0x18u);
    if (player.memory->fault) return result;
    for (bit = 0u; bit < 8u; ++bit) {
        uint16_t packed;
        GameAddress destination;
        if ((result.mask & (1u << bit)) == 0u) continue;
        packed = game_memory_read_u16(player.memory,
            packet + 0x1au + result.field_count * 2u);
        if (player.memory->fault) return result;
        if (bit != 7u) result.field_count++;
        switch (bit) {
            case 0u:
                game_memory_write_u32(player.memory, player.address + 0x18u,
                    game_player_float_bits((float)((uint32_t)packed * 2u)));
                break;
            case 1u:
                game_memory_write_u32(player.memory, player.address + 0x1cu,
                    game_player_float_bits((float)(int16_t)packed));
                break;
            case 2u:
                game_memory_write_u32(player.memory, player.address + 0x20u,
                    game_player_float_bits((float)((uint32_t)packed * 2u)));
                break;
            case 3u: destination = player.address + 0x24u; goto write_word;
            case 4u: destination = player.address + 0x26u; goto write_word;
            case 5u: destination = player.address + 0x28u; goto write_word;
            case 6u: destination = player.address + 0x26cu; goto write_word;
            default:
                destination = player.address + 0x224u;
write_word:
                game_memory_write_u16(player.memory, destination, packed);
                break;
        }
        if (player.memory->fault) return result;
    }
    return result;
}

static float game_player_bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float game_player_read_float(GameMemory *memory, GameAddress address) {
    return game_player_bits_float(game_memory_read_u32(memory, address));
}

static float game_player_store_float(long double value) {
    volatile float stored = (float)value;
    return stored;
}

static float game_player_clamp_axis(GameMemory *memory, float coordinate,
        float extent, int unordered_uses_lower) {
    float lower = game_player_store_float((long double)extent *
        game_player_read_float(memory, 0x00574d78u));
    if ((unordered_uses_lower && (isnan(coordinate) || isnan(lower))) ||
            coordinate < lower)
        return lower;
    {
        volatile long double upper = (long double)extent *
            game_player_read_float(memory, 0x00574d7cu);
        upper += game_player_read_float(memory, 0x00574d80u);
        if (coordinate > upper) return game_player_store_float(upper);
    }
    return coordinate;
}

GamePlayerPositionFinalizePlan game_player_plan_position_finalize(
        GamePlayerRef player) {
    GamePlayerPositionFinalizePlan plan = {0};
    float extent = game_player_read_float(player.memory,
        player.address + 0x38u);
    float x = game_player_read_float(player.memory, player.address + 0x18u);
    float z;
    float y;
    uint32_t y_bits;
    float upper;
    x = game_player_clamp_axis(player.memory, x, extent, 0);
    z = game_player_read_float(player.memory, player.address + 0x20u);
    z = game_player_clamp_axis(player.memory, z, extent, 1);
    y = game_player_read_float(player.memory, player.address + 0x1cu);
    y_bits = game_player_float_bits(y);
    if (y_bits > 0xc6ea6000u) {
        y = game_player_bits_float(0xc6ea6000u);
    } else {
        upper = game_player_read_float(player.memory, 0x00574d88u);
        if (isnan(upper) || isnan(y) || upper <= y) y = upper;
    }
    if (player.memory->fault) return plan;
    plan.x_bits = game_player_float_bits(x);
    plan.y_bits = game_player_float_bits(y);
    plan.z_bits = game_player_float_bits(z);
    plan.delta_x_bits = game_player_float_bits(game_player_store_float(
        (long double)x - game_player_read_float(player.memory,
            player.address + 0x1bcu)));
    plan.delta_y_bits = game_player_float_bits(game_player_store_float(
        (long double)y - game_player_read_float(player.memory,
            player.address + 0x1c0u)));
    plan.delta_z_bits = game_player_float_bits(game_player_store_float(
        (long double)z - game_player_read_float(player.memory,
            player.address + 0x1c4u)));
    plan.angle_delta = (uint16_t)(game_memory_read_u16(player.memory,
        player.address + 0x24u) - game_memory_read_u16(player.memory,
        player.address + 0x1c8u));
    return plan;
}

int game_player_commit_position_finalize(GamePlayerRef player,
        GamePlayerPositionFinalizePlan *plan) {
    if (!plan || player.memory->fault) return 0;
    game_memory_write_u32(player.memory, player.address + 0x18u,
        plan->x_bits);
    game_memory_write_u32(player.memory, player.address + 0x1cu,
        plan->y_bits);
    game_memory_write_u32(player.memory, player.address + 0x20u,
        plan->z_bits);
    game_memory_write_u32(player.memory, player.address + 0x1acu,
        plan->delta_x_bits);
    game_memory_write_u32(player.memory, player.address + 0x1b0u,
        plan->delta_y_bits);
    game_memory_write_u32(player.memory, player.address + 0x1b4u,
        plan->delta_z_bits);
    game_memory_write_u16(player.memory, player.address + 0x1b8u,
        plan->angle_delta);
    plan->vtable = game_memory_read_u32(player.memory, player.address);
    plan->first_record = game_memory_read_u32(player.memory,
        player.address + 0xb0u);
    return !player.memory->fault;
}

static uint64_t game_player_truncate_extended(long double value) {
    if (!isfinite(value) || value >= 9223372036854775808.0L ||
            value < (long double)INT64_MIN)
        return UINT64_C(0x8000000000000000);
    return (uint64_t)(int64_t)value;
}

static uint32_t game_player_drop_cell_offset(uint32_t x, uint32_t z) {
    int32_t row = (int32_t)(x & 0xfffffe3fu) >> 6;
    return (uint32_t)row + (z & 0xfffffe00u) * 4u + 5u;
}

static GamePlayerDropEdgeResult player_drop_edge_test(GamePlayerRef player,GameAddress candidate,
        const float current[3],const float previous[3],const float footprint[8][3]) {
    GamePlayerDropEdgeResult result = {0};
    float candidate_x = previous?previous[0]:game_player_read_float(player.memory, candidate);
    float candidate_y = previous?previous[1]:game_player_read_float(player.memory, candidate + 4u);
    float candidate_z = previous?previous[2]:game_player_read_float(player.memory, candidate + 8u);
    float current_x = current?current[0]:game_player_read_float(player.memory,
        player.address + 0x64u);
    float current_y = current?current[1]:game_player_read_float(player.memory,
        player.address + 0x68u);
    float current_z = current?current[2]:game_player_read_float(player.memory,
        player.address + 0x6cu);
    unsigned index;
    if (player.memory->fault) return result;
    if ((candidate_x == current_x || isnan(candidate_x) || isnan(current_x)) &&
            (current_y == candidate_y || isnan(current_y) ||
                isnan(candidate_y)) &&
            (current_z == candidate_z || isnan(current_z) ||
                isnan(candidate_z)))
        return result;
    for (index = 0u; index < 8u; ++index) {
        GameAddress point = player.address + 0x424u + index * 12u;
        float offset_x = footprint?footprint[index][0]:game_player_read_float(player.memory, point);
        float offset_z = footprint?footprint[index][2]:game_player_read_float(player.memory, point + 8u);
        float current_point_x = game_player_store_float(
            (long double)offset_x + current_x);
        float current_point_z = game_player_store_float(
            (long double)offset_z + current_z);
        float candidate_point_x = game_player_store_float(
            (long double)candidate_x + offset_x);
        float candidate_point_z = game_player_store_float(
            (long double)offset_z + candidate_z);
        uint64_t integer_x = game_player_truncate_extended(current_point_x);
        uint64_t integer_z = game_player_truncate_extended(current_point_z);
        GameAddress table = game_memory_read_u32(player.memory, 0x00597d90u);
        uint8_t flags;
        if (player.memory->fault) return result;
        {uint32_t offset=game_player_drop_cell_offset((uint32_t)integer_x,(uint32_t)integer_z);
        flags=player.memory->terrain && offset<65536u*8u ?
            player.memory->terrain->cells[offset/8u].flags : game_memory_read_u8(player.memory,table+offset);}
        if (player.memory->fault) return result;
        result.set_registers = 1;
        result.final_ecx = table;
        result.final_edx = (uint32_t)(integer_z >> 32u);
        if ((flags & 0x04u) != 0u) {
            long double current_height = gameplay_geometry_terrain_height(
                player.memory, current_point_x, current_point_z);
            float stored_height;
            long double candidate_height;
            if (player.memory->fault) return result;
            stored_height = game_player_store_float(current_height);
            candidate_height = gameplay_geometry_terrain_height(player.memory,
                candidate_point_x, candidate_point_z);
            if (player.memory->fault) return result;
            result.final_ecx = player.address;
            result.final_edx = game_player_float_bits(candidate_point_x);
            if (!(candidate_height >= (long double)stored_height)) {
                result.blocked = 1u;
                return result;
            }
        }
    }
    return result;
}
GamePlayerDropEdgeResult player_drop_edge_values(GamePlayerRef player,
        const float current[3],const float previous[3],const float footprint[8][3]){
    return player_drop_edge_test(player,0u,current,previous,footprint);
}
GamePlayerDropEdgeResult game_player_test_drop_edge(GamePlayerRef player,GameAddress candidate){
    PresentationPose *pose=presentation_instance_pose(player.address);
    PlayerRig *rig=presentation_instance_player_rig(player.address);
    return player_drop_edge_test(player,candidate,pose?pose->local_position:NULL,NULL,
        rig?(const float (*)[3])rig->terrain_ring:NULL);
}
