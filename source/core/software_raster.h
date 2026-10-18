#ifndef UPRISING_SOFTWARE_RASTER_H
#define UPRISING_SOFTWARE_RASTER_H
#include "render_arena.h"
typedef struct SoftwareRasterFrame {
    uint8_t *color,*depth;
    size_t color_bytes,depth_bytes;
    int32_t width,height,color_stride,depth_stride;
} SoftwareRasterFrame;
typedef struct SoftwareRasterTexture {
    const uint8_t *pixels;size_t bytes;int32_t width,height;
} SoftwareRasterTexture;
/*515950/515e20: exclusive clip bounds and the original inclusive, delayed
 * error-accumulator line walk. Coordinates are ordinary values. */
int software_raster_clip_line(int32_t coordinates[4],const int32_t bounds[4]);
int software_raster_line(const SoftwareRasterFrame *,const int32_t coordinates[4],uint8_t color);
/*Ordinary scan conversion for the original indexed software packet family.
 * Borrowed frame/vertex/texture storage stays with its existing owners.
 * 1 drawn/culled,0 invalid storage,-1 unsupported mode before any drawing. */
int software_raster_polygon(const SoftwareRasterFrame *,uint32_t flags,uint8_t color,
    const RenderArenaVertex *const *,const RenderArenaUv *,uint32_t count,
    const SoftwareRasterTexture *);
#endif
