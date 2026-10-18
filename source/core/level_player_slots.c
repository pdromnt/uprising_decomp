#include "level_player_slots.h"
#include "resource_file.h"
#include "level_effects.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_LEVEL_PLAYER_SLOTS = 16 };
typedef struct LevelPlayerSlots {
    LevelPlayerSlot slots[MAX_LEVEL_PLAYER_SLOTS];
    uint32_t scene_players[6], previous_scenes[6], assigned_scenes[6];
    LevelPlayerClass classes[6];
    struct { uint32_t player, object, head_offset; } *members;
    size_t member_count, member_capacity;
    struct { uint32_t *items; size_t capacity; } snapshots[6][3];
    uint8_t scenes_live;
    uint8_t active_scene_mask, active_scene_cursor, active_scene_started;
    LevelPlayerSlotsStats stats;
} LevelPlayerSlots;
static LevelPlayerSlots active;

static int next_line(ResourceFile *file, char *line, size_t capacity) {
    while (resource_file_line(file, line, capacity)) {
        const unsigned char *text = (const unsigned char *)line;
        while (isspace(*text)) ++text;
        if (*text && *text != '#') return 1;
    }
    return 0;
}

int level_player_slots_load(const char *path, int32_t offset, void *context,
    int (*place)(void *, uint32_t, uint32_t, const LevelPlayerSlot *),
    uint32_t *next_offset) {
    ResourceFile *file;
    LevelPlayerSlot parsed[MAX_LEVEL_PLAYER_SLOTS];
    char line[512];
    uint32_t count, end;
    unsigned i;
    if (!path || !place || !next_offset || offset < 0) return 0;
    file = resource_file_open(path);
    if (!file) return 0;
    if (!resource_file_seek(file, offset, SEEK_SET) ||
        !next_line(file, line, sizeof(line)) ||
        sscanf(line, "slots %u", &count) != 1 ||
        !count || count > MAX_LEVEL_PLAYER_SLOTS) goto fail;
    for (i = 0; i < count; ++i) {
        if (!next_line(file, line, sizeof(line)) ||
            sscanf(line, "%f %f %f", &parsed[i].x, &parsed[i].z,
                &parsed[i].heading_degrees) != 3) goto fail;
    }
    if (!resource_file_tell(file, &end)) goto fail;
    resource_file_close(file);
    for (i = 0; i < count; ++i) {
        if (!place(context, i, count, &parsed[i])) return 0;
        active.stats.placements++;
    }
    for (i = 0; i < count; ++i) active.slots[i] = parsed[i];
    active.stats.loads++;
    active.stats.generation++;
    active.stats.count = count;
    *next_offset = end;
    return 1;
fail:
    resource_file_close(file);
    return 0;
}

int level_player_slots_get(uint32_t index, LevelPlayerSlot *output) {
    if (!output || index >= active.stats.count) return 0;
    *output = active.slots[index];
    return 1;
}

int level_player_slots_place_owned(GameMemory *memory,uint32_t index, uint32_t count,
    const LevelPlayerSlot *slot, void *context,
    const LevelPlayerSceneOps *ops) {
    uint32_t assignment, player, subtype, scene, sequence, next;
    uint16_t heading;
    int first;
    if (!slot || !ops || !ops->publish_anchor || !ops->publish_spawn ||
        !ops->assignment || !ops->subtype || !ops->make_scene ||
        !ops->publish_scene || !ops->attach_scene || !ops->place_scene ||
        !active.scenes_live || !count || index >= count) return 0;
    first = index == 0u;
    if (first && !ops->publish_anchor(context, slot->x, slot->z)) return 0;
    if (first && count != 1u) return 1;
    heading = first ? 0u : (uint16_t)((int32_t)(
        slot->heading_degrees * 5.688889026641846f) & 0x7ff);
    if (!ops->publish_spawn(context, first ? 0u : index - 1u,
            slot->x, slot->z, heading)) return 0;
    if (first) assignment = 0u;
    else if (!ops->assignment(context, index - 1u, &assignment)) return 0;
    if (assignment == UINT32_MAX) return 1;
    if (assignment >= 6u ||
        !level_player_slots_scene_player(assignment, &player) || !player ||
        !ops->subtype(context, player, &subtype) ||
        !ops->make_scene(context, subtype, &scene) || !scene) return 0;
    /* From here the scene is owned, including a failed attach or placement. */
    if (!level_player_slots_assign_scene(player, scene) ||
        !ops->publish_scene(context, player, scene) ||
        !ops->attach_scene(context, player, scene) ||
        !level_player_slots_next_owner_sequence(memory,player, &sequence, &next) ||
        !ops->place_scene(context, player, scene, heading, sequence, next,
            first))
        return 0;
    level_player_slots_note_scene_created();
    return 1;
}

