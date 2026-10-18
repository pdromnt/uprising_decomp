/* Native fixed-function OpenGL device. Included after PortablePlatform so the
 * existing platform owner owns its context, upload snapshots and readback.
 * The preserved renderer supplies depth, combine, chroma and UV semantics;
 * this device does not link that runtime or load a graphics wrapper DLL. */
#include <GL/gl.h>
#include <GL/glext.h>
#include <math.h>

typedef struct Gl1Texture {
    uint32_t address,bytes,format,palette_generation,chroma;
    uint32_t ncc_generation,ncc_table;
    int dirty,chroma_enabled;
    int first_lod,last_lod,aspect;
    unsigned width,height;
    unsigned gpu_width,gpu_height,gpu_levels;
    GLuint name;
    uint8_t *pixels;
} Gl1Texture;
typedef struct Gl1Device {
    GameApplication *application;
    HWND window;HDC dc;HGLRC context;DWORD thread;
    PFNGLGENFRAMEBUFFERSPROC gen_framebuffers;
    PFNGLBINDFRAMEBUFFERPROC bind_framebuffer;
    PFNGLFRAMEBUFFERTEXTURE2DPROC framebuffer_texture;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC check_framebuffer;
    PFNGLDELETEFRAMEBUFFERSPROC delete_framebuffers;
    PFNGLGENRENDERBUFFERSPROC gen_renderbuffers;
    PFNGLBINDRENDERBUFFERPROC bind_renderbuffer;
    PFNGLRENDERBUFFERSTORAGEPROC renderbuffer_storage;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC framebuffer_renderbuffer;
    PFNGLDELETERENDERBUFFERSPROC delete_renderbuffers;
    PFNGLFOGCOORDFPROC fog_coordinate;
    GLuint framebuffers[2],colors[2],depth,lfb_texture;
    unsigned front,back,width,height;
    Gl1Texture textures[4096],*bound;
    uint32_t palette[256],palette_generation,chroma;
    uint32_t ncc[2][256],ncc_generation[2],ncc_selected;
    int ncc_loaded[2];
    uint32_t color[5],alpha[5],depth_mode,alpha_function;
    float constant[4],alpha_reference,density,lod_bias;
    GLint min_filter,mag_filter,wrap_s,wrap_t;
    int chroma_enabled,textured,constant_enabled,failed,locked,write_lock;
    int32_t depth_bias;
    uint8_t fog[64],*rgba;
    uint16_t *lfb;
    uint32_t lock_buffer;
    float projection_center,projection_scale;
    uint64_t triangles,uploads;
    char sky_name[128];
    GLuint sky_layers[3];
    unsigned sky_triangles;
    DWORD sky_tick;
    int sky_loaded,sky_active,fog_pending;
    float sky_yaw,sky_horizon,sky_yaw_scale,sky_offset,sky_depth;
    float fog_color[4];
    GLuint remote_mask;
    int32_t remote_bounds[4];
} Gl1Device;
/* Callbacks borrow the single window-thread device; PortablePlatform owns it. */
static Gl1Device *gl1_current;
static void gl1_fail(const char *reason){
    Gl1Device *g=gl1_current;if(!g || g->failed)return;
    g->failed=1;game_application_log(g->application,"native OpenGL1 failure: %s\n",reason);
}
static int gl1_ready(void){
    Gl1Device *g=gl1_current;
    if(!g || !g->context || g->failed)return 0;
    if(g->thread!=GetCurrentThreadId()){gl1_fail("wrong context thread");return 0;}
    if(wglGetCurrentContext()!=g->context && !wglMakeCurrent(g->dc,g->context)){
        gl1_fail("make current");return 0;
    }
    return 1;
}
static PROC gl1_extension(const char *core,const char *extension){
    PROC p=wglGetProcAddress(core);
    if(!p || p==(PROC)(uintptr_t)1 || p==(PROC)(uintptr_t)2 ||
       p==(PROC)(uintptr_t)3 || p==(PROC)(intptr_t)-1)p=wglGetProcAddress(extension);
    return p;
}
static void gl1_dimensions(int lod,int aspect,unsigned *width,unsigned *height){
    unsigned largest=256u>>(unsigned)lod;
    *width=aspect<=3?largest:largest>>(aspect-3);
    *height=aspect>=3?largest:largest>>(3-aspect);
    if(!*width)*width=1;if(!*height)*height=1;
}
static uint32_t WINAPI gl1_texture_bytes(uint32_t mask,const NativeGlideTextureInfo *info){
    uint32_t bytes=0;int first,last,lod;(void)mask;
    if(!info || info->small_lod<0 || info->small_lod>8 || info->large_lod<0 ||
       info->large_lod>8 || info->aspect<0 || info->aspect>6 || info->format>14)return 0;
    first=info->small_lod<info->large_lod?info->small_lod:info->large_lod;
    last=info->small_lod>info->large_lod?info->small_lod:info->large_lod;
    for(lod=first;lod<=last;lod++){
        unsigned w,h;gl1_dimensions(lod,info->aspect,&w,&h);
        bytes+=(w*h*(info->format>=8?2u:1u)+7u)&~7u;
    }
    return bytes;
}
static GLenum gl1_compare(uint32_t value){
    static const GLenum comparisons[8]={GL_NEVER,GL_LESS,GL_EQUAL,GL_LEQUAL,
        GL_GREATER,GL_NOTEQUAL,GL_GEQUAL,GL_ALWAYS};return comparisons[value&7u];
}
static GLenum gl1_blend(uint32_t value){
    static const GLenum functions[8]={GL_ZERO,GL_SRC_ALPHA,GL_SRC_COLOR,GL_DST_ALPHA,
        GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ONE_MINUS_SRC_COLOR,GL_ONE_MINUS_DST_ALPHA};
    return value<8?functions[value]:value==15?GL_SRC_ALPHA_SATURATE:GL_ONE;
}
/* Preserve the established FBI W encoding, with defined exceptional conversion.
 * One is near; zero/65536 wrap to the far representation used by viewport masks. */
