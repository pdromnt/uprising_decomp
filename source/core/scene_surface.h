#ifndef UPRISING_SCENE_SURFACE_H
#define UPRISING_SCENE_SURFACE_H
#include "game_memory.h"
#include "asset_mesh.h"
#include "render_arena.h"
#include <stddef.h>
#include <stdint.h>
#define SCENE_SURFACE_BASE 0xf8000000u
typedef struct SceneSurfaceAnimationInput {
    float external_phase,phase_step,period;
    float camera[3],grid_factor,x_wrap,z_wrap,uv_scale;
    float origin,height,grid_step;
    float cosine,sine,inverse_cosine,inverse_sine;
    float pitch_scale,vertical_scale;
    float view[12],far_depth,rounding_magic;
} SceneSurfaceAnimationInput;
typedef struct SceneSurface {
    uint8_t *table[5];
    GameAddress address[5];
    RenderArenaPacket polygons[25];
    AssetMeshVertex source[36];
    RenderArenaVertex transformed[36];
    GameAddress links[25][4];
    AssetMeshUV uv[25][4];
    uint32_t native_submissions;
    uint32_t native_queue_packets;
    uint32_t typed_queue_packets;
    uint32_t owned_ready_frames;
    uint32_t last_cursors[4];
    uint32_t texture_handle;
    void (*release_texture)(void *,uint32_t);
    void *texture_context;
    float phase;
    uint32_t published_phase_bits;
    uint8_t phase_active;
    uint8_t links_clean;
    uint8_t active;
} SceneSurface;
int scene_surface_initialize(SceneSurface *);
void scene_surface_release(SceneSurface *);
int scene_surface_take_texture(SceneSurface *,uint32_t,
    void (*release)(void *,uint32_t),void *context);
int scene_surface_read(const SceneSurface *,GameAddress,void *,size_t);
int scene_surface_write(SceneSurface *,GameAddress,const void *,size_t);
int scene_surface_write_pipeline(SceneSurface *,GameAddress,const void *,size_t);
int scene_surface_set_polygon(SceneSurface *,unsigned,const float [8]);
int scene_surface_set_vertex(SceneSurface *,unsigned,float,float,float);
RenderArenaVertex *scene_surface_transformed(SceneSurface *,GameAddress,
    size_t count);
float *scene_surface_transformed_component(SceneSurface *,GameAddress);
RenderArenaPacket *scene_surface_packet(SceneSurface *,GameAddress);
const GameAddress *scene_surface_link(const SceneSurface *,GameAddress);
int scene_surface_uv_component(const SceneSurface *,GameAddress,float *);
int scene_surface_set_uv_component(SceneSurface *,GameAddress,float);
int scene_surface_animate(SceneSurface *,const SceneSurfaceAnimationInput *,
    float matrix[12],float *phase);
#endif
