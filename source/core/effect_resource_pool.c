#include "effect_resource_pool.h"

#include <string.h>
#include <stdlib.h>
#include "scene_projection_storage.h"
#include "effect_state.h"

static int resource_read(void *,uint32_t,void *,size_t);
static int resource_write(void *,uint32_t,const void *,size_t);
EffectResource *effect_resource_find(GameAddress address) {
    return scene_projection_storage_state(address,resource_read);
}
static uint32_t resource_size(uint16_t kind){static const uint8_t sizes[]={28u,24u,32u,44u,28u,68u};return kind<=5u?sizes[kind]:0u;}
static int event_pool(const EffectResourcePool *pool){return pool && pool->object_kind>=1u && pool->object_kind<=3u;}
static int recyclable_pool(const EffectResourcePool *pool){return pool && pool->object_kind>=1u && pool->object_kind<=4u;}
static GameAddress resource_address(const EffectResource *record) {
    return record ? record->projection : 0u;
}
static void resource_encode(const EffectResource *r,uint32_t words[17]) {
    memset(words,0,68u);words[0]=r->vtable;words[1]=resource_address(r->next);words[2]=r->kind;
    if(r->owner->object_kind==4u){words[3]=r->clock_buffer;words[4]=r->clock_length;words[5]=r->clock_capacity;words[6]=r->clock_sequence;return;}
    if(r->owner->object_kind==0u){words[3]=r->contact;memcpy(words+4,r->contact_vector,12u);return;}
    if(event_pool(r->owner)){
        words[3]=r->deadline;((uint8_t *)words)[16]=r->event_code;((uint8_t *)words)[17]=r->event_route;
        memcpy((uint8_t *)words+18u,r->event_padding,6u);
        if(r->owner->object_kind==2u)memcpy(words+6u,r->event_arguments,8u);
        if(r->owner->object_kind==3u){
            ((uint8_t *)words)[24]=r->packet_mask;((uint8_t *)words)[25]=r->packet_padding;
            memcpy((uint8_t *)words+26u,r->packet_data,16u);memcpy((uint8_t *)words+42u,&r->packet_tail,2u);
        }
        return;
    }
    memcpy((uint8_t *)words+12u,&r->type,1u);memcpy((uint8_t *)words+13u,r->reserved_type,3u);
    words[4]=r->second;words[5]=r->third;words[6]=r->seventh;memcpy(words+7,&r->fourth,4u);
    words[8]=r->fifth;words[9]=r->sixth;words[10]=r->eighth;
    memcpy((uint8_t *)words+44u,&r->ninth,2u);memcpy((uint8_t *)words+46u,&r->reserved_ninth,2u);
    words[12]=r->tenth;words[13]=r->eleventh;
    memcpy((uint8_t *)words+56u,&r->twelfth,2u);memcpy((uint8_t *)words+58u,&r->reserved_twelfth,2u);
    memcpy(words+15,&r->thirteenth,4u);words[16]=r->fourteenth;
}
static int resource_decode(EffectResource *r,const uint32_t words[17]) {
    EffectResource *next=effect_resource_find(words[1]);
    if(words[1] && (!next || (next->owner!=r->owner &&
       !(event_pool(r->owner) && event_pool(next->owner)))))return 0;
    r->vtable=words[0];r->next=next;r->kind=words[2];
    if(r->owner->object_kind==4u){
        if(words[5]!=r->owner->packet_capacity || words[4]>words[5] ||
           (!words[5] && words[3]) || (r->clock_buffer && (r->clock_buffer!=words[3] || r->clock_capacity!=words[5])))return 0;
        r->clock_buffer=words[3];r->clock_length=words[4];r->clock_capacity=words[5];r->clock_sequence=words[6];return 1;}
    if(r->owner->object_kind==0u){r->contact=words[3];memcpy(r->contact_vector,words+4,12u);return 1;}
    if(event_pool(r->owner)){
        r->deadline=words[3];r->event_code=((const uint8_t *)words)[16];r->event_route=((const uint8_t *)words)[17];
        memcpy(r->event_padding,(const uint8_t *)words+18u,6u);
        if(r->owner->object_kind==2u)memcpy(r->event_arguments,words+6u,8u);
        if(r->owner->object_kind==3u){
            r->packet_mask=((const uint8_t *)words)[24];r->packet_padding=((const uint8_t *)words)[25];
            memcpy(r->packet_data,(const uint8_t *)words+26u,16u);memcpy(&r->packet_tail,(const uint8_t *)words+42u,2u);
        }
        return 1;
    }
    memcpy(&r->type,(const uint8_t *)words+12u,1u);memcpy(r->reserved_type,(const uint8_t *)words+13u,3u);
    r->second=words[4];r->third=words[5];r->seventh=words[6];memcpy(&r->fourth,words+7,4u);
    r->fifth=words[8];r->sixth=words[9];r->eighth=words[10];
    memcpy(&r->ninth,(const uint8_t *)words+44u,2u);memcpy(&r->reserved_ninth,(const uint8_t *)words+46u,2u);
    r->tenth=words[12];r->eleventh=words[13];
    memcpy(&r->twelfth,(const uint8_t *)words+56u,2u);memcpy(&r->reserved_twelfth,(const uint8_t *)words+58u,2u);
    memcpy(&r->thirteenth,words+15,4u);r->fourteenth=words[16];return 1;
}
static int resource_read(void *opaque,uint32_t offset,void *bytes,size_t size) {
    EffectResource *r=opaque;uint32_t words[17];uint32_t limit=resource_size(r->owner->object_kind);
    if(offset>=limit || size>limit-offset)return 0;
    resource_encode(r,words);memcpy(bytes,(uint8_t *)words+offset,size);return 1;
}
static int resource_write(void *opaque,uint32_t offset,const void *bytes,size_t size) {
    EffectResource *r=opaque;uint32_t words[17];uint32_t limit=resource_size(r->owner->object_kind);
    if(offset>=limit || size>limit-offset)return 0;
    resource_encode(r,words);memcpy((uint8_t *)words+offset,bytes,size);return resource_decode(r,words);
}
int effect_resource_configure(EffectResource *r,const GameStateRecordConfig *c) {
    if(!r || !c || r->owner->object_kind!=5u || r->in_pool)return 0;
    r->type=c->type;r->second=c->second;r->third=c->third;r->seventh=c->seventh;
    r->fourth=(float)c->fourth;r->fifth=c->fifth;r->sixth=c->sixth;r->eighth=c->eighth;
    r->ninth=c->ninth;r->tenth=c->tenth;r->eleventh=c->eleventh;r->twelfth=c->twelfth;
    r->thirteenth=(float)c->thirteenth;r->fourteenth=c->fourteenth;return 1;
}
static void resource_reset(EffectResource *r) {
    if(r->owner->object_kind==4u){uint8_t zeros[64]={0};uint32_t offset=0;
        while(offset<r->clock_capacity){size_t size=r->clock_capacity-offset;if(size>sizeof(zeros))size=sizeof(zeros);
            scene_projection_storage_write(r->clock_buffer+offset,zeros,size);offset+=(uint32_t)size;}
        r->clock_length=0;r->clock_sequence=UINT32_MAX;return;}
    if(r->owner->object_kind==0u){r->contact=0u;memset(r->contact_vector,0,12u);return;}
    if(event_pool(r->owner)){
        r->deadline=0u;r->event_code=0u;
        if(r->owner->object_kind==2u)memset(r->event_arguments,0,8u);
        if(r->owner->object_kind==3u){r->packet_mask=0u;memset(r->packet_data,0,16u);}
        return;
    }
    r->type=0u;r->second=r->third=r->seventh=r->fifth=r->sixth=r->eighth=0u;
    r->fourth=r->thirteenth=0.0f;r->ninth=0u;r->tenth=r->eleventh=0u;r->twelfth=1u;
    /* Retail deliberately retains padding and the final configuration word. */
}
static EffectResourcePool pools[5];

