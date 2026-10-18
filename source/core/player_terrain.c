#include "terrain_asset.h"
#include "player_terrain.h"

#include <string.h>

enum {
    PLAYER_POSITION_X = 0x018u,
    PLAYER_POSITION_Y = 0x01cu,
    PLAYER_POSITION_Z = 0x020u,
    PLAYER_HEALTH = 0x058u,
    PLAYER_SAFE_X = 0x2fcu,
    PLAYER_SAFE_Y = 0x300u,
    PLAYER_SAFE_Z = 0x304u,
    PLAYER_HEIGHT_CELLS = 0x00597d90u,
    PLAYER_SIMULATION_TICK = 0x005ac6acu
};

static float read_f32(GamePlayerRef player, uint32_t offset) {
    uint32_t bits = game_memory_read_u32(player.memory,
        player.address + offset);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t arithmetic_shift_six(uint32_t value) {
    uint32_t shifted = value >> 6u;
    if ((value & 0x80000000u) != 0u) shifted |= 0xfc000000u;
    return shifted;
}

int game_player_update_terrain_hazard(GamePlayerRef player,
    GameAddress scratch_point, const GamePlayerTerrainOps *ops,
    GamePlayerTerrainResult *result) {
    uint32_t health;
    uint32_t integer_z;
    uint32_t integer_x;
    uint32_t row;
    uint32_t column;
    GameAddress height_cells;
    uint8_t material;
    float world_x;
    float world_z;
    long double terrain_height;
    uint32_t damage_tick;
    int below;
    GamePlayerTerrainResult phase_result = GAME_PLAYER_TERRAIN_CONTINUE;
    if (result) *result = GAME_PLAYER_TERRAIN_CONTINUE;
    if (!ops || !ops->truncate_f32 || !ops->terrain_height ||
        !ops->clearance_below_limit || !ops->apply_damage ||
        !ops->emit_effect) return 0;
    health = game_memory_read_u32(player.memory,
        player.address + PLAYER_HEALTH);
    if (player.memory->fault) return 0;
    if (ops->observe_health) ops->observe_health(ops->context, health);
    if (health == 0u) {
        if (result) *result = GAME_PLAYER_TERRAIN_HEALTH_ZERO;
        return 1;
    }
    if (!ops->truncate_f32(ops->context,
            player.address + PLAYER_POSITION_Z, &integer_z) ||
        !ops->truncate_f32(ops->context,
            player.address + PLAYER_POSITION_X, &integer_x)) return 0;
    row = (integer_z & 0xfffffe00u) * 4u;
    column = arithmetic_shift_six(integer_x & 0xfffffe3fu);
    height_cells = game_memory_read_u32(player.memory, PLAYER_HEIGHT_CELLS);
    if (player.memory->fault) return 0;
    material = player.memory->terrain && row+column<65536u*8u ?
        player.memory->terrain->cells[(row+column)/8u].flags :
        game_memory_read_u8(player.memory,height_cells+row+column+5u);
    if (player.memory->fault) return 0;
    if ((material & 0x40u) == 0u) {
        uint32_t x = game_memory_read_u32(player.memory,
            player.address + PLAYER_POSITION_X);
        uint32_t y = game_memory_read_u32(player.memory,
            player.address + PLAYER_POSITION_Y);
        uint32_t z = game_memory_read_u32(player.memory,
            player.address + PLAYER_POSITION_Z);
        if (player.memory->fault) return 0;
        game_memory_write_u32(player.memory, player.address + PLAYER_SAFE_X, x);
        if (player.memory->fault) return 0;
        game_memory_write_u32(player.memory, player.address + PLAYER_SAFE_Y, y);
        if (player.memory->fault) return 0;
        game_memory_write_u32(player.memory, player.address + PLAYER_SAFE_Z, z);
        return !player.memory->fault;
    }
    world_z = read_f32(player, PLAYER_POSITION_Z);
    world_x = read_f32(player, PLAYER_POSITION_X);
    if (player.memory->fault || !ops->terrain_height(ops->context, world_x,
            world_z, &terrain_height) ||
        !ops->clearance_below_limit(ops->context, player, terrain_height,
            &below)) return 0;
    if (!below) return 1;
    damage_tick = game_memory_read_u32(player.memory, PLAYER_SIMULATION_TICK);
    if (player.memory->fault) return 0;
    if ((damage_tick & 15u) == 0u &&
        !ops->apply_damage(ops->context, player, &phase_result)) return 0;
    if (phase_result == GAME_PLAYER_TERRAIN_PLAYER_DIED) {
        if (result) *result = phase_result;
        return 1;
    }
    return ops->emit_effect(ops->context, player, scratch_point);
}
