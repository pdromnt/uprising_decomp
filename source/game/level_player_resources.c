#include "level_player_resources.h"
#include "level_factory.h"
#include "effect_resource_pool.h"
#include "level_run.h"
#include "model_assets.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const uint16_t slots[]={0x19c,0x1a4,0x1ac,0x1b0,0x1b4,0x1a0,0x1a8,0x1b8,0x1bc,0x1c0,0x1c4,0x1c8,0x1cc,0x1d0,0x1d4,0x1d8,0x1e0,0x1e4,0x1e8,0x1ec,0x1f0,0x1f4};
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static int32_t signed_word(uint32_t word){int32_t value;memcpy(&value,&word,4);return value;}
int game_level_player_base_resources_release(GameApplication *a,GameAddress player){
    EffectResourcePool *pool=effect_resource_pool_find(0x5ad7d0);unsigned i;
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i){
        GameAddress id=get(a,player+slots[i]);if(a->memory.fault)return 0;
        if(id && (!pool || effect_resource_pool_return_chain(&a->memory,pool,id)!=1))return 0;
        game_memory_write_u32(&a->memory,player+slots[i],0);
    }return !a->memory.fault;
}
/*43b410 receives fourteen real arguments. Recover those arguments from the
 *original catalog/descriptor record; only the existing pool owns their state. */
static int configure(GameApplication *a,GameAddress player,unsigned slot,unsigned pool_kind,GameAddress source,int raw,unsigned type,int alternate,uint32_t final){
    static const uint8_t catalog_offsets[]={0x24,0x38,0x28,0x34,0x10,0x30,0x2c,0,0x3c,0x44,0x48,0x40,0x4c};
    static const uint8_t raw_offsets[]={0,0x14,0,0x10,4,0xc,8,0x18,0x1c,0x24,0x28,0x20,0x2c};
    uint32_t words[14]={0};GameStateRecordConfig config;unsigned i;
    EffectResourcePool *pool=effect_resource_pool_find(0x5ad7d0);GameAddress id;EffectResource *record;
    if(!pool || get(a,player+slot))return 0;
    if(raw==3)words[7]=1; /*The empty command resource carries one enabled bit. */
    else if(raw==4){words[2]=get(a,source);words[3]=get(a,source+0x10);words[5]=get(a,source+0xc);words[7]=get(a,source+0x18);}
    else for(i=0;i<13;++i){
        if(raw && i==0)words[i]=type;
        else if(i==7 && (!raw || (raw==1 && type!=0x15)))words[i]=UINT32_MAX;
        else words[i]=get(a,source+(raw?raw_offsets[i]+(alternate?52u:0u):catalog_offsets[i]+(alternate?84u:0u)));
    }
    words[13]=final;if(a->memory.fault)return 0;
    config.type=(uint8_t)words[0];config.second=words[1];config.third=words[2];config.fourth=signed_word(words[3]);
    config.fifth=words[4];config.sixth=words[5];config.seventh=words[6];config.eighth=words[7];
    config.ninth=(uint16_t)words[8];config.tenth=words[9];config.eleventh=words[10];config.twelfth=(uint16_t)words[11];
    config.thirteenth=signed_word(words[12]);config.fourteenth=words[13];
    id=effect_resource_pool_acquire_kind(&a->memory,pool,pool_kind);record=effect_resource_find(id);
    if(!id || !record)return 0;
    if(!effect_resource_configure(record,&config)){(void)effect_resource_pool_return_chain(&a->memory,pool,id);return 0;}
    game_memory_write_u32(&a->memory,player+slot,id);return !a->memory.fault;
}
static int template(GameApplication *a,unsigned type,uint32_t variant){return game_level_factory_template(a,(uint8_t)type,(uint16_t)variant)!=0;}
static int fill(GameApplication *a,unsigned type,uint32_t variant,int count){return template(a,type,variant) && game_level_factory_fill(a,(uint8_t)type,(uint16_t)variant,count);}
static int weapon_templates(GameApplication *a,GameAddress player,uint8_t type){
    uint32_t main,kind,variant;
    return game_level_player_catalog_value(a,player,type,3,0,&main) &&
        game_level_player_catalog_value(a,player,type,9,0,&kind) &&
        game_level_player_catalog_value(a,player,type,14,0,&variant) &&
        template(a,type,main) && fill(a,(uint8_t)kind,variant,2);
}
static int bank(GameApplication *a,GameAddress player,uint8_t type,unsigned slot,unsigned kind,int alternate,uint32_t final){
    GameAddress source;return game_level_player_catalog_record(a,player,type,&source) && configure(a,player,slot,kind,source,0,0,alternate,final);
}
static int descriptor_pair(GameApplication *a,GameAddress player,uint8_t type,const GameAddress bases[3],unsigned slot,unsigned pool_kind,unsigned record_type,int prepare){
    uint32_t choice;unsigned faction=game_memory_read_u16(&a->memory,player+8);GameAddress source;
    if(!game_level_player_catalog_choice(a,player,type,&choice))return 0;
    source=bases[faction==4?1:faction==5?2:0]+choice*112u;
    if(prepare && !template(a,record_type,get(a,source+0x14)))return 0;
    return configure(a,player,slot,pool_kind,source,1,record_type,0,0) && configure(a,player,slot+4,pool_kind+(pool_kind==6?0:1),source,1,record_type,1,0);
}
/*4769a0/477df0: construct the actual shared weapon/effect resource set and
 *its factory dependencies. Allocation failures retire successful resources;
 *templates retain their existing independent factory/cache owners. */
