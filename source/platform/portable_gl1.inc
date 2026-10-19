/* Native fixed-function OpenGL device. Included after PortablePlatform so the
 * existing platform owner owns its context, upload snapshots and readback.
 * The preserved renderer supplies depth, combine, chroma and UV semantics;
 * this device does not link that runtime or load a graphics wrapper DLL. */
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <math.h>
#ifdef UPRISING_EXPERIMENTAL_VULKAN
#include "vulkan_menu.h"
#include "vulkan_draw.h"
#endif

typedef struct Gl1Texture {
    uint32_t key,bytes,format,palette_generation,chroma;
    uint32_t ncc_generation,ncc_table;
    int dirty,chroma_enabled;
    int first_lod,last_lod,aspect;
    unsigned width,height;
    unsigned gpu_width,gpu_height,gpu_levels;
    GLuint name;
    uint8_t *pixels;
    struct Gl1Texture *next;
    void *vulkan_texture;
    unsigned native_tile;int ui_tile;
} Gl1Texture;
typedef struct Gl1Device {
    GameApplication *application;
    SDL_Window *window;SDL_GLContext context;SDL_ThreadID thread;
    int context_state_saved;GLint previous_matrix_mode;
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
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC multisample_storage;
    PFNGLBLITFRAMEBUFFERPROC blit_framebuffer;
    GLuint framebuffers[2],colors[2],depth,lfb_texture;
    GLuint aa_framebuffers[2],aa_colors[2],aa_depth;int aa_samples;
    unsigned front,back,width,height;
    Gl1Texture textures[4096],*texture_index[2048],*bound;
    uint32_t palette[256],palette_generation,chroma;
    uint32_t ncc[2][256],ncc_generation[2],ncc_selected;
    int ncc_loaded[2];
    RenderColorMode color_mode;RenderAlphaMode alpha_mode;
    uint32_t depth_mode,alpha_function;
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
    uint32_t sky_tick;
    int sky_loaded,sky_active,fog_pending;
    float sky_yaw,sky_horizon,sky_yaw_scale,sky_offset,sky_depth;
    float fog_color[4];
    GLuint remote_mask;
    int32_t remote_bounds[4];
    struct Vk1Device *vulkan;
    uint32_t native_depth_write,native_depth_func,native_cull;
    int native_blend,native_fog;
    float native_fog_start,native_fog_end;
} Gl1Device;
/* Callbacks borrow the single window-thread device; PortablePlatform owns it. */
static Gl1Device *gl1_current;
#ifdef UPRISING_EXPERIMENTAL_VULKAN
static int vk1_open(SDL_Window *,VulkanMenu *);
static void vk1_close(void),vk1_texture_release(Gl1Texture *),vk1_refresh(Gl1Texture *);
static void vk1_triangle(const GameRenderVertex *,const GameRenderVertex *,const GameRenderVertex *);
static void vk1_line(const GameRenderVertex *,const GameRenderVertex *),vk1_point(const GameRenderVertex *);
static void vk1_clear(uint32_t,uint32_t,uint32_t),vk1_swap(void);
static int vk1_map(int,unsigned,RenderPixelView *),vk1_unmap(void);
static int vk1_sky_upload(unsigned,unsigned,unsigned),vk1_sky_end(void);
static void vk1_sky_release(void);
static int vk1_remote_read(const int32_t[4]),vk1_remote_upload(unsigned,unsigned),vk1_remote_sky(const GameRemoteSkyFrame *);
#define VK1_RETURN(call) do {if(gl1_current && gl1_current->vulkan)return (call);}while(0)
#define VK1_DO(call) do {if(gl1_current && gl1_current->vulkan){call;return;}}while(0)
#else
#define VK1_RETURN(call) do {}while(0)
#define VK1_DO(call) do {}while(0)
#endif
static void gl1_fail(const char *reason){
    Gl1Device *g=gl1_current;if(!g || g->failed)return;
    g->failed=1;game_application_log(g->application,"native OpenGL1 failure: %s\n",reason);
}
static int gl1_ready(void){
    Gl1Device *g=gl1_current;
    if(!g || (!g->context && !g->vulkan) || g->failed)return 0;
    if(g->thread!=SDL_GetCurrentThreadID()){gl1_fail("wrong context thread");return 0;}
    /* This owner is the only context user on the window thread. Context
     * creation binds it; teardown unbinds it. Avoid SDL TLS lookups for every
     * primitive/state command, and verify ownership at the frame boundary. */
    return 1;
}
static int gl1_gl_ready(void){return gl1_ready() && !gl1_current->vulkan;}
static SDL_FunctionPointer gl1_extension(const char *core,const char *extension){
    SDL_FunctionPointer p=SDL_GL_GetProcAddress(core);
    if(!p)p=SDL_GL_GetProcAddress(extension);
    return p;
}
static GLuint gl1_render_target(Gl1Device *g,unsigned buffer){return g->aa_samples?g->aa_framebuffers[buffer]:g->framebuffers[buffer];}
static void gl1_resolve(Gl1Device *g,unsigned buffer){
    if(!g->aa_samples)return;
    g->bind_framebuffer(GL_READ_FRAMEBUFFER,g->aa_framebuffers[buffer]);glReadBuffer(GL_COLOR_ATTACHMENT0);
    g->bind_framebuffer(GL_DRAW_FRAMEBUFFER,g->framebuffers[buffer]);glDrawBuffer(GL_COLOR_ATTACHMENT0);
    g->blit_framebuffer(0,0,g->width,g->height,0,0,g->width,g->height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    g->bind_framebuffer(GL_FRAMEBUFFER,gl1_render_target(g,g->back));
}
/* Fog-color and monitor-mask sampling also read the live world. ReadPixels
 * cannot read a multisample FBO; resolve color while retaining the draw owner. */
static void gl1_readback_begin(Gl1Device *g){
    if(!g->aa_samples)return;
    gl1_resolve(g,g->back);
    g->bind_framebuffer(GL_READ_FRAMEBUFFER,g->framebuffers[g->back]);glReadBuffer(GL_COLOR_ATTACHMENT0);
}
static void gl1_readback_end(Gl1Device *g){
    if(g->aa_samples)g->bind_framebuffer(GL_READ_FRAMEBUFFER,gl1_render_target(g,g->back));
}
static void gl1_msaa_release(Gl1Device *g){
    if(g->delete_framebuffers)g->delete_framebuffers(2,g->aa_framebuffers);
    if(g->delete_renderbuffers){g->delete_renderbuffers(2,g->aa_colors);if(g->aa_depth)g->delete_renderbuffers(1,&g->aa_depth);}
    memset(g->aa_framebuffers,0,sizeof(g->aa_framebuffers));memset(g->aa_colors,0,sizeof(g->aa_colors));
    g->aa_depth=0;g->aa_samples=0;
}
static void gl1_msaa_open(Gl1Device *g){
    int samples=game_display_antialias_samples(g->application);unsigned i;SDL_FunctionPointer entry;
    if(!samples)return;
    entry=gl1_extension("glRenderbufferStorageMultisample","glRenderbufferStorageMultisampleEXT");
    if(!entry){game_display_antialias_unavailable(g->application);return;}
    memcpy(&g->multisample_storage,&entry,sizeof(entry));
    entry=gl1_extension("glBlitFramebuffer","glBlitFramebufferEXT");
    if(!entry){game_display_antialias_unavailable(g->application);return;}
    memcpy(&g->blit_framebuffer,&entry,sizeof(entry));
    g->gen_framebuffers(2,g->aa_framebuffers);g->gen_renderbuffers(2,g->aa_colors);g->gen_renderbuffers(1,&g->aa_depth);
    g->bind_renderbuffer(GL_RENDERBUFFER,g->aa_depth);
    g->multisample_storage(GL_RENDERBUFFER,samples,GL_DEPTH24_STENCIL8,g->width,g->height);
    for(i=0;i<2;++i){
        g->bind_framebuffer(GL_FRAMEBUFFER,g->aa_framebuffers[i]);g->bind_renderbuffer(GL_RENDERBUFFER,g->aa_colors[i]);
        g->multisample_storage(GL_RENDERBUFFER,samples,GL_RGBA8,g->width,g->height);
        g->framebuffer_renderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,g->aa_colors[i]);
        g->framebuffer_renderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,g->aa_depth);
        if(g->check_framebuffer(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE || glGetError()!=GL_NO_ERROR){
            gl1_msaa_release(g);game_application_log(g->application,"%s\n","MSAA target allocation failed; using single-sample rendering");
            game_display_antialias_unavailable(g->application);
            for(i=0;i<8 && glGetError()!=GL_NO_ERROR;++i){}
            return;
        }
        glClearColor(0,0,0,0);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    }
    g->aa_samples=samples;
    game_application_log(g->application,"3D MSAA samples: %s\n",samples==8?"8":samples==4?"4":"2");
}
static void gl1_dimensions(int lod,int aspect,unsigned *width,unsigned *height){
    unsigned largest=256u>>(unsigned)lod;
    *width=aspect<=3?largest:largest>>(aspect-3);
    *height=aspect>=3?largest:largest>>(3-aspect);
    if(!*width)*width=1;if(!*height)*height=1;
}
static uint32_t gl1_texture_bytes(const RenderTextureView *info){
    uint32_t bytes=0;int first,last,lod;
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
    if(g->vulkan)return;
    if(g->chroma_enabled && g->textured){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0);}
    else if(g->alpha_function==7)glDisable(GL_ALPHA_TEST);
    else {glEnable(GL_ALPHA_TEST);glAlphaFunc(gl1_compare(g->alpha_function),g->alpha_reference);}
}
static void gl1_combine(void){
    Gl1Device *g=gl1_current;GLenum alpha=GL_PRIMARY_COLOR;
    if(!gl1_ready())return;
    g->textured=g->color_mode==RENDER_COLOR_TEXTURE || g->color_mode==RENDER_COLOR_INVERSE_TEXTURE;
    g->constant_enabled=g->color_mode==RENDER_COLOR_CONSTANT;
    if(g->vulkan)return;
    if(!g->textured){glDisable(GL_TEXTURE_2D);gl1_alpha_state();return;}
    glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);
    if(g->color_mode==RENDER_COLOR_INVERSE_TEXTURE){
        glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_ONE_MINUS_SRC_COLOR);
    }else glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_REPLACE);
    if(g->alpha_mode==RENDER_ALPHA_TEXTURE)alpha=GL_TEXTURE;
    else if(g->alpha_mode==RENDER_ALPHA_CONSTANT)alpha=GL_CONSTANT;
    if(g->chroma_enabled)alpha=GL_TEXTURE;
    glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,alpha);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA,GL_SRC_ALPHA);
    glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,g->constant);gl1_alpha_state();
}
static unsigned gl1_texture_bucket(uint32_t key){return (key^(key>>12))&2047u;}
static Gl1Texture *gl1_find(uint32_t key){
    Gl1Device *g=gl1_current;Gl1Texture *t;
    for(t=g->texture_index[gl1_texture_bucket(key)];t;t=t->next)
        if(t->key==key)return t;
    return NULL;
}
static void gl1_release_texture(uint32_t key){
    Gl1Device *g=gl1_current;Gl1Texture *t,**link;
    if(!g)return;
    link=&g->texture_index[gl1_texture_bucket(key)];
    while((t=*link)!=NULL){
        if(t->key==key){
            *link=t->next;if(g->bound==t)g->bound=NULL;
            VK1_DO(vk1_texture_release(t);free(t->pixels);memset(t,0,sizeof(*t)));
            if(t->name && g->context && gl1_ready())glDeleteTextures(1,&t->name);
            free(t->pixels);memset(t,0,sizeof(*t));return;
        }
        link=&t->next;
    }
}
static void gl1_release_textures(void){
    Gl1Device *g=gl1_current;unsigned i;if(!g)return;
    for(i=0;i<4096;i++)if(g->textures[i].pixels)gl1_release_texture(g->textures[i].key);
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
    VK1_DO(vk1_refresh(texture));
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
    return t->dirty || !t->gpu_width || (!g->vulkan && (t->chroma_enabled!=g->chroma_enabled ||
        (g->chroma_enabled && t->chroma!=g->chroma))) ||
        ((t->format==5 || t->format==14) && t->palette_generation!=g->palette_generation) ||
        ((t->format==1 || t->format==9) && (t->ncc_table!=g->ncc_selected || t->ncc_generation!=g->ncc_generation[g->ncc_selected]));
}
static void gl1_upload(uint32_t address,const RenderTextureView *info){
    Gl1Device *g=gl1_current;Gl1Texture *texture;uint32_t bytes;unsigned i;
    if(!gl1_ready())return;
    bytes=gl1_texture_bytes(info);
    if(!bytes || !info->pixels || !address){gl1_fail("texture upload extent");return;}
    texture=gl1_find(address);
    if(!texture){for(i=0;i<4096;i++)if(!g->textures[i].pixels){texture=g->textures+i;break;}}
    if(!texture){gl1_fail("texture staging capacity");return;}
    if(texture->bytes!=bytes){uint8_t *pixels=realloc(texture->pixels,bytes);if(!pixels){gl1_fail("texture staging allocation");return;}texture->pixels=pixels;}
    if(!texture->key){unsigned bucket=gl1_texture_bucket(address);texture->next=g->texture_index[bucket];g->texture_index[bucket]=texture;}
    memcpy(texture->pixels,info->pixels,bytes);texture->key=address;texture->bytes=bytes;texture->format=info->format;texture->aspect=info->aspect;
    texture->first_lod=info->small_lod<info->large_lod?info->small_lod:info->large_lod;
    texture->last_lod=info->small_lod>info->large_lod?info->small_lod:info->large_lod;
    gl1_dimensions(texture->first_lod,texture->aspect,&texture->width,&texture->height);
    if(!g->vulkan && !texture->name)glGenTextures(1,&texture->name);
    /* Decode at submission, after the caller has selected this texture's
     * palette/NCC/chroma state. Uploading here converts the previous material. */
    texture->dirty=1;++g->uploads;
    texture->ui_tile=0;
}
static void gl1_ui_tile(uint32_t address){Gl1Texture *texture=gl1_find(address);if(texture && texture->width==32 && texture->height==32 && texture->format==5)texture->ui_tile=1;}
static void gl1_source(uint32_t address){
    Gl1Device *g=gl1_current;Gl1Texture *texture;
    if(!gl1_ready())return;
    texture=gl1_find(address);if(!texture){gl1_fail("binding absent texture upload");return;}
    g->bound=texture;
    if(g->vulkan)return;
    glBindTexture(GL_TEXTURE_2D,texture->name);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,g->min_filter);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,g->mag_filter);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,g->wrap_s);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,g->wrap_t);
}
static float gl1_primary_alpha(const GameRenderVertex *v){
    Gl1Device *g=gl1_current;
    if(g->alpha_mode==RENDER_ALPHA_ZERO)return 0;
    if(g->alpha_mode==RENDER_ALPHA_CONSTANT)return g->constant[3];
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
    else {float divisor=g->color_mode==RENDER_COLOR_INVERSE_TEXTURE?256:255;
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
static void gl1_triangle(const GameRenderVertex *a,const GameRenderVertex *b,const GameRenderVertex *c){
    if(!gl1_draw_ready())return;
    if(gl1_current->sky_active){gl1_sky_triangle(a,b,c);return;}
    VK1_DO(vk1_triangle(a,b,c));
    glBegin(GL_TRIANGLES);gl1_emit(a);gl1_emit(b);gl1_emit(c);glEnd();++gl1_current->triangles;
}
static void gl1_line(const GameRenderVertex *a,const GameRenderVertex *b){VK1_DO(vk1_line(a,b));if(gl1_draw_ready()){glBegin(GL_LINES);gl1_emit(a);gl1_emit(b);glEnd();}}
static void gl1_point(const GameRenderVertex *a){VK1_DO(vk1_point(a));if(gl1_draw_ready()){glBegin(GL_POINTS);gl1_emit(a);glEnd();}}
static int gl1_world_point(const GameRenderVertex *a){
    Gl1Device *g=gl1_current;GameRenderVertex vertex=*a;int textured,constant;
    if(!gl1_ready())return 0;
    /* Captured world points retain the original palette's byte order (BGR).
     * Decode it here, like palette textures and HUD points, without changing
     * the original-comparison geometry. Palette particles have no UV: do not
     * sample the preceding building material or its chroma/constant color. */
    vertex.r=a->b;vertex.b=a->r;
    textured=g->textured;constant=g->constant_enabled;
    if(g->vulkan){g->textured=g->constant_enabled=0;gl1_point(&vertex);g->textured=textured;g->constant_enabled=constant;return !g->failed;}
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);
    g->textured=g->constant_enabled=0;
    glBegin(GL_POINTS);gl1_emit(&vertex);glEnd();
    g->textured=textured;g->constant_enabled=constant;glPopAttrib();return !g->failed;
}
static void gl1_color_mode(RenderColorMode mode){gl1_current->color_mode=mode;gl1_combine();}
static void gl1_alpha_mode(RenderAlphaMode mode){gl1_current->alpha_mode=mode;gl1_combine();}
static void gl1_constant(uint32_t value){Gl1Device *g=gl1_current;unsigned i;for(i=0;i<4;i++)g->constant[i]=(float)((value>>(i==3?24:16-i*8))&255)/255;gl1_combine();}
static void gl1_alpha_test(uint32_t v){gl1_current->alpha_function=v;if(gl1_gl_ready())gl1_alpha_state();}
static void gl1_alpha_reference(uint32_t v){gl1_current->alpha_reference=(v&255)/255.0f;if(gl1_gl_ready())gl1_alpha_state();}
static void gl1_chroma_mode(uint32_t v){gl1_current->chroma_enabled=v!=0;if(gl1_gl_ready())gl1_alpha_state();}
static void gl1_chroma_value(uint32_t v){gl1_current->chroma=v&0xffffff;}
static void gl1_filter(int min,int mag){
    Gl1Device *g=gl1_current;g->min_filter=min?GL_LINEAR:GL_NEAREST;g->mag_filter=mag?GL_LINEAR:GL_NEAREST;
    if(gl1_gl_ready() && g->bound){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,g->min_filter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,g->mag_filter);}
}
static void gl1_clamp(int s,int t){
    /* The preserved renderer already fixed this terrain seam: Edge clamping
     * repeats the edge texel; GL_CLAMP mixes in the border with linear filtering. */
    Gl1Device *g=gl1_current;
    g->wrap_s=s?GL_CLAMP_TO_EDGE:GL_REPEAT;g->wrap_t=t?GL_CLAMP_TO_EDGE:GL_REPEAT;
    if(gl1_gl_ready() && g->bound){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,g->wrap_s);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,g->wrap_t);}
}
static void gl1_palette_range(const uint32_t *pixels,uint32_t first,uint32_t last){
    Gl1Device *g=gl1_current;if(!pixels || first>last || last>=256){gl1_fail("palette range");return;}
    if(memcmp(g->palette+first,pixels+first,(last-first+1)*4)){memcpy(g->palette+first,pixels+first,(last-first+1)*4);++g->palette_generation;}
}
static void gl1_ncc_palette(const uint8_t pixels[112]){
    Gl1Device *g=gl1_current;uint32_t rgb[256];
    const unsigned table=0;if(!pixels){gl1_fail("texture table upload");return;}

    if(!asset_texture_ncc_palette((const uint8_t *)pixels,rgb)){gl1_fail("NCC table decode");return;}
    if(!g->ncc_loaded[table] || memcmp(g->ncc[table],rgb,sizeof(rgb))){
        memcpy(g->ncc[table],rgb,sizeof(rgb));++g->ncc_generation[table];g->ncc_loaded[table]=1;
    }
}
static void gl1_depth_mode(uint32_t v){gl1_current->depth_mode=v;if(gl1_gl_ready()){if(v)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);}}
static void gl1_depth_mask(uint32_t v){gl1_current->native_depth_write=v;if(gl1_gl_ready())glDepthMask(v?GL_TRUE:GL_FALSE);}
static void gl1_depth_func(uint32_t v){gl1_current->native_depth_func=v&7;if(gl1_gl_ready())glDepthFunc(gl1_compare(v));}
static void gl1_depth_bias(int32_t v){gl1_current->depth_bias=v;}
static void gl1_dither(uint32_t v){if(gl1_gl_ready()){if(v)glEnable(GL_DITHER);else glDisable(GL_DITHER);}}
static void gl1_cull(uint32_t v){gl1_current->native_cull=v;if(gl1_gl_ready()){if(v){glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(v==1?GL_CCW:GL_CW);}else glDisable(GL_CULL_FACE);}}
static void gl1_alpha_blend(int enabled){gl1_current->native_blend=enabled;if(gl1_gl_ready()){glEnable(GL_BLEND);glBlendFunc(enabled?GL_SRC_ALPHA:GL_ONE,enabled?GL_ONE_MINUS_SRC_ALPHA:GL_ZERO);}}
static void gl1_lod_bias(float v){gl1_current->lod_bias=v;}
static void gl1_fog_generate(uint8_t *table,float density){unsigned i;gl1_current->density=density;for(i=0;i<64;i++){float w=ldexpf(1+(i&3)*0.25f,i>>2),d=density*w;table[i]=(uint8_t)(255*(1-expf(-d*d)));}}
static void gl1_fog_color(uint32_t v){GLfloat rgba[4]={(float)((v>>16)&255)/255,(float)((v>>8)&255)/255,(float)(v&255)/255,1};memcpy(gl1_current->fog_color,rgba,sizeof(rgba));if(gl1_gl_ready())glFogfv(GL_FOG_COLOR,rgba);}
static void gl1_fog_table(const uint8_t *table){memcpy(gl1_current->fog,table,64);}
static void gl1_fog_mode(uint32_t v){
    Gl1Device *g=gl1_current;if(!gl1_ready())return;
    if(g->vulkan){g->native_fog=v?(g->sky_loaded?1:2):0;return;}
    if(g->sky_loaded){gl1_level_fog(v!=0);return;}
    if(v){uint32_t bits;glFogi(GL_FOG_COORDINATE_SOURCE,GL_FOG_COORDINATE);glFogi(GL_FOG_MODE,GL_EXP2);glFogf(GL_FOG_DENSITY,g->density);glEnable(GL_FOG);
        bits=game_memory_read_u32(&g->application->memory,0x6290c8);memcpy(&g->projection_center,&bits,4);
        bits=game_memory_read_u32(&g->application->memory,0x6290f4);memcpy(&g->projection_scale,&bits,4);
        if(!(g->projection_scale>1))g->projection_scale=381.3607f;
    }else glDisable(GL_FOG);
}
/*Hardware reject counters are unavailable through fixed-function OpenGL.
 *Keep those diagnostic lanes zero, rather than fabricate raster results. */