void level_player_slots_note_scene_created(void) {
    active.stats.scenes_created++;
}

int level_player_slots_begin_scenes(GameMemory *memory,const uint32_t players[6],
    const uint32_t previous_scenes[6], const uint16_t owners[6],
    const uint32_t sequences[6]) {
    unsigned i;
    if (!memory || memory->fault || !players || !previous_scenes || !owners || !sequences ||
        active.scenes_live) return 0;
    for(i=0;i<6;i++)if(players[i]){
        game_memory_write_u16(memory,players[i]+8,owners[i]);
        game_memory_write_u32(memory,players[i]+24,sequences[i]);
    }
    if(memory->fault)return 0;
    memcpy(active.scene_players, players, sizeof(active.scene_players));
    memcpy(active.previous_scenes, previous_scenes,
        sizeof(active.previous_scenes));
    memset(active.classes, 0, sizeof(active.classes));
    memset(active.assigned_scenes, 0, sizeof(active.assigned_scenes));
    active.scenes_live = 1;
    return 1;
}

int level_player_slots_bind_class(uint32_t player,
    LevelPlayerClass player_class) {
    unsigned i;
    if (!active.scenes_live || !player ||
        player_class > LEVEL_PLAYER_IMPERIAL) return 0;
    for (i = 0; i < 6u; ++i)
        if (active.scene_players[i] == player) {
            active.classes[i] = player_class;
            return 1;
        }
    return 0;
}

LevelPlayerClass level_player_slots_class(uint32_t player) {
    unsigned i;
    if (!active.scenes_live || !player) return LEVEL_PLAYER_EXTERNAL;
    for (i = 0; i < 6u; ++i)
        if (active.scene_players[i] == player) return active.classes[i];
    return LEVEL_PLAYER_EXTERNAL;
}

int level_player_slots_next_owner_sequence(GameMemory *memory,uint32_t player,
    uint32_t *packed, uint32_t *next_sequence) {
    unsigned i;
    if (!memory || memory->fault || !active.scenes_live || !packed || !next_sequence) return 0;
    for (i = 0; i < 6u; ++i) {
        if (active.scene_players[i] != player) continue;
        /*The catalog record is authoritative across raw producers and archive
         *restore. A private counter here reused already-live identities. */
        uint32_t sequence=game_memory_read_u32(memory,player+24);
        *packed = game_memory_read_u16(memory,player+8) | (sequence<<6u);
        *next_sequence=sequence+1u;
        game_memory_write_u32(memory,player+24,*next_sequence);
        if(memory->fault)return 0;
        active.stats.sequences++;
        return 1;
    }
    return 0;
}

int level_player_slots_owns_player(uint32_t player) {
    unsigned i;
    if (!active.scenes_live || !player) return 0;
    for (i = 0; i < 6u; ++i)
        if (active.scene_players[i] == player) return 1;
    return 0;
}

