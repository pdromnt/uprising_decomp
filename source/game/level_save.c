#include "level_save.h"
#include "level_routes.h"
#include "level_load.h"
#include "level_objects.h"
#include "application_archives.h"
#include "tagged_scalar_stream.h"
#include "gameplay_state.h"
#include "presentation_instance.h"
#include "player_scene_children.h"
#include "level_player_scene.h"
#include "level_factory.h"
#include "effect_model.h"
#include "runtime_node_pool.h"
#include "gameplay_catalog.h"
#include "game_intrusive_list.h"
#include "level_player_resources.h"
#include "level_run.h"
#include "radar_compositor.h"
#include "menu_sound.h"
#include "scene_projection_storage.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

/* These are real remaining virtual writers, not CPU adapters or successful
 * placeholders. Shipping cannot link until their complete behavior is native. */
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int aircraft_archive_sound(GameApplication *a,GameAddress p){
    GameAddress reference=get(a,0x58f890);SoundPlaybackRequest sound={0};
    long double x=(long double)real(a,p+24)-real(a,reference+12),z=(long double)real(a,p+32)-real(a,reference+20);
    sound.sound=24;sound.volume=1;sound.flags=sound.force=1;sound.distance_squared=(float)(x*x+z*z);
    if(a->memory.fault || !game_menu_sound_play(a,&sound))return 0;
    game_memory_write_u32(&a->memory,p+0x180,get(a,get(a,0x628a90)));return !a->memory.fault;
}
static int write_port(void *f,const uint8_t *b,size_t width,size_t count){
    return width && count<=SIZE_MAX/width && resource_file_write(f,b,width*count)==width*count;
}
static int flush_port(void *f){return resource_file_flush(f);}
static int record(ResourceFile *f,uint32_t tag,const uint8_t *b,size_t width,size_t count){
    uint32_t marker;TaggedRecordStreamServices s={f,NULL,write_port,flush_port};
    return tagged_record_stream_write_elements(&marker,tag,b,width,count,&s);
}
static int scalar(ResourceFile *f,uint32_t tag,uint32_t value,unsigned width){
    uint8_t b[4];unsigned i;for(i=0;i<width;i++)b[i]=(uint8_t)(value>>(i*8));
    return record(f,tag,b,width,1);
}
static int u32(ResourceFile *f,uint32_t v){return scalar(f,0xfedcba92u,v,4);}
static int f32(ResourceFile *f,uint32_t v){return scalar(f,0xfedcba93u,v,4);}
static int u8(ResourceFile *f,uint32_t v){return scalar(f,0xfedcba91u,v,1);}
static int raw_word(ResourceFile *f,uint32_t word){
    uint8_t bytes[4];unsigned i;for(i=0;i<4;i++)bytes[i]=(uint8_t)(word>>(i*8));
    return resource_file_write(f,bytes,4)==4;
}
static int raw_field(GameApplication *a,ResourceFile *f,GameAddress p,size_t size){
    uint8_t bytes[0x60];return size<=sizeof(bytes) && game_memory_read(&a->memory,p,bytes,size) && resource_file_write(f,bytes,size)==size;
}
/* Canonical byte access already defines endian and alignment. Only transient
 * output bytes are allocated; no serialized mirror is retained or synchronized. */
static int memory_record(GameApplication *a,ResourceFile *f,uint32_t tag,GameAddress p,size_t width,size_t count){
    uint8_t *bytes;size_t size;int ok;
    if(!width || count>SIZE_MAX/width)return 0;
    size=width*count;if(size>UINT32_MAX || size>UINT32_MAX-p)return 0;
    bytes=malloc(size?size:1);if(!bytes)return 0;
    ok=game_memory_read(&a->memory,p,bytes,size) && record(f,tag,bytes,width,count);
    free(bytes);return ok;
}
/* Validate sibling lists before counting/writing, including malformed cycles. */
static int list_count(GameApplication *a,GameAddress first,unsigned next,uint32_t *count){
    GameAddress slow=first,fast=first,p=first;uint32_t n=0;
    while(p){if(n==INT32_MAX)return 0;++n;p=get(a,p+next);
        slow=slow?get(a,slow+next):0;
        fast=fast?get(a,fast+next):0;fast=fast?get(a,fast+next):0;
        if(a->memory.fault || (slow && slow==fast))return 0;
    }*count=n;return 1;
}
/*4cf190: current order records keep their live linked-list owner. */
static int order_save(GameApplication *a,GameAddress p,ResourceFile *f){
    return u32(f,get(a,p+0xc)) && scalar(f,0xfedcba90u,get(a,p+0x14),4) &&
        u32(f,get(a,p+0x24)) && u32(f,get(a,p+0x28)) && u32(f,get(a,p+0x2c)) &&
        memory_record(a,f,0xfedcba9bu,p+0x18,4,3);
}
/*53c640 saves a finite prefix and the original ring traversal count. The ring
 *count excludes the return-to-start transition; preserve that format exactly.
 *Malformed cycles without flag1 fail at the same bounded traversal limit used
 *by existing native list codecs, rather than spinning in shipping. */
