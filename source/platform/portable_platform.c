#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "recyclable_records.h"

#include "portable_input_devices.h"
#include "portable_sdl.h"
#include "portable_display.h"
#include "host_file.h"
#include <SDL3/SDL_opengl.h>
#include "game_initial_data.h"
#include "application_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "application_entry.h"
#include "application_archives.h"
#include "menu_widgets.h"
#include "menu_movie.h"
#include "menu_settings.h"
#include "sound_platform.h"
#include "scene_projection_storage.h"
#include "presentation_instance.h"
#include "runtime_node_pool.h"
#include "application_name_registry.h"
#include "level_entity_store.h"
#include "gameplay_spatial.h"
#include "projection_owner_pages.h"
#include "render_arena.h"
#include "render_runtime_pools.h"
#include "render_texture_cache.h"
#include "level_load.h"
#include "screen_primitives.h"
#include "render_motion.h"
#include "ui_tiled_image.h"
#include "radar_compositor.h"
#include "model_assets.h"
#include "scene_surface.h"
#include "level_world_surface.h"
#include "level_world_render.h"
#include "level_scene_render.h"
#include "game_text_rasterizer.h"
#include "ui_geometry.h"
/* CRT-only durability/cwd adapters. File operations and paths use SDL; game
 * asset readers retain their existing FILE ownership and shared working root. */
static int asset_directory(const char *path){
#ifdef _WIN32
    wchar_t *wide=(wchar_t *)SDL_iconv_string("UTF-16LE","UTF-8",path,strlen(path)+1);
    int ok=wide && !_wchdir(wide);SDL_free(wide);return ok;
#else
    return !chdir(path);
#endif
}
static int durable_file(FILE *file){
    if(fflush(file))return 0;
#ifdef _WIN32
    return !_commit(_fileno(file));
#else
    return !fsync(fileno(file));
#endif
}

typedef enum RenderColorMode { RENDER_COLOR_VERTEX,RENDER_COLOR_CONSTANT,RENDER_COLOR_TEXTURE,RENDER_COLOR_INVERSE_TEXTURE } RenderColorMode;
typedef enum RenderAlphaMode { RENDER_ALPHA_ZERO,RENDER_ALPHA_VERTEX,RENDER_ALPHA_TEXTURE,RENDER_ALPHA_CONSTANT } RenderAlphaMode;
typedef struct RenderTextureView {
    uint32_t small_lod,large_lod,aspect,format;
    void *pixels;
} RenderTextureView;

typedef struct ArchiveFile {ResourceFile *file;GameAddress identity;struct ArchiveFile *next;} ArchiveFile;
/*Mapped RGB565 pixels are borrowed only until unmap; pointers stay native. */
typedef struct RenderPixelView {void *pixels;uint32_t stride;} RenderPixelView;
typedef struct PortablePlatform {
    int isolated_input;
    char *executable;
    FILE *log;
    char *user_directory;
    GameConfig *configuration;
    char configuration_path[4128];
    int configuration_pending;
    int configuration_ready;
    int renderer_initialized;
    int window_opened;
    int text_pixels_locked;
    int capture_attempted;
    int screen_mask_active;
    ArchiveFile *archives;
    uint32_t archive_sequence;
    void *graphics_owner;
} PortablePlatform;

#include "portable_gl1.inc"

int game_application_capture_png(GameApplication *app,const char *path){
    PortablePlatform *p=app->platform;RenderPixelView pixels={0};SDL_Surface *image;int ok;
    if(!p || !path)return 0;
    if(!p->window_opened)return game_menu_capture_png(app,path);
    if(!gl1_map_pixels(0,0,&pixels))return 0;
    image=SDL_CreateSurfaceFrom((int)gl1_current->width,(int)gl1_current->height,
        SDL_PIXELFORMAT_RGB565,pixels.pixels,(int)pixels.stride);
    ok=image && SDL_SavePNG(image,path);
    SDL_DestroySurface(image);
    if(!gl1_unmap_pixels())ok=0;
    if(!ok)game_application_log(app,"PNG screenshot failed: %s\n",SDL_GetError());
    return ok;
}

/* Native file operations share the application's isolated user-data owner.
 * Directory enumeration uses that same saves directory as archive loading. */
int game_application_file_path(GameApplication *app,const char *name,int write,char *path,size_t capacity) {
    PortablePlatform *p=app->platform;const char *segment;int size;
    if(!name || !*name || !path || !capacity || name[0]=='/' || name[0]=='\\' || strchr(name,':'))return 0;
    for(segment=name;*segment;) {
        const char *end=segment;while(*end && *end!='/' && *end!='\\')++end;
        if(end-segment==2 && segment[0]=='.' && segment[1]=='.')return 0;
        segment=*end?end+1:end;
    }
    if(write && (!p || !p->user_directory))return 0;
    if(p && p->user_directory && (write || !strncmp(name,"saves\\",6) || !strncmp(name,"saves/",6)))
        size=snprintf(path,capacity,"%s/%s",p->user_directory,name);
    else size=snprintf(path,capacity,"%s",name);
    if(size<0 || (size_t)size>=capacity)return 0;
    {size_t i;for(i=0;path[i];++i)if(path[i]=='\\')path[i]='/';}
    return 1;
}
int game_application_file_delete(GameApplication *app,const char *name) {
    char path[4128];return game_application_file_path(app,name,1,path,sizeof(path)) && SDL_RemovePath(path);
}
/*00531980/CopyFileA: preserve overwrite and the caller's ignored failure.
 * All streams remain native full-width objects and close on every exit. */
int game_application_file_copy(GameApplication *app,const char *name,const char *destination) {
    GameAddress id=game_application_archive_open(app,name,0);ResourceFile *source,*target;char path[4128];uint8_t bytes[8192];size_t count;int ok=1;
    if(!id)return 0;
    source=game_application_archive_file(app,id);
    if(!game_application_file_path(app,destination,1,path,sizeof(path))){game_application_archive_close(app,id);return 0;}
    target=resource_file_open_mode(path,"wb");
    if(!target){game_application_archive_close(app,id);return 0;}
    while((count=resource_file_read(source,bytes,sizeof(bytes)))!=0)
        if(resource_file_write(target,bytes,count)!=count){ok=0;break;}
    if(!resource_file_flush(target))ok=0;
    resource_file_close(target);game_application_archive_close(app,id);return ok;
}
ResourceFile *game_application_archive_file(GameApplication *app,GameAddress id) {
    PortablePlatform *p=app->platform;ArchiveFile *entry;
    if(!p)return NULL;
    for(entry=p->archives;entry;entry=entry->next)if(entry->identity==id)return entry->file;
    return NULL;
}
int game_application_archive_close(GameApplication *app,GameAddress id) {
    PortablePlatform *p=app->platform;ArchiveFile **link,*entry;
    if(!p)return 0;
    for(link=&p->archives;*link;link=&(*link)->next)if((*link)->identity==id) {
        entry=*link;*link=entry->next;resource_file_close(entry->file);free(entry);return 1;
    }
    return 0;
}
GameAddress game_application_archive_open(GameApplication *app,const char *name,int write) {
    PortablePlatform *p=app->platform;ResourceFile *file=NULL;ArchiveFile *entry;char path[4128];const char *segment;
    if(!p || !name || !*name || p->archive_sequence>=0xffffffu)return 0;
    if(write) {
        if(!p->user_directory || name[0]=='/' || name[0]=='\\' || strchr(name,':'))return 0;
        for(segment=name;*segment;) {
            const char *end=segment;while(*end && *end!='/' && *end!='\\')++end;
            if(end-segment==2 && segment[0]=='.' && segment[1]=='.')return 0;
            segment=*end?end+1:end;
        }
    }
    if(p->user_directory) {
        int size=snprintf(path,sizeof(path),"%s/%s",p->user_directory,name);
        if(size<0 || (size_t)size>=sizeof(path))return 0;
        file=resource_file_open_mode(path,write?"wb":"rb");
    }
    if(!file && !write)file=resource_file_open(name);
    if(!file)return 0;
    entry=malloc(sizeof(*entry));if(!entry){resource_file_close(file);return 0;}
    entry->file=file;entry->identity=0xf7000000u+(++p->archive_sequence);
    entry->next=p->archives;p->archives=entry;return entry->identity;
}

