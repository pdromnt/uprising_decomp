#ifndef UPRISING_RENDER_FRAME_OWNER_H
#define UPRISING_RENDER_FRAME_OWNER_H

#include "gameplay_clipper.h"
#include <stddef.h>
#include <stdint.h>

enum {RENDER_FRAME_CELL_COUNT=9u,RENDER_WORLD_CELL_COUNT=2u};

typedef struct RenderViewport {
    float center_x,center_y,top,left,bottom,right;
    float y_min,x_min,y_max,x_max,vertical,scale;
} RenderViewport;

typedef struct RenderFrameOwner {
    GameplayClipCursors cursors;
    uint32_t queue_head;
    uint32_t auxiliary[4];
} RenderFrameOwner;

typedef struct RenderWorldInputs {
    float projection_magic,depth_scale,fog_max,near_offset,fog_scale;
    int32_t depth_limit;
    uint32_t texture_frame;
    uint8_t base_palette_entries[4];
} RenderWorldInputs;

RenderFrameOwner *render_frame_owner(void);
/* Bind native cursor/head state from the x86 image only on first use. Later
 * frame bindings keep the authoritative values written by native renderers. */
int render_frame_owner_bind(const uint32_t initial[5]);
uint32_t render_frame_owner_binding_count(void);
void render_frame_owner_note_publication(void);
uint32_t render_frame_owner_publication_count(void);
void render_frame_cells_reset(void);
void render_frame_cells_release(void);
int render_frame_cell_read_native(unsigned cell,size_t offset,void *output,
    size_t bytes,const void *initial);
int render_frame_cell_write_native(unsigned cell,size_t offset,
    const void *input,size_t bytes,const void *initial);
void render_frame_cell_stats(uint32_t *reads,uint32_t *writes);
int render_frame_palette_read_native(size_t offset,void *output,size_t bytes,
    const uint8_t initial[1024]);
int render_frame_palette_write_native(size_t offset,const void *input,
    size_t bytes,const uint8_t initial[1024]);
int render_frame_palette_snapshot(uint32_t output[256],
    const uint8_t initial[1024]);
void render_frame_palette_stats(uint32_t *snapshots,
    uint32_t *image_divergences);
int render_frame_counter_read_native(size_t offset,void *output,size_t bytes,
    const uint8_t initial[12]);
int render_frame_counter_write_native(size_t offset,const void *input,
    size_t bytes,const uint8_t initial[12]);
int render_frame_counter_snapshot(uint32_t output[3],
    const uint8_t initial[12]);
int render_frame_view_read_native(size_t offset,void *output,size_t bytes,
    const uint8_t initial[48]);
int render_frame_view_write_native(size_t offset,const void *input,
    size_t bytes,const uint8_t initial[48]);
int render_frame_view_snapshot(RenderViewport *output,
    const uint8_t initial[48]);
void render_frame_view_stats(uint32_t *snapshots,
    uint32_t *image_divergences);
int render_world_inputs_ready(void);
int render_world_inputs_initialize(const uint32_t initial[8]);
int render_world_inputs_snapshot(RenderWorldInputs *output);
uint32_t render_world_inputs_snapshot_count(void);
int render_world_cell_read_native(unsigned cell,size_t offset,void *output,
    size_t bytes,const uint8_t initial[4]);
int render_world_cell_write_native(unsigned cell,size_t offset,
    const void *input,size_t bytes,const uint8_t initial[4]);
#endif