typedef struct DynamicPool {EffectResourcePool pool;struct DynamicPool *next;} DynamicPool;
static DynamicPool *clock_pools;
static EffectResourcePool *slot(GameAddress address) {
    DynamicPool *entry;
    if (address == 0x005ad7d0u) return &pools[0];
    if (address == 0x00631910u) return &pools[1];
    if(address==0x005cee40u)return &pools[2];
    if(address==0x005cef90u)return &pools[3];
    if(address==0x005ceee8u)return &pools[4];
    for(entry=clock_pools;entry;entry=entry->next)if(entry->pool.projection==address)return &entry->pool;
    return NULL;
}

EffectResourcePool *effect_resource_pool_find(GameAddress projection) {
    EffectResourcePool *pool = slot(projection);
    return pool && pool->active ? pool : NULL;
}

int effect_resource_pool_begin(GameMemory *memory, GameAddress projection,
        uint32_t initial_count, uint16_t object_kind, uint32_t refill_count) {
    EffectResourcePool *pool = slot(projection);
    unsigned i;
    if(!pool && object_kind==4u && memory && !memory->fault){DynamicPool *entry=calloc(1,sizeof(*entry));
        if(!entry)return 0;
        entry->pool.projection=projection;entry->next=clock_pools;clock_pools=entry;pool=&entry->pool;}
    if (!memory || !pool || pool->active || (!resource_size(object_kind)))
        return 0;
    memset(pool, 0, sizeof(*pool));
    pool->projection = projection;
    pool->initial_count = initial_count;
    pool->refill_count = refill_count;
    pool->object_kind = object_kind;
    pool->requested_kind = UINT32_MAX;
    pool->active = 1;
    if(recyclable_pool(pool)){
        pool->free_count=initial_count;pool->release_sampled=1u;pool->last_used=initial_count;
        pool->average_used=(float)(int32_t)initial_count;
        game_memory_write_u32(memory,projection+0x8cu,1u);game_memory_write_u32(memory,projection+0x90u,initial_count);
        game_memory_write(memory,projection+0x94u,&pool->average_used,4u);
        game_memory_write_u32(memory,projection+0x98u,initial_count);game_memory_write_u32(memory,projection+0x9cu,0u);
        game_memory_write_u32(memory,projection+0xa0u,0u);
    }
    game_memory_write_u32(memory, projection, 0u);
    game_memory_write_u32(memory, projection + 4u, initial_count);
    game_memory_write_u16(memory, projection + 8u, object_kind);
    game_memory_write_u32(memory, projection + 12u, refill_count);
    for (i = 0; i < 30u; ++i)
        game_memory_write_u32(memory, projection + 0x10u + i * 4u, 0u);
    game_memory_write_u32(memory, projection + 0x88u, UINT32_MAX);
    return !memory->fault;
}

