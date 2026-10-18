#include "level_citadels.h"
#include "resource_file.h"
#include "scene_projection_storage.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct LevelCitadels {
    LevelCitadelDefinition definitions[LEVEL_CITADELS_MAX];
    struct { uint32_t object, positions, angles, children, distances,
        slots, footprints; } runtime[LEVEL_CITADELS_MAX];
    LevelCitadelsStats stats;
} LevelCitadels;
static LevelCitadels active;

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void write_float(GameMemory *memory, uint32_t address, float value) {
    game_memory_write_u32(memory, address, float_bits(value));
}

int level_citadels_construct_runtime(GameMemory *memory, uint32_t index,
    uint32_t object, const LevelCitadelDefinition *definition,
    void *height_context, LevelCitadelHeight height) {
    uint32_t positions = 0, angles = 0, children = 0, distances = 0;
    uint32_t slots = 0, footprints = 0;
    uint32_t count, i;
    float min_x, min_z, max_x, max_z, center_x, center_z, center_y;
    float base_x, base_z;
    if (!memory || !definition || !height || !object ||
        index >= LEVEL_CITADELS_MAX || active.runtime[index].object ||
        !definition->footprint_count ||
        definition->footprint_count > LEVEL_CITADEL_FOOTPRINTS_MAX ||
        definition->upgrade_count > LEVEL_CITADEL_UPGRADES_MAX) return 0;
    count = definition->upgrade_count + 1u;
    positions = scene_projection_storage_allocate(count * 12u,
        "citadel positions");
    angles = scene_projection_storage_allocate(count * 2u,
        "citadel headings");
    children = scene_projection_storage_allocate(count * 4u,
        "citadel children");
    distances = scene_projection_storage_allocate(count * 4u,
        "citadel distances");
    slots = scene_projection_storage_allocate(count * 8u,
        "citadel slots");
    if (definition->footprint_count > 1u)
        footprints = scene_projection_storage_allocate(
            definition->footprint_count * 16u, "citadel footprints");
    if (!positions || !angles || !children || !distances || !slots ||
        (definition->footprint_count > 1u && !footprints)) goto fail;
    min_x = max_x = definition->footprints[0].min_x;
    min_z = max_z = definition->footprints[0].min_z;
    for (i = 0; i < definition->footprint_count; ++i) {
        const LevelCitadelRect *rect = &definition->footprints[i];
        if (rect->min_x < min_x) min_x = rect->min_x;
        if (rect->min_z < min_z) min_z = rect->min_z;
        if (rect->max_x > max_x) max_x = rect->max_x;
        if (rect->max_z > max_z) max_z = rect->max_z;
        if (footprints) {
            write_float(memory, footprints + i * 16u, rect->min_x);
            write_float(memory, footprints + i * 16u + 4u, rect->min_z);
            write_float(memory, footprints + i * 16u + 8u, rect->max_x);
            write_float(memory, footprints + i * 16u + 12u, rect->max_z);
        }
    }
    if (definition->claim.min_x < min_x) min_x = definition->claim.min_x;
    if (definition->claim.min_z < min_z) min_z = definition->claim.min_z;
    if (definition->claim.max_x > max_x) max_x = definition->claim.max_x;
    if (definition->claim.max_z > max_z) max_z = definition->claim.max_z;
    center_x = (min_x + max_x) * 0.5f;
    center_z = (min_z + max_z) * 0.5f;
    if (!height(height_context, center_x, center_z, &center_y)) goto fail;
    base_x = definition->base.x * 512.0f;
    base_z = definition->base.z * 512.0f;
    for (i = 0; i < count; ++i) {
        const LevelCitadelPlacement *place = i ?
            &definition->upgrades[i - 1u] : &definition->base;
        float x = place->x * 512.0f, z = place->z * 512.0f, y;
        float dx = x - base_x, dz = z - base_z;
        if (!height(height_context, x, z, &y)) goto fail;
        write_float(memory, positions + i * 12u, x);
        write_float(memory, positions + i * 12u + 4u, y);
        write_float(memory, positions + i * 12u + 8u, z);
        game_memory_write_u16(memory, angles + i * 2u,
            (uint16_t)((int32_t)(place->heading_degrees *
                5.688889026641846f) & 0x7ff));
        game_memory_write_u32(memory, children + i * 4u, 0u);
        write_float(memory, distances + i * 4u, sqrtf(dx * dx + dz * dz));
        game_memory_write_u32(memory, slots + i * 8u, 0xffffffffu);
        game_memory_write_u32(memory, slots + i * 8u + 4u, 0xffffffffu);
    }
    game_memory_write_u32(memory, object, index);
    write_float(memory, object + 4u, definition->claim.min_x);
    write_float(memory, object + 8u, definition->claim.min_z);
    write_float(memory, object + 12u, definition->claim.max_x);
    write_float(memory, object + 16u, definition->claim.max_z);
    game_memory_write_u32(memory, object + 0x14u,
        definition->health == -1 ? 5000u : (uint32_t)definition->health);
    game_memory_write_u32(memory, object + 0x18u, children);
    game_memory_write_u32(memory, object + 0x1cu, positions);
    game_memory_write_u32(memory, object + 0x20u, angles);
    game_memory_write_u16(memory, object + 0x24u, (uint16_t)count);
    game_memory_write_u16(memory, object + 0x26u, 0u);
    game_memory_write_u32(memory, object + 0x28u, distances);
    game_memory_write_u32(memory, object + 0x2cu, slots);
    game_memory_write_u32(memory, object + 0x30u, footprints);
    game_memory_write_u16(memory, object + 0x34u,
        footprints ? (uint16_t)definition->footprint_count : 0u);
    write_float(memory, object + 0x38u, min_x);
    write_float(memory, object + 0x3cu, min_z);
    write_float(memory, object + 0x40u, max_x);
    write_float(memory, object + 0x44u, max_z);
    write_float(memory, object + 0x48u, center_x);
    write_float(memory, object + 0x4cu, center_y);
    write_float(memory, object + 0x50u, center_z);
    for (i = 0x54u; i <= 0x68u; i += 4u)
        game_memory_write_u32(memory, object + i, 0u);
    if (memory->fault) goto fail;
    active.runtime[index].object = object;
    active.runtime[index].positions = positions;
    active.runtime[index].angles = angles;
    active.runtime[index].children = children;
    active.runtime[index].distances = distances;
    active.runtime[index].slots = slots;
    active.runtime[index].footprints = footprints;
    active.stats.runtime_constructed++;
    active.stats.runtime_live++;
    return 1;
fail:
    if (footprints) scene_projection_storage_free(footprints);
    if (slots) scene_projection_storage_free(slots);
    if (distances) scene_projection_storage_free(distances);
    if (children) scene_projection_storage_free(children);
    if (angles) scene_projection_storage_free(angles);
    if (positions) scene_projection_storage_free(positions);
    return 0;
}

