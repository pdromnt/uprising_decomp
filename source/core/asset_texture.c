#include "asset_texture.h"
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static unsigned exponent(uint32_t value) { unsigned n=0;while(value>1 && n<8){value>>=1;++n;}return n; }
void asset_texture_release(AssetTexture *texture) { if(texture){free(texture->pixels);memset(texture,0,sizeof(*texture));} }
int asset_texture_software_bitmap(const ResourceBlob *blob,AssetTexture *output) {
    AssetTexture texture={0};uint32_t width,height,colors,stride;size_t offset,bytes;
    if(!blob || !blob->data || blob->size<54 || !output || output->pixels)return 0;
    if(blob->data[0]!='B' || blob->data[1]!='M' || u32(blob->data+14)!=40 ||
       blob->data[26]!=1 || blob->data[27] || blob->data[28]!=8 ||
       blob->data[29] || u32(blob->data+30))return 0;
    width=u32(blob->data+18);height=u32(blob->data+22);colors=u32(blob->data+46);
    if(!width || width>INT32_MAX || !height || height>INT32_MAX)return 0;
    if(!colors)colors=256;
    if(colors>256)return 0;
    stride=(width+3u)&~3u;offset=54u+(size_t)colors*4u;
    if((uint64_t)stride*height>SIZE_MAX || offset>blob->size)return 0;
    bytes=(size_t)stride*height;
    if(bytes>blob->size-offset)return 0;
    texture.pixels=malloc(bytes);if(!texture.pixels)return 0;
    /*The original ignores bfOffBits and normalizes biSizeImage to stride*height. */
    memcpy(texture.pixels,blob->data+offset,bytes);
    texture.width=texture.render_width=width;texture.height=texture.render_height=height;
    texture.row_stride=stride;texture.format=5;texture.size=bytes;
    *output=texture;return 1;
}
static int expand_alpha(AssetTexture *texture,int alpha_mode,const uint8_t palette[1024]) {
    if(alpha_mode>=0) {
        uint8_t *expanded;size_t i, count=alpha_mode==1?(size_t)texture->render_width*texture->render_height:texture->size;
        if(alpha_mode!=1 && !palette){return 0;}
        expanded=calloc(texture->size,2);if(!expanded){return 0;}
        for(i=0;i<count;i++) {
            uint32_t x=(uint32_t)(i%texture->render_width),y=(uint32_t)(i/texture->render_width);
            unsigned sample=texture->pixels[i];float alpha;
            if(sample==254)continue;
            expanded[i*2]=(uint8_t)sample;
            if(alpha_mode!=1) {
                unsigned r=palette[sample*4],g=palette[sample*4+1],b=palette[sample*4+2];
                alpha=(float)((long double)(r*r+g*g+b*b)*(float)0.0019607844296842813);
                if(alpha_mode==2){
                    alpha=(float)(200.0L-(long double)(r*r+g*g+b*b)*(float)0.0019607844296842813);
                    if(alpha<0)alpha=0;
                    expanded[i*2]=0;
                } else if(alpha>255)alpha=255;
                expanded[i*2+1]=(uint8_t)alpha;
            } else {
                int neighbors=8,dx,dy;
                for(dy=-1;dy<=1;dy++) {
                    int row=(int)y+dy;
                    if(row<0 || row>=(int)texture->render_height){--neighbors;continue;}
                    for(dx=-1;dx<=1;dx++) {
                        int column=(int)x+dx;
                        if(column<0 || column>=(int)texture->render_width || texture->pixels[(size_t)row*texture->render_width+(unsigned)column]==254)--neighbors;
                    }
                }
                alpha=(float)((long double)neighbors*(float)0.11249999701976776+(float)0.10000000149011612);
                if(alpha>1)alpha=1;
                expanded[i*2+1]=(uint8_t)((long double)alpha*255);
            }
        }
        free(texture->pixels);texture->pixels=expanded;texture->size=texture->size*2;texture->format=14;
    }
    return 1;
}
int asset_texture_bitmap(const ResourceBlob *blob,int alpha_mode,const uint8_t palette[1024],AssetTexture *output) {
    AssetTexture texture={0};uint32_t width,height,colors,y;unsigned wx,hy;int difference;
    size_t offset,base_size;uint8_t *indexed;
    if(!blob || !output || output->pixels || blob->size<54 || alpha_mode < -1 || alpha_mode>2)return 0;
    if(u32(blob->data+14)!=40 || blob->data[28]!=8 || blob->data[29] || u32(blob->data+30))return 0;
    width=u32(blob->data+18);height=u32(blob->data+22);
    if(height>INT32_MAX)height=0u-height;
    if(!width || width>INT32_MAX || !height || height>INT32_MAX)return 0;
    colors=u32(blob->data+46);if(!colors)colors=256;
    if(colors>256)return 0;
    offset=54+(size_t)colors*4;wx=exponent(width);hy=exponent(height);
    texture.width=1u<<wx;texture.height=1u<<hy;
    if(offset>blob->size || (uint64_t)(texture.height-1)*width+texture.width>blob->size-offset)return 0;
    difference=(int)wx-(int)hy;if(difference < -3)difference=-3;if(difference>3)difference=3;
    texture.lod=(uint8_t)(8-(wx>hy?wx:hy));texture.aspect=(uint8_t)(3-difference);
    texture.render_width=texture.render_height=256u>>texture.lod;
    if(texture.aspect<3)texture.render_height>>=3-texture.aspect;
    if(texture.aspect>3)texture.render_width>>=texture.aspect-3;
    if(!texture.render_width)texture.render_width=1;
    if(!texture.render_height)texture.render_height=1;
    base_size=(size_t)texture.render_width*texture.render_height;
    if(base_size<(size_t)texture.width*texture.height)base_size=(size_t)texture.width*texture.height;
    indexed=calloc(base_size,1);if(!indexed)return 0;
    for(y=0;y<texture.height;y++)memcpy(indexed+(size_t)y*texture.width,blob->data+offset+(size_t)y*width,texture.width);
    texture.format=5;texture.pixels=indexed;texture.size=base_size;
    if(!expand_alpha(&texture,alpha_mode,palette)){asset_texture_release(&texture);return 0;}
    *output=texture;return 1;
}