int effect_resource_pool_prepare(GameMemory *memory, EffectResourcePool *pool,
        uint32_t kind) {
    if (!memory || !pool || !pool->active || kind >= 30u) return 0;
    pool->requested_kind = kind;
    game_memory_write_u32(memory, pool->projection + 0x88u, kind);
    return !memory->fault;
}

int effect_resource_pool_add(GameMemory *memory, EffectResourcePool *pool,
        GameAddress resource) {
    if (!memory || !pool || !pool->active || !resource) return 0;
    EffectResource *record=calloc(1u,sizeof(*record));uint32_t words[17]={0u};unsigned i;
    if(!record)return 0;
    record->owner=pool;record->projection=resource;
    for(i=0u;i<resource_size(pool->object_kind)/4u;i++)words[i]=game_memory_read_u32(memory,resource+i*4u);
    if(memory->fault || !resource_decode(record,words) ||
       !scene_projection_storage_bind_state(resource,record,resource_read,resource_write)) {free(record);return 0;}
    record->next=pool->head;record->in_pool=1;record->allocation_next=pool->allocations;
    pool->allocations=record;pool->head=record;pool->free_head=resource;
    ++pool->allocated_count;
    game_memory_write_u32(memory, pool->projection, resource);
    return !memory->fault;
}