int game_application_ui_begin(GameApplication *app) {
    PortablePlatform *p=app->platform;
    if(!p || p->renderer_initialized)return 0;
    /*Rendering requires an explicitly windowed native display. */
    if(SDL_getenv("UPRISING_RECOMP_HEADLESS")){
        game_application_log(app,"%s\n","HEADLESS_NATIVE_DISPLAY_DISABLED: windowed rendering required");
        return 0;
    }
    if(!SDL_getenv("UPRISING_RECOMP_WINDOWED"))return 0;
    return gl1_create(app,p);
}

/*439930: SDL-owned context and native fog/texture setup. */
int game_application_level_graphics_begin(GameApplication *a) {
    PortablePlatform *p=a->platform;uint8_t fog[64];uint32_t palette[256];
    float density;uint32_t bits=0x37fba882;unsigned i;char diagnostic[160];
    if(SDL_getenv("UPRISING_RECOMP_HEADLESS")){
        game_application_log(a,"%s\n","HEADLESS_NATIVE_DISPLAY_DISABLED: windowed rendering required");
        return 0;
    }
    if(!p || !p->renderer_initialized || a->memory.fault || p->window_opened){
        snprintf(diagnostic,sizeof(diagnostic),"native graphics initialization rejected: initialized=%d opened=%d fault=%d",p?p->renderer_initialized:0,p?p->window_opened:0,a->memory.fault);
        game_application_log(a,"%s\n",diagnostic);return 0;
    }
    /* Borrow the explicit SDL owner; foreground discovery is unnecessary. */
    if(!gl1_open(game_menu_sdl_window(a))){
        snprintf(diagnostic,sizeof(diagnostic),"native OpenGL window open failed: owner=%p",(void*)game_menu_sdl_window(a));game_application_log(a,"%s\n",diagnostic);return 0;
    }
    p->window_opened=1;p->capture_attempted=0;memcpy(&density,&bits,sizeof(density));
    gl1_fog_generate(fog,density);
    if(!game_memory_write(&a->memory,0x5ad348,fog,sizeof(fog)))goto fail;
    gl1_fog_color(0x83b9be);gl1_fog_table(fog);
    gl1_filter(1,1);gl1_lod_bias(0.5f);
    gl1_chroma_mode(1);gl1_chroma_value(0xff00ff);gl1_alpha_reference(255);
    /*4176e0 immediately uploads the existing P8 palette after opening. */
    for(i=0;i<256;++i)palette[i]=game_memory_read_u32(&a->memory,0x5ad3a8+i*4);
    if(a->memory.fault)goto fail;
    gl1_palette_range(palette,0,255);
    if(!game_menu_game_palette(a,palette))goto fail;
    return 1;
fail:gl1_close();p->window_opened=0;return 0;
}
/*4399b0, also used when the platform owner itself is retired. */
int game_application_level_graphics_end(GameApplication *a) {
    PortablePlatform *p=a->platform;if(!p)return 1;
    if(p->text_pixels_locked){gl1_unmap_pixels();p->text_pixels_locked=0;}
    if(p->window_opened){gl1_close();p->window_opened=0;}
    return 1;
}
int game_level_screen_clear(GameApplication *a,int enabled) {
    PortablePlatform *p=a->platform;
    if(enabled || !p || !p->window_opened )return 0;
    gl1_clear(0x83b9be,0,0xffff);return 1;
}
int game_level_screen_present(GameApplication *a,uint32_t interval) {
    PortablePlatform *p=a->platform;int32_t signed_interval;
    /*Do not enter a backend's nested message pump after this frame requested
     *reload/termination. The level loop retires the existing display owner. */
    if(game_memory_read_u32(&a->memory,0x5ac970) || game_memory_read_u32(&a->memory,0x58360c) ||
       game_memory_read_u32(&a->memory,0x59d4c8))return !a->memory.fault;
    if(!p || !p->window_opened )return 0;
    memcpy(&signed_interval,&interval,4);gl1_swap(signed_interval);
    if(!gl1_current || gl1_current->failed)return 0;
    return 1;
}
/*4d7680 SDK state is isolated here; the portable caller owns depth geometry. */
int game_application_frame_depth_state(GameApplication *a,int enabled){
    PortablePlatform *p=a->platform;
    if(!p || !p->window_opened )return 0;
    if(!enabled){

        gl1_depth_mode(2);gl1_depth_mask(1);gl1_clear(0x83b9be,0,0xffff);return 1;
    }

    gl1_alpha_blend(0);gl1_alpha_mode(RENDER_ALPHA_ZERO);
    gl1_fog_mode(0);gl1_cull(0);
    /* The SDK's compare-to-bias path stores the comparison bias as depth,
     * leaving the whole viewport at near depth. Clear the original viewport
     * quad through ordinary W writes; world_begin restores normal testing.
     * Exact preservation of preexisting zero-depth pixels remains unproved. */
    gl1_depth_bias(0);p->screen_mask_active=1;
    gl1_depth_mode(2);gl1_depth_func(7);gl1_dither(0);gl1_depth_mask(1);
    gl1_color_mode(RENDER_COLOR_CONSTANT);gl1_constant(0x83b9be);return 1;
}
extern int game_menu_level_surface_flip(GameApplication *);
/* Opt-in evidence from the actual front buffer. Pixel views are borrowed only
 * until unmap; capture does not own another presentation surface. */