int game_level_player_base_prepare(GameApplication *a,GameAddress player){
    static const GameAddress guns[3]={0x5e7650,0x5e67e0,0x5e8380},tank_effects[3]={0x5ea1e8,0x5e5c10,0x5e9498};
    static const GameAddress aircraft_effects[3]={0x5e3ec8,0x5ef258,0x5e1ee8};
    static const GameAddress repair[3]={0x5eda38,0x5ecba8,0x5e6e30},bursts[3]={0x5e9268,0x5e1cb8,0x5e4d20},secondary[3]={0x5ec348,0x5eb1e8,0x5eaf48};
    unsigned i;uint32_t selected,variant;for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i)if(get(a,player+slots[i]))return 0;
    game_memory_write_u32(&a->memory,player+0x1f8,0);
    if(!weapon_templates(a,player,0x40) || !game_level_player_catalog_mount(a,player,9,0x40,5,&selected) ||
       !fill(a,9,(uint16_t)selected,2) || !fill(a,0x15,(uint16_t)selected,1) ||
       !bank(a,player,0x40,0x19c,7,0,0) || !bank(a,player,0x40,0x1a0,8,1,0) ||
       !descriptor_pair(a,player,0x40,guns,0x1bc,6,9,0) ||
       !descriptor_pair(a,player,0x40,tank_effects,0x1c4,23,0x15,0) ||
       !weapon_templates(a,player,0x21) || !bank(a,player,0x21,0x1a4,5,0,0) || !bank(a,player,0x21,0x1a8,5,1,0) ||
       !descriptor_pair(a,player,0x21,aircraft_effects,0x1ac,21,0x15,0) ||
       !weapon_templates(a,player,0x27) || !bank(a,player,0x27,0x1b4,4,0,0) || !bank(a,player,0x27,0x1b8,4,1,0))goto fail;
    if(!game_level_player_catalog_value(a,player,0x26,3,0,&selected) || !game_level_player_catalog_value(a,player,0x26,14,0,&variant) ||
       !template(a,0x26,selected) || !fill(a,4,variant,3) || !fill(a,14,variant,4) ||
       !game_level_player_catalog_value(a,player,0x29,3,0,&selected) || !template(a,0x29,selected) ||
       !weapon_templates(a,player,0x22) || !bank(a,player,0x22,0x1cc,3,0,0) ||
       !weapon_templates(a,player,0x25) || !bank(a,player,0x25,0x1d0,2,0,0) ||
       !weapon_templates(a,player,0x28) || !bank(a,player,0x28,0x1d4,8,0,0) ||
       !weapon_templates(a,player,0x24) || !bank(a,player,0x24,0x1d8,1,0,0x14000) ||
       !descriptor_pair(a,player,0x26,repair,0x1e0,9,2,0) ||
       !descriptor_pair(a,player,0x27,bursts,0x1e8,11,12,1) ||
       !descriptor_pair(a,player,0x27,secondary,0x1f0,9,2,0))goto fail;
    return !a->memory.fault;
