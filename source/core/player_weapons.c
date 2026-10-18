#include "player_weapons.h"

#include <string.h>

enum {
    PLAYER_ACTION_FLAGS = 0x140u,
    PLAYER_SELECTED_LEFT = 0x1ccu,
    PLAYER_SELECTED_RIGHT = 0x1d0u,
    PLAYER_WEAPON_PROXY = 0x210u,
    PLAYER_LEFT_MODE = 0x214u,
    PLAYER_RIGHT_MODE = 0x218u,
    PLAYER_WORLD_MATRIX = 0x080u,
    PLAYER_MOUNT_MATRIX = 0x230u,
    PLAYER_ATTACHMENT_MATRIX = 0x278u,
    PLAYER_SPECIAL_SHOT_TICK = 0x2c4u,
    PLAYER_TYPE2_SCALE = 0x2e4u,
    PLAYER_STATE_FLAGS = 0x2f4u,
    WEAPON_LAST_USE_TICK = 0x08u,
    WEAPON_DESCRIPTOR = 0x14u,
    DESCRIPTOR_TYPE = 0x0cu,
    DESCRIPTOR_DURATION = 0x20u,
    FIRE_LEFT = 0x00010000u,
    FIRE_RIGHT = 0x00020000u,
    FIRE_MASK = FIRE_LEFT | FIRE_RIGHT,
    FIRE_BLOCKED = 0x08u,
    EFFECT_TICK = 0x005acbe8u,
    SIMULATION_TICK = 0x005ac6acu
};