static void capture_game_frame(GameApplication *a,unsigned buffer){
    PortablePlatform *p=a->platform;const char *directory=SDL_getenv("UPRISING_RECOMP_CAPTURE_DIR");
    char path[4128];RenderPixelView view={0};SDL_Surface *borrowed=NULL,*image=NULL;
    uint32_t width,height;int ok=0,locked=0;
    if(p->capture_attempted)return;
    p->capture_attempted=1;if(!directory || !*directory)return;
    width=game_memory_read_u32(&a->memory,0x627928);height=game_memory_read_u32(&a->memory,0x62792c);
    if(a->memory.fault || !width || !height || width>16384 || height>16384)goto done;
    if(!gl1_map_pixels(0,buffer,&view))goto done;
    locked=1;if(!view.pixels || view.stride<width*2 || view.stride>INT32_MAX)goto done;
    borrowed=SDL_CreateSurfaceFrom((int)width,(int)height,SDL_PIXELFORMAT_RGB565,view.pixels,(int)view.stride);
    if(!borrowed || !(image=SDL_ConvertSurface(borrowed,SDL_PIXELFORMAT_RGB24)))goto done;
    if(snprintf(path,sizeof(path),"%s/native-%s-frame.bmp",directory,buffer?"before-text":"game")>=(int)sizeof(path))goto done;
    ok=SDL_SaveBMP(image,path);
done:
    SDL_DestroySurface(image);SDL_DestroySurface(borrowed);
    if(locked && !gl1_unmap_pixels())ok=0;
    game_application_log(a,"native frame capture: %s\n",ok?"saved":"FAILED");
}/*5189a0: present the active native display lane and capture its front buffer. */
int game_application_frame_present(GameApplication *a){
    if(!game_level_screen_present(a,1) || !game_menu_level_surface_flip(a))return 0;
    capture_game_frame(a,0);return 1;
}
extern int game_menu_accelerated_text_begin(GameApplication *,GameAddress,const GameUiRect *,const GameUiRect *,GameTextTarget *,int32_t *);
extern int game_menu_accelerated_text_end(GameApplication *);
/*532bf0: borrow the actual backend pixels for exactly one glyph operation. */
int game_application_glyph_begin(GameApplication *a,GameTextTarget *target,int32_t *fallback){
    PortablePlatform *p=a->platform;GameMemory *m=&a->memory;RenderPixelView info={0};
    uint32_t height=game_memory_read_u32(m,0x62792c);
    if(p && SDL_getenv("UPRISING_RECOMP_CAPTURE_BEFORE_TEXT"))capture_game_frame(a,1);
    if(!game_memory_read_u32(m,0x5840f0)){
        GameUiRect clip;uint32_t v;int32_t *fields[4]={&clip.left,&clip.top,&clip.right,&clip.bottom};
        static const GameAddress addresses[4]={0x59d450,0x59d458,0x59d454,0x59d45c};unsigned i;
        for(i=0;i<4;i++){v=game_memory_read_u32(m,addresses[i]);memcpy(fields[i],&v,4);}
        return !m->fault && game_menu_accelerated_text_begin(a,0,&clip,&clip,target,fallback);
    }
    if(m->fault || !target || !fallback || !p || !p->window_opened || p->text_pixels_locked ){
        fprintf(stderr,"native text backend unavailable: fault=%u window=%d locked=%d\n",
            m->fault,p?p->window_opened:0,p?p->text_pixels_locked:0);return 0;
    }
    gl1_depth_mode(0);gl1_depth_mask(0);
    if(!gl1_map_pixels(1,1,&info)){fprintf(stderr,"native text backend: LFB lock rejected\n");goto fail;}
    p->text_pixels_locked=1;
    if(!info.pixels || !info.stride || info.stride>INT32_MAX || (info.stride&1) || !height ||
       (uint64_t)info.stride*height>SIZE_MAX){fprintf(stderr,"native text backend: invalid LFB pixels=%p stride=%u height=%u\n",info.pixels,info.stride,height);goto fail;}
    memset(target,0,sizeof(*target));target->pixels=info.pixels;target->pixel_bytes=(size_t)info.stride*height;
    target->stride=(int32_t)(info.stride/2);target->rgb565=1;
#ifdef UPRISING_EXPERIMENTAL_VULKAN
    if(gl1_current->vulkan){
        Vk1Device *v=gl1_current->vulkan;
        v->written_range[0]=UINT32_MAX;v->written_range[1]=0;v->track_text=1;
        target->written_range=v->written_range;
    }
#endif
    *fallback=20;return 1;
fail:
    if(p->text_pixels_locked){gl1_unmap_pixels();p->text_pixels_locked=0;}
    gl1_depth_mode(2);gl1_depth_func(3);gl1_depth_mask(1);return 0;
}
int game_application_glyph_end(GameApplication *a){
    PortablePlatform *p=a->platform;int ok;
    if(p && p->text_pixels_locked){
        ok=gl1_unmap_pixels()!=0;p->text_pixels_locked=0;
        gl1_depth_mode(2);gl1_depth_func(3);gl1_depth_mask(1);return ok;
    }
    return !game_memory_read_u32(&a->memory,0x5840f0) && game_menu_accelerated_text_end(a);
}
/*439e00/439e30: the palette frame's actual colored quad and SDK state. */
int game_application_palette_quad(GameApplication *a,int32_t left,int32_t top,int32_t right,int32_t bottom,uint8_t color){
    PortablePlatform *p=a->platform;GameRenderVertex vertices[4],inherited[4]={{0}};uint8_t colors[4][3];unsigned i,k;
    if(!p || !p->window_opened )return 0;
    for(i=0;i<4;i++)for(k=0;k<3;k++)colors[i][k]=game_memory_read_u8(&a->memory,0x5ad3a8+(uint32_t)color*4+2-k);
    if(a->memory.fault)return 0;
    gl1_fog_mode(0);gl1_alpha_blend(0);gl1_color_mode(RENDER_COLOR_VERTEX);
    gl1_depth_mode(2);gl1_depth_func(3);gl1_depth_mask(1);gl1_cull(0);
    game_screen_build_quad(vertices,inherited,left,top,right,bottom,colors,0x3f000000);
    gl1_triangle(vertices,vertices+1,vertices+2);gl1_triangle(vertices,vertices+2,vertices+3);return 1;
}
int game_level_screen_palette(GameApplication *a,GameAddress address) {
    PortablePlatform *p=a->platform;uint32_t colors[256];unsigned i;
    if(!p || !p->window_opened )return 0;
    for(i=0;i<256;++i)colors[i]=game_memory_read_u32(&a->memory,address+i*4);
    if(a->memory.fault)return 0;
    gl1_palette_range(colors,0,255);return game_menu_game_palette(a,colors);
}
/*4d9570: the final overlay queue shares the native world renderer. Restore
 *chromakey even when a submission fails, so failure cleanup preserves SDK state. */
int game_level_frame_004d9570(GameApplication *a){
    PortablePlatform *p=a->platform;
    GameAddress head=game_memory_read_u32(&a->memory,0x604f08);int ok;
    if(a->memory.fault)return 0;
    if(!head)return 1;
    if(!p || !p->window_opened )return 0;
    gl1_chroma_mode(0);ok=game_level_world_render(a,head);gl1_chroma_mode(1);
    return ok;
}
typedef struct LevelImageDraw {GameApplication *app;TextureRecord *texture;int started;} LevelImageDraw;
static int level_image_begin(void *context) {(void)context;return 1;}
static int level_image_tile(void *context,const uint8_t pixels[1024],const GameRenderVertex vertices[4]) {
    LevelImageDraw *draw=context;GameApplication *a=draw->app;
    TextureRecord *record=draw->texture;RenderTextureView info;
    if(texture_store_write_indexed(record,pixels,1024,0,0,1)<0)return 0;
    if(!(record->flags&0x80000000u)) {
        if(!game_level_texture_use(a,record->id))return 0;
    } else {
        info.small_lod=info.large_lod=record->image.lod;info.aspect=record->image.aspect;
        info.format=record->image.format;info.pixels=record->image.pixels;
        gl1_upload(record->address,&info);
    }
    gl1_ui_tile(record->address);
    if(!draw->started) {
        gl1_color_mode(RENDER_COLOR_TEXTURE);gl1_alpha_blend(0);
        gl1_depth_func(7);gl1_depth_mode(2);gl1_depth_mask(1);gl1_filter(0,0);
        gl1_dither(0);gl1_fog_mode(0);draw->started=1;
    }
    return game_application_surface_texture(a,record) && game_application_surface_polygon(a,vertices,4);
}
static int level_image_finish(void *context) {
    (void)context;
    gl1_filter(1,1);return 1;
}
extern TextureRecord *game_model_ui_tile_texture(GameApplication *);
static int frame_image_tile(void *context,const uint8_t pixels[1024],const GameRenderVertex vertices[4]){
    LevelImageDraw *draw=context;TextureRecord *record=draw->texture;
    RenderTextureView info;
    if(texture_store_write_indexed(record,pixels,1024,0,0,1)<0 || !game_level_texture_use(draw->app,record->id))return 0;
    info.small_lod=info.large_lod=record->image.lod;info.aspect=record->image.aspect;
    info.format=record->image.format;info.pixels=record->image.pixels;
    gl1_upload(record->address,&info);
    gl1_ui_tile(record->address);
    if(!game_application_surface_texture(draw->app,record))return 0;
    gl1_triangle(vertices,vertices+1,vertices+2);gl1_triangle(vertices,vertices+2,vertices+3);return 1;
}
/*4d8b50: the existing tiled-image algorithm consumes bounded owned bitmap
 * pixels directly. The source/destination may alias; neither is rewritten. */
int game_level_frame_004d8b50(GameApplication *a,const uint32_t source[4],uint32_t destination[4],const uint32_t descriptor[2],float oow){
    PortablePlatform *p=a->platform;GameUiIndexedImage image;GameUiRect src,dst;LevelImageDraw draw;
    GameUiTileOps ops={&draw,level_image_begin,frame_image_tile,level_image_finish};int32_t c[8];unsigned i;
    if(!source || !destination || !descriptor || !p || !p->window_opened )return 0;
    image.width=image.stride=descriptor[0]&65535;image.height=descriptor[0]>>16;
    if(!image.width || image.width>32767 || !image.height || (uint64_t)image.stride*image.height>SIZE_MAX)return 0;
    image.size=(size_t)image.stride*image.height;
    image.pixels=scene_projection_storage_borrow_bytes(descriptor[1],image.size);
    if(!image.pixels)image.pixels=game_radar_map_borrow(descriptor[1],image.size);
    if(!image.pixels)return 0;
    for(i=0;i<4;i++){memcpy(c+i,source+i,4);memcpy(c+4+i,destination+i,4);}
    src.left=c[0];src.top=c[1];src.right=c[2];src.bottom=c[3];
    dst.left=c[4];dst.top=c[5];dst.right=c[6];dst.bottom=c[7];
    draw.app=a;draw.texture=game_model_ui_tile_texture(a);draw.started=0;if(!draw.texture)return 0;
    gl1_alpha_blend(0);gl1_color_mode(RENDER_COLOR_TEXTURE);
    gl1_depth_func(3);gl1_depth_mode(2);gl1_filter(0,0);gl1_dither(0);gl1_fog_mode(0);
    {uint32_t bits=game_memory_read_u32(&a->memory,0x574a18);float epsilon;int ok;memcpy(&epsilon,&bits,4);
     ok=!a->memory.fault && game_ui_draw_tiled_image(&image,&src,&dst,oow,epsilon,&ops)==GAME_UI_TILED_IMAGE_OK;
     if(!ok)gl1_filter(1,1);return ok;}
}
/*43b0b0: the established native tile algorithm and existing TextureStore own
 * all working pixels. The source snapshot is transient and the scratch texture
 * is retired after submission, including failed submissions. */
