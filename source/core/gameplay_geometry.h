#ifndef UPRISING_NATIVE_GAMEPLAY_GEOMETRY_H
#define UPRISING_NATIVE_GAMEPLAY_GEOMETRY_H

#include "game_memory.h"
#include "screen_primitives.h"
#include "asset_mesh.h"
#include "scene_model.h"
#include "scene_surface.h"
#include "gameplay_clipper.h"
#include "terrain_edge_stream.h"

#include <stdint.h>

long double gameplay_geometry_square_root(float value);
int gameplay_geometry_entity_position(GameMemory *,GameAddress,float[3]);
int gameplay_geometry_position_clear(GameMemory *,GameAddress,const float position[3],int *clear);
int gameplay_geometry_position_clear_except(GameMemory *,GameAddress,const float position[3],GameAddress excluded,int *clear);
int gameplay_geometry_nearby_location(GameMemory *,float x,float z,float radius,float output[2]);
int gameplay_geometry_circle_location(GameMemory *,float x,float z,float radius,float output[2]);
float gameplay_geometry_fast_distance(GameMemory *,float,float,float,float);
int gameplay_geometry_interpolate_timed_vector(GameMemory *memory,
    GameAddress output, GameAddress first, float first_duration,
    GameAddress second, float second_duration, long double final_values[3]);

void gameplay_geometry_mirror_square_texture(GameMemory *memory,
    GameAddress pixels, int32_t dimension);
GameAddress gameplay_geometry_select_visible_batches(GameMemory *memory,
    GameAddress batches, int32_t batch_count, float threshold);
void gameplay_geometry_smooth_yaw(GameMemory *memory, GameAddress object);
void gameplay_geometry_premultiply_uv(GameMemory *memory, GameAddress head);
int gameplay_geometry_world_bounds_contains(GameMemory *memory,
    GameAddress object);
GameAddress gameplay_geometry_filter_backfaces(GameMemory *memory,
    GameAddress head, float threshold);
int gameplay_geometry_project_vertices(GameMemory *memory,
    GameAddress begin, GameAddress end, float center_x, float center_y,
    float scale, float vertical_scale, float numerator);

typedef struct GameplayGeometryTransformResult {
    uint32_t eax;
    uint32_t ebx;
    GameAddress output;
    int processed;
} GameplayGeometryTransformResult;

GameplayGeometryTransformResult gameplay_geometry_transform_vertex_stream(
    GameMemory *memory, GameAddress matrix, GameAddress begin,
    GameAddress end, GameAddress output, uint32_t initial_eax);
GameplayGeometryTransformResult gameplay_geometry_transform_packed_stream(
    GameMemory *memory, GameAddress matrix, GameAddress begin,
    GameAddress end, GameAddress output, uint32_t initial_eax,
    uint32_t initial_ebx);

typedef struct GameplayGeometryObjectTransformResult {
    uint32_t flags;
    uint32_t edx;
    int transformed;
    int mark_complete;
} GameplayGeometryObjectTransformResult;

GameplayGeometryObjectTransformResult gameplay_geometry_transform_object_points(
    GameMemory *memory, GameAddress object, uint32_t initial_edx);

typedef struct GameplayGeometryPacketResult {
    GameAddress positions;
    GameAddress packet;
    uint32_t material;
} GameplayGeometryPacketResult;

typedef struct GameplayTerrainTriangle { uint8_t corner[3];float uv[3][2]; } GameplayTerrainTriangle;
void gameplay_geometry_terrain_triangles(uint32_t diagonal,uint32_t layout,GameplayTerrainTriangle output[2]);
GameplayGeometryPacketResult gameplay_geometry_project_terrain_packets(GameMemory *memory,
    const GameAddress vertices[4],uint32_t material,uint32_t diagonal,uint32_t layout);
int16_t gameplay_geometry_wave_height(GameMemory *memory,float x,float z,int alternate);
GameplayGeometryPacketResult gameplay_geometry_build_terrain_packets(
    GameMemory *memory, GameAddress vertices, uint32_t material,
    uint32_t diagonal, uint32_t layout_index);