static int vehicle_save(GameApplication *a,GameAddress p,ResourceFile *f){
    TaggedRecordStreamServices records={f,NULL,write_port,flush_port};
    PresentationArchiveServices services={&records,NULL,NULL,NULL};
    uint32_t marker=0,prefix=0,ring=0,total,i;GameAddress first=get(a,p+0xdc),node=first,start;
    if(!u8(f,game_memory_read_u8(&a->memory,p+0xd8)) ||
       !presentation_instance_stream(&a->memory,p,0,1,&marker,&services) ||
       !f32(f,get(a,p+0xb4)) || !f32(f,get(a,p+0xb8)) ||
       !memory_record(a,f,0xfedcba9bu,p+0xbc,4,3) ||
       !scalar(f,0xfedcba90u,get(a,p+0xcc),4) || !scalar(f,0xfedcba90u,get(a,p+0xd0),4) ||
       !u32(f,get(a,p+0xd4)))return 0;
    while(node && !(get(a,node+0x2c)&1)){
        if(prefix==65536)return 0;
        ++prefix;node=get(a,node+4);if(a->memory.fault)return 0;
    }
    start=node;
    if(start){
        do{node=get(a,node+4);if(node==start)break;if(ring==65536)return 0;++ring;}while(node && !a->memory.fault);
    }
    if(a->memory.fault || ring>INT32_MAX-prefix || !u32(f,prefix) || !u32(f,ring))return 0;
    total=prefix+ring;node=first;
    for(i=0;i<total;i++){if(!node || !order_save(a,node,f))return 0;node=get(a,node+4);}
    if(!u32(f,get(a,p+0xe0)) || !memory_record(a,f,0xfedcba9bu,p+0xe4,4,3) ||
       !scalar(f,0xfedcba90u,get(a,p+0xf4),4) || !memory_record(a,f,0xfedcba9bu,p+0xf8,4,3))return 0;
    node=get(a,p+0x104);if(!u32(f,node?get(a,node):UINT32_MAX))return 0;
    for(i=0;i<2;i++){node=get(a,p+0x108+i*4);if(!scalar(f,0xfedcba90u,node?get(a,node+0x14):0,4))return 0;}
    for(i=0;i<3;i++)if(!u32(f,get(a,p+0x12c+i*4)))return 0;
    node=get(a,p+0x138);return scalar(f,0xfedcba90u,node?get(a,node+0x14):0,4) && !a->memory.fault;
}
static int attachment_word(void *f,uint32_t value){return u32(f,value);}
static int read_port(void *f,uint8_t *bytes,size_t width,size_t count,uint32_t *elements){
    size_t n;if(!width || count>SIZE_MAX/width || count>UINT32_MAX)return 0;
    n=resource_file_read(f,bytes,width*count);*elements=(uint32_t)(n/width);return 1;
}
static int restore_bytes(GameApplication *a,ResourceFile *f,uint8_t *bytes,size_t width,size_t count){
    TaggedRecordStreamServices s={f,read_port,NULL,NULL};uint32_t marker=get(a,0x63c964),header,elements;
    if(!tagged_record_stream_read_elements(&marker,bytes,width,count,&header,&elements,&s))return 0;
    game_memory_write_u32(&a->memory,0x63c964,marker);game_memory_write_u32(&a->memory,0x59d4b8,header);
    return !a->memory.fault;
}
static int restore_field(GameApplication *a,ResourceFile *f,GameAddress p,size_t width,size_t count){
    uint8_t bytes[12];size_t size=width*count;
    if(size>sizeof(bytes) || !game_memory_read(&a->memory,p,bytes,size) || !restore_bytes(a,f,bytes,width,count))return 0;
    return game_memory_write(&a->memory,p,bytes,size);
}
static int restore_word(GameApplication *a,ResourceFile *f,uint32_t *word){
    uint8_t bytes[4];unsigned i;for(i=0;i<4;i++)bytes[i]=(uint8_t)(*word>>(i*8));
    if(!restore_bytes(a,f,bytes,4,1))return 0;
    *word=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8|(uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;return 1;
}
static int restore_raw_field(GameApplication *a,ResourceFile *f,GameAddress p,size_t size){
    uint8_t bytes[0x60];
    if(size>sizeof(bytes) || !game_memory_read(&a->memory,p,bytes,size))return 0;
    (void)resource_file_read(f,bytes,size);
    return game_memory_write(&a->memory,p,bytes,size);
}
/* Bounded tagged record access for the original strategy layouts. These are
 * archive fields, not an execution format; live allocations keep their owners. */
static int strategy_field(GameApplication *a,ResourceFile *f,int reading,GameAddress p,
    uint32_t tag,size_t width,size_t count){
    uint8_t *bytes;size_t size;int ok;
    if(!reading)return memory_record(a,f,tag,p,width,count);
    if(!width || count>UINT32_MAX/width)return 0;size=width*count;
    bytes=malloc(size?size:1);if(!bytes)return 0;
    ok=game_memory_read(&a->memory,p,bytes,size) && restore_bytes(a,f,bytes,width,count) &&
        game_memory_write(&a->memory,p,bytes,size);free(bytes);return ok;
}
static int strategy_words(GameApplication *a,ResourceFile *f,int reading,GameAddress p,unsigned count){
    unsigned i;for(i=0;i<count;i++)if(!strategy_field(a,f,reading,p+i*4,0xfedcba92,4,1))return 0;
    return !a->memory.fault;
}
static int strategy_outer(GameApplication *a,ResourceFile *f,int reading,GameAddress p){
    TaggedRecordStreamServices records={f,reading?read_port:NULL,reading?NULL:write_port,reading?NULL:flush_port};
    GameplayContainerArchiveServices services={&records,NULL,NULL,NULL,NULL};
    uint32_t marker=get(a,0x63c964);int ok=gameplay_state_stream_outer(&a->memory,reading,&marker,&services,p);
    if(reading)game_memory_write_u32(&a->memory,0x63c964,marker);return ok && !a->memory.fault;
}
/* Citadel references are indices into the already loaded map owner. Entity
 * references remain IDs until the final catalog fix pass. */
static int strategy_reference(GameApplication *a,ResourceFile *f,int reading,GameAddress slot,int citadel){
    GameAddress target=get(a,slot);uint32_t identity;
    if(!reading)return scalar(f,citadel==2?0xfedcba92:0xfedcba90,
        target?get(a,target+(citadel?0:0x14)):(citadel?UINT32_MAX:0),4);
    identity=citadel?UINT32_MAX:0;if(!restore_word(a,f,&identity))return 0;
    target=identity;
    if(citadel){
        if(identity==UINT32_MAX)target=0;
        else {uint32_t count=game_memory_read_u16(&a->memory,0x639a74);
            if(count>=32768 || identity>=count)return 0;target=get(a,get(a,0x639a70)+identity*4);}
    }
    game_memory_write_u32(&a->memory,slot,target);return !a->memory.fault;
}
/*4424b0/442690: embedded search state, retained path and map references. */
static int strategy_projection(GameApplication *a,ResourceFile *f,int reading,GameAddress p,unsigned sites){
    unsigned i,j;static const unsigned float_fields[]={0x24,0x28,0x38};
    for(i=4;i<=0x3c;i+=4){uint32_t tag=0xfedcba92;
        for(j=0;j<3;j++)if(i==float_fields[j])tag=0xfedcba93;
        if(!strategy_field(a,f,reading,p+i,tag,4,1))return 0;
    }
    if(!strategy_reference(a,f,reading,p+0x40,1) || !strategy_outer(a,f,reading,p+0x44))return 0;
    for(i=0;i<sites;i++)for(j=0;j<4;j++)
        if(!strategy_reference(a,f,reading,get(a,p+0x68+j*4)+i*4,1))return 0;
    return strategy_words(a,f,reading,p+0x78,3) &&
        strategy_field(a,f,reading,get(a,p+0x84),0xfedcba9b,4,sites) && strategy_words(a,f,reading,p+0x88,2);
}
/*4c9250/4c9290: the shared800-byte navigation bitmap rows, with stable
 * allocation IDs. No file word is interpreted as a native pointer. */
static int strategy_navigation(GameApplication *a,ResourceFile *f,int reading){
    uint32_t count=get(a,0x60425c),i;GameAddress rows=get(a,0x604258);
    if(reading){
        if(!game_level_navigation_routes_release(a,0x604258) || !restore_word(a,f,&count))return 0;
        if(count>32767)return 0;
        rows=count?scene_projection_storage_allocate((size_t)count*4,"restored navigation rows"):0;
        if(count && !rows)return 0;
        game_memory_write_u32(&a->memory,0x604258,rows);game_memory_write_u32(&a->memory,0x60425c,count);
        for(i=0;i<count;i++){
            GameAddress row=scene_projection_storage_allocate(800,"restored navigation bitmap");if(!row)return 0;
            game_memory_write_u32(&a->memory,rows+i*4,row);
        }
    }else if(count>32767 || !u32(f,count))return 0;
    for(i=0;i<count;i++)if(!strategy_field(a,f,reading,get(a,rows+i*4),0xfedcba99,1,800))return 0;
    return !a->memory.fault;
}
static int imperial_strategy_archive(GameApplication *a,GameAddress p,ResourceFile *f,int reading);
/*53c840 shares the actual tagged pose codec and native command-pool owner.
 *The archived prefix/ring identities are data; no archive word becomes a host
 *pointer. Relationship identities are resolved only after all members load. */
static int vehicle_restore(GameApplication *a,GameAddress p,ResourceFile *f){
    TaggedRecordStreamServices records={f,read_port,NULL,NULL};uint32_t marker=get(a,0x63c964),prefix=0,ring=0,total,i,group=0;
    PresentationArchiveServices services={&records,a,game_level_entity_archive_frames,game_level_entity_archive_publish};
    GameAddress old=get(a,p+0xdc),first=0,previous=0,current=0,start=0;int ok=0;uint8_t kind=game_memory_read_u8(&a->memory,p+12);
    if(!restore_field(a,f,p+0xd8,1,1))return 0;marker=get(a,0x63c964);
    if(!presentation_instance_stream(&a->memory,p,1,1,&marker,&services))return 0;
    game_memory_write_u32(&a->memory,0x63c964,marker);
    if(kind!=0xfe && kind!=0x23){
        if(old && runtime_node_pool_return_chain(&a->memory,old)!=1)return 0;
        game_memory_write_u32(&a->memory,p+0xdc,0);
        if(!game_level_entity_runtime_initialize(a,p))return 0;
    }
    if(!restore_field(a,f,p+0xb4,4,1) || !restore_field(a,f,p+0xb8,4,1) || !restore_field(a,f,p+0xbc,4,3) ||
       !restore_field(a,f,p+0xcc,4,1) || !restore_field(a,f,p+0xd0,4,1) || !restore_field(a,f,p+0xd4,4,1) ||
       !restore_word(a,f,&prefix) || !restore_word(a,f,&ring))return 0;
    if(prefix>65536 || ring>65536 || prefix+ring>65536)return 0;total=prefix+ring;
    old=get(a,p+0xdc);if(old && runtime_node_pool_return_chain(&a->memory,old)!=1)return 0;
    game_memory_write_u32(&a->memory,p+0xdc,0);
    for(i=0;i<total;i++){
        current=runtime_node_pool_acquire(&a->memory);if(!current)goto cleanup;
        if(!first)first=current;if(previous){game_memory_write_u32(&a->memory,previous+4,current);game_memory_write_u32(&a->memory,current+8,previous);}
        previous=current;if(i==prefix)start=current;
        if(!restore_field(a,f,current+12,4,1) || !restore_field(a,f,current+20,4,1) ||
           !restore_field(a,f,current+36,4,1) || !restore_field(a,f,current+40,4,1) ||
           !restore_field(a,f,current+44,4,1) || !restore_field(a,f,current+24,4,3))goto cleanup;
    }
    if(ring){if(!start || !previous || !(get(a,start+44)&1u))goto cleanup;
        game_memory_write_u32(&a->memory,previous+4,start);game_memory_write_u32(&a->memory,start+8,previous);}
    game_memory_write_u32(&a->memory,p+0xdc,first);first=0;
    if(!restore_field(a,f,p+0xe0,4,1) || !restore_field(a,f,p+0xe4,4,3) || !restore_field(a,f,p+0xf4,4,1) ||
       !restore_field(a,f,p+0xf8,4,3) || !restore_word(a,f,&group))goto cleanup;
    if(group!=UINT32_MAX){int16_t count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
        if(count<0 || group>=(uint32_t)count)goto cleanup;
        game_memory_write_u32(&a->memory,p+0x104,get(a,get(a,0x639a70)+group*4));
    }else game_memory_write_u32(&a->memory,p+0x104,0);
    for(i=0;i<2;i++)if(!restore_field(a,f,p+0x108+i*4,4,1))goto cleanup;
    for(i=0;i<3;i++)if(!restore_field(a,f,p+0x12c+i*4,4,1))goto cleanup;
    ok=restore_field(a,f,p+0x138,4,1);
cleanup:if(first){GameMemory cleanup=a->memory;cleanup.fault=0;if(runtime_node_pool_return_chain(&cleanup,first)!=1)ok=0;}
    return ok && !a->memory.fault;
}
typedef struct RestoreWords {GameApplication *app;ResourceFile *file;} RestoreWords;
static int attachment_read_word(void *context,uint32_t *word){RestoreWords *s=context;return restore_word(s->app,s->file,word);}
static int attachment_restore(GameApplication *a,GameAddress child,ResourceFile *f){
    RestoreWords context={a,f};PlayerAttachmentArchiveServices services={&context,NULL,attachment_read_word};
    PlayerAttachmentState state={0};
    if(!child || get(a,get(a,child)+0x10)!=0x43b500)return 0;
    state.quantity=get(a,child+12);state.last_tick=get(a,child+8);
    if(!player_attachment_load(&state,&services))return 0;
    game_memory_write_u32(&a->memory,child+12,state.quantity);game_memory_write_u32(&a->memory,child+8,state.last_tick);return !a->memory.fault;
}
/*4e9170: restore the group state and recreate its live attachment owners,
 *then read ammunition/ticks into those same records. References fix later. */
static int group_vehicle_restore(GameApplication *a,GameAddress p,ResourceFile *f){
    static const unsigned fields[]={0x140,0x1ec,0x1f0,0x1f4,0x1f8,0x1fc};
    static const unsigned shorts[]={0x210,0x212,0x224};unsigned i;uint32_t second=0;
    if(get(a,get(a,p)+0x1f4)!=0x4e6340)return 0;
    gameplay_state_reset_actor(&a->memory,p);
    if(!restore_field(a,f,p+0x14c,2,1) || !restore_field(a,f,p+0x14a,2,1))return 0;
    game_memory_write_u16(&a->memory,p+0x1ba,game_memory_read_u16(&a->memory,p+0x14c));
    game_memory_write_u16(&a->memory,p+0x1b8,game_memory_read_u16(&a->memory,p+0x14a));
    if(!vehicle_restore(a,p,f))return 0;
    for(i=0;i<6;i++)if(!restore_field(a,f,p+fields[i],4,1))return 0;
    if(!restore_field(a,f,p+0x200,4,3))return 0;
    for(i=0;i<3;i++)if(!restore_field(a,f,p+shorts[i],2,1))return 0;
    if(!game_level_group_vehicle_restore_components(a,p) || !attachment_restore(a,get(a,p+0x21c),f) ||
       !restore_word(a,f,&second))return 0;
    if(second && !attachment_restore(a,get(a,p+0x220),f))return 0;
    for(i=0;i<4;i++)if(!restore_field(a,f,p+0x214+i*2,2,1))return 0;
    game_memory_write_u32(&a->memory,p+0x144,UINT32_MAX);return !a->memory.fault;
}
static int scenery_restore_base(GameApplication *a,GameAddress p,ResourceFile *f){
    uint32_t marker=get(a,0x63c964),flags=get(a,p+0x84);int32_t count=(int32_t)get(a,p+0x80);unsigned i;
    TaggedRecordStreamServices records={f,read_port,NULL,NULL};
    PresentationArchiveServices services={&records,a,game_level_entity_archive_frames,game_level_entity_archive_publish};
    if(count>65536)return 0;
    for(i=0;i<4;i++)game_memory_write_u32(&a->memory,p+0x6c+i*4,0);
    for(i=0;(int32_t)i<count;i++)game_memory_write_u32(&a->memory,flags+i*4,1);
    game_memory_write_u32(&a->memory,p+0x88,0);
    if(a->memory.fault || !presentation_instance_stream(&a->memory,p,1,0,&marker,&services))return 0;
    game_memory_write_u32(&a->memory,0x63c964,marker);return !a->memory.fault;
}
extern GameAddress game_level_ground_controller_restore_create(GameApplication *,GameAddress);
int game_level_entity_restore(GameApplication *a,GameAddress p,ResourceFile *f){
    uint32_t method,marker;unsigned layer,index;EffectModel *model;uint8_t posture;
    TaggedRecordStreamServices records={f,read_port,NULL,NULL};
    PresentationArchiveServices services={&records,a,game_level_entity_archive_frames,game_level_entity_archive_publish};
    if(!a || !p || !f || a->memory.fault)return 0;method=get(a,get(a,p)+0x70);
    switch(method){
    case 0x452320:{
        GameAddress reference;SoundPlaybackRequest sound={0};long double x,y,z,scale;unsigned i;
        game_memory_write_u32(&a->memory,p+0x38,0x45000000u);marker=get(a,0x63c964);
        if(!presentation_instance_stream(&a->memory,p,1,1,&marker,&services))return 0;
        game_memory_write_u32(&a->memory,0x63c964,marker);
        reference=get(a,0x58f890);scale=real(a,0x573430);
        x=(long double)real(a,p+24)-real(a,reference+12);y=(long double)real(a,p+28)-real(a,reference+16);z=(long double)real(a,p+32)-real(a,reference+20);
        sound.sound=24;sound.volume=1;sound.pitch=0.55f;sound.flags=sound.force=1;
        sound.distance_squared=(float)((y*y*scale+z*z*scale)+x*x*scale);
        if(a->memory.fault || !game_menu_sound_play(a,&sound))return 0;
        game_memory_write_u32(&a->memory,p+0xd0,get(a,get(a,0x628a90)));
        if(!restore_field(a,f,p+0xb4,4,1) || !restore_field(a,f,p+0xb8,4,1))return 0;
        for(i=0;i<3;i++)if(!restore_field(a,f,p+0xbc+i*2,2,1))return 0;
        for(i=0;i<3;i++)if(!restore_field(a,f,p+0xc4+i*4,4,1))return 0;
        for(i=0;i<4;i++)if(!restore_field(a,f,p+0xd8+i*12,4,3))return 0;
        return restore_field(a,f,p+0xd4,4,1) && !a->memory.fault;
    }
    case 0x4c0120:return imperial_strategy_archive(a,p,f,1);
    case 0x5339f0:{
        uint32_t present=0;GameAddress controller;
        if(!scenery_restore_base(a,p,f) || !restore_field(a,f,p+0xa0,4,1) ||
           !restore_field(a,f,p+0xa4,4,1) || !restore_field(a,f,p+0xac,4,1) ||
           !restore_word(a,f,&present))return 0;
        if(!present){game_memory_write_u32(&a->memory,p+0xa8,0);return !a->memory.fault;}
        controller=game_level_ground_controller_restore_create(a,p);
        return controller && game_level_entity_restore(a,controller,f);
    }
    /*4600f0: four raw little-endian records precede the tagged base. Saved
     *peer/owner identities are resolved only after every entity is restored. */
    case 0x4600f0:
        if(!restore_raw_field(a,f,p+0x64,12) || !restore_raw_field(a,f,p+0x70,4) ||
           !restore_raw_field(a,f,p+0x74,4) || !restore_raw_field(a,f,p+0x78,4))return 0;
        marker=get(a,0x63c964);
        if(!presentation_instance_stream(&a->memory,p,1,0,&marker,&services))return 0;
        game_memory_write_u32(&a->memory,0x63c964,marker);
        game_memory_write_u32(&a->memory,p+0x7c,0);game_memory_write_u32(&a->memory,p+0x80,0);
        game_memory_write_u32(&a->memory,p+0x84,0);return !a->memory.fault;
    case 0x45eff0:
        marker=get(a,0x63c964);
        if(!presentation_instance_stream(&a->memory,p,1,1,&marker,&services))return 0;
        game_memory_write_u32(&a->memory,0x63c964,marker);
        return restore_field(a,f,p+0xb4,4,1) && restore_field(a,f,p+0xb8,4,1) &&
            game_level_gate_occupancy(a,p,get(a,p+0xb8)!=1);
    case 0x434950:{
        uint32_t format=0,selector=0;GameAddress catalog;
        marker=get(a,0x63c964);
        if(!presentation_instance_stream(&a->memory,p,1,0,&marker,&services))return 0;
        game_memory_write_u32(&a->memory,0x63c964,marker);
        if(!restore_field(a,f,p+0x68,4,1) || !restore_field(a,f,p+0x6c,1,1) || !restore_word(a,f,&format))return 0;
        catalog=get(a,0x5f0210+(get(a,p+20)&63u)*4);
        if(format==0xf0004567u){
            if(!restore_word(a,f,&selector) || selector>2)return 0;
            selector=selector==0?0x1bc:selector==1?0x1fc:0x4b0;
        }else selector=format?0x1bc:0x1fc; /*Original pre-marker saves. */
        game_memory_write_u32(&a->memory,p+0x64,get(a,catalog+selector));return !a->memory.fault;
    }
    /*4396a0 clears transient articulation links, restores both words, then
     *4996a0 restores the existing pose/model through its established owner. */
    case 0x4396a0:
        game_memory_write_u32(&a->memory,p+0x31c,0);
        game_memory_write_u32(&a->memory,p+0x30c,0);
        game_memory_write_u32(&a->memory,p+0x2fc,0);
        if(!restore_field(a,f,p+0x2f4,4,1) || !restore_field(a,f,p+0x2f8,4,1))return 0;
        layer=2;break;
    case 0x4996a0:layer=2;break;
    case 0x511180:layer=3;break;
    case 0x546920:{
        static const unsigned fields[]={0x160,0x164,0x168,0x16c,0x170,0x174};unsigned i;uint32_t present;
        if(!vehicle_restore(a,p,f) || !restore_field(a,f,p+0x148,4,1) ||
           !restore_field(a,f,p+0x14c,2,1) || !restore_field(a,f,p+0x14e,2,1) ||
           !restore_field(a,f,p+0x150,4,1) || !restore_field(a,f,p+0x154,4,3))return 0;
        for(i=0;i<6;i++)if(!restore_field(a,f,p+fields[i],4,1))return 0;
        game_memory_write_u32(&a->memory,p+0x17c,0);game_memory_write_u32(&a->memory,p+0x180,0);
        game_memory_write_u32(&a->memory,p+0x178,UINT32_MAX);
        if(!game_level_infantry_guns_build(a,p,get(a,0x5f0210+(get(a,p+20)&63u)*4)) ||
           !attachment_restore(a,get(a,p+0x13c),f))return 0;
        for(i=0;i<2;i++){present=0;if(!restore_word(a,f,&present) ||
            (present && !attachment_restore(a,get(a,p+0x140+i*4),f)))return 0;}
        return !a->memory.fault;
    }
    case 0x55c710:{
        static const unsigned fields[]={0x150,0x154,0x158,0x160,0x164,0x168,0x16c,0x170,0x174,0x178,0x17c,0x184,0x188,0x190};
        unsigned i;uint32_t present=0;
        if(!vehicle_restore(a,p,f) || !game_level_aircraft_archive_gun(a,p))return 0;
        for(i=0;i<3;i++)if(!restore_field(a,f,p+0x13c+i*2,2,1))return 0;
        if(!restore_field(a,f,p+0x144,4,3))return 0;
        for(i=0;i<14;i++)if(!restore_field(a,f,p+fields[i],4,1))return 0;
        if(!restore_word(a,f,&present) || (present && !attachment_restore(a,get(a,p+0x194),f)))return 0;
        game_memory_write_u32(&a->memory,p+0x15c,0);return aircraft_archive_sound(a,p);
    }
    case 0x554940:
        game_memory_write_u32(&a->memory,p+0x2b8,0);game_memory_write_u32(&a->memory,p+0x2c8,0);
        if(!group_vehicle_restore(a,p,f) || !restore_field(a,f,p+0x260,4,1) ||
           !restore_field(a,f,p+0x2b0,4,1) || !restore_field(a,f,p+0x2b4,4,1) ||
           !restore_field(a,f,p+0x298,4,1) || !restore_field(a,f,p+0x29c,4,1) ||
           !restore_field(a,f,p+0x2a0,4,3) || !restore_field(a,f,p+0x2ac,2,1))return 0;
        return !a->memory.fault;
    case 0x4082f0:return group_vehicle_restore(a,p,f) && restore_field(a,f,p+0x260,4,1);
    case 0x51ae00:
        if(!restore_field(a,f,p+0xac,4,1))return 0;
        /*The subtype archive then reads4fa840's byte and4833c0's base. */
    case 0x4fa840:
        if(!restore_field(a,f,p+0xa4,1,1))return 0;
    case 0x4833c0:return scenery_restore_base(a,p,f);
    /*459ff0's classFE archive uses untagged little-endian records. Its model
     *and command initialization are intentionally skipped by the shared base. */
    case 0x459ff0:
        return vehicle_restore(a,p,f) && restore_raw_field(a,f,p+0x13c,20) &&
            restore_raw_field(a,f,p+0x150,0x4c);
    case 0x4f9ae0:{
        unsigned i;uint32_t selected[2];
        (void)game_list_detach_node(&a->memory,p,8,4);
        if(a->memory.fault || !vehicle_restore(a,p,f))return 0;
        /*The level bootstrap supplies an active rig. A finalized player's
         *4f9a90 archive has no child records, just as4f9ae0 skips null slots.
         *Finalize the bootstrap scene before consuming the saved selections.
         *This also restores the dead-player camera/input state while keeping
         *the pooled rig/model for later respawn and final destruction. */
        if(!get(a,p+0x58) && !game_level_player_finalize(a,p))return 0;
        for(i=0;i<PLAYER_SCENE_CHILD_COUNT;i++){
            GameAddress child=get(a,p+0x1d4+i*4);if(child && !attachment_restore(a,child,f))return 0;
        }
        if(!restore_field(a,f,p+0x214,4,2))return 0;
        selected[0]=get(a,p+0x214);selected[1]=get(a,p+0x218);
        if(selected[0]>=PLAYER_SCENE_CHILD_COUNT || selected[1]>=PLAYER_SCENE_CHILD_COUNT)return 0;
        game_memory_write_u32(&a->memory,p+0x1cc,get(a,p+0x1d4+selected[0]*4));
        game_memory_write_u32(&a->memory,p+0x1d0,get(a,p+0x1d4+selected[1]*4));
        for(i=0;i<3;i++)game_memory_write_u32(&a->memory,p+0x1bc+i*4,get(a,p+24+i*4));
        game_memory_write_u16(&a->memory,p+0x1c8,game_memory_read_u16(&a->memory,p+0x24));return !a->memory.fault;
    }
    case 0x40efe0:
        if(!restore_field(a,f,p+0x144,2,1) || !restore_field(a,f,p+0x146,2,1) || !vehicle_restore(a,p,f) ||
           !restore_field(a,f,p+0x13c,4,1) || !restore_field(a,f,p+0x140,4,1) || !restore_field(a,f,p+0x148,1,1) ||
           !restore_field(a,f,p+0x14c,4,1))return 0;
        posture=game_memory_read_u8(&a->memory,p+0x148);index=get(a,p+0xe0)==2?1:get(a,p+0xe0)==5?5:
            get(a,p+0xe0)==1?(posture==2?2:posture==4?4:3):0;model=effect_model_find(get(a,p+0x54));
        return effect_model_select_profile(&a->memory,model,(uint16_t)index,get(a,0x5ac6ac)) &&
            game_level_class40_guns_build(a,p,get(a,0x589740)) && attachment_restore(a,get(a,p+0x150),f) && attachment_restore(a,get(a,p+0x154),f);
    case 0x4e9170:return group_vehicle_restore(a,p,f);
    case 0x53c840:return vehicle_restore(a,p,f);
    case 0x4517c0:return vehicle_restore(a,p,f) && restore_field(a,f,p+0x13c,2,1);
    case 0x42f030:layer=0;break;
    case 0x536310:layer=1;break;
    default:fprintf(stderr,"Unreconstructed entity restore method %08x\n",(unsigned)method);return 0;
    }
    marker=get(a,0x63c964);
    if(!presentation_instance_stream(&a->memory,p,1,layer,&marker,&services))return 0;
    game_memory_write_u32(&a->memory,0x63c964,marker);return !a->memory.fault;
}
static int restore_resolve(void *app,uint32_t identity,GameAddress *result){
    GameApplication *a=app;
    /*Optional archive links use zero for null. A finalized local player's
     *identity is also zero; a general catalog lookup would bind every empty
     *link to that pooled player, including its own follower link. */
    if(!identity){*result=0;return !a->memory.fault;}
    return gameplay_catalog_resolve_identity(&a->memory,identity,NULL,NULL,NULL,result);
}
/*53caa0/4cf260: resolve only after every catalog member has been loaded. The
 *same owned command graph receives handles; saved IDs never become pointers. */
int game_level_entity_restore_fix(GameApplication *a,GameAddress p){
    static const unsigned peers[]={0x108,0x10c,0x138};GameAddress target,node,ring=0;
    uint32_t identity,method;unsigned i,visits=0;int aircraft,imperial;
    method=get(a,get(a,p)+0x74);if(method==0x42f100)return !a->memory.fault;
    if(method==0x452460){
        uint32_t packed=get(a,p+0xd4);GameAddress descriptor=0;
        if(packed!=UINT32_MAX){
            uint32_t owner=packed>>16,slot=packed&65535u;GameAddress group;
            int16_t count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
            if(count<0 || owner>=(uint32_t)count)return 0;
            group=get(a,get(a,0x639a70)+owner*4);if(!group)return 0;
            count=(int16_t)game_memory_read_u16(&a->memory,group+0x24);
            if(count<0 || slot>=(uint32_t)count)return 0;
            descriptor=get(a,get(a,group+0x18)+slot*4);
        }
        game_memory_write_u32(&a->memory,p+0xd4,descriptor);return !a->memory.fault;
    }
    if(method==0x533ae0){
        target=get(a,p+0xa8);return !target || game_level_entity_restore_fix(a,target);
    }
    if(method==0x460160){
        uint32_t word=get(a,p+0x78);int32_t owner;GameAddress catalog=0;
        memcpy(&owner,&word,4);target=0;
        if(!restore_resolve(a,get(a,p+0x74),&target) || !target)return 0;
        game_memory_write_u32(&a->memory,p+0x74,target);
        if(owner>=0){if(owner>=6)return 0;catalog=get(a,0x5f0210+(uint32_t)owner*4);}
        return !a->memory.fault && game_level_teleporter_bind(a,p,catalog);
    }
    imperial=method==0x4c0990;
    aircraft=method==0x55c960;if(aircraft || imperial)method=0x53caa0;
    if(method==0x4e9300){
        identity=get(a,p+0x140);target=0;
        if(identity && !restore_resolve(a,identity,&target))return 0;
        game_memory_write_u32(&a->memory,p+0x13c,target);
        if(identity && !target)game_memory_write_u32(&a->memory,p+0x140,0);
        if(!restore_resolve(a,get(a,p+0x1fc),&target))return 0;
        game_memory_write_u32(&a->memory,p+0x1fc,target);method=0x53caa0;
    }
    if(method!=0x53caa0 && method!=0x45a0e0){fprintf(stderr,"Unreconstructed entity archive fix %08x\n",(unsigned)method);return 0;}
    for(i=0;i<2;i++){
        unsigned id_field=i?0xf4:0xcc,object_field=i?0xf0:0xc8;identity=get(a,p+id_field);target=0;
        if(!identity)continue;
        if(!restore_resolve(a,identity,&target))return 0;
        game_memory_write_u32(&a->memory,p+object_field,target);
        if(!target)game_memory_write_u32(&a->memory,p+id_field,0);
    }
    node=get(a,p+0xdc);
    while(node){
        RuntimeNode *command=runtime_node_pool_find(node);
        if(++visits>65536 || !command || command->in_pool)return 0;
        if(!ring && (command->flags&1u))ring=node;
        target=0;if(command->target_identity && !restore_resolve(a,command->target_identity,&target))return 0;
        command->target=target;if(!target)command->target_identity=0;
        node=command->next?command->next->projection:0;if(node==ring)break;
    }
    for(i=0;i<3;i++){
        if(!restore_resolve(a,get(a,p+peers[i]),&target))return 0;
        game_memory_write_u32(&a->memory,p+peers[i],target);
    }
    if(method==0x45a0e0)for(i=0;i<5;i++){
        if(!restore_resolve(a,get(a,p+0x13c+i*4),&target))return 0;
        game_memory_write_u32(&a->memory,p+0x13c+i*4,target);
    }
    if(aircraft){
        identity=get(a,p+0x190);target=0;
        if(identity && !restore_resolve(a,identity,&target))return 0;
        game_memory_write_u32(&a->memory,p+0x18c,target);
        if(identity && !target)game_memory_write_u32(&a->memory,p+0x190,0);
    }
    if(imperial){
        static const unsigned banks[]={0x62c,0x7ec,0x9ac,0xb6c};
        unsigned sites=game_memory_read_u16(&a->memory,0x639a74),j;
        if(sites>=32768)return 0;
        for(i=0;i<sites*8;i++){
            GameAddress slot=get(a,p+0x2c8)+i*4;
            if(!restore_resolve(a,get(a,slot),&target))return 0;
            game_memory_write_u32(&a->memory,slot,target);
        }
        for(i=0;i<16;i++){
            if(!restore_resolve(a,get(a,p+0x2cc+i*4),&target))return 0;
            game_memory_write_u32(&a->memory,p+0x2cc+i*4,target);
        }
        for(i=0;i<14;i++){
            if(!restore_resolve(a,get(a,p+0x3f8+i*4),&target))return 0;
            game_memory_write_u32(&a->memory,p+0x3c0+i*4,target);
        }
        for(i=0;i<112;i++)for(j=0;j<4;j++){
            if(!restore_resolve(a,get(a,p+banks[j]+i*4),&target))return 0;
            game_memory_write_u32(&a->memory,p+banks[j]+i*4,target);
        }
        if(!restore_resolve(a,get(a,p+0x50c),&target))return 0;
        game_memory_write_u32(&a->memory,p+0x508,target);
    }
    return !a->memory.fault;
}
static int attachment_save(GameApplication *a,GameAddress child,ResourceFile *f){
    PlayerAttachmentArchiveServices services={f,attachment_word,NULL};
    PlayerAttachmentState temporary={0};const PlayerAttachmentState *state;
    if(!child || get(a,get(a,child)+0xc)!=0x43b4d0)return 0;
    state=player_scene_children_attachment(child);
    if(!state){temporary.quantity=get(a,child+0xc);temporary.last_tick=get(a,child+8);state=&temporary;}
    return !a->memory.fault && player_attachment_save(state,&services);
}
/*4bf8e0/4c0120: strategy state in archive order. The actual AI records and
 *their existing arrays/paths remain authoritative; references become IDs. */
static int imperial_strategy_archive(GameApplication *a,GameAddress p,ResourceFile *f,int reading){
    static const unsigned first_words[]={0x258,0x25c,0x264,0x260,0x268};
    static const unsigned work_arrays[]={0x30c,0x310,0x314,0x318,0x31c,0x320,0x324,0x328};
    static const unsigned tail_arrays[]={0xe1c,0xe20,0xe24,0xe28,0xe2c,0xe30,0xe34,0xe40,0xe44};
    static const unsigned banks[]={0x62c,0x7ec,0x9ac,0xb6c};
    uint32_t present=get(a,p+0x13c)!=0;unsigned sites=game_memory_read_u16(&a->memory,0x639a74),i,j,k;
    if(sites>=32768 || !(reading?vehicle_restore(a,p,f):vehicle_save(a,p,f)))return 0;
    /*Retail emits1 for its present gun; define the absent case with0 as
     *required by the reader instead of omitting its presence record. */
    if(reading){if(!restore_word(a,f,&present))return 0;}
    else if(!u32(f,present))return 0;
    if(present && !(reading?attachment_restore(a,get(a,p+0x13c),f):attachment_save(a,get(a,p+0x13c),f)))return 0;
    if(!strategy_navigation(a,f,reading) || !strategy_words(a,f,reading,p+0x140,1))return 0;
    for(i=0;i<6;i++)if(!strategy_field(a,f,reading,p+0x144+i*16,0xfedcba9a,4,4))return 0;
    if(!strategy_words(a,f,reading,p+0x1a4,1) || !strategy_field(a,f,reading,p+0x1a8,0xfedcba99,1,14) ||
       !strategy_field(a,f,reading,p+0x1b6,0xfedcba99,1,14) || !strategy_words(a,f,reading,p+0x1c4,1) ||
       !strategy_projection(a,f,reading,p+0x1c8,sites))return 0;
    for(i=0;i<5;i++)if(!strategy_words(a,f,reading,p+first_words[i],1))return 0;
    for(i=0;i<3;i++)if(!strategy_field(a,f,reading,get(a,p+0x280+i*4),0xfedcba9a,4,sites))return 0;
    if(!strategy_words(a,f,reading,p+0x28c,15))return 0;
    for(i=0;i<sites*8;i++)if(!strategy_reference(a,f,reading,get(a,p+0x2c8)+i*4,0))return 0;
    for(i=0;i<16;i++)if(!strategy_reference(a,f,reading,p+0x2cc+i*4,0))return 0;
    for(i=0;i<8;i++)if(!strategy_field(a,f,reading,get(a,p+work_arrays[i]),0xfedcba9a,4,sites))return 0;
    if(!strategy_words(a,f,reading,p+0x32c,4) || !strategy_field(a,f,reading,p+0x33c,0xfedcba93,4,1))return 0;
    for(i=0;i<14;i++)if(!strategy_reference(a,f,reading,p+0x340+i*4,2))return 0;
    if(!strategy_field(a,f,reading,p+0x378,0xfedcba9a,4,14))return 0;
    for(i=0;i<4;i++)if(!strategy_field(a,f,reading,get(a,p+0x3b0+i*4),0xfedcba9a,4,sites))return 0;
    if(!strategy_field(a,f,reading,p+0x3f8,0xfedcba98,4,14))return 0;
    for(i=0;i<14;i++)for(j=0;j<2;j++)if(!strategy_outer(a,f,reading,get(a,p+0x430+j*56+i*4)))return 0;
    if(!strategy_field(a,f,reading,p+0x4a0,0xfedcba9a,4,14))return 0;
    for(i=0;i<4;i++)if(!strategy_field(a,f,reading,p+0x4d8+i*12,0xfedcba9b,4,3))return 0;
    for(i=0;i<4;i++)if(!strategy_field(a,f,reading,p+0x510+i*56,0xfedcba9a,4,14))return 0;
    if(!strategy_words(a,f,reading,p+0x5f0,1) || !strategy_field(a,f,reading,p+0x5f4,0xfedcba9a,4,14))return 0;
    for(i=0;i<112;i++)for(k=0;k<4;k++)if(!strategy_reference(a,f,reading,p+banks[k]+i*4,0))return 0;
    if(!strategy_field(a,f,reading,p+0xd2c,0xfedcba9b,4,3) || !strategy_words(a,f,reading,p+0xd38,1) ||
       !strategy_words(a,f,reading,p+0xd94,3) || !strategy_field(a,f,reading,p+0xda0,0xfedcba9a,4,14) ||
       !strategy_field(a,f,reading,p+0xdd8,0xfedcba9a,4,14) || !strategy_words(a,f,reading,p+0xe10,3))return 0;
    for(i=0;i<9;i++)if(!strategy_field(a,f,reading,get(a,p+tail_arrays[i]),i==8?0xfedcba98:0xfedcba9a,4,sites))return 0;
    if(!strategy_words(a,f,reading,p+0xe48,1))return 0;
    for(i=0;i<20;i++)if(!strategy_field(a,f,reading,p+0xe4c+i*12,0xfedcba9b,4,3))return 0;
    return strategy_words(a,f,reading,p+0xf3c,2) && strategy_field(a,f,reading,p+0xf44,0xfedcba9a,4,4) &&
        strategy_words(a,f,reading,p+0xf54,3) && strategy_field(a,f,reading,p+0x50c,0xfedcba90,4,1) &&
        strategy_words(a,f,reading,p+0xf60,1);
}
/*4e9000: tagged group state and two owned attachments. Borrowed entity
 *references become stable IDs; no host pointers or record padding are saved. */
static int group_vehicle_save(GameApplication *a,GameAddress p,ResourceFile *f){
    GameAddress target,second;unsigned i;
    if(!scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x14c),2) ||
       !scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x14a),2) || !vehicle_save(a,p,f) ||
       !scalar(f,0xfedcba90u,get(a,p+0x140),4) || !u32(f,get(a,p+0x1ec)) ||
       !f32(f,get(a,p+0x1f0)) || !f32(f,get(a,p+0x1f4)) || !u32(f,get(a,p+0x1f8)))return 0;
    target=get(a,p+0x1fc);
    if(!scalar(f,0xfedcba90u,target?get(a,target+20):0,4) || !memory_record(a,f,0xfedcba9bu,p+0x200,4,3) ||
       !scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x210),2) ||
       !scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x212),2) ||
       !scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x224),2) ||
       !attachment_save(a,get(a,p+0x21c),f))return 0;
    second=get(a,p+0x220);if(!u32(f,second!=0) || (second && !attachment_save(a,second,f)))return 0;
    for(i=0;i<4;i++)if(!scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x214+i*2),2))return 0;
    return !a->memory.fault;
}
/*459ee0: command archive followed by raw little-endian identities/classFE tail. */
static int resource_cluster_save(GameApplication *a,GameAddress p,ResourceFile *f){
    uint8_t identities[20],tail[0x4c];unsigned i,j;
    if(!vehicle_save(a,p,f))return 0;
    for(i=0;i<5;i++){
        GameAddress unit=get(a,p+0x13c+i*4);uint32_t identity=unit?get(a,unit+0x14):0;
        for(j=0;j<4;j++)identities[i*4+j]=(uint8_t)(identity>>(j*8));
    }
    return !a->memory.fault && game_memory_read(&a->memory,p+0x150,tail,sizeof(tail)) &&
        resource_file_write(f,identities,sizeof(identities))==sizeof(identities) &&
        resource_file_write(f,tail,sizeof(tail))==sizeof(tail);
}
/*4f9a90 ->53c640 -> fifteen virtual0c=43b4d0 writers. The actual native
 *attachment owns quantity/tick. A bounded legacy record may be borrowed during
 *migration; that read-only temporary never becomes a competing state owner. */