int game_level_screen_image(GameApplication *a,GameAddress source,uint32_t stride) {
    PortablePlatform *p=a->platform;uint8_t *pixels;unsigned row;int ok;
    AssetTexture scratch={0};MaterialValue material={0};LevelImageDraw draw;
    GameUiIndexedImage image;GameUiTileOps ops={&draw,level_image_begin,level_image_tile,level_image_finish};
    if(!p || !p->window_opened || stride<640 ||
        (uint64_t)source+479u*(uint64_t)stride+640u>UINT32_MAX)return 0;
    pixels=malloc(640u*480u);if(!pixels)return 0;
    for(row=0;row<480;++row)if(!game_memory_read(&a->memory,source+row*stride,pixels+row*640u,640)){free(pixels);return 0;}
    scratch.width=scratch.height=scratch.render_width=scratch.render_height=32;
    scratch.lod=3;scratch.aspect=3;scratch.format=5;scratch.size=1024;scratch.pixels=calloc(1,1024);
    if(!scratch.pixels || !game_model_texture_take(a,&scratch,0,&material)) {
        asset_texture_release(&scratch);free(pixels);return 0;
    }
    draw.app=a;draw.texture=game_model_texture_record(a,material.backend_handle);draw.started=0;
    image.pixels=pixels;image.size=640u*480u;image.width=image.stride=640;image.height=480;
    ok=draw.texture && game_ui_draw_fullscreen_image(&image,&ops)==GAME_UI_TILED_IMAGE_OK;
    free(pixels);if(!game_model_texture_release(a,material.backend_handle))ok=0;
    return ok && !a->memory.fault;
}
void game_application_ui_shutdown(GameApplication *app) {
    PortablePlatform *p=app->platform;
    if(!p)return;
    game_application_level_graphics_end(app);
    if(p->renderer_initialized)gl1_shutdown();
    gl1_current=NULL;p->renderer_initialized=0;
}
/*439ce0: ordinary SDK vertices and actual palette bytes. Native DLL pointers
 * stay with the platform owner; no address-ID calling convention adapter. */
int game_application_render_line(GameApplication *a,const int32_t coordinates[4],uint8_t color){
    PortablePlatform *p=a->platform;GameRenderVertex inherited[2]={{0}},vertices[2];
    GameAddress palette=0x5ad3a8u+(uint32_t)color*4u;uint8_t r,g,b;
    if(!p || !p->renderer_initialized )return 0;
    b=game_memory_read_u8(&a->memory,palette);g=game_memory_read_u8(&a->memory,palette+1);r=game_memory_read_u8(&a->memory,palette+2);
    if(a->memory.fault)return 0;
    game_screen_build_segment(vertices,inherited,coordinates,r,g,b);
    gl1_dither(0);gl1_alpha_blend(0);gl1_color_mode(RENDER_COLOR_VERTEX);
    gl1_depth_mode(2);gl1_depth_func(3);gl1_depth_mask(1);
    gl1_line(&vertices[0],&vertices[1]);return 1;
}
int game_application_palette_point(GameApplication *a,int32_t x,int32_t y,uint8_t color){
    PortablePlatform *p=a->platform;GameRenderVertex inherited={0},vertex;
    GameAddress palette=0x5ad3a8+(uint32_t)color*4;
    if(!p || !p->renderer_initialized )return 0;
    game_screen_build_point(&vertex,&inherited,x,y,game_memory_read_u8(&a->memory,palette+2),
        game_memory_read_u8(&a->memory,palette+1),game_memory_read_u8(&a->memory,palette));
    if(a->memory.fault)return 0;
    gl1_fog_mode(0);gl1_alpha_blend(0);gl1_color_mode(RENDER_COLOR_VERTEX);
    gl1_depth_mode(2);gl1_depth_func(3);gl1_depth_mask(1);gl1_point(&vertex);return 1;
}
/*Circle's shader is established once, even when all segments are clipped. */
int game_application_palette_lines(GameApplication *a,const int32_t lines[][4],unsigned count,uint8_t color){
    PortablePlatform *p=a->platform;GameAddress palette=0x5ad3a8+(uint32_t)color*4;
    GameRenderVertex inherited[2]={{0}},vertices[2];uint8_t r,g,b;unsigned i;
    if(!p || !p->renderer_initialized || count>32 || (count && !lines))return 0;
    r=game_memory_read_u8(&a->memory,palette+2);g=game_memory_read_u8(&a->memory,palette+1);b=game_memory_read_u8(&a->memory,palette);
    if(a->memory.fault)return 0;
    gl1_fog_mode(0);gl1_alpha_blend(0);gl1_color_mode(RENDER_COLOR_VERTEX);
    gl1_depth_mode(2);gl1_depth_func(3);gl1_depth_mask(1);
    for(i=0;i<count;i++){game_screen_build_segment(vertices,inherited,lines[i],r,g,b);gl1_line(vertices,vertices+1);}
    return 1;
}
/*The native SDK ABI is isolated here. Device addresses are numbers; the
 *texture-info data member is a full-width native pointer, never a guest word. */

static int texture_statistics(void *context,uint32_t output[5]){
    PortablePlatform *p=((GameApplication *)context)->platform;
    if(!p || !p->renderer_initialized )return 0;
    gl1_stats(output);gl1_reset_stats();return 1;
}
static int texture_upload(void *context,uint32_t address,const uint8_t format[4],GameAddress data,uint32_t bytes){
    GameApplication *a=context;PortablePlatform *p=a->platform;RenderTextureView info;uint8_t *pixels;
    if(!p || !p->renderer_initialized )return 0;
    pixels=scene_projection_storage_borrow_bytes(data,bytes);
    if(!pixels)return 0;
    info.small_lod=format[0];info.large_lod=format[1];info.aspect=format[2];info.format=format[3];info.pixels=pixels;
    {uint32_t required=gl1_texture_bytes(&info);if(!required || required>bytes)return 0;}
    gl1_upload(address,&info);return gl1_current && !gl1_current->failed;
}
static TextureRecord *texture_record(void *context,uint32_t id){return game_model_texture_record(context,id);}
static int texture_upload_owned(void *context,uint32_t address,const AssetTexture *image){
    PortablePlatform *p=((GameApplication *)context)->platform;RenderTextureView info;uint8_t minimum[8]={0};
    if(!p || !p->renderer_initialized || !image || !image->pixels)return 0;
    info.small_lod=image->lod;info.large_lod=image->lod;info.aspect=image->aspect;info.format=image->format;info.pixels=image->pixels;
    {uint32_t required=gl1_texture_bytes(&info);
     if(required==sizeof(minimum) && image->size<sizeof(minimum)){
        /*The synchronous SDK upload reads its minimum8-byte block. Supply
         *bounded zero padding, without changing the owned logical image. */
        memcpy(minimum,image->pixels,image->size);info.pixels=minimum;
     }else if(!required || required>image->size){
        fprintf(stderr,"native texture upload size failed: width=%u height=%u lod=%u aspect=%u format=%u bytes=%u required=%u\n",
            image->width,image->height,image->lod,image->aspect,image->format,(unsigned)image->size,required);return 0;}}
    gl1_upload(address,&info);return gl1_current && !gl1_current->failed;
}
static void texture_release(void *context,uint32_t key){(void)context;gl1_release_texture(key);}
static void texture_reset(void *context){(void)context;gl1_release_textures();}
static GameRenderTextureServices texture_services(GameApplication *a){
    GameRenderTextureServices s={a,texture_statistics,texture_upload,texture_record,texture_upload_owned,texture_release,texture_reset};return s;
}
int game_level_interface_prepare(GameApplication *a){
    GameRenderTextureServices s=texture_services(a);return game_render_texture_cache_begin(&a->memory,&s);
}
int game_level_interface_end(GameApplication *a){
    if(!game_level_projected_texture_release(a))return 0;
    if(!game_render_texture_cache_end(&a->memory))return 0;
    game_model_texture_residency_clear(a);return !a->memory.fault;
}
int game_level_texture_frame(GameApplication *a){
    GameRenderTextureServices s=texture_services(a);return game_render_texture_cache_frame(&a->memory,&s);
}
int game_level_texture_use(GameApplication *a,GameAddress texture){
    GameRenderTextureServices s=texture_services(a);return game_render_texture_use(&a->memory,texture,&s);
}
/*The actual4d7bb0 shader state and triangle algorithm are exposed as native
 *platform services. SDK pointers stay inside this full-width platform owner. */
