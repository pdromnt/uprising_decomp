#ifndef UPRISING_TERRAIN_EDGE_STREAM_H
#define UPRISING_TERRAIN_EDGE_STREAM_H

#include <stdint.h>

typedef struct GameplayTerrainPoint {
    float x,y,z;
} GameplayTerrainPoint;

typedef struct GameplayTerrainEdge {
    int32_t first_row,last_row,first_column,last_column;
    int32_t column_16_16,column_step_16_16;
} GameplayTerrainEdge;

typedef struct GameplayTerrainEdgeStream {
    GameplayTerrainEdge edges[10];
    uint32_t count;
} GameplayTerrainEdgeStream;

/* Five x/z points form the clipped world span; two trailing records hold the
   column endpoints consumed by the terrain row scan. */
int gameplay_terrain_edge_stream_build(const GameplayTerrainPoint points[5],
    GameplayTerrainEdgeStream *stream);

#endif
