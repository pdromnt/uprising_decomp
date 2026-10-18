#include "level_run.h"
#include "level_clock.h"
#include "menu_network.h"
#include "scene_projection_storage.h"
#include "effect_resource_pool.h"
#include "event_queue.h"
#include "menu_pause.h"
#include <string.h>
#include <stdio.h>
/*Clock packets use the distinct kind4 queue. These ordinary-C queue/network
 *providers remain required implementations, not event-pool aliases. */
int game_level_clock_queue_configure(GameApplication *a,GameAddress pool,uint32_t count,uint16_t kind,uint32_t refill,uint32_t capacity){
    return kind==4 && effect_resource_pool_initialize_clock_packets(&a->memory,pool,count,refill,capacity);
}
int game_level_clock_queue_destroy(GameApplication *a,GameAddress pool){return effect_resource_pool_destroy_owned(&a->memory,pool);}
int game_level_clock_queue_return_chain(GameApplication *a,GameAddress owner,GameAddress chain){
    EffectResourcePool *pool=effect_resource_pool_find(owner);
    return !chain || (pool && pool->object_kind==4 && effect_resource_pool_release_events(&a->memory,pool,chain)==1);
}
int game_level_clock_queue_return(GameApplication *a,GameAddress owner,GameAddress packet){return game_level_clock_queue_return_chain(a,owner,packet);}
extern uint32_t game_menu_milliseconds(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int signed_less(uint32_t x,uint32_t y){return (x^0x80000000u)<(y^0x80000000u);}
static EffectResource *clock_packet(GameApplication *a,GameAddress clock,GameAddress packet){
    EffectResource *r=effect_resource_find(packet);
    return r && !r->in_pool && r->owner->object_kind==4 &&
        r->owner->projection==get(a,clock+4) && r->clock_length<=r->clock_capacity?r:NULL;
}
static int packet_sequence(GameApplication *a,GameAddress clock,GameAddress packet,uint32_t *sequence){
    EffectResource *r=clock_packet(a,clock,packet);
    if(!r || r->clock_length<5)return 0;
    *sequence=get(a,r->clock_buffer+1);return !a->memory.fault;
}
/*43ef40: four ordered incoming lanes share the packet pool's existing nodes.
 * Sequence subtraction wraps; waiting-list comparisons retain signed ordering. */
int game_level_clock_received_packet(GameApplication *a,GameAddress clock,GameAddress packet){
    EffectResource *r=clock_packet(a,clock,packet);GameAddress head,tail,waiting,node,next;
    uint32_t lane,sequence,last,other;unsigned visits=0;
    if(!r || r->clock_sequence>=4 || !packet_sequence(a,clock,packet,&sequence))return 0;
    lane=r->clock_sequence;head=clock+0x2c+lane*4;tail=clock+0x3c+lane*4;
    waiting=clock+0x4c+lane*4;last=get(a,clock+0x5c+lane*4);
    if(last-sequence==UINT32_MAX){
        if(!get(a,head)){put(a,head,packet);put(a,tail,packet);}
        else {put(a,get(a,tail)+4,packet);put(a,packet+4,0);put(a,tail,packet);}
        put(a,clock+0x5c+lane*4,sequence);
        while((node=get(a,waiting))!=0){
            if(++visits>65536 || !packet_sequence(a,clock,node,&other))return 0;
            if(other-sequence!=1)break;
            next=get(a,node+4);put(a,waiting,next);put(a,get(a,tail)+4,node);
            put(a,node+4,0);put(a,tail,node);put(a,clock+0x5c+lane*4,++sequence);
        }
        return !a->memory.fault;
    }
    if(signed_less(sequence,last+1))return game_level_clock_queue_return(a,get(a,clock+4),packet);
    node=get(a,waiting);
    if(!node){put(a,waiting,packet);return !a->memory.fault;}
    if(!packet_sequence(a,clock,node,&other))return 0;
    if(signed_less(sequence,other)){put(a,packet+4,node);put(a,waiting,packet);return !a->memory.fault;}
    if(sequence==other)return game_level_clock_queue_return(a,get(a,clock+4),packet);
    for(;;){
        if(++visits>65536)return 0;
        next=get(a,node+4);
        if(!next){put(a,node+4,packet);put(a,packet+4,0);return !a->memory.fault;}
        if(!packet_sequence(a,clock,next,&other))return 0;
        if(sequence==other)return game_level_clock_queue_return(a,get(a,clock+4),packet);
        if(signed_less(sequence,other)){put(a,packet+4,next);put(a,node+4,packet);return !a->memory.fault;}
        node=next;
    }
}
static void clock_send_diagnostic(GameApplication *a,uint32_t status,uint8_t first){
    int32_t signed_status;char message[80];int length=0;memcpy(&signed_status,&status,4);
    switch(status){
    case 0x80070057:length=snprintf(message,sizeof(message),"Invalid params %d\n",(int)signed_status);break;
    case 0x88770082:length=snprintf(message,sizeof(message),"Invalid object %d\n",(int)signed_status);break;
    case 0x88770096:length=snprintf(message,sizeof(message),"Invalid player %d %d\n",first,(int)signed_status);break;
    case 0x8877010e:length=snprintf(message,sizeof(message),"busy %d\n",(int)signed_status);break;
    default:break;
    }
    if(length>0 && (unsigned)length<sizeof(message))game_memory_write(&a->memory,0x5a3888,message,(size_t)length+1);
}
/*440140's verified command-format table. Entries select native decoding, never
 * register/stack execution. All wire reads use bounded portable memory access. */
static const uint8_t command_format[55]={11,11,7,7,7,7,7,7,7,8,7,7,8,10,7,7,7,7,7,7,7,7,7,9,7,0,4,1,0,0,5,5,6,0,0,0,3,3,0,3,3,4,4,5,5,2,0,2,0,0,4,1,1,0,0};
/*43f660: decoded events transfer directly into the player's existing queue.
 * The pool owns each event before insertion and recovers it on failed publish. */
int game_level_clock_apply_packet(GameApplication *a,GameAddress clock,GameAddress packet){
    static const uint8_t extra[12]={0,1,4,2,2,8,8,4,5,8,12,5};
    static const uint8_t kinds[12]={1,2,3,12,4,5,5,2,6,6,7,1};
    EffectResource *r=clock_packet(a,clock,packet);EffectResourcePool *pool;
    GameAddress root,event,buffer;uint32_t pos=5,deadline=0,value,identity,lane,format,size,offset;
    uint8_t code,mask;unsigned i,count;int commands=0;
    if(!r || r->clock_sequence>=4 || r->clock_length<5)return 0;
    lane=r->clock_sequence;buffer=r->clock_buffer;root=get(a,0x5f0210+lane*4);
    if(!root || a->memory.fault)return 0;
    while(pos<r->clock_length){
        code=game_memory_read_u8(&a->memory,buffer+pos++);
        if(code==54){if(!game_level_pause_begin(a))return 0;continue;}
        if(r->clock_length-pos<4)return 0;
        value=get(a,buffer+pos);pos+=4;
        if(code==55){
            if(!game_level_pause_resume(a,value))return 0;
            /*43f6d4 skips the end-game signal when this flag is zero. */
            if(get(a,clock+0xa0)){put(a,clock+0xa0,0);if(!game_level_resume_presentation(a))return 0;}
            continue;
        }
        if(code>=55)return 0;
        deadline=value;
        format=command_format[code];size=extra[format];count=0;mask=0;
        if(r->clock_length-pos<size)return 0;
        if(format==11){
            mask=game_memory_read_u8(&a->memory,buffer+pos+4);
            for(i=0;i<8;++i)count+=(mask>>i)&1;
            size+=count*2;if(r->clock_length-pos<size)return 0;
        }
        identity=format>=7?get(a,buffer+pos):game_memory_read_u16(&a->memory,root+8);
        pool=effect_resource_pool_find(format==0 || format==7?0x5cee40:format==11?0x5ceee8:0x5cef90);
        if(!pool || a->memory.fault)return 0;
        event=effect_resource_pool_acquire_kind(&a->memory,pool,kinds[format]);if(!event)return 0;
        put(a,event+12,deadline);game_memory_write_u8(&a->memory,event+16,code);put(a,event+20,identity);
        offset=pos+(format>=7?4:0);
        if(format==11){
            game_memory_write_u8(&a->memory,event+24,mask);++offset;
            for(i=0;i<count;++i)game_memory_write_u16(&a->memory,event+26+i*2,game_memory_read_u16(&a->memory,buffer+offset+i*2));
        }else if(format!=0 && format!=7){
            for(i=0;i<size-(format>=7?4:0);++i)
                game_memory_write_u8(&a->memory,event+24+i,game_memory_read_u8(&a->memory,buffer+offset+i));
        }
        if(a->memory.fault || !game_root_event_receive(&a->memory,root,event)){
            GameMemory cleanup=a->memory;cleanup.fault=0;effect_resource_pool_return_chain(&cleanup,pool,event);return 0;
        }
        pos+=size;commands=1;
        if(code==25){
            uint32_t player=game_memory_read_u16(&a->memory,root+8),sequence;uint8_t ack[8]={0x43,0,0,0,0,0,0,0};
            if(player>=4)return 0;
            game_memory_write_u8(&a->memory,clock+0xa4,(uint8_t)(game_memory_read_u8(&a->memory,clock+0xa4)|(1u<<(player&7))));
            put(a,clock+0x90+player*4,game_menu_milliseconds(a)-get(a,0x5ac90c));
            sequence=get(a,clock+0x5c+player*4);for(i=0;i<4;++i)ack[4+i]=(uint8_t)(sequence>>(i*8));
            /*Original stack padding was unspecified; portable padding is zero. */
            clock_send_diagnostic(a,game_network_send_result(a,get(a,0x5f0038),get(a,root+0x3c0),ack,8),ack[0]);
        }
    }
    if(commands && !signed_less(deadline,get(a,clock+0x80+lane*4)))put(a,clock+0x80+lane*4,deadline);
    return !a->memory.fault;
}
int game_level_clock_refresh(GameApplication *a,GameAddress clock){
    unsigned lane,visits;GameAddress head,node;
    for(lane=0;lane<4;++lane){
        head=get(a,clock+0x2c+lane*4);visits=0;
        for(node=head;node;node=get(a,node+4))
            if(++visits>65536 || !game_level_clock_apply_packet(a,clock,node))return 0;
        if(head && !game_level_clock_queue_return_chain(a,get(a,clock+4),head))return 0;
        put(a,clock+0x2c+lane*4,0);put(a,clock+0x3c+lane*4,0);
    }
    return !a->memory.fault;
}
/*43fc60: submit the current owned packet, then move it to the retained sent
 *lane even after a transport error. That lane owns it until clock retirement. */
static int clock_submit_packet(GameApplication *a,GameAddress clock,GameAddress packet){
    GameAddress tail;EffectResource *record;const uint8_t *bytes;uint8_t empty=0;
    uint32_t status;
    record=clock_packet(a,clock,packet);if(!record)return 0;
    if(record->clock_length>record->clock_capacity)return 0;
    /* Borrow the allocation at its owned capacity; transport consumes only
     * the used prefix. The storage API requires the exact allocation size. */
    bytes=record->clock_capacity?
        scene_projection_storage_borrow_bytes(record->clock_buffer,record->clock_capacity):&empty;
    if(!bytes || a->memory.fault)return 0;
    status=game_network_send_result(a,get(a,0x5f0038),0,bytes,record->clock_length);
    clock_send_diagnostic(a,status,bytes[0]);
    if(!get(a,clock+0x24)){put(a,clock+0x24,packet);put(a,clock+0x28,packet);}
    else {tail=get(a,clock+0x28);put(a,tail+4,packet);put(a,packet+4,0);put(a,clock+0x28,packet);}
    put(a,clock+0x18,0);return !a->memory.fault;
}
int game_level_clock_pending(GameApplication *a,GameAddress clock,uint32_t *result){
    GameAddress packet;
    if(!result || a->memory.fault)return 0;
    *result=get(a,clock+0x14);if(*result)return !a->memory.fault;
    put(a,clock+0x10,(game_menu_milliseconds(a)-get(a,0x5ac90c))>>5);
    packet=get(a,clock+0x18);*result=packet;
    return packet?clock_submit_packet(a,clock,packet):!a->memory.fault;
}
int game_level_clock_packet_flush(GameApplication *a,GameAddress clock){uint32_t result;return game_level_clock_pending(a,clock,&result);}
static GameAddress clock_prepare_packet(GameApplication *a,GameAddress clock,uint32_t kind){
    EffectResourcePool *pool=effect_resource_pool_find(get(a,clock+4));EffectResource *r;GameAddress packet;uint32_t sequence;
    if(!pool || pool->object_kind!=4 || pool->packet_capacity<5)return 0;
    packet=effect_resource_pool_acquire_kind(&a->memory,pool,kind);r=effect_resource_find(packet);if(!r)return 0;
    sequence=get(a,clock+0x7c);game_memory_write_u8(&a->memory,r->clock_buffer,65);put(a,r->clock_buffer+1,sequence);
    put(a,packet+16,5);put(a,packet+4,0);put(a,clock+0x7c,sequence+1);put(a,clock+0x18,packet);
    return a->memory.fault?0:packet;
}
/*43e9c0: append one command atomically; a capacity rollover sends and retains
 *the old packet, then allocates the next sequence from the same owned pool. */
int game_level_clock_packet_enqueue(GameApplication *a,GameAddress clock,const uint8_t *data,uint32_t size){
    GameAddress packet=get(a,clock+0x18);EffectResource *r;EffectResourcePool *pool;
    if(a->memory.fault || (!data && size))return 0;
    pool=effect_resource_pool_find(get(a,clock+4));
    if(!pool || pool->object_kind!=4 || pool->packet_capacity<5 || size>pool->packet_capacity-5)return 0;
    if(!packet)packet=clock_prepare_packet(a,clock,0);
    r=clock_packet(a,clock,packet);if(!r)return 0;
    if(size>r->clock_capacity-r->clock_length){
        if(!clock_submit_packet(a,clock,packet))return 0;
        packet=clock_prepare_packet(a,clock,1);r=clock_packet(a,clock,packet);if(!r)return 0;
    }
    if(size && !game_memory_write(&a->memory,r->clock_buffer+r->clock_length,data,size))return 0;
    put(a,packet+16,r->clock_length+size);return !a->memory.fault;
}
/*4402d0: publish the existing scheduled event's wire form. Code25 flushes
 *before blocking later event publication, as the game-over caller requires. */
int game_level_clock_observe_event(GameApplication *a,GameAddress event){
    static const uint8_t extra[12]={0,1,4,2,2,8,8,4,5,8,12,5};
    GameAddress clock=get(a,0x5accd4);uint8_t bytes[26],code,mask;uint32_t deadline,format,size,offset;unsigned i,count=0;
    if(!clock || !event || a->memory.fault)return 0;
    if(get(a,clock+0x14))return !a->memory.fault;
    code=game_memory_read_u8(&a->memory,event+16);if(code>=54)return 0;
    format=command_format[code];size=5+extra[format];bytes[0]=code;deadline=get(a,event+12);
    for(i=0;i<4;++i)bytes[1+i]=(uint8_t)(deadline>>(i*8));
    offset=5;
    if(format>=7){uint32_t identity=get(a,event+20);for(i=0;i<4;++i)bytes[offset++]=(uint8_t)(identity>>(i*8));}
    if(format==11){
        mask=game_memory_read_u8(&a->memory,event+24);bytes[offset++]=mask;
        for(i=0;i<8;++i)count+=(mask>>i)&1;
        size+=count*2;
        for(i=0;i<count*2;++i)bytes[offset+i]=game_memory_read_u8(&a->memory,event+26+i);
    }else if(format!=0 && format!=7){
        for(i=0;i<size-offset;++i)bytes[offset+i]=game_memory_read_u8(&a->memory,event+24+i);
    }
    if(a->memory.fault || !game_memory_write(&a->memory,0x5ad8c8,bytes,size) || !game_level_clock_packet_enqueue(a,clock,bytes,size))return 0;
    if(code==25){if(!game_level_clock_packet_flush(a,clock))return 0;put(a,clock+0x14,1);}
    return !a->memory.fault;
}
typedef struct ClockBuild {GameApplication *a;GameAddress allocations[2];unsigned count;int pool_configured;} ClockBuild;
/*The native clock owner is singleton too. Pending construction retains its
 *allocation IDs until cleanup succeeds, rather than losing them on a failed
 *queue provider. It contains ownership metadata, not a second clock record. */
static ClockBuild building;
static int discard_build(GameApplication *a) {
    if(!building.count)return 1;
    if(building.a!=a)return 0;
    if(building.pool_configured) {
        if(!game_level_clock_queue_destroy(a,building.allocations[1]))return 0;
        building.pool_configured=0;
    }
    while(building.count) {
        if(!scene_projection_storage_free(building.allocations[building.count-1]))return 0;
        building.allocations[--building.count]=0;
    }
    building.a=NULL;return 1;
}
static int allocate(void *c,uint32_t bytes,uint32_t *result) {
    ClockBuild *b=c;if(b->count==2)return 0;
    *result=scene_projection_storage_allocate(bytes,"level clock record");
    if(!*result)return 0;
    b->allocations[b->count++]=*result;return 1;
}
static int initialize(void *c,uint32_t pool){ClockBuild *b=c;game_memory_write_u16(&b->a->memory,pool+8,UINT16_MAX);put(b->a,pool,0);return !b->a->memory.fault;}
static int configure(void *c,uint32_t pool,uint32_t count,uint32_t kind,uint32_t refill,uint32_t capacity) {
    ClockBuild *b=c;
    /*Remember the attempted queue so its own partial-allocation owner unwinds. */
    b->pool_configured=1;
    return game_level_clock_queue_configure(b->a,pool,count,(uint16_t)kind,refill,capacity);
}
static int project(void *c,uint32_t object,uint32_t pool,uint32_t channel) {
    ClockBuild *b=c;GameApplication *a=b->a;unsigned i;
    put(a,object,0x572fe8);put(a,object+4,pool);put(a,object+8,channel);put(a,object+0xc,3);
    for(i=0;i<8;++i){put(a,0x5ad9c8+i*4,0);put(a,0x5ad9e8+i*4,0);}
    for(i=0x10;i<=0x28;i+=4)put(a,object+i,0);
    for(i=0;i<4;++i) {
        put(a,object+0x2c+i*4,0);put(a,object+0x3c+i*4,0);put(a,object+0x4c+i*4,0);
        put(a,object+0x5c+i*4,UINT32_MAX);put(a,object+0x6c+i*4,UINT32_MAX);
        put(a,object+0x80+i*4,UINT32_MAX);put(a,object+0x90+i*4,0);
    }
    put(a,object+0x7c,0);put(a,object+0xa0,0);game_memory_write_u8(&a->memory,object+0xa4,0);
    return !a->memory.fault;
}
int game_level_clock_start(GameApplication *a) {
    GameAddress object;uint32_t bits;float smooth;
    LevelClockCreateServices s={&building,allocate,initialize,configure,project};
    if(!discard_build(a))return 0;
    building.a=a;
    if(level_clock_current()->active || get(a,0x5accd4))return 0;
    if(!get(a,0x5eff04)){level_clock_end();put(a,0x5accd4,0);return !a->memory.fault;}
    if(!level_clock_create(get(a,0x5efee8),&s,&object) || a->memory.fault) {
        (void)discard_build(a);
        return 0;
    }
    put(a,0x5accd4,object);put(a,0x5acc80,0);put(a,0x5ac918,0);put(a,0x5ac928,0);
    level_clock_begin(object,(int32_t)get(a,object+0xc));smooth=level_clock_current()->smooth_a;
    /*Ownership is now with level_clock_current and the actual clock+pool. */
    memset(&building,0,sizeof(building));
    memcpy(&bits,&smooth,4);put(a,0x5acc38,bits);return !a->memory.fault;
}
/*43e8b0/43e4f0: return the active packet and each linked lane before retiring
 *the queue, then reset the borrowed network service. Clear successful fields. */
static int retire(void *c,uint32_t o) {
    GameApplication *a=c;GameAddress pool=get(a,o+4),packet=get(a,o+0x18);unsigned i;
    if(packet){if(!game_level_clock_queue_return(a,pool,packet))return 0;put(a,o+0x18,0);}
    for(i=0x1c;i<=0x24;i+=8) {
        packet=get(a,o+i);
        if(packet){if(!game_level_clock_queue_return_chain(a,pool,packet))return 0;put(a,o+i,0);put(a,o+i+4,0);}
    }
    for(i=0;i<4;++i) {
        packet=get(a,o+0x2c+i*4);
        if(packet){if(!game_level_clock_queue_return_chain(a,pool,packet))return 0;put(a,o+0x2c+i*4,0);put(a,o+0x3c+i*4,0);}
        packet=get(a,o+0x4c+i*4);
        if(packet){if(!game_level_clock_queue_return_chain(a,pool,packet))return 0;put(a,o+0x4c+i*4,0);}
    }
    if(pool){if(!game_level_clock_queue_destroy(a,pool) || !scene_projection_storage_free(pool))return 0;put(a,o+4,0);}
    if(!a->network)return 0;
    /*43e4f0 requests transport close and ignores its HRESULT. An already
     *closed session must not strand the otherwise retired clock allocation. */
    (void)game_network_transport_close(a);
    return !a->memory.fault && scene_projection_storage_free(o);
}
static int sample(void *c,uint32_t *v){*v=game_menu_milliseconds(c);return 1;}
static int pending(void *c,uint32_t o,uint32_t *v){return game_level_clock_pending(c,o,v);}
static int refresh(void *c,uint32_t o){return game_level_clock_refresh(c,o);}
static int unpublish(void *c){GameApplication *a=c;put(a,0x5accd4,0);return !a->memory.fault;}
/*43e8b0/43e4f0: final-player retirement deletes the clock immediately, without
 *the frame drain used by ordinary level shutdown. Its existing owner retains
 *successful teardown steps until the borrowed publication is cleared. */
int game_level_clock_destroy(GameApplication *a){
    LevelClock *clock=level_clock_current();
    if(!discard_build(a))return 0;
    if(!clock->active)return get(a,0x5accd4)==0 && !a->memory.fault;
    if(clock->object!=get(a,0x5accd4))return 0;
    if(clock->stop_phase!=2){if(!retire(a,clock->object))return 0;clock->stop_phase=2;}
    if(!unpublish(a))return 0;
    level_clock_end();return !a->memory.fault;
}
int game_level_clock_stop(GameApplication *a) {
    LevelClockStopServices s={a,sample,pending,refresh,retire,unpublish};
    if(!discard_build(a))return 0;
    if(!level_clock_current()->active)return get(a,0x5accd4)==0 && !a->memory.fault;
    return level_clock_stop_pending(get(a,0x5ac90c),&s);
}