int game_application_surface_begin(GameApplication *a){
    PortablePlatform *p=a->platform;
    if(!p || !p->renderer_initialized )return 0;
    gl1_color_mode(RENDER_COLOR_TEXTURE);gl1_alpha_blend(0);
    gl1_alpha_mode(RENDER_ALPHA_ZERO);gl1_depth_mode(2);gl1_depth_func(1);gl1_depth_mask(0);
    gl1_dither(2);gl1_fog_mode(0);gl1_cull(0);return 1;
}
int game_application_sky_begin(GameApplication *a,const GameSkyFrame *frame){
    if(!a || !a->platform)return 0;
    return gl1_sky_begin(frame);
}
int game_application_sky_end(GameApplication *a){
    if(!a || !a->platform)return 0;
    return gl1_sky_end();
}
int game_application_remote_sky(GameApplication *a,const GameRemoteSkyFrame *frame){
    if(!a || !a->platform)return 0;
    return gl1_remote_sky(frame);
}
int game_application_surface_clamp(GameApplication *a,int enabled){
    PortablePlatform *p=a->platform;if(!p || !p->renderer_initialized )return 0;
    gl1_clamp((uint32_t)enabled,(uint32_t)enabled);return 1;
}
int game_application_surface_texture(GameApplication *a,const TextureRecord *texture){
    PortablePlatform *p=a->platform;
    if(!p || !p->renderer_initialized || !texture || !texture->image.pixels)return 0;
    gl1_source(texture->address);return 1;
}
int game_application_surface_polygon(GameApplication *a,const GameRenderVertex *vertices,unsigned count){
    PortablePlatform *p=a->platform;GameRenderVertex mask[32];float y[32];uint8_t triangles[30][3];unsigned i,n;
    if(!p || !p->renderer_initialized || !vertices || !count || count>32)return 0;
    if(p->screen_mask_active){
        memcpy(mask,vertices,count*sizeof(*vertices));
        /* 65536 wraps to zero in the original FBI W representation. */
        for(i=0;i<count;i++)if(mask[i].oow==65536.0f)mask[i].oow=0;
        vertices=mask;
    }
    for(i=0;i<count;++i)y[i]=vertices[i].y;n=game_screen_polygon_triangles(y,count,triangles);
#ifdef UPRISING_EXPERIMENTAL_VULKAN
    if(gl1_current && gl1_current->vulkan && !gl1_current->sky_active){
        if(!gl1_draw_ready())return 0;
        return vk1_polygon(vertices,count,triangles,n);
    }
#endif
    for(i=0;i<n;++i)gl1_triangle(vertices+triangles[i][0],vertices+triangles[i][1],vertices+triangles[i][2]);
    return 1;
}
int game_application_surface_motion_polygon(GameApplication *a,const GameRenderVertex *v,unsigned count,const uint32_t *references){
#ifdef UPRISING_EXPERIMENTAL_VULKAN
    int result;
    if(gl1_current && gl1_current->vulkan && gl1_current->world_depth_capture){
        gl1_current->motion_vertices=v;gl1_current->motion_references=references;gl1_current->motion_count=count;
        result=game_application_surface_polygon(a,v,count);
        gl1_current->motion_vertices=NULL;gl1_current->motion_references=NULL;gl1_current->motion_count=0;
        return result;
    }
#endif
    (void)references;return game_application_surface_polygon(a,v,count);
}
int game_application_texture_update(GameApplication *a,const TextureRecord *texture){
    PortablePlatform *p=a->platform;RenderTextureView info;
    if(!p || !p->renderer_initialized || !texture ||
       !texture->image.pixels || !(texture->flags&0x80000000u))return 0;
    info.small_lod=info.large_lod=texture->image.lod;info.aspect=texture->image.aspect;
    info.format=texture->image.format;info.pixels=texture->image.pixels;
    gl1_upload(texture->address,&info);return 1;
}
int game_application_scene_polygon_begin(GameApplication *a,int textured){
    PortablePlatform *p=a->platform;
    if(!p || !p->renderer_initialized )return 0;
    if(textured){

        gl1_color_mode(RENDER_COLOR_TEXTURE);gl1_dither(0);
    }else{

        gl1_constant(0x7f000000);gl1_alpha_mode(RENDER_ALPHA_CONSTANT);
        gl1_color_mode(RENDER_COLOR_VERTEX);gl1_cull(0);
    }
    gl1_fog_mode(0);gl1_alpha_blend(0);gl1_depth_mode(2);gl1_depth_func(3);return 1;
}
int game_application_scene_translucency(GameApplication *a,int enabled){
    PortablePlatform *p=a->platform;
    if(!p || !p->renderer_initialized )return 0;
    gl1_alpha_blend(enabled);gl1_depth_mask(enabled?0:1);return 1;
}
int game_application_scene_depth(GameApplication *a,int enabled){
    if(!gl1_current || gl1_current->application!=a || !gl1_ready())return 0;
#ifdef UPRISING_EXPERIMENTAL_VULKAN
    if(enabled && gl1_current->vulkan){
        Vk1Device *n=gl1_current->vulkan;float projection[5],view[12],jitter[2]={0,0};unsigned i;
        Uint64 view_time=SDL_GetTicksNS();
        if(n->observe){n->scene_start_ns=view_time;if(n->frame_start && view_time>=n->frame_start)n->prepare_cost_ns+=view_time-n->frame_start;}
        uint32_t crop[4]={0,0,gl1_current->width,gl1_current->height};
        uint32_t output[2];int requested=game_display_temporal_output(a,output);
        static const GameAddress fields[5]={0x6290c8,0x6290cc,0x6290f0,0x6290f4,0x574cb0};
        uint32_t camera=game_memory_read_u32(&a->memory,0x58f890),mode=game_memory_read_u32(&a->memory,0x604760);
        for(i=0;i<5;i++){uint32_t bits=game_memory_read_u32(&a->memory,fields[i]);memcpy(projection+i,&bits,4);}
        for(i=0;i<12;i++){uint32_t bits=game_memory_read_u32(&a->memory,0x5cedf0+i*4);memcpy(view+i,&bits,4);}
        if(a->memory.fault)return 0;
        /* Half the4096-unit spatial cell is a conservative single-frame jump.
         * Check elapsed time before resolving any geometry or sky samples. */
        n->history_cut_reasons=render_motion_camera(&n->history_pose,n->frames+1,view_time,view,2048);
        if(n->history_cut_reasons){
            vk1_history_reset(n);n->fsr_reset=1;n->fsr_time=0;
            if(n->history_cut_reasons&RENDER_CAMERA_INVALID)return 0;
            if(n->history_cut_reasons&~RENDER_CAMERA_INITIAL)n->history_camera_cuts++;
        }
        if(!output[0] || !output[1]){
            output[0]=n->fsr_output[0]?n->fsr_output[0]:gl1_current->width;
            output[1]=n->fsr_output[1]?n->fsr_output[1]:gl1_current->height;
        }
        if(requested!=n->fsr_requested || memcmp(output,n->fsr_output,sizeof(output))){vk1_history_reset(n);n->fsr_reset=1;n->fsr_time=0;}
        n->fsr_requested=requested;memcpy(n->fsr_output,output,sizeof(output));
        if(n->composed[gl1_current->back] && !vk1_flush(vk1_back(),0))return 0;
        n->composed[gl1_current->back]=0;
        n->spatial_view=!requested && game_display_antialias_samples(a)==1;
        n->fsr_monitor=(n->fsr_requested || n->spatial_view) && mode==7;
        if(n->fsr_monitor){
            uint32_t left=game_memory_read_u32(&a->memory,0x6025e0),top=game_memory_read_u32(&a->memory,0x6025e4);
            uint32_t right=game_memory_read_u32(&a->memory,0x6025e8),bottom=game_memory_read_u32(&a->memory,0x6025ec);
            if(a->memory.fault || right<=left || bottom<=top || right>gl1_current->width || bottom>gl1_current->height)return 0;
            crop[0]=left;crop[1]=top;crop[2]=right-left;crop[3]=bottom-top;
        }
        if(camera!=n->history_camera || mode!=n->history_mode || memcmp(projection,n->history_projection,sizeof(projection)) || memcmp(crop,n->fsr_crop,sizeof(crop))){vk1_history_reset(n);n->fsr_reset=1;}
        n->history_camera=camera;n->history_mode=mode;memcpy(n->history_projection,projection,sizeof(projection));
        memcpy(n->fsr_crop,crop,sizeof(crop));
        /* Subtracting the crop origin centers the actual monitor camera.
         * Previous-minus-current pixel vectors are translation invariant. */
        n->fsr_view=n->fsr_requested && fabsf(projection[0]-(crop[0]+crop[2]*.5f))<.01f && fabsf(projection[1]-(crop[1]+crop[3]*.5f))<.01f;
        n->spatial_view=n->spatial_view && fabsf(projection[0]-(crop[0]+crop[2]*.5f))<.01f && fabsf(projection[1]-(crop[1]+crop[3]*.5f))<.01f;
        if(n->fsr_monitor && !n->fsr_view && !n->spatial_view)return 0;
        if(!n->fsr_view){n->fsr_reset=1;n->fsr_time=0;}
#ifdef UPRISING_FSR3_UPSCALER
        if(n->fsr_view)render_motion_jitter(n->frames,fsr3_upscaler_jitter_phases(crop[2],game_display_temporal_native(a)?crop[2]:(uint32_t)(((uint64_t)crop[2]*output[0]+gl1_current->width-1)/gl1_current->width)),jitter);
        else
#endif
        if(n->jitter_enabled)render_motion_jitter(n->frames,8,jitter);
        /* Terrain and model near clipping both use z=10. The infinite depth
         * encoding does not remove their existing finite visibility limits. */
        if(!vulkan_draw_projection(vk1_back(),10,projection[4],jitter[0],jitter[1]))return 0;
        if(!vk1_ready_draw() || !vulkan_draw_reset_reciprocal_depth(vk1_back()))return 0;
    }
    if(!enabled && gl1_current->world_depth_capture && gl1_current->vulkan){
        Vk1Device *n=gl1_current->vulkan;
        if(n->observe && n->scene_start_ns)n->scene_cost_ns+=SDL_GetTicksNS()-n->scene_start_ns;
        if(!vk1_scene_complete())return 0;
    }
#endif
    gl1_current->world_depth_capture=enabled!=0;return 1;
}
int game_application_world_begin(GameApplication *a){
    PortablePlatform *p=a->platform;
    if(!p || !p->renderer_initialized )return 0;
    p->screen_mask_active=0;
    gl1_color_mode(RENDER_COLOR_INVERSE_TEXTURE);gl1_alpha_mode(RENDER_ALPHA_TEXTURE);
    gl1_alpha_blend(1);gl1_depth_mode(2);gl1_depth_func(1);gl1_depth_mask(1);gl1_dither(2);gl1_cull(0);
    gl1_level_fog(1);
    return 1;
}
void game_application_world_antialias(GameApplication *a,int enabled){
    Gl1Device *g=gl1_current;
    if(!g || g->application!=a || !gl1_ready())return;
    if(g->vulkan)return; /* Vulkan multisampling is a later parity step. */
    if(enabled && g->aa_samples)glEnable(GL_MULTISAMPLE);else glDisable(GL_MULTISAMPLE);
}
int game_application_world_state(GameApplication *a,WorldRenderState kind,int value){
    PortablePlatform *p=a->platform;if(!p || !p->renderer_initialized)return 0;
    switch(kind){
    case WORLD_ALPHA_FADE:

        gl1_alpha_mode(value?RENDER_ALPHA_VERTEX:RENDER_ALPHA_TEXTURE);gl1_alpha_test(value?2:7);return 1;
    case WORLD_ALPHA_TEST:gl1_alpha_test((uint32_t)value);return 1;
    case WORLD_DEPTH_BIAS:gl1_depth_bias(value);return 1;
    case WORLD_TEXTURE_FILTER:gl1_filter(value?1:0,value?1:0);return 1;
    case WORLD_DEPTH_WRITE:gl1_depth_mask(value?1:0);return 1;
    }return 0;
}
int game_application_world_palette(GameApplication *a,const uint32_t *colors,unsigned first,unsigned last,int full){
    PortablePlatform *p=a->platform;
    if(!p || !p->renderer_initialized || !colors || first>last || last>255)return 0;
    if(full){gl1_palette_range(colors,0,255);}
    else{gl1_palette_range(colors,first,last);}return 1;
}
int game_application_world_ncc(GameApplication *a,const uint8_t table[112]){
    PortablePlatform *p=a->platform;
    if(!p || !p->renderer_initialized || !table)return 0;
    gl1_ncc_palette(table);return 1;
}
int game_application_world_point(GameApplication *a,const GameRenderVertex *vertex){
    PortablePlatform *p=a->platform;if(!p || !p->renderer_initialized || !vertex)return 0;
    return gl1_world_point(vertex);
}
int game_application_render_texture(GameApplication *a,GameAddress id){
    const TextureRecord *record=game_model_texture(a,id);PortablePlatform *p=a->platform;
    RenderTextureView info;uint8_t format[4];uint32_t size;GameAddress pixels;
    if(record)return game_application_surface_texture(a,record);
    if(!p || !p->renderer_initialized )return 0;
    if(!game_memory_read(&a->memory,id+16,format,4))return 0;
    size=game_memory_read_u32(&a->memory,id+12);pixels=game_memory_read_u32(&a->memory,id+20);
    info.small_lod=format[0];info.large_lod=format[1];info.aspect=format[2];info.format=format[3];
    info.pixels=scene_projection_storage_borrow_bytes(pixels,size);
    if(a->memory.fault || !size || !info.pixels)return 0;
    gl1_source(game_memory_read_u32(&a->memory,id+8));return !a->memory.fault;
}
int game_application_projected_begin(GameApplication *a,ProjectedQueueKind kind){
    PortablePlatform *p=a->platform;
    if((unsigned)kind>=3 || !p || !p->renderer_initialized )return 0;
    gl1_fog_mode(0);gl1_alpha_blend(1);
    if(kind!=PROJECTED_TEXTURED)gl1_constant(kind==PROJECTED_ALPHA?UINT32_MAX:0);
    gl1_alpha_mode(RENDER_ALPHA_VERTEX);gl1_color_mode(kind==PROJECTED_TEXTURED?RENDER_COLOR_TEXTURE:RENDER_COLOR_CONSTANT);
    gl1_depth_mode(2);gl1_depth_func(3);gl1_depth_mask(0);gl1_cull(0);
    if(kind==PROJECTED_TEXTURED)gl1_filter(0,0);gl1_depth_bias(-100);return 1;
}
int game_application_projected_end(GameApplication *a,ProjectedQueueKind kind){
    PortablePlatform *p=a->platform;
    if((unsigned)kind>=3 || !p || !p->renderer_initialized )return 0;
    if(kind==PROJECTED_TEXTURED)gl1_filter(1,1);gl1_depth_bias(0);return 1;
}

