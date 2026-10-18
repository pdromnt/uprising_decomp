#include "model_assets.h"
#include "application_archives.h"
#include "glide_texture_cache.h"
#include "scene_projection_storage.h"
#include <stdlib.h>
#include <string.h>

static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void store(uint8_t *p,uint32_t v){unsigned i;for(i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
/*559940: LOD/aspect select actual power-of-two dimensions. The unused final
 *descriptor word is not an image size and is never interpreted as a pointer. */
static int dimensions(const uint8_t format[4],uint16_t *width,uint16_t *height){
    int x,y,aspect;
    if(format[0]>8 || format[1]>format[0] || format[2]>6)return 0;
    x=y=8-format[1];aspect=(int)format[2]-3;
    if(aspect<0)y+=aspect;else x-=aspect;
    if(x<0 || y<0)return 0;
    *width=(uint16_t)(1u<<x);*height=(uint16_t)(1u<<y);return 1;
}
/*5598a0, shared by559bf0/55a240: the stream contains a32-byte defined record,
 *pixels, then optionally the112-byte NCC table. File pointer words are obsolete
 *identities, not addresses to dereference. The existing cache owns the newly
 *allocated record/pixels/auxiliary table through its normal retirement. */
static int read_texture(GameApplication *a,ResourceFile *file,uint32_t requested_flags,MaterialValue *out){
    uint8_t header[32],ncc[112],*pixels=NULL;uint32_t bytes,flags;uint16_t width,height;
    GameAddress record=0,aux=0;int ok=0;
    if(resource_file_read(file,header,32)!=32 || !dimensions(header+16,&width,&height))return 0;
    bytes=word(header+12);flags=word(header);
    if(!bytes || bytes>0x200000)return 0;
    pixels=malloc(bytes);if(!pixels)goto done;
    if(resource_file_read(file,pixels,bytes)!=bytes)goto done;
    if(flags&0x40000000u){
        if(resource_file_read(file,ncc,112)!=112)goto done;
        aux=scene_projection_storage_allocate(112,"cached texture NCC table");
        if(!aux || !game_memory_write(&a->memory,aux,ncc,112))goto done;
    }
    record=game_glide_texture_create(&a->memory,flags|requested_flags,header+16,pixels,bytes);
    if(!record)goto done;
    game_memory_write_u32(&a->memory,record+24,aux);aux=0;
    game_memory_write_u32(&a->memory,record+28,word(header+28));
    out->width=width;out->height=height;out->backend_handle=record;ok=!a->memory.fault;
done:
    free(pixels);if(aux)scene_projection_storage_free(aux);return ok;
}
/*The write route decodes through the same native bitmap producer as the normal
 *path. Serialization is transient; TextureStore remains its only image owner.
 *Native pointers never enter this32-bit format. Loading repairs the saved IDs. */
int game_model_cached_material_load(GameApplication *a,const char *path,int alpha,uint32_t flags,MaterialValue *out){
    ResourceFile *file;TextureRecord *record;uint8_t header[32]={0};
    if(!path || !out || a->memory.fault)return 0;
    file=game_application_archive_file(a,game_memory_read_u32(&a->memory,0x5a0800));
    if(!file){game_application_log(a,"texture cache","native stream owner is unavailable");return 0;}
    if(game_memory_read_u32(&a->memory,0x5a0804))return read_texture(a,file,flags,out);
    /*439a20/439a80 apply their caller flags after559bf0/55a240 completes cache
     *I/O. They belong to this live use, not the reusable serialized texture. */
    if(!game_model_bitmap_material_load(a,path,alpha,0,out))return 0;
    record=game_model_texture_record(a,out->backend_handle);
    if(!record || record->image.size>UINT32_MAX)return 0;
    store(header,record->flags&0x7ffffffbu);store(header+12,(uint32_t)record->image.size);
    header[16]=header[17]=record->image.lod;header[18]=record->image.aspect;header[19]=record->image.format;
    store(header+20,record->id);
    /*Original cache writes do not invalidate a successfully decoded texture. */
    if(resource_file_write(file,header,32)!=32 || resource_file_write(file,record->image.pixels,record->image.size)!=record->image.size)
        game_application_log(a,"texture cache","partial output; decoded texture remains valid");
    record->flags|=flags;
    return !a->memory.fault;
}
