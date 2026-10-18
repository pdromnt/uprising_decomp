#include "level_load.h"
#include "model_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct TerrainSource {GameApplication *app;ResourceFile *file;const char *path;} TerrainSource;
static int terrain_line(void *context,char line[256]){
    TerrainSource *source=context;return resource_file_line(source->file,line,256)!=0;
}
static int terrain_bytes(void *context,void *output,size_t bytes){
    TerrainSource *source=context;return resource_file_read(source->file,output,bytes)==bytes;
}
static void terrain_tile_release(void *context,uint32_t handle){
    (void)game_model_texture_release(context,handle);
}
/*Packed banks and named tiles share the existing pixel owner. The text tile
 *line is consumed even when the packed bank supplies its image. */
static int terrain_tiles(void *context,uint32_t count,MaterialValue *output){
    TerrainSource *source=context;GameApplication *a=source->app;ResourceFile *pack=NULL;
    ResourceBlob bank={0};const char *base=source->path,*separator;char stem[256],path[512],line[256],name[256];
    size_t length;uint32_t i,created=0;int packed=0,ok=0,n;
    separator=strrchr(base,'/');if(separator)base=separator+1;
    separator=strrchr(base,'\\');if(separator)base=separator+1;
    length=strcspn(base,".");if(!length || length>=sizeof(stem) || count>65536)return 0;
    memcpy(stem,base,length);stem[length]=0;
    n=snprintf(path,sizeof(path),"grids/packs/%s_01.pak",stem);if(n<0 || (size_t)n>=sizeof(path))return 0;
    pack=resource_file_open(path);
    if(pack){
        packed=1;bank.size=4+(size_t)count*1024;bank.data=malloc(bank.size);
        if(!bank.data || resource_file_read(pack,bank.data,bank.size)!=bank.size)goto done;
        resource_file_close(pack);pack=NULL;
    }
    for(i=0;i<count;++i){AssetTexture image={0};MaterialValue value={0};
        if(!terrain_line(source,line))goto done;
        if(packed){
            int published=asset_texture_terrain_tile(&bank,i,&image) && game_model_texture_take(a,&image,0,&value);
            asset_texture_release(&image);if(!published)goto done;
        }else{
            if(sscanf(line,"%255s",name)!=1)goto done;
            n=snprintf(path,sizeof(path),"tiles1/%s",name);if(n<0 || (size_t)n>=sizeof(path))goto done;
            if(!game_model_material_load(a,path,-1,0,1,&value))goto done;
        }
        output[created++]=value;
    }
    ok=!a->memory.fault;
done:
    if(pack)resource_file_close(pack);
    /*This descriptor borrows our manually allocated pack buffer. It was not
     * created by resource_blob_load and has no allocator callback. */
    free(bank.data);
    if(!ok)while(created){--created;terrain_tile_release(a,output[created].backend_handle);memset(&output[created],0,sizeof(*output));}
    return ok;
}
/*512820: the parser, decoded cells, tile pixels and teardown use native owners.
 *Publish only after the complete replacement has succeeded. */
int game_level_terrain_load(GameApplication *a,const char *path,int32_t offset){
    ResourceFile *file;TerrainSource context;LevelTerrainSource source;uint32_t seed;int ok,created=0;
    if(!a || !path || offset<0 || a->memory.fault)return 0;
    file=resource_file_open(path);if(!file)return 0;
    if(!a->terrain){a->terrain=calloc(1,sizeof(*a->terrain));created=1;}
    if(!a->terrain){resource_file_close(file);return 0;}
    context.app=a;context.file=file;context.path=path;
    source.context=&context;source.release_context=a;source.read_line=terrain_line;source.read_bytes=terrain_bytes;
    source.load_tiles=terrain_tiles;source.release_tile=terrain_tile_release;
    seed=game_memory_read_u32(&a->memory,0x5a1cd0);
    ok=!a->memory.fault && resource_file_seek(file,offset,SEEK_SET) &&
       level_terrain_load(a->terrain,&source,game_memory_read_u32(&a->memory,0x626718)!=0,&seed);
    resource_file_close(file);if(!ok){if(created){free(a->terrain);a->terrain=NULL;}return 0;}
    a->memory.terrain=level_terrain_asset(a->terrain);
    game_memory_write_u32(&a->memory,0x597d90,GAME_TERRAIN_CELLS);
    game_memory_write_u32(&a->memory,0x597d8c,GAME_TERRAIN_COARSE);
    game_memory_write_u32(&a->memory,0x597d94,GAME_TERRAIN_MATERIALS);
    game_memory_write_u16(&a->memory,0x626720,a->terrain->asset.width);
    game_memory_write_u16(&a->memory,0x626722,a->terrain->asset.height);
    game_memory_write_u16(&a->memory,0x626724,(uint16_t)a->terrain->asset.minimum);
    game_memory_write_u16(&a->memory,0x626726,(uint16_t)a->terrain->asset.maximum);
    game_memory_write_u32(&a->memory,0x5a1cd0,seed);return !a->memory.fault;
}
int game_level_terrain_release(GameApplication *a){
    if(!a)return 0;
    level_terrain_unload(a->terrain);free(a->terrain);a->terrain=NULL;a->memory.terrain=NULL;
    game_memory_write_u32(&a->memory,0x597d90,0);game_memory_write_u32(&a->memory,0x597d8c,0);game_memory_write_u32(&a->memory,0x597d94,0);
    return !a->memory.fault;
}
/*Bounded legacy views select fields of the sole owner, byte by byte. They never
 *serialize another writable terrain grid or expose host padding/alignment. */
