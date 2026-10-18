#ifndef UPRISING_RENDER_RUNTIME_POOLS_H
#define UPRISING_RENDER_RUNTIME_POOLS_H

#include <stddef.h>
#include <stdint.h>
#include "render_frame_owner.h"
#include "render_arena.h"

#define RENDER_RUNTIME_POOL_COUNT 7u

typedef struct RenderRuntimePoolStats {
    uint32_t loads, releases, live, reads, writes;
    uint32_t frame_reads, frame_writes;
    uint32_t typed_views,typed_writes;
} RenderRuntimePoolStats;

typedef struct RenderRuntimeViewRect {
    int16_t y_min,x_min,y_max,x_max;
} RenderRuntimeViewRect;
typedef struct RenderRuntimePools {
    RenderArenaPacket *packets;
    RenderArenaVertex *vertices;
    uint32_t *indices;
    RenderArenaUv *uv,*uv_work;
    float *scalars;
    RenderRuntimeViewRect *views;
} RenderRuntimePools;

int render_runtime_pools_initialize(void);
void render_runtime_pools_release(void);
size_t render_runtime_pool_size(unsigned index);
int render_runtime_pool_read_native(unsigned index,size_t offset,
    void *output,size_t bytes);
int render_runtime_pool_write_native(unsigned index,size_t offset,
    const void *input,size_t bytes);
const RenderRuntimePools *render_runtime_pool_owner(void);
RenderArenaPacket *render_runtime_pool_packet_native(size_t offset);
RenderArenaVertex *render_runtime_pool_vertex_native(size_t offset);
uint32_t *render_runtime_pool_index_native(size_t offset);
RenderArenaUv *render_runtime_pool_uv_native(unsigned index,size_t offset);
float *render_runtime_pool_float_component_native(unsigned index,
    size_t offset);
RenderRuntimeViewRect *render_runtime_pool_view_rect_native(size_t offset);
void render_runtime_pool_note_typed_write_native(unsigned index,size_t offset);
void render_runtime_pool_stats(RenderRuntimePoolStats *output);
#endif
