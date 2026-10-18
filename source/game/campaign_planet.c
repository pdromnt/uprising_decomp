#include "campaign_planet.h"
#include "model_assets.h"
#include "scene_instance_owner.h"
#include "game_matrix.h"
#include "portable_crt.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/*The SDF font-model branch is a real format dependency. Existing BDM assets
 * use the native loader; no CPU call or substitute model hides missing SDF. */
extern GameAddress game_campaign_text_model_load(GameApplication *,const char *);
extern int game_menu_locale_uppercase(GameApplication *,char *,size_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
static void write_real(GameApplication *a,GameAddress p,float v){uint32_t bits;memcpy(&bits,&v,4);put(a,p,bits);}
/*45b040: preserve zero UV components and original width/bias comparisons. */
int game_campaign_planet_materials_prepare(GameApplication *a,GameAddress holder) {
    GameAddress descriptor=get(a,holder+0x50),polygons=get(a,descriptor+0x18);uint32_t count=get(a,descriptor+0x1c),i;
    if(count>65536)return 0;
    for(i=0;i<count;++i) {
        GameAddress polygon=polygons+i*40,uv=get(a,polygon+0x10),material=get(a,polygon+0x14);unsigned o;
        float width=(float)(int16_t)game_memory_read_u16(&a->memory,material),limit;long double threshold;
        if((get(a,uv)&0x7fffffff) && (get(a,uv+8)&0x7fffffff) && (get(a,uv+16)&0x7fffffff))continue;
        threshold=(long double)width*real(a,0x573790);limit=(float)threshold;
        if(threshold>=real(a,uv) && !(real(a,uv+8)>limit) && !(real(a,uv+16)>limit))continue;
        for(o=0;o<24;o+=8)if(!(get(a,uv+o)&0x7fffffff))write_real(a,uv+o,width);
    }
    return !a->memory.fault;
}
static int polygon_flags(GameApplication *a,GameAddress holder,uint32_t flags,GameAddress material) {
    GameAddress descriptor=get(a,holder+0x50),polygons=get(a,descriptor+0x18);uint32_t count=get(a,descriptor+0x1c),i;
    if(count>65536)return 0;
    for(i=0;i<count;++i){GameAddress p=polygons+i*40;if(flags)put(a,p+8,get(a,p+8)|flags);if(material)put(a,p+0x14,material);}
    return !a->memory.fault;
}
static int bases_release(GameApplication *a) {
    unsigned i;for(i=0;i<3;++i){GameAddress slot=0x5853f8+i*4,p=get(a,slot);
        if(p && !game_application_scene_model_release(a,p))return 0;put(a,slot,0);}
    for(i=0;i<8;++i){GameAddress slot=0x5e1450+i*4,p=get(a,slot);
        if(p){if(!game_model_texture_release(a,get(a,p+4)))return 0;put(a,p+4,0);}
        if(p && !scene_projection_storage_free(p))return 0;put(a,slot,0);}
    return !a->memory.fault;
}
static int bases_prepare(GameApplication *a) {
    static const char *models[]={"models\\himodA.bdm","models\\himodB.bdm","models\\alien.bdm"};
    static const char *fonts[]={"fonts\\himodA.sdf","fonts\\himodB.sdf","fonts\\alien.sdf"};
    unsigned i;char path[80];
    if(get(a,0x5853f8))return !a->memory.fault;
    for(i=0;i<3;++i) {
        ResourceBlob probe={0};GameAddress p;int exists=game_application_load_blob(a,models[i],&probe);resource_blob_release(&probe);
        p=exists?game_model_file_load(a,models[i],0):game_campaign_text_model_load(a,fonts[i]);
        if(!p)goto fail;put(a,0x5853f8+i*4,p);put(a,p+0x58,1);
        if(!game_campaign_planet_materials_prepare(a,p) || (i<2 && !polygon_flags(a,p,256,0)))goto fail;
    }
    for(i=0;i<8;++i) {
        MaterialValue material;GameAddress p;
        snprintf(path,sizeof(path),"bitmaps\\planettex\\impflag%u.bmp",i);
        if(!game_model_software_material_load(a,path,&material))goto fail;
        p=scene_projection_storage_allocate(8,"campaign flag material");if(!p)goto fail;
        put(a,0x5e1450+i*4,p);game_memory_write_u16(&a->memory,p,material.width);game_memory_write_u16(&a->memory,p+2,material.height);put(a,p+4,material.backend_handle);
    }
    return !a->memory.fault;
fail:(void)bases_release(a);return 0;
}
/*45b1b0: one owned legacy planet record and three native model clones. Shared
 * source meshes survive until the last planet retires; matrices remain in the
 * existing model owner and are consumed directly by its native graph. */
GameAddress game_campaign_planet_create(GameApplication *a,const char *name,const float position[3],float depth,uint16_t angle,float radius,uint32_t kind,float scale,float speed) {
    static const unsigned models[]={0x578,0x57c,0};GameAddress p=0,clone=0,material;unsigned i;
    float matrix[12],composed[12],rotation[12]={0},translation[12]={1,0,0,0,0,1,0,0,0,0,1,0};
    if(!position || kind>=4096 || (name && strlen(name)>=128) || !bases_prepare(a))return 0;
    p=scene_projection_storage_allocate(0x5b0,"campaign planet");
    if(!p){if(!get(a,0x585404))(void)bases_release(a);return 0;}
    game_memory_fill_u8(&a->memory,p,0x5b0,0,0);put(a,0x585404,get(a,0x585404)+1);
    write_real(a,p+0xc,radius);write_real(a,p+0x10,speed);put(a,p+0x134,5);
    put(a,p+0x584,1);put(a,p+0x588,10);put(a,p+0x58c,20);
    if(name && !game_memory_write(&a->memory,p+0x30,name,strlen(name)+1))goto fail;
    for(i=0;i<3;++i)write_real(a,p+0x138+i*4,position[i]);write_real(a,p+0x144,depth);
    game_memory_write_u16(&a->memory,p+0x148,angle);game_memory_write_u16(&a->memory,p+0x1c,angle);
    write_real(a,p+0x130,scale);put(a,p+0x564,kind);
    material=get(a,get(a,0x5a0520)+kind*4);
    for(i=0;i<3;++i) {
        if(scene_instance_owner_clone(&a->memory,get(a,0x5853f8+i*4),&clone)!=1)goto fail;
        put(a,p+models[i],clone);put(a,clone+0x58,1);
        if(!polygon_flags(a,clone,i<2?256:0,i==2?material:0))goto fail;
    }
    clone=get(a,p);for(i=0;i<12;++i)matrix[i]=(float)((long double)real(a,clone+4+i*4)*scale);
    matrix[11]=(float)((long double)matrix[11]-depth);
    rotation[0]=rotation[10]=real(a,0x6298a8+((angle+512u)&2047u)*4);rotation[2]=real(a,0x6298a8+(angle&2047u)*4);
    rotation[8]=-rotation[2];rotation[5]=1;
    game_matrix_compose_values(composed,rotation,matrix);memcpy(matrix,composed,sizeof(matrix));
    translation[3]=position[0];translation[7]=position[1];translation[11]=position[2];
    game_matrix_compose_values(composed,translation,matrix);memcpy(matrix,composed,sizeof(matrix));
    for(i=0;i<12;++i)write_real(a,clone+4+i*4,matrix[i]);
    if(a->memory.fault)goto fail;return p;
fail:(void)game_campaign_planet_destroy(a,p);return 0;
}
/*45b850: retire clones and owned auxiliary bytes before the shared bases. */
int game_campaign_planet_destroy(GameApplication *a,GameAddress p) {
    static const unsigned slots[]={0x578,0x57c,0};unsigned i;uint32_t count;GameAddress auxiliary;
    if(!p)return !a->memory.fault;
    for(i=0;i<3;++i){GameAddress model=get(a,p+slots[i]);if(model && !game_application_scene_model_release(a,model))return 0;put(a,p+slots[i],0);}
    auxiliary=get(a,p+0x14c);if(auxiliary && !scene_projection_storage_free(auxiliary))return 0;put(a,p+0x14c,0);
    count=get(a,0x585404);if(!count)return 0;put(a,0x585404,count-1);
    if(count==1 && !bases_release(a))return 0;
    return !a->memory.fault && scene_projection_storage_free(p);
}
static int string_read(GameApplication *a,GameAddress p,char *out,size_t capacity) {
    size_t i;for(i=0;i<capacity;++i){out[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!out[i])return !a->memory.fault;}return 0;
}
/*45bca0: bind actual mission metadata, preserve the shared RNG consumption
 * order, publish reward and optional bonus through the same planet record. */
int game_campaign_planet_bind(GameApplication *a,GameAddress planet,GameAddress mission) {
    char level[128],title[128],bonus[1024];GameAddress description=0,old,p;uint32_t difficulty,count,length=0,base,delta;long double range;
    if(!planet || !mission || !string_read(a,mission+0x9d0,level,sizeof(level)))return 0;
    memcpy(title,level,strlen(level)+1);{char *suffix=strstr(title,".lfl");if(suffix)*suffix=0;}
    if(!game_menu_locale_uppercase(a,title,sizeof(title)))return 0;
    while(length<65536 && game_memory_read_u8(&a->memory,mission+length))++length;
    if(length==65536 || a->memory.fault)return 0;
    description=scene_projection_storage_allocate(length+1,"campaign mission description");if(!description)return 0;
    {
        uint32_t i;for(i=0;i<=length;++i)game_memory_write_u8(&a->memory,description+i,game_memory_read_u8(&a->memory,mission+i));
    }
    difficulty=get(a,mission+0xa50);if(difficulty>=7)goto fail;
    base=get(a,0x585410+difficulty*4);range=(long double)(int32_t)base*real(a,0x5737a0);
    if(!isfinite(range) || range<0 || range>INT32_MAX)goto fail;
    count=(uint32_t)(int32_t)range;delta=count?portable_crt_rand_step(&a->memory)%count:0;
    if(portable_crt_rand_step(&a->memory)%2)delta=0u-delta;
    put(a,planet+0x150,base+delta);count=get(a,mission+0xa60);
    if(count) {
        if(count>65536)goto fail;
        p=get(a,get(a,mission+0xa5c)+(portable_crt_rand_step(&a->memory)%count)*4);
        if(!string_read(a,p,bonus,sizeof(bonus)) || !game_memory_write(&a->memory,planet+0x154,bonus,strlen(bonus)+1))goto fail;
        delta=portable_crt_rand_step(&a->memory)%360;if(portable_crt_rand_step(&a->memory)%2)delta=0u-delta;
        put(a,planet+0x558,1);put(a,planet+0x554,0);put(a,planet+0x560,difficulty*1000u+delta);
    }else put(a,planet+0x558,0);
    put(a,planet+0x568,1);put(a,planet+0x56c,0);
    delta=portable_crt_rand_step(&a->memory)%70;if(portable_crt_rand_step(&a->memory)%2)delta=0u-delta;
    put(a,planet+0x570,difficulty*600u+delta);
    if(!game_memory_write(&a->memory,planet+0xb0,level,strlen(level)+1) || !game_memory_write(&a->memory,planet+0x30,title,strlen(title)+1))goto fail;
    old=get(a,planet+0x14c);if(old && !scene_projection_storage_free(old))goto fail;
    put(a,planet+0x14c,description);put(a,planet+0x24,1);return !a->memory.fault;
fail:scene_projection_storage_free(description);return 0;
}
