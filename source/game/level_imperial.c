#include "level_load.h"
#include "level_player_resources.h"
#include "presentation_instance.h"
#include "presentation_lifecycle.h"
#include "recyclable_records.h"
#include "object_defaults.h"
#include "scene_projection_storage.h"
#include "level_factory.h"
#include "model_assets.h"
#include <string.h>

/*47a340 ->4b3aa0 ->4b4110/4be070. The faction's2e0 slot owns the
 *bounded imperial record; its arrays and embedded route own their allocations.
 *Presentation and recyclable records retain their existing native owners. */
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t b=get(a,p);float f;memcpy(&f,&b,4);return f;}
static int32_t signed_word(uint32_t b){int32_t v;memcpy(&v,&b,4);return v;}
static const uint16_t array_slots[]={0x2c8,0x314,0x30c,0x310,0x318,0x31c,0x320,0x280,0x284,0x288,0x324,0x328,
    0x3b0,0x3b4,0x3b8,0x3bc,0xe1c,0xe20,0xe24,0xe28,0xe2c,0xe30,0xe34,0xe38,0xe3c,0xe40,0xe44,0xf64};
static int free_slot(GameApplication *a,GameAddress slot){GameAddress p=get(a,slot);
    if(a->memory.fault || (p && !scene_projection_storage_free(p)))return 0;put(a,slot,0);return !a->memory.fault;}
static int free_page(void *c,GameAddress p){(void)c;return !p || scene_projection_storage_free(p);}
static int model_release(void *c,GameAddress p){return !p || game_application_scene_model_release(c,p);}
static int pool_release(void *c,GameAddress p){return game_level_factory_release_pool_member(c,p);}
static int packet_release(void *c,GameAddress p){return game_level_factory_detach_packet_chain(c,p);}
/*4b3e30/4c0b60: arrays,28 lists,embedded controller and shared presentation.
 *Clear each retired slot so ordinary failure cleanup can be retried. */
