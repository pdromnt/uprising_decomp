#include "level_geometry.h"
#include "level_hud.h"
#include "asset_bitmap.h"
#include "scene_projection_storage.h"
#include "radar_compositor.h"
#include <stdio.h>
#include <string.h>

static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t value){game_memory_write_u32(&a->memory,p,value);}
static int name_read(GameApplication *a,GameAddress at,char output[260]){
    unsigned i;for(i=0;i<260;++i){output[i]=(char)game_memory_read_u8(&a->memory,at+i);if(!output[i])return !a->memory.fault;}return 0;
}
/*These original overview publications are the authoritative writable images.
 *The page owner holds their bytes; there is no serialized/native state pair. */
static int images_release(GameApplication *a){
    static const GameAddress slots[]={0x6025c4,0x60262c,0x60256c};unsigned i;int ok=1;
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i){
        GameAddress pixels=get(a,slots[i]);
        if(pixels && !scene_projection_storage_free(pixels)){ok=0;continue;}
        put(a,slots[i],0);
    }
    if(!game_level_hud_quad_release(a,get(a,0x602630)))ok=0;
    return ok && !a->memory.fault;
}
int game_level_geometry_release(GameApplication *a){
    int ok=game_level_geometry_controls_release(a);
    if(!game_radar_map_release(&a->memory))ok=0;
    if(!images_release(a))ok=0;
    return ok && !a->memory.fault;
}
/*49ca40 flag bitmap: reverse padded BMP rows into packed bytes. The second
 *image is the original mutable overview template, not a synchronization mirror. */
static int flag_images(GameApplication *a){
    char path[260];ResourceBlob blob={0};AssetBitmap image;GameAddress first=0,second=0;
    uint8_t *pixels,*template_pixels;size_t bytes;unsigned row;int ok=0;
    if(!name_read(a,0x58deb8,path) || !game_application_load_blob(a,path,&blob))return 0;
    if(!asset_bitmap8_parse(&blob,&image) || image.width>INT16_MAX || image.height>INT16_MAX ||
        image.height>SIZE_MAX/image.width)goto done;
    bytes=(size_t)image.width*image.height;
    first=scene_projection_storage_allocate(bytes,"overview flag image");
    second=scene_projection_storage_allocate(bytes,"overview flag template");
    pixels=scene_projection_storage_borrow_bytes(first,bytes);
    template_pixels=scene_projection_storage_borrow_bytes(second,bytes);
    if(!pixels || !template_pixels)goto done;
    for(row=0;row<image.height;++row)memcpy(pixels+(size_t)row*image.width,
        image.pixels+(size_t)(image.height-1-row)*image.stride,image.width);
    memcpy(template_pixels,pixels,bytes);
    game_memory_write_u16(&a->memory,0x602628,(uint16_t)image.width);
    game_memory_write_u16(&a->memory,0x60262a,(uint16_t)image.height);
    game_memory_write_u16(&a->memory,0x602568,(uint16_t)image.width);
    game_memory_write_u16(&a->memory,0x60256a,(uint16_t)image.height);
    put(a,0x60262c,first);first=0;put(a,0x60256c,second);second=0;ok=!a->memory.fault;
done:
    if(first)scene_projection_storage_free(first);
    if(second)scene_projection_storage_free(second);
    resource_blob_release(&blob);return ok;
}
/*516a20 and49cc72: optional grids/gohs TGA keeps the verified format/parser.
 *Its pixels are reversed once in their sole writable overview page. The input
 *blob is borrowed during decoding and then retired by its existing file owner. */
static int overview_image(GameApplication *a,const char *level){
    char path[260];ResourceBlob blob={0};AssetBitmap image;GameAddress id=0;
    uint8_t *pixels;unsigned row;size_t length=strlen(level),bytes;int n,ok=1;
    if(length<3)return 0;
    n=snprintf(path,sizeof(path),"grids\\gohs\\%s",level);
    if(n<3 || (size_t)n>=sizeof(path))return 0;
    memcpy(path+n-3,"tga",3);put(a,0x6025f4,0);
    if(!game_application_load_blob(a,path,&blob))return !a->memory.fault;
    if(!asset_tga8_parse(&blob,&image))goto done; /*Original optional failure. */
    if(image.width>INT16_MAX || image.height>INT16_MAX){ok=0;goto done;}
    bytes=(size_t)image.width*image.height;
    id=scene_projection_storage_allocate(bytes,"overview terrain image");
    pixels=scene_projection_storage_borrow_bytes(id,bytes);
    if(!pixels){ok=0;goto done;}
    for(row=0;row<image.height;++row)memcpy(pixels+(size_t)row*image.width,
        image.pixels+(size_t)(image.height-1-row)*image.width,image.width);
    game_memory_write_u16(&a->memory,0x6025c0,(uint16_t)image.width);
    game_memory_write_u16(&a->memory,0x6025c2,(uint16_t)image.height);
    put(a,0x6025c4,id);id=0;
done:
    if(id)scene_projection_storage_free(id);
    resource_blob_release(&blob);return ok && !a->memory.fault;
}
/*4d8630 is the minimap producer, not a menu/display-layout helper. Reuse its
 *existing native resizing/fog/map owner with the original unflipped TGA input. */
static int minimap_load(GameApplication *a,const char *level){
    char path[260];ResourceBlob blob={0};AssetBitmap image;int n,ok=1;
    n=snprintf(path,sizeof(path),"grids\\gohs\\%s",level);
    if(n<3 || (size_t)n>=sizeof(path))return 0;
    memcpy(path+n-3,"tga",3);
    if(!game_application_load_blob(a,path,&blob))return !a->memory.fault;
    if(asset_tga8_parse(&blob,&image))ok=game_radar_map_initialize(&a->memory,image.pixels,image.pixel_bytes,image.width,image.height);
    resource_blob_release(&blob);return ok && !a->memory.fault;
}
int game_level_geometry_load(GameApplication *a,GameAddress level_name){
    char level[260];GameAddress icon;unsigned i;
    if(!name_read(a,level_name,level) || get(a,0x60262c) || get(a,0x60256c) ||
       get(a,0x6025c4) || get(a,0x602630) || get(a,0x60259c) || get(a,0x602620) ||
       get(a,0x60261c) || get(a,0x602698) || a->memory.fault)return 0;
    game_memory_write_u8(&a->memory,0x6026b8,0);
    for(i=0;i<10;++i)put(a,0x602538+i*4,0xbf800000);
    if(!flag_images(a))goto fail;
    icon=game_level_hud_quad_create(a);put(a,0x602630,icon);
    if(!icon || !game_level_hud_quad_image(a,icon,10.0f,0x58df34) || !overview_image(a,level))goto fail;
    if(!game_level_geometry_controls_initialize(a) || !minimap_load(a,level))goto fail;
    put(a,0x5ac9c8,320);put(a,0x5ac9cc,240);
    return !a->memory.fault;
fail:
    (void)game_level_geometry_release(a);return 0;
}
