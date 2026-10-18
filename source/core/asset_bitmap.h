#ifndef UPRISING_ASSET_BITMAP_H
#define UPRISING_ASSET_BITMAP_H
#include "resource_blob.h"
typedef struct AssetBitmap {
    uint32_t width,height,stride,palette_count;
    const uint8_t *palette_bgra;
    const uint8_t *pixels;
    size_t pixel_bytes;
    int top_down; /*Native decoded frames; retail bitmap views default to bottom-up. */
    unsigned rgb24; /* BGR triples borrowed from an uncompressed true-color DIB. */
} AssetBitmap;
/* Borrowed views into an owned ResourceBlob. No native-struct deserialization.
 * Retail bitmap8 uses the 256-entry palette directly following the 40-byte DIB
 * and pixels immediately after it (not bfOffBits). Positive-height BI_RGB only. */
int asset_bitmap8_parse(const ResourceBlob *blob, AssetBitmap *bitmap);
/* Menu surfaces also accept 24-bit BI_RGB. Keep the original immutable blob;
 * select colors against the destination palette when drawing. */
int asset_bitmap_menu_parse(const ResourceBlob *blob, AssetBitmap *bitmap);
uint32_t asset_bitmap_pixel(const AssetBitmap *bitmap, uint32_t x, uint32_t y);
/*Draw a borrowed menu bitmap into a bounded, possibly unaligned RGB565 lock.
 *Rectangles are left/top/right/bottom, with exclusive right/bottom edges. */
int asset_bitmap_blit_rgb565(const AssetBitmap *,const int32_t source[4],const int32_t destination[4],
    uint8_t *pixels,size_t bytes,uint32_t width,uint32_t height,size_t stride,int transparent,uint32_t key,int mirror);
int asset_rgb565_blit(const uint8_t *source_pixels,size_t source_bytes,uint32_t source_width,uint32_t source_height,size_t source_stride,
    const int32_t source[4],const int32_t destination[4],uint8_t *pixels,size_t bytes,uint32_t width,uint32_t height,size_t stride,
    int transparent,uint16_t key_low,uint16_t key_high);
/*413190 deliberately copies width bytes per padded row even for a 24-bit
 *header. This sprite codec is distinct from DIB color conversion. */
int asset_bitmap_sprite_parse(const ResourceBlob *blob, AssetBitmap *bitmap);
/* 516a20: exact supported uncompressed indexed TGA header. Palette is skipped;
 * pixels remain borrowed from the blob in their original row order. */
int asset_tga8_parse(const ResourceBlob *blob,AssetBitmap *bitmap);
/* Startup uses only DIB dimensions and the palette; pixel payload is not needed.
 * Encodes LOGPALETTE-compatible RGB+reserved bytes for the platform boundary. */
int asset_bitmap_startup_palette(const ResourceBlob *blob, AssetBitmap *bitmap,
    uint8_t *palette, size_t capacity, size_t *written);
#endif
