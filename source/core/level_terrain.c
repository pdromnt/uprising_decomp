#include "level_terrain.h"
#include <stdio.h>
#include <stdlib.h>

TerrainAsset *level_terrain_asset(LevelTerrain *level){
    return level && level->active?&level->asset:NULL;
}
void level_terrain_unload(LevelTerrain *level){
    size_t i;
    if(!level || !level->active)return;
    for(i=0;i<level->asset.material_count;i++)
        if(level->asset.materials[i].backend_handle && level->release_tile)
            level->release_tile(level->tile_context,
                level->asset.materials[i].backend_handle);
    terrain_asset_release(&level->asset);
    level->release_tile=NULL;level->tile_context=NULL;level->active=0;
    ++level->generation;
}
int level_terrain_load(LevelTerrain *level,const LevelTerrainSource *source,
    int lighting,uint32_t *seed){
    TerrainAsset next={0};MaterialValue *materials=NULL;
    char header[256];uint8_t *payload=NULL;
    uint32_t next_seed;int width,height,count;size_t bytes,i;
    if(!level || !source || !source->read_line || !source->read_bytes ||
       !source->load_tiles || !source->release_tile || !seed ||
       !source->read_line(source->context,header) ||
       sscanf(header,"%d %d %d",&width,&height,&count)!=3 ||
       count<0 || count>65536)return 0;
    bytes=terrain_asset_payload_size((unsigned)width,(unsigned)height);
    if(!bytes)return 0;
    materials=calloc(count? (size_t)count:1u,sizeof(*materials));
    if(!materials)return 0;
    if(!source->load_tiles(source->context,(uint32_t)count,materials)){
        free(materials);return 0;
    }
    payload=malloc(bytes);
    next_seed=*seed;
    if(!payload || !source->read_bytes(source->context,payload,bytes) ||
       !terrain_asset_decode(payload,bytes,(unsigned)width,(unsigned)height,
           lighting,&next_seed,&next)){
        for(i=0;i<(size_t)count;i++)
            if(materials[i].backend_handle)
                source->release_tile(source->release_context,
                    materials[i].backend_handle);
        free(payload);free(materials);terrain_asset_release(&next);return 0;
    }
    free(payload);
    next.materials=materials;next.material_count=(size_t)count;
    level_terrain_unload(level);
    level->asset=next;level->release_tile=source->release_tile;
    level->tile_context=source->release_context;level->active=1;
    ++level->generation;*seed=next_seed;
    return 1;
}
