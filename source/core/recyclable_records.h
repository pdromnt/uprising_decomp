#ifndef UPRISING_RECYCLABLE_RECORDS_H
#define UPRISING_RECYCLABLE_RECORDS_H
#include "game_memory.h"
/* Verified 0x24/0x58 records. IDs plus bounded offsets address bytes owned
 * here; no host pointer is encoded in a record or narrowed to uint32_t. */
#define RECYCLABLE_RECORD_BASE 0xf7000000u
#define RECYCLABLE_RECORD_END 0xf8000000u
typedef struct RecyclableRecordServices {
    void *context;
    GameAddress (*allocate)(void *,size_t);
    int (*release)(void *,GameAddress);
} RecyclableRecordServices;
typedef struct RecyclablePosition {float x,z;} RecyclablePosition;
RecyclablePosition recyclable_position_read(GameMemory *,GameAddress);
GameAddress recyclable_record_allocate(size_t);
int recyclable_record_release(GameAddress);
int recyclable_record_read(GameAddress,void *,size_t);
int recyclable_record_write(GameAddress,const void *,size_t);
void recyclable_record_shutdown(void);
int recyclable_records_preallocate(GameMemory *,uint32_t outer_count,uint32_t inner_count,const RecyclableRecordServices *);
GameAddress recyclable_inner_acquire(GameMemory *,GameAddress,uint8_t,uint8_t,const RecyclableRecordServices *);
GameAddress recyclable_outer_acquire(GameMemory *,GameAddress,const RecyclableRecordServices *);
int recyclable_outer_initialize(GameMemory *,GameAddress,const RecyclableRecordServices *);
int recyclable_outer_copy(GameMemory *,GameAddress,GameAddress,const RecyclableRecordServices *);
int recyclable_outer_clear(GameMemory *,GameAddress,const RecyclableRecordServices *);
int recyclable_outer_reset_payload(GameMemory *,GameAddress,const RecyclableRecordServices *);
int recyclable_outer_retire(GameMemory *,GameAddress,const RecyclableRecordServices *);
int recyclable_record_destroy(GameMemory *,GameAddress,unsigned,const RecyclableRecordServices *);
int recyclable_container_reset(GameMemory *,GameAddress,uint8_t,uint8_t,uint32_t,int,int,const RecyclableRecordServices *);
long double recyclable_node_distance(GameMemory *,GameAddress owner,GameAddress node);
int recyclable_container_insert(GameMemory *,GameAddress owner,GameAddress node,int indexed,const RecyclableRecordServices *);
GameAddress recyclable_outer_append(GameMemory *,GameAddress,uint8_t,uint8_t,uint32_t,uint32_t,const RecyclableRecordServices *);
int recyclable_container_keep(GameMemory *,GameAddress,GameAddress,uint32_t,uint32_t,const RecyclableRecordServices *);
uint32_t recyclable_route_begin(GameMemory *,GameAddress,uint8_t,uint8_t,uint8_t,uint8_t,GameAddress,uint32_t,const RecyclableRecordServices *);
uint32_t recyclable_route_continue(GameMemory *,GameAddress,GameAddress,const RecyclableRecordServices *);
int recyclable_route_reachable(GameMemory *,GameAddress,uint8_t,uint8_t,uint8_t,uint8_t,int *);
int recyclable_grid_accept(GameMemory *,GameAddress,GameAddress,const RecyclableRecordServices *);
uint32_t recyclable_grid_route_begin(GameMemory *,GameAddress,uint8_t,uint8_t,uint8_t,uint8_t,GameAddress,uint32_t,const RecyclableRecordServices *);
uint32_t recyclable_grid_route_continue(GameMemory *,GameAddress,GameAddress,const RecyclableRecordServices *);
uint32_t recyclable_grid_flood_begin(GameMemory *,GameAddress,uint8_t,uint8_t,uint32_t,GameAddress,const RecyclableRecordServices *);
uint32_t recyclable_grid_attack_begin(GameMemory *,GameAddress,uint8_t,uint8_t,uint8_t,uint8_t,uint32_t,GameAddress,uint32_t,uint32_t,uint32_t,uint32_t,const RecyclableRecordServices *);
int recyclable_grid_pick_route(GameMemory *,GameAddress,GameAddress,int32_t,int32_t *);
GameAddress recyclable_grid_random_route(GameMemory *,GameAddress);
uint32_t recyclable_spatial_cache_update(GameMemory *,GameAddress,GameAddress,RecyclablePosition,GameAddress,uint32_t,const RecyclableRecordServices *);
int recyclable_candidate_distances(GameMemory *,RecyclablePosition,GameAddress candidates,GameAddress distances);
uint32_t recyclable_visibility_update(GameMemory *,GameAddress owner,GameAddress selected,RecyclablePosition,GameAddress output,const RecyclableRecordServices *);
uint32_t recyclable_projection_update(GameMemory *,GameAddress owner,GameAddress selected,GameAddress output,const RecyclableRecordServices *);
int recyclable_projection_construct(GameMemory *,GameAddress,const RecyclableRecordServices *);
int recyclable_projection_allocate_arrays(GameMemory *,GameAddress,const RecyclableRecordServices *);
int recyclable_projection_destroy(GameMemory *,GameAddress,const RecyclableRecordServices *);
int recyclable_route_simplify(GameMemory *,GameAddress,uint32_t radius_bits,const RecyclableRecordServices *);
int recyclable_queued_transition(GameMemory *,GameAddress,const RecyclableRecordServices *);
int recyclable_projection_cancel(GameMemory *,GameAddress);
int recyclable_queued_root_transition(GameMemory *,GameAddress,const RecyclableRecordServices *);
#endif