static uint16_t gl1_w_depth(float oow){
    uint64_t fixed;uint32_t low,depth;uint16_t high;int exponent=0;
    if(isnan(oow) || oow<0)return 0xffff;
    if(!isfinite(oow) || (long double)oow*4294967296.0L>=0x1p63L)fixed=UINT64_C(0x8000000000000000);
    else fixed=(uint64_t)(int64_t)(oow*4294967296.0f);
    if(fixed&UINT64_C(0x0000ffff00000000))return 0;
    if(!(fixed&UINT64_C(0xffff0000)))return 0xf001;
    low=(uint32_t)fixed;high=(uint16_t)(low>>16);
    while(!(high&0x8000u) && exponent<15){high=(uint16_t)(high<<1);++exponent;}
    depth=((uint32_t)exponent<<12)+((~low>>(19-exponent))&0xfffu)+1u;
    return (uint16_t)(depth>65535?65535:depth);
}
static void gl1_alpha_state(void){
    Gl1Device *g=gl1_current;
    if(g->chroma_enabled && g->textured){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0);}
    else if(g->alpha_function==7)glDisable(GL_ALPHA_TEST);
    else {glEnable(GL_ALPHA_TEST);glAlphaFunc(gl1_compare(g->alpha_function),g->alpha_reference);}
}
static void gl1_combine(void){
    Gl1Device *g=gl1_current;GLenum alpha=GL_PRIMARY_COLOR;
    if(!gl1_ready())return;
    g->textured=g->color[0]>=3 && g->color[3]==1;
    g->constant_enabled=(g->color[0]==1 && g->color[2]==1) ||
        (g->color[0]==3 && g->color[1]==8 && g->color[3]==2);
    if(!g->textured){glDisable(GL_TEXTURE_2D);gl1_alpha_state();return;}
    glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);
    if(g->color[0]==3 && g->color[1]==9 && g->color[2]==0){
        glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_ONE_MINUS_SRC_COLOR);
    }else glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_REPLACE);
    if(g->alpha[0]==3 && g->alpha[1]==8){
        if(g->alpha[3]==1)alpha=GL_TEXTURE;else if(g->alpha[3]==2)alpha=GL_CONSTANT;
    }else if(g->alpha[0]==1 && g->alpha[2]==1)alpha=GL_CONSTANT;
    if(g->chroma_enabled)alpha=GL_TEXTURE;
    glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,alpha);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA,GL_SRC_ALPHA);
    glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,g->constant);gl1_alpha_state();
}
static Gl1Texture *gl1_find(uint32_t address){
    unsigned i;for(i=0;i<4096;i++)if(gl1_current->textures[i].pixels &&
        gl1_current->textures[i].address==address)return gl1_current->textures+i;return NULL;
}
static void gl1_convert(const Gl1Texture *texture,const uint8_t *input,unsigned count,uint8_t *out){
    Gl1Device *g=gl1_current;unsigned i;
    for(i=0;i<count;i++){
        uint32_t rgb=0;unsigned alpha=255,v=input[i*(texture->format>=8?2u:1u)];
        if(texture->format>=8)v|=(unsigned)input[i*2+1]<<8;
        switch(texture->format){
        case 1:case 9:rgb=g->ncc[g->ncc_selected][v&255];if(texture->format==9)alpha=v>>8;break;
        case 0:case 8:rgb=(((v>>5)&7)*255/7<<16)|(((v>>2)&7)*255/7<<8)|((v&3)*255/3);if(texture->format==8)alpha=v>>8;break;
        case 2:rgb=0xffffff;alpha=v;break;
        case 3:rgb=v*0x010101u;break;
        case 4:rgb=(v&15)*17*0x010101u;alpha=(v>>4)*17;break;
        case 5:case 14:rgb=g->palette[v&255];if(texture->format==14)alpha=v>>8;break;
        case 10:rgb=(((v>>11)&31)*255/31<<16)|(((v>>5)&63)*255/63<<8)|((v&31)*255/31);break;
        case 11:rgb=(((v>>10)&31)*255/31<<16)|(((v>>5)&31)*255/31<<8)|((v&31)*255/31);alpha=v&0x8000?255:0;break;
        case 12:rgb=(((v>>8)&15)*17<<16)|(((v>>4)&15)*17<<8)|((v&15)*17);alpha=(v>>12)*17;break;
        case 13:rgb=(v&255)*0x010101u;alpha=v>>8;break;
        default:gl1_fail("unsupported uploaded texture format");break;
        }
        out[i*4]=(uint8_t)(rgb>>16);out[i*4+1]=(uint8_t)(rgb>>8);out[i*4+2]=(uint8_t)rgb;
        out[i*4+3]=(uint8_t)(g->chroma_enabled && (rgb&0xffffff)==g->chroma?0:alpha);
    }
}
static void gl1_refresh(Gl1Texture *texture){
    Gl1Device *g=gl1_current;size_t offset=0;int lod;
    int compressed=texture->format==1 || texture->format==9;
    glBindTexture(GL_TEXTURE_2D,texture->name);
    /* The cache uploads/binds pixels before world submission downloads NCC.
     * Keep their owned staging bytes, and decode only once the table exists. */
    if(compressed && !g->ncc_loaded[g->ncc_selected])return;
    unsigned levels=(unsigned)(texture->last_lod-texture->first_lod+1);
    int allocate=texture->gpu_width!=texture->width || texture->gpu_height!=texture->height || texture->gpu_levels!=levels;
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    for(lod=texture->first_lod;lod<=texture->last_lod;lod++){
        unsigned w,h;gl1_dimensions(lod,texture->aspect,&w,&h);
        gl1_convert(texture,texture->pixels+offset,w*h,g->rgba);
        if(allocate)glTexImage2D(GL_TEXTURE_2D,lod-texture->first_lod,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,g->rgba);
        else glTexSubImage2D(GL_TEXTURE_2D,lod-texture->first_lod,0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,g->rgba);
        offset+=(w*h*(texture->format>=8?2u:1u)+7u)&~7u;
    }
    texture->palette_generation=g->palette_generation;
    texture->chroma_enabled=g->chroma_enabled;texture->chroma=g->chroma;texture->dirty=0;
    texture->ncc_table=g->ncc_selected;texture->ncc_generation=g->ncc_generation[g->ncc_selected];
    texture->gpu_width=texture->width;texture->gpu_height=texture->height;texture->gpu_levels=levels;
}
static int gl1_texture_stale(const Gl1Texture *t){
    Gl1Device *g=gl1_current;
    return t->dirty || !t->gpu_width || t->chroma_enabled!=g->chroma_enabled ||
        (g->chroma_enabled && t->chroma!=g->chroma) ||
        ((t->format==5 || t->format==14) && t->palette_generation!=g->palette_generation) ||
        ((t->format==1 || t->format==9) && (t->ncc_table!=g->ncc_selected || t->ncc_generation!=g->ncc_generation[g->ncc_selected]));
}
static void WINAPI gl1_upload(uint32_t unit,uint32_t address,uint32_t mask,const NativeGlideTextureInfo *info){
    Gl1Device *g=gl1_current;Gl1Texture *texture;uint32_t bytes;unsigned i;
    if(!gl1_ready())return;
    bytes=gl1_texture_bytes(mask,info);
    if(unit || !bytes || !info->pixels || address>0x200000u || bytes>0x200000u-address){gl1_fail("texture upload extent");return;}
    texture=gl1_find(address);
    /* Upload snapshots are device staging, never a second writable game owner.
     * Retire overwritten spans so their total payload remains within device RAM. */
    for(i=0;i<4096;i++){
        Gl1Texture *old=g->textures+i;
        if(old!=texture && old->pixels && old->address<address+bytes && address<old->address+old->bytes){
            if(g->bound==old)g->bound=NULL;
            glDeleteTextures(1,&old->name);free(old->pixels);memset(old,0,sizeof(*old));
        }
    }
    if(!texture){for(i=0;i<4096;i++)if(!g->textures[i].pixels){texture=g->textures+i;break;}}
    if(!texture){gl1_fail("texture staging capacity");return;}
    if(texture->bytes!=bytes){uint8_t *pixels=realloc(texture->pixels,bytes);if(!pixels){gl1_fail("texture staging allocation");return;}texture->pixels=pixels;}
    memcpy(texture->pixels,info->pixels,bytes);texture->address=address;texture->bytes=bytes;texture->format=info->format;texture->aspect=info->aspect;
    texture->first_lod=info->small_lod<info->large_lod?info->small_lod:info->large_lod;
    texture->last_lod=info->small_lod>info->large_lod?info->small_lod:info->large_lod;
    gl1_dimensions(texture->first_lod,texture->aspect,&texture->width,&texture->height);
    if(!texture->name)glGenTextures(1,&texture->name);
    /* Decode at submission, after the caller has selected this texture's
     * palette/NCC/chroma state. Uploading here converts the previous material. */
    texture->dirty=1;++g->uploads;
}
static void WINAPI gl1_source(uint32_t unit,uint32_t address,uint32_t mask,const NativeGlideTextureInfo *info){
    Gl1Device *g=gl1_current;Gl1Texture *texture;(void)mask;(void)info;
    if(!gl1_ready())return;
    texture=gl1_find(address);if(unit || !texture){gl1_fail("binding absent texture upload");return;}
    g->bound=texture;
    glBindTexture(GL_TEXTURE_2D,texture->name);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,g->min_filter);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,g->mag_filter);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,g->wrap_s);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,g->wrap_t);
}
static float gl1_primary_alpha(const GameRenderVertex *v){
    Gl1Device *g=gl1_current;
    if(g->alpha[0]==0)return 0;
    if((g->alpha[0]==1 && g->alpha[2]==1) || (g->alpha[0]==3 && g->alpha[1]==8 && g->alpha[3]==2))return g->constant[3];
    return v->a/255;
}
static void gl1_emit(const GameRenderVertex *v){
    Gl1Device *g=gl1_current;float depth,q=v->oow;
    if(g->depth_mode==2 || g->depth_mode==4){
        int value=gl1_w_depth(q)+(g->depth_mode==2?(int16_t)g->depth_bias:0);
        depth=(float)(value<0?0:value>65535?65535:value)/65535;
    }else depth=(v->ooz+(g->depth_mode==1?g->depth_bias:0))/65535;
    if(depth<0)depth=0;if(depth>1)depth=1;
    if(g->constant_enabled)glColor4fv(g->constant);
    else {float divisor=g->color[0]==3 && g->color[1]==9 && !g->color[2] && g->color[3]==1?256:255;
        glColor4f(v->r/divisor,v->g/divisor,v->b/divisor,gl1_primary_alpha(v));}
    if(!isfinite(q) || !q)q=v->oow0;if(!isfinite(q) || !q)q=1;
    if(g->fog_coordinate){float lateral=(v->x-g->projection_center)/g->projection_scale;
        g->fog_coordinate(q>0 && q<0.5f?sqrtf(1+lateral*lateral)/q:0);}
    if(g->textured && g->bound){
        float s=256,t=256;
        if(g->bound->width<g->bound->height)s*=((float)g->bound->width/g->bound->height);
        else if(g->bound->height<g->bound->width)t*=((float)g->bound->height/g->bound->width);
        glTexCoord4f(v->sow0/s,v->tow0/t,0,q);
    }
    glVertex3f(v->x,v->y,1-2*depth);
}
#include "portable_gl1_sky.inc"
static int gl1_draw_ready(void){
    Gl1Device *g=gl1_current;
    if(!gl1_ready())return 0;
    if(g->textured && g->bound){
        if((g->bound->format==1 || g->bound->format==9) && !g->ncc_loaded[g->ncc_selected]){
            gl1_fail("compressed texture has no NCC table");return 0;
        }
        if(gl1_texture_stale(g->bound))gl1_refresh(g->bound);
    }
    return 1;
}
static void WINAPI gl1_triangle(const GameRenderVertex *a,const GameRenderVertex *b,const GameRenderVertex *c){
    if(!gl1_draw_ready())return;
    if(gl1_current->sky_active){gl1_sky_triangle(a,b,c);return;}
    glBegin(GL_TRIANGLES);gl1_emit(a);gl1_emit(b);gl1_emit(c);glEnd();++gl1_current->triangles;
}
static void WINAPI gl1_line(const GameRenderVertex *a,const GameRenderVertex *b){if(gl1_draw_ready()){glBegin(GL_LINES);gl1_emit(a);gl1_emit(b);glEnd();}}
static void WINAPI gl1_point(const GameRenderVertex *a){if(gl1_draw_ready()){glBegin(GL_POINTS);gl1_emit(a);glEnd();}}
static int gl1_world_point(const GameRenderVertex *a){
    Gl1Device *g=gl1_current;GameRenderVertex vertex=*a;int textured,constant;
    if(!gl1_ready())return 0;
    /* Captured world points retain the original palette's byte order (BGR).
     * Decode it here, like palette textures and HUD points, without changing
     * the original-comparison geometry. Palette particles have no UV: do not
     * sample the preceding building material or its chroma/constant color. */
    vertex.r=a->b;vertex.b=a->r;
    textured=g->textured;constant=g->constant_enabled;
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);
    g->textured=g->constant_enabled=0;
    glBegin(GL_POINTS);gl1_emit(&vertex);glEnd();
    g->textured=textured;g->constant_enabled=constant;glPopAttrib();return !g->failed;
}
static void WINAPI gl1_color_combine(uint32_t f,uint32_t factor,uint32_t local,uint32_t other,uint32_t invert){
    uint32_t values[5]={f,factor,local,other,invert};memcpy(gl1_current->color,values,sizeof(values));gl1_combine();
}
static void WINAPI gl1_alpha_combine(uint32_t f,uint32_t factor,uint32_t local,uint32_t other,uint32_t invert){
    uint32_t values[5]={f,factor,local,other,invert};memcpy(gl1_current->alpha,values,sizeof(values));gl1_combine();
}
static void WINAPI gl1_tex_combine(uint32_t unit,uint32_t rgb,uint32_t rf,uint32_t alpha,uint32_t af,uint32_t ri,uint32_t ai){
    /*One TMU: the observed ZERO/LOCAL combinations select its existing texture.
     *The framebuffer combine above supplies the actual RGB/alpha program. */
    if(unit || rgb>1 || alpha>1 || rf || af || ri || ai)gl1_fail("unsupported TMU combine program");
}
static void WINAPI gl1_constant(uint32_t value){Gl1Device *g=gl1_current;unsigned i;for(i=0;i<4;i++)g->constant[i]=(float)((value>>(i==3?24:16-i*8))&255)/255;gl1_combine();}
static void WINAPI gl1_alpha_test(uint32_t v){gl1_current->alpha_function=v;if(gl1_ready())gl1_alpha_state();}
static void WINAPI gl1_alpha_reference(uint32_t v){gl1_current->alpha_reference=(v&255)/255.0f;if(gl1_ready())gl1_alpha_state();}
static void WINAPI gl1_chroma_mode(uint32_t v){gl1_current->chroma_enabled=v!=0;if(gl1_ready())gl1_alpha_state();}
static void WINAPI gl1_chroma_value(uint32_t v){gl1_current->chroma=v&0xffffff;}
static void WINAPI gl1_filter(uint32_t unit,uint32_t min,uint32_t mag){
    Gl1Device *g=gl1_current;if(unit){gl1_fail("texture filter unit");return;}g->min_filter=min?GL_LINEAR:GL_NEAREST;g->mag_filter=mag?GL_LINEAR:GL_NEAREST;
    if(gl1_ready() && g->bound){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,g->min_filter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,g->mag_filter);}
}
static void WINAPI gl1_clamp(uint32_t unit,uint32_t s,uint32_t t){
    /* The preserved renderer already fixed this terrain seam: Glide clamp
     * repeats the edge texel; GL_CLAMP mixes in the border with linear filtering. */
    Gl1Device *g=gl1_current;if(unit){gl1_fail("texture clamp unit");return;}
    g->wrap_s=s?GL_CLAMP_TO_EDGE:GL_REPEAT;g->wrap_t=t?GL_CLAMP_TO_EDGE:GL_REPEAT;
    if(gl1_ready() && g->bound){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,g->wrap_s);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,g->wrap_t);}
}
static void WINAPI gl1_palette_range(uint32_t unit,uint32_t table,const uint32_t *pixels,uint32_t first,uint32_t last){
    Gl1Device *g=gl1_current;if(unit || table!=2 || !pixels || first>last || last>=256){gl1_fail("palette range");return;}
    if(memcmp(g->palette+first,pixels+first,(last-first+1)*4)){memcpy(g->palette+first,pixels+first,(last-first+1)*4);++g->palette_generation;}
}
static void WINAPI gl1_palette(uint32_t unit,uint32_t table,const uint32_t *pixels){
    Gl1Device *g=gl1_current;uint32_t rgb[256];
    if(unit || !pixels || table>2){gl1_fail("texture table upload");return;}
    if(table==2){gl1_palette_range(unit,table,pixels,0,255);return;}
    if(!asset_texture_ncc_palette((const uint8_t *)pixels,rgb)){gl1_fail("NCC table decode");return;}
    if(!g->ncc_loaded[table] || memcmp(g->ncc[table],rgb,sizeof(rgb))){
        memcpy(g->ncc[table],rgb,sizeof(rgb));++g->ncc_generation[table];g->ncc_loaded[table]=1;
    }
}
static void WINAPI gl1_depth_mode(uint32_t v){gl1_current->depth_mode=v;if(gl1_ready()){if(v)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);}}
static void WINAPI gl1_depth_mask(uint32_t v){if(gl1_ready())glDepthMask(v?GL_TRUE:GL_FALSE);}
static void WINAPI gl1_depth_func(uint32_t v){if(gl1_ready())glDepthFunc(gl1_compare(v));}
static void WINAPI gl1_depth_bias(int32_t v){gl1_current->depth_bias=v;}
static void WINAPI gl1_dither(uint32_t v){if(gl1_ready()){if(v)glEnable(GL_DITHER);else glDisable(GL_DITHER);}}
static void WINAPI gl1_cull(uint32_t v){if(gl1_ready()){if(v){glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(v==1?GL_CCW:GL_CW);}else glDisable(GL_CULL_FACE);}}
static void WINAPI gl1_alpha_blend(uint32_t source,uint32_t dest,uint32_t as,uint32_t ad){(void)as;(void)ad;if(gl1_ready()){glEnable(GL_BLEND);glBlendFunc(gl1_blend(source),gl1_blend(dest));}}
static void WINAPI gl1_lod_bias(uint32_t unit,float v){if(unit)gl1_fail("LOD unit");else gl1_current->lod_bias=v;}
static void WINAPI gl1_ncc(uint32_t unit,uint32_t table){if(unit || table>1)gl1_fail("NCC selector");else gl1_current->ncc_selected=table;}
static void WINAPI gl1_fog_generate(uint8_t *table,float density){unsigned i;gl1_current->density=density;for(i=0;i<64;i++){float w=ldexpf(1+(i&3)*0.25f,i>>2),d=density*w;table[i]=(uint8_t)(255*(1-expf(-d*d)));}}
static void WINAPI gl1_fog_color(uint32_t v){GLfloat rgba[4]={(float)((v>>16)&255)/255,(float)((v>>8)&255)/255,(float)(v&255)/255,1};if(gl1_ready())glFogfv(GL_FOG_COLOR,rgba);}
static void WINAPI gl1_fog_table(const uint8_t *table){memcpy(gl1_current->fog,table,64);}
static void WINAPI gl1_fog_mode(uint32_t v){
    Gl1Device *g=gl1_current;if(!gl1_ready())return;
    if(g->sky_loaded){gl1_level_fog(v!=0);return;}
    if(v){uint32_t bits;glFogi(GL_FOG_COORDINATE_SOURCE,GL_FOG_COORDINATE);glFogi(GL_FOG_MODE,GL_EXP2);glFogf(GL_FOG_DENSITY,g->density);glEnable(GL_FOG);
        bits=game_memory_read_u32(&g->application->memory,0x6290c8);memcpy(&g->projection_center,&bits,4);
        bits=game_memory_read_u32(&g->application->memory,0x6290f4);memcpy(&g->projection_scale,&bits,4);
        if(!(g->projection_scale>1))g->projection_scale=381.3607f;
    }else glDisable(GL_FOG);
}
static uint32_t WINAPI gl1_tex_min(uint32_t unit){if(unit)gl1_fail("texture memory unit");return 0;}
static uint32_t WINAPI gl1_tex_max(uint32_t unit){if(unit)gl1_fail("texture memory unit");return 0x1fffff;}
/*Hardware reject counters are unavailable through fixed-function OpenGL.
 *Keep those diagnostic lanes zero, rather than fabricate raster results. */
