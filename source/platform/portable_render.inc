/* Native projected renderer state, texture decoding and platform ownership.
 * Vulkan owns executable graphics behavior; no GL context or fallback exists. */
#include <math.h>
#include "vulkan_menu.h"
#include "vulkan_draw.h"
#include "vulkan_temporal.h"
#include "fsr3_upscaler.h"
enum {RENDER_SAMPLE_NEAREST=0,RENDER_SAMPLE_LINEAR=1,RENDER_WRAP_REPEAT=0,RENDER_WRAP_CLAMP=1};
typedef struct RenderTexture {
    uint32_t key,bytes,format,palette_generation,chroma;
    uint32_t ncc_generation,ncc_table;
    int dirty,chroma_enabled;
    int first_lod,aspect;
    unsigned width,height;
    unsigned gpu_width,gpu_height;
    uint8_t *pixels;
    struct RenderTexture *next;
    void *vulkan_texture;
    uint8_t *vulkan_pixels;
    unsigned native_tile;int ui_tile;
} RenderTexture;
typedef struct RenderDevice {
    GameApplication *application;
    SDL_Window *window;SDL_ThreadID thread;
    unsigned front,back,width,height;
    RenderTexture textures[4096],*texture_index[2048],*bound;
    uint32_t palette[256],palette_generation,chroma;
    uint32_t ncc[2][256],ncc_generation[2],ncc_selected;
    int ncc_loaded[2];
    RenderColorMode color_mode;RenderAlphaMode alpha_mode;
    uint32_t depth_mode,alpha_function;
    float constant[4],alpha_reference,density;
    int min_filter,mag_filter,wrap_s,wrap_t;
    int chroma_enabled,textured,constant_enabled,failed,locked,write_lock;
    int32_t depth_bias;
    uint8_t fog[64],*rgba;
    uint16_t *lfb;
    uint32_t lock_buffer;
    float projection_center,projection_scale;
    uint64_t triangles,uploads;
    char sky_name[128];
    unsigned sky_layers[3];
    unsigned sky_triangles;
    uint32_t sky_tick;
    int sky_loaded,sky_active,fog_pending;
    float sky_yaw,sky_horizon,sky_yaw_scale,sky_offset,sky_depth;
    float fog_color[4];
    unsigned remote_mask;
    int32_t remote_bounds[4];
    struct Vk1Device *vulkan;
    uint32_t native_depth_write,native_depth_func,native_cull;
    int native_blend,native_fog;
    int world_depth_capture;
    const GameRenderVertex *motion_vertices;
    const uint32_t *motion_references;
    unsigned motion_count;
    float native_fog_start,native_fog_end;
} RenderDevice;

static RenderDevice *render_current;
static int render_ready(void);
static void render_fail(const char *);
static int vk1_open(SDL_Window *,VulkanMenu *);
static void vk1_close(void),vk1_texture_release(RenderTexture *),vk1_refresh(RenderTexture *);
static void vk1_triangle(const GameRenderVertex *,const GameRenderVertex *,const GameRenderVertex *);
static void vk1_line(const GameRenderVertex *,const GameRenderVertex *),vk1_point(const GameRenderVertex *);
static void vk1_clear(uint32_t,uint32_t,uint32_t),vk1_swap(void);
static int vk1_map(int,unsigned,RenderPixelView *),vk1_unmap(void);
static int vk1_sky_upload(unsigned,unsigned,unsigned),vk1_sky_end(void);
static void vk1_sky_release(void);
static int vk1_remote_read(const int32_t[4]),vk1_remote_upload(unsigned,unsigned),vk1_remote_sky(const GameRemoteSkyFrame *);

static void render_close(void),render_combine(void);
static int render_texture_stale(const RenderTexture *);