fail:
    (void)game_level_player_base_resources_release(a,player);return 0;
}
static int release_slot(GameApplication *a,GameAddress slot){
    GameAddress id=get(a,slot);EffectResourcePool *pool=effect_resource_pool_find(0x5ad7d0);
    if(a->memory.fault || (id && (!pool || effect_resource_pool_return_chain(&a->memory,pool,id)!=1)))return 0;
    game_memory_write_u32(&a->memory,slot,0);return !a->memory.fault;
}
int game_level_player_resources_release(GameApplication *a,GameAddress player){
    static const uint16_t extra[]={0x470,0x478,0x4a0,0x4a4,0x4a8,0x47c,0x480,0x484,0x488,0x48c,0x474,0x490,0x494,0x498,0x1fc,0x4ac,0x4b0,0x49c,0x1dc};unsigned i;
    /*The AI uses1fc as a catalog choice, while the scene class publishes an
     *owned resource there. Do not reinterpret a different class's fields. */
    {uint32_t method=get(a,get(a,player)+0x1c);
        if(method==0x479730){
            return release_slot(a,player+0x1fc) && release_slot(a,player+0x1dc) &&
                game_level_player_base_resources_release(a,player);
        }
        if(method==0x478c10)return release_slot(a,player+0x1dc) && game_level_player_base_resources_release(a,player);
        if(method!=0x47b0f0)return game_level_player_base_resources_release(a,player);
    }
    for(i=0;i<sizeof(extra)/sizeof(extra[0]);++i)if(!release_slot(a,player+extra[i]))return 0;
    return game_level_player_base_resources_release(a,player);
}
/*478c10: shared scene/imperial prerequisites, including the original compact
 *availability checks. Their factory owners remain independent of resource IDs. */
static int faction_prepare(GameApplication *a,GameAddress player){
    static const uint8_t types[]={0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x89,0x88};
    uint32_t selected,choice;unsigned i;
    if(!game_level_player_base_prepare(a,player))return 0;
    for(i=0;i<11;++i)game_memory_write_u32(&a->memory,player+0x200+i*4,0);
    if(!game_level_player_catalog_value(a,player,0x2a,3,0,&selected) || !template(a,0x2a,selected) ||
       !weapon_templates(a,player,0x23) || !bank(a,player,0x23,0x1dc,0,0,0))return 0;
    for(i=0;i<sizeof(types);++i){
        if(!game_level_player_catalog_choice(a,player,types[i],&choice))return 0;
        if(choice&0x80000000u)continue;
        if(!game_level_player_catalog_value(a,player,types[i],3,0,&selected) || !fill(a,types[i],(uint16_t)selected,2))return 0;
        if(types[i]==0x89 && !fill(a,11,0,2))return 0;
    }return !a->memory.fault;
}
/*41a300: one command model definition, owned by the published global slots.
 *Transient dimensions/radius stay local; no parallel serialized definition. */
