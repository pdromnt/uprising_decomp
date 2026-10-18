#include "player_update.h"

int game_player_update_phases(GamePlayerRef player, uint32_t initial_health,
    GameAddress low_frame, const GamePlayerUpdateOps *ops) {
    GamePlayerDeathResult death_result = GAME_PLAYER_DEATH_CONTINUE;
    GamePlayerFrameGate frame_gate;
    GamePlayerTerrainResult terrain_result = GAME_PLAYER_TERRAIN_CONTINUE;
    if (!ops || !ops->frame || !ops->weapons || !ops->death || !ops->terrain ||
        !ops->matrix_compose) return 0;
    if (initial_health == 0u) return 1;
    if (!game_player_update_delayed_death(player, low_frame, ops->death,
            &death_result)) return 0;
    if (death_result == GAME_PLAYER_DEATH_FINALIZED) return 1;
    if (ops->before_frame)
        ops->before_frame(ops->context, player.address);
    frame_gate = game_player_prepare_frame(player, ops->frame);
    if (frame_gate == GAME_PLAYER_FRAME_FAULT) return 0;
    if (frame_gate == GAME_PLAYER_FRAME_STOP) return 1;
    if (!game_player_update_terrain_hazard(player, low_frame + 0x10u,
            ops->terrain, &terrain_result)) return 0;
    if (terrain_result != GAME_PLAYER_TERRAIN_CONTINUE) return 1;
    return game_player_update_weapons(player, ops->weapons,
        ops->matrix_compose, ops->matrix_context, low_frame + 0x40u,
        low_frame + 0x10u);
}