static int player_save(GameApplication *a,GameAddress p,ResourceFile *f){
    unsigned i;
    if(!vehicle_save(a,p,f))return 0;
    for(i=0;i<PLAYER_SCENE_CHILD_COUNT;i++){
        GameAddress child=get(a,p+0x1d4+i*4);
        if(!child)continue;
        if(!attachment_save(a,child,f))return 0;
    }
    return memory_record(a,f,0xfedcba9au,p+0x214,4,2) && !a->memory.fault;
}
/*42efb0/536300: shared saved scene identity, position, angles and health.
 *These are canonical fixed-width values, not host pointers or object padding. */
int game_level_entity_save(GameApplication *a,GameAddress p,ResourceFile *f){
    uint32_t method,marker=0;unsigned layer;
    TaggedRecordStreamServices records={f,NULL,write_port,flush_port};
    PresentationArchiveServices services={&records,NULL,NULL,NULL};
    if(!a || !f || !p || a->memory.fault)return 0;
    method=get(a,get(a,p)+0x6c);if(a->memory.fault)return 0;
    switch(method){
    case 0x460070:{
        GameAddress peer=get(a,p+0x74),owner=get(a,p+0x78);if(!peer)return 0;
        return raw_field(a,f,p+0x64,12) && raw_field(a,f,p+0x70,4) && raw_word(f,get(a,peer+20)) &&
            raw_word(f,owner?game_memory_read_u16(&a->memory,owner+8):UINT32_MAX) &&
            presentation_instance_stream(&a->memory,p,0,0,&marker,&services);
    }
    case 0x45efb0:
        return presentation_instance_stream(&a->memory,p,0,1,&marker,&services) &&
            u32(f,get(a,p+0xb4)) && u32(f,get(a,p+0xb8));
    case 0x4348d0:{
        GameAddress catalog=get(a,0x5f0210+(get(a,p+20)&63u)*4),recipe=get(a,p+0x64);
        uint32_t selector=recipe==get(a,catalog+0x1bc)?0:recipe==get(a,catalog+0x1fc)?1:2;
        return presentation_instance_stream(&a->memory,p,0,0,&marker,&services) &&
            u32(f,get(a,p+0x68)) && u8(f,game_memory_read_u8(&a->memory,p+0x6c)) &&
            u32(f,0xf0004567u) && u32(f,selector);
    }
    /*439660 saves the two live articulation words before499670. */
    case 0x439660:
        if(!u32(f,get(a,p+0x2f4)) || !u32(f,get(a,p+0x2f8)))return 0;
        layer=2;break;
    /*40ef40 writes its two shorts before the shared vehicle archive. The two
     *ordinary auxiliary gun records use the same43b4d0 quantity/tick codec. */
    case 0x40ef40:
        return scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x144),2) &&
            scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x146),2) && vehicle_save(a,p,f) &&
            u32(f,get(a,p+0x13c)) && u32(f,get(a,p+0x140)) && u8(f,game_memory_read_u8(&a->memory,p+0x148)) &&
            u32(f,get(a,p+0x14c)) && attachment_save(a,get(a,p+0x150),f) && attachment_save(a,get(a,p+0x154),f);
    case 0x451790:return vehicle_save(a,p,f) && scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x13c),2);
    case 0x4e9000:return group_vehicle_save(a,p,f);
    case 0x5467e0:{
        static const unsigned fields[]={0x160,0x164,0x168,0x16c,0x170,0x174};unsigned i;GameAddress child;
        if(!vehicle_save(a,p,f) || !f32(f,get(a,p+0x148)) ||
           !scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x14c),2) ||
           !scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x14e),2) ||
           !f32(f,get(a,p+0x150)) || !memory_record(a,f,0xfedcba9bu,p+0x154,4,3))return 0;
        for(i=0;i<6;i++)if(!(i==1 || i>=4?f32(f,get(a,p+fields[i])):u32(f,get(a,p+fields[i]))))return 0;
        if(!attachment_save(a,get(a,p+0x13c),f))return 0;
        for(i=0;i<2;i++){child=get(a,p+0x140+i*4);if(!u32(f,child!=0) || (child && !attachment_save(a,child,f)))return 0;}
        return !a->memory.fault;
    }
    case 0x55c570:{
        static const unsigned fields[]={0x150,0x154,0x158,0x160,0x164,0x168,0x16c,0x170,0x174,0x178,0x17c,0x184,0x188};
        unsigned i;GameAddress child;
        if(!vehicle_save(a,p,f))return 0;
        for(i=0;i<3;i++)if(!scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x13c+i*2),2))return 0;
        if(!memory_record(a,f,0xfedcba9bu,p+0x144,4,3))return 0;
        for(i=0;i<13;i++)if(!(i>=6 && i<=8?f32(f,get(a,p+fields[i])):u32(f,get(a,p+fields[i]))))return 0;
        if(!scalar(f,0xfedcba90u,get(a,p+0x18c)?get(a,p+0x190):0,4))return 0;
        child=get(a,p+0x194);if(!u32(f,child!=0) || (child && !attachment_save(a,child,f)))return 0;
        return aircraft_archive_sound(a,p);
    }
    case 0x4082c0:return group_vehicle_save(a,p,f) && scalar(f,0xfedcba90u,get(a,p+0x260),4);
    case 0x5548c0:
        return group_vehicle_save(a,p,f) && scalar(f,0xfedcba90u,get(a,p+0x260),4) &&
            u32(f,get(a,p+0x2b0)) && u32(f,get(a,p+0x2b4)) && scalar(f,0xfedcba90u,get(a,p+0x298),4) &&
            u32(f,get(a,p+0x29c)) && memory_record(a,f,0xfedcba9bu,p+0x2a0,4,3) &&
            scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0x2ac),2);
    case 0x4bf8e0:return imperial_strategy_archive(a,p,f,0);
    case 0x53c640:return vehicle_save(a,p,f);
    case 0x459ee0:return resource_cluster_save(a,p,f);
    case 0x4f9a90:return player_save(a,p,f);
    case 0x452240:{
        GameAddress descriptor=get(a,p+0xd4);uint32_t packed=UINT32_MAX;unsigned i;
        if(!presentation_instance_stream(&a->memory,p,0,1,&marker,&services) ||
           !f32(f,get(a,p+0xb4)) || !f32(f,get(a,p+0xb8)))return 0;
        for(i=0;i<3;i++)if(!scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,p+0xbc+i*2),2))return 0;
        for(i=0;i<3;i++)if(!u32(f,get(a,p+0xc4+i*4)))return 0;
        for(i=0;i<4;i++)if(!memory_record(a,f,0xfedcba9bu,p+0xd8+i*12,4,3))return 0;
        if(descriptor){uint16_t word=game_memory_read_u16(&a->memory,descriptor+8);int16_t slot;
            memcpy(&slot,&word,2);packed=(get(a,get(a,descriptor+4))<<16)|(uint32_t)(int32_t)slot;
        }
        return scalar(f,0xfedcba90u,packed,4) && !a->memory.fault;
    }
    case 0x51add0:
        if(!u32(f,get(a,p+0xac)))return 0;
    case 0x4fa810:
        if(!u8(f,game_memory_read_u8(&a->memory,p+0xa4)))return 0;
        /*4fa810 continues into4833b0's scenery archive. Its neighboring
         *533980 producer archive has a distinct controller pointer at a8. */
        layer=0;break;
    case 0x533980:
        if(!presentation_instance_stream(&a->memory,p,0,0,&marker,&services) ||
           !u32(f,get(a,p+0xa0)) || !f32(f,get(a,p+0xa4)) || !u32(f,get(a,p+0xac)) ||
           !u32(f,get(a,p+0xa8)!=0))return 0;
        return !get(a,p+0xa8) || game_level_entity_save(a,get(a,p+0xa8),f);
    case 0x4833b0:case 0x42efb0:layer=0;break;
    case 0x536300:layer=1;break;
    case 0x499670:layer=2;break;
    case 0x511140:layer=3;break;
    default:
        fprintf(stderr,"Unreconstructed entity save %08x object=%08x\n",(unsigned)method,(unsigned)p);
        a->memory.fault=1;a->memory.fault_address=method;return 0;
    }
    return presentation_instance_stream(&a->memory,p,0,layer,&marker,&services);
}
/* Existing core codecs consume these actual native member algorithms. The
 * same records/owners serve gameplay and saving, with no CPU callback bridge. */