int game_level_restore_player_command(GameApplication *a,unsigned slot){
    GameModelDefinition definition={0};
    if(slot>=10)return 0;
    if(get(a,0x5acd38+slot*4))return !a->memory.fault;
    if(get(a,0x5acd10+slot*4) || get(a,0x5acce8+slot*4) || a->memory.fault)return 0;
    if(!game_model_definition_read(a,ASSET_MODEL_SIMPLE,(uint16_t)slot,0x583db0,0,0,0,&definition))return 0;
    game_memory_write_u32(&a->memory,0x5acd10+slot*4,definition.markers);
    game_memory_write_u32(&a->memory,0x5acce8+slot*4,definition.marker_count);
    game_memory_write_u32(&a->memory,0x5acd38+slot*4,definition.model);
    return !a->memory.fault;
}
int game_level_player_command_models_release(GameApplication *a){
    unsigned i;for(i=0;i<10;++i){GameModelDefinition definition={0};
        definition.format=ASSET_MODEL_SIMPLE;definition.model=get(a,0x5acd38+i*4);
        definition.markers=get(a,0x5acd10+i*4);definition.marker_count=get(a,0x5acce8+i*4);
        if(a->memory.fault || !game_model_definition_release(a,&definition))return 0;
        game_memory_write_u32(&a->memory,0x5acd38+i*4,0);game_memory_write_u32(&a->memory,0x5acd10+i*4,0);game_memory_write_u32(&a->memory,0x5acce8+i*4,0);
    }return !a->memory.fault;
}
/*480500: the player's actual ordered command defaults. */
static int command_defaults(GameApplication *a,GameAddress player){
    static const uint8_t defaults[98]={5,3,2,1,6,4,0,3,2,1,7,4,5,0,3,2,2,4,5,0,0,3,2,4,1,5,0,0,7,1,4,2,3,5,0,2,1,7,5,3,4,0,2,3,4,7,1,5,0,6,3,2,5,1,0,0,6,3,2,5,1,0,0,4,3,2,5,1,7,0,2,5,3,1,4,0,0,7,1,4,2,3,5,0,7,1,4,2,3,5,0,7,1,4,2,3,5,0};
    unsigned i;for(i=0;i<4;++i)game_memory_write_u32(&a->memory,player+0x4cc+i*4,get(a,player+0x4b4+i*4));
    for(i=0;i<3;++i){game_memory_write_u32(&a->memory,player+0x4dc+i*4,0);game_memory_write_u32(&a->memory,player+0x4e8+i*4,1);}
    game_memory_write_u32(&a->memory,player+0x4c8,0);return game_memory_write(&a->memory,player+0x518,defaults,sizeof(defaults));
}
static uint32_t integer_word(long double value){
    value=truncl(value);return !isfinite(value) || value<-9223372036854775808.0L || value>=9223372036854775808.0L?0:(uint32_t)(uint64_t)(int64_t)value;
}
static int player_sample(GameApplication *a,GameAddress player){
    uint32_t index=get(a,0x6308c0),lower=get(a,player+0x5fc),upper=get(a,player+0x600),bits;float sample;
    if(index==1024)index=0;if(index>=1024)return 0;
    bits=get(a,0x6308c8+index*4);memcpy(&sample,&bits,4);
    game_memory_write_u32(&a->memory,0x6308c0,index+1);
    game_memory_write_u32(&a->memory,player+0x5a4,lower+integer_word((long double)signed_word(upper-lower)*sample));return !a->memory.fault;
}
typedef struct PlayerPartRecipe {uint16_t slot,choice;uint8_t pool_kind,type,mode,optional;GameAddress records;} PlayerPartRecipe;
/*47b0f0: these records are consumed directly by4f4f60's fifteen heterogeneous
 *children and the player/effect update paths. IDs borrow the same pool owner. */
