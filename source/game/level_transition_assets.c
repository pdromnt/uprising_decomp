#include "level_load.h"
#include "asset_bitmap.h"
#include "scene_projection_storage.h"
#include "runtime_utilities.h"
#include <string.h>

static int filename(GameApplication *a,GameAddress address,char path[260]){
    unsigned i;
    if(!address)return 0;
    for(i=0;i<260;++i){path[i]=(char)game_memory_read_u8(&a->memory,address+i);if(!path[i])return !a->memory.fault;}
    return 0;
}
/*4d81e0: the game palette is the original BGRA table, unlike the window's
 *LOGPALETTE conversion. Borrow bytes only until the resource owner is released. */
int game_level_palette_load(GameApplication *a,GameAddress name,GameAddress output){
    ResourceBlob blob={0};AssetBitmap bitmap;char path[260];uint8_t encoded[1028];size_t written;int ok=0;
    if(!filename(a,name,path) || !game_application_load_blob(a,path,&blob))return 0;
    if(asset_bitmap_startup_palette(&blob,&bitmap,encoded,sizeof(encoded),&written) && bitmap.palette_count==256)
        ok=game_memory_write(&a->memory,output,bitmap.palette_bgra,1024);
    resource_blob_release(&blob);return ok && !a->memory.fault;
}
/*518040/4d8310/4d8590: supported transition images are640x480 indexed BMPs.
 *The original flips240 pairs of640-byte rows. Publish the same pixels in an
 *existing owned page; the fixed-width global remains the fullscreen source ID. */
static GameAddress bitmap_pixels(GameApplication *a,GameAddress name,int flip,uint32_t *width,uint32_t *height){
    ResourceBlob blob={0};AssetBitmap bitmap;GameAddress pixels=0;char path[260];unsigned row;
    if(!filename(a,name,path) || !game_application_load_blob(a,path,&blob))return 0;
    if(!asset_bitmap8_parse(&blob,&bitmap) || bitmap.width>65535 || bitmap.height>65535 ||
       (flip && (bitmap.width!=640 || bitmap.height!=480)))goto done;
    pixels=scene_projection_storage_allocate(bitmap.pixel_bytes,"level bitmap pixels");
    if(!pixels)goto done;
    for(row=0;row<bitmap.height;++row)if(!game_memory_write(&a->memory,pixels+row*bitmap.stride,
            bitmap.pixels+(flip?bitmap.height-1-row:row)*bitmap.stride,bitmap.stride)){
        (void)scene_projection_storage_free(pixels);pixels=0;break;
    }
    if(pixels){if(width)*width=bitmap.width;if(height)*height=bitmap.height;}
done:
    resource_blob_release(&blob);return pixels;
}
static int replace(GameApplication *a,GameAddress slot,GameAddress next){
    GameAddress previous=game_memory_read_u32(&a->memory,slot);
    if(a->memory.fault || (previous && !scene_projection_storage_free(previous)))return 0;
    game_memory_write_u32(&a->memory,slot,next);return !a->memory.fault;
}
int game_level_palette_initialize(GameApplication *a){
    GameAddress first=bitmap_pixels(a,0x59613c,1,NULL,NULL),second=0;
    if(!first || !game_level_palette_load(a,0x59613c,0x605460) || !(second=bitmap_pixels(a,0x596150,1,NULL,NULL)))goto failed;
    if(!replace(a,0x5960e8,first))goto failed;
    first=0;
    if(!replace(a,0x5960ec,second))goto failed;
    return 1;
failed:
    if(first)(void)scene_projection_storage_free(first);
    if(second)(void)scene_projection_storage_free(second);
    return 0;
}
int game_level_palette_map(GameApplication *a){
    GameAddress pixels=bitmap_pixels(a,0x596164,1,NULL,NULL);
    if(!pixels)return 0;
    if(replace(a,0x5960f0,pixels))return 1;
    (void)scene_projection_storage_free(pixels);return 0;
}
int game_level_transition_assets_release(GameApplication *a){
    static const GameAddress slots[]={0x5960e8,0x5960ec,0x5960f0};GameMemory cleanup=a->memory;unsigned i;
    cleanup.fault=0;
    for(i=0;i<3;++i){GameAddress pixels=game_memory_read_u32(&cleanup,slots[i]);
        if(cleanup.fault || (pixels && !scene_projection_storage_free(pixels)))return 0;
        game_memory_write_u32(&cleanup,slots[i],0);
    }
    return !cleanup.fault;
}
/*51bcc0/51bd40: the existing eight-byte descriptor owns dimensions and the
 *unflipped padded bitmap. It is an actual bitmap owner, not a callback region. */
int game_level_input_regions_load(GameApplication *a,GameAddress name){
    GameMemory *m=&a->memory;uint32_t width=0,height=0;
    GameAddress descriptor=runtime_display_descriptor(m),previous=0,pixels;
    int created=0;
    if(m->fault)return 0;
    pixels=bitmap_pixels(a,name,0,&width,&height);if(!pixels)return 0;
    if(!descriptor){descriptor=scene_projection_storage_allocate(8,"level bitmap descriptor");created=1;}
    if(!descriptor)goto failed;
    if(!created)previous=game_memory_read_u32(m,descriptor+4);
    if(m->fault || (previous && !scene_projection_storage_free(previous)))goto failed;
    runtime_store_display_descriptor(m,descriptor,(uint16_t)width,(uint16_t)height,pixels);
    runtime_publish_display_descriptor(m,descriptor);return !m->fault;
failed:
    if(created && descriptor)(void)scene_projection_storage_free(descriptor);
    (void)scene_projection_storage_free(pixels);return 0;
}
int game_level_input_regions_initialize(GameApplication *a){return game_level_input_regions_load(a,0x5980b0);}
int game_level_input_regions_release(GameApplication *a){
    GameMemory cleanup=a->memory;GameAddress descriptor,pixels;cleanup.fault=0;
    descriptor=runtime_display_descriptor(&cleanup);
    if(cleanup.fault)return 0;
    if(!descriptor)return 1;
    pixels=game_memory_read_u32(&cleanup,descriptor+4);
    if(cleanup.fault || (pixels && !scene_projection_storage_free(pixels)) || !scene_projection_storage_free(descriptor))return 0;
    runtime_publish_display_descriptor(&cleanup,0);return !cleanup.fault;
}
