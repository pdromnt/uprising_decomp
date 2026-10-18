#ifndef UPRISING_RENDER_RUNTIME_PROJECTION_H
#define UPRISING_RENDER_RUNTIME_PROJECTION_H

#include "render_runtime_pools.h"

/* Synthetic x86 addresses are published only by this compatibility adapter.
 * Native storage and typed access use pool indices and byte offsets. */
#define RENDER_RUNTIME_POOL_BASE 0xfb000000u
#define RENDER_RUNTIME_POOL_STRIDE 0x10000u
#define RENDER_RUNTIME_POOL_END \
    (RENDER_RUNTIME_POOL_BASE + RENDER_RUNTIME_POOL_STRIDE * RENDER_RUNTIME_POOL_COUNT)

static inline uint32_t render_runtime_pool_view(unsigned index){
    return index<RENDER_RUNTIME_POOL_COUNT?
        RENDER_RUNTIME_POOL_BASE+index*RENDER_RUNTIME_POOL_STRIDE:0u;
}
static inline int render_runtime_pool_decode(uint32_t address,
    unsigned *index,size_t *offset){
    uint32_t relative;
    if(!index || !offset || address<RENDER_RUNTIME_POOL_BASE ||
       address>=RENDER_RUNTIME_POOL_END)return 0;
    relative=address-RENDER_RUNTIME_POOL_BASE;
    *index=relative/RENDER_RUNTIME_POOL_STRIDE;
    *offset=relative%RENDER_RUNTIME_POOL_STRIDE;
    return 1;
}
static inline int render_runtime_pool_read(uint32_t address,void *output,
    size_t bytes){
    unsigned index;size_t offset;
    return render_runtime_pool_decode(address,&index,&offset) &&
        render_runtime_pool_read_native(index,offset,output,bytes);
}
static inline int render_runtime_pool_write(uint32_t address,const void *input,
    size_t bytes){
    unsigned index;size_t offset;
    return render_runtime_pool_decode(address,&index,&offset) &&
        render_runtime_pool_write_native(index,offset,input,bytes);
}
static inline RenderArenaPacket *render_runtime_pool_packet(uint32_t address){
    unsigned index;size_t offset;
    return render_runtime_pool_decode(address,&index,&offset) && index==0u?
        render_runtime_pool_packet_native(offset):NULL;
}
static inline RenderArenaVertex *render_runtime_pool_vertex(uint32_t address){
    unsigned index;size_t offset;
    return render_runtime_pool_decode(address,&index,&offset) && index==1u?
        render_runtime_pool_vertex_native(offset):NULL;
}
static inline uint32_t *render_runtime_pool_index(uint32_t address){
    unsigned index;size_t offset;
    return render_runtime_pool_decode(address,&index,&offset) && index==2u?
        render_runtime_pool_index_native(offset):NULL;
}
static inline RenderArenaUv *render_runtime_pool_uv(uint32_t address){
    unsigned index;size_t offset;
    return render_runtime_pool_decode(address,&index,&offset)?
        render_runtime_pool_uv_native(index,offset):NULL;
}
static inline float *render_runtime_pool_float_component(uint32_t address){
    unsigned index;size_t offset;
    return render_runtime_pool_decode(address,&index,&offset)?
        render_runtime_pool_float_component_native(index,offset):NULL;
}
static inline RenderRuntimeViewRect *render_runtime_pool_view_rect(
    uint32_t address){
    unsigned index;size_t offset;
    return render_runtime_pool_decode(address,&index,&offset) && index==6u?
        render_runtime_pool_view_rect_native(offset):NULL;
}
static inline void render_runtime_pool_note_typed_write(uint32_t address){
    unsigned index;size_t offset;
    if(render_runtime_pool_decode(address,&index,&offset))
        render_runtime_pool_note_typed_write_native(index,offset);
}

#endif
