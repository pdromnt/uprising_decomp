#ifndef UPRISING_SCREEN_PRIMITIVES_H
#define UPRISING_SCREEN_PRIMITIVES_H

#include <stdint.h>

typedef struct GameRenderVertex {
    float x, y, z;
    float r, g, b;
    float ooz;
    float a;
    float oow;
    float sow0, tow0, oow0;
    float sow1, tow1, oow1;
} GameRenderVertex;

typedef struct GameFullscreenTileFields {
    float x[4];
    float y[4];
    float oow[4];
    float texture_extent;
} GameFullscreenTileFields;

void game_screen_build_point(GameRenderVertex *output,
    const GameRenderVertex *inherited, int32_t x, int32_t y,
    uint8_t red, uint8_t green, uint8_t blue);
void game_screen_build_vertical_line(GameRenderVertex output[2],
    const GameRenderVertex *inherited, int32_t x, int32_t y,
    uint8_t red, uint8_t green, uint8_t blue);
void game_screen_build_segment(GameRenderVertex output[2],
    const GameRenderVertex inherited[2], const int32_t coordinates[4],
    uint8_t red, uint8_t green, uint8_t blue);
void game_screen_build_quad(GameRenderVertex output[4],
    const GameRenderVertex inherited[4], int32_t left, int32_t top,
    int32_t right, int32_t bottom, const uint8_t colors[4][3],
    uint32_t reciprocal_depth_bits);
void game_screen_build_colored_polygon_vertex(GameRenderVertex *output,
    const GameRenderVertex *inherited, float x, float y, float epsilon,
    uint8_t red, uint8_t green, uint8_t blue, uint32_t depth_bits);
void game_screen_build_textured_polygon_vertex(GameRenderVertex *output,
    const GameRenderVertex *inherited, float x, float y, float epsilon,
    float s, float t, float texture_scale);
void game_screen_fullscreen_tile_vertices(GameRenderVertex output[4],unsigned x,unsigned y,unsigned size);
void game_screen_fullscreen_tile_fields(GameFullscreenTileFields *fields,
    unsigned x, unsigned y, unsigned tile_size);

typedef struct GameWorldVertexInput {float x,y,depth,light,u,v;} GameWorldVertexInput;
typedef struct GameWorldPolygonParams {
    uint32_t flags;int16_t width,height;float fog_scale,fog_max,fog_start,fog_factor,projection_magic;
} GameWorldPolygonParams;
int game_screen_build_world_polygon(GameRenderVertex *output,const GameWorldVertexInput *input,
    unsigned count,const GameWorldPolygonParams *params);
unsigned game_screen_polygon_triangles(const float *y,unsigned count,uint8_t output[30][3]);
#endif
