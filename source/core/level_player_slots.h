#ifndef UPRISING_LEVEL_PLAYER_SLOTS_H
#define UPRISING_LEVEL_PLAYER_SLOTS_H

#include <stdint.h>
#include <stddef.h>
#include "game_memory.h"

typedef struct LevelPlayerSlot {
    float x, z, heading_degrees;
} LevelPlayerSlot;

typedef enum LevelPlayerClass {
    LEVEL_PLAYER_EXTERNAL = 0,
    LEVEL_PLAYER_BASE,
    LEVEL_PLAYER_SCENE,
    LEVEL_PLAYER_FACTION,
    LEVEL_PLAYER_IMPERIAL
} LevelPlayerClass;

typedef struct LevelPlayerSlotsStats {
    uint32_t loads, unloads, generation, count, placements, scenes_created;
    uint32_t sequences, scene_insertions, scene_removals, live_members;
    uint32_t discarded_members;
    uint32_t destroy_passes, owner_destroyed, update_visits;
    uint32_t player_scenes_destroyed;
} LevelPlayerSlotsStats;

/* The slot owner decides which player receives a scene and owns each new
 * scene from factory return through rollback/unload. These operations are the
 * remaining external object and player-method boundary. */
typedef struct LevelPlayerSceneOps {
    int (*publish_anchor)(void *, float x, float z);
    int (*publish_spawn)(void *, uint32_t slot, float x, float z,
        uint16_t heading);
    int (*assignment)(void *, uint32_t slot, uint32_t *assignment);
    int (*subtype)(void *, uint32_t player, uint32_t *subtype);
    int (*make_scene)(void *, uint32_t subtype, uint32_t *scene);
    int (*publish_scene)(void *, uint32_t player, uint32_t scene);
    int (*attach_scene)(void *, uint32_t player, uint32_t scene);
    int (*place_scene)(void *, uint32_t player, uint32_t scene,
        uint16_t heading, uint32_t sequence, uint32_t next_sequence,
        int first);
} LevelPlayerSceneOps;

/* The section owner keeps placements for the active level. The callback
 * constructs the connected player scene through the current platform bridge. */
int level_player_slots_load(const char *path, int32_t offset, void *context,
    int (*place)(void *, uint32_t, uint32_t, const LevelPlayerSlot *),
    uint32_t *next_offset);
int level_player_slots_get(uint32_t index, LevelPlayerSlot *output);
int level_player_slots_place_owned(GameMemory *,uint32_t index, uint32_t count,
    const LevelPlayerSlot *slot, void *context,
    const LevelPlayerSceneOps *ops);
void level_player_slots_note_scene_created(void);
/* Tracks scene handles assigned while loading, including partial constructors.
 * Player/scene handles stay opaque until the shared factory is reconstructed. */
int level_player_slots_begin_scenes(GameMemory *,const uint32_t players[6],
    const uint32_t previous_scenes[6], const uint16_t owners[6],
    const uint32_t sequences[6]);
int level_player_slots_bind_class(uint32_t player, LevelPlayerClass player_class);
LevelPlayerClass level_player_slots_class(uint32_t player);
int level_player_slots_next_owner_sequence(GameMemory *,uint32_t player,
    uint32_t *packed, uint32_t *next_sequence);
int level_player_slots_owns_player(uint32_t player);
int level_player_slots_attach_scene_member(uint32_t player, uint32_t object,
    uint32_t head_offset);
int level_player_slots_forget_scene_member(uint32_t object);
int level_player_slots_snapshot_scene_members(uint32_t player,
    uint32_t head_offset, const uint32_t **objects, size_t *count);
int level_player_slots_has_scene_member(uint32_t player, uint32_t object);
void level_player_slots_note_scene_update(void);
int level_player_slots_destroy_scene_members(void *context,
    int (*destroy)(void *, uint32_t));
int level_player_slots_assign_scene(uint32_t player, uint32_t scene);
int level_player_slots_destroy_new_scenes(void *context,
    int (*destroy)(void *, uint32_t));
/* Normal unload snapshots the active players once and retains its cursor if
 * an external destructor fails. Successful player destructors are not called
 * again on a resumed unload. */
int level_player_slots_destroy_active_scenes(void *context,
    int (*active)(void *, uint32_t, int *),
    int (*destroy)(void *, uint32_t), uint32_t players[6]);
int level_player_slots_scene_player(unsigned index, uint32_t *player);
void level_player_slots_unload(void);
void level_player_slots_stats(LevelPlayerSlotsStats *output);

#endif
