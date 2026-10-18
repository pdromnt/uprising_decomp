#include "campaign_world.h"
#include "portable_crt.h"
#include "scene_projection_storage.h"
#include "game_matrix.h"
#include <math.h>
#include <string.h>
extern int game_level_geometry_prepare(GameApplication *);
extern int game_level_geometry_finalize(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
static uint32_t random_next(GameApplication *a){return portable_crt_rand_step(&a->memory);}
static int text(GameApplication *a,GameAddress p,char *out,size_t size) {
    size_t i;for(i=0;i<size;++i){out[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!out[i])return !a->memory.fault;}return 0;
}
static GameAddress owned_text(GameApplication *a,const char *value) {
    size_t size=strlen(value)+1;GameAddress p=scene_projection_storage_allocate(size,"campaign name");
    if(p && !game_memory_write(&a->memory,p,value,size)){scene_projection_storage_free(p);return 0;}return p;
}
GameAddress game_campaign_system(GameApplication *a,uint32_t index) {
    GameAddress root=get(a,0x585408);if(!root || index>=get(a,root+8))return 0;return get(a,get(a,root+4)+index*4);
}
GameAddress game_campaign_mission(GameApplication *a,uint32_t system,uint32_t mission) {
    GameAddress p=game_campaign_system(a,system);if(!p || mission>=get(a,p+4))return 0;return get(a,get(a,p)+mission*4);
}
/*45cb80's complete ownership graph, reused by fresh/load failure and exit.
 * Systems own names and planet arrays; planets own their models/descriptions. */
int game_campaign_destroy(GameApplication *a) {
    GameAddress root=get(a,0x585408),systems;uint32_t count,i,j;if(!root)return !a->memory.fault;
    systems=get(a,root+4);count=get(a,root+8);if(count>65536)return 0;
    for(i=0;i<count;++i) {
        GameAddress system=get(a,systems+i*4),planets,name;uint32_t size;
        if(!system)continue;planets=get(a,system);size=get(a,system+4);if(size>65536)return 0;
        for(j=0;j<size;++j){GameAddress slot=planets+j*4,p=get(a,slot);if(p && !game_campaign_planet_destroy(a,p))return 0;put(a,slot,0);}
        if(planets && !scene_projection_storage_free(planets))return 0;name=get(a,system+8);
        if(name && !scene_projection_storage_free(name))return 0;
        if(!scene_projection_storage_free(system))return 0;put(a,systems+i*4,0);
    }
    if(systems && !scene_projection_storage_free(systems))return 0;
    {GameAddress name=get(a,root);if(name && !scene_projection_storage_free(name))return 0;}
    if(!scene_projection_storage_free(root))return 0;put(a,0x585408,0);return !a->memory.fault;
}
static GameAddress mission_template(GameApplication *a,uint32_t index) {
    uint32_t category=0,previous=0,offset,count;
    for(;category<7;++category){count=get(a,0x5a04c0+category*4);if(index<previous+count)break;previous+=count;}
    if(category==7)return 0;offset=category*50u+index-previous;
    return get(a,get(a,0x5a0530)+get(a,0x639b08+offset*4)*4);
}
/*45d4c0: seven actual systems, their central models, orbital placement and
 * mission template assignment. The existing parsed name/material/mission
 * tables are borrowed. RNG draws preserve the original generation order. */
int game_campaign_new(GameApplication *a) {
    static const uint16_t five[][4]={{1119,1876,1677,200},{2000,320,320,2000}};
    static const uint16_t four[][3]={{1876,1677,204},{320,320,2000}};
    static const uint16_t three[][2]={{1677,204},{320,2000}};
    GameAddress root=0,systems,system,planets,p;float origin[3]={0,0,0};char name[128];
    uint32_t names=get(a,0x5a0504),index,mission_index=0,j,count;long double span;uint32_t radius_range,speed_range;
    if(get(a,0x585408) || !names || names>65536 || !game_level_geometry_prepare(a))return 0;
    p=get(a,get(a,0x5a04fc)+(random_next(a)%names)*4);if(!text(a,p,name,sizeof(name)))goto fail;
    root=scene_projection_storage_allocate(0x28,"campaign world");if(!root)goto fail;
    game_memory_fill_u8(&a->memory,root,0x28,0,0);put(a,0x585408,root);put(a,root+0x14,UINT32_MAX);put(a,root+0x18,UINT32_MAX);
    p=owned_text(a,name);if(!p)goto fail;put(a,root,p);
    systems=scene_projection_storage_allocate(7*4,"campaign systems");if(!systems)goto fail;put(a,root+4,systems);put(a,root+8,7);
    game_memory_fill_u8(&a->memory,systems,7*4,0,0);put(a,0x5e1448,1200);
    span=((long double)real(a,0x573778)-real(a,0x57377c))*real(a,0x5737c0);
    if(!isfinite(span) || span<1 || span>INT32_MAX)goto fail;radius_range=(uint32_t)(int32_t)span;
    span=((long double)real(a,0x573780)-real(a,0x573784))*real(a,0x5737c4);
    if(!isfinite(span) || span<1 || span>INT32_MAX)goto fail;speed_range=(uint32_t)(int32_t)span;
    for(index=0;index<7;++index) {
        uint16_t angles[4]={0};count=get(a,0x5a04e0+index*4)+1;if(count<2 || count>5)goto fail;
        system=scene_projection_storage_allocate(20,"campaign system");if(!system)goto fail;
        game_memory_fill_u8(&a->memory,system,20,0,0);put(a,systems+index*4,system);
        p=get(a,get(a,0x5a0508)+get(a,get(a,0x5a050c)+index*4)*4);
        if(!text(a,p,name,sizeof(name)) || !(p=owned_text(a,name)))goto fail;put(a,system+8,p);
        planets=scene_projection_storage_allocate(count*4,"campaign planets");if(!planets)goto fail;
        game_memory_fill_u8(&a->memory,planets,count*4,0,0);put(a,system,planets);put(a,system+4,count);put(a,system+0xc,get(a,0x63a120+(index%10)*4));
        p=game_campaign_planet_create(a,name,origin,0,0,60000,0,1,-3000);if(!p)goto fail;put(a,planets,p);
        (void)random_next(a);
        if(count==5)memcpy(angles,five[random_next(a)%2],sizeof(angles));
        else if(count==4)memcpy(angles,four[random_next(a)%2],3*sizeof(*angles));
        else if(count==3)memcpy(angles,three[random_next(a)%2],2*sizeof(*angles));
        else angles[0]=(uint16_t)((random_next(a)%348+2000)&2047);
        if(index==6)angles[0]=47;
        for(j=1;j<count;++j) {
            float radius,speed,sign,depth;GameAddress template;
            radius=(float)((long double)j*real(a,0x5737c8)+(long double)(random_next(a)%radius_range)*real(a,0x5737c8)+real(a,0x57377c));
            sign=real(a,random_next(a)%2?0x573798:0x5737cc);
            speed=(float)(((long double)(random_next(a)%speed_range)*real(a,0x5737d0)+real(a,0x573784))*sign);
            depth=(float)((long double)(count-j-1)*real(a,0x5737d4)+real(a,0x5737d8));
            /*45d8cf/45d8f3 pass EBX, the global mission ordinal, to the two
             * shuffled catalogs. A system index made every orbiting planet
             * share one writable texture, including its animated flag pixels. */
            if(mission_index>=get(a,0x5a051c) || mission_index>=get(a,0x5a052c))goto fail;
            p=get(a,get(a,0x5a0514)+get(a,get(a,0x5a0518)+mission_index*4)*4);if(!text(a,p,name,sizeof(name)))goto fail;
            p=game_campaign_planet_create(a,name,origin,depth,angles[j-1],radius,get(a,get(a,0x5a0528)+mission_index*4),1,speed);if(!p)goto fail;
            put(a,planets+j*4,p);put(a,get(a,planets+(j-1)*4)+4,p);template=mission_template(a,mission_index++);
            if(!template || !game_campaign_planet_bind(a,p,template))goto fail;
        }
        put(a,get(a,planets)+8,get(a,planets+4));
    }
    if(!game_level_geometry_finalize(a))goto fail;put(a,root+0x1c,0);put(a,root+0x20,1);put(a,game_campaign_system(a,0)+0x10,1);
    return !a->memory.fault;
fail:(void)game_campaign_destroy(a);(void)game_level_geometry_finalize(a);return 0;
}
/*45cca0: first completion awards the planet's existing reward. More than one
 * completed mission unlocks the next systems according to the original groups. */
int game_campaign_complete(GameApplication *a,uint32_t index,uint32_t mission) {
    GameAddress root=get(a,0x585408),system=game_campaign_system(a,index),planet=game_campaign_mission(a,index,mission);uint32_t done=0,j,count;
    if(!planet || !system)return 0;if(get(a,planet+0x2c))return !a->memory.fault;
    put(a,planet+0x2c,1);put(a,0x5e1448,get(a,0x5e1448)+get(a,planet+0x150));count=get(a,system+4);
    for(j=0;j<count;++j)if(get(a,get(a,get(a,system)+j*4)+0x2c))++done;
    if(done>1) {
        unsigned ahead=index%3<2?2:1;for(j=1;j<=ahead && index+j<get(a,root+8);++j)put(a,game_campaign_system(a,index+j)+0x10,1);
    }
    return !a->memory.fault;
}
static uint32_t truncate_low(long double v) {
    if(!isfinite(v) || v< -0x1p63L || v>=0x1p63L)return 0;
    return (uint32_t)(uint64_t)(int64_t)v;
}
/*45cdd0: advance the selected system's actual planet matrices. Keep the
 * original large-angle division (not a modern fmod), signed16 quantization,
 * stored fractional remainder and rotation about each model's own center. */
int game_campaign_world_advance(GameApplication *a,uint32_t elapsed) {
    GameAddress root=get(a,0x585408),system=game_campaign_system(a,get(a,root+0x1c));uint32_t count=get(a,system+4),j,i;
    if(!system || count>65536)return 0;
    for(j=1;j<count;++j){
        GameAddress planet=game_campaign_mission(a,get(a,root+0x1c),j),model;float matrix[12],next[12],shift[12]={1,0,0,0,0,1,0,0,0,0,1,0},rotation[12]={0},position[3],delta,remainder;
        uint32_t bits,integer,magnitude;int32_t quantized;uint16_t raw,angle;
        if(!planet)continue;
        delta=(float)((long double)(elapsed<0x80000000u?(int64_t)elapsed:(int64_t)elapsed-INT64_C(4294967296))/real(a,planet+0x10)*real(a,0x5737b0)+real(a,planet+0x14));
        integer=truncate_low(delta);magnitude=(integer&0x80000000u)?0u-integer:integer;
        if((int32_t)magnitude>=360)delta=(float)((long double)delta/(fabsl((long double)(double)delta)*real(a,0x5737b0)));
        raw=(uint16_t)truncate_low((long double)delta*real(a,0x57379c));quantized=raw<0x8000u?(int32_t)raw:(int32_t)raw-65536;angle=raw&2047u;
        remainder=(float)((long double)delta-(long double)quantized*real(a,0x5737b4));memcpy(&bits,&remainder,4);put(a,planet+0x14,bits);
        model=get(a,planet);if(!model)return 0;
        for(i=0;i<12;++i)matrix[i]=real(a,model+4+i*4);
        for(i=0;i<3;++i){position[i]=matrix[i*4+3];shift[i*4+3]=-position[i];memcpy(&bits,&position[i],4);put(a,0x5e1470+i*4,bits);}
        game_matrix_compose_values(next,shift,matrix);memcpy(matrix,next,sizeof(matrix));
        rotation[0]=rotation[10]=real(a,0x6298a8+((angle+512u)&2047u)*4);rotation[2]=real(a,0x6298a8+angle*4);rotation[8]=-rotation[2];rotation[5]=1;
        game_matrix_compose_values(next,rotation,matrix);memcpy(matrix,next,sizeof(matrix));
        for(i=0;i<3;++i)shift[i*4+3]=position[i];game_matrix_compose_values(next,shift,matrix);
        for(i=0;i<12;++i){memcpy(&bits,&next[i],4);put(a,model+4+i*4,bits);}
    }
    return !a->memory.fault;
}
