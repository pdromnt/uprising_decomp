#include "player_frame.h"

#include "event_queue.h"
#include <stdio.h>

enum {
    PLAYER_POSITION_X = 0x018u,
    PLAYER_POSITION_Y = 0x01cu,
    PLAYER_POSITION_Z = 0x020u,
    PLAYER_YAW = 0x024u,
    PLAYER_HEALTH = 0x058u,
    PLAYER_EVENT_QUEUE = 0x13cu,
    PLAYER_PREVIOUS_X = 0x1bcu,
    PLAYER_PREVIOUS_Y = 0x1c0u,
    PLAYER_PREVIOUS_Z = 0x1c4u,
    PLAYER_PREVIOUS_YAW = 0x1c8u,
    PLAYER_DEFAULT_PRIMARY = 0x1d4u,
    PLAYER_WEAPON_PROXY = 0x210u,
    PLAYER_NETWORK_TICK = 0x2f0u,
    PLAYER_DEATH_TICK = 0x498u,
    WEAPON_COUNT = 0x0cu,
    PLAYER_WORLD_MANAGER = 0x0058973cu,
    PLAYER_HEALTH_CHEAT = 0x00594d38u,
    PLAYER_AMMO_CHEAT = 0x00594d40u,
    PLAYER_SIMULATION_TICK = 0x005ac6acu,
    PLAYER_NETWORK_RECONCILE = 0x005eff04u
};

static int snapshot_pose(GamePlayerRef player) {
    uint32_t x = game_memory_read_u32(player.memory,
        player.address + PLAYER_POSITION_X);
    uint32_t y = game_memory_read_u32(player.memory,
        player.address + PLAYER_POSITION_Y);
    uint32_t z = game_memory_read_u32(player.memory,
        player.address + PLAYER_POSITION_Z);
    uint16_t yaw;
    if (player.memory->fault) return 0;
    yaw = game_memory_read_u16(player.memory, player.address + PLAYER_YAW);
    if (player.memory->fault) return 0;
    game_memory_write_u32(player.memory, player.address + PLAYER_PREVIOUS_X, x);
    game_memory_write_u32(player.memory, player.address + PLAYER_PREVIOUS_Y, y);
    game_memory_write_u32(player.memory, player.address + PLAYER_PREVIOUS_Z, z);
    game_memory_write_u16(player.memory, player.address + PLAYER_PREVIOUS_YAW,
        yaw);
    return !player.memory->fault;
}

static int apply_cheats_and_recharge(GamePlayerRef player,
    const GamePlayerFrameCallbacks *callbacks) {
    uint32_t enabled = game_memory_read_u32(player.memory,
        PLAYER_HEALTH_CHEAT);
    uint32_t tick;
    GameAddress world = 0u;
    if (player.memory->fault) return 0;
    if (enabled != 0u) {
        uint32_t health;
        if (!callbacks || !callbacks->health ||
                !callbacks->health(callbacks->context, player.memory,
                    player.address, &health)) return 0;
        game_memory_write_u32(player.memory, player.address + PLAYER_HEALTH,
            health);
        if (player.memory->fault) return 0;
    }
    enabled = game_memory_read_u32(player.memory, PLAYER_AMMO_CHEAT);
    if (player.memory->fault) return 0;
    if (enabled != 0u && !game_player_refill_inventory_to_max(player))
        return 0;
    tick = game_memory_read_u32(player.memory, PLAYER_SIMULATION_TICK);
    if (player.memory->fault) return 0;
    if ((tick & 3u) == 0u) {
        world = game_memory_read_u32(player.memory, PLAYER_WORLD_MANAGER);
        if (player.memory->fault) return 0;
    }
    return game_player_update_recharge(player, tick, world, NULL);
}

static int mirror_primary_and_expire_queue(GamePlayerRef player,
    const GamePlayerFrameCallbacks *callbacks) {
    GameAddress primary = game_memory_read_u32(player.memory,
        player.address + PLAYER_DEFAULT_PRIMARY);
    GameAddress proxy = game_memory_read_u32(player.memory,
        player.address + PLAYER_WEAPON_PROXY);
    uint32_t count;
    if (player.memory->fault) return 0;
    count = game_memory_read_u32(player.memory, primary + WEAPON_COUNT);
    if (player.memory->fault) return 0;
    game_memory_write_u32(player.memory, proxy + WEAPON_COUNT, count);
    if (player.memory->fault) return 0;
    return game_timed_event_queue_expire(player.memory,
        callbacks ? &callbacks->events : NULL,
        player.address + PLAYER_EVENT_QUEUE);
}

static int reconcile_network_pose(GamePlayerRef player,
    const GamePlayerFrameCallbacks *callbacks) {
    uint32_t enabled = game_memory_read_u32(player.memory,
        PLAYER_NETWORK_RECONCILE);
    uint32_t tick;
    uint32_t last_tick;
    uint32_t result;
    if (player.memory->fault || enabled == 0u) return !player.memory->fault;
    tick = game_memory_read_u32(player.memory, PLAYER_SIMULATION_TICK);
    last_tick = game_memory_read_u32(player.memory,
        player.address + PLAYER_NETWORK_TICK);
    if (player.memory->fault || tick == last_tick) return !player.memory->fault;
    if (!callbacks || !callbacks->spatial_unlink ||
            !callbacks->sample_animation || !callbacks->process_collisions ||
            !callbacks->spatial_unlink(callbacks->context, player.address) ||
            !callbacks->sample_animation(callbacks->context, player.address) ||
            !callbacks->process_collisions(callbacks->context,
                player.address, &result)) return 0;
    if (result != 0u && (!callbacks->spatial_insert ||
            !callbacks->spatial_insert(callbacks->context,
                player.address))) return 0;
    return 1;
}

GamePlayerFrameGate game_player_prepare_frame(GamePlayerRef player,
    const GamePlayerFrameCallbacks *callbacks) {
    uint32_t health;
    uint32_t death_tick;
    if (!snapshot_pose(player)){fprintf(stderr,"native player prepare failed: snapshot\n");return GAME_PLAYER_FRAME_FAULT;}
    if (!apply_cheats_and_recharge(player, callbacks)){fprintf(stderr,"native player prepare failed: recharge\n");return GAME_PLAYER_FRAME_FAULT;}
    if (!mirror_primary_and_expire_queue(player, callbacks)){fprintf(stderr,"native player prepare failed: events\n");return GAME_PLAYER_FRAME_FAULT;}
    health = game_memory_read_u32(player.memory,
        player.address + PLAYER_HEALTH);
    if (player.memory->fault) return GAME_PLAYER_FRAME_FAULT;
    if (health == 0u) return GAME_PLAYER_FRAME_STOP;
    death_tick = game_memory_read_u32(player.memory,
        player.address + PLAYER_DEATH_TICK);
    if (player.memory->fault) return GAME_PLAYER_FRAME_FAULT;
    if (death_tick != 0xffffffffu) return GAME_PLAYER_FRAME_STOP;
    if (!reconcile_network_pose(player, callbacks)){
        fprintf(stderr,"native player prepare failed: network pose\n");return GAME_PLAYER_FRAME_FAULT;
    }
    health = game_memory_read_u32(player.memory,
        player.address + PLAYER_HEALTH);
    if (player.memory->fault) return GAME_PLAYER_FRAME_FAULT;
    return health == 0u ? GAME_PLAYER_FRAME_STOP :
        GAME_PLAYER_FRAME_CONTINUE;
}