int effect_resource_pool_refill(GameMemory *memory, EffectResourcePool *pool) {
    if (!memory || !pool || !pool->active || pool->free_head) return 0;
    pool->initial_count += pool->refill_count;
    if(recyclable_pool(pool)){pool->free_count+=pool->refill_count;game_memory_write_u32(memory,pool->projection+0x90u,pool->free_count);}
    game_memory_write_u32(memory, pool->projection + 4u,
        pool->initial_count);
    return !memory->fault;
}

GameAddress effect_resource_pool_acquire(GameMemory *memory,
        EffectResourcePool *pool) {
    GameAddress resource, next;
    uint32_t kind;
    if (!memory || !pool || !pool->active || !pool->free_head ||
        pool->requested_kind >= 30u) return 0;
    if(recyclable_pool(pool)){
        if(pool->release_sampled){pool->previous_used=pool->initial_count-pool->free_count;game_memory_write_u32(memory,pool->projection+0x9cu,pool->previous_used);}
        pool->release_sampled=0u;--pool->free_count;
        game_memory_write_u32(memory,pool->projection+0x8cu,0u);game_memory_write_u32(memory,pool->projection+0x90u,pool->free_count);
    }
    resource = pool->free_head;
    next = resource_address(pool->head->next);
    if (memory->fault) return 0;
    kind = pool->requested_kind;
    pool->head=pool->head->next;
    pool->free_head = next;
    effect_resource_find(resource)->in_pool=0;
    pool->requested_kind = UINT32_MAX;
    ++pool->live_by_kind[kind];
    game_memory_write_u32(memory, pool->projection, next);
    game_memory_write_u32(memory, resource + 4u, 0u);
    game_memory_write_u32(memory, resource + 8u, kind);
    game_memory_write_u32(memory, pool->projection + 0x10u + kind * 4u,
        pool->live_by_kind[kind]);
    game_memory_write_u32(memory, pool->projection + 0x88u, UINT32_MAX);
    return memory->fault ? 0 : resource;
}

static GameAddress resource_create(GameMemory *memory,EffectResourcePool *pool){
    GameAddress address=scene_projection_storage_allocate(resource_size(pool->object_kind),"pooled resource"),buffer=0;
    if(!address)return 0;
    if(pool->object_kind==4u){
        if(pool->packet_capacity){buffer=scene_projection_storage_allocate(pool->packet_capacity,"clock packet bytes");if(!buffer)goto fail;}
        game_memory_write_u32(memory,address,0x572fb8);game_memory_write_u32(memory,address+4,0);
        game_memory_write_u32(memory,address+12,buffer);game_memory_write_u32(memory,address+16,0);
        game_memory_write_u32(memory,address+20,pool->packet_capacity);game_memory_write_u32(memory,address+24,UINT32_MAX);
    }else if(!game_effect_resource_initialize(memory,address,pool->object_kind))goto fail;
    if(!effect_resource_pool_add(memory,pool,address))goto fail;
    return address;
fail:if(buffer)scene_projection_storage_free(buffer);scene_projection_storage_free(address);return 0;
}
GameAddress effect_resource_pool_acquire_kind(GameMemory *memory,EffectResourcePool *pool,uint32_t kind) {
    unsigned i;
    if(!effect_resource_pool_prepare(memory,pool,kind))return 0u;
    if(!pool->head){
        if(!effect_resource_pool_refill(memory,pool))return 0u;
        for(i=0u;i<pool->refill_count;i++){
            if(!resource_create(memory,pool)){memory->fault=1;return 0u;}
        }
    }
    return effect_resource_pool_acquire(memory,pool);
}
int effect_resource_pool_return(GameMemory *memory, EffectResourcePool *pool,
        GameAddress resource) {
    EffectResource *record=effect_resource_find(resource);uint32_t kind;
    if(recyclable_pool(pool)){
        EffectResource *next;
        int result;
        if(!record || record->owner!=pool || record->in_pool)return 0;
        next=record->next;record->next=NULL;
        result=effect_resource_pool_release_events(memory,pool,resource);
        if(result!=1 && effect_resource_find(resource)==record && !record->in_pool)record->next=next;
        return result==1;
    }
    if(!memory || memory->fault || !pool || !pool->active || !record || record->owner!=pool || record->in_pool)return 0;
    kind=record->kind;if(kind>=30u || !pool->live_by_kind[kind])return 0;
    resource_reset(record);record->next=pool->head;record->in_pool=1;
    pool->head=record;pool->free_head=resource;--pool->live_by_kind[kind];
    game_memory_write_u32(memory,pool->projection,resource);
    game_memory_write_u32(memory,pool->projection+0x10u+kind*4u,pool->live_by_kind[kind]);
    return !memory->fault;
}
int effect_resource_pool_return_chain(GameMemory *memory,EffectResourcePool *pool,GameAddress address) {
    EffectResource *first=effect_resource_find(address),*cursor,*last=NULL;uint32_t counts[30]={0u},visits=0u;unsigned i;
    if(recyclable_pool(pool))return effect_resource_pool_release_events(memory,pool,address);
    if(!memory || memory->fault)return -1;
    if(!address)return 1;
    if(!pool || !first || first->owner!=pool)return 0;
    for(cursor=first;cursor;cursor=cursor->next){
        if(cursor->owner!=pool || cursor->in_pool || cursor->kind>=30u || ++visits>pool->allocated_count)goto malformed;
        ++counts[cursor->kind];last=cursor;
    }
    for(i=0u;i<30u;i++)if(counts[i]>pool->live_by_kind[i])goto malformed;
    for(cursor=first;cursor;cursor=cursor->next){resource_reset(cursor);cursor->in_pool=1;}
    last->next=pool->head;pool->head=first;pool->free_head=address;
    for(i=0u;i<30u;i++)if(counts[i]){pool->live_by_kind[i]-=counts[i];game_memory_write_u32(memory,pool->projection+0x10u+i*4u,pool->live_by_kind[i]);}
    game_memory_write_u32(memory,pool->projection,address);return memory->fault?-1:1;
malformed:
    memory->fault=1;return -1;
}