static void render_combine(void){RenderDevice *g=render_current;if(!render_ready())return;g->textured=g->color_mode==RENDER_COLOR_TEXTURE || g->color_mode==RENDER_COLOR_INVERSE_TEXTURE;g->constant_enabled=g->color_mode==RENDER_COLOR_CONSTANT;}
static unsigned render_texture_bucket(uint32_t key){return (key^(key>>12))&2047u;}
static RenderTexture *render_find(uint32_t key){
    RenderDevice *g=render_current;RenderTexture *t;
    for(t=g->texture_index[render_texture_bucket(key)];t;t=t->next)
        if(t->key==key)return t;
    return NULL;
}
static void render_release_texture(uint32_t key){
 RenderDevice *g=render_current;RenderTexture *t,**link;if(!g)return;
 link=&g->texture_index[render_texture_bucket(key)];
 while((t=*link)!=NULL){if(t->key==key){*link=t->next;if(g->bound==t)g->bound=NULL;vk1_texture_release(t);free(t->pixels);memset(t,0,sizeof(*t));return;}link=&t->next;}
}
static int render_texture_stale(const RenderTexture *t){RenderDevice *g=render_current;return t->dirty || !t->gpu_width || ((t->format==5 || t->format==14) && t->palette_generation!=g->palette_generation) || ((t->format==1 || t->format==9) && (t->ncc_table!=g->ncc_selected || t->ncc_generation!=g->ncc_generation[g->ncc_selected]));}
static void render_source(uint32_t address){RenderDevice *g=render_current;RenderTexture *t;if(!render_ready())return;t=render_find(address);if(!t){render_fail("binding absent texture upload");return;}g->bound=t;}
static void render_alpha_test(uint32_t v){render_current->alpha_function=v;}
static void render_alpha_reference(uint32_t v){render_current->alpha_reference=(v&255)/255.0f;}
static void render_chroma_mode(uint32_t v){render_current->chroma_enabled=v!=0;}
static void render_filter(int min,int mag){render_current->min_filter=min?RENDER_SAMPLE_LINEAR:RENDER_SAMPLE_NEAREST;render_current->mag_filter=mag?RENDER_SAMPLE_LINEAR:RENDER_SAMPLE_NEAREST;}
static void render_clamp(int s,int t){render_current->wrap_s=s?RENDER_WRAP_CLAMP:RENDER_WRAP_REPEAT;render_current->wrap_t=t?RENDER_WRAP_CLAMP:RENDER_WRAP_REPEAT;}
static void render_depth_mode(uint32_t v){render_current->depth_mode=v;}
static void render_depth_mask(uint32_t v){render_current->native_depth_write=v;}
static void render_depth_func(uint32_t v){render_current->native_depth_func=v&7;}
static void render_cull(uint32_t v){render_current->native_cull=v;}
static void render_alpha_blend(int enabled){render_current->native_blend=enabled;}
static void render_fog_color(uint32_t v){float rgba[4]={(float)((v>>16)&255)/255,(float)((v>>8)&255)/255,(float)(v&255)/255,1};memcpy(render_current->fog_color,rgba,sizeof(rgba));}
static void render_fog_mode(uint32_t v){RenderDevice *g=render_current;if(render_ready())g->native_fog=v?(g->sky_loaded?1:2):0;}
static void render_close(void){
 RenderDevice *g=render_current;if(!g)return;if(g->vulkan)vk1_close();
 for(unsigned i=0;i<4096;i++){free(g->textures[i].pixels);memset(g->textures+i,0,sizeof(g->textures[i]));}
 memset(g->texture_index,0,sizeof(g->texture_index));free(g->lfb);free(g->rgba);g->lfb=NULL;g->rgba=NULL;g->bound=NULL;g->locked=0;
 memset(g->sky_layers,0,sizeof(g->sky_layers));g->sky_name[0]=0;g->sky_loaded=g->sky_active=g->fog_pending=0;
 memset(g->ncc_loaded,0,sizeof(g->ncc_loaded));g->ncc_selected=0;g->remote_mask=0;g->world_depth_capture=0;
}