static void gl1_stats(uint32_t *out){memset(out,0,5*4);}
static void gl1_reset_stats(void){gl1_current->triangles=gl1_current->uploads=0;}
static void gl1_clear(uint32_t color,uint32_t alpha,uint32_t depth){
    VK1_DO(vk1_clear(color,alpha,depth));
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
static void gl1_swap(int32_t interval){
    VK1_DO(vk1_swap());
    Gl1Device *g=gl1_current;int width,height;unsigned old;(void)interval;
    if(!gl1_ready())return;
    if(SDL_GL_GetCurrentContext()!=g->context){gl1_fail("lost current context");return;}
    if(glGetError()!=GL_NO_ERROR){gl1_fail("OpenGL draw/state error");return;}
    gl1_resolve(g,g->back);old=g->front;g->front=g->back;g->back=old;
    glPushAttrib(GL_ALL_ATTRIB_BITS);g->bind_framebuffer(GL_FRAMEBUFFER,0);
    if(!SDL_GetWindowSizeInPixels(g->window,&width,&height)){gl1_fail("present client extent");glPopAttrib();return;}
    if(!game_display_present(g->application,g->colors[g->front],g->width,g->height,0))gl1_fail("display presentation");
    if(!SDL_GL_SwapWindow(g->window))gl1_fail("swap buffers");
    g->bind_framebuffer(GL_FRAMEBUFFER,gl1_render_target(g,g->back));glPopAttrib();glViewport(0,0,g->width,g->height);
}
static int32_t gl1_map_pixels(int write,unsigned buffer,RenderPixelView *info){
    VK1_RETURN(vk1_map(write,buffer,info));
    Gl1Device *g=gl1_current;unsigned x,y;
    if(!gl1_ready() || !info || g->locked || buffer>1)return 0;
    g->locked=1;g->write_lock=write!=0;g->lock_buffer=buffer;
    if(g->write_lock){for(x=0;x<g->width*g->height;x++)g->lfb[x]=0xf81f;}
    else{
        gl1_resolve(g,buffer==0?g->front:g->back);
        g->bind_framebuffer(GL_FRAMEBUFFER,g->framebuffers[buffer==0?g->front:g->back]);
        glReadBuffer(GL_COLOR_ATTACHMENT0);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,g->width,g->height,GL_RGBA,GL_UNSIGNED_BYTE,g->rgba);
        for(y=0;y<g->height;y++)for(x=0;x<g->width;x++){
            const uint8_t *p=g->rgba+((size_t)(g->height-y-1)*g->width+x)*4;
            g->lfb[(size_t)y*g->width+x]=(uint16_t)((p[0]>>3)<<11|(p[1]>>2)<<5|(p[2]>>3));
        }
        g->bind_framebuffer(GL_FRAMEBUFFER,gl1_render_target(g,g->back));
    }
    info->pixels=g->lfb;info->stride=g->width*2;return 1;
}
static int gl1_unmap_pixels(void){
    VK1_RETURN(vk1_unmap());
    Gl1Device *g=gl1_current;unsigned x,y,buffer=g?g->lock_buffer:0;
    if(!gl1_ready() || !g->locked)return 0;
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
        glPushAttrib(GL_ALL_ATTRIB_BITS);g->bind_framebuffer(GL_FRAMEBUFFER,gl1_render_target(g,buffer==0?g->front:g->back));
        glDisable(GL_MULTISAMPLE);
        /* One device-owned overlay replaces the driver-specific glDrawPixels
         * path. The existing bounded LFB owner and magenta transparency stay
         * authoritative; only this write's bounded rectangle is uploaded. */
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_FOG);glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,g->lfb_texture);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexSubImage2D(GL_TEXTURE_2D,0,left,g->height-bottom,width,bottom-top,GL_RGBA,GL_UNSIGNED_BYTE,g->rgba);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);gl1_screen_rect(left,top,right,bottom);
        glPopAttrib();g->bind_framebuffer(GL_FRAMEBUFFER,gl1_render_target(g,g->back));
    }
    g->locked=0;return 1;
}
static void gl1_close(void){
    Gl1Device *g=gl1_current;unsigned i;if(!g)return;
#ifdef UPRISING_EXPERIMENTAL_VULKAN
    if(g->vulkan)vk1_close();
#endif
    if(g->context){
        if(SDL_GL_GetCurrentContext()==g->context || SDL_GL_MakeCurrent(g->window,g->context)){
            for(i=0;i<4096;i++)if(g->textures[i].name)glDeleteTextures(1,&g->textures[i].name);
            gl1_msaa_release(g);
            if(g->delete_framebuffers)g->delete_framebuffers(2,g->framebuffers);
            if(g->delete_renderbuffers && g->depth)g->delete_renderbuffers(1,&g->depth);
            glDeleteTextures(2,g->colors);glDeleteTextures(3,g->sky_layers);
            if(g->lfb_texture)glDeleteTextures(1,&g->lfb_texture);
            if(g->remote_mask)glDeleteTextures(1,&g->remote_mask);
            if(g->context_state_saved){
                glMatrixMode(GL_TEXTURE);glPopMatrix();
                glMatrixMode(GL_MODELVIEW);glPopMatrix();
                glMatrixMode(GL_PROJECTION);glPopMatrix();
                glMatrixMode(g->previous_matrix_mode);
                glPopClientAttrib();glPopAttrib();
            }
        }
        /* The SDL window owns the context across menu/level transitions. */
        g->context=NULL;
    }
    g->context_state_saved=0;
    for(i=0;i<4096;i++){free(g->textures[i].pixels);memset(g->textures+i,0,sizeof(g->textures[i]));}
    memset(g->texture_index,0,sizeof(g->texture_index));
    free(g->lfb);free(g->rgba);g->lfb=NULL;g->rgba=NULL;g->bound=NULL;g->locked=0;
    memset(g->framebuffers,0,sizeof(g->framebuffers));memset(g->colors,0,sizeof(g->colors));g->depth=0;
    memset(g->sky_layers,0,sizeof(g->sky_layers));g->sky_name[0]=0;
    g->sky_loaded=g->sky_active=g->fog_pending=0;
    memset(g->ncc_loaded,0,sizeof(g->ncc_loaded));g->ncc_selected=0;
    g->remote_mask=g->lfb_texture=0;
}
static void gl1_shutdown(void){gl1_close();free(gl1_current);gl1_current=NULL;}
static int32_t gl1_open(SDL_Window *window){
    Gl1Device *g=gl1_current;unsigned i;
    if(!g || g->context || g->vulkan || !window)return 0;
#ifdef UPRISING_EXPERIMENTAL_VULKAN
    if(game_menu_vulkan(g->application))return vk1_open(window,game_menu_vulkan(g->application));
#endif

    g->failed=0;g->window=window;g->width=640;g->height=480;g->front=0;g->back=1;
    g->context=game_menu_gl_context(g->application);
    if(!g->context || !SDL_GL_MakeCurrent(window,g->context))goto failed;
    /* A level borrows the window's context. Preserve the menu's GL state so
     * the next level starts from the same state as the first, including depth
     * writes and pixel-store settings used during framebuffer initialization. */
    glGetIntegerv(GL_MATRIX_MODE,&g->previous_matrix_mode);
    glPushAttrib(GL_ALL_ATTRIB_BITS);glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);glPushMatrix();
    glMatrixMode(GL_MODELVIEW);glPushMatrix();
    glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);g->context_state_saved=1;
    g->thread=SDL_GetCurrentThreadID();
    /* The established renderer is paced by its game clock, not vsync. */
    SDL_GL_SetSwapInterval(0);
