#ifndef UPRISING_LEVEL_CITADELS_H
#define UPRISING_LEVEL_CITADELS_H

#include <stdint.h>
#include "game_memory.h"

enum {
    LEVEL_CITADELS_MAX = 16,
    LEVEL_CITADEL_FOOTPRINTS_MAX = 16,
    LEVEL_CITADEL_UPGRADES_MAX = 8
};

typedef struct LevelCitadelRect {
    float min_x, min_z, max_x, max_z;
} LevelCitadelRect;

typedef struct LevelCitadelPlacement {
    float x, z, heading_degrees;
    int32_t kind;
} LevelCitadelPlacement;

typedef struct LevelCitadelDefinition {
    int32_t owner, health;
    uint32_t footprint_count, upgrade_count;
    LevelCitadelRect footprints[LEVEL_CITADEL_FOOTPRINTS_MAX];
    LevelCitadelRect claim;
    LevelCitadelPlacement base;
    LevelCitadelPlacement upgrades[LEVEL_CITADEL_UPGRADES_MAX];
} LevelCitadelDefinition;

typedef struct LevelCitadelsStats {
    uint32_t loads, unloads, generation, count, constructed;
    uint32_t runtime_constructed, runtime_retired, runtime_live;
} LevelCitadelsStats;

typedef int (*LevelCitadelHeight)(void *, float x, float z, float *height);
/* The native citadel owner retains runtime geometry; x86 entity fields are a
 * projection for player/AI consumers still awaiting migration. */
int level_citadels_construct_runtime(GameMemory *, uint32_t index,
    uint32_t object, const LevelCitadelDefinition *, void *height_context,
    LevelCitadelHeight height);
void level_citadels_retire_runtime(uint32_t object);
int level_citadels_owns_runtime(uint32_t object);

/* Parses the whole section before constructing any object. The native owner
 * keeps the definitions until level unload. The factory owns live objects. */
int level_citadels_load(const char *path, int32_t offset, void *context,
    int (*begin)(void *, uint32_t),
    int (*construct)(void *, uint32_t, const LevelCitadelDefinition *),
    uint32_t *next_offset);
int level_citadels_get(uint32_t index, LevelCitadelDefinition *output);
void level_citadels_unload(void);
void level_citadels_stats(LevelCitadelsStats *output);

#endif
