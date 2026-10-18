#ifndef RECOMP_RENDERER_HOST_H
#define RECOMP_RENDERER_HOST_H

#include <stdint.h>
#include <stddef.h>

/* Host-native renderer API. Guest faults are checked by the ABI adapters;
   renderer implementation and callers receive only a boolean readiness gate. */
typedef struct RuntimeRenderVertex {
    float x, y, z;
    float r, g, b;
    float ooz;
    float a;
    float oow;
    float sow0, tow0, oow0;
    float sow1, tow1, oow1;
} RuntimeRenderVertex;

typedef struct RuntimeTextureInfo {
    int32_t small_lod;
    int32_t large_lod;
    int32_t aspect_ratio;
    uint32_t format;
    uint32_t data;
} RuntimeTextureInfo;

typedef struct RuntimeSurfaceLock {
    int32_t size;
    uint32_t pixels;
    uint32_t stride;
    int32_t write_mode;
    int32_t origin;
} RuntimeSurfaceLock;

int runtime_renderer_host_draw_triangle(int ready, const RuntimeRenderVertex *a,
    const RuntimeRenderVertex *b, const RuntimeRenderVertex *c);
int runtime_renderer_host_draw_point(int ready, const RuntimeRenderVertex *vertex);
int runtime_renderer_host_draw_line(int ready, const RuntimeRenderVertex *a,
    const RuntimeRenderVertex *b);
int runtime_renderer_host_draw_polygon(int ready, RuntimeRenderVertex *vertices,
    unsigned count);
int runtime_renderer_host_world_begin(int ready);
int runtime_renderer_host_ui_image_begin(int ready);
int runtime_renderer_host_screen_point_begin(int ready);
int runtime_renderer_host_screen_line_begin(int ready);
int runtime_renderer_host_screen_quad_begin(int ready);
int runtime_renderer_host_colored_polygon_begin(int ready);
int runtime_renderer_host_set_colored_polygon_translucency(int ready, int enabled);
int runtime_renderer_host_upload_texture(int ready, uint32_t address,
    const RuntimeTextureInfo *info);
int runtime_renderer_host_textured_polygon_begin(int ready, uint32_t address,
    const RuntimeTextureInfo *info);
int runtime_renderer_host_fullscreen_tiles_begin(int ready, uint32_t address,
    const RuntimeTextureInfo *info);
int runtime_renderer_host_screen_mask_begin(int ready);
int runtime_renderer_host_clear_color_depth(int ready, uint32_t color,
    uint32_t depth);
int runtime_renderer_host_present(int ready, uint32_t interval);
int runtime_renderer_host_bind_texture(int ready, uint32_t address,
    const RuntimeTextureInfo *info);
int runtime_renderer_host_update_palette(int ready, const uint32_t *palette);
int runtime_renderer_host_update_ncc(int ready,const uint8_t table[112]);
int runtime_renderer_host_update_palette_range(int ready, const uint32_t *palette,
    unsigned first, unsigned last);
int runtime_renderer_host_set_alpha_fade(int ready, int enabled);
int runtime_renderer_host_set_depth_write(int ready, int enabled);
int runtime_renderer_host_set_depth_bias(int ready, int32_t bias);
int runtime_renderer_host_set_alpha_test(int ready, uint32_t function);
int runtime_renderer_host_set_texture_filter(int ready, int linear);
int runtime_renderer_host_text_surface_begin(int ready);
int runtime_renderer_host_lock_text_surface(int ready, RuntimeSurfaceLock *lock);
int runtime_renderer_host_unlock_text_surface(int ready);
int runtime_renderer_host_text_surface_end(int ready);
int runtime_renderer_initialize_device(void);
int runtime_renderer_open_default(void);
int runtime_renderer_close(void);
int runtime_renderer_shutdown(void);

/* kind:0 plain,1 textured,2 alpha projected polygons. */
int runtime_renderer_host_projected_begin(int ready,int kind);
int runtime_renderer_host_projected_end(int ready,int kind);
int runtime_renderer_host_upload_pixels(int ready,uint32_t address,
    const RuntimeTextureInfo *,const void *pixels,size_t size);
int runtime_renderer_host_surface_begin(int ready);
int runtime_renderer_host_surface_clamp(int ready,int enabled);
#endif