/* Actual 00506210/00506c30 callbacks and 00506640 resource/bootstrap chain.
 * Definitions must be ordinary native code before this target can link. */


extern int game_application_asset_bootstrap(GameApplication *);
extern void game_application_asset_shutdown(GameApplication *);
extern void game_application_window_shutdown(GameApplication *);
extern int game_level_display_masks_release(GameApplication *);
extern int game_level_progress_end(GameApplication *);

static int owned_read(void *context, GameAddress address, void *output, size_t size) {
    if(presentation_instance_projection_read(address,output,size) ||
       runtime_node_pool_projection_read(address,output,size))return 1;
    GameApplication *app = context;
    GameplaySpatialGrid *grid = gameplay_spatial_grid(0x5f0800u);
    if(address>=SCENE_SURFACE_BASE && address<SCENE_SURFACE_BASE+0x5000u){
        if(!scene_surface_read(app->memory.surface,address,output,size))goto fail;return 1;
    }
    if(address>=GAME_TERRAIN_CELLS && address<GAME_TERRAIN_MATERIALS+0x80000u){
        if(!game_level_terrain_read(app,address,output,size))goto fail;
        return 1;
    }
    if(address>=RENDER_ARENA_BASE && address<RENDER_ARENA_END) {
        if(!render_arena_read(address,output,size))goto fail;
        return 1;
    }
    if(address>=RADAR_MAP_BASE && address<RADAR_MAP_END) {
        if(!game_radar_map_read(address,output,size))goto fail;
        return 1;
    }
    if(address>=0xfb000000u && address<0xfb070000u) {
        unsigned pool=(address-0xfb000000u)/0x10000u,unit=pool==6u?2u:4u;
        size_t offset=(address-0xfb000000u)%0x10000u,i,capacity=render_runtime_pool_size(pool);
        if(offset>capacity || size>capacity-offset)goto fail;
        for(i=0;i<size;++i) {
            size_t at=offset+i;uint32_t bits=0;
            if(unit==2u) {
                uint16_t half;
                if(!render_runtime_pool_read_native(pool,at&~(size_t)1u,&half,2u))goto fail;
                bits=half;
            } else if(!render_runtime_pool_read_native(pool,at&~(size_t)3u,&bits,4u))goto fail;
            ((uint8_t *)output)[i]=(uint8_t)(bits>>((at%unit)*8u));
        }
        return 1;
    }
    if (grid && address >= grid->identity && address - grid->identity < 0xf00cu) {
        size_t index;
        if (size > 0xf00cu - (address - grid->identity)) goto fail;
        for (index = 0; index < size; ++index) {
            uint32_t word;
            GameAddress cursor = address + (GameAddress)index;
            if (!gameplay_spatial_projection_read(cursor & ~3u, &word, 4u)) goto fail;
            ((unsigned char *)output)[index] = (unsigned char)(word >> ((cursor & 3u) * 8u));
        }
        return 1;
    }
    if(address>=LEVEL_ENTITY_VIEW_BASE && address<LEVEL_ENTITY_VIEW_END)
        return level_entity_store_read(address,output,size);
    if(address>=APPLICATION_NAME_VIEW_BASE && address<APPLICATION_NAME_VIEW_END)
        return application_name_registry_read(address,output,size);
    return scene_projection_storage_read(address, output, size);
fail:
    app->memory.fault = 1; app->memory.fault_address = address;
    app->memory.fault_size = size; app->memory.fault_write = 0; return 0;
}
static int owned_write(void *context, GameAddress address, const void *input, size_t size) {
    if(presentation_instance_projection_write(address,input,size) ||
       runtime_node_pool_projection_write(address,input,size))return 1;
    GameApplication *app = context;
    GameplaySpatialGrid *grid = gameplay_spatial_grid(0x5f0800u);
    if(address>=SCENE_SURFACE_BASE && address<SCENE_SURFACE_BASE+0x5000u){
        if(!scene_surface_write_pipeline(app->memory.surface,address,input,size))goto fail;return 1;
    }
    if(address>=GAME_TERRAIN_CELLS && address<GAME_TERRAIN_MATERIALS+0x80000u){
        if(!game_level_terrain_write(app,address,input,size))goto fail;
        return 1;
    }
    if(address>=RENDER_ARENA_BASE && address<RENDER_ARENA_END) {
        if(!render_arena_write(address,input,size))goto fail;
        return 1;
    }
    if(address>=RADAR_MAP_BASE && address<RADAR_MAP_END) {
        if(!game_radar_map_write(address,input,size))goto fail;
        return 1;
    }
    if(address>=0xfb000000u && address<0xfb070000u) {
        unsigned pool=(address-0xfb000000u)/0x10000u,unit=pool==6u?2u:4u;
        size_t offset=(address-0xfb000000u)%0x10000u,i,capacity=render_runtime_pool_size(pool);
        if(offset>capacity || size>capacity-offset)goto fail;
        for(i=0;i<size;++i) {
            size_t at=offset+i,aligned=at&~(size_t)(unit-1u);uint32_t bits=0,shift=(unsigned)(at%unit)*8u;
            uint16_t half;
            if(unit==2u) {
                if(!render_runtime_pool_read_native(pool,aligned,&half,2u))goto fail;
                bits=half;
            } else if(!render_runtime_pool_read_native(pool,aligned,&bits,4u))goto fail;
            bits=(bits&~(255u<<shift))|((uint32_t)((const uint8_t *)input)[i]<<shift);
            if(unit==2u) {
                half=(uint16_t)bits;
                if(!render_runtime_pool_write_native(pool,aligned,&half,2u))goto fail;
            } else if(!render_runtime_pool_write_native(pool,aligned,&bits,4u))goto fail;
        }
        return 1;
    }
    if (grid && address >= grid->identity && address - grid->identity < 0xf00cu) {
        size_t index;
        if (size > 0xf00cu - (address - grid->identity)) goto fail;
        for (index = 0; index < size; ++index) {
            uint32_t word, shift;
            GameAddress cursor = address + (GameAddress)index;
            shift = (cursor & 3u) * 8u;
            if (!gameplay_spatial_projection_read(cursor & ~3u, &word, 4u)) goto fail;
            word = (word & ~(255u << shift)) | (uint32_t)((const unsigned char *)input)[index] << shift;
            if (!gameplay_spatial_projection_write(cursor & ~3u, &word, 4u)) goto fail;
        }
        return 1;
    }
    if(address>=LEVEL_ENTITY_VIEW_BASE && address<LEVEL_ENTITY_VIEW_END)
        return level_entity_store_write(address,input,size);
    if(address>=APPLICATION_NAME_VIEW_BASE && address<APPLICATION_NAME_VIEW_END)
        return application_name_registry_write(address,input,size);
    return scene_projection_storage_write(address, input, size);
fail:
    app->memory.fault = 1; app->memory.fault_address = address;
    app->memory.fault_size = size; app->memory.fault_write = 1; return 0;
}