typedef struct GameplayGeometryPointOps {
    void *context;
    int (*draw)(void *context, const GameRenderVertex *vertex);
} GameplayGeometryPointOps;
typedef struct GameplayPointFrame {
    float x_center,y_center,y_scale,projection;
    float x_min,y_min,x_max,y_max,rounding_magic;
    const uint32_t *palette;
} GameplayPointFrame;
typedef struct GameplayPointInput {
    float x,y,z;
    uint8_t active;
} GameplayPointInput;
typedef struct GameplayPointList {
    GameplayPointInput *items;
    uint32_t count,palette;
} GameplayPointList;
int gameplay_geometry_capture_point_list(GameMemory *,GameAddress,
    GameplayPointList *);
int gameplay_geometry_draw_captured_points(const GameplayPointList *,
    const GameplayPointFrame *,const GameplayGeometryPointOps *);
void gameplay_geometry_release_point_list(GameplayPointList *);

typedef struct GameplayGeometryTriangleOps {
    void *context;
    int (*draw)(void *context, GameAddress first, GameAddress second,
        GameAddress third);
} GameplayGeometryTriangleOps;

int gameplay_geometry_draw_point_list(GameMemory *memory, GameAddress list,
    const GameplayPointFrame *frame,const GameplayGeometryPointOps *ops);
GameAddress gameplay_geometry_triangulate_ring(GameMemory *memory,
    uint32_t count, GameAddress begin,
    const GameplayGeometryTriangleOps *ops);

typedef struct GameplayGeometryTextureOps {
    void *context;
    int (*allocate)(void *context, uint32_t size, GameAddress *result);
    int (*release)(void *context, GameAddress resource);
} GameplayGeometryTextureOps;

int gameplay_geometry_expand_tall_texture(GameMemory *memory,
    GameAddress info, const GameplayGeometryTextureOps *ops);

typedef struct GameplayGeometryFootprintResult {
    GameAddress height_table;
    uint32_t integer_z_high;
} GameplayGeometryFootprintResult;

typedef struct GameplayHeadingMatrixResult {
    GameAddress matrix;
    uint32_t translation_x;
} GameplayHeadingMatrixResult;

typedef struct GameplaySpriteOverlapResult {
    uint32_t value;
    uint32_t branch;
    uint32_t depth_bits;
    uint32_t distance_bits;
    uint32_t sample_index;
    uint32_t horizontal_squared;
    uint32_t distance_squared;
} GameplaySpriteOverlapResult;

int gameplay_geometry_footprint_blocked(GameMemory *memory,
    GameAddress player, GameplayGeometryFootprintResult *result);
int gameplay_geometry_footprint_values(GameMemory *,const float position[3],
    const float footprint[8][3],GameplayGeometryFootprintResult *);
int gameplay_geometry_world_bounds_values(float radius,const float position[3]);
long double gameplay_geometry_terrain_height(GameMemory *memory,
    float world_x, float world_z);
long double gameplay_geometry_detail_height(GameMemory *memory,
    float world_x, float world_z);
int gameplay_geometry_terrain_normal(GameMemory *memory, GameAddress output,
    float world_x, float world_z);
int gameplay_geometry_terrain_normal_values(GameMemory *,float output[3],float,float);
int16_t gameplay_geometry_build_object_vertices(GameMemory *memory,
    GameAddress object, int use_template);
GameplayHeadingMatrixResult gameplay_geometry_build_heading_matrix(
    GameMemory *memory, GameAddress object);
void gameplay_geometry_build_orientation_matrix(GameMemory *memory,
    GameAddress object);
void gameplay_geometry_build_yaw_matrix(GameMemory *memory,
    GameAddress object);
void gameplay_geometry_setup_angle_basis(GameMemory *memory, uint16_t angle);
GameplaySpriteOverlapResult gameplay_geometry_sprite_overlap(
    GameMemory *memory, GameAddress context, GameAddress sample);

int16_t gameplay_geometry_terrain_cell_height(GameMemory *memory,float x,float z);
uint8_t gameplay_geometry_terrain_class(GameMemory *memory,float x,float y,float z);
typedef struct GameplayTerrainFrame {
    GameplayClipCursors clip;
    GameAddress vertex_begin,node_cursor,render_head,previous_head;
    float center_x,center_y,scale,vertical,numerator,backface_threshold;
    float bounds[4];
    int32_t far_distance;
    GameplayTerrainEdge edges[10];
    uint8_t edge_column_changed[10];
} GameplayTerrainFrame;
int gameplay_geometry_render_terrain_frame(GameMemory *,GameAddress matrix,
    int32_t count,float world_x,float world_z,GameplayTerrainFrame *);