typedef struct SaveRecords {GameApplication *app;ResourceFile *file;} SaveRecords;
static int save_member(void *context,GameAddress object,unsigned operation){
    SaveRecords *s=context;
    if(operation==0)return game_level_entity_save(s->app,object,s->file);
    if(operation==3)return game_level_command_save(s->app,object,s->file);
    return 0;
}
int game_level_command_save(GameApplication *a,GameAddress p,ResourceFile *f){
    unsigned family;uint32_t method,marker=0;SaveRecords context={a,f};
    TaggedRecordStreamServices records={f,NULL,write_port,flush_port};
    GameplayRootArchiveServices services={&records,&context,save_member,NULL,NULL,NULL,NULL,NULL};
    if(!a || !p || !f || a->memory.fault)return 0;
    method=get(a,get(a,p)+0x58);if(a->memory.fault)return 0;
    switch(method){
    case 0x42ec20:return 1; /*Verified retail empty command-state writer. */
    case 0x53d920:family=0;break;
    case 0x53e840:family=1;break;
    case 0x53f1e0:case 0x53fcb0:family=2;break;
    default:
        fprintf(stderr,"Unreconstructed command save %08x object=%08x\n",(unsigned)method,(unsigned)p);
        a->memory.fault=1;a->memory.fault_address=method;return 0;
    }
    return gameplay_state_stream_group_member(&a->memory,0,family,&marker,&services,p);
}
/*Five catalog virtual78 methods share the established core algorithms. */
int game_level_catalog_save(GameApplication *a,GameAddress p,ResourceFile *f){
    uint32_t method,marker=0;SaveRecords context={a,f};
    TaggedRecordStreamServices records={f,NULL,write_port,flush_port};
    GameplayRootArchiveServices services={&records,&context,save_member,NULL,NULL,NULL,NULL,NULL};
    if(!a || !p || !f || a->memory.fault)return 0;
    method=get(a,get(a,p)+0x78);if(a->memory.fault)return 0;
    switch(method){
    case 0x477900:case 0x4785f0:return gameplay_state_stream_root(&a->memory,0,&marker,&services,p);
    case 0x4791b0:return gameplay_state_save_player_root_base(&a->memory,&marker,&services,p);
    case 0x47a590:return game_level_entity_save(a,get(a,p+0x2e0),f) &&
        gameplay_state_save_player_root_base(&a->memory,&marker,&services,p);
    case 0x481a50:return gameplay_state_stream_player_root(&a->memory,0,&marker,&services,p);
    default:return 0;
    }
}
extern int game_level_citadel_activate(GameApplication *,GameAddress,GameAddress,int,uint16_t *);
extern int game_level_citadel_restore_member(GameApplication *,GameAddress,uint16_t,unsigned);
static int restore_member(void *,GameAddress,unsigned);
static int restore_variant(void *context,GameAddress owner,uint8_t kind,uint32_t *variant){
    SaveRecords *s=context;return game_level_player_catalog_value(s->app,owner,kind,3,0,variant);
}
static int restore_create(void *context,uint8_t kind,uint16_t variant,GameAddress *object){
    SaveRecords *s=context;*object=game_level_factory_take(s->app,kind,variant);
    return *object!=0 && !s->app->memory.fault;
}
static int restore_bind(void *context,GameAddress group,GameAddress catalog,uint32_t enabled){
    SaveRecords *s=context;uint16_t status;
    return game_level_citadel_activate(s->app,group,catalog,enabled!=0,&status);
}
static int restore_bind_member(void *context,GameAddress group,uint16_t identity,unsigned slot){
    SaveRecords *s=context;return game_level_citadel_restore_member(s->app,group,identity,slot);
}
static int restore_identity(void *context,uint32_t identity,GameAddress *result){
    SaveRecords *s=context;return restore_resolve(s->app,identity,result);
}
static GameplayRootArchiveServices restore_services(SaveRecords *context,const TaggedRecordStreamServices *records){
    GameplayRootArchiveServices services={records,context,restore_member,restore_variant,restore_create,
        restore_bind,restore_bind_member,restore_identity};return services;
}
static int restore_member(void *context,GameAddress object,unsigned operation){
    SaveRecords *s=context;GameApplication *a=s->app;uint32_t method,marker;unsigned family;
    TaggedRecordStreamServices records={s->file,read_port,NULL,NULL};
    GameplayRootArchiveServices services=restore_services(s,&records);int ok;
    if(operation==1)return game_level_entity_restore(a,object,s->file);
    if(operation==2)return game_level_entity_restore_fix(a,object);
    if(operation!=4 && operation!=5)return 0;
    /*The paired writer identifies the verified shared descriptor family. */
    method=get(a,get(a,object)+0x58);
    switch(method){
    case 0x42ec20:return !a->memory.fault;
    case 0x53d920:family=0;break;
    case 0x53e840:family=1;break;
    case 0x53f1e0:case 0x53fcb0:family=2;break;
    default:fprintf(stderr,"Unreconstructed command archive reader %08x\n",(unsigned)method);return 0;
    }
    if(operation==5)return gameplay_state_fix_group_member(&a->memory,family,&services,object);
    marker=get(a,0x63c964);ok=gameplay_state_stream_group_member(&a->memory,1,family,&marker,&services,object);
    game_memory_write_u32(&a->memory,0x63c964,marker);return ok && !a->memory.fault;
}
/*477af0,479250,47a5c0,481b70: load into existing catalog/primary owners.
 *Fixups are deliberately separate, after all participating catalogs exist. */
