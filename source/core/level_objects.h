#ifndef UPRISING_LEVEL_OBJECTS_H
#define UPRISING_LEVEL_OBJECTS_H

#include <stdint.h>

enum { LEVEL_OBJECTS_MAX = 256, LEVEL_OBJECT_NAME_SIZE = 64 };

typedef struct LevelObjectDefinition {
    int32_t kind, owner;
    float x, z, y, heading_degrees;
    char name[LEVEL_OBJECT_NAME_SIZE];
    char attribute[LEVEL_OBJECT_NAME_SIZE];
    char parent_name[LEVEL_OBJECT_NAME_SIZE];
    int32_t flags, group, variant;
} LevelObjectDefinition;

typedef struct LevelObjectsStats {
    uint32_t loads, unloads, generation, count, constructed, links;
} LevelObjectsStats;

/*4a98fd..4a9912 replaces stored altitude for ground objects; only26/27/b0
 *retain the level's512-unit Y coordinate. Keep the terrain sum wide. */
float level_object_start_height(uint8_t type,float level_height,float center_height,long double terrain_height);

/* Owns the complete objects section and created-object index until unload.
 * The construction callback is the remaining x86 object-factory boundary. */
int level_objects_load(const char *path, int32_t offset, void *context,
    int (*begin)(void *, uint32_t, uint32_t),
    int (*construct)(void *, uint32_t, const LevelObjectDefinition *, uint32_t *),
    int (*link)(void *, uint32_t, uint32_t),
    void (*end)(void *), uint32_t *next_offset);
int level_objects_get(uint32_t index, LevelObjectDefinition *output,
    uint32_t *object);
void level_objects_unload(void);
void level_objects_stats(LevelObjectsStats *output);

#endif
