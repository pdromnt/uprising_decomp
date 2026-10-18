#ifndef UPRISING_EFFECT_RESOURCE_POOL_H
#define UPRISING_EFFECT_RESOURCE_POOL_H

#include "game_memory.h"
#include "game_state_ops.h"

/* Native contact/configuration records. Projections are borrowed ABI views. */
typedef struct EffectResourcePool EffectResourcePool;
typedef struct EffectResource EffectResource;
struct EffectResource {
    EffectResource *next, *allocation_next;
    struct EffectResourcePool *owner;
    GameAddress projection, vtable;
    uint32_t kind;
    GameAddress contact;
    float contact_vector[3];
    uint8_t type, reserved_type[3];
    uint32_t second, third, seventh, fifth, sixth, eighth;
    float fourth, thirteenth;
    uint16_t ninth, reserved_ninth, twelfth, reserved_twelfth;
    uint32_t tenth, eleventh, fourteenth;
    uint32_t deadline;
    uint8_t event_code, event_route, event_padding[6], packet_mask, packet_padding;
    uint32_t event_arguments[2];
    uint16_t packet_data[8];
    uint16_t packet_tail;
    GameAddress clock_buffer;
    uint32_t clock_length,clock_capacity,clock_sequence;
    int in_pool;
};
EffectResource *effect_resource_find(GameAddress projection);
int effect_resource_configure(EffectResource *,const GameStateRecordConfig *);
GameAddress effect_resource_pool_acquire_kind(GameMemory *, EffectResourcePool *, uint32_t);
/* 1 returned, 0 foreign, -1 malformed/faulted. Resets the entire owned chain. */
int effect_resource_pool_return_chain(GameMemory *, struct EffectResourcePool *, GameAddress);
/* Event pools retain or trim a whole released chain by the retail usage average. */
int effect_resource_pool_release_events(GameMemory *, EffectResourcePool *, GameAddress);
int effect_resource_pool_initialize_events(GameMemory *,GameAddress,uint32_t,uint16_t,uint32_t);
int effect_resource_pool_initialize_clock_packets(GameMemory *,GameAddress,uint32_t,uint32_t,uint32_t);
int effect_resource_pool_destroy_owned(GameMemory *,GameAddress);
struct EffectResourcePool {
    GameAddress projection;
    GameAddress free_head;
    EffectResource *head, *allocations;
    uint32_t initial_count;
    uint32_t refill_count;
    uint32_t allocated_count;
    uint32_t requested_kind;
    uint32_t live_by_kind[30];
    uint32_t free_count, release_sampled, last_used, previous_used;
    float average_used;
    uint16_t object_kind;
    uint32_t packet_capacity;
    int active;
};

EffectResourcePool *effect_resource_pool_find(GameAddress projection);
int effect_resource_pool_begin(GameMemory *memory, GameAddress projection,
    uint32_t initial_count, uint16_t object_kind, uint32_t refill_count);
int effect_resource_pool_prepare(GameMemory *memory, EffectResourcePool *pool,
    uint32_t kind);
int effect_resource_pool_add(GameMemory *memory, EffectResourcePool *pool,
    GameAddress resource);
int effect_resource_pool_refill(GameMemory *memory, EffectResourcePool *pool);
GameAddress effect_resource_pool_acquire(GameMemory *memory,
    EffectResourcePool *pool);
int effect_resource_pool_return(GameMemory *memory, EffectResourcePool *pool,
    GameAddress resource);
GameAddress effect_resource_pool_take_free(GameMemory *memory,
    EffectResourcePool *pool);
void effect_resource_pool_end(EffectResourcePool *pool);

#endif
