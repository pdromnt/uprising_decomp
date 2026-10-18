#ifndef UPRISING_LEVEL_ENTITY_STORE_H
#define UPRISING_LEVEL_ENTITY_STORE_H

#include <stdint.h>
#include <stddef.h>

/* Legacy readers see a bounded address view; IDs are native-owned. */
#define LEVEL_ENTITY_VIEW_BASE 0xfe400000u
#define LEVEL_ENTITY_VIEW_END  0xfe420000u

typedef struct LevelEntityStoreStats {
    uint32_t loads, releases, live, count, destroyed, destroy_passes;
    uint32_t tracked, transferred, orphan_destroyed, pending;
} LevelEntityStoreStats;

/* One level is active at a time. IDs are opaque external object handles;
 * the backing array is native-owned and projected only for x86 callers. */
uint32_t level_entity_store_begin(uint32_t count);
uint32_t level_entity_store_view(void);
int level_entity_store_owns(uint32_t view);
int level_entity_store_release(uint32_t view);
void level_entity_store_shutdown(void);
int level_entity_store_get(uint32_t index,uint32_t *object_id);
int level_entity_store_set(uint32_t index,uint32_t object_id);
int level_entity_store_read(uint32_t address, void *output, size_t size);
int level_entity_store_write(uint32_t address, const void *input, size_t size);
/* Visits live objects in original slot order. IDs remain visible until the
 * table is freed, matching the external destructors' original call context. */
int level_entity_store_destroy_all(void *context,
    int (*destroy)(void *,uint32_t));
/* Objects made by record/effect factories start here before player ownership
 * is established. Once transferred, player-scene teardown owns destruction. */
int level_entity_store_track_guest(uint32_t object_id);
int level_entity_store_transfer_guest(uint32_t object_id);
int level_entity_store_destroy_orphans(void *context,
    int (*destroy)(void *,uint32_t));
void level_entity_store_clear_guest_objects(void);
void level_entity_store_stats(LevelEntityStoreStats *);

#endif