GameConfig *game_application_config(GameApplication *app){PortablePlatform *p=app->platform;return p?p->configuration:NULL;}
int game_application_config_flush(GameApplication *app){
    PortablePlatform *p=app->platform;char temporary[4140];FILE *file;int ok;
    if(!p || !p->configuration || !p->configuration_ready || !*p->configuration_path)return 1;
    if(!p->configuration_pending && !game_config_dirty(p->configuration))return 1;
    if(snprintf(temporary,sizeof(temporary),"%s.tmp",p->configuration_path)>=(int)sizeof(temporary))return 0;
    file=fopen(temporary,"wb");if(!file)return 0;
    ok=game_config_write(p->configuration,file);
    if(ok && !durable_file(file))ok=0;
    if(fclose(file))ok=0;
    if(!ok || !SDL_RenamePath(temporary,p->configuration_path)){
        SDL_RemovePath(temporary);return 0;
    }
    game_config_saved(p->configuration);p->configuration_pending=0;return 1;
}
static int configuration_begin(GameApplication *app){
    PortablePlatform *p=app->platform;char directory[4128];FILE *file;int loaded=0;
    p->configuration=game_config_create();if(!p->configuration)return 0;
    if(snprintf(directory,sizeof(directory),"%s/CONFIG",p->user_directory)>=(int)sizeof(directory))return 0;
    if(!SDL_CreateDirectory(directory))return 0;
    if(snprintf(p->configuration_path,sizeof(p->configuration_path),"%s/settings.ini",directory)>=(int)sizeof(p->configuration_path))return 0;
    file=fopen(p->configuration_path,"rb");
    if(file){loaded=game_config_read(p->configuration,file);fclose(file);if(!loaded)return 0;}
    else {
        if(errno!=ENOENT)return 0;
        /* Isolated runs borrow the user's initial preferences once, then own
         * their independent CONFIG. There is no per-frame file syncing. */
        p->configuration_pending=1;file=fopen("CONFIG/settings.ini","rb");
        if(file){loaded=game_config_read(p->configuration,file);fclose(file);if(!loaded)return 0;}
    }
    if(!loaded){
        char line[128];unsigned lines=0;int value;
        file=fopen("uprising-recomp-settings.ini","rb");
        if(file){while(++lines<=128 && fgets(line,sizeof(line),file)){
            if(sscanf(line,"music_mode=%d",&value)==1 && value>=0 && value<=2)
                game_config_set_number(p->configuration,"music","mode",(uint32_t)value);
            else if(sscanf(line,"music_track=%d",&value)==1 && value>=1 && value<=99)
                game_config_set_number(p->configuration,"music","track",(uint32_t)value);
        }fclose(file);}
    }
    p->configuration_ready=game_config_set_number(p->configuration,"configuration","version",1);
    return p->configuration_ready;
}
int game_application_platform_start(GameApplication *app,const char *executable) {
    const char *stage="platform allocation",*base=SDL_getenv("UPRISING_ASSET_DIRECTORY"),*directory=SDL_getenv("UPRISING_USER_DIRECTORY");
    if(!base || !*base)base=SDL_GetBasePath();
#ifdef __APPLE__
    char home_directory[4128];
#endif
    PortablePlatform *platform=calloc(1,sizeof(*platform));
    if(!platform)return 0;
    app->platform=platform;
    {const char *isolated=SDL_getenv("UPRISING_RECOMP_ISOLATED_INPUT");
     platform->isolated_input=isolated && !strcmp(isolated,"1");}
    if(!base || !executable || !*executable)goto fail;
    platform->executable=SDL_strdup(executable);if(!platform->executable)goto fail;
    if(!directory || !*directory){
#ifdef __APPLE__
        const char *home=SDL_GetUserFolder(SDL_FOLDER_HOME);
        int length;
        if(!home || !*home)goto fail;
        length=snprintf(home_directory,sizeof(home_directory),"%s/uprising",home);
        if(length<0 || (size_t)length>=sizeof(home_directory))goto fail;
        if(!SDL_CreateDirectory(home_directory))goto fail;
        directory=home_directory;
#else
        directory=base;
#endif
    }
    if(strlen(directory)>4095)goto fail;
    platform->user_directory=SDL_strdup(directory);if(!platform->user_directory)goto fail;
    /* Resolve assets once, even when a shortcut/debugger starts elsewhere.
     * All writes still use the independently owned user-data directory. */
    if(!asset_directory(base))goto fail;
    if(!SDL_getenv("UPRISING_RECOMP_HEADLESS") && !SDL_getenv("UPRISING_RECOMP_WINDOWED"))
        SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"UPRISING_RECOMP_WINDOWED","1",false);
    if(!configuration_begin(app))goto fail;
    if(SDL_getenv("UPRISING_USER_DIRECTORY") ||
       SDL_getenv("UPRISING_RECOMP_NATIVE_OBSERVE")){
        char log_path[4128];
        if(game_application_file_path(app,"uprising-portable.log",1,log_path,sizeof(log_path)))platform->log=fopen(log_path,"w");
    }
    if(platform->log)setvbuf(platform->log,NULL,_IOLBF,0);
    if(platform->isolated_input)
        game_application_log(app,"native automated input isolation: %s\n","physical devices and desktop cursor excluded");
    stage = "source-defined initial records";
    if (!game_initial_data_create(&app->memory)) goto fail;
    app->memory.read_fallback = owned_read;
    app->memory.write_fallback = owned_write;
    app->memory.fallback_context = app;
    app->memory.owned_pages = projection_owner_pages();
    stage = "global constructors";
    if (!game_application_initialize_globals(app)) goto fail;
    stage = "platform startup";
    if (!game_application_startup(app)) goto fail;
    return 1;
