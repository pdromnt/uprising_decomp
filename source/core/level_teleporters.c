#include "level_teleporters.h"
#include "resource_file.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct LevelTeleporters {
    LevelTeleporterDefinition definitions[LEVEL_TELEPORTERS_MAX];
    uint32_t sources[LEVEL_TELEPORTERS_MAX];
    uint32_t destinations[LEVEL_TELEPORTERS_MAX];
    struct {
        float position[3];
        float lift;
        int32_t phase;
        uint32_t spin;
        uint8_t rising;
    } visual[LEVEL_TELEPORTERS_MAX];
    LevelTeleportersStats stats;
} LevelTeleporters;
static LevelTeleporters active;

static int next_line(ResourceFile *file, char *line, size_t capacity) {
    while (resource_file_line(file, line, capacity)) {
        const unsigned char *text = (const unsigned char *)line;
        while (isspace(*text)) ++text;
        if (*text && *text != '#') return 1;
    }
    return 0;
}

int level_teleporters_load(const char *path, int32_t offset, void *context,
    int (*construct)(void *, uint32_t, const LevelTeleporterDefinition *,
        uint32_t *, uint32_t *),
    uint32_t *next_offset) {
    ResourceFile *file;
    char line[512];
    uint32_t count, following_section;
    unsigned i;
    if (!path || !construct || !next_offset || offset < 0) return 0;
    file = resource_file_open(path);
    if (!file) return 0;
    if (!resource_file_seek(file, offset, SEEK_SET) ||
        !next_line(file, line, sizeof(line)) ||
        sscanf(line, "teleporters %u", &count) != 1 ||
        count > LEVEL_TELEPORTERS_MAX) goto fail;
    memset(active.definitions, 0, sizeof(active.definitions));
    memset(active.sources, 0, sizeof(active.sources));
    memset(active.destinations, 0, sizeof(active.destinations));
    memset(active.visual, 0, sizeof(active.visual));
    for (i = 0; i < count; ++i) {
        LevelTeleporterDefinition *record = &active.definitions[i];
        if (!next_line(file, line, sizeof(line)) ||
            sscanf(line, "%63s %f %f %f %f %f %f %f", record->name,
                &record->heading_degrees, &record->source.x,
                &record->source.z, &record->source.y,
                &record->destination.x, &record->destination.z,
                &record->destination.y) != 8) goto fail;
    }
    if (!resource_file_tell(file, &following_section)) goto fail;
    resource_file_close(file);
    for (i = 0; i < count; ++i) {
        if (!construct(context, i, &active.definitions[i],
                &active.sources[i], &active.destinations[i])) return 0;
        active.stats.constructed++;
        if (active.sources[i] && active.destinations[i])
            active.stats.paired_entities++;
        if (strcmp(active.definitions[i].name, "NULL")) active.stats.named++;
    }
    active.stats.loads++;
    active.stats.generation++;
    active.stats.count = count;
    *next_offset = following_section;
    return 1;
fail:
    resource_file_close(file);
    return 0;
}

int level_teleporters_get(uint32_t index, LevelTeleporterDefinition *output,
    uint32_t *source_object, uint32_t *destination_object) {
    if (!output || !source_object || !destination_object ||
        index >= active.stats.count) return 0;
    *output = active.definitions[index];
    *source_object = active.sources[index];
    *destination_object = active.destinations[index];
    return 1;
}

void level_teleporters_unload(void) {
    if (active.stats.count) {
        active.stats.unloads++;
        active.stats.count = 0;
        memset(active.sources, 0, sizeof(active.sources));
        memset(active.destinations, 0, sizeof(active.destinations));
        memset(active.visual, 0, sizeof(active.visual));
    }
}

int level_teleporters_destroy_owned(void *context,
    int (*destroy)(void *, uint32_t)) {
    uint32_t i;
    if (!destroy) return 0;
    active.stats.destroy_passes++;
    for (i = LEVEL_TELEPORTERS_MAX; i > 0; --i) {
        uint32_t index = i - 1u;
        if (active.destinations[index]) {
            if (!destroy(context, active.destinations[index])) return 0;
            active.destinations[index] = 0;
            memset(&active.visual[index], 0, sizeof(active.visual[index]));
            active.stats.destroyed_entities++;
        }
        if (active.sources[index]) {
            if (!destroy(context, active.sources[index])) return 0;
            active.sources[index] = 0;
            active.stats.destroyed_entities++;
        }
    }
    return 1;
}

int level_teleporters_bind_visual(uint32_t index, uint32_t destination,
    const float position[3], float lift) {
    if (!destination || !position || index >= LEVEL_TELEPORTERS_MAX ||
        active.destinations[index] != destination) return 0;
    memcpy(active.visual[index].position, position,
        sizeof(active.visual[index].position));
    active.visual[index].lift = lift;
    active.visual[index].phase = 0;
    active.visual[index].spin = 0;
    active.visual[index].rising = 0;
    return 1;
}

int level_teleporters_visual_step(uint32_t destination,
    LevelTeleporterVisual *frame) {
    uint32_t i;
    if (!destination || !frame) return 0;
    for (i = 0; i < active.stats.count; ++i) {
        if (active.destinations[i] == destination) {
            if (active.visual[i].rising) {
                active.visual[i].phase += 2;
                if (active.visual[i].phase >= 40) {
                    active.visual[i].phase = 40;
                    active.visual[i].rising = 0;
                }
            } else {
                active.visual[i].phase -= 2;
                if (active.visual[i].phase <= -40) {
                    active.visual[i].phase = -40;
                    active.visual[i].rising = 1;
                }
            }
            active.visual[i].spin = (active.visual[i].spin + 20u) & 0x3ffu;
            memcpy(frame->position, active.visual[i].position,
                sizeof(frame->position));
            frame->lift = active.visual[i].lift;
            frame->phase = (float)active.visual[i].phase;
            frame->spin = active.visual[i].spin;
            frame->phase_ticks = active.visual[i].phase;
            frame->rising = active.visual[i].rising;
            active.stats.visual_frames++;
            return 1;
        }
    }
    return 0;
}

int level_teleporters_begin_visual_update(uint32_t destination) {
    uint32_t i;
    if (!destination) return 0;
    for (i = 0; i < active.stats.count; ++i)
        if (active.destinations[i] == destination) {
            active.stats.update_visits++;
            return 1;
        }
    return 0;
}

void level_teleporters_stats(LevelTeleportersStats *output) {
    if (output) *output = active.stats;
}
