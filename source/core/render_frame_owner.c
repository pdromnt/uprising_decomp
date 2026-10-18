#include "render_frame_owner.h"
#include <string.h>

static RenderFrameOwner frame_owner;
static uint32_t frame_palette[256];
static uint32_t frame_counters[3];
static int frame_counters_known;
static RenderViewport frame_view;
static int frame_view_known;
static uint8_t frame_view_written[sizeof(RenderViewport)];
static uint8_t frame_view_image[sizeof(RenderViewport)];
static RenderWorldInputs world_inputs;
static int world_inputs_known;
static uint8_t world_cell_known[2];
static uint32_t world_input_snapshots;
static uint32_t view_snapshots,view_image_divergences;
static int frame_palette_known;
static uint32_t palette_snapshots,palette_image_divergences;
static uint8_t frame_cell_known[RENDER_FRAME_CELL_COUNT];
static uint32_t read_count,write_count;
static uint32_t binding_count,publication_count;
static int frame_active;

RenderFrameOwner *render_frame_owner(void){return frame_active?&frame_owner:NULL;}
void render_frame_cells_reset(void){
    memset(&frame_owner,0,sizeof(frame_owner));
    memset(frame_cell_known,0,sizeof(frame_cell_known));
    memset(frame_palette,0,sizeof(frame_palette));
    frame_palette_known=0;
    memset(frame_counters,0,sizeof(frame_counters));
    frame_counters_known=0;
    memset(&frame_view,0,sizeof(frame_view));
    frame_view_known=0;
    memset(frame_view_written,0,sizeof(frame_view_written));
    memset(frame_view_image,0,sizeof(frame_view_image));
    memset(&world_inputs,0,sizeof(world_inputs));
    world_inputs_known=0;
    memset(world_cell_known,0,sizeof(world_cell_known));
    world_input_snapshots=0u;
    view_snapshots=view_image_divergences=0u;
    palette_snapshots=palette_image_divergences=0u;
    binding_count=publication_count=0u;
    frame_active=1;
}
void render_frame_cells_release(void){
    memset(&frame_owner,0,sizeof(frame_owner));
    memset(frame_cell_known,0,sizeof(frame_cell_known));
    memset(frame_palette,0,sizeof(frame_palette));
    frame_palette_known=0;
    memset(frame_counters,0,sizeof(frame_counters));
    frame_counters_known=0;
    memset(&frame_view,0,sizeof(frame_view));
    frame_view_known=0;
    memset(frame_view_written,0,sizeof(frame_view_written));
    memset(frame_view_image,0,sizeof(frame_view_image));
    memset(&world_inputs,0,sizeof(world_inputs));
    world_inputs_known=0;
    memset(world_cell_known,0,sizeof(world_cell_known));
    frame_active=0;
}
int render_world_inputs_ready(void){return frame_active && world_inputs_known;}
int render_world_inputs_initialize(const uint32_t initial[8]){
    if(!frame_active || !initial)return 0;
    if(world_inputs_known)return 1;
    memcpy(&world_inputs.projection_magic,&initial[0],4u);
    memcpy(&world_inputs.depth_scale,&initial[1],4u);
    memcpy(&world_inputs.fog_max,&initial[2],4u);
    memcpy(&world_inputs.near_offset,&initial[3],4u);
    memcpy(&world_inputs.fog_scale,&initial[4],4u);
    if(!world_cell_known[0]){
        world_inputs.depth_limit=(int32_t)initial[5];
        world_cell_known[0]=1u;
    }
    if(!world_cell_known[1]){
        world_inputs.texture_frame=initial[6];
        world_cell_known[1]=1u;
    }
    memcpy(world_inputs.base_palette_entries,&initial[7],4u);
    world_inputs_known=1;
    return 1;
}
int render_world_inputs_snapshot(RenderWorldInputs *output){
    if(!output || !render_world_inputs_ready())return 0;
    *output=world_inputs;
    ++world_input_snapshots;
    return 1;
}
uint32_t render_world_inputs_snapshot_count(void){return world_input_snapshots;}
static uint8_t *world_cell_span(unsigned cell,size_t offset,size_t bytes,
    const uint8_t initial[4]){
    uint8_t *values[2]={(uint8_t *)&world_inputs.depth_limit,
        (uint8_t *)&world_inputs.texture_frame};
    if(!frame_active || !initial || cell>=RENDER_WORLD_CELL_COUNT ||
       offset>=4u || bytes>4u-offset)return NULL;
    if(!world_cell_known[cell]){
        memcpy(values[cell],initial,4u);
        world_cell_known[cell]=1u;
    }
    return values[cell]+offset;
}
int render_world_cell_read_native(unsigned cell,size_t offset,void *output,
    size_t bytes,
    const uint8_t initial[4]){
    uint8_t *span=world_cell_span(cell,offset,bytes,initial);
    if(!output || !span)return 0;
    memcpy(output,span,bytes);return 1;
}
int render_world_cell_write_native(unsigned cell,size_t offset,
    const void *input,size_t bytes,
    const uint8_t initial[4]){
    uint8_t *span=world_cell_span(cell,offset,bytes,initial);
    if(!input || !span)return 0;
    memcpy(span,input,bytes);return 1;
}
static int palette_span(size_t offset,size_t bytes,
    const uint8_t initial[1024],uint8_t **span){
    if(!frame_active || !initial || !span ||
       offset>=1024u || bytes>1024u-offset)return 0;
    if(!frame_palette_known){
        memcpy(frame_palette,initial,sizeof(frame_palette));
        frame_palette_known=1;
    }
    *span=(uint8_t *)frame_palette+offset;
    return 1;
}
int render_frame_palette_read_native(size_t offset,void *output,size_t bytes,
    const uint8_t initial[1024]){
    uint8_t *span;
    if(!output || !palette_span(offset,bytes,initial,&span))return 0;
    memcpy(output,span,bytes);return 1;
}
int render_frame_palette_write_native(size_t offset,const void *input,
    size_t bytes,
    const uint8_t initial[1024]){
    uint8_t *span;
    if(!input || !palette_span(offset,bytes,initial,&span))return 0;
    memcpy(span,input,bytes);return 1;
}
int render_frame_palette_snapshot(uint32_t output[256],
    const uint8_t initial[1024]){
    uint8_t *span;
    if(!output || !palette_span(0u,1024u,initial,&span))
        return 0;
    ++palette_snapshots;
    if(memcmp(span,initial,1024u))++palette_image_divergences;
    memcpy(output,span,1024u);return 1;
}
void render_frame_palette_stats(uint32_t *snapshots,
    uint32_t *image_divergences){
    if(snapshots)*snapshots=palette_snapshots;
    if(image_divergences)*image_divergences=palette_image_divergences;
}
static int counter_span(size_t offset,size_t bytes,
    const uint8_t initial[12],uint8_t **span){
    if(!frame_active || !initial || !span ||
       offset>=12u || bytes>12u-offset)return 0;
    if(!frame_counters_known){
        memcpy(frame_counters,initial,sizeof(frame_counters));
        frame_counters_known=1;
    }
    *span=(uint8_t *)frame_counters+offset;
    return 1;
}
int render_frame_counter_read_native(size_t offset,void *output,size_t bytes,
    const uint8_t initial[12]){
    uint8_t *span;
    if(!output || !counter_span(offset,bytes,initial,&span))return 0;
    memcpy(output,span,bytes);return 1;
}
int render_frame_counter_write_native(size_t offset,const void *input,
    size_t bytes,
    const uint8_t initial[12]){
    uint8_t *span;
    if(!input || !counter_span(offset,bytes,initial,&span))return 0;
    memcpy(span,input,bytes);return 1;
}
int render_frame_counter_snapshot(uint32_t output[3],
    const uint8_t initial[12]){
    uint8_t *span;
    if(!output || !counter_span(0u,12u,initial,&span))
        return 0;
    memcpy(output,span,12u);return 1;
}
static int view_span(size_t offset,size_t bytes,
    const uint8_t initial[48],uint8_t **span){
    size_t index;
    if(!frame_active || !initial || !span ||
       sizeof(frame_view)!=48u ||
       offset>=48u || bytes>48u-offset)return 0;
    if(!frame_view_known){
        memcpy(&frame_view,initial,48u);
        memcpy(frame_view_image,initial,48u);
        frame_view_known=1;
    }
    /* Guest writes can arrive after both an early read and a native write.
       An image change takes ownership of that byte until the next typed write. */
    for(index=offset;index<offset+bytes;index++){
        if(initial[index]!=frame_view_image[index]){
            frame_view_image[index]=initial[index];
            frame_view_written[index]=0u;
        }
        if(!frame_view_written[index])
            ((uint8_t *)&frame_view)[index]=initial[index];
    }
    *span=(uint8_t *)&frame_view+offset;
    return 1;
}
int render_frame_view_read_native(size_t offset,void *output,size_t bytes,
    const uint8_t initial[48]){
    uint8_t *span;
    if(!output || !view_span(offset,bytes,initial,&span))return 0;
    memcpy(output,span,bytes);return 1;
}
int render_frame_view_write_native(size_t offset,const void *input,
    size_t bytes,
    const uint8_t initial[48]){
    uint8_t *span;
    if(!input || !view_span(offset,bytes,initial,&span))return 0;
    memcpy(span,input,bytes);
    memset(frame_view_written+offset,1,bytes);
    return 1;
}
int render_frame_view_snapshot(RenderViewport *output,
    const uint8_t initial[48]){
    uint8_t *span;
    if(!output || !view_span(0u,48u,initial,&span))return 0;
    ++view_snapshots;
    if(memcmp(span,initial,48u))++view_image_divergences;
    memcpy(output,span,48u);return 1;
}
void render_frame_view_stats(uint32_t *snapshots,
    uint32_t *image_divergences){
    if(snapshots)*snapshots=view_snapshots;
    if(image_divergences)*image_divergences=view_image_divergences;
}
void render_frame_cell_stats(uint32_t *reads,uint32_t *writes){
    if(reads)*reads=read_count;
    if(writes)*writes=write_count;
}
static uint32_t *frame_cell(unsigned index){
    switch(index){
        case 0u:return &frame_owner.auxiliary[0];
        case 1u:return &frame_owner.auxiliary[1];
        case 2u:return &frame_owner.cursors.packet;
        case 3u:return &frame_owner.auxiliary[2];
        case 4u:return &frame_owner.cursors.scalar;
        case 5u:return &frame_owner.cursors.vertex;
        case 6u:return &frame_owner.cursors.uv;
        case 7u:return &frame_owner.queue_head;
        default:return &frame_owner.auxiliary[3];
    }
}
int render_frame_owner_bind(const uint32_t initial[5]){
    static const unsigned cells[5]={5u,2u,6u,4u,7u};
    unsigned i;
    if(!frame_active || !initial)return 0;
    for(i=0;i<5u;i++)if(!frame_cell_known[cells[i]]){
        *frame_cell(cells[i])=initial[i];
        frame_cell_known[cells[i]]=1u;
    }
    ++binding_count;
    return 1;
}
uint32_t render_frame_owner_binding_count(void){return binding_count;}
void render_frame_owner_note_publication(void){
    if(frame_active)++publication_count;
}
uint32_t render_frame_owner_publication_count(void){
    return publication_count;
}
static uint8_t *frame_cell_span(unsigned index,size_t offset,size_t bytes,
    const void *initial){
    if(!frame_active || !initial || index>=RENDER_FRAME_CELL_COUNT ||
       offset>=sizeof(uint32_t) || bytes>sizeof(uint32_t)-offset)return NULL;
    if(!frame_cell_known[index]){
        memcpy(frame_cell(index),initial,sizeof(uint32_t));
        frame_cell_known[index]=1u;
    }
    return (uint8_t *)frame_cell(index)+offset;
}
int render_frame_cell_read_native(unsigned index,size_t offset,void *output,
    size_t bytes,const void *initial){
    uint8_t *source=frame_cell_span(index,offset,bytes,initial);
    if(!source || !output)return 0;
    memcpy(output,source,bytes);++read_count;return 1;
}
int render_frame_cell_write_native(unsigned index,size_t offset,
    const void *input,size_t bytes,const void *initial){
    uint8_t *destination=frame_cell_span(index,offset,bytes,initial);
    if(!destination || !input)return 0;
    memcpy(destination,input,bytes);++write_count;return 1;
}
