#include "player_death.h"

enum {
    PLAYER_DEATH_TICK = 0x498u,
    PLAYER_SIMULATION_TICK = 0x005ac6acu,
    PLAYER_DEATH_EFFECT_AGE = 93u,
    PLAYER_DEATH_FINALIZE_AGE = 111u
};

int game_player_update_delayed_death(GamePlayerRef player,
    GameAddress guest_stack_top, const GamePlayerDeathOps *ops,
    GamePlayerDeathResult *result) {
    uint32_t death_tick;
    uint32_t tick;
    uint32_t age;
    if (result) *result = GAME_PLAYER_DEATH_CONTINUE;
    death_tick = game_memory_read_u32(player.memory,
        player.address + PLAYER_DEATH_TICK);
    if (player.memory->fault) return 0;
    if (death_tick == 0xffffffffu) return 1;
    tick = game_memory_read_u32(player.memory, PLAYER_SIMULATION_TICK);
    if (player.memory->fault) return 0;
    age = death_tick - tick;
    if (age == (uint32_t)-(int32_t)PLAYER_DEATH_EFFECT_AGE) {
        if (!ops || !ops->make_effect || !ops->play_sound ||
            !ops->make_effect(ops->context, player, guest_stack_top) ||
            !ops->play_sound(ops->context, player, guest_stack_top)) return 0;
        return 1;
    }
    if (age == (uint32_t)-(int32_t)PLAYER_DEATH_FINALIZE_AGE) {
        if (!ops || !ops->finalize ||
            !ops->finalize(ops->context, player.address)) return 0;
        if (result) *result = GAME_PLAYER_DEATH_FINALIZED;
    }
    return 1;
}
