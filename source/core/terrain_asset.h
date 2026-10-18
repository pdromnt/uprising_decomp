#ifndef UPRISING_TERRAIN_ASSET_H
#define UPRISING_TERRAIN_ASSET_H
#include <stdint.h>
#include <stddef.h>
#include "material_cache.h"
typedef struct TerrainCell { int16_t height;uint16_t tile;uint8_t light,flags;uint16_t reserved; } TerrainCell;
typedef struct TerrainAsset { TerrainCell *cells;int16_t coarse[4096];uint16_t width,height;int16_t minimum,maximum;MaterialValue *materials;size_t material_count; } TerrainAsset;
size_t terrain_asset_payload_size(unsigned width,unsigned height);
/* Output must be empty. Owns a fixed256x256 grid; release after consumers finish. */
int terrain_asset_decode(const uint8_t *,size_t,unsigned,unsigned,int lighting,uint32_t *seed,TerrainAsset *);
void terrain_asset_release(TerrainAsset *);
void terrain_asset_light_direction(float output[3]);
uint8_t terrain_asset_light(int16_t center,int16_t east,int16_t south,int16_t west,int16_t north,const float direction[3],uint32_t *seed);
int terrain_height_step_allowed(const TerrainCell *cells,int x1,int y1,int x2,int y2,float diagonal,float axial);
void terrain_build_adjacency(const TerrainCell *cells,float diagonal,float axial,uint8_t output[8192]);
#endif