int game_level_imperial_release(GameApplication *a,GameAddress root){
    GameAddress p=get(a,root+0x2e0);unsigned i;PresentationReleaseServices services={a,model_release,free_page,pool_release,packet_release};
    if(a->memory.fault)return 0;if(!p)return 1;
    if(get(a,p+0x13c)){put(a,get(a,p+0x13c),0x572dc0);if(!free_slot(a,p+0x13c))return 0;}
    for(i=0;i<sizeof(array_slots)/sizeof(array_slots[0]);i++)if(!free_slot(a,p+array_slots[i]))return 0;
    for(i=0;i<28;i++){GameAddress slot=p+0x430+i*4,list=get(a,slot);
        if(list && !recyclable_outer_retire(&a->memory,list,NULL))return 0;put(a,slot,0);}
    if(get(a,p+0x1c8)==0x5730e0){
        if(!recyclable_projection_destroy(&a->memory,p+0x1c8,NULL))return 0;put(a,p+0x1c8,0);}
    if(presentation_instance_find(p) && presentation_instance_release(&a->memory,p,&services,1,1)!=1)return 0;
    if(a->memory.fault || !scene_projection_storage_free(p))return 0;put(a,root+0x2e0,0);return !a->memory.fault;
}
static int strategy_initialize(GameApplication *a,GameAddress p,GameAddress root){
    static const struct {uint16_t slot;uint32_t value;} defaults[]={
        {0xc4,640},{0x38,1920},{0x28,160},{0x44,320},{0x3c,320},{0x24,1},{0x70,2},{0x80,1},
        {0x90,480},{0x64,70},{0x20,1},{0x30,1},{0x60,50},{0x94,1},{0xc8,2},{0x88,0},{0x40,2},
        {0xb8,3},{0x34,3},{0xa8,3},{0x5c,4},{0x98,4},{0xb0,2},{0x54,1},{0x50,1},{0xa0,1},
        {0xa4,3840},{0xac,9000},{0x48,6},{0x8c,5},{0xb4,0x40200000},{0x84,0x3f000000},{0x7c,0x40c00000}};
    static const uint16_t zero_arrays[]={0x30c,0x2c8,0x310,0x318,0x31c,0x320,0x324,0x328,0x284,0x280,0x288};
    static const uint16_t minus_arrays[]={0x3b0,0x3b4,0x3b8,0x3bc,0xe40,0xe44};
    static const uint16_t zero_ranges[]={0x62c,0x9ac,0x7ec,0xb6c};
    unsigned i,j;int count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);int32_t maximum=0;
    if(count<0)return 0;
    if(!get(a,0x5ac67c))for(i=0;i<sizeof(defaults)/sizeof(defaults[0]);i++)put(a,0x604100+defaults[i].slot,defaults[i].value);
    if(signed_word(get(a,root+0x2dc))>=0){
        GameAddress gun=scene_projection_storage_allocate(0x2c,"imperial gun"),catalog=get(a,root+0x1fc);
        if(!gun || !catalog){if(gun)scene_projection_storage_free(gun);return 0;}
        put(a,p+0x13c,gun);put(a,gun,0x572dd8);put(a,gun+4,p);put(a,gun+8,0);put(a,gun+12,get(a,catalog+0x28));
        game_memory_write_u16(&a->memory,gun+0x10,3);put(a,gun+0x14,catalog);put(a,gun+0x18,0);put(a,gun+0x1c,0);
        /*Retail passes an uninitialized local position here. Define it rather
         *than importing undefined stack contents into portable execution. */
        for(i=0;i<3;i++)put(a,gun+0x20+i*4,0);
    }
    for(i=0;i<(unsigned)count;i++){GameAddress site=get(a,get(a,0x639a70)+i*4);int32_t n=(int16_t)game_memory_read_u16(&a->memory,site+0x24);if(n>maximum)maximum=n;}
    put(a,0x60416c,(uint32_t)maximum);put(a,p+0x334,0);put(a,p+0x32c,0);put(a,p+0x330,UINT32_MAX);
    if(!recyclable_projection_allocate_arrays(&a->memory,p+0x1c8,NULL))return 0;
    put(a,p+0x26c,get(a,p+0x234));put(a,p+0x270,get(a,p+0x230));put(a,p+0x274,get(a,p+0x238));
    for(i=0;i<sizeof(array_slots)/sizeof(array_slots[0]);i++){
        size_t stride=(i==1)?12:(i==0 || i==2 || i==3 || i==4)?32:4;
        GameAddress array=count?scene_projection_storage_allocate((size_t)count*stride,"imperial site work"):0;
        if(count && !array)return 0;put(a,p+array_slots[i],array);
    }
    put(a,p+0x2b4,0);put(a,p+0x2ac,0xe100);put(a,p+0xd94,get(a,0x604188));
    if(get(a,p+0xd94)==0xfffffffe){
        uint32_t index=get(a,0x6308c0);if(index==1024)index=0;if(index>=1024)return 0;
        put(a,0x6308c0,index+1);put(a,p+0xd94,0u-presentation_lifecycle_phase(real(a,0x6308c8+index*4),real(a,0x5746a0)));
    }
    for(i=0;i<sizeof(zero_arrays)/sizeof(zero_arrays[0]);i++){
        unsigned stride=(zero_arrays[i]==0x30c || zero_arrays[i]==0x2c8 || zero_arrays[i]==0x310 || zero_arrays[i]==0x318)?8:1;
        /*These three arrays hold unassigned indices, rather than zero counts. */
        uint32_t value=(zero_arrays[i]==0x31c || zero_arrays[i]==0x320 || zero_arrays[i]==0x324)?UINT32_MAX:0;
        for(j=0;j<(unsigned)count*stride;j++)put(a,get(a,p+zero_arrays[i])+j*4,value);
    }
    for(i=0;i<sizeof(minus_arrays)/sizeof(minus_arrays[0]);i++)for(j=0;j<(unsigned)count;j++)put(a,get(a,p+minus_arrays[i])+j*4,UINT32_MAX);
    for(i=0;i<4;i++)for(j=0;j<112;j++)put(a,p+zero_ranges[i]+j*4,0);
    for(i=0;i<14;i++)put(a,p+0xdd8+i*4,0);
    return !a->memory.fault;
}
int game_level_imperial_initialize(GameApplication *a,GameAddress root){
    static const uint16_t zero[]={0xd9c,0x13c,0x140,0x5f0,0x70,0x2c8,0x30c,0x31c,0x320,0x280,0x284,0x288,0x324,0x328,0x258,0x25c,0x260,0x264,0x338,
        0xd7c,0xd80,0xd84,0xd88,0xd8c,0xd90,0xd94,0x1a4,0x1c4,0xe14,0xe18,0xe10,0xe48,0xf54,0xf58,0x50c,0x508,0x2bc,0x2c0,0x2c4};
    static const struct {uint16_t slot;uint32_t value;} defaults[]={{0xd68,2},{0xd6c,10},{0xd70,4},{0xd74,1},{0xd78,3},
        {0xd3c,2},{0xd40,10},{0xd44,4},{0xd48,1},{0xd4c,3},{0xd50,5},{0xd54,9},{0xd58,7},{0xd5c,6},{0xd60,8},{0xd64,8},{0xd98,0x7fffffff},{0xf5c,UINT32_MAX}};
    PresentationInstanceDefinition definition={0};GameAddress p;uint32_t sequence;unsigned i;
    if(!a || !root || a->memory.fault || get(a,root+0x2e0))return 0;
    p=scene_projection_storage_allocate(0xf68,"imperial strategy");if(!p)return 0;
    put(a,root+0x2e0,p); /*Ownership precedes every fallible constructor. */
    if(!presentation_instance_create(&a->memory,p,&definition,1) || !recyclable_projection_construct(&a->memory,p+0x1c8,NULL))goto fail;
    put(a,p,0x574710);
    /*479510 is the root's real3c setter, not its catalog-choice callback. */
    if(get(a,get(a,root)+0x3c)!=0x479510)goto fail;
    put(a,root+0x22c,get(a,0x6041ac));
    for(i=0;i<sizeof(zero)/sizeof(zero[0]);i++)put(a,p+zero[i],0);
    put(a,p+0x278,p+0x1f4);put(a,p+0x27c,p+0x1f8);put(a,p+0x1f4,UINT32_MAX);put(a,p+0x1f8,0);
    for(i=0;i<14;i++){
        GameAddress first=recyclable_outer_acquire(&a->memory,0,NULL);if(!first)goto fail;put(a,p+0x430+i*4,first);
        first=recyclable_outer_acquire(&a->memory,0,NULL);if(!first)goto fail;put(a,p+0x468+i*4,first);
    }
    for(i=0;i<24;i++)put(a,p+0x144+i*4,0);
    for(i=0;i<4;i++)put(a,p+0xf44+i*4,0);
    if(!game_object_clear_default_ranges(&a->memory,p,0))goto fail;
    for(i=0;i<sizeof(defaults)/sizeof(defaults[0]);i++)put(a,p+defaults[i].slot,defaults[i].value);
    for(i=0;i<28;i++)game_memory_write_u8(&a->memory,p+0x1a8+i,0);
    game_memory_write_u8(&a->memory,p+12,0xfe);put(a,p+0x10,0x3860);
    sequence=get(a,root+0x18);put(a,root+0x18,sequence+1);put(a,p+0x14,(sequence<<6)|game_memory_read_u16(&a->memory,root+8));
    if(!strategy_initialize(a,p,root))goto fail;
    if(get(a,0x604120)){
        if(!(get(a,p+0x1a4)&0x100) && !get(a,0x590a6c)){put(a,p+0xdc0,get(a,0x5ac6ac));
            game_memory_write_u8(&a->memory,p+0x1b0,0);game_memory_write_u8(&a->memory,p+0x1be,0);put(a,p+0x1a4,get(a,p+0x1a4)|0x100);}
        put(a,p+0xd9c,get(a,p+0xd9c)+1);
    }
    return !a->memory.fault;
fail:
    (void)game_level_imperial_release(a,root);return 0;
}