static void render_fail(const char *reason){
    RenderDevice *g=render_current;if(!g || g->failed)return;
    g->failed=1;game_application_log(g->application,"native renderer failure: %s\n",reason);
}
static int render_ready(void){
    RenderDevice *g=render_current;
    if(!g || !g->vulkan || g->failed)return 0;
    if(g->thread!=SDL_GetCurrentThreadID()){render_fail("wrong renderer thread");return 0;}
    /* The platform owns one renderer on the window thread. */
    return 1;
}
static void render_dimensions(int lod,int aspect,unsigned *width,unsigned *height){
    unsigned largest=256u>>(unsigned)lod;
    *width=aspect<=3?largest:largest>>(aspect-3);
    *height=aspect>=3?largest:largest>>(3-aspect);
    if(!*width)*width=1;if(!*height)*height=1;
}
static uint32_t render_texture_bytes(const RenderTextureView *info){
    uint32_t bytes=0;int first,last,lod;
    if(!info || info->small_lod<0 || info->small_lod>8 || info->large_lod<0 ||
       info->large_lod>8 || info->aspect<0 || info->aspect>6 || info->format>14)return 0;
    first=info->small_lod<info->large_lod?info->small_lod:info->large_lod;
    last=info->small_lod>info->large_lod?info->small_lod:info->large_lod;
    for(lod=first;lod<=last;lod++){
        unsigned w,h;render_dimensions(lod,info->aspect,&w,&h);
        bytes+=(w*h*(info->format>=8?2u:1u)+7u)&~7u;
    }
    return bytes;
}
static uint16_t render_w_depth(float oow){
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


static void render_release_textures(void){
    RenderDevice *g=render_current;unsigned i;if(!g)return;
    for(i=0;i<4096;i++)if(g->textures[i].pixels)render_release_texture(g->textures[i].key);
}
static void render_convert(const RenderTexture *texture,const uint8_t *input,unsigned count,uint8_t *out){
    RenderDevice *g=render_current;unsigned i;
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
        default:render_fail("unsupported uploaded texture format");break;
        }
        out[i*4]=(uint8_t)(rgb>>16);out[i*4+1]=(uint8_t)(rgb>>8);out[i*4+2]=(uint8_t)rgb;
        out[i*4+3]=(uint8_t)(g->chroma_enabled && (rgb&0xffffff)==g->chroma?0:alpha);
    }
}
static void render_upload(uint32_t address,const RenderTextureView *info){
    RenderDevice *g=render_current;RenderTexture *texture;uint32_t bytes;unsigned i;
    if(!render_ready())return;
    bytes=render_texture_bytes(info);
    if(!bytes || !info->pixels || !address){render_fail("texture upload extent");return;}
    texture=render_find(address);
    if(!texture){for(i=0;i<4096;i++)if(!g->textures[i].pixels){texture=g->textures+i;break;}}
    if(!texture){render_fail("texture staging capacity");return;}
    if(texture->bytes!=bytes){uint8_t *pixels=realloc(texture->pixels,bytes);if(!pixels){render_fail("texture staging allocation");return;}texture->pixels=pixels;}
    if(!texture->key){unsigned bucket=render_texture_bucket(address);texture->next=g->texture_index[bucket];g->texture_index[bucket]=texture;}
    memcpy(texture->pixels,info->pixels,bytes);texture->key=address;texture->bytes=bytes;texture->format=info->format;texture->aspect=info->aspect;
    texture->first_lod=info->small_lod<info->large_lod?info->small_lod:info->large_lod;
    render_dimensions(texture->first_lod,texture->aspect,&texture->width,&texture->height);
    /* Decode at submission, after the caller has selected this texture's
     * palette/NCC/chroma state. Uploading here converts the previous material. */
    texture->dirty=1;++g->uploads;
    texture->ui_tile=0;
}
static void render_ui_tile(uint32_t address){RenderTexture *texture=render_find(address);if(texture && texture->width==32 && texture->height==32 && texture->format==5)texture->ui_tile=1;}
static int render_draw_ready(void){
    RenderDevice *g=render_current;
    if(!render_ready())return 0;
    if(g->textured && g->bound){
        if((g->bound->format==1 || g->bound->format==9) && !g->ncc_loaded[g->ncc_selected]){
            render_fail("compressed texture has no NCC table");return 0;
        }
        if(render_texture_stale(g->bound))vk1_refresh(g->bound);
    }
    return 1;
}
static void render_color_mode(RenderColorMode mode){render_current->color_mode=mode;render_combine();}
static void render_alpha_mode(RenderAlphaMode mode){render_current->alpha_mode=mode;render_combine();}
static void render_constant(uint32_t value){RenderDevice *g=render_current;unsigned i;for(i=0;i<4;i++)g->constant[i]=(float)((value>>(i==3?24:16-i*8))&255)/255;render_combine();}
static void render_chroma_value(uint32_t v){render_current->chroma=v&0xffffff;}
static void render_palette_range(const uint32_t *pixels,uint32_t first,uint32_t last){
    RenderDevice *g=render_current;if(!pixels || first>last || last>=256){render_fail("palette range");return;}
    if(memcmp(g->palette+first,pixels+first,(last-first+1)*4)){memcpy(g->palette+first,pixels+first,(last-first+1)*4);++g->palette_generation;}
}
static void render_ncc_palette(const uint8_t pixels[112]){
    RenderDevice *g=render_current;uint32_t rgb[256];
    const unsigned table=0;if(!pixels){render_fail("texture table upload");return;}

    if(!asset_texture_ncc_palette((const uint8_t *)pixels,rgb)){render_fail("NCC table decode");return;}
    if(!g->ncc_loaded[table] || memcmp(g->ncc[table],rgb,sizeof(rgb))){
        memcpy(g->ncc[table],rgb,sizeof(rgb));++g->ncc_generation[table];g->ncc_loaded[table]=1;
    }
}
static void render_depth_bias(int32_t v){render_current->depth_bias=v;}
static void render_fog_generate(uint8_t *table,float density){unsigned i;render_current->density=density;for(i=0;i<64;i++){float w=ldexpf(1+(i&3)*0.25f,i>>2),d=density*w;table[i]=(uint8_t)(255*(1-expf(-d*d)));}}
static void render_fog_table(const uint8_t *table){memcpy(render_current->fog,table,64);}
static void render_stats(uint32_t *out){memset(out,0,5*4);}
static void render_reset_stats(void){render_current->triangles=render_current->uploads=0;}
static void render_shutdown(void){render_close();free(render_current);render_current=NULL;}
static int render_create(GameApplication *a,PortablePlatform *p){
    if(render_current)return 0;
    render_current=calloc(1,sizeof(*render_current));if(!render_current)return 0;
    render_current->application=a;p->graphics_owner=render_current;
    p->renderer_initialized=1;
    game_application_log(a,"%s\n","NATIVE_VULKAN_CPU_FREE: native projected device; direct calls and texture identities");return 1;
}
#include "portable_render_sky.inc"
static void render_triangle(const GameRenderVertex *a,const GameRenderVertex *b,const GameRenderVertex *c){
    if(!render_draw_ready())return;
    if(render_current->sky_active){render_sky_triangle(a,b,c);return;}
    vk1_triangle(a,b,c);
}
static int render_world_point(const GameRenderVertex *a){
    RenderDevice *g=render_current;GameRenderVertex vertex=*a;int textured,constant;
    if(!render_ready())return 0;
    /* Captured world points retain the original palette's byte order (BGR).
     * Decode it here, like palette textures and HUD points, without changing
     * the original-comparison geometry. Palette particles have no UV: do not
     * sample the preceding building material or its chroma/constant color. */
    vertex.r=a->b;vertex.b=a->r;
    textured=g->textured;constant=g->constant_enabled;
    g->textured=g->constant_enabled=0;vk1_point(&vertex);g->textured=textured;g->constant_enabled=constant;return !g->failed;
}
#include "portable_vk.inc"