int game_level_catalog_restore(GameApplication *a,GameAddress p,ResourceFile *f){
    SaveRecords context={a,f};TaggedRecordStreamServices records={f,read_port,NULL,NULL};
    GameplayRootArchiveServices services=restore_services(&context,&records);uint32_t method,marker;int ok;
    if(!a || !p || !f || a->memory.fault)return 0;
    method=get(a,get(a,p)+0x78);marker=get(a,0x63c964);
    switch(method){
    case 0x477900:case 0x4785f0:ok=gameplay_state_stream_root(&a->memory,1,&marker,&services,p);break;
    case 0x4791b0:ok=gameplay_state_stream_player_root_base(&a->memory,1,&marker,&services,p);break;
    case 0x47a590:
        if(!game_level_entity_restore(a,get(a,p+0x2e0),f))return 0;
        marker=get(a,0x63c964);ok=gameplay_state_stream_player_root_base(&a->memory,1,&marker,&services,p);break;
    case 0x481a50:ok=gameplay_state_stream_player_root(&a->memory,1,&marker,&services,p);break;
    default:fprintf(stderr,"Unreconstructed catalog archive reader %08x\n",(unsigned)method);return 0;
    }
    game_memory_write_u32(&a->memory,0x63c964,marker);return ok && !a->memory.fault;
}
static int catalog_restore_fix(GameApplication *a,GameAddress p,ResourceFile *f){
    SaveRecords context={a,f};GameplayRootArchiveServices services=restore_services(&context,NULL);
    uint32_t method=get(a,get(a,p)+0x78);
    if(method==0x481a50)return gameplay_state_fix_player_root(&a->memory,&services,p);
    if(method==0x47a590 && !game_level_entity_restore_fix(a,get(a,p+0x2e0)))return 0;
    if(method!=0x477900 && method!=0x4785f0 && method!=0x4791b0 && method!=0x47a590)return 0;
    return gameplay_state_fix_root(&a->memory,&services,p);
}
static int path_segment(GameApplication *a,ResourceFile *f,GameAddress p){
    uint32_t count;GameAddress child;unsigned i;
    if(!p || !list_count(a,get(a,p+0xc),4,&count))return 0;
    for(i=0;i<3;i++)if(!f32(f,get(a,p+0x14+i*4)))return 0;
    if(!u32(f,get(a,p+0x20)) || !u32(f,count))return 0;
    for(child=get(a,p+0xc);child;child=get(a,child+4)){
        if(!u8(f,game_memory_read_u8(&a->memory,child+0xd)) ||
           !u8(f,game_memory_read_u8(&a->memory,child+0xc)) ||
           !memory_record(a,f,0xfedcba99u,child+0x18,1,64))return 0;
    }return !a->memory.fault;
}
/*454570: head metadata, current segment, then remaining sibling segments. */
int game_level_save_path(GameApplication *a,ResourceFile *f,GameAddress p){
    uint32_t count;GameAddress sibling;
    if(!a || !f || !p || !list_count(a,get(a,p+4),4,&count))return 0;
    if(!u8(f,game_memory_read_u8(&a->memory,p+0xc)) || !u8(f,game_memory_read_u8(&a->memory,p+0xd)) ||
       !u32(f,get(a,p+0x10)) || !f32(f,get(a,p+0x14)) || !f32(f,get(a,p+0x18)) ||
       !path_segment(a,f,get(a,p+0x1c)) || !u32(f,count))return 0;
    for(sibling=get(a,p+4);sibling;sibling=get(a,sibling+4))if(!path_segment(a,f,sibling))return 0;
    return !a->memory.fault;
}
/*4577c0: navigation working state and indexed path segments. */
int game_level_save_navigation(GameApplication *a,ResourceFile *f,GameAddress p){
    uint32_t limit;unsigned i;
    if(!a || !f || !p || a->memory.fault)return 0;
    if(!u32(f,get(a,p+0x1800)) || !u32(f,get(a,p+0x1804)) || !f32(f,get(a,p+0x1808)) ||
       !u32(f,get(a,p+0x180c)) || !f32(f,get(a,p+0x11830)))return 0;
    if(!get(a,p+0x1804))return !a->memory.fault;
    if(!game_level_save_path(a,f,p+0x1810) || !memory_record(a,f,0xfedcba99u,p,1,0x800) ||
       !memory_record(a,f,0xfedcba99u,p+0x800,1,0x1000) ||
       !u32(f,get(a,p+0x12034)) || !u32(f,get(a,p+0x12038)))return 0;
    limit=get(a,p+0x12034)+1u;
    if((int32_t)limit<=0)return !a->memory.fault;
    if(limit>(UINT32_MAX-p-0x1203c)/4)return 0;
    for(i=0;i<limit;i++)if(!path_segment(a,f,get(a,p+0x1203c+i*4)))return 0;
    return !a->memory.fault;
}
/*4fe650: motion-search working state; seven words, score and optional path. */
int game_level_save_motion(GameApplication *a,ResourceFile *f,GameAddress p){
    unsigned i;if(!a || !f || !p || a->memory.fault)return 0;
    for(i=0;i<7;i++)if(!u32(f,get(a,p+0x2000+i*4)))return 0;
    if(!f32(f,get(a,p+0x2024)))return 0;
    if(!get(a,p+0x2004))return !a->memory.fault;
    if(!memory_record(a,f,0xfedcba99u,p,1,0x2000) || !game_level_save_path(a,f,p+0x2028))return 0;
    for(i=0;i<4;i++)if(!u32(f,get(a,p+0x6048+i*4)))return 0;
    return !a->memory.fault;
}
/*4e1c20 uses raw words, unlike the tagged variables that follow in45a9e0. */
int game_level_save_script(GameApplication *a,ResourceFile *f){
    static const GameAddress globals[]={0x605890,0x604164,0x604160,0x60412c,0x6041b4,0x604184,
        0x60417c,0x60415c,0x6041a8,0x604198,0x6041b0,0x604154,0x604150,0x6041c0,0x6041a0,
        0x60418c,0x6041a4,0x604130,0x604194,0x604178,0x5ac690,0x5ac694,0x5ac698,0x5ac69c,0x604138,0x6041c4,0x604140,0x604134,
        0x604128,0x604190,0x60413c,0x604144,0x60419c,0x604170,0x604124,0x604180,0x6041c8,
        0x6041b8,0x604148,0x629730};
    uint32_t count;GameAddress p;unsigned i,kind;
    for(i=0;i<sizeof(globals)/sizeof(globals[0]);i++){
        uint8_t b[4];if(!game_memory_read(&a->memory,globals[i],b,4) || resource_file_write(f,b,4)!=4)return 0;
    }
    for(kind=0;kind<2;kind++){
        count=get(a,kind?0x605888:0x605884);if(!u32(f,count))return 0;
        if((int32_t)count<0)continue;
        for(i=0;i<count;i++){
            p=get(a,get(a,kind?0x60586c:0x605870)+i*4);
            if(!p || !memory_record(a,f,0xfedcbaa0u,p,1,32))return 0;
            if(kind){if(!memory_record(a,f,0xfedcba9bu,p+0x20,4,3))return 0;}
            else if(!u32(f,get(a,p+0x20)))return 0;
        }
    }
    p=get(a,0x605868);if(!list_count(a,p,0x28,&count) || !u32(f,count))return 0;
    for(;p;p=get(a,p+0x28)){
        if(!memory_record(a,f,0xfedcbaa0u,p,1,32) || !scalar(f,0xfedcba90u,get(a,p+0x24),4) ||
           !u8(f,game_memory_read_u8(&a->memory,p+0x2c)))return 0;
    }return !a->memory.fault;
}
static int truncated(long double n,int32_t *out){
    if(!isfinite(n) || n<(long double)INT32_MIN || n>(long double)INT32_MAX)return 0;
    *out=(int32_t)n;return 1;
}
/*4a3650: nearest-cell resampling of the current minimap and per-player triples.
 *Ratios are rounded to float before the original truncating conversions. */
