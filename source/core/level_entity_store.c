#include "level_entity_store.h"
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum { LEVEL_GUEST_OBJECT_CAPACITY = 256 + 2 * 16 + 16 };
typedef struct LevelGuestObject {
    uint32_t id;
    uint8_t transferred;
} LevelGuestObject;

typedef struct LevelEntityStore {
    uint32_t *ids;
    uint8_t *destroyed;
    uint32_t count,view;
    LevelGuestObject guest_objects[LEVEL_GUEST_OBJECT_CAPACITY];
    uint32_t guest_count;
    LevelEntityStoreStats stats;
} LevelEntityStore;

static LevelEntityStore entities;

uint32_t level_entity_store_begin(uint32_t count) {
    uint32_t *ids;
    uint8_t *destroyed;
    if (!count || count > INT16_MAX || entities.view) return 0u;
    ids=calloc(count,sizeof(*ids));
    if (!ids) return 0u;
    destroyed=calloc(count,sizeof(*destroyed));
    if (!destroyed) { free(ids); return 0u; }
    entities.ids=ids;
    entities.destroyed=destroyed;
    entities.count=count;
    entities.view=LEVEL_ENTITY_VIEW_BASE;
    entities.stats.loads++;
    entities.stats.live=1u;
    entities.stats.count=count;
    return entities.view;
}

int level_entity_store_owns(uint32_t view) {
    return view && view==entities.view;
}

uint32_t level_entity_store_view(void) { return entities.view; }

int level_entity_store_release(uint32_t view) {
    if (!level_entity_store_owns(view)) return 0;
    free(entities.ids);
    free(entities.destroyed);
    entities.ids=NULL;
    entities.destroyed=NULL;
    entities.count=entities.view=0u;
    entities.stats.releases++;
    entities.stats.live=entities.stats.count=0u;
    return 1;
}

void level_entity_store_shutdown(void) {
    if (entities.view) level_entity_store_release(entities.view);
    level_entity_store_clear_guest_objects();
}

int level_entity_store_get(uint32_t index,uint32_t *object_id) {
    if (!object_id || !entities.ids || index>=entities.count) return 0;
    *object_id=entities.ids[index];
    return 1;
}

int level_entity_store_set(uint32_t index,uint32_t object_id) {
    if (!entities.ids || index>=entities.count) return 0;
    entities.ids[index]=object_id;
    entities.destroyed[index]=0u;
    return 1;
}

static int view_offset(uint32_t address, size_t size, size_t *offset) {
    size_t bytes=(size_t)entities.count*sizeof(*entities.ids);
    if (!entities.ids || !offset || address<LEVEL_ENTITY_VIEW_BASE ||
            address>=LEVEL_ENTITY_VIEW_END) return 0;
    *offset=(size_t)(address-LEVEL_ENTITY_VIEW_BASE);
    return *offset<=bytes && size<=bytes-*offset;
}

int level_entity_store_read(uint32_t address, void *output, size_t size) {
    size_t offset,i;
    if (!output || !view_offset(address,size,&offset)) return 0;
    for(i=0;i<size;++i) {
        size_t at=offset+i;
        ((uint8_t *)output)[i]=(uint8_t)(entities.ids[at/4u]>>((at%4u)*8u));
    }
    return 1;
}

int level_entity_store_write(uint32_t address, const void *input, size_t size) {
    size_t offset,i;
    if (!input || !view_offset(address,size,&offset)) return 0;
    for(i=0;i<size;++i) {
        size_t at=offset+i;unsigned shift=(unsigned)(at%4u)*8u;
        entities.ids[at/4u]=(entities.ids[at/4u]&~(255u<<shift))|
            (uint32_t)((const uint8_t *)input)[i]<<shift;
    }
    if (size) {
        size_t first=offset/sizeof(*entities.ids);
        size_t last=(offset+size-1u)/sizeof(*entities.ids);
        memset(entities.destroyed+first,0,last-first+1u);
    }
    return 1;
}

int level_entity_store_destroy_all(void *context,
    int (*destroy)(void *,uint32_t)) {
    uint32_t i;
    if (!entities.ids || !destroy) return 0;
    entities.stats.destroy_passes++;
    for (i=0;i<entities.count;i++) {
        uint32_t object_id=entities.ids[i];
        if (!object_id || entities.destroyed[i]) continue;
        if (!destroy(context,object_id)) return 0;
        entities.destroyed[i]=1u;
        entities.stats.destroyed++;
    }
    return 1;
}

int level_entity_store_track_guest(uint32_t object_id) {
    uint32_t i;
    if (!object_id || entities.guest_count == LEVEL_GUEST_OBJECT_CAPACITY)
        return 0;
    for (i = 0; i < entities.guest_count; ++i)
        if (entities.guest_objects[i].id == object_id) return 0;
    entities.guest_objects[entities.guest_count].id = object_id;
    entities.guest_objects[entities.guest_count].transferred = 0;
    ++entities.guest_count;
    ++entities.stats.tracked;
    ++entities.stats.pending;
    return 1;
}

int level_entity_store_transfer_guest(uint32_t object_id) {
    uint32_t i;
    for (i = 0; i < entities.guest_count; ++i) {
        LevelGuestObject *object = &entities.guest_objects[i];
        if (object->id != object_id || object->transferred) continue;
        object->transferred = 1;
        ++entities.stats.transferred;
        --entities.stats.pending;
        return 1;
    }
    return 0;
}

int level_entity_store_destroy_orphans(void *context,
    int (*destroy)(void *,uint32_t)) {
    uint32_t i;
    if (!destroy) return 0;
    for (i = entities.guest_count; i > 0; --i) {
        LevelGuestObject *object = &entities.guest_objects[i - 1u];
        if (!object->id || object->transferred) continue;
        if (!destroy(context, object->id)) return 0;
        object->id = 0;
        --entities.stats.pending;
        ++entities.stats.orphan_destroyed;
    }
    return 1;
}

void level_entity_store_clear_guest_objects(void) {
    memset(entities.guest_objects, 0, sizeof(entities.guest_objects));
    entities.guest_count = 0;
    entities.stats.pending = 0;
}

void level_entity_store_stats(LevelEntityStoreStats *output) {
    if (output) *output=entities.stats;
}
