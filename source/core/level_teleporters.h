#ifndef UPRISING_LEVEL_TELEPORTERS_H
#define UPRISING_LEVEL_TELEPORTERS_H

#include <stdint.h>

enum { LEVEL_TELEPORTERS_MAX = 16, LEVEL_TELEPORTER_NAME_SIZE = 64 };

typedef struct LevelTeleporterPoint {
    float x, z, y;
} LevelTeleporterPoint;

typedef struct LevelTeleporterDefinition {
    char name[LEVEL_TELEPORTER_NAME_SIZE];
    float heading_degrees;
    LevelTeleporterPoint source, destination;
} LevelTeleporterDefinition;

typedef struct LevelTeleportersStats {
    uint32_t loads, unloads, generation, count, constructed;
    uint32_t named, paired_entities, update_visits, visual_frames;
    uint32_t destroy_passes, destroyed_entities;
} LevelTeleportersStats;

typedef struct LevelTeleporterVisual {
    float position[3];
    float lift;
    float phase;
    uint32_t spin;
    int32_t phase_ticks;
    uint32_t rising;
} LevelTeleporterVisual;

int level_teleporters_load(const char *path, int32_t offset, void *context,
    int (*construct)(void *, uint32_t, const LevelTeleporterDefinition *,
        uint32_t *, uint32_t *),
    uint32_t *next_offset);
int level_teleporters_get(uint32_t index, LevelTeleporterDefinition *output,
    uint32_t *source_object, uint32_t *destination_object);
void level_teleporters_unload(void);
void level_teleporters_stats(LevelTeleportersStats *output);
/* The level owns completed pairs through scene teardown. Destruction is
 * retryable: each successfully deleted object is removed from the owner. */
int level_teleporters_destroy_owned(void *context,
    int (*destroy)(void *, uint32_t));
/* The destination owns its animation clock; presentation only consumes frames. */
int level_teleporters_bind_visual(uint32_t index, uint32_t destination,
    const float position[3], float lift);
int level_teleporters_visual_step(uint32_t destination,
    LevelTeleporterVisual *frame);
int level_teleporters_begin_visual_update(uint32_t destination);

#endif