int asset_texture_indexed(const uint8_t *pixels,size_t size,uint32_t width,uint32_t height,
    int alpha_mode,const uint8_t palette[1024],AssetTexture *output) {
    AssetTexture texture={0};unsigned wx,hy;int difference;size_t count,render;
    if(!pixels || !output || output->pixels || !width || !height || width>INT32_MAX || height>INT32_MAX || alpha_mode < -1 || alpha_mode>2)return 0;
    if((uint64_t)width*height>SIZE_MAX || (uint64_t)width*height>size)return 0;
    count=(size_t)width*height;wx=exponent(width);hy=exponent(height);
    texture.width=1u<<wx;texture.height=1u<<hy;
    difference=(int)wx-(int)hy;if(difference < -3)difference=-3;if(difference>3)difference=3;
    texture.lod=(uint8_t)(8-(wx>hy?wx:hy));texture.aspect=(uint8_t)(3-difference);
    texture.render_width=texture.render_height=256u>>texture.lod;
    if(texture.aspect<3)texture.render_height>>=3-texture.aspect;
    if(texture.aspect>3)texture.render_width>>=texture.aspect-3;
    if(!texture.render_width)texture.render_width=1;
    if(!texture.render_height)texture.render_height=1;
    render=(size_t)texture.render_width*texture.render_height;
    texture.size=count>render?count:render;
    if(texture.size>SIZE_MAX/2)return 0;
    texture.pixels=calloc(texture.size,1);if(!texture.pixels)return 0;
    memcpy(texture.pixels,pixels,count);texture.format=5;
    if(!expand_alpha(&texture,alpha_mode,palette)){asset_texture_release(&texture);return 0;}
    *output=texture;return 1;
}

int asset_texture_rgb565(const ResourceBlob *blob,AssetTexture *output) {
    AssetTexture image={0};size_t i;
    if(!blob || !blob->data || blob->size<18u+256u*256u*3u || !output)return 0;
    image.pixels=malloc(256u*256u*2u);if(!image.pixels)return 0;
    image.width=image.height=image.render_width=image.render_height=256;
    image.aspect=3;image.format=10;image.size=256u*256u*2u;
    for(i=0;i<256u*256u;i++) {
        const uint8_t *p=blob->data+18+i*3;
        uint16_t value=(uint16_t)((p[0]>>3)|((p[1]&252u)<<3)|((p[2]&248u)<<8));
        image.pixels[i*2]=(uint8_t)value;image.pixels[i*2+1]=(uint8_t)(value>>8);
    }
    *output=image;return 1;
}

int asset_texture_terrain_tile(const ResourceBlob *blob,size_t index,AssetTexture *output) {
    uint8_t pixels[1024];unsigned x,y;const uint8_t *source;
    if(!blob || !blob->data || blob->size<4 || blob->data[0]!=32 || blob->data[1]!=0 || blob->data[2]!=32 || blob->data[3]!=0)return 0;
    if(index>=(blob->size-4)/1024)return 0;
    source=blob->data+4+index*1024;
    for(y=0;y<32;y++)for(x=0;x<32;x++)pixels[y*32+x]=source[y*32+31-x];
    return asset_texture_indexed(pixels,sizeof(pixels),32,32,-1,NULL,output);
}

int asset_texture_ncc_palette(const uint8_t table[112],uint32_t rgb[256]){
    unsigned texel,channel;
    if(!table || !rgb)return 0;
    /* GuNccTable:16 Y bytes,24 I bytes,24 Q bytes, then twelve device words.
     * The device words are authoritative, including when unpacked fields differ.
     * Y occupies four byte-packed words; I/Q each have four RGB9 words. */
    for(texel=0;texel<256;texel++){
        unsigned y=texel>>4,i=(texel>>2)&3,q=texel&3;
        uint32_t yi=u32(table+64+(y/4)*4),ii=u32(table+80+i*4),qi=u32(table+96+q*4),color=0;
        int luminance=(int)((yi>>((y&3)*8))&255);
        for(channel=0;channel<3;channel++){
            unsigned shift=(2-channel)*9;
            int a=(int)((ii>>shift)&511),b=(int)((qi>>shift)&511),value;
            if(a&256)a-=512;
            if(b&256)b-=512;
            value=luminance+a+b;
            if(value<0)value=0;
            if(value>255)value=255;
            color=(color<<8)|(unsigned)value;
        }
        rgb[texel]=color;
    }
    return 1;
}