int level_player_slots_attach_scene_member(uint32_t player,
    uint32_t object, uint32_t head_offset) {
    size_t i;
    unsigned slot;
    void *grown;
    if (!active.scenes_live || !object ||
        (head_offset != 0x1cu && head_offset != 0x80u &&
            head_offset != 0xe4u)) return 0;
    for (slot = 0; slot < 6u; ++slot)
        if (active.scene_players[slot] == player) break;
    if (slot == 6u) return 0;
    for (i = 0; i < active.member_count; ++i)
        if (active.members[i].object == object) return 0;
    if (active.member_count == active.member_capacity) {
        size_t capacity = active.member_capacity ? active.member_capacity * 2u : 64u;
        if (capacity < active.member_capacity ||
            capacity > SIZE_MAX / sizeof(*active.members)) return 0;
        grown = realloc(active.members, capacity * sizeof(*active.members));
        if (!grown) return 0;
        active.members = grown;
        active.member_capacity = capacity;
    }
    active.members[active.member_count].player = player;
    active.members[active.member_count].object = object;
    active.members[active.member_count].head_offset = head_offset;
    active.member_count++;
    active.stats.scene_insertions++;
    active.stats.live_members++;
    return 1;
}

int level_player_slots_forget_scene_member(uint32_t object) {
    size_t i;
    for (i = 0; i < active.member_count; ++i) {
        if (active.members[i].object != object) continue;
        --active.member_count;
        memmove(&active.members[i], &active.members[i + 1u],
            (active.member_count - i) * sizeof(*active.members));
        active.stats.scene_removals++;
        active.stats.live_members--;
        return 1;
    }
    return 0;
}

int level_player_slots_snapshot_scene_members(uint32_t player,
    uint32_t head_offset, const uint32_t **objects, size_t *count) {
    size_t i, used = 0;
    unsigned slot, bucket;
    void *grown;
    if (!objects || !count || !active.scenes_live ||
        !level_player_slots_owns_player(player)) return 0;
    for (slot = 0; slot < 6u; ++slot)
        if (active.scene_players[slot] == player) break;
    if (slot == 6u) return 0;
    bucket = head_offset == 0x1cu ? 0u :
        head_offset == 0x80u ? 1u :
        head_offset == 0xe4u ? 2u : 3u;
    if (bucket == 3u) return 0;
    if (active.snapshots[slot][bucket].capacity < active.member_count) {
        grown = realloc(active.snapshots[slot][bucket].items,
            active.member_count * sizeof(uint32_t));
        if (!grown) return 0;
        active.snapshots[slot][bucket].items = grown;
        active.snapshots[slot][bucket].capacity = active.member_count;
    }
    for (i = active.member_count; i > 0; --i) {
        if (active.members[i - 1u].player == player &&
            active.members[i - 1u].head_offset == head_offset)
            active.snapshots[slot][bucket].items[used++] =
                active.members[i - 1u].object;
    }
    *objects = active.snapshots[slot][bucket].items;
    *count = used;
    return 1;
}

int level_player_slots_has_scene_member(uint32_t player, uint32_t object) {
    size_t i;
    for (i = 0; i < active.member_count; ++i)
        if (active.members[i].player == player &&
            active.members[i].object == object) return 1;
    return 0;
}

void level_player_slots_note_scene_update(void) {
    active.stats.update_visits++;
}

int level_player_slots_destroy_scene_members(void *context,
    int (*destroy)(void *, uint32_t)) {
    if (!destroy) return 0;
    /* Resource preparation can fail before a scene session exists. There is
     * then no member owner to retire; reject only an inconsistent live list. */
    if (!active.scenes_live) return active.member_count == 0;
    active.stats.destroy_passes++;
    while (active.member_count) {
        uint32_t object = active.members[active.member_count - 1u].object;
        size_t i;
        if (!destroy(context, object)) {
            fprintf(stderr,"native scene member destruction failed: object=%08x\n",object);
            return 0;
        }
        for (i = 0; i < active.member_count; ++i)
            if (active.members[i].object == object) {
                fprintf(stderr,"native scene member retained after destruction: object=%08x\n",object);
                return 0;
            }
        active.stats.owner_destroyed++;
    }
    return 1;
}

