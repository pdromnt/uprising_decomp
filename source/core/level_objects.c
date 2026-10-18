#include "level_objects.h"
#include "resource_file.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct LevelObjects {
    LevelObjectDefinition definitions[LEVEL_OBJECTS_MAX];
    uint32_t objects[LEVEL_OBJECTS_MAX];
    LevelObjectsStats stats;
} LevelObjects;
static LevelObjects active;

float level_object_start_height(uint8_t type,float level_height,float center_height,long double terrain_height) {
    if(type==0x26 || type==0x27 || type==0xb0)return level_height*512.0f;
    return (float)(terrain_height+(long double)center_height);
}

static int next_line(ResourceFile *file, char *line, size_t capacity) {
    while (resource_file_line(file, line, capacity)) {
        const unsigned char *text = (const unsigned char *)line;
        while (isspace(*text)) ++text;
        if (*text && *text != '#') return 1;
    }
    return 0;
}

int level_objects_load(const char *path, int32_t offset, void *context,
    int (*begin)(void *, uint32_t, uint32_t),
    int (*construct)(void *, uint32_t, const LevelObjectDefinition *, uint32_t *),
    int (*link)(void *, uint32_t, uint32_t),
    void (*end)(void *), uint32_t *next_offset) {
    ResourceFile *file;
    char line[512];
    uint32_t count, first_record, following_section;
    unsigned i, j;
    int started = 0;
    if (!path || !begin || !construct || !link || !end || !next_offset ||
        offset < 0) return 0;
    file = resource_file_open(path);
    if (!file) return 0;
    if (!resource_file_seek(file, offset, SEEK_SET) ||
        !next_line(file, line, sizeof(line)) ||
        sscanf(line, "objects %u", &count) != 1 ||
        count > LEVEL_OBJECTS_MAX ||
        !resource_file_tell(file, &first_record)) goto fail;
    memset(active.definitions, 0, sizeof(active.definitions));
    memset(active.objects, 0, sizeof(active.objects));
    for (i = 0; i < count; ++i) {
        LevelObjectDefinition *record = &active.definitions[i];
        if (!next_line(file, line, sizeof(line)) ||
            sscanf(line, "%d %d %f %f %f %f %63s %63s %63s %d %d %d",
                &record->kind, &record->owner, &record->x, &record->z,
                &record->y, &record->heading_degrees, record->name,
                record->attribute, record->parent_name, &record->flags,
                &record->group, &record->variant) != 12) goto fail;
    }
    if (!resource_file_tell(file, &following_section)) goto fail;
    resource_file_close(file);
    file = NULL;
    if (!begin(context, count, first_record)) goto fail;
    started = 1;
    for (i = 0; i < count; ++i) {
        if (!construct(context, i, &active.definitions[i],
                &active.objects[i])) {
            fprintf(stderr,"native level object construction failed: row=%u kind=%d owner=%d name=%s\n",
                i,active.definitions[i].kind,active.definitions[i].owner,active.definitions[i].name);
            goto fail;
        }
        active.stats.constructed++;
    }
    for (i = 0; i < count; ++i) {
        const char *parent = active.definitions[i].parent_name;
        if (!strcmp(parent, "NULL")) continue;
        for (j = 0; j < count; ++j)
            if (!strcmp(parent, active.definitions[j].name)) break;
        if (j == count || !link(context, active.objects[i],
                active.objects[j])) goto fail;
        active.stats.links++;
    }
    end(context);
    active.stats.loads++;
    active.stats.generation++;
    active.stats.count = count;
    *next_offset = following_section;
    return 1;
fail:
    if (file) resource_file_close(file);
    if (started) end(context);
    memset(active.objects, 0, sizeof(active.objects));
    return 0;
}

int level_objects_get(uint32_t index, LevelObjectDefinition *output,
    uint32_t *object) {
    if (index >= active.stats.count || !output || !object) return 0;
    *output = active.definitions[index];
    *object = active.objects[index];
    return 1;
}

void level_objects_unload(void) {
    if (active.stats.count) {
        active.stats.unloads++;
        active.stats.count = 0;
        memset(active.objects, 0, sizeof(active.objects));
    }
}

void level_objects_stats(LevelObjectsStats *output) {
    if (output) *output = active.stats;
}