typedef struct GameplayMeshSubmission {
    GameAddress matrix,source_vertices,transformed_vertices,polygons;
    int32_t vertex_count,polygon_count;
    uint32_t flags,texture_state,material_state;float far_depth;
} GameplayMeshSubmission;
typedef struct GameplayRenderFrame {
    GameAddress clip_vertices,clip_packets,clip_uv,clip_indices,queue_head;
    float center_x,center_y,scale,vertical,numerator,backface_threshold;
    float bounds[4];
    GameplayClipCursors *cursors;
    GameAddress *queue_head_value;
} GameplayRenderFrame;
int gameplay_geometry_submit_mesh(GameMemory *,const GameplayMeshSubmission *);
int gameplay_geometry_submit_mesh_frame(GameMemory *,const GameplayMeshSubmission *,
    const GameplayRenderFrame *);
int gameplay_geometry_submit_scene_mesh(GameMemory *,const GameplayMeshSubmission *,
    const GameplayRenderFrame *,const float matrix[12],const AssetMeshVertex *,
    const GameplayClipPolygonInput *);
int gameplay_geometry_submit_visible_scene_mesh(GameMemory *,
    const GameplayMeshSubmission *,const GameplayRenderFrame *,
    const float matrix[12],const AssetMeshVertex *,GameAddress batches,
    int32_t batch_count,const ScenePolygon *native_polygons);
int gameplay_geometry_classify_visible_queue(GameMemory *,GameAddress head,
    GameAddress view_begin,GameAddress view_end);
typedef struct GameplayViewRect {
    int16_t y_min,x_min,y_max,x_max;
} GameplayViewRect;
/* 1 classified, 0 external queue storage, -1 malformed owned queue. */
int gameplay_geometry_classify_owned_visible_queue(GameAddress head,
    const GameplayViewRect *views,size_t view_count);
typedef int (*GameplaySurfacePolygon)(void *,const GameRenderVertex *,unsigned);
typedef struct GameplaySurfaceStorageOps {
    void *context;
    int (*read)(void *,GameAddress,void *,size_t);
    int (*write)(void *,GameAddress,const void *,size_t);
    int (*cursors_ready)(void *,const GameplayClipCursors *);
    uint32_t *(*index)(void *,GameAddress);
    float *(*component)(void *,GameAddress);
    RenderArenaPacket *(*packet)(void *,GameAddress);
    void (*note_write)(void *,GameAddress);
} GameplaySurfaceStorageOps;
int gameplay_geometry_render_surface(GameMemory *,GameplaySurfacePolygon,void *);
int gameplay_geometry_render_surface_tables(GameMemory *,const GameAddress [5],
    GameplaySurfacePolygon,void *);
int gameplay_geometry_render_surface_frame(GameMemory *,const GameAddress [5],
    const GameplayRenderFrame *,const SceneSurfaceAnimationInput *,
    GameplaySurfacePolygon,void *,SceneSurface *);
int gameplay_geometry_render_owned_surface(SceneSurface *,
    const GameplayRenderFrame *,const SceneSurfaceAnimationInput *,
    GameplaySurfacePolygon,void *);
int gameplay_geometry_render_owned_surface_storage(SceneSurface *,
    const GameplayRenderFrame *,const SceneSurfaceAnimationInput *,
    GameplaySurfacePolygon,void *,const GameplaySurfaceStorageOps *);
int gameplay_geometry_owned_surface_ready(const SceneSurface *,
    const GameplayRenderFrame *);
void gameplay_geometry_orientation_values(GameMemory *, const uint16_t angles[3],
    const float position[3], float output[12]);
/*532250: center, logical size and xmin/ymin/xmax/ymax bounds; the verified
 *projection record retains its original scalar publications. */
int gameplay_geometry_projection_setup(GameMemory *,GameAddress,const float center[2],
    float width,float height,const float bounds[4]);
#endif
