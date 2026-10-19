#include "portable_font.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "portable_font_metrics.inc"
typedef struct MenuGlyph {uint8_t *pixels;int width,height,left,top,ready;} MenuGlyph;
typedef struct MenuFont {FT_Face face;MenuGlyph glyphs[256];int bold;} MenuFont;
struct GameMenuFonts {FT_Library library;MenuFont fonts[7];};

GameMenuFonts *game_menu_fonts_open(void){
    static const char *paths[7]={"FONTS/LiberationSans-Regular.ttf","FONTS/LiberationSans-Regular.ttf",
        "FONTS/Neuropol.ttf","FONTS/Neuropol.ttf","FONTS/Neuropol.ttf","FONTS/LAUCLP__.TTF","FONTS/LAUCLP__.TTF"};
    static const unsigned sizes[7]={9,10,9,12,14,15,50};
    GameMenuFonts *fonts=calloc(1,sizeof(*fonts));unsigned i;
    if(!fonts)return NULL;
    if(FT_Init_FreeType(&fonts->library))goto fail;
    for(i=0;i<7;++i){
        if(FT_New_Face(fonts->library,paths[i],0,&fonts->fonts[i].face) ||
           FT_Set_Pixel_Sizes(fonts->fonts[i].face,0,sizes[i]))goto fail;
        fonts->fonts[i].bold=i>=4;
    }
    return fonts;
fail:game_menu_fonts_close(fonts);return NULL;
}
void game_menu_fonts_close(GameMenuFonts *fonts){
    unsigned i,j;if(!fonts)return;
    for(i=0;i<7;++i){
        for(j=0;j<256;++j)free(fonts->fonts[i].glyphs[j].pixels);
        if(fonts->fonts[i].face)FT_Done_Face(fonts->fonts[i].face);
    }
    if(fonts->library)FT_Done_FreeType(fonts->library);
    free(fonts);
}
static FT_ULong character(unsigned code){
    static const uint16_t cp1252[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,
        0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,0x90,0x2018,0x2019,0x201c,
        0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
    return code>=128 && code<160?cp1252[code-128]:code;
}
static MenuGlyph *glyph(MenuFont *font,unsigned code){
    MenuGlyph *g=font->glyphs+code;FT_GlyphSlot slot;int x,y,pitch;
    if(g->ready)return g;
    if(FT_Load_Char(font->face,character(code),FT_LOAD_TARGET_MONO))return NULL;
    slot=font->face->glyph;
    if(font->bold)FT_GlyphSlot_Embolden(slot);
    if(FT_Render_Glyph(slot,FT_RENDER_MODE_MONO))return NULL;
    if(slot->bitmap.width>256 || slot->bitmap.rows>256)return NULL;
    g->width=(int)slot->bitmap.width;g->height=(int)slot->bitmap.rows;
    g->left=slot->bitmap_left;g->top=slot->bitmap_top;pitch=slot->bitmap.pitch;
    if(g->width && g->height){
        if(!slot->bitmap.buffer || pitch==INT_MIN || slot->bitmap.pixel_mode!=FT_PIXEL_MODE_MONO || abs(pitch)<(g->width+7)/8)return NULL;
        g->pixels=malloc((size_t)g->width*g->height);if(!g->pixels)return NULL;
        for(y=0;y<g->height;++y){
            const uint8_t *row=slot->bitmap.buffer+(ptrdiff_t)(pitch<0?g->height-1-y:y)*abs(pitch);
            for(x=0;x<g->width;++x)g->pixels[(size_t)y*g->width+x]=(row[x/8]>>(7-(x&7)))&1;
        }
    }
    g->ready=1;return g;
}
int game_menu_font_measure(GameMenuFonts *fonts,unsigned font,const char *text,int32_t *width,int32_t *height){
    size_t i,length;int64_t total=0;
    if(!fonts || font>=7 || !text || !width || !height)return 0;
    length=strlen(text);if(length>65535)return 0;
    for(i=0;i<length;++i)total+=font_advances[font][(uint8_t)text[i]];
    if(total>INT32_MAX)return 0;
    *width=(int32_t)total;*height=font_line_metrics[font][0];return 1;
}
static uint16_t *text_codes(const char *text,uint32_t flags,size_t *count){
    size_t i,n=0,length;uint16_t *codes;int underline=0;
    if(!text || (length=strlen(text))>65535)return NULL;
    codes=malloc((length+1)*sizeof(*codes));if(!codes)return NULL;
    for(i=0;i<length;++i){
        unsigned ch=(uint8_t)text[i];
        if(ch=='&' && !(flags&0x800u) && i+1<length){
            if(text[i+1]=='&'){++i;ch='&';}else{underline=1;continue;}
        }
        if(ch=='\r'){if(text[i+1]=='\n')++i;ch='\n';}
        if(ch=='\n' && (flags&0x20u))ch=' ';
        codes[n++]=(uint16_t)(ch|((underline && !(flags&0x100000u))?256u:0u));underline=0;
    }
    *count=n;return codes;
}
static int advance(unsigned font,unsigned code,int position,int extra){
    if(code=='\t'){
        int stop=font_advances[font][' ']*8;if(stop<=0)stop=8;
        return stop-position%stop;
    }
    return font_advances[font][code]+extra;
}
static void line(const uint16_t *codes,size_t count,size_t start,unsigned font,int limit,int extra,
        uint32_t flags,size_t *end,size_t *next,int *width){
    size_t i=start,space=SIZE_MAX;int position=0,space_width=0;
    for(;i<count && (codes[i]&255u)!='\n';++i){
        unsigned ch=codes[i]&255u;
        if(ch==' ' || ch=='\t'){space=i;space_width=position;}
        position+=advance(font,ch,position,extra);
        if((flags&0x10u) && !(flags&0x20u) && position>limit && space!=SIZE_MAX){
            *end=space;*width=space_width;i=space;
            while(i<count && ((codes[i]&255u)==' ' || (codes[i]&255u)=='\t'))++i;
            *next=i;return;
        }
    }
    *end=i;*next=i<count?i+1:i;*width=position;
}
static int layout(GameMenuFonts *fonts,unsigned font,const char *text,uint8_t *pixels,size_t bytes,
        int32_t width,int32_t height,int32_t stride,int32_t left,int32_t top,int32_t right,
        uint8_t color,uint32_t flags,int extra,uint32_t *measured){
    uint16_t *codes;size_t count,start=0,end,next,i;int rows=0,line_width,x,y,baseline;
    if(!fonts || font>=7 || right<=left || left< -1048576 || top< -1048576 ||
       (int64_t)right-left>INT_MAX || (pixels && (width<=0 || height<=0 || width>8192 || height>8192 ||
       left>width+1048576 || top>height+1048576 || stride<width ||
       (uint64_t)(uint32_t)stride*(uint32_t)height>bytes)))return 0;
    codes=text_codes(text,flags,&count);if(!codes)return 0;
    do{
        line(codes,count,start,font,right-left,extra,flags,&end,&next,&line_width);
        x=left;if(flags&1u)x+=(right-left-line_width)/2;else if(flags&2u)x+=right-left-line_width;
        y=top+rows*font_line_metrics[font][0];
        if((flags&0x20u) && (flags&4u))y+=(height-top-font_line_metrics[font][0])/2;
        else if((flags&0x20u) && (flags&8u))y=height-font_line_metrics[font][0];
        baseline=y+font_line_metrics[font][1];
        if(pixels)for(i=start;i<end;++i){
            unsigned code=codes[i]&255u;int dx,dy,gx,gy;MenuGlyph *g;
            if(code!='\t' && code!=' '){
                g=glyph(fonts->fonts+font,code);if(!g){free(codes);return 0;}
                if(!(flags&0x200000u))for(gy=0;gy<g->height;++gy){
                    dy=baseline-g->top+gy;if(dy<top || dy<0 || dy>=height)continue;
                    for(gx=0;gx<g->width;++gx){
                        dx=x+g->left+gx;if(dx<left || dx<0 || dx>=right || dx>=width)continue;
                        if(g->pixels[(size_t)gy*g->width+gx])pixels[(size_t)dy*stride+dx]=color;
                    }
                }
            }
            dx=advance(font,code,x-left,extra);
            if((codes[i]&256u) && baseline+1>=top && baseline+1>=0 && baseline+1<height)
                for(gx=0;gx<dx;++gx)if(x+gx>=left && x+gx>=0 && x+gx<right && x+gx<width)
                    pixels[(size_t)(baseline+1)*stride+x+gx]=color;
            x+=dx;
        }
        ++rows;if(next<=start)break;start=next;
    }while(start<count);
    if(measured)*measured=(uint32_t)rows*font_line_metrics[font][0];
    free(codes);return 1;
}
int game_menu_font_wrapped_height(GameMenuFonts *fonts,unsigned font,const char *text,int32_t width,uint32_t *height){
    if(!height || width<=1)return 0;
    return layout(fonts,font,text,NULL,0,0,0,0,0,0,width-1,0,0x50u,0,height);
}
int game_menu_font_paint(GameMenuFonts *fonts,unsigned font,const char *text,uint8_t *pixels,size_t bytes,
        int32_t width,int32_t height,int32_t stride,int32_t x,int32_t y,int32_t right,uint8_t color,uint32_t flags,int extra){
    return pixels && layout(fonts,font,text,pixels,bytes,width,height,stride,x,y,right,color,flags|0x50u,extra,NULL);
}