int level_player_slots_assign_scene(uint32_t player, uint32_t scene) {
    unsigned i;
    if (!active.scenes_live || !player || !scene) return 0;
    for (i = 0; i < 6u; ++i)
        if (active.scene_players[i] == player) {
            active.assigned_scenes[i] = scene;
            return 1;
        }
    return 0;
}

int level_player_slots_destroy_new_scenes(void *context,
    int (*destroy)(void *, uint32_t)) {
    unsigned i;
    if (!destroy) return 0;
    if (!active.scenes_live) return active.member_count == 0;
    for (i = 6u; i > 0; --i) {
        unsigned slot = i - 1u;
        uint32_t scene = active.assigned_scenes[slot];
        if (!scene || scene == active.previous_scenes[slot]) continue;
        if (!destroy(context, active.scene_players[slot])) return 0;
        active.assigned_scenes[slot] = 0;
        active.stats.player_scenes_destroyed++;
    }
    return 1;
}

int level_player_slots_destroy_active_scenes(void *context,
    int (*is_active)(void *, uint32_t, int *),
    int (*destroy)(void *, uint32_t), uint32_t players[6]) {
    unsigned i;
    if (!is_active || !destroy || !players)
        return 0;
    if (!active.scenes_live) {
        memset(players, 0, 6u * sizeof(*players));
        return active.member_count == 0;
    }
    if (!active.active_scene_started) {
        uint8_t mask = 0u;
        for (i = 0; i < 6u; ++i) {
            int present = 0;
            uint32_t player = active.scene_players[i];
            if (!player || !is_active(context, player, &present)) return 0;
            if (present) mask |= (uint8_t)(1u << i);
        }
        active.active_scene_mask = mask;
        active.active_scene_started = 1u;
    }
    for (i = 0; i < 6u; ++i) players[i] = active.scene_players[i];
    while (active.active_scene_cursor < 6u) {
        unsigned slot = active.active_scene_cursor;
        if ((active.active_scene_mask & (1u << slot)) &&
            !destroy(context, active.scene_players[slot])) return 0;
        if (active.active_scene_mask & (1u << slot))
            active.stats.player_scenes_destroyed++;
        active.active_scene_cursor++;
    }
    return 1;
}

int level_player_slots_scene_player(unsigned index, uint32_t *player) {
    if (!player || !active.scenes_live || index >= 6u) return 0;
    *player = active.scene_players[index];
    return 1;
}

void level_player_slots_unload(void) {
    unsigned slot, bucket;
    for (slot = 0; slot < 6u; ++slot)
        for (bucket = 0; bucket < 3u; ++bucket) {
            free(active.snapshots[slot][bucket].items);
            active.snapshots[slot][bucket].items = NULL;
            active.snapshots[slot][bucket].capacity = 0;
        }
    active.stats.discarded_members += (uint32_t)active.member_count;
    level_effects_clear();
    free(active.members);
    active.members = NULL;
    active.member_count = active.member_capacity = 0;
    active.stats.live_members = 0;
    active.scenes_live = 0;
    active.active_scene_mask = active.active_scene_cursor =
        active.active_scene_started = 0u;
    memset(active.scene_players, 0, sizeof(active.scene_players));
    memset(active.previous_scenes, 0, sizeof(active.previous_scenes));
    memset(active.assigned_scenes, 0, sizeof(active.assigned_scenes));
    memset(active.classes, 0, sizeof(active.classes));
    if (active.stats.count) {
        active.stats.unloads++;
        active.stats.count = 0;
    }
}

void level_player_slots_stats(LevelPlayerSlotsStats *output) {
    if (output) *output = active.stats;
}
