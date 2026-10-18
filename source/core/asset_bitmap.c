#include "asset_bitmap.h"
#include <string.h>
static uint16_t u16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]|(uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int asset_tga8_parse(const ResourceBlob *blob,AssetBitmap *bitmap){
    static const uint8_t signature[]={0,1,1,0,0,0,1,24,0,0,0,0};
    AssetBitmap image;size_t bytes;
    if(!blob || !blob->data || !bitmap || blob->size<786u ||
        memcmp(blob->data,signature,sizeof(signature)) || blob->data[16]!=8u || blob->data[17]!=0u)return 0;
    memset(&image,0,sizeof(image));image.width=u16(blob->data+12u);image.height=u16(blob->data+14u);
    if(!image.width || !image.height)return 0;
    bytes=(size_t)image.width*image.height;if(bytes>blob->size-786u)return 0;
    image.stride=image.width;image.palette_count=256u;image.pixels=blob->data+786u;image.pixel_bytes=bytes;
    *bitmap=image;return 1;
}
static int header(const ResourceBlob *blob,AssetBitmap *bitmap) {
    const uint8_t *p;
    if(!blob || !blob->data || blob->size<54 || !bitmap)return 0;
    p=blob->data;
    if(p[0]!='B' || p[1]!='M' || u32(p+14)!=40)return 0;
    memset(bitmap,0,sizeof(*bitmap));bitmap->width=u32(p+18);bitmap->height=u32(p+22);
    if(!bitmap->width || bitmap->width>INT32_MAX || !bitmap->height || bitmap->height>INT32_MAX)return 0;
    bitmap->palette_bgra=p+54;return 1;
}
int asset_bitmap8_parse(const ResourceBlob *blob,AssetBitmap *bitmap) {
    AssetBitmap parsed;size_t bytes;
    if(!header(blob,&parsed) || !bitmap)return 0;
    if(u16(blob->data+26)!=1 || u16(blob->data+28)!=8 || u32(blob->data+30)!=0)return 0;
    parsed.stride=(parsed.width+3u)&~3u;
    if(parsed.height>SIZE_MAX/parsed.stride)return 0;
    bytes=(size_t)parsed.stride*parsed.height;
    if(blob->size<1078 || bytes>blob->size-1078)return 0;
    parsed.palette_count=256;parsed.pixels=blob->data+1078;parsed.pixel_bytes=bytes;
    *bitmap=parsed;return 1;
}
int asset_bitmap_menu_parse(const ResourceBlob *blob,AssetBitmap *bitmap) {
    AssetBitmap parsed;uint32_t offset,height;size_t bytes;
    if(asset_bitmap8_parse(blob,bitmap))return 1;
    if(!blob || !blob->data || blob->size<54u || !bitmap)return 0;
    if(blob->data[0]!='B' || blob->data[1]!='M' || u32(blob->data+14)!=40u ||
        u16(blob->data+26)!=1u || u16(blob->data+28)!=24u || u32(blob->data+30))return 0;
    memset(&parsed,0,sizeof(parsed));
    parsed.width=u32(blob->data+18);height=u32(blob->data+22);
    if(!parsed.width || parsed.width>(UINT32_MAX-3u)/3u || !height || height==0x80000000u)return 0;
    parsed.top_down=height>INT32_MAX;
    parsed.height=parsed.top_down?0u-height:height;
    parsed.stride=(parsed.width*3u+3u)&~3u;parsed.rgb24=1u;
    offset=u32(blob->data+10);
    if(offset<54u || offset>blob->size || parsed.height>SIZE_MAX/parsed.stride)return 0;
    bytes=(size_t)parsed.height*parsed.stride;
    if(bytes>blob->size-offset)return 0;
    parsed.pixels=blob->data+offset;parsed.pixel_bytes=bytes;
    *bitmap=parsed;return 1;
}
uint32_t asset_bitmap_pixel(const AssetBitmap *bitmap,uint32_t x,uint32_t y) {
    const uint8_t *p;
    size_t offset;unsigned size;
    if(!bitmap || !bitmap->pixels || x>=bitmap->width || y>=bitmap->height)return 0;
    size=bitmap->rgb24?3u:1u;
    offset=(size_t)(bitmap->top_down?y:bitmap->height-1u-y)*bitmap->stride+(size_t)x*size;
    if(offset>bitmap->pixel_bytes || size>bitmap->pixel_bytes-offset)return 0;
    p=bitmap->pixels+offset;
    return bitmap->rgb24?(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16:p[0];
}
int asset_bitmap_blit_rgb565(const AssetBitmap *image,const int32_t source[4],const int32_t destination[4],
    uint8_t *pixels,size_t bytes,uint32_t width,uint32_t height,size_t stride,int transparent,uint32_t key,int mirror){
    int64_t dx,dy,sx,sy,left,top,right,bottom,x,y;uint16_t colors[256];unsigned i;
    if(!image || !source || !destination || !pixels || !image->pixels || !image->width || !image->height ||
       !width || !height || (uint64_t)width*2u>stride || height>SIZE_MAX/stride || (size_t)height*stride>bytes ||
       (uint64_t)image->width*(image->rgb24?3u:1u)>image->stride ||
       image->height>SIZE_MAX/image->stride || (size_t)image->height*image->stride>image->pixel_bytes)return 0;
    dx=(int64_t)destination[2]-destination[0];dy=(int64_t)destination[3]-destination[1];
    sx=(int64_t)source[2]-source[0];sy=(int64_t)source[3]-source[1];
    if(dx<=0 || dy<=0 || sx<=0 || sy<=0)return 1;
    if(source[0]<0 || source[1]<0 || (uint32_t)source[2]>image->width || (uint32_t)source[3]>image->height ||
       (!image->rgb24 && (!image->palette_bgra || !image->palette_count || image->palette_count>256)))return 0;
    for(i=0;i<256;i++){uint32_t c=0;
        if(i<image->palette_count && image->palette_bgra)c=u32(image->palette_bgra+i*4u);
        colors[i]=(uint16_t)(((c>>8)&0xf800u)|((c>>5)&0x7e0u)|((c>>3)&31u));
    }
    left=destination[0]<0?0:destination[0];top=destination[1]<0?0:destination[1];
    right=destination[2]>(int64_t)width?(int64_t)width:destination[2];bottom=destination[3]>(int64_t)height?(int64_t)height:destination[3];
    for(y=top;y<bottom;y++)for(x=left;x<right;x++){
        uint32_t ix=(uint32_t)(source[0]+(x-destination[0])*sx/dx),iy=(uint32_t)(source[1]+(y-destination[1])*sy/dy);
        uint32_t value;uint16_t color;size_t offset;
        if(mirror){ix=(uint32_t)source[2]-1u-(ix-(uint32_t)source[0]);iy=(uint32_t)source[3]-1u-(iy-(uint32_t)source[1]);}
        value=asset_bitmap_pixel(image,ix,iy);
        if(transparent && value==(image->rgb24?key&0xffffffu:(uint8_t)key))continue;
        if(image->rgb24)color=(uint16_t)(((value>>8)&0xf800u)|((value>>5)&0x7e0u)|((value>>3)&31u));
        else{if(value>=image->palette_count)return 0;color=colors[value];}
        offset=(size_t)y*stride+(size_t)x*2u;pixels[offset]=(uint8_t)color;pixels[offset+1]=(uint8_t)(color>>8);
    }
    return 1;
}
int asset_rgb565_blit(const uint8_t *input,size_t input_bytes,uint32_t input_width,uint32_t input_height,size_t input_stride,
    const int32_t source[4],const int32_t destination[4],uint8_t *pixels,size_t bytes,uint32_t width,uint32_t height,size_t stride,
    int transparent,uint16_t key_low,uint16_t key_high){
    int64_t dx,dy,sx,sy,left,top,right,bottom,x,y;
    if(!input || !pixels || !source || !destination || !input_width || !input_height || !width || !height ||
       (uint64_t)width*2>stride || height>SIZE_MAX/stride || (size_t)height*stride>bytes ||
       (uint64_t)input_width*2>input_stride || input_height>SIZE_MAX/input_stride || (size_t)input_height*input_stride>input_bytes)return 0;
    dx=(int64_t)destination[2]-destination[0];dy=(int64_t)destination[3]-destination[1];
    sx=(int64_t)source[2]-source[0];sy=(int64_t)source[3]-source[1];
    if(dx<=0 || dy<=0 || sx<=0 || sy<=0)return 1;
    if(source[0]<0 || source[1]<0 || (uint32_t)source[2]>input_width || (uint32_t)source[3]>input_height)return 0;
    left=destination[0]<0?0:destination[0];top=destination[1]<0?0:destination[1];
    right=destination[2]>(int64_t)width?(int64_t)width:destination[2];bottom=destination[3]>(int64_t)height?(int64_t)height:destination[3];
    for(y=top;y<bottom;y++)for(x=left;x<right;x++){
        size_t ix=(size_t)(source[0]+(x-destination[0])*sx/dx),iy=(size_t)(source[1]+(y-destination[1])*sy/dy);
        uint16_t color=u16(input+iy*input_stride+ix*2);size_t offset;
        if(transparent && color>=key_low && color<=key_high)continue;
        offset=(size_t)y*stride+(size_t)x*2;pixels[offset]=(uint8_t)color;pixels[offset+1]=(uint8_t)(color>>8);
    }
    return 1;
}
int asset_bitmap_sprite_parse(const ResourceBlob *blob,AssetBitmap *bitmap) {
    AssetBitmap parsed;uint32_t offset;size_t bytes;
    if(!header(blob,&parsed) || !bitmap || parsed.width>65535u || parsed.height>65535u)return 0;
    offset=u32(blob->data+10);parsed.stride=(parsed.width+3u)&~3u;
    if(offset>blob->size || parsed.height>SIZE_MAX/parsed.stride)return 0;
    /*The height-one branch publishes dimensions but never reads source pixels. */
    bytes=parsed.height>1u?(size_t)(parsed.height-1u)*parsed.stride+parsed.width:0u;
    if(bytes>blob->size-offset)return 0;
    parsed.pixels=blob->data+offset;parsed.pixel_bytes=bytes;
    *bitmap=parsed;return 1;
}
int asset_bitmap_startup_palette(const ResourceBlob *blob,AssetBitmap *bitmap,
    uint8_t *palette,size_t capacity,size_t *written) {
    AssetBitmap parsed;uint32_t colors,bits,index;size_t bytes;
    if(!header(blob,&parsed) || !bitmap || !palette || !written)return 0;
    bits=u16(blob->data+28);colors=bits<=8?u32(blob->data+46):0;
    if(bits<=8 && !colors)colors=1u<<bits;
    if(colors>256 || (size_t)colors*4>blob->size-54)return 0;
    bytes=4+(size_t)colors*4;if(capacity<bytes)return 0;
    palette[0]=0;palette[1]=3;palette[2]=(uint8_t)colors;palette[3]=(uint8_t)(colors>>8);
    for(index=0;index<colors;index++) {
        const uint8_t *source=parsed.palette_bgra+index*4;
        uint8_t *target=palette+4+index*4;
        target[0]=source[2];target[1]=source[1];target[2]=source[0];target[3]=source[3];
    }
    parsed.palette_count=colors;*bitmap=parsed;*written=bytes;return 1;
}
