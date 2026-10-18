#ifndef UPRISING_PLAYER_RUNTIME_POOL_H
#define UPRISING_PLAYER_RUNTIME_POOL_H

#include "game_memory.h"

/* Key 0x20's per-index runtime template and ready-clone list. Constructors
 * and retirement use the existing presentation, rig and child owners. */
enum { PLAYER_RUNTIME_POOL_SLOTS = 15 };
enum { PLAYER_RUNTIME_PARTS = 15 };
typedef struct PlayerRuntimeActive {
    GameAddress clone;
    GameAddress parts[PLAYER_RUNTIME_PARTS];
    GameAddress auxiliary;
    uint8_t parts_bound;
    uint8_t retiring;
    uint8_t destroy_cursor;
    uint8_t destroy_phase;
} PlayerRuntimeActive;
typedef struct PlayerRuntimePoolSlot {
    GameAddress template_object;
    GameAddress template_parts[PLAYER_RUNTIME_PARTS];
    GameAddress template_auxiliary;
    uint8_t template_parts_bound;
    uint8_t template_destroy_cursor;
    uint8_t template_destroy_phase;
    GameAddress *ready;
    uint32_t ready_count;
    uint32_t ready_capacity;
    PlayerRuntimeActive *active;
    uint32_t active_count;
    uint32_t active_capacity;
} PlayerRuntimePoolSlot;

typedef struct PlayerRuntimeDestroyServices {
    void *context;
    int (*release_child)(void *, GameAddress child, unsigned part);
    int (*clear_projection)(void *, GameAddress clone);
    int (*clear_events)(void *, GameAddress clone);
    int (*destroy_base)(void *, GameAddress clone);
} PlayerRuntimeDestroyServices;

int player_runtime_pool_bind(uint16_t index, GameAddress template_object);
int player_runtime_pool_attach(uint16_t index, GameAddress clone);
int player_runtime_pool_has_ready(uint16_t index);
GameAddress player_runtime_pool_take(uint16_t index);
/* Shutdown resumes a clone already being deleted before taking another. */
GameAddress player_runtime_pool_begin_shutdown(uint16_t index);
int player_runtime_pool_has_borrowed(uint16_t index);
int player_runtime_pool_retire(GameAddress clone);
int player_runtime_pool_bind_parts(GameMemory *memory, GameAddress clone);
int player_runtime_pool_parts(GameAddress clone,
    GameAddress output[PLAYER_RUNTIME_PARTS]);
int player_runtime_pool_auxiliary(GameAddress clone, GameAddress *output);
/* Returns zero for an unknown clone or failed callback. Successful steps are
 * remembered so a shutdown retry never repeats a completed child release. */
int player_runtime_pool_destroy_parts(GameAddress clone,
    const PlayerRuntimeDestroyServices *services);
int player_runtime_pool_slot(uint16_t index, PlayerRuntimePoolSlot *output);
void player_runtime_pool_forget(uint16_t index);

#endif