static int32_t signed_bits(uint32_t bits) {
    int32_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int weapon_read_type(GamePlayerRef player, GameAddress weapon,
    uint8_t *type) {
    GameAddress descriptor = game_memory_read_u32(player.memory,
        weapon + WEAPON_DESCRIPTOR);
    if (player.memory->fault) return 0;
    *type = game_memory_read_u8(player.memory, descriptor + DESCRIPTOR_TYPE);
    return !player.memory->fault;
}

static int weapon_call(GamePlayerRef player,
    const GamePlayerWeaponCallbacks *callbacks,
    GameAddress weapon, GameAddress firing_matrix, uint32_t *result) {
    return callbacks && callbacks->fire && callbacks->fire(callbacks->context,
        player.memory, weapon, firing_matrix, result);
}

static int side_can_fire(GamePlayerRef player, uint32_t mode_offset) {
    uint32_t state = game_memory_read_u32(player.memory,
        player.address + PLAYER_STATE_FLAGS);
    uint32_t mode;
    if (player.memory->fault) return 0;
    if ((state & FIRE_BLOCKED) == 0u) return 1;
    mode = game_memory_read_u32(player.memory, player.address + mode_offset);
    if (player.memory->fault) return 0;
    return mode == 0u || mode == 2u;
}

static int update_type2_cooldown(GamePlayerRef player,
    GameAddress selected) {
    GameAddress proxy;
    GameAddress descriptor;
    uint8_t type;
    uint32_t tick;
    int32_t duration;
    if (!weapon_read_type(player, selected, &type)) return 0;
    if (type != 2u || game_memory_read_u32(player.memory,
            player.address + PLAYER_TYPE2_SCALE) != 0x3f800000u)
        return !player.memory->fault;
    proxy = game_memory_read_u32(player.memory,
        player.address + PLAYER_WEAPON_PROXY);
    if (player.memory->fault) return 0;
    tick = game_memory_read_u32(player.memory, SIMULATION_TICK);
    if (player.memory->fault) return 0;
    descriptor = game_memory_read_u32(player.memory,
        proxy + WEAPON_DESCRIPTOR);
    if (player.memory->fault) return 0;
    duration = signed_bits(game_memory_read_u32(player.memory,
        descriptor + DESCRIPTOR_DURATION));
    if (player.memory->fault) return 0;
    game_memory_write_u32(player.memory, proxy + WEAPON_LAST_USE_TICK,
        tick - (uint32_t)(duration >> 1));
    return !player.memory->fault;
}

static int after_weapon_result(GamePlayerRef player,
    uint32_t selected_offset, uint32_t result) {
    GameAddress selected;
    uint8_t type;
    if (result != 3u) return 1;
    selected = game_memory_read_u32(player.memory,
        player.address + selected_offset);
    if (player.memory->fault || !weapon_read_type(player, selected, &type))
        return 0;
    if (type == 6u || type == 7u || type == 8u) {
        uint32_t tick = game_memory_read_u32(player.memory, EFFECT_TICK);
        if (player.memory->fault) return 0;
        game_memory_write_u32(player.memory,
            player.address + PLAYER_SPECIAL_SHOT_TICK, tick);
        if (player.memory->fault) return 0;
    }
    return update_type2_cooldown(player, selected);
}

static int call_type2_proxy(GamePlayerRef player,
    const GamePlayerWeaponCallbacks *callbacks,
    uint32_t selected_offset, GameAddress firing_matrix) {
    GameAddress selected = game_memory_read_u32(player.memory,
        player.address + selected_offset);
    GameAddress proxy;
    uint8_t type;
    if (player.memory->fault || !weapon_read_type(player, selected, &type))
        return 0;
    if (type != 2u || game_memory_read_u32(player.memory,
            player.address + PLAYER_TYPE2_SCALE) != 0x3f800000u)
        return !player.memory->fault;
    proxy = game_memory_read_u32(player.memory,
        player.address + PLAYER_WEAPON_PROXY);
    if (player.memory->fault) return 0;
    return weapon_call(player, callbacks, proxy, firing_matrix, NULL);
}

static int process_weapon_side(GamePlayerRef player,
    const GamePlayerWeaponCallbacks *callbacks,
    uint32_t fire_bit, uint32_t mode_offset, uint32_t selected_offset,
    GameAddress firing_matrix) {
    uint32_t flags = game_memory_read_u32(player.memory,
        player.address + PLAYER_ACTION_FLAGS);
    GameAddress selected;
    uint32_t result;
    if (player.memory->fault) return 0;
    if ((flags & fire_bit) == 0u) return 1;
    if (!side_can_fire(player, mode_offset)) return !player.memory->fault;
    selected = game_memory_read_u32(player.memory,
        player.address + selected_offset);
    if (player.memory->fault || !weapon_call(player, callbacks, selected,
            firing_matrix, &result)) return 0;
    if (!after_weapon_result(player, selected_offset, result)) return 0;
    return call_type2_proxy(player, callbacks, selected_offset, firing_matrix);
}

int game_player_build_firing_transform(GamePlayerRef player,
    GameMatrixCompose compose, void *compose_context,
    GameAddress intermediate_matrix, GameAddress firing_matrix) {
    if (!compose || !compose(compose_context, intermediate_matrix,
            player.address + PLAYER_ATTACHMENT_MATRIX,
            player.address + PLAYER_MOUNT_MATRIX)) return 0;
    return compose(compose_context, firing_matrix,
        player.address + PLAYER_WORLD_MATRIX, intermediate_matrix);
}

int game_player_update_weapons(GamePlayerRef player,
    const GamePlayerWeaponCallbacks *callbacks,
    GameMatrixCompose compose, void *compose_context,
    GameAddress intermediate_matrix, GameAddress firing_matrix) {
    uint32_t flags = game_memory_read_u32(player.memory,
        player.address + PLAYER_ACTION_FLAGS);
    if (player.memory->fault) return 0;
    if ((flags & FIRE_MASK) == 0u) return 1;
    if (!game_player_build_firing_transform(player, compose, compose_context,
            intermediate_matrix, firing_matrix) ||
        !game_player_repair_empty_weapon_selections(player) ||
        !process_weapon_side(player, callbacks, FIRE_LEFT, PLAYER_LEFT_MODE,
            PLAYER_SELECTED_LEFT, firing_matrix) ||
        !process_weapon_side(player, callbacks, FIRE_RIGHT, PLAYER_RIGHT_MODE,
            PLAYER_SELECTED_RIGHT, firing_matrix)) return 0;
    return 1;
}