int level_citadels_owns_runtime(uint32_t object) {
    unsigned i;
    if (!object) return 0;
    for (i = 0; i < LEVEL_CITADELS_MAX; ++i)
        if (active.runtime[i].object == object) return 1;
    return 0;
}
void level_citadels_retire_runtime(uint32_t object) {
    unsigned i;
    for (i = 0; i < LEVEL_CITADELS_MAX; ++i) {
        if (active.runtime[i].object != object) continue;
        memset(&active.runtime[i], 0, sizeof(active.runtime[i]));
        active.stats.runtime_retired++;
        --active.stats.runtime_live;
        return;
    }
}

static int next_line(ResourceFile *file, char *line, size_t capacity) {
    while (resource_file_line(file, line, capacity)) {
        const unsigned char *text = (const unsigned char *)line;
        while (isspace(*text)) ++text;
        if (*text && *text != '#') return 1;
    }
    return 0;
}

static int read_rect(const char *line, LevelCitadelRect *rect) {
    int x1, z1, x2, z2;
    if (sscanf(line, "%d %d %d %d", &x1, &z1, &x2, &z2) != 4) return 0;
    rect->min_x = (float)(x1 < x2 ? x1 : x2) * 512.0f;
    rect->min_z = (float)(z1 < z2 ? z1 : z2) * 512.0f;
    rect->max_x = (float)(x1 > x2 ? x1 : x2) * 512.0f;
    rect->max_z = (float)(z1 > z2 ? z1 : z2) * 512.0f;
    return 1;
}