fail:
    fprintf(stderr,"native startup failed: %s; memory fault=%d at %08lx size=%lu; SDL: %s\n",
        stage, app->memory.fault, (unsigned long)app->memory.fault_address,
        (unsigned long)app->memory.fault_size,SDL_GetError());
    fflush(stderr);
    game_application_platform_stop(app);
    return 0;
}
int game_application_isolated_input(GameApplication *app) {
    PortablePlatform *platform=app->platform;return platform && platform->isolated_input;
}
int game_application_platform_stop(GameApplication *app) {
    PortablePlatform *platform = app->platform;
    int settings_ok=1;
    if(app->memory.data && !app->memory.fault)
        settings_ok=game_menu_settings_checkpoint(app);
    game_menu_movies_dispose(app);
    if(app->memory.data && !game_level_world_surface_release(app))game_application_log(app,"world surface cleanup","retirement failed");
    if(app->memory.data)(void)game_level_progress_end(app);
    if(app->memory.data)(void)game_level_interface_end(app);
    int ok = game_application_shutdown_globals(app) && settings_ok;
    {extern int game_level_routes_release(GameApplication *);
        if(!game_level_routes_release(app))ok=0;}
    game_application_ui_shutdown(app);
    game_menu_images_dispose(app);
    if(app->memory.data)(void)game_level_display_masks_release(app);
    game_input_devices_end(app);
    {extern void game_music_shutdown(GameApplication *);game_music_shutdown(app);}
    sound_platform_shutdown();
    render_runtime_pools_release();
    render_arena_release();
    game_application_window_shutdown(app);
    game_application_asset_shutdown(app);
    game_application_services_dispose(app);
    recyclable_record_shutdown();
    scene_projection_storage_release_all();
    if (platform) {
        if(!game_application_config_flush(app))ok=0;
        game_config_free(platform->configuration);
        while(platform->archives)(void)game_application_archive_close(app,platform->archives->identity);
        if (platform->log) fclose(platform->log);
        SDL_free(platform->executable);
        SDL_free(platform->user_directory);
        free(platform);
    }
    SDL_Quit();
    free(app->memory.data);
    memset(app, 0, sizeof(*app));
    return ok;
}

ResourceFile *game_application_settings_file(GameApplication *app,int write) {
    (void)app;if(write)return NULL;
    /* Read-only, one-time legacy migration. All new writes use CONFIG. */
    return resource_file_open("FONTS/settings.cfg");
}
ResourceFile *game_application_bindings_file(GameApplication *app,int write) {
    (void)app;if(write)return NULL;
    return resource_file_open("FONTS/keymap.cfg");
}

int game_application_startup(GameApplication *app) {
    PortablePlatform *platform=app->platform;GameMemory *m=&app->memory;
    size_t length=strlen(platform->executable);
    game_application_log(app,"startup user directory: %s\n",platform->user_directory);
    game_application_log(app,"startup executable: %s\n",platform->executable);
    if(!game_application_directory(app,"saves")){
        fprintf(stderr,"startup saves directory failed: %s; SDL: %s\n",platform->user_directory,SDL_GetError());return 0;
    }
    game_memory_write_u32(m,0x597084u,0u);
    /* This bounded path is retained for the menu's read-only executable probe;
     * no module, OS-version or registry state participates in game startup. */
    if(length>=260){fprintf(stderr,"startup executable path exceeds the legacy 259-byte limit\n");return 0;}
    if(!game_memory_write(m,0x5eff38u,platform->executable,length+1u)){
        fprintf(stderr,"startup executable path publication failed\n");return 0;
    }
    game_application_log(app,"startup stage: %s\n","asset bootstrap");
    if(!game_application_asset_bootstrap(app)){
        fprintf(stderr,"startup asset bootstrap failed; SDL: %s\n",SDL_GetError());return 0;
    }
    return !m->fault;
}int game_application_audio_muted(GameApplication *app) {
    (void)app;
    return SDL_getenv("UPRISING_RECOMP_HEADLESS")!=0 ||
        SDL_getenv("UPRISING_RECOMP_MUTE")!=0;
}
void game_application_sleep(GameApplication *app, unsigned milliseconds) {
    (void)app; SDL_Delay(milliseconds);
}
int game_application_directory(GameApplication *app,const char *path){
    char resolved[4128];
    return game_application_file_path(app,path,1,resolved,sizeof(resolved)) && SDL_CreateDirectory(resolved);
}
int game_application_error(GameApplication *app,const char *message,const char *title){
    if(SDL_getenv("UPRISING_RECOMP_HEADLESS") || game_application_isolated_input(app)){
        fprintf(stderr,"%s: %s\n",title?title:"native game",message?message:"");fflush(stderr);
        if(app && app->platform)game_application_log(app,"%s\n",message?message:"");
        return 0;
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,title?title:"Uprising",message?message:"",game_menu_sdl_window(app));
    return 0;
}
int game_application_output_directory(GameApplication *app,const char *path){
    return game_application_directory(app,path);
}void game_application_log(GameApplication *app, const char *format, const char *argument) {
    FILE *log = ((PortablePlatform *)app->platform)->log;
    if (log) {fprintf(log, format, argument);fflush(log);}
}
int game_application_calendar(GameApplication *app, int32_t calendar[9]) {
    time_t now = time(NULL);
    struct tm *date = localtime(&now);
    (void)app;
    if (!date) return 0;
    calendar[0] = date->tm_sec; calendar[1] = date->tm_min;
    calendar[2] = date->tm_hour; calendar[3] = date->tm_mday;
    calendar[4] = date->tm_mon + 1; calendar[5] = date->tm_year;
    calendar[6] = date->tm_wday; calendar[7] = date->tm_yday;
    calendar[8] = date->tm_isdst;
    return 1;
}