static void WINAPI gl1_stats(uint32_t *out){memset(out,0,5*4);}
static void WINAPI gl1_reset_stats(void){gl1_current->triangles=gl1_current->uploads=0;}
static void WINAPI gl1_clear(uint32_t color,uint32_t alpha,uint32_t depth){
    GLboolean mask;
    if(!gl1_ready())return;glClearColor(((color>>16)&255)/255.0f,((color>>8)&255)/255.0f,(color&255)/255.0f,(alpha&255)/255.0f);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glDepthMask(GL_TRUE);
    glClearDepth((depth&65535)/65535.0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glDepthMask(mask);
}
static void gl1_screen_rect(unsigned left,unsigned top,unsigned right,unsigned bottom){
    Gl1Device *g=gl1_current;
    float l=(float)left/g->width,r=(float)right/g->width;
    float t=1-(float)top/g->height,b=1-(float)bottom/g->height;
    glColor4f(1,1,1,1);glBegin(GL_QUADS);
    glTexCoord2f(l,t);glVertex2f((float)left,(float)top);glTexCoord2f(r,t);glVertex2f((float)right,(float)top);
    glTexCoord2f(r,b);glVertex2f((float)right,(float)bottom);glTexCoord2f(l,b);glVertex2f((float)left,(float)bottom);glEnd();
}
static void gl1_quad(GLuint texture){
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_FOG);glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    gl1_screen_rect(0,0,gl1_current->width,gl1_current->height);
}
static void WINAPI gl1_swap(int32_t interval){
    Gl1Device *g=gl1_current;RECT client;unsigned old;(void)interval;
    if(!gl1_ready())return;
    if(glGetError()!=GL_NO_ERROR){gl1_fail("OpenGL draw/state error");return;}
    old=g->front;g->front=g->back;g->back=old;
    glPushAttrib(GL_ALL_ATTRIB_BITS);g->bind_framebuffer(GL_FRAMEBUFFER,0);
    if(!GetClientRect(g->window,&client)){gl1_fail("present client extent");glPopAttrib();return;}
    glViewport(0,0,client.right-client.left,client.bottom-client.top);gl1_quad(g->colors[g->front]);
    if(!SwapBuffers(g->dc))gl1_fail("swap buffers");
    g->bind_framebuffer(GL_FRAMEBUFFER,g->framebuffers[g->back]);glPopAttrib();glViewport(0,0,g->width,g->height);
}
static int32_t WINAPI gl1_lock(uint32_t type,uint32_t buffer,uint32_t format,uint32_t origin,uint32_t pipeline,NativeGlideLfbInfo *info){
    Gl1Device *g=gl1_current;unsigned x,y;(void)pipeline;
    if(!gl1_ready() || !info || g->locked || type>1 || buffer>1 || format || origin)return 0;
    g->locked=1;g->write_lock=type==1;g->lock_buffer=buffer;
    if(g->write_lock){for(x=0;x<g->width*g->height;x++)g->lfb[x]=0xf81f;}
    else{
        g->bind_framebuffer(GL_FRAMEBUFFER,g->framebuffers[buffer==0?g->front:g->back]);
        glReadBuffer(GL_COLOR_ATTACHMENT0);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,g->width,g->height,GL_RGBA,GL_UNSIGNED_BYTE,g->rgba);
        for(y=0;y<g->height;y++)for(x=0;x<g->width;x++){
            const uint8_t *p=g->rgba+((size_t)(g->height-y-1)*g->width+x)*4;
            g->lfb[(size_t)y*g->width+x]=(uint16_t)((p[0]>>3)<<11|(p[1]>>2)<<5|(p[2]>>3));
        }
        g->bind_framebuffer(GL_FRAMEBUFFER,g->framebuffers[g->back]);
    }
    info->size=sizeof(*info);info->pixels=g->lfb;info->stride=g->width*2;info->write_mode=0;info->origin=0;return 1;
}
static int32_t WINAPI gl1_unlock(uint32_t type,uint32_t buffer){
    Gl1Device *g=gl1_current;unsigned x,y;(void)type;
    if(!gl1_ready() || !g->locked || buffer!=g->lock_buffer)return 0;
    if(g->write_lock){
        unsigned left=g->width,top=g->height,right=0,bottom=0,width;
        /* Native text callers lock once per string. Find their actual ink;
         * untouched magenta must neither upload nor repaint stale overlay data. */
        for(y=0;y<g->height;y++)for(x=0;x<g->width;x++)if(g->lfb[(size_t)y*g->width+x]!=0xf81f){
            if(x<left)left=x;if(x>=right)right=x+1;
            if(y<top)top=y;if(y>=bottom)bottom=y+1;
        }
        if(left==g->width){g->locked=0;return 1;}
        width=right-left;
        for(y=top;y<bottom;y++)for(x=left;x<right;x++){
            uint16_t v=g->lfb[(size_t)y*g->width+x];uint8_t *p=g->rgba+((size_t)(bottom-y-1)*width+x-left)*4;
            p[0]=(uint8_t)(((v>>11)&31)*255/31);p[1]=(uint8_t)(((v>>5)&63)*255/63);p[2]=(uint8_t)((v&31)*255/31);p[3]=v==0xf81f?0:255;
        }
        glPushAttrib(GL_ALL_ATTRIB_BITS);g->bind_framebuffer(GL_FRAMEBUFFER,g->framebuffers[buffer==0?g->front:g->back]);
        /* One device-owned overlay replaces the driver-specific glDrawPixels
         * path. The existing bounded LFB owner and magenta transparency stay
         * authoritative; only this write's bounded rectangle is uploaded. */
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_FOG);glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,g->lfb_texture);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexSubImage2D(GL_TEXTURE_2D,0,left,g->height-bottom,width,bottom-top,GL_RGBA,GL_UNSIGNED_BYTE,g->rgba);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);gl1_screen_rect(left,top,right,bottom);
        glPopAttrib();g->bind_framebuffer(GL_FRAMEBUFFER,g->framebuffers[g->back]);
    }
    g->locked=0;return 1;
}
static void WINAPI gl1_lfb_depth(uint32_t value){(void)value; /*Text writes run with depth disabled by their native caller. */}
static void WINAPI gl1_close(void){
    Gl1Device *g=gl1_current;unsigned i;if(!g)return;
    if(g->context){
        if(wglGetCurrentContext()==g->context || wglMakeCurrent(g->dc,g->context)){
            for(i=0;i<4096;i++)if(g->textures[i].name)glDeleteTextures(1,&g->textures[i].name);
            if(g->delete_framebuffers)g->delete_framebuffers(2,g->framebuffers);
            if(g->delete_renderbuffers && g->depth)g->delete_renderbuffers(1,&g->depth);
            glDeleteTextures(2,g->colors);glDeleteTextures(3,g->sky_layers);
            if(g->lfb_texture)glDeleteTextures(1,&g->lfb_texture);
            if(g->remote_mask)glDeleteTextures(1,&g->remote_mask);
            wglMakeCurrent(NULL,NULL);
        }
        wglDeleteContext(g->context);g->context=NULL;
    }
    if(g->dc){ReleaseDC(g->window,g->dc);g->dc=NULL;}
    for(i=0;i<4096;i++){free(g->textures[i].pixels);memset(g->textures+i,0,sizeof(g->textures[i]));}
    free(g->lfb);free(g->rgba);g->lfb=NULL;g->rgba=NULL;g->bound=NULL;g->locked=0;
    memset(g->framebuffers,0,sizeof(g->framebuffers));memset(g->colors,0,sizeof(g->colors));g->depth=0;
    memset(g->sky_layers,0,sizeof(g->sky_layers));g->sky_name[0]=0;
    g->sky_loaded=g->sky_active=g->fog_pending=0;
    memset(g->ncc_loaded,0,sizeof(g->ncc_loaded));g->ncc_selected=0;
    g->remote_mask=g->lfb_texture=0;
}
static void WINAPI gl1_shutdown(void){gl1_close();free(gl1_current);gl1_current=NULL;}
static int32_t WINAPI gl1_open(uintptr_t handle,uint32_t resolution,uint32_t refresh,uint32_t color,uint32_t origin,int32_t buffers,int32_t auxiliary){
    Gl1Device *g=gl1_current;PIXELFORMATDESCRIPTOR descriptor={0};int format;unsigned i;
    (void)resolution;(void)refresh;(void)color;(void)origin;(void)buffers;(void)auxiliary;
    if(!g || g->context || !IsWindow((HWND)handle))return 0;
    g->failed=0;g->window=(HWND)handle;g->width=640;g->height=480;g->front=0;g->back=1;
    descriptor.nSize=sizeof(descriptor);descriptor.nVersion=1;
    descriptor.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    descriptor.iPixelType=PFD_TYPE_RGBA;descriptor.cColorBits=32;descriptor.cDepthBits=24;descriptor.cStencilBits=8;
    g->dc=GetDC(g->window);if(!g->dc)goto failed;
    format=GetPixelFormat(g->dc);
    if(!format){format=ChoosePixelFormat(g->dc,&descriptor);if(!format || !SetPixelFormat(g->dc,format,&descriptor))goto failed;}
    g->context=wglCreateContext(g->dc);if(!g->context || !wglMakeCurrent(g->dc,g->context))goto failed;
    g->thread=GetCurrentThreadId();
#define GL1_EXTENSION(field,core,ext) do{PROC entry=gl1_extension(core,ext);if(!entry)goto failed;memcpy(&g->field,&entry,sizeof(entry));}while(0)
    GL1_EXTENSION(gen_framebuffers,"glGenFramebuffers","glGenFramebuffersEXT");
    GL1_EXTENSION(bind_framebuffer,"glBindFramebuffer","glBindFramebufferEXT");
    GL1_EXTENSION(framebuffer_texture,"glFramebufferTexture2D","glFramebufferTexture2DEXT");
    GL1_EXTENSION(check_framebuffer,"glCheckFramebufferStatus","glCheckFramebufferStatusEXT");
    GL1_EXTENSION(delete_framebuffers,"glDeleteFramebuffers","glDeleteFramebuffersEXT");
    GL1_EXTENSION(gen_renderbuffers,"glGenRenderbuffers","glGenRenderbuffersEXT");
    GL1_EXTENSION(bind_renderbuffer,"glBindRenderbuffer","glBindRenderbufferEXT");
    GL1_EXTENSION(renderbuffer_storage,"glRenderbufferStorage","glRenderbufferStorageEXT");
    GL1_EXTENSION(framebuffer_renderbuffer,"glFramebufferRenderbuffer","glFramebufferRenderbufferEXT");
    GL1_EXTENSION(delete_renderbuffers,"glDeleteRenderbuffers","glDeleteRenderbuffersEXT");
    GL1_EXTENSION(fog_coordinate,"glFogCoordf","glFogCoordfEXT");
#undef GL1_EXTENSION
    g->lfb=malloc((size_t)g->width*g->height*2);g->rgba=malloc((size_t)g->width*g->height*4);
    if(!g->lfb || !g->rgba)goto failed;
    glGenTextures(1,&g->lfb_texture);glBindTexture(GL_TEXTURE_2D,g->lfb_texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,g->width,g->height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    g->gen_framebuffers(2,g->framebuffers);glGenTextures(2,g->colors);
    g->gen_renderbuffers(1,&g->depth);g->bind_renderbuffer(GL_RENDERBUFFER,g->depth);
    g->renderbuffer_storage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,g->width,g->height);
    for(i=0;i<2;i++){
        g->bind_framebuffer(GL_FRAMEBUFFER,g->framebuffers[i]);glBindTexture(GL_TEXTURE_2D,g->colors[i]);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,g->width,g->height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        g->framebuffer_texture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,g->colors[i],0);
        g->framebuffer_renderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,g->depth);
        if(g->check_framebuffer(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)goto failed;
        glClearColor(0,0,0,0);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    }
    g->bind_framebuffer(GL_FRAMEBUFFER,g->framebuffers[g->back]);glViewport(0,0,g->width,g->height);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,g->width,g->height,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_DEPTH_TEST);glShadeModel(GL_SMOOTH);
    g->palette_generation=1;g->min_filter=g->mag_filter=GL_LINEAR;g->wrap_s=g->wrap_t=GL_REPEAT;
    g->constant[0]=g->constant[1]=g->constant[2]=g->constant[3]=1;g->alpha_function=7;
    g->projection_center=320;g->projection_scale=381.3607f;
    game_application_log(g->application,"native OpenGL1 device: %s\n",(const char *)glGetString(GL_RENDERER));
    if(glGetError()!=GL_NO_ERROR)goto failed;return 1;