int game_level_save_spatial(GameApplication *a,ResourceFile *f){
    int32_t width,height,source_width,source_height,x,y,sx,sy,count;uint8_t *pixels;
    float rx,ry;size_t size;GameAddress source,rows;int ok=0;unsigned i;
    if(!get(a,0x58de70))return !a->memory.fault;
    width=(int32_t)get(a,0x605440);height=(int32_t)get(a,0x605458);
    source_width=(int16_t)game_memory_read_u16(&a->memory,0x596100);
    source_height=(int16_t)game_memory_read_u16(&a->memory,0x596102);
    if(width<=0 || height<=0 || source_width<=0 || source_height<=0 || (size_t)height>SIZE_MAX/(size_t)width)return 0;
    size=(size_t)width*(size_t)height;if(size>UINT32_MAX)return 0;
    pixels=malloc(size);if(!pixels)return 0;
    rx=(float)((long double)source_width/width);ry=(float)((long double)source_height/height);
    source=get(a,0x596104);
    for(y=0;y<height;y++){
        if(!truncated((long double)y*ry,&sy) || sy<0 || sy>=source_height)goto done;
        for(x=0;x<width;x++){
            uint64_t index;if(!truncated((long double)x*rx,&sx) || sx<0 || sx>=source_width)goto done;
            index=(uint64_t)(uint32_t)sy*(uint32_t)source_width+(uint32_t)sx;
            if(index>UINT32_MAX-source)goto done;
            pixels[(size_t)y*width+x]=game_memory_read_u8(&a->memory,source+(uint32_t)index);
        }
    }
    if(a->memory.fault || !record(f,0xfedcba99u,pixels,1,size))goto done;
    count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);rows=get(a,0x602698);
    for(i=0;i<(unsigned)(count>0?count:0);i++){
        if(!scalar(f,0xfedcba94u,get(a,rows+i*12),4) ||
           !scalar(f,0xfedcba94u,get(a,rows+i*12+4),4) || !u32(f,get(a,rows+i*12+8)))goto done;
    }ok=!a->memory.fault;
