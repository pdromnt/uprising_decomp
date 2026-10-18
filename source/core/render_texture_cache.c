#include "render_texture_cache.h"
#include "scene_projection_storage.h"
#include <string.h>

static GameRenderTextureServices active;
static uint32_t get(GameMemory *m,GameAddress p){return game_memory_read_u32(m,p);}
static void put(GameMemory *m,GameAddress p,uint32_t v){game_memory_write_u32(m,p,v);}

int game_render_texture_cache_begin(GameMemory *m,const GameRenderTextureServices *s){
    GameAddress records,aligned;
    if(!s || !s->statistics || !s->upload || !s->release || !s->reset || get(m,0x63a168))return 0;
    /* Stream-cached materials retain their established bounded record owner.
     * No VRAM spans, address allocator, frame eviction ring or 2MiB budget. */
    records=scene_projection_storage_allocate(0x2001f,"render texture records");
    if(!records)return 0;
    aligned=(records+31u)&~31u;
    if(!game_memory_fill_u8(m,records,0x2001f,0,0)){
        scene_projection_storage_free(records);return 0;
    }
    active=*s;put(m,0x63a168,records);put(m,0x63a184,aligned);
    put(m,0x63a158,aligned);put(m,0x63a154,1);return !m->fault;
}
int game_render_texture_cache_end(GameMemory *m){
    GameAddress records=get(m,0x63a168),p=get(m,0x63a184),end=get(m,0x63a158);
    if(!records)return !m->fault;
    if(end<p || end-p>0x20000 || ((end-p)&31))return 0;
    for(;p<end;p+=32){
        GameAddress data=get(m,p+20),aux=get(m,p+24);
        if(!(get(m,p)&4u)){
            if(data && !scene_projection_storage_free(data))return 0;
            if(aux && !scene_projection_storage_free(aux))return 0;
        }
    }
    if(active.reset)active.reset(active.context);
    if(!scene_projection_storage_free(records))return 0;
    put(m,0x63a168,0);put(m,0x63a184,0);put(m,0x63a158,0);
    memset(&active,0,sizeof(active));return !m->fault;
}
GameAddress game_render_texture_create(GameMemory *m,uint32_t flags,const uint8_t format[4],const uint8_t *pixels,uint32_t bytes){
    GameAddress p=get(m,0x63a158),base=get(m,0x63a184),data;
    if(!get(m,0x63a168) || !format || !pixels || !bytes || bytes>0x200000 || p<base || p-base>=0x20000)return 0;
    data=scene_projection_storage_allocate(bytes,"render texture payload");if(!data)return 0;
    if(!game_memory_write(m,data,pixels,bytes) || !game_memory_write(m,p+16,format,4)){
        scene_projection_storage_free(data);return 0;
    }
    put(m,p,flags&0x7ffffffbu);put(m,p+4,0);put(m,p+8,p);
    put(m,p+12,bytes);put(m,p+20,data);put(m,0x63a158,p+32);
    return m->fault?0:p;
}
int game_render_texture_cache_frame(GameMemory *m,const GameRenderTextureServices *s){
    uint32_t statistics[5];unsigned i;
    if(!get(m,0x63a168) || !s || !s->statistics || !s->statistics(s->context,statistics))return 0;
    put(m,0x63a154,get(m,0x63a154)+1);
    for(i=0;i<5;i++)put(m,0x63a170+i*4,statistics[i]);
    put(m,0x5a07f4,0);put(m,0x5a07f8,0);put(m,0x5a07fc,0);return !m->fault;
}
int game_render_texture_use(GameMemory *m,GameAddress texture,const GameRenderTextureServices *s){
    GameAddress base=get(m,0x63a184),end=get(m,0x63a158);
    uint32_t frame=get(m,0x63a154),bytes,flags;uint8_t format[4];
    TextureRecord *record=s && s->native_record?s->native_record(s->context,texture):NULL;
    if(!get(m,0x63a168) || !s)return 0;
    if(record){
        record->last_use=frame;
        if(record->flags&0x80000000u)return 1;
        if(!s->upload_owned || !record->image.size || record->image.size>UINT32_MAX ||
           !s->upload_owned(s->context,texture,&record->image))return 0;
        bytes=(uint32_t)record->image.size;record->address=texture;record->flags|=0x80000000u;
    }else{
        if(texture<base || texture>=end || ((texture-base)&31) || !s->upload)return 0;
        flags=get(m,texture);put(m,texture+4,frame);
        if(flags&0x80000000u)return !m->fault;
        bytes=get(m,texture+12);
        if(!game_memory_read(m,texture+16,format,4) ||
           !s->upload(s->context,texture,format,get(m,texture+20),bytes))return 0;
        put(m,texture+8,texture);put(m,texture,flags|0x80000000u);
    }
    put(m,0x5a07f4,get(m,0x5a07f4)+bytes);return !m->fault;
}
int game_render_texture_forget(GameMemory *m,GameAddress texture){
    TextureRecord *record;
    if(!get(m,0x63a168))return !m->fault;
    record=active.native_record?active.native_record(active.context,texture):NULL;
    if(record){record->flags&=0x7fffffffu;record->address=0;}
    else {GameAddress base=get(m,0x63a184),end=get(m,0x63a158);
        if(texture>=base && texture<end && !((texture-base)&31)){
            put(m,texture,get(m,texture)&0x7fffffffu);put(m,texture+8,0);
        }
    }
    if(active.release)active.release(active.context,texture);
    return !m->fault;
}