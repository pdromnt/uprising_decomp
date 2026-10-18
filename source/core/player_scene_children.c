#include "player_scene_children.h"
#include "scene_projection_storage.h"

#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct ChildrenEntry {
    PlayerSceneChildren graph;
    struct ChildrenEntry *next;
} ChildrenEntry;
static ChildrenEntry *entries;

static void put_float(GameMemory *memory, GameAddress address, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    game_memory_write_u32(memory, address, bits);
}

static float get_float(GameMemory *memory, GameAddress address) {
    uint32_t bits = game_memory_read_u32(memory, address);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t trunc_low(long double value) {
    if (!isfinite(value) || value < (long double)INT64_MIN ||
        value >= (long double)INT64_MAX) return 0x80000000u;
    return (uint32_t)(int64_t)value;
}

int player_scene_children_prepare(GameMemory *memory, GameAddress scene,
        GameAddress motion, int32_t height, int32_t top, int32_t bottom,
        int32_t base) {
    float width_scale, height_scale, depth_scale;
    if (!memory) return 0;
    /* Float stores precede the extended calculations in the original. */
    /* Exact literals from the builder, rather than a compiler's 1/3. */
    game_memory_write_u32(memory, scene + 0x2dcu, 0x3eaaaa9fu);
    game_memory_write_u32(memory, scene + 0x2e0u, 0x3eaaaa9fu);
    game_memory_write_u32(memory, scene + 0x2e4u, 0x3eaaaac1u);
    width_scale = get_float(memory, scene + 0x2dcu);
    height_scale = get_float(memory, scene + 0x2e0u);
    depth_scale = get_float(memory, scene + 0x2e4u);
    put_float(memory, scene + 0x128u,
        (float)(((long double)width_scale * get_float(memory, 0x00574d58u) +
        get_float(memory, 0x00574d3cu)) * (float)height));
    game_memory_write_u32(memory, motion + 0x20u,
        (uint32_t)top - trunc_low((long double)top * depth_scale *
            get_float(memory, 0x00574d5cu)));
    game_memory_write_u32(memory, motion + 0x14u,
        (uint32_t)bottom + trunc_low((long double)bottom * depth_scale *
            get_float(memory, 0x00574d5cu)));
    game_memory_write_u32(memory, scene + 0x5cu,
        (uint32_t)base + trunc_low((long double)base * height_scale *
            get_float(memory, 0x00574d60u)));
    game_memory_write_u32(memory, scene + 0x2ecu, 0x3f6147aeu);
    game_memory_write_u32(memory, scene + 0x2f4u, 4u);
    game_memory_write_u32(memory, scene + 0x49cu, 1u);
    game_memory_write_u32(memory, scene + 0x4a0u, 1u);
    game_memory_write_u16(memory, scene + 0x2ceu, 0u);
    game_memory_write_u32(memory, scene + 0x2d0u, 0u);
    game_memory_write_u16(memory, scene + 0x2ccu, 0u);
    put_float(memory, scene + 0x2e8u, 1.0f);
    put_float(memory, scene + 0x2d4u, 1.0f);
    game_memory_write_u32(memory, scene + 0x2d8u, 0u);
    game_memory_write_u8(memory, scene + 0xd8u,
        game_memory_read_u8(memory, scene + 0xd8u) | 1u);
    return !memory->fault;
}

static int construct_child(GameMemory *memory, PlayerSceneChild *child) {
    GameSpatialAttachmentInit *init = &child->attachment;
    unsigned axis;
    size_t size = child->kind == PLAYER_SCENE_COMMAND ? 0x20u :
        child->kind >= PLAYER_SCENE_POSITION ? 0x2cu : 0xbcu;
    child->projection = scene_projection_storage_allocate(size,
        "player scene child");
    if (!child->projection) return 0;
    if (child->kind <= PLAYER_SCENE_DERIVED_SPATIAL) {
        if (!game_spatial_attachment_construct(memory, child->projection,
                init)) return 0;
        if (child->kind == PLAYER_SCENE_DERIVED_SPATIAL)
            game_memory_write_u32(memory, child->projection, 0x00572e20u);
    } else {
        GameAddress object = child->projection;
        GameAddress link = game_memory_read_u32(memory, init->motion + 0x28u);
        if (memory->fault) return 0;
        game_memory_write_u32(memory, object,
            child->kind == PLAYER_SCENE_COMMAND ? 0x00572e38u :
            child->kind == PLAYER_SCENE_POSITION ? 0x00572dd8u : 0x00572df0u);
        game_memory_write_u32(memory, object + 4u, init->parent);
        game_memory_write_u32(memory, object + 8u, 0u);
        game_memory_write_u32(memory, object + 0xcu, link);
        game_memory_write_u16(memory, object + 0x10u, init->heading);
        game_memory_write_u32(memory, object + 0x14u, init->motion);
        game_memory_write_u32(memory, object + 0x18u, 0u);
        game_memory_write_u32(memory, object + 0x1cu, 0u);
        if (child->kind != PLAYER_SCENE_COMMAND)
            for (axis = 0; axis < 3; ++axis)
                put_float(memory, object + 0x20u + axis * 4u,
                    init->position[axis]);
    }
    if (memory->fault || !player_attachment_import(memory, child->projection,
            child->kind, &child->state)) return 0;
    return scene_projection_storage_bind_state(child->projection, &child->state,
        player_attachment_projection_read, player_attachment_projection_write);
}

static void configure(PlayerSceneChild *child, GameAddress scene,
        GameAddress motion, uint16_t heading, const float position[3],
        const float secondary[3], unsigned oriented, unsigned mode,
        unsigned resource, unsigned slot, PlayerSceneChildKind kind) {
    child->kind = kind;
    child->attachment.parent = scene;
    child->attachment.motion = motion;
    child->attachment.heading = heading;
    memcpy(child->attachment.position, position, 3u * sizeof(float));
    memcpy(child->attachment.secondary_position, secondary, 3u * sizeof(float));
    child->attachment.orient_to_motion = oriented;
    child->attachment.mode = mode;
    child->attachment.resource = resource;
    child->attachment.slot = slot;
}

const PlayerSceneChildren *player_scene_children_find(GameAddress scene) {
    ChildrenEntry *entry;
    for (entry = entries; entry; entry = entry->next)
        if (entry->graph.scene == scene) return &entry->graph;
    return NULL;
}

PlayerAttachmentState *player_scene_children_attachment(GameAddress child) {
    return scene_projection_storage_state(child,
        player_attachment_projection_read);
}

int player_scene_children_create(GameMemory *memory, GameAddress scene,
        const PlayerSceneChildrenDefinition *d) {
    ChildrenEntry *entry;
    PlayerSceneChildren *graph;
    float position[3], secondary[3];
    unsigned i;
    if (!memory || !scene || !d || player_scene_children_find(scene)) return 0;
    entry = calloc(1, sizeof(*entry));
    if (!entry) return 0;
    graph = &entry->graph;
    graph->scene = scene;
    /* Preserve retail creation order: body, auxiliary, mount, command,
     * shifted body, then the optional effects and position records. */
    configure(&graph->children[0], scene, d->motion[0], 11u,
        d->mounts[0], d->mounts[1], 1u, 1u, 1u, 0u, PLAYER_SCENE_SPATIAL);
    memcpy(position, d->mounts[0], sizeof(position));
    memcpy(secondary, d->mounts[1], sizeof(secondary));
    {
    int32_t signed_position_bits;
    memcpy(&signed_position_bits, &position[0], sizeof(signed_position_bits));
    if (signed_position_bits > 0) {
        position[0] -= d->small_scale;
        secondary[0] += d->small_scale;
    } else {
        position[0] -= d->lateral_shift;
        position[1] += d->vertical_shift;
        position[2] -= d->rear_shift;
        secondary[0] += d->lateral_shift;
        secondary[1] += d->vertical_shift;
        secondary[2] -= d->rear_shift;
    }
    }
    configure(&graph->auxiliary, scene, d->motion[0], 11u,
        secondary, position, 1u, 1u, 1u, 0u, PLAYER_SCENE_SPATIAL);
    configure(&graph->children[1], scene, d->motion[1], 17u,
        d->mounts[2], d->mounts[3], 1u, 0u, 1u, 0u, PLAYER_SCENE_SPATIAL);
    configure(&graph->children[2], scene, d->motion[2], 0xffffu,
        position, secondary, 0u, 0u, 0u, 0u, PLAYER_SCENE_COMMAND);
    memcpy(position, d->mounts[0], sizeof(position));
    memcpy(secondary, d->mounts[1], sizeof(secondary));
    position[2] -= d->mount_rear_shift;
    secondary[2] -= d->mount_rear_shift;
    configure(&graph->children[11], scene, d->motion[11], 2u,
        position, secondary, 1u, 1u, 1u, 0u, PLAYER_SCENE_SPATIAL);
    position[0] = 74.0f; position[1] = 0.0f; position[2] = 500.0f;
    configure(&graph->children[3], scene, d->motion[3], 14u,
        position, position, 0u, 0u, 1u, 0u, PLAYER_SCENE_SPATIAL);
    configure(&graph->children[9], scene, d->motion[9], 0xffffu,
        position, position, 0u, 0u, 1u, 0u, PLAYER_SCENE_SPATIAL);
    position[0] = 0.0f;
    position[1] = d->dimensions[1] * -0.5f;
    position[2] = d->dimensions[2] * 0.5f;
    configure(&graph->children[8], scene, d->motion[8], 17u,
        position, position, 0u, 0u, 1u, 0u, PLAYER_SCENE_SPATIAL);
    configure(&graph->children[4], scene, d->motion[4], 17u,
        d->mounts[2], d->mounts[3], 1u, 0u, 12u, 5u,
        PLAYER_SCENE_DERIVED_SPATIAL);
    configure(&graph->children[5], scene, d->motion[5], 17u,
        d->mounts[2], d->mounts[3], 1u, 1u, 1u, 0u, PLAYER_SCENE_SPATIAL);
    for (i = 12u; i < 15u; ++i)
        configure(&graph->children[i], scene, d->motion[i], 17u,
            d->mounts[2], d->mounts[3], 1u, 1u, 1u, 0u, PLAYER_SCENE_SPATIAL);
    position[1] = d->dimensions[1] * 0.2f;
    position[2] = d->dimensions[2] * 0.2f;
    configure(&graph->children[6], scene, d->motion[6], 26u,
        position, position, 0u, 0u, 1u, 0u, PLAYER_SCENE_SPATIAL);
    position[1] = 0.0f; position[2] = d->dimensions[2] * -0.5f;
    configure(&graph->children[7], scene, d->motion[7], 3u,
        position, position, 0u, 0u, 0u, 0u, PLAYER_SCENE_POSITION);
    position[1] = d->dimensions[1] * 0.2f;
    position[2] = d->dimensions[2] * 0.2f;
    configure(&graph->children[10], scene, d->motion[10], 125u,
        position, position, 0u, 0u, 0u, 0u, PLAYER_SCENE_MOTION_POSITION);
    {
        static const unsigned order[] = {0u, 15u, 1u, 2u, 11u, 3u, 9u,
            8u, 4u, 5u, 12u, 13u, 14u, 6u, 7u, 10u};
        for (i = 0u; i < sizeof(order) / sizeof(order[0]); ++i) {
            unsigned part = order[i];
            PlayerSceneChild *child = part == 15u ? &graph->auxiliary :
                &graph->children[part];
            if (part != 15u && d->choices[part] < 0) continue;
            if (!construct_child(memory, child)) goto failed;
        }
    }
    for (i = 0u; i < PLAYER_SCENE_CHILD_COUNT; ++i)
        game_memory_write_u32(memory, scene + 0x1d4u + i * 4u,
            graph->children[i].projection);
    game_memory_write_u32(memory, scene + 0x210u, graph->auxiliary.projection);
    if (memory->fault) goto failed;
    entry->next = entries;
    entries = entry;
    return 1;
failed:
    for (i = 0u; i < PLAYER_SCENE_CHILD_COUNT; ++i)
        if (graph->children[i].projection)
            scene_projection_storage_free(graph->children[i].projection);
    if (graph->auxiliary.projection)
        scene_projection_storage_free(graph->auxiliary.projection);
    free(entry);
    return 0;
}

int player_scene_children_release(GameAddress child) {
    ChildrenEntry *entry;
    unsigned i;
    for (entry = entries; entry; entry = entry->next) {
        for (i = 0u; i <= PLAYER_SCENE_CHILD_COUNT; ++i) {
            PlayerSceneChild *part = i == PLAYER_SCENE_CHILD_COUNT ?
                &entry->graph.auxiliary : &entry->graph.children[i];
            if (part->projection != child || !child) continue;
            if (!scene_projection_storage_free(child)) return -1;
            part->projection = 0u;
            return 1;
        }
    }
    return 0;
}

int player_scene_children_finalize(GameMemory *memory, GameAddress scene) {
    const PlayerSceneChildren *graph = player_scene_children_find(scene);
    unsigned i;
    if (!memory || !graph) return 0;
    for (i = 0u; i < PLAYER_SCENE_CHILD_COUNT; ++i) {
        GameAddress child = graph->children[i].projection;
        if (child) {
            GameAddress link = game_memory_read_u32(memory, child + 0xcu);
            if (memory->fault) return 0;
            game_memory_write_u32(memory, scene + 0x3e8u + i * 4u, link);
            if (memory->fault || player_scene_children_release(child) != 1) return 0;
        }
        game_memory_write_u32(memory, scene + 0x1d4u + i * 4u, 0u);
        if (memory->fault) return 0;
    }
    if (graph->auxiliary.projection &&
        player_scene_children_release(graph->auxiliary.projection) != 1) return 0;
    game_memory_write_u32(memory, scene + 0x210u, 0u);
    game_memory_write_u32(memory, scene + 0x1ccu, 0u);
    game_memory_write_u32(memory, scene + 0x1d0u, 0u);
    if (memory->fault) return 0;
    player_scene_children_retire(scene);
    return 1;
}

void player_scene_children_retire(GameAddress scene) {
    ChildrenEntry **cursor = &entries;
    while (*cursor) {
        ChildrenEntry *entry = *cursor;
        if (entry->graph.scene == scene) {
            *cursor = entry->next;
            free(entry);
            return;
        }
        cursor = &entry->next;
    }
}
