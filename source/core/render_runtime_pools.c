#include "render_runtime_pools.h"
#include <stdlib.h>
#include <string.h>

enum {
    PACKET_COUNT=700,
    VERTEX_COUNT=3000,
    INDEX_COUNT=3000,
    UV_COUNT=3000,
    SCALAR_COUNT=3000,
    VIEW_COUNT=50
};
static const size_t pool_sizes[RENDER_RUNTIME_POOL_COUNT]={
    PACKET_COUNT*sizeof(RenderArenaPacket),
    VERTEX_COUNT*sizeof(RenderArenaVertex),
    INDEX_COUNT*sizeof(uint32_t),
    UV_COUNT*sizeof(RenderArenaUv),
    UV_COUNT*sizeof(RenderArenaUv),
    SCALAR_COUNT*sizeof(float),
    VIEW_COUNT*sizeof(RenderRuntimeViewRect)
};
static RenderRuntimePools pools;
static RenderRuntimePoolStats stats;

size_t render_runtime_pool_size(unsigned index){
    return index<RENDER_RUNTIME_POOL_COUNT?pool_sizes[index]:0u;
}
const RenderRuntimePools *render_runtime_pool_owner(void){
    return stats.live?&pools:NULL;
}
void render_runtime_pool_stats(RenderRuntimePoolStats *output){
    if(output){
        *output=stats;
        render_frame_cell_stats(&output->frame_reads,&output->frame_writes);
    }
}
static void release_storage(RenderRuntimePools *storage){
    free(storage->packets);free(storage->vertices);free(storage->indices);
    free(storage->uv);free(storage->uv_work);free(storage->scalars);
    free(storage->views);memset(storage,0,sizeof(*storage));
}
void render_runtime_pools_release(void){
    if(!stats.live)return;
    render_frame_cells_release();
    release_storage(&pools);
    stats.live=0u;++stats.releases;
}
int render_runtime_pools_initialize(void){
    RenderRuntimePools next={0};
    next.packets=calloc(PACKET_COUNT,sizeof(*next.packets));
    next.vertices=calloc(VERTEX_COUNT,sizeof(*next.vertices));
    next.indices=calloc(INDEX_COUNT,sizeof(*next.indices));
    next.uv=calloc(UV_COUNT,sizeof(*next.uv));
    next.uv_work=calloc(UV_COUNT,sizeof(*next.uv_work));
    next.scalars=calloc(SCALAR_COUNT,sizeof(*next.scalars));
    next.views=calloc(VIEW_COUNT,sizeof(*next.views));
    if(!next.packets || !next.vertices || !next.indices || !next.uv ||
       !next.uv_work || !next.scalars || !next.views){
        release_storage(&next);return 0;
    }
    render_runtime_pools_release();pools=next;
    render_frame_cells_reset();stats.live=1u;++stats.loads;
    return 1;
}
static uint8_t *pool_data(unsigned index){
    switch(index){
    case 0:return (uint8_t *)pools.packets;
    case 1:return (uint8_t *)pools.vertices;
    case 2:return (uint8_t *)pools.indices;
    case 3:return (uint8_t *)pools.uv;
    case 4:return (uint8_t *)pools.uv_work;
    case 5:return (uint8_t *)pools.scalars;
    case 6:return (uint8_t *)pools.views;
    }
    return NULL;
}
static uint8_t *pool_span(unsigned index,size_t offset,size_t bytes){
    if(!stats.live || index>=RENDER_RUNTIME_POOL_COUNT)return NULL;
    if(offset>pool_sizes[index] || bytes>pool_sizes[index]-offset)return NULL;
    return pool_data(index)+offset;
}
int render_runtime_pool_read_native(unsigned index,size_t offset,
    void *output,size_t bytes){
    uint8_t *source=pool_span(index,offset,bytes);
    if(!source || !output)return 0;
    memcpy(output,source,bytes);++stats.reads;return 1;
}
int render_runtime_pool_write_native(unsigned index,size_t offset,
    const void *input,size_t bytes){
    uint8_t *destination=pool_span(index,offset,bytes);
    if(!destination || !input)return 0;
    memcpy(destination,input,bytes);++stats.writes;return 1;
}
static void *clip_span(unsigned pool,size_t offset,size_t bytes,
    size_t alignment){
    if(offset%alignment)return NULL;
    return pool_span(pool,offset,bytes);
}
RenderArenaPacket *render_runtime_pool_packet_native(size_t offset){
    RenderArenaPacket *view=clip_span(0u,offset,sizeof(RenderArenaPacket),
        sizeof(RenderArenaPacket));
    if(view)++stats.typed_views;
    return view;
}
RenderArenaVertex *render_runtime_pool_vertex_native(size_t offset){
    RenderArenaVertex *view=clip_span(1u,offset,sizeof(RenderArenaVertex),
        sizeof(RenderArenaVertex));
    if(view)++stats.typed_views;
    return view;
}
uint32_t *render_runtime_pool_index_native(size_t offset){
    uint32_t *view=clip_span(2u,offset,sizeof(uint32_t),sizeof(uint32_t));
    if(view)++stats.typed_views;
    return view;
}
RenderArenaUv *render_runtime_pool_uv_native(unsigned index,size_t offset){
    RenderArenaUv *view=NULL;
    if(index==3u || index==4u)
        view=clip_span(index,offset,sizeof(RenderArenaUv),
            sizeof(RenderArenaUv));
    if(view)++stats.typed_views;
    return view;
}
float *render_runtime_pool_float_component_native(unsigned index,
    size_t offset){
    RenderArenaVertex *vertex;RenderArenaUv *uv;
    if(offset%sizeof(float))return NULL;
    if(index==1u){
        vertex=render_runtime_pool_vertex_native(offset-offset%
            sizeof(RenderArenaVertex));
        if(!vertex)return NULL;
        offset%=sizeof(RenderArenaVertex);
        return offset==12u?&vertex->light:&vertex->position[offset/4u];
    }
    if(index==3u || index==4u){
        uv=render_runtime_pool_uv_native(index,offset-offset%
            sizeof(RenderArenaUv));
        if(!uv)return NULL;
        return offset%sizeof(RenderArenaUv)?&uv->v:&uv->u;
    }
    if(index==5u){float *scalar=clip_span(5u,offset,sizeof(float),
        sizeof(float));
     if(scalar)++stats.typed_views;
     return scalar;}
    return NULL;
}
RenderRuntimeViewRect *render_runtime_pool_view_rect_native(size_t offset){
    RenderRuntimeViewRect *view=clip_span(6u,offset,
        sizeof(RenderRuntimeViewRect),
        sizeof(RenderRuntimeViewRect));
    if(view)++stats.typed_views;
    return view;
}
void render_runtime_pool_note_typed_write_native(unsigned index,size_t offset){
    if(pool_span(index,offset,1u))++stats.typed_writes;
}
