#include "level_movement_paths.h"
#include "resource_file.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct LevelMovementPaths {
    LevelMovementPath paths[LEVEL_MOVEMENT_PATHS_MAX];
    LevelMovementPathsStats stats;
} LevelMovementPaths;
static LevelMovementPaths active;

static int next_line(ResourceFile *file, char *line, size_t capacity) {
    while (resource_file_line(file, line, capacity)) {
        const unsigned char *text = (const unsigned char *)line;
        while (isspace(*text)) ++text;
        if (*text && *text != '#') return 1;
    }
    return 0;
}

int level_movement_paths_load(const char *path, int32_t offset,
    void *context,
    int (*begin)(void *, uint32_t, const LevelMovementPath *),
    int (*construct)(void *, uint32_t, const LevelMovementPath *, int *),
    int (*publish)(void *), uint32_t *next_offset) {
    ResourceFile *file;
    char line[512];
    uint32_t count, following_section;
    unsigned i, j;
    if (!path || !begin || !construct || !publish || !next_offset ||
        offset < 0) return 0;
    file = resource_file_open(path);
    if (!file) return 0;
    if (!resource_file_seek(file, offset, SEEK_SET) ||
        !next_line(file, line, sizeof(line)) ||
        sscanf(line, "movement_paths %u", &count) != 1 ||
        count > LEVEL_MOVEMENT_PATHS_MAX) goto fail;
    memset(active.paths, 0, sizeof(active.paths));
    for (i = 0; i < count; ++i) {
        LevelMovementPath *record = &active.paths[i];
        if (!next_line(file, line, sizeof(line)) ||
            sscanf(line, "%63s %u %d", record->name,
                &record->point_count, &record->flags) != 3 ||
            record->point_count > LEVEL_MOVEMENT_POINTS_MAX) goto fail;
        for (j = 0; j < record->point_count; ++j) {
            LevelMovementPoint *point = &record->points[j];
            if (!next_line(file, line, sizeof(line)) ||
                sscanf(line, "%f %f %f", &point->x, &point->z,
                    &point->y) != 3) goto fail;
        }
    }
    if (!resource_file_tell(file, &following_section)) goto fail;
    resource_file_close(file);
    if (!begin(context, count, active.paths)) return 0;
    for (i = 0; i < count; ++i) {
        int gate_route = 0;
        if (!construct(context, i, &active.paths[i], &gate_route)) return 0;
        active.stats.constructed++;
        if (gate_route) active.stats.gate_routes++;
        else active.stats.ordinary_routes++;
    }
    if (!publish(context)) return 0;
    active.stats.loads++;
    active.stats.generation++;
    active.stats.count = count;
    *next_offset = following_section;
    return 1;
fail:
    resource_file_close(file);
    return 0;
}

int level_movement_paths_get(uint32_t index, LevelMovementPath *output) {
    if (!output || index >= active.stats.count) return 0;
    *output = active.paths[index];
    return 1;
}

void level_movement_paths_unload(void) {
    if (active.stats.count) {
        active.stats.unloads++;
        active.stats.count = 0;
    }
}

void level_movement_paths_stats(LevelMovementPathsStats *output) {
    if (output) *output = active.stats;
}