done:free(pixels);return ok;
}
/*45ab00 reuses the established script definitions and native archive codec. */
static int restore_script(GameApplication *a,ResourceFile *f){
    TaggedRecordStreamServices records={f,read_port,NULL,NULL};GameplayEntityArchiveResult result;
    uint32_t marker=get(a,0x63c964);
    int ok=gameplay_state_stream_entities(&a->memory,1,&marker,&records,restore_resolve,a,&result);
    game_memory_write_u32(&a->memory,0x63c964,marker);return ok && !a->memory.fault;
}
static int restore_map_record(void *context,uint8_t *bytes,size_t count){
    SaveRecords *s=context;return restore_bytes(s->app,s->file,bytes,1,count);
}
static int restore_spatial(GameApplication *a,ResourceFile *f){
    SaveRecords context={a,f};GameRadarArchiveServices services={&context,NULL,restore_map_record};
    return game_radar_map_load(&a->memory,&services);
}
/*45a880 ->45a560: every catalog is read before identity fixups, followed by
 *groups, global runtime values, navigation/motion, factions, scripts and map.
 *The archive handle resolves to an owned full-width ResourceFile. */
int game_level_restore_environment(GameApplication *a,GameAddress archive){
    ResourceFile *f;SaveRecords context;TaggedRecordStreamServices records;
    GameplayRootArchiveServices roots;GameplayContainerArchiveServices containers;
    GameAddress catalogs[3],groups,group;uint32_t marker,offset=0;int32_t count;unsigned i=0;
    const char *stage="group state";
    if(!a || a->memory.fault || !(f=game_application_archive_file(a,archive)))return 0;
    context.app=a;context.file=f;records=(TaggedRecordStreamServices){f,read_port,NULL,NULL};
    roots=restore_services(&context,&records);
    containers=(GameplayContainerArchiveServices){&records,NULL,NULL,NULL,NULL};
    count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);groups=get(a,0x639a70);
    for(i=0;count>0 && i<(unsigned)count;i++){
        group=get(a,groups+i*4);if(!group || !restore_field(a,f,group+0x14,4,1))goto failed;
    }
    catalogs[0]=get(a,0x5f0208);catalogs[1]=get(a,0x5ac678)?get(a,0x5f0220):0;
    catalogs[2]=get(a,0x5ac67c)?get(a,0x5f0224):0;
    stage="catalog records";if(!catalogs[0])goto failed;
    for(i=0;i<3;i++)if(catalogs[i]){
        game_memory_write_u32(&a->memory,0x589740,catalogs[i]);
        if(!game_level_catalog_restore(a,catalogs[i],f))goto failed;
    }
    stage="catalog identities";
    for(i=0;i<3;i++)if(catalogs[i] && !catalog_restore_fix(a,catalogs[i],f))goto failed;
    stage="group identities";
    for(i=0;count>0 && i<(unsigned)count;i++)if(!gameplay_state_fix_group(&a->memory,&roots,get(a,groups+i*4)))goto failed;
    /*These are four distinct tagged records, not one array record. */
    stage="runtime globals";
    if(!restore_field(a,f,0x603744,2,1) || !restore_field(a,f,0x604114,4,1) ||
       !restore_field(a,f,0x6040a4,4,1) || !restore_field(a,f,0x6040fc,4,1))goto failed;
    marker=get(a,0x63c964);
    stage="navigation/motion";
    if(!gameplay_state_stream_grid(&a->memory,1,0,&marker,&containers,0x5cf038) ||
       !gameplay_state_stream_grid(&a->memory,1,1,&marker,&containers,0x60a4c8))goto failed;
    game_memory_write_u32(&a->memory,0x63c964,marker);
    stage="factions";if(!gameplay_state_stream_factions(&a->memory,1,&roots))goto failed;
    stage="gate route flags";if(!game_level_gate_restore_route_flags(a))goto failed;
    stage="script";if(!restore_script(a,f))goto failed;
    stage="map";if(!restore_spatial(a,f) || a->memory.fault)goto failed;
    return 1;
