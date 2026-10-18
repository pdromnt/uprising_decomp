#ifndef UPRISING_RENDER_PACKET_VIEW_H
#define UPRISING_RENDER_PACKET_VIEW_H
#include "render_arena.h"
#include "scene_projection_storage.h"

/* Arena packets and scene polygon packets have one typed header contract.
 * Other external packet addresses still cross the GameMemory adapter. */
static inline RenderArenaPacket *render_packet_view(uint32_t address) {
    RenderArenaPacket *packet=render_arena_packets(address,1u);
    return packet?packet:scene_projection_storage_packet(address);
}

/*Screen clipping appends index lists to its shared packet scratch cursor.
 * Keep normal typed views as the fast path; decode only the bounded owned
 * word slice when a list occupies scratch storage instead of the index bank. */
static inline const uint32_t *render_packet_indices(uint32_t address,uint32_t count,uint32_t output[32]){
    const uint32_t *indices;uint8_t bytes[128];unsigned i;
    if(count>32 || (address&3u))return NULL;
    indices=render_arena_indices(address,count);
    if(!indices)indices=scene_projection_storage_links(address,count);
    if(indices)return indices;
    if(!output || !render_arena_read(address,bytes,(size_t)count*4))return NULL;
    for(i=0;i<count;++i)output[i]=(uint32_t)bytes[i*4]|((uint32_t)bytes[i*4+1]<<8)|
        ((uint32_t)bytes[i*4+2]<<16)|((uint32_t)bytes[i*4+3]<<24);
    return output;
}
static inline int render_packet_set_next(uint32_t address,uint32_t next_ref){
    return render_arena_packet_set_next(address,next_ref) ||
        scene_projection_storage_packet_set_next(address,next_ref);
}
static inline int render_packet_link(uint32_t address,uint32_t *next_ref,
    RenderArenaPacket **next_owned){
    return render_arena_packet_link(address,next_ref,next_owned) ||
        scene_projection_storage_packet_link(address,next_ref,next_owned);
}
#endif
