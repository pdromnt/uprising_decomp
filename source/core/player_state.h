#ifndef UPRISING_NATIVE_PLAYER_STATE_H
#define UPRISING_NATIVE_PLAYER_STATE_H

#include "game_memory.h"

#include <stdint.h>

typedef struct GamePlayerRef {
    GameMemory *memory;
    GameAddress address;
} GamePlayerRef;

typedef enum GamePlayerRechargeResult {
    GAME_PLAYER_RECHARGE_NONE = 0,
    GAME_PLAYER_RECHARGE_PASSIVE = 1,
    GAME_PLAYER_RECHARGE_ZONE = 2
} GamePlayerRechargeResult;

GamePlayerRef game_player_ref(GameMemory *memory, GameAddress address);
int game_player_refill_inventory_to_max(GamePlayerRef player);
int game_player_update_recharge(GamePlayerRef player, uint32_t tick,
    GameAddress world_manager, GamePlayerRechargeResult *result);
int game_player_repair_empty_weapon_selections(GamePlayerRef player);

typedef struct GamePlayerCollisionOps {
    void *context;
    int (*query_buckets)(void *context, GameAddress player,
        uint32_t x_bits, uint32_t z_bits, GameAddress *buckets);
    int (*probe)(void *context, GameAddress candidate, GameAddress player,
        uint32_t first, uint32_t second, uint32_t *collides);
} GamePlayerCollisionOps;

int game_player_should_ignore_collider(GamePlayerRef player,
    GameAddress candidate);
int game_player_find_collision(GamePlayerRef player, uint32_t first,
    uint32_t second, GameAddress excluded, int remember_hit,
    const GamePlayerCollisionOps *ops, GameAddress *found);

typedef struct GamePlayerAimResult {
    uint32_t partial;
    uint32_t tail;
    uint32_t direction;
} GamePlayerAimResult;

GamePlayerAimResult game_player_composite_aim(GamePlayerRef player,
    uint32_t base_angle);

typedef enum GamePlayerAnimationSampleKind {
    GAME_PLAYER_SAMPLE_FLOAT = 0,
    GAME_PLAYER_SAMPLE_UNSIGNED_ANGLE = 1,
    GAME_PLAYER_SAMPLE_SIGNED_ANGLE = 2
} GamePlayerAnimationSampleKind;

typedef struct GamePlayerAnimationOps {
    void *context;
    int (*sample)(void *context, GameAddress channel,
        GameAddress destination, GamePlayerAnimationSampleKind kind);
} GamePlayerAnimationOps;

int game_player_sample_animation(GamePlayerRef player,
    const GamePlayerAnimationOps *ops);

typedef struct GamePlayerPresentationPlan {
    uint32_t owner_index;
    GameAddress owner;
    GameAddress active_world;
    uint32_t position_x;
    uint32_t position_y;
    uint32_t position_z;
    GameAddress vtable;
    GameAddress first_target;
    GameAddress second_target;
    int active;
} GamePlayerPresentationPlan;

GamePlayerPresentationPlan game_player_prepare_presentation(
    GamePlayerRef player, GameAddress source);
uint32_t game_player_finish_presentation(GamePlayerRef player);

typedef enum GamePlayerTurretDisplayMode {
    GAME_PLAYER_TURRET_DISPLAY_CHANNEL = 0,
    GAME_PLAYER_TURRET_DISPLAY_ZERO = 1,
    GAME_PLAYER_TURRET_DISPLAY_SCALED = 2
} GamePlayerTurretDisplayMode;

typedef struct GamePlayerTurretDisplayPlan {
    int16_t pitch;
    int16_t channel;
    int32_t delta;
    GamePlayerTurretDisplayMode mode;
} GamePlayerTurretDisplayPlan;

GamePlayerTurretDisplayPlan game_player_turret_display_plan(
    GamePlayerRef player);

typedef enum GamePlayerHistoryMode {
    GAME_PLAYER_HISTORY_APPEND = 0,
    GAME_PLAYER_HISTORY_REPLACE = 1
} GamePlayerHistoryMode;

typedef enum GamePlayerHistorySampleKind {
    GAME_PLAYER_HISTORY_SCALAR = 0,
    GAME_PLAYER_HISTORY_UNSIGNED_ANGLE = 1,
    GAME_PLAYER_HISTORY_SIGNED_ANGLE = 2
} GamePlayerHistorySampleKind;

typedef struct GamePlayerHistoryOps {
    void *context;
    int (*sample)(void *context, GamePlayerRef player,
        GamePlayerHistoryMode mode, unsigned index,
        GameAddress source, GameAddress channel,
        GamePlayerHistorySampleKind kind);
} GamePlayerHistoryOps;

int game_player_record_history(GamePlayerRef player,
    GamePlayerHistoryMode mode, const GamePlayerHistoryOps *ops);

typedef struct GamePlayerDeathBeginPlan {
    uint32_t tick;
    uint32_t owner_index;
    GameAddress owner;
    GameAddress local_player;
    uint32_t game_mode;
    int is_local;
} GamePlayerDeathBeginPlan;

GamePlayerDeathBeginPlan game_player_begin_death(GamePlayerRef player);

typedef enum GamePlayerDeathCameraPath {
    GAME_PLAYER_DEATH_CAMERA_CURRENT = 0,
    GAME_PLAYER_DEATH_CAMERA_SECONDARY = 1,
    GAME_PLAYER_DEATH_CAMERA_FALLBACK = 2
} GamePlayerDeathCameraPath;

