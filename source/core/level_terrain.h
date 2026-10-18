#ifndef UPRISING_LEVEL_TERRAIN_H
#define UPRISING_LEVEL_TERRAIN_H
#include "terrain_asset.h"

/* The source owns any partial tile cleanup when load_tiles fails. Successful
   tiles transfer to the level until unload or replacement. */
typedef struct LevelTerrainSource {
    void *context;
    void *release_context;
    int (*read_line)(void *,char output[256]);
    int (*read_bytes)(void *,void *,size_t);
    int (*load_tiles)(void *,uint32_t,MaterialValue *);
    void (*release_tile)(void *,uint32_t);
} LevelTerrainSource;
typedef struct LevelTerrain {
    TerrainAsset asset;
    void (*release_tile)(void *,uint32_t);
    void *tile_context;
    uint32_t generation;
    int active;
} LevelTerrain;

int level_terrain_load(LevelTerrain *,const LevelTerrainSource *,int lighting,
    uint32_t *seed);
void level_terrain_unload(LevelTerrain *);
TerrainAsset *level_terrain_asset(LevelTerrain *);
#endif