failed:
    resource_file_tell(f,&offset);
    fprintf(stderr,"Native restore failed: %s index=%u offset=%u fault=%d\n",stage,i,offset,a->memory.fault);
    return 0;
}
/*45a200 and45a3a0 share the identical runtime serialization sequence. */
int game_level_save_footer(GameApplication *a,ResourceFile *f){
    GameAddress player,rows,p;uint32_t length=0;int32_t count;unsigned i;uint32_t entities;
    if(!a || !f || a->memory.fault)return 0;
    do{if(length==4096)return 0;++length;}while(game_memory_read_u8(&a->memory,0x5efe80+length-1));
    if(!u32(f,length) || !memory_record(a,f,0xfedcbaa0u,0x5efe80,1,length) ||
       !memory_record(a,f,0xfedcba9au,0x5ac678,4,10))return 0;
    if(!memory_record(a,f,0xfedcba9au,get(a,0x5f0220)+0x29c,4,17) ||
       !memory_record(a,f,0xfedcba9au,get(a,0x5f0224)+0x1fc,4,13) ||
       !memory_record(a,f,0xfedcba9au,get(a,0x5f0208)+0x29c,4,32))return 0;
    count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);rows=get(a,0x639a70);
    for(i=0;i<(unsigned)(count>0?count:0);i++)if(!u32(f,get(a,get(a,rows+i*4)+0x14)))return 0;
    player=get(a,0x5f0208);if(!game_level_catalog_save(a,player,f))return 0;
    if(get(a,0x5ac678) && !game_level_catalog_save(a,get(a,0x5f0220),f))return 0;
    if(get(a,0x5ac67c) && !game_level_catalog_save(a,get(a,0x5f0224),f))return 0;
    if(!scalar(f,0xfedcba97u,game_memory_read_u16(&a->memory,0x603744),2) ||
       !f32(f,get(a,0x604114)) || !f32(f,get(a,0x6040a4)) || !f32(f,get(a,0x6040fc)) ||
       !game_level_save_navigation(a,f,0x5cf038) || !game_level_save_motion(a,f,0x60a4c8))return 0;
    entities=get(a,0x5e147c);rows=get(a,0x5e1480);
    for(i=0;(int32_t)entities>0 && i<entities;i++){
        p=get(a,get(a,rows+i*4)+0x20);{uint32_t ignored;if(!list_count(a,p,0xcc,&ignored))return 0;}
        for(;p;p=get(a,p+0xcc))if(!game_level_entity_save(a,p,f))return 0;
    }
    return game_level_save_script(a,f) && game_level_save_spatial(a,f) && u32(f,get(a,0x5ac6ac)) && !a->memory.fault;
}
int game_level_save_scenario(GameApplication *a,const char *name){
    char path[4128];ResourceFile *f;int ok;
    if(!a || !name || a->memory.fault)return 0;
    if(!game_application_file_path(a,name,1,path,sizeof(path)))return 0;
    f=resource_file_open_mode(path,"wb");if(!f)return 0;
    ok=game_level_save_footer(a,f);resource_file_close(f);return ok;
}
/*530260 preserves the campaign prefix in saves/temp, records its end offset,
 *and appends the real runtime footer. Both streams have local host owners. */
int game_menu_saved_snapshot_export(GameApplication *a,const char *name){
    char input[4128],output[4128],relative[160];ResourceFile *in=NULL,*out=NULL;
    uint8_t b[4096];uint32_t length,remaining;int n,ok=0;
    if(!a || !name || a->memory.fault)return 0;
    n=snprintf(relative,sizeof(relative),"saves\\%s.sav",name);
    if(n<0 || (size_t)n>=sizeof(relative) || !game_application_file_path(a,"saves\\temp",0,input,sizeof(input)) ||
       !game_application_file_path(a,relative,1,output,sizeof(output)))return 0;
    in=resource_file_open_mode(input,"rb");if(!in)goto done;
    if(resource_file_read(in,b,4)!=4)goto done;
    length=(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;
    if(!length && (!resource_file_seek(in,0,2) || !resource_file_tell(in,&length)))goto done;
    if(length<4 || length>INT32_MAX || !resource_file_seek(in,4,0))goto done;
    out=resource_file_open_mode(output,"wb");if(!out)goto done;
    for(n=0;n<4;n++)b[n]=(uint8_t)(length>>(n*8));
    if(resource_file_write(out,b,4)!=4)goto done;
    remaining=length-4;
    while(remaining){size_t chunk=remaining<sizeof(b)?remaining:sizeof(b);
        if(resource_file_read(in,b,chunk)!=chunk || resource_file_write(out,b,chunk)!=chunk)goto done;
        remaining-=(uint32_t)chunk;
    }
    ok=game_level_save_footer(a,out);
done:if(out)resource_file_close(out);if(in)resource_file_close(in);return ok;
}
