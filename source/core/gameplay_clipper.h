#ifndef UPRISING_NATIVE_GAMEPLAY_CLIPPER_H
#define UPRISING_NATIVE_GAMEPLAY_CLIPPER_H

#include "game_memory.h"

#include <stdint.h>

#define GAMEPLAY_CLIP_MAX_VERTICES 25u
typedef struct GameplayClipPolygonInput {
    int32_t count;
    uint32_t flags;
    uint32_t vertices[GAMEPLAY_CLIP_MAX_VERTICES];
    float uv[GAMEPLAY_CLIP_MAX_VERTICES][2];
    float scalars[GAMEPLAY_CLIP_MAX_VERTICES];
} GameplayClipPolygonInput;
typedef struct GameplayClipCursors {
    GameAddress vertex,packet,uv,scalar;
} GameplayClipCursors;

GameAddress gameplay_clipper_clip_polygons(GameMemory *memory,
    GameAddress polygons, int32_t polygon_count, float near_z, float far_z,
    GameAddress vertex_cursor_pointer, GameAddress index_cursor_pointer,
    GameAddress uv_cursor_pointer, GameAddress scalar_cursor_pointer);
GameAddress gameplay_clipper_clip_polygon_inputs(GameMemory *memory,
    GameAddress polygons, int32_t polygon_count,
    const GameplayClipPolygonInput *inputs, float near_z, float far_z,
    GameAddress vertex_cursor_pointer, GameAddress index_cursor_pointer,
    GameAddress uv_cursor_pointer, GameAddress scalar_cursor_pointer);
GameAddress gameplay_clipper_clip_polygon_inputs_state(GameMemory *memory,
    GameAddress polygons,int32_t polygon_count,const GameplayClipPolygonInput *inputs,
    float near_z,float far_z,GameplayClipCursors *cursors);

typedef struct GameplayClipStatus {
    float last_coordinate,last_boundary;int had_compare,had_intersection;
} GameplayClipStatus;
GameAddress gameplay_clipper_near_list(GameMemory *,GameAddress,float,float,
    GameAddress,GameAddress,GameAddress,GameAddress,GameplayClipStatus *);
GameAddress gameplay_clipper_near_list_state(GameMemory *,GameAddress,float,float,
    GameplayClipCursors *,GameplayClipStatus *);
void gameplay_clipper_screen_list(GameMemory *,GameAddress,GameAddress,const float[4],
    GameAddress,GameAddress,GameAddress,GameAddress,GameplayClipStatus *);
GameAddress gameplay_clipper_screen_chain(GameMemory *,GameAddress,GameAddress *,const float[4],
    GameAddress,GameAddress,GameAddress,GameAddress,GameplayClipStatus *);
GameAddress gameplay_clipper_screen_chain_state(GameMemory *,GameAddress,GameAddress *,
    const float[4],GameplayClipCursors *,GameplayClipStatus *);
#endif