#define GL1_EXTENSION(field,core,ext) do{SDL_FunctionPointer entry=gl1_extension(core,ext);if(!entry)goto failed;memcpy(&g->field,&entry,sizeof(entry));}while(0)
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
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,g->width,g->height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        g->framebuffer_texture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,g->colors[i],0);
        g->framebuffer_renderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,g->depth);
        if(g->check_framebuffer(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)goto failed;
        glClearColor(0,0,0,0);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    }
    gl1_msaa_open(g);
    g->bind_framebuffer(GL_FRAMEBUFFER,gl1_render_target(g,g->back));glViewport(0,0,g->width,g->height);
    glDisable(GL_MULTISAMPLE);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,g->width,g->height,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_DEPTH_TEST);glShadeModel(GL_SMOOTH);
    g->palette_generation=1;g->min_filter=g->mag_filter=GL_LINEAR;g->wrap_s=g->wrap_t=GL_REPEAT;
    g->constant[0]=g->constant[1]=g->constant[2]=g->constant[3]=1;g->alpha_function=7;
    g->projection_center=320;g->projection_scale=381.3607f;
    game_application_log(g->application,"native OpenGL1 device: %s\n",(const char *)glGetString(GL_RENDERER));
    if(glGetError()!=GL_NO_ERROR)goto failed;return 1;
failed:gl1_fail("window/context/framebuffer initialization");gl1_close();return 0;
}
static int gl1_create(GameApplication *a,PortablePlatform *p){
    if(gl1_current)return 0;
    gl1_current=calloc(1,sizeof(*gl1_current));if(!gl1_current)return 0;
    gl1_current->application=a;p->graphics_owner=gl1_current;
    p->renderer_initialized=1;
    game_application_log(a,"%s\n","NATIVE_OPENGL1_CPU_FREE: native fixed-function device; direct calls and texture identities");return 1;
}
#ifdef UPRISING_EXPERIMENTAL_VULKAN
#include "portable_vk.inc"
#endif