static int event_publish(GameMemory *memory,EffectResourcePool *pool){
    game_memory_write_u32(memory,pool->projection,pool->free_head);
    game_memory_write_u32(memory,pool->projection+4u,pool->initial_count);
    game_memory_write_u32(memory,pool->projection+0x8cu,pool->release_sampled);
    game_memory_write_u32(memory,pool->projection+0x90u,pool->free_count);
    game_memory_write(memory,pool->projection+0x94u,&pool->average_used,4u);
    game_memory_write_u32(memory,pool->projection+0x98u,pool->last_used);
    return !memory->fault;
}
static int resource_delete(EffectResourcePool *pool,EffectResource *record){
    EffectResource **link=&pool->allocations;
    while(*link && *link!=record)link=&(*link)->allocation_next;
    if(!*link)return 0;
    if(record->clock_buffer && !scene_projection_storage_free(record->clock_buffer))return 0;
    record->clock_buffer=0;
    if(!scene_projection_storage_free(record->projection))return 0;
    *link=record->allocation_next;free(record);--pool->allocated_count;--pool->initial_count;return 1;
}
int effect_resource_pool_release_events(GameMemory *memory,EffectResourcePool *pool,GameAddress address){
    EffectResource *first=effect_resource_find(address),*cursor,*last=NULL;uint32_t visits=0u,counts[30]={0u};unsigned i;
    int trim;
    if(!memory || memory->fault || !recyclable_pool(pool))return -1;
    if(!address)return 1;
    if(!first || first->owner!=pool)return 0;
    for(cursor=first;cursor;cursor=cursor->next){
        if(cursor->owner!=pool || cursor->in_pool || cursor->kind>=30u || ++visits>pool->allocated_count)goto failed;
        ++counts[cursor->kind];last=cursor;
    }
    if(!pool->release_sampled){
        volatile long double weighted=(long double)pool->average_used*9.0L;
        pool->last_used=pool->initial_count-pool->free_count;
        pool->average_used=(float)((weighted+(int32_t)pool->last_used)*(long double)0.1f);
    }
    pool->release_sampled=1u;
    trim=(long double)(int32_t)pool->free_count>(long double)pool->average_used*2.0L;
    if(trim){
        cursor=first;
        while(cursor){EffectResource *next=cursor->next;if(!resource_delete(pool,cursor))goto failed;cursor=next;}
        /* Retail's trimming branch intentionally leaves per-kind counters unchanged. */
    }else {
        for(i=0u;i<30u;i++)if(counts[i]>pool->live_by_kind[i])goto failed;
        for(cursor=first;cursor;cursor=cursor->next){resource_reset(cursor);cursor->in_pool=1;}
        last->next=pool->head;pool->head=first;pool->free_head=address;pool->free_count+=visits;
        for(i=0u;i<30u;i++)if(counts[i]){pool->live_by_kind[i]-=counts[i];game_memory_write_u32(memory,pool->projection+0x10u+i*4u,pool->live_by_kind[i]);}
    }
    return event_publish(memory,pool)?1:-1;
failed:
    memory->fault=1;return -1;
}
int effect_resource_pool_initialize_events(GameMemory *memory,GameAddress owner,uint32_t count,uint16_t kind,uint32_t refill){
    EffectResourcePool *pool;unsigned i;
    if(kind<1u || kind>3u || !effect_resource_pool_begin(memory,owner,count,kind,refill))return 0;
    pool=effect_resource_pool_find(owner);
    for(i=0u;i<count;i++){
        GameAddress address=scene_projection_storage_allocate(resource_size(kind),"event resource");
        if(!address || !game_effect_resource_initialize(memory,address,kind) || !effect_resource_pool_add(memory,pool,address)){
            if(address)scene_projection_storage_free(address);
            memory->fault=1;return 0;
        }
    }
    return !memory->fault;
}

