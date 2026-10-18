#ifndef UPRISING_GAMEPLAY_SPATIAL_H
#define UPRISING_GAMEPLAY_SPATIAL_H

#include "game_memory.h"

#include <stdint.h>

/* The game has one world index. Entity handles are borrowed from their owning
 * families; bucket storage and neighboring-cell selection belong to this grid. */
typedef struct GameplaySpatialGrid {
    GameAddress identity;
    GameAddress heads[2][32][32];
    int active;
} GameplaySpatialGrid;
GameplaySpatialGrid *gameplay_spatial_grid(GameAddress manager);
int gameplay_spatial_begin(GameAddress manager);
int gameplay_spatial_end(void);
int gameplay_spatial_projection_read(GameAddress,void *,size_t);
int gameplay_spatial_projection_write(GameAddress,const void *,size_t);
int gameplay_spatial_query_heads(GameplaySpatialGrid *,float,float,GameAddress heads[9]);
int gameplay_spatial_query_values(GameAddress manager,float x,float z,
    GameAddress buckets[9]);
int gameplay_spatial_query_group(GameAddress manager,float x,float z,
    GameAddress *group);

typedef struct GameplaySpatialInsertResult {
    GameAddress bucket;
    GameAddress bucket_base;
    GameAddress previous_head;
} GameplaySpatialInsertResult;

GameplaySpatialInsertResult gameplay_spatial_insert(GameMemory *memory,
    GameAddress manager, GameAddress entity, int32_t x_cell,
    int32_t z_cell, int alternate_plane);
GameAddress gameplay_spatial_extended_bucket(GameAddress manager,
    int32_t combined_cell);
void gameplay_spatial_clear_mask(GameMemory *memory, GameAddress mask,
    uint32_t dword_count, int reverse);
uint8_t gameplay_spatial_mask_value(GameMemory *memory, GameAddress mask,
    uint32_t cell_index);
uint32_t gameplay_spatial_mask_bit(GameMemory *memory, GameAddress mask,
    uint32_t cell_index, uint32_t bit_index);
int gameplay_spatial_cell_inside(int32_t x_cell, int32_t z_cell);
int gameplay_spatial_select_visible_cells(GameMemory *,float x,float z,uint16_t heading);
void gameplay_spatial_initialize_neighbors(GameMemory *memory,
    GameAddress manager);
GameAddress gameplay_spatial_bucket_head(GameAddress manager,
    uint32_t row, uint32_t column, uint32_t plane);

#endif
