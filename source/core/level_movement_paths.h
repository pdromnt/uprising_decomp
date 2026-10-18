#ifndef UPRISING_LEVEL_MOVEMENT_PATHS_H
#define UPRISING_LEVEL_MOVEMENT_PATHS_H

#include <stdint.h>

enum { LEVEL_MOVEMENT_PATHS_MAX = 64, LEVEL_MOVEMENT_POINTS_MAX = 16,
       LEVEL_MOVEMENT_NAME_SIZE = 64 };

typedef struct LevelMovementPoint {
    float x, z, y;
} LevelMovementPoint;

typedef struct LevelMovementPath {
    char name[LEVEL_MOVEMENT_NAME_SIZE];
    uint32_t point_count;
    int32_t flags;
    LevelMovementPoint points[LEVEL_MOVEMENT_POINTS_MAX];
} LevelMovementPath;

typedef struct LevelMovementPathsStats {
    uint32_t loads, unloads, generation, count, constructed;
    uint32_t gate_routes, ordinary_routes;
} LevelMovementPathsStats;

int level_movement_paths_load(const char *path, int32_t offset,
    void *context,
    int (*begin)(void *, uint32_t, const LevelMovementPath *),
    int (*construct)(void *, uint32_t, const LevelMovementPath *, int *),
    int (*publish)(void *), uint32_t *next_offset);
int level_movement_paths_get(uint32_t index, LevelMovementPath *output);
void level_movement_paths_unload(void);
void level_movement_paths_stats(LevelMovementPathsStats *output);

#endif