failed:gl1_fail("window/context/framebuffer initialization");gl1_close();return 0;
}
static int gl1_install(GameApplication *a,PortablePlatform *p){
    if(gl1_current)return 0;
    gl1_current=calloc(1,sizeof(*gl1_current));if(!gl1_current)return 0;
    gl1_current->application=a;p->graphics_owner=gl1_current;
    p->glide_shutdown=gl1_shutdown;p->window_open=gl1_open;p->window_close=gl1_close;
    p->fog_generate=gl1_fog_generate;p->fog_color=gl1_fog_color;p->fog_table=gl1_fog_table;p->fog_mode=gl1_fog_mode;
    p->texture_lod_bias=gl1_lod_bias;p->texture_ncc_table=gl1_ncc;p->chroma_mode=gl1_chroma_mode;p->chroma_value=gl1_chroma_value;
    p->alpha_reference=gl1_alpha_reference;p->alpha_test=gl1_alpha_test;p->constant_color=gl1_constant;
    p->buffer_clear=gl1_clear;p->buffer_swap=gl1_swap;p->lfb_lock=gl1_lock;p->lfb_unlock=gl1_unlock;p->lfb_depth=gl1_lfb_depth;
    p->texture_min=gl1_tex_min;p->texture_max=gl1_tex_max;p->texture_bytes=gl1_texture_bytes;p->texture_upload=gl1_upload;
    p->texture_statistics=gl1_stats;p->texture_reset_statistics=gl1_reset_stats;p->texture_source=gl1_source;
    p->draw_triangle=gl1_triangle;p->draw_line=gl1_line;p->draw_point=gl1_point;
    p->texture_filter=gl1_filter;p->texture_clamp=gl1_clamp;p->palette_upload=gl1_palette;p->palette_range=gl1_palette_range;
    p->texture_combine=gl1_tex_combine;p->color_combine=gl1_color_combine;p->alpha_combine=gl1_alpha_combine;
    p->depth_mode=gl1_depth_mode;p->depth_function=gl1_depth_func;p->depth_mask=gl1_depth_mask;p->depth_bias=gl1_depth_bias;
    p->alpha_blend=gl1_alpha_blend;p->cull_mode=gl1_cull;p->dither=gl1_dither;p->glide_initialized=1;
    game_application_log(a,"%s\n","NATIVE_OPENGL1_CPU_FREE: native fixed-function device; no Glide DLL fallback");return 1;
}