typedef struct GamePlayerDeathCameraPlan {
    GameAddress primary;
    GameAddress current;
    GameAddress secondary;
    uint32_t distance;
    uint16_t angle;
    GameAddress camera_source;
    GameAddress pose_source;
    GameAddress working_player;
    GamePlayerDeathCameraPath path;
} GamePlayerDeathCameraPlan;

GamePlayerDeathCameraPlan game_player_prepare_death_camera(
    GamePlayerRef player);
GameAddress game_player_death_camera_controller(GameMemory *memory);

typedef struct GamePlayerSlotOps {
    void *context;
    int (*configure)(void *context, GameAddress player, uint32_t argument);
    int (*unlink)(void *context, GameAddress player);
    int (*refresh)(void *context, GameAddress player, uint32_t argument);
    int (*visit)(void *context, unsigned index, GameAddress slot,
        GameAddress vtable, GameAddress target, uint32_t argument);
    int (*advance)(void *context, unsigned index);
    int (*select)(void *context, GameAddress player, uint32_t argument);
} GamePlayerSlotOps;

typedef struct GamePlayerSlotSnapshot {
    uint32_t first_index;
    uint32_t second_index;
    GameAddress first_slot;
    GameAddress second_slot;
    uint32_t position_x;
    uint32_t position_y;
    uint32_t position_z;
    uint16_t angle;
} GamePlayerSlotSnapshot;

int game_player_configure_slots(GamePlayerRef player, uint32_t argument,
    const GamePlayerSlotOps *ops);
int game_player_refresh_slots(GamePlayerRef player, uint32_t argument,
    const GamePlayerSlotOps *ops, GamePlayerSlotSnapshot *snapshot);

typedef struct GamePlayerCallSquareOps {
    void *context;
    int (*compare_count)(void *context, int32_t count);
    int (*prepare_candidate)(void *context, GameAddress cursor,
        uint32_t index);
    int (*contains)(void *context, GameAddress square, GameAddress player,
        int *inside);
    int (*select)(void *context, GameAddress table, uint32_t index);
    int (*advance_table)(void *context, GameAddress cursor,
        uint32_t index, int32_t count);
    int (*prepare_chain)(void *context, GameAddress owner);
    int (*advance_chain)(void *context, GameAddress square);
} GamePlayerCallSquareOps;

typedef struct GamePlayerCallSquareSearch {
    GameAddress square;
    GameAddress table;
    GameAddress cursor;
    GameAddress candidate;
    uint32_t index;
    int32_t count;
    int found;
} GamePlayerCallSquareSearch;

GamePlayerCallSquareSearch game_player_find_call_square(
    GamePlayerRef player, const GamePlayerCallSquareOps *ops);

typedef struct GamePlayerOwnedSquareSearch {
    GameAddress owner;
    GameAddress square;
    int found;
} GamePlayerOwnedSquareSearch;

GamePlayerOwnedSquareSearch game_player_find_owned_call_square(
    GamePlayerRef player, uint32_t owner_index,
    const GamePlayerCallSquareOps *ops);

typedef struct GamePlayerNetworkDeltaOps {
    void *context;
    int (*quantized)(void *context, unsigned index,
        GameAddress live, GameAddress snapshot, uint8_t mask_bit,
        int halve);
    int (*word)(void *context, unsigned index, GameAddress live,
        GameAddress snapshot, uint8_t mask_bit);
    int (*combined_angle)(void *context, GameAddress first,
        GameAddress second, GameAddress snapshot);
    int (*finish)(void *context, GamePlayerRef player);
} GamePlayerNetworkDeltaOps;

int game_player_emit_network_delta(GamePlayerRef player,
    const GamePlayerNetworkDeltaOps *ops);
typedef struct GamePlayerNetworkDelta {
    uint16_t values[8];
    uint32_t float_words[2];
    uint8_t mask,count,route,ready,float_update;
} GamePlayerNetworkDelta;
int game_player_build_network_delta(GameMemory *,GameAddress queue,GamePlayerNetworkDelta *);

typedef struct GamePlayerNetworkPacketResult {
    uint32_t field_count;
    uint8_t mask;
} GamePlayerNetworkPacketResult;

GamePlayerNetworkPacketResult game_player_apply_network_packet(
    GamePlayerRef player, GameAddress packet);

typedef struct GamePlayerPositionFinalizePlan {
    uint32_t x_bits;
    uint32_t y_bits;
    uint32_t z_bits;
    uint32_t delta_x_bits;
    uint32_t delta_y_bits;
    uint32_t delta_z_bits;
    uint16_t angle_delta;
    GameAddress vtable;
    GameAddress first_record;
} GamePlayerPositionFinalizePlan;

GamePlayerPositionFinalizePlan game_player_plan_position_finalize(
    GamePlayerRef player);
int game_player_commit_position_finalize(GamePlayerRef player,
    GamePlayerPositionFinalizePlan *plan);

typedef struct GamePlayerDropEdgeResult {
    uint32_t blocked;
    uint32_t final_ecx;
    uint32_t final_edx;
    int set_registers;
} GamePlayerDropEdgeResult;

GamePlayerDropEdgeResult game_player_test_drop_edge(GamePlayerRef player,
    GameAddress candidate);
GamePlayerDropEdgeResult player_drop_edge_values(GamePlayerRef,
    const float current[3],const float candidate[3],const float footprint[8][3]);

#endif
