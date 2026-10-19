#ifndef UPRISING_WORLD_RENDERER_H
#define UPRISING_WORLD_RENDERER_H
#include "game_memory.h"
#include "screen_primitives.h"
#include "material_cache.h"
#include "gameplay_geometry.h"
#include <stddef.h>
typedef enum WorldRenderState {WORLD_ALPHA_FADE,WORLD_ALPHA_TEST,WORLD_DEPTH_BIAS,WORLD_TEXTURE_FILTER,WORLD_DEPTH_WRITE} WorldRenderState;
/*The40000000 texture flag denotes the112-byte NCC table, not the1024-byte
 *P8 palette. Keep its defined byte representation independent of host layout. */
typedef struct WorldTextureProperties {uint32_t flags;uint8_t ncc[112];} WorldTextureProperties;
typedef struct WorldRenderFrame {
    float depth_scale,near_offset,fog_scale,fog_max,projection_magic;
    int32_t depth_limit;
    uint32_t texture_frame,palette_bytes,triangles;
    uint8_t base_palette_entries[4];
    uint32_t palette[256];
    GameplayPointFrame points;
} WorldRenderFrame;
typedef struct WorldRendererOps {
    void *context;
    int (*begin)(void *);
    int (*texture)(void *,uint32_t handle,uint32_t frame,WorldTextureProperties *);
    int (*state)(void *,WorldRenderState,int);
    int (*palette)(void *,const uint32_t *,unsigned first,unsigned last,int full);
    int (*polygon)(void *,const GameRenderVertex *,unsigned);
    int (*point)(void *,const GameRenderVertex *);
    int (*material)(void *,uint32_t reference,MaterialValue *);
    /* Preflight native texture ownership before beginning an owned queue. */
    int (*texture_owned)(void *,uint32_t handle);
    /* External palette maps remain an explicit capture-only compatibility edge. */
    int (*palette_map)(void *,GameAddress,uint8_t entries[4]);
    int (*ncc)(void *,const uint8_t table[112]);
    int (*motion_polygon)(void *,const GameRenderVertex *,unsigned,const uint64_t *keys);
} WorldRendererOps;
/* Captured draw state is independent of guest memory. palette_key is an
 * opaque identity for consecutive palette reuse. The producer owns point
 * storage while these immutable commands are drawn. */
typedef struct WorldRenderCommand {
    uint32_t count,flags,palette_key;
    MaterialValue material;
    uint8_t palette_map[4];
    GameWorldVertexInput input[32];
    uint64_t motion_keys[32];
    GameplayPointList points;
} WorldRenderCommand;
int world_renderer_draw_commands(const WorldRenderCommand *,size_t,
    WorldRenderFrame *,const WorldRendererOps *);
typedef struct WorldRendererStats {
    uint32_t captured_queues,native_packets,guest_packets,drawn_packets;
    uint32_t native_vertices,guest_vertices,native_uv,guest_uv;
    uint32_t scene_indices,guest_indices,scene_vertices,scene_uv;
    uint32_t point_lists,point_inputs;
    uint32_t projected_packets,projected_arena_vertices;
    uint32_t projected_scene_vertices,projected_guest_vertices;
    uint32_t projected_scene_uv,projected_guest_uv;
    uint32_t generated_fallbacks;
    uint32_t owned_attempts,owned_submissions,owned_rejects,owned_scanned;
    uint32_t owned_reject_kind;
    GameAddress owned_reject_address;
    uint32_t owned_texture_binds,external_texture_binds;
} WorldRendererStats;
void world_renderer_stats(WorldRendererStats *);
void world_renderer_note_texture_owner(int owned);
/* -1 requests legacy fallback before renderer state changes;0 failure;1 success. */
int world_renderer_submit(GameMemory *,GameAddress head,WorldRenderFrame *,
    const WorldRendererOps *);
/* 1 drawn, 0 backend failure, -1 mixed/unsupported before begin. */
int world_renderer_submit_owned(GameAddress head,struct TerrainAsset *,
    WorldRenderFrame *,const WorldRendererOps *);
typedef enum ProjectedQueueKind {PROJECTED_PLAIN,PROJECTED_TEXTURED,PROJECTED_ALPHA} ProjectedQueueKind;
typedef struct ProjectedRendererOps {
    void *context;
    int (*begin)(void *,ProjectedQueueKind);
    int (*polygon)(void *,const GameRenderVertex *,unsigned);
    int (*end)(void *,ProjectedQueueKind);
} ProjectedRendererOps;
typedef struct ProjectedRenderCommand {
    uint32_t count;
    GameWorldVertexInput input[32];
} ProjectedRenderCommand;
/* Draw one decoded packet; queue traversal and live relinking stay with its
 * producer. The callback's polygon result is returned directly. */
int world_renderer_draw_projected_command(const ProjectedRenderCommand *,
    ProjectedQueueKind,float magic,float alpha_scale,
    const ProjectedRendererOps *);
/* 1 submitted, 0 backend failure, -1 mixed/unsupported before begin. */
int world_renderer_projected_native(GameAddress head,ProjectedQueueKind,
    float magic,float alpha_scale,const ProjectedRendererOps *);
int world_renderer_projected(GameMemory *,GameAddress head,ProjectedQueueKind,
    float projection_magic,float alpha_scale,const ProjectedRendererOps *);
#endif
