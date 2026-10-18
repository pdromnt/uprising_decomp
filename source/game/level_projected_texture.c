#include "level_world_render.h"
#include "model_assets.h"
#include <string.h>

/*5036f0 and the5052d0 consumer share this exclusive normal bitmap reference.
 *Cached records belong to the existing cache, whose retirement owns their bytes.
 *Descriptor globals are immutable setup views, with a fixed-width pixel owner
 *ID. They never contain a native pointer or require per-frame synchronization. */
int game_level_projected_texture_release(GameApplication *a){
    uint32_t id=game_memory_read_u32(&a->memory,0x597068);unsigned i;
    if(a->memory.fault)return 0;
    if(id && game_model_texture(a,id) && !game_model_texture_release(a,id))return 0;
    game_memory_write_u32(&a->memory,0x597068,0);
    for(i=0;i<5;++i)game_memory_write_u32(&a->memory,0x624ee8+i*4,0);return !a->memory.fault;
}
int game_level_graphics_initialize(GameApplication *a){
    MaterialValue material={0};const TextureRecord *record;char path[512];uint8_t format[4];
    uint32_t old,pixels;unsigned i;
    old=game_memory_read_u32(&a->memory,0x597068);
    for(i=0;i<sizeof(path);++i){path[i]=(char)game_memory_read_u8(&a->memory,0x59706c+i);if(!path[i])break;}
    if(i==sizeof(path) || a->memory.fault || !game_model_material_load(a,path,-1,0,1,&material))return 0;
    record=game_model_texture(a,material.backend_handle);
    if(record){format[0]=format[1]=record->image.lod;format[2]=record->image.aspect;format[3]=record->image.format;pixels=record->id;}
    else{
        if(!game_memory_read(&a->memory,material.backend_handle+16,format,4))return 0;
        pixels=game_memory_read_u32(&a->memory,material.backend_handle+20);
    }
    if(old && game_model_texture(a,old) && !game_model_texture_release(a,old)){
        if(record)(void)game_model_texture_release(a,material.backend_handle);return 0;
    }
    game_memory_write_u32(&a->memory,0x597068,material.backend_handle);
    for(i=0;i<4;++i)game_memory_write_u32(&a->memory,0x624ee8+i*4,format[i]);
    game_memory_write_u32(&a->memory,0x624ef8,pixels);return !a->memory.fault;
}