static int terrain_byte(GameApplication *a,GameAddress address,uint8_t *byte,int write){
    TerrainAsset *t=level_terrain_asset(a->terrain);uint32_t offset,index,part,value,shift;
    if(!t)return 0;
    if(address>=GAME_TERRAIN_CELLS && address<GAME_TERRAIN_COARSE){TerrainCell *cell;
        offset=address-GAME_TERRAIN_CELLS;index=offset/8;part=offset%8;cell=&t->cells[index];
        value=part<2?(uint16_t)cell->height:part<4?cell->tile:part==4?cell->light:part==5?cell->flags:cell->reserved;
        shift=part<4 || part>=6?(part&1)*8:0;
        if(!write){*byte=(uint8_t)(value>>shift);return 1;}
        value=(value&~(255u<<shift))|((uint32_t)*byte<<shift);
        if(part<2)cell->height=(int16_t)(value<32768?(int32_t)value:(int32_t)value-65536);
        else if(part<4)cell->tile=(uint16_t)value;
        else if(part==4)cell->light=(uint8_t)value;
        else if(part==5)cell->flags=(uint8_t)value;
        else cell->reserved=(uint16_t)value;
        return 1;
    }
    if(address>=GAME_TERRAIN_COARSE && address<GAME_TERRAIN_COARSE+8192){
        offset=address-GAME_TERRAIN_COARSE;shift=(offset&1)*8;value=(uint16_t)t->coarse[offset/2];
        if(!write)*byte=(uint8_t)(value>>shift);
        else{value=(value&~(255u<<shift))|((uint32_t)*byte<<shift);t->coarse[offset/2]=(int16_t)(value<32768?(int32_t)value:(int32_t)value-65536);}
        return 1;
    }
    if(address>=GAME_TERRAIN_MATERIALS){MaterialValue *material;
        offset=address-GAME_TERRAIN_MATERIALS;index=offset/8;part=offset%8;if(index>=t->material_count)return 0;
        material=&t->materials[index];value=part<2?material->width:part<4?material->height:material->backend_handle;
        shift=part<4?(part&1)*8:(part-4)*8;
        if(!write)*byte=(uint8_t)(value>>shift);
        else{value=(value&~(255u<<shift))|((uint32_t)*byte<<shift);
            if(part<2)material->width=(uint16_t)value;else if(part<4)material->height=(uint16_t)value;else material->backend_handle=value;}
        return 1;
    }
    return 0;
}
int game_level_terrain_read(GameApplication *a,GameAddress address,void *output,size_t size){
    size_t i;uint8_t *bytes=output;if(!a || !output || size>UINT32_MAX-address)return 0;
    for(i=0;i<size;++i)if(!terrain_byte(a,address+(uint32_t)i,&bytes[i],0))return 0;
    return 1;
}
int game_level_terrain_write(GameApplication *a,GameAddress address,const void *input,size_t size){
    size_t i;uint8_t byte;const uint8_t *bytes=input;if(!a || !input || size>UINT32_MAX-address)return 0;
    for(i=0;i<size;++i)if(!terrain_byte(a,address+(uint32_t)i,&byte,0))return 0;
    for(i=0;i<size;++i){byte=bytes[i];if(!terrain_byte(a,address+(uint32_t)i,&byte,1))return 0;}
    return 1;
}