/*48f380 ->43e250 allocation,48f660 reuse and43e360 full buffer reset.
 *Kind4 uses the same authoritative native pool as the existing event families. */
int effect_resource_pool_initialize_clock_packets(GameMemory *memory,GameAddress owner,uint32_t count,uint32_t refill,uint32_t capacity){
    EffectResourcePool *pool;uint32_t i;
    if(!memory || memory->fault || !owner || count>65536u || refill>65536u || capacity>1048576u ||
       !effect_resource_pool_begin(memory,owner,count,4,refill))return 0;
    pool=effect_resource_pool_find(owner);pool->packet_capacity=capacity;
    game_memory_write_u32(memory,owner+0xa0,capacity);
    for(i=0;i<count;++i)if(!resource_create(memory,pool)){memory->fault=1;return 0;}
    return !memory->fault;
}
int effect_resource_pool_destroy_owned(GameMemory *memory,GameAddress owner){
    EffectResourcePool *pool=effect_resource_pool_find(owner);EffectResource *record;
    if(!pool)return 1;
    /*Active packet lanes return first. Partial construction preserves every
     *successful allocation in this owner so normal failure cleanup can retire it. */
    for(record=pool->allocations;record;record=record->allocation_next)if(!record->in_pool)return 0;
    while(pool->allocations)if(!resource_delete(pool,pool->allocations))return 0;
    pool->head=NULL;pool->free_head=0;pool->free_count=0;
    {GameMemory cleanup=*memory;cleanup.fault=0;game_memory_write_u32(&cleanup,owner,0);if(cleanup.fault)return 0;}
    effect_resource_pool_end(pool);return 1;
}
GameAddress effect_resource_pool_take_free(GameMemory *memory,
        EffectResourcePool *pool) {
    GameAddress resource, next;
    if (!memory || !pool || !pool->active || !pool->free_head) return 0;
    resource = pool->free_head;
    next = resource_address(pool->head->next);
    if (memory->fault) return 0;
    pool->head=pool->head->next;
    pool->free_head = next;
    game_memory_write_u32(memory, pool->projection, next);
    return memory->fault ? 0 : resource;
}

void effect_resource_pool_end(EffectResourcePool *pool) {
    if(pool){DynamicPool **link=&clock_pools;EffectResource *record=pool->allocations;
        while(record){EffectResource *next=record->allocation_next;free(record);record=next;}memset(pool,0,sizeof(*pool));
        while(*link){if(&(*link)->pool==pool){DynamicPool *entry=*link;*link=entry->next;free(entry);break;}link=&(*link)->next;}}
}