int level_citadels_load(const char *path, int32_t offset, void *context,
    int (*begin)(void *, uint32_t),
    int (*construct)(void *, uint32_t, const LevelCitadelDefinition *),
    uint32_t *next_offset) {
    ResourceFile *file;
    LevelCitadelDefinition parsed[LEVEL_CITADELS_MAX];
    char line[512];
    uint32_t count, end;
    unsigned i, j;
    if (!path || !begin || !construct || !next_offset || offset < 0) return 0;
    file = resource_file_open(path);
    if (!file) return 0;
    if (!resource_file_seek(file, offset, SEEK_SET) ||
        !next_line(file, line, sizeof(line)) ||
        sscanf(line, "num_citadels %u", &count) != 1 ||
        !count || count > LEVEL_CITADELS_MAX) goto fail;
    memset(parsed, 0, sizeof(parsed));
    for (i = 0; i < count; ++i) {
        LevelCitadelDefinition *item = &parsed[i];
        unsigned footprints, upgrades, listed_upgrades, claim_count;
        if (!next_line(file, line, sizeof(line)) ||
            sscanf(line, "citadel %d %u %u %d", &item->owner, &footprints,
                &upgrades, &item->health) != 4 ||
            !footprints || footprints > LEVEL_CITADEL_FOOTPRINTS_MAX ||
            upgrades > LEVEL_CITADEL_UPGRADES_MAX) goto fail;
        item->footprint_count = footprints;
        item->upgrade_count = upgrades;
        for (j = 0; j < footprints; ++j)
            if (!next_line(file, line, sizeof(line)) ||
                !read_rect(line, &item->footprints[j])) goto fail;
        if (!next_line(file, line, sizeof(line)) ||
            sscanf(line, "claim %u", &claim_count) != 1 ||
            claim_count != 1u ||
            !next_line(file, line, sizeof(line)) ||
            !read_rect(line, &item->claim)) goto fail;
        if (!next_line(file, line, sizeof(line)) ||
            sscanf(line, "base %f %f %f", &item->base.x, &item->base.z,
                &item->base.heading_degrees) != 3 ||
            !next_line(file, line, sizeof(line)) ||
            sscanf(line, "upgrades %u", &listed_upgrades) != 1 ||
            listed_upgrades != upgrades) goto fail;
        for (j = 0; j < upgrades; ++j)
            if (!next_line(file, line, sizeof(line)) ||
                sscanf(line, "%f %f %f %d", &item->upgrades[j].x,
                    &item->upgrades[j].z,
                    &item->upgrades[j].heading_degrees,
                    &item->upgrades[j].kind) != 4) goto fail;
    }
    if (!resource_file_tell(file, &end)) goto fail;
    resource_file_close(file);
    if (!begin(context, count)) return 0;
    for (i = 0; i < count; ++i) {
        if (!construct(context, i, &parsed[i])) return 0;
        active.stats.constructed++;
    }
    for (i = 0; i < count; ++i) active.definitions[i] = parsed[i];
    active.stats.loads++;
    active.stats.generation++;
    active.stats.count = count;
    *next_offset = end;
    return 1;
fail:
    resource_file_close(file);
    return 0;
}

int level_citadels_get(uint32_t index, LevelCitadelDefinition *output) {
    if (!output || index >= active.stats.count) return 0;
    *output = active.definitions[index];
    return 1;
}

void level_citadels_unload(void) {
    if (active.stats.count) {
        active.stats.unloads++;
        active.stats.count = 0;
    }
}

void level_citadels_stats(LevelCitadelsStats *output) {
    if (output) *output = active.stats;
}
