#include "mesh_buffer.h"
#include "gameplay_state.h"
#include "scene_projection_storage.h"
#include <string.h>

static GameAddress owned_allocate(void *context,uint32_t bytes){
    (void)context;return scene_projection_storage_allocate(bytes?bytes:1u,"mutable mesh buffer");
}
static int owned_release(void *context,GameAddress address){
    (void)context;return !address || scene_projection_storage_free(address);
}
const MeshBufferServices *mesh_buffer_owned_services(void){
    static const MeshBufferServices services={NULL,owned_allocate,owned_release};return &services;
}
int mesh_buffer_is_native(GameMemory *m,GameAddress object){
    uint32_t base,size;const char *purpose;GameAddress container=game_memory_read_u32(m,object+0x50u);
    return !m->fault && scene_projection_storage_describe(container,&base,&size,&purpose) &&
        base==container && size==0x4cu && !strcmp(purpose,"mutable mesh buffer");
}

static GameAddress copy_name(GameMemory *m,GameAddress source,const MeshBufferServices *s){
    uint32_t size=0,i;GameAddress result;
    do {if(size==4096u)return 0;} while(game_memory_read_u8(m,source+size++) && !m->fault);
    if(m->fault || !(result=s->allocate(s->context,size)))return 0;
    for(i=0;i<size && !m->fault;i++)game_memory_write_u8(m,result+i,game_memory_read_u8(m,source+i));
    if(m->fault){s->release(s->context,result);return 0;}return result;
}
int mesh_buffer_construct(GameMemory *m,GameAddress object,int16_t vertices,int16_t triangles,
        GameAddress name,GameAddress buffer_name,const MeshBufferServices *s){
    static const uint32_t offsets[5]={0x18u,4u,0x10u,8u,0x0cu};
    uint32_t sizes[5],i;GameAddress container,inner_name,allocated[5]={0};
    if(!s || !s->allocate || !s->release || vertices<0 || triangles<0)return 0;
    (void)name; /* Original 44fb90 ignores its configuration argument. */
    container=s->allocate(s->context,0x4cu);
    if(!container)return 0;
    inner_name=copy_name(m,buffer_name,s);
    if(!inner_name){s->release(s->context,container);return 0;}
    /* Shared 413630 / 44fb90 base: transform, name and resource links. */
    for(i=4u;i<=0x38u;i+=4u)game_memory_write_u32(m,object+i,
        (i==4u || i==0x18u || i==0x2cu)?0x3f800000u:0u);
    game_memory_write_u8(m,object+0x3cu,0u);
    for(i=0x40u;i<=0x4cu;i+=4u)game_memory_write_u32(m,object+i,0u);
    game_memory_write_u8(m,object+0x54u,0u);
    game_memory_write_u32(m,object+0x58u,0u);
    game_memory_write_u32(m,object,0x576e70u);
    game_memory_write_u32(m,object+0x50u,container);
    game_memory_write_u16(m,object+0x5cu,(uint16_t)vertices);
    game_memory_write_u16(m,object+0x5eu,(uint16_t)triangles);
    for(i=0;i<0x4cu;i+=4u)game_memory_write_u32(m,container+i,0u);
    game_memory_write_u32(m,container+0x30u,inner_name);
    game_memory_write_u32(m,container+0x48u,1u);
    sizes[0]=(uint32_t)triangles*40u;sizes[1]=(uint32_t)vertices*16u;
    sizes[2]=(uint32_t)triangles*12u;sizes[3]=sizes[4]=(uint32_t)triangles*24u;
    for(i=0;i<5u && !m->fault;i++){
        GameAddress block=s->allocate(s->context,sizes[i]);
        if(!block && sizes[i]){mesh_buffer_release(m,object,s);return 0;}
        allocated[i]=block;
        game_memory_write_u32(m,container+offsets[i],block);
    }
    gameplay_state_initialize_surface_rows(m,container,triangles);
    game_memory_write_u32(m,object+0x60u,0u);
    if(m->fault){
        for(i=0;i<5u;i++)if(allocated[i])s->release(s->context,allocated[i]);
        s->release(s->context,inner_name);s->release(s->context,container);return 0;
    }
    return 1;
}
int mesh_buffer_bind(GameMemory *m,GameAddress object,GameAddress source){
    GameAddress target=game_memory_read_u32(m,object+0x50u);
    GameAddress records=game_memory_read_u32(m,target+0x18u);
    GameAddress links=game_memory_read_u32(m,target+0x10u);
    GameAddress uv=game_memory_read_u32(m,target+8u);
    uint32_t i;int32_t count=(int32_t)game_memory_read_u32(m,source+0x1cu);
    if(m->fault || count<0 || count>(int16_t)game_memory_read_u16(m,object+0x5eu))return 0;
    game_memory_write_u32(m,target,game_memory_read_u32(m,source));
    game_memory_write_u32(m,target+0x48u,0u);
    for(i=0x14u;i<=0x2cu;i+=4u)if(i!=0x18u)
        game_memory_write_u32(m,target+i,game_memory_read_u32(m,source+i));
    for(i=0;i<(uint32_t)count && !m->fault;i++){
        GameAddress record=records+i*40u,original=game_memory_read_u32(m,source+0x18u)+i*40u;
        game_memory_write_u32(m,record,3u);
        game_memory_write_u32(m,record+4u,links+i*12u);
        game_memory_write_u32(m,record+16u,game_memory_read_u32(m,target+0x0cu)+i*24u);
        game_memory_write_u32(m,record+20u,game_memory_read_u32(m,original+20u));
        game_memory_write_u32(m,record+8u,game_memory_read_u32(m,original+8u));
    }
    for(i=0;i<(uint32_t)count*3u && !m->fault;i++){
        GameAddress original=game_memory_read_u32(m,source+8u)+i*8u;
        uint32_t index=game_memory_read_u32(m,game_memory_read_u32(m,source+0x10u)+i*4u);
        index=(index-game_memory_read_u32(m,source+4u))&~15u;
        game_memory_write_u32(m,uv+i*8u,game_memory_read_u32(m,original));
        game_memory_write_u32(m,uv+i*8u+4u,game_memory_read_u32(m,original+4u));
        game_memory_write_u32(m,links+i*4u,game_memory_read_u32(m,target+4u)+index);
    }
    game_memory_write_u32(m,target+0x34u,game_memory_read_u32(m,source+0x34u));
    game_memory_write_u32(m,object+0x60u,1u);return !m->fault;
}
/*540c90: clear the borrowed source binding, preserving owned mesh capacity. */
int mesh_buffer_clear(GameMemory *m,GameAddress object){
    GameAddress container=game_memory_read_u32(m,object+0x50);
    game_memory_write_u32(m,object+0x60,0);game_memory_write_u32(m,container+0x1c,0);
    game_memory_write_u32(m,container+0x14,0);game_memory_write_u32(m,container,0);
    game_memory_write_u32(m,container+0x34,0);return !m->fault;
}
int mesh_buffer_release(GameMemory *m,GameAddress object,const MeshBufferServices *s){
    static const uint32_t offsets[]={8u,0x0cu,0x10u,0x18u,4u,0x30u,0x40u,0x44u};
    GameAddress container=game_memory_read_u32(m,object+0x50u);unsigned i;
    /* Foreign child graphs remain an explicit adapter boundary. */
    if(m->fault || game_memory_read_u32(m,object+0x44u) || game_memory_read_u32(m,object+0x48u))return 0;
    if(container){
        for(i=0;i<sizeof(offsets)/sizeof(offsets[0]) && !m->fault;i++){
            GameAddress block=game_memory_read_u32(m,container+offsets[i]);
            if(block && !s->release(s->context,block))return 0;
            game_memory_write_u32(m,container+offsets[i],0u);
        }
        if(!s->release(s->context,container))return 0;
        game_memory_write_u32(m,object+0x50u,0u);
    }
    {GameAddress name=game_memory_read_u32(m,object+0x58u);
        if(name && !s->release(s->context,name))return 0;
        game_memory_write_u32(m,object+0x58u,0u);}
    return !m->fault;
}
GameAddress mesh_buffer_clone(GameMemory *m,GameAddress source,const MeshBufferServices *s){
    int16_t vertices=(int16_t)game_memory_read_u16(m,source+0x5cu);
    int16_t triangles=(int16_t)game_memory_read_u16(m,source+0x5eu);
    uint8_t flags=game_memory_read_u8(m,source+0x3cu);GameAddress object;
    if(m->fault || !(object=s->allocate(s->context,0x64u)))return 0;
    if(!mesh_buffer_construct(m,object,vertices,triangles,0x59d81cu,0x59d824u,s)){
        s->release(s->context,object);return 0;
    }
    game_memory_write_u8(m,object+0x3cu,flags);return m->fault?0:object;
}