static int scene_prepare(GameApplication *a,GameAddress player){
    static const uint16_t clear[]={0x604,0x580,0x584,0x508,0x504,0x500,0x514,0x510,0x50c,0x5a0,0x58c,0x590};
    static const struct {uint16_t choice;uint8_t item,count;} optional_templates[]={
        {0x30c,19,20},{0x314,21,20},{0x318,22,20},{0x2f0,3,5},{0x2f4,7,5},{0x2f8,8,5},
        {0x310,20,5},{0x2fc,13,5},{0x2dc,9,5},{0x300,12,5},{0x304,15,5},{0x308,17,5}};
    static const PlayerPartRecipe parts[]={
        {0x470,0x2e4,15,2,2,0,0x5e4bd0},{0x478,0x2e8,14,6,2,0,0x5eb988},
        {0x4a4,0,16,0,3,0,0},{0x4a8,0x2ec,17,0,4,1,0x5e4a10},
        {0x47c,0x2f0,13,3,2,1,0x5eb4c8},{0x480,0x300,12,12,2,1,0x5ef9b8},
        {0x484,0x2f4,11,7,2,1,0x5e5a10},{0x488,0x2f8,10,8,2,1,0x5e17f8},
        {0x48c,0x310,19,20,2,1,0x5ef488},{0x474,0x30c,18,19,2,1,0x5e2568},
        {0x490,0x314,20,21,2,1,0x5e2188},{0x494,0x318,20,22,2,1,0x5e73d0},
        {0x498,0x2fc,9,13,2,1,0x5e48c0},{0x1fc,0x2dc,8,9,2,1,0x5ee608},
        {0x4ac,0x308,14,17,2,1,0x5ebe48},{0x4b0,0x308,14,9,2,1,0x5eae68},
        {0x49c,0x304,13,15,2,1,0x5ef4f8},{0x4a0,0x2d0,7,2,2,0,0x5e9fb8}};
    unsigned i;uint32_t selected;
    for(i=0;i<sizeof(parts)/sizeof(parts[0]);++i)if(get(a,player+parts[i].slot))return 0;
    if(get(a,player+0x1dc) || a->memory.fault)return 0;
    if(!faction_prepare(a,player))goto fail;
    game_memory_write_u32(&a->memory,player+0x444,0);game_memory_write_u32(&a->memory,player+0x44c,UINT32_MAX);
    for(i=0;i<10;++i){game_memory_write_u32(&a->memory,player+0x3cc+i*4,0);game_memory_write_u32(&a->memory,player+0x3f4+i*4,0);game_memory_write_u32(&a->memory,player+0x41c+i*4,0);}
    game_memory_write_u32(&a->memory,player+0x57c,UINT32_MAX);
    for(i=0;i<sizeof(clear)/sizeof(clear[0]);++i)game_memory_write_u32(&a->memory,player+clear[i],0);
    if(!command_defaults(a,player))goto fail;
    for(i=0;i<9;++i)if(get(a,player+0x5ac+i*8) && !game_level_restore_player_command(a,i))goto fail;
    if(!player_sample(a,player))goto fail;
    game_memory_write_u32(&a->memory,player+0x594,UINT32_MAX);
    if(!game_level_player_catalog_value(a,player,0x20,3,0,&selected) || !fill(a,0x20,(uint16_t)selected,1))goto fail;
    if(!game_level_player_catalog_mount(a,player,2,0x20,5,&selected) || !fill(a,2,(uint16_t)selected,5) ||
       !game_level_player_catalog_mount(a,player,2,0x23,5,&selected) || !fill(a,2,(uint16_t)selected,5) ||
       !game_level_player_catalog_mount(a,player,6,0x20,5,&selected) || !fill(a,6,(uint16_t)selected,5))goto fail;
    for(i=0;i<sizeof(optional_templates)/sizeof(optional_templates[0]);++i){
        if(get(a,player+optional_templates[i].choice)&0x80000000u)continue;
        if(!game_level_player_catalog_mount(a,player,optional_templates[i].item,0x20,5,&selected)){
            fprintf(stderr,"native player optional catalog failed: player=%08x kind=%u choice=%u\n",
                (unsigned)player,optional_templates[i].item,get(a,player+optional_templates[i].choice));goto fail;
        }
        if(!fill(a,optional_templates[i].item,(uint16_t)selected,optional_templates[i].count)){
            fprintf(stderr,"native player optional factory failed: player=%08x kind=%u choice=%u variant=%u phase=%u\n",
                (unsigned)player,optional_templates[i].item,get(a,player+optional_templates[i].choice),selected,get(a,0x624f8c));goto fail;
        }
        if(optional_templates[i].item==17){
            if(!fill(a,18,(uint16_t)selected,5) || !game_level_player_catalog_mount(a,player,9,17,5,&selected) || !fill(a,9,(uint16_t)selected,5))goto fail;
        }
    }
    for(i=0;i<sizeof(parts)/sizeof(parts[0]);++i){
        const PlayerPartRecipe *part=parts+i;uint32_t choice=part->choice?get(a,player+part->choice):0;
        if(part->optional && (choice&0x80000000u))continue;
        if(!configure(a,player,part->slot,part->pool_kind,part->records+choice*112u,part->mode,part->type,0,0))goto fail;
        if(part->slot==0x4a8 && !game_level_restore_player_command(a,1))goto fail;
    }return !a->memory.fault;
fail:
    (void)game_level_player_resources_release(a,player);return 0;
}
int game_level_player_prepare(GameApplication *a,GameAddress player){
    uint32_t method=get(a,get(a,player)+0x1c);
    if(a->memory.fault)return 0;
    if(method==0x4769a0 || method==0x477df0)return game_level_player_base_prepare(a,player);
    if(method==0x47b0f0)return scene_prepare(a,player);
    if(method==0x478c10)return faction_prepare(a,player);
    if(method==0x479730){
        uint32_t choice=get(a,player+0x2dc),variant;
        /*479730 prewarms the optional imperial position weapon before the
         *shared478c10 faction resources. The same pool owns its1fc record. */
        if(!(choice&0x80000000u) &&
           (!game_level_player_catalog_mount(a,player,9,0x20,5,&variant) ||
            !fill(a,9,(uint16_t)variant,5) ||
            !configure(a,player,0x1fc,8,0x5ee608+choice*112u,2,9,0,0)))goto imperial_failed;
        if(faction_prepare(a,player))return !a->memory.fault;
imperial_failed:
        (void)game_level_player_resources_release(a,player);return 0;
    }
    return 0; /*Unknown classes remain a diagnostic failure, never CPU re-entry. */
}
