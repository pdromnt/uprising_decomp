#include "campaign_world.h"
#include "resource_file.h"
#include "scene_projection_storage.h"
#include "application_archives.h"
#include <string.h>
extern int game_level_geometry_prepare(GameApplication *);
extern int game_level_geometry_finalize(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int word(ResourceFile *f,uint32_t *v,unsigned bytes,int writing) {
    uint8_t b[4];unsigned i;
    if(writing){for(i=0;i<bytes;++i)b[i]=(uint8_t)(*v>>(i*8));return resource_file_write(f,b,bytes)==bytes;}
    if(resource_file_read(f,b,bytes)!=bytes)return 0;
    *v=0;for(i=0;i<bytes;++i)*v|=(uint32_t)b[i]<<(i*8);return 1;
}
static int fields(GameApplication *a,ResourceFile *f,GameAddress p,unsigned count,int writing) {
    unsigned i;for(i=0;i<count;++i){uint32_t v=writing?get(a,p+i*4):0;if(!word(f,&v,4,writing))return 0;if(!writing)put(a,p+i*4,v);}return !a->memory.fault;
}
static int bytes(GameApplication *a,ResourceFile *f,GameAddress p,unsigned size,int writing) {
    uint8_t b[256];unsigned offset=0;
    while(offset<size){unsigned n=size-offset;if(n>sizeof(b))n=sizeof(b);
        if(writing){if(!game_memory_read(&a->memory,p+offset,b,n) || resource_file_write(f,b,n)!=n)return 0;}
        else if(resource_file_read(f,b,n)!=n || !game_memory_write(&a->memory,p+offset,b,n))return 0;
        offset+=n;
    }return 1;
}
/* 4c9350/4c93d0: byte length followed by bytes, without a file terminator. */
static GameAddress string_read(GameApplication *a,ResourceFile *f,unsigned limit) {
    GameAddress p;uint32_t length=0;
    if(!word(f,&length,4,0) || length>limit)return 0;
    p=scene_projection_storage_allocate((size_t)length+1,"campaign saved string");if(!p)return 0;
    if(!bytes(a,f,p,length,0)){scene_projection_storage_free(p);return 0;}
    game_memory_write_u8(&a->memory,p+length,0);return p;
}
static int string_write(GameApplication *a,ResourceFile *f,GameAddress p,unsigned limit) {
    uint32_t length=0;if(p){while(length<=limit && game_memory_read_u8(&a->memory,p+length))++length;if(length>limit)return 0;}
    return !a->memory.fault && word(f,&length,4,1) && (!length || bytes(a,f,p,length,1));
}
static float number(uint32_t bits){float v;memcpy(&v,&bits,4);return v;}
static GameAddress planet_read(GameApplication *a,ResourceFile *f) {
    GameAddress name=0,level=0,description=0,p=0;char label[128];float origin[3]={0,0,0};
    uint32_t pose[3],depth=0,angle=0,radius=0,kind=0,scale=0,speed=0,mission=0;unsigned i;
    name=string_read(a,f,127);if(!name)goto fail;
    for(i=0;i<3;++i)if(!word(f,&pose[i],4,0))goto fail;
    if(!word(f,&depth,4,0) || !word(f,&angle,2,0) || !word(f,&radius,4,0) || !word(f,&kind,4,0) || !word(f,&scale,4,0) || !word(f,&speed,4,0))goto fail;
    for(i=0;i<sizeof(label);++i){label[i]=(char)game_memory_read_u8(&a->memory,name+i);if(!label[i])break;}
    if(i==sizeof(label))goto fail;
    /* 45d040 consumes saved position but deliberately constructs at origin. */
    p=game_campaign_planet_create(a,label,origin,number(depth),(uint16_t)angle,number(radius),kind,number(scale),number(speed));if(!p)goto fail;
    if(!word(f,&mission,4,0))goto fail;
    if(mission){
        level=string_read(a,f,127);description=string_read(a,f,65536);if(!level || !description)goto fail;
        for(i=0;i<128;++i){uint8_t c=game_memory_read_u8(&a->memory,level+i);game_memory_write_u8(&a->memory,p+0xb0+i,c);if(!c)break;}
        put(a,p+0x14c,description);description=0;put(a,p+0x24,1);
        if(!fields(a,f,p+0x2c,1,0) || !fields(a,f,p+0x150,1,0) ||
           !bytes(a,f,p+0x154,0x410,0) || !bytes(a,f,p+0x154,0x410,0) || !fields(a,f,p+0x568,3,0))goto fail;
    }
    scene_projection_storage_free(name);if(level)scene_projection_storage_free(level);return p;
fail:if(name)scene_projection_storage_free(name);if(level)scene_projection_storage_free(level);if(description)scene_projection_storage_free(description);
    if(p)(void)game_campaign_planet_destroy(a,p);return 0;
}
static int planet_write(GameApplication *a,ResourceFile *f,GameAddress p) {
    static const unsigned offsets[]={0x144,0x148,0xc,0x564,0x130,0x10,0x24};unsigned i;
    if(!string_write(a,f,p+0x30,127) || !fields(a,f,p+0x138,3,1))return 0;
    for(i=0;i<sizeof(offsets)/sizeof(*offsets);++i){uint32_t v=offsets[i]==0x148?game_memory_read_u16(&a->memory,p+0x148):get(a,p+offsets[i]);if(!word(f,&v,offsets[i]==0x148?2:4,1))return 0;}
    if(!get(a,p+0x24))return !a->memory.fault;
    return string_write(a,f,p+0xb0,127) && string_write(a,f,get(a,p+0x14c),65536) && fields(a,f,p+0x2c,1,1) && fields(a,f,p+0x150,1,1) &&
        bytes(a,f,p+0x154,0x410,1) && bytes(a,f,p+0x154,0x410,1) && fields(a,f,p+0x568,3,1);
}
/* 45da10: stream ownership stays native; published legacy records own every
 * allocation and cleanup uses the same world destructor as normal departure. */
int game_campaign_load(GameApplication *a,const char *path) {
    ResourceFile *f;GameAddress root=0,systems=0,p;uint32_t version=0,count=0,i,j;int ok=0;char resolved[4128];
    if(!a || a->memory.fault || get(a,0x585408) ||
       !game_application_file_path(a,path,0,resolved,sizeof(resolved)) || !(f=resource_file_open(resolved)))return 0;
    if(!game_level_geometry_prepare(a))goto done;
    if(!word(f,&version,4,0))goto fail;
    root=scene_projection_storage_allocate(0x28,"saved campaign world");if(!root)goto fail;
    game_memory_fill_u8(&a->memory,root,0x28,0,0);put(a,0x585408,root);
    p=string_read(a,f,127);if(!p)goto fail;put(a,root,p);
    if(!word(f,&count,4,0) || !count || count>65536)goto fail;
    systems=scene_projection_storage_allocate((size_t)count*4,"saved campaign systems");if(!systems)goto fail;
    game_memory_fill_u8(&a->memory,systems,(size_t)count*4,0,0);put(a,root+4,systems);put(a,root+8,count);
    if(!fields(a,f,root+0x24,1,0) || !fields(a,f,0x5e1448,1,0))goto fail;
    for(i=0;i<count;++i){GameAddress system,planets;uint32_t size=0;
        system=scene_projection_storage_allocate(20,"saved campaign system");if(!system)goto fail;
        game_memory_fill_u8(&a->memory,system,20,0,0);put(a,systems+i*4,system);
        p=string_read(a,f,127);if(!p)goto fail;put(a,system+8,p);
        if(!word(f,&size,4,0) || !size || size>65536)goto fail;
        planets=scene_projection_storage_allocate((size_t)size*4,"saved campaign planets");if(!planets)goto fail;
        game_memory_fill_u8(&a->memory,planets,(size_t)size*4,0,0);put(a,system,planets);put(a,system+4,size);
        for(j=0;j<size;++j){p=planet_read(a,f);if(!p)goto fail;put(a,planets+j*4,p);if(j)put(a,get(a,planets+(j-1)*4)+4,p);}
        if(size>1)put(a,get(a,planets)+8,get(a,planets+4));
        if(!fields(a,f,system+0x10,1,0) || !fields(a,f,system+0xc,1,0))goto fail;
    }
    if(!fields(a,f,root+0x14,2,0) || !fields(a,f,0x6027f0,0xf0,0))goto fail;
    put(a,root+0x1c,get(a,root+0x14));put(a,root+0x20,get(a,root+0x18));
    if(!game_campaign_mission(a,get(a,root+0x1c),get(a,root+0x20)))goto fail;
    ok=game_level_geometry_finalize(a) && !a->memory.fault;if(!ok)goto fail;goto done;
fail:(void)game_campaign_destroy(a);(void)game_level_geometry_finalize(a);
done:resource_file_close(f);return ok;
}
/* 45de40/4a7310: caller supplies the isolated or normal save path explicitly. */
int game_campaign_save(GameApplication *a,const char *path) {
    GameAddress root;ResourceFile *f;uint32_t version=0,count,i,j;int ok=0;char resolved[4128];
    if(!a || a->memory.fault)return 0;
    root=get(a,0x585408);
    if(!root || !(count=get(a,root+8)) || count>65536 ||
       !game_application_file_path(a,path,1,resolved,sizeof(resolved)) || !(f=resource_file_open_mode(resolved,"wb")))return 0;
    if(!word(f,&version,4,1) || !string_write(a,f,get(a,root),127) || !fields(a,f,root+8,1,1) || !fields(a,f,root+0x24,1,1) || !fields(a,f,0x5e1448,1,1))goto done;
    for(i=0;i<count;++i){GameAddress system=game_campaign_system(a,i);uint32_t size=get(a,system+4);
        if(!size || size>65536 || !string_write(a,f,get(a,system+8),127) || !fields(a,f,system+4,1,1))goto done;
        for(j=0;j<size;++j)if(!planet_write(a,f,game_campaign_mission(a,i,j)))goto done;
        if(!fields(a,f,system+0x10,1,1) || !fields(a,f,system+0xc,1,1))goto done;
    }
    put(a,root+0x14,get(a,root+0x1c));put(a,root+0x18,get(a,root+0x20));
    ok=fields(a,f,root+0x14,2,1) && fields(a,f,0x6027f0,0xf0,1) && resource_file_flush(f) && !a->memory.fault;
done:resource_file_close(f);return ok;
}
void game_campaign_factions_default(GameApplication *a) {
    static const unsigned sentinel[]={0x60,0x40,0x68,0x6c,0x64,0x70,0x58,0x5c,0x54,0x74,0x78,0x7c,0x50,0x38};
    unsigned faction,i;for(faction=0;faction<4;++faction){GameAddress base=0x6027f0+faction*0xf0;
        game_memory_fill_u8(&a->memory,base,0x80,0,0);
        for(i=0;i<sizeof(sentinel)/sizeof(*sentinel);++i)put(a,base+sentinel[i],UINT32_MAX);
        put(a,base+0x80,2000);put(a,base+0x84,5);for(i=0;i<4;++i)put(a,base+0x88+i*4,0);
        for(i=0;i<9;++i){put(a,base+0x9c+i*8,0xffffffbb);put(a,base+0x98+i*8,0xffffffbb);}
        for(i=0;i<4;++i)put(a,base+0xe0+i*4,0xffffffbb);
    }
}
