#ifndef UPRISING_ASSET_TEXTURE_H
#define UPRISING_ASSET_TEXTURE_H
#include "resource_blob.h"
typedef struct AssetTexture {
    uint32_t width,height,render_width,render_height;
    uint8_t lod,aspect,format;
    uint8_t *pixels;
    size_t size;
    /* Software bitmap rows retain retail padding; zero means packed rows. */
    uint32_t row_stride;
} AssetTexture;
/* Crops the retail BMP rows to power-of-two dimensions, capped at256. Row
 * stepping follows the texture loader's source width, not the BMP8 UI stride.
 * alpha_mode: -1 indexed,0 palette luminance,1 neighborhood coverage,2 inverse luminance. */
int asset_texture_bitmap(const ResourceBlob *blob,int alpha_mode,
    const uint8_t palette_rgba[1024],AssetTexture *output);
/* Copy caller pixels; later caller mutations require explicit synchronization. */
int asset_texture_indexed(const uint8_t *pixels,size_t size,uint32_t width,uint32_t height,
    int alpha_mode,const uint8_t palette_rgba[1024],AssetTexture *output);
void asset_texture_release(AssetTexture *texture);
/*516650/518040: unflipped, padded 8-bit BMP rows, without hardware cropping. */
int asset_texture_software_bitmap(const ResourceBlob *,AssetTexture *);

/* Retail 256x256 BGR payload, beginning after the fixed 18-byte file header. */
int asset_texture_rgb565(const ResourceBlob *,AssetTexture *);
/* Decode and mirror one32x32 terrain tile from the packed bank. */
int asset_texture_terrain_tile(const ResourceBlob *,size_t index,AssetTexture *);
/* Expand the112-byte NCC device table's packed register values into RGB.
 * Explicit little-endian words and signed9-bit chroma; no host struct casts. */
int asset_texture_ncc_palette(const uint8_t table[112],uint32_t rgb[256]);
#endif
