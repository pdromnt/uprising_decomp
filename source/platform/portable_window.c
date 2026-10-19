#include "app_identity.h"
#include "portable_sdl.h"
#include "portable_font.h"
#include <SDL3/SDL_main.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include "menu_widgets.h"
#include "portable_input_devices.h"
#include "scene_projection_storage.h"
#include "game_text_rasterizer.h"
#include "ui_geometry.h"
#include "gameplay_state.h"
#include "level_world_surface.h"

typedef struct MenuTextSurface {
    GameAddress identity;SDL_Surface *surface;void *locked_pixels;
    uint32_t key;int key_set;struct MenuTextSurface *next;
} MenuTextSurface;
/* SDL pixels are authoritative. The RGBA surface is only a device-upload
 * staging buffer; gameplay and widgets never read or synchronize from it. */
typedef struct MenuWindow {
    GameAddress identity;HWND handle;HINSTANCE instance;
    SDL_Window *window;SDL_GLContext context;int sdl_initialized;
    SDL_Surface *front,*back,*upload;SDL_Palette *palette;SDL_Color colors[256];
    GLuint menu_texture;unsigned width,height;int hidden,palette_valid;
    int pointer_capture_requested;int32_t isolated_pointer[2];
    GameMenuFonts *fonts;MenuTextSurface *text_surfaces;
    void *frame_pixels,*text_lock_pixels;
} MenuWindow;
extern int game_music_update(GameApplication *);
extern void game_application_splash_end(GameApplication *);
extern int game_application_level_graphics_end(GameApplication *);
extern int game_application_level_graphics_begin(GameApplication *);
static MenuTextSurface *text_surface_find(MenuWindow *,GameAddress);
static int present_windowed_front(MenuWindow *);

static int surface_span(SDL_Surface *surface,GameMenuPixelSpan *span){
    if(!surface || !span || surface->format!=SDL_PIXELFORMAT_INDEX8 || surface->w<=0 || surface->h<=0 ||
       surface->w>8192 || surface->h>8192 || surface->pitch<surface->w ||
       (uint64_t)(uint32_t)surface->pitch*(uint32_t)surface->h>SIZE_MAX)return 0;
    span->pixels=surface->pixels;span->bytes=(size_t)surface->pitch*surface->h;
    span->width=surface->w;span->height=surface->h;span->stride=surface->pitch;return span->pixels!=NULL;
}
static int palette_set(MenuWindow *w,const SDL_Color colors[256]){
    SDL_Color opaque[256];unsigned i;
    if(!w || !w->palette)return 0;
    for(i=0;i<256;++i){opaque[i]=colors[i];opaque[i].a=255;}
    if(!SDL_SetPaletteColors(w->palette,opaque,0,256))return 0;
    memcpy(w->colors,colors,sizeof(w->colors));w->palette_valid=1;return 1;
}
static SDL_Surface *new_surface(MenuWindow *w,unsigned width,unsigned height){
    SDL_Surface *surface;
    if(!w || !w->palette || !width || !height || width>8192 || height>8192)return NULL;
    surface=SDL_CreateSurface((int)width,(int)height,SDL_PIXELFORMAT_INDEX8);if(!surface)return NULL;
    if(!SDL_SetSurfacePalette(surface,w->palette) || !SDL_SetSurfaceBlendMode(surface,SDL_BLENDMODE_NONE)){
        SDL_DestroySurface(surface);return NULL;
    }
    return surface;
}
static int create_surfaces(MenuWindow *w){
    SDL_Color black[256]={{0}};
    w->palette=SDL_CreatePalette(256);if(!w->palette)return 0;
    if(!palette_set(w,black))return 0;w->palette_valid=0;
    w->front=new_surface(w,w->width,w->height);w->back=new_surface(w,w->width,w->height);
    return w->front && w->back;
}
static void window_surfaces_release(MenuWindow *w){
    if(w->frame_pixels && w->front)SDL_UnlockSurface(w->front);
    if(w->text_lock_pixels && w->back)SDL_UnlockSurface(w->back);
    w->frame_pixels=w->text_lock_pixels=NULL;
    SDL_DestroySurface(w->front);SDL_DestroySurface(w->back);w->front=w->back=NULL;
    SDL_DestroyPalette(w->palette);w->palette=NULL;w->palette_valid=0;
}
static int game_menu_window_resources_begin(GameApplication *a){
    MenuWindow *w=a->menu_window;unsigned i;
    if(!w)return 0;
    w->fonts=game_menu_fonts_open();
    if(!w->fonts){game_application_log(a,"%s\n","menu font initialization failed; verify the complete FONTS assets");return 0;}
    for(i=0;i<7;++i)game_memory_write_u32(&a->memory,w->identity+0x80+i*4,w->identity+0x80+i*4);
    game_memory_write_u32(&a->memory,0x58fd34,1);game_memory_write_u32(&a->memory,0x58fd38,1);
    return !a->memory.fault;
}
static void game_menu_window_resources_end(GameApplication *a){
    MenuWindow *w=a->menu_window;unsigned i;if(!w)return;
    game_menu_fonts_close(w->fonts);w->fonts=NULL;
    for(i=0;i<7;++i)game_memory_write_u32(&a->memory,w->identity+0x80+i*4,0);
    game_memory_write_u32(&a->memory,0x58fd34,0);game_memory_write_u32(&a->memory,0x58fd38,0);
}
static int pointer_clip_release(MenuWindow *w){
    return !w || !w->window || (SDL_SetWindowRelativeMouseMode(w->window,false) &&
        SDL_SetWindowMouseGrab(w->window,false));
}
/* SDL owns confinement and releases it on focus loss. Physical capture is
 * never requested by isolated automation, pause or option screens. */
int game_menu_pointer_capture(GameApplication *a,int enabled){
    MenuWindow *w=a->menu_window;SDL_WindowFlags flags;
    if(!w)return !enabled;
    if(game_application_isolated_input(a))return 1;
    if(game_memory_read_u32(&a->memory,0x5acbe4) || game_memory_read_u32(&a->memory,0x5910a4))enabled=0;
    w->pointer_capture_requested=enabled!=0;
    flags=SDL_GetWindowFlags(w->window);
    if(!enabled || w->hidden || !(flags&SDL_WINDOW_INPUT_FOCUS) || (flags&SDL_WINDOW_MINIMIZED))
        return pointer_clip_release(w);
    return SDL_SetWindowMouseGrab(w->window,true) && SDL_SetWindowRelativeMouseMode(w->window,true);
}

/*418f20: publish the loop's owned termination flag once. */
int game_level_request_end(GameApplication *a){
    if(game_memory_read_u32(&a->memory,0x5ac970))return !a->memory.fault;
    if(!a->menu_window || !a->menu_window->handle)return 0;
    game_memory_write_u32(&a->memory,0x5ac970,1);
    return !a->memory.fault;
}

static void acquire_input(GameApplication *a) {
    if(!game_menu_input_acquire(a,0) || !game_menu_input_acquire(a,1) ||
       (game_memory_read_u32(&a->memory,0x604768u) && !game_menu_input_acquire(a,2)))a->memory.fault=1;
}
int game_menu_pump_events(GameApplication *a,int deliver_input){
    SDL_Event event;MenuWindow *w=a->menu_window;unsigned drained=0;
    if(!w || !w->window)return 0;
    while(drained++<64 && SDL_PollEvent(&event)){
        game_input_devices_event(a,&event);
        switch(event.type){
        case SDL_EVENT_QUIT:case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if(!pointer_clip_release(w))return 0;
            game_memory_write_u32(&a->memory,0x59d4c8,1);break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            game_memory_write_u32(&a->memory,0x6270f4,1);acquire_input(a);
            if(!game_menu_pointer_capture(a,w->pointer_capture_requested))return 0;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:case SDL_EVENT_WINDOW_MINIMIZED:
            if(!game_menu_slider_cancel(a))return 0;
            if(!pointer_clip_release(w) || !game_menu_input_unacquire(a))return 0;
            game_memory_write_u32(&a->memory,0x6270f4,
                w->hidden || game_application_isolated_input(a)?1:0);break;
        case SDL_EVENT_WINDOW_RESTORED:
            if(SDL_GetWindowFlags(w->window)&SDL_WINDOW_INPUT_FOCUS){
                game_memory_write_u32(&a->memory,0x6270f4,1);acquire_input(a);
            }
            if(!game_menu_pointer_capture(a,w->pointer_capture_requested))return 0;
            break;
        case SDL_EVENT_WINDOW_MOVED:
            {RECT bounds;
             if(!GetWindowRect(w->handle,&bounds))return 0;
             game_memory_write_u32(&a->memory,w->identity+0x58,(uint32_t)bounds.left);
             game_memory_write_u32(&a->memory,w->identity+0x5c,(uint32_t)bounds.top);}
            if(!game_menu_pointer_capture(a,w->pointer_capture_requested))return 0;
            break;
        case SDL_EVENT_WINDOW_EXPOSED:
            if(w->front && !game_memory_read_u32(&a->memory,0x5840f0) && !present_windowed_front(w))return 0;
            break;
        default:break;
        }
        if(deliver_input && game_memory_read_u32(&a->memory,0x6270f4) &&
           !game_menu_event_forward(a,&event))return 0;
    }
    return game_music_update(a) && !a->memory.fault;
}
void *game_menu_window_handle(GameApplication *a) {return a->menu_window?a->menu_window->handle:NULL;}
SDL_Window *game_menu_sdl_window(GameApplication *a){return a->menu_window?a->menu_window->window:NULL;}
static void display_unfinished(GameApplication *a,const char *kind,uint32_t identity) {
    char message[112];snprintf(message,sizeof(message),"%s requires native reconstruction: %08x",kind,(unsigned)identity);
    game_application_log(a,"%s\n",message);
}
/*517830: retire the actual display owners, not their published identities.
 * Older auxiliary COM services have no native producer yet. A nonzero unknown
 * identity is an unfinished route, never permission to discard its owner. */
int game_level_display_end(GameApplication *a) {
    static const GameAddress auxiliary[]={0x6278f8,0x6278fc,0x62694c,0x626e58};
    static const GameAddress aliases[]={0x597f64,0x626e78,0x62707c,0x626d58};
    MenuWindow *w=a->menu_window;unsigned i;int ok=1;
    for(i=0;i<3;++i) {
        uint32_t id=game_memory_read_u32(&a->memory,aliases[i]);
        uint32_t offset=i==0?0x130:i==1?0x128:0x12c;
        if(id && (!w || id!=w->identity+offset)) {
            display_unfinished(a,"unowned display identity",id);return 0;
        }
    }
    if(game_memory_read_u32(&a->memory,0x626d58)) {
        display_unfinished(a,"display lock alias",0x626d58);return 0;
    }
    for(i=0;i<sizeof(auxiliary)/sizeof(auxiliary[0]);++i)
        if(game_memory_read_u32(&a->memory,auxiliary[i])) {
            display_unfinished(a,"display service",auxiliary[i]);ok=0;
        }
    if(w) {
        while(w->text_surfaces)
            if(!game_menu_text_surface_release(a,w->text_surfaces->identity))return 0;
        window_surfaces_release(w);
        game_memory_write_u32(&a->memory,w->identity+0x128,0);
        game_memory_write_u32(&a->memory,w->identity+0x12c,0);
    }
    for(i=0;i<sizeof(aliases)/sizeof(aliases[0]);++i)game_memory_write_u32(&a->memory,aliases[i],0);
    if(!game_level_world_surface_release(a))ok=0;
    return ok && !a->memory.fault;
}
SDL_GLContext game_menu_gl_context(GameApplication *a){return a->menu_window?a->menu_window->context:NULL;}
int game_level_display_restore_menu(GameApplication *a){
    MenuWindow *w=a->menu_window;if(!w || !game_level_display_end(a))return 0;
    game_memory_write_u32(&a->memory,0x627068,w->identity+0x124);
    game_memory_write_u32(&a->memory,w->identity+0x124,w->identity+0x124);return !a->memory.fault;
}
int game_level_display_begin(GameApplication *a){
    MenuWindow *w=a->menu_window;
    if(!w || !w->context || w->front || w->back || w->text_surfaces){
        game_application_log(a,"%s\n","SDL display initialization has live menu surfaces or no context");return 0;
    }
    game_memory_write_u32(&a->memory,0x627068,w->identity+0x124);
    game_memory_write_u32(&a->memory,w->identity+0x124,w->identity+0x124);
    return !a->memory.fault && game_application_level_graphics_begin(a);
}
static int pointer_scale(MenuWindow *w,int32_t point[2],int to_client){
    int wide,high;int64_t x,y;
    if(!w || !point || !SDL_GetWindowSize(w->window,&wide,&high) || wide<=0 || high<=0 || !w->width || !w->height)return 0;
    x=to_client?(int64_t)point[0]*wide/w->width:(int64_t)point[0]*w->width/wide;
    y=to_client?(int64_t)point[1]*high/w->height:(int64_t)point[1]*w->height/high;
    if(x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX)return 0;
    point[0]=(int32_t)x;point[1]=(int32_t)y;return 1;
}
int game_menu_pointer_from_client(GameApplication *a,int32_t point[2]){return pointer_scale(a->menu_window,point,0);}
int game_menu_pointer_from_screen(GameApplication *a,int32_t point[2]){
    MenuWindow *w=a->menu_window;int x,y;int64_t dx,dy;
    if(!w || !point || !SDL_GetWindowPosition(w->window,&x,&y))return 0;
    dx=(int64_t)point[0]-x;dy=(int64_t)point[1]-y;
    if(dx<INT32_MIN || dx>INT32_MAX || dy<INT32_MIN || dy>INT32_MAX)return 0;
    point[0]=(int32_t)dx;point[1]=(int32_t)dy;return pointer_scale(w,point,0);
}
int game_menu_pointer_position(GameApplication *a,int32_t x,int32_t y){
    MenuWindow *w=a->menu_window;int32_t point[2]={x,y};if(!w)return 0;
    if(game_application_isolated_input(a)){w->isolated_pointer[0]=x;w->isolated_pointer[1]=y;return 1;}
    if(!(SDL_GetWindowFlags(w->window)&SDL_WINDOW_INPUT_FOCUS))return 1;
    if(!pointer_scale(w,point,1))return 0;
    SDL_WarpMouseInWindow(w->window,(float)point[0],(float)point[1]);return 1;
}
int game_menu_pointer_read(GameApplication *a,int32_t output[2]){
    MenuWindow *w=a->menu_window;float x,y;
    if(!w || !output)return 0;
    if(game_application_isolated_input(a)){memcpy(output,w->isolated_pointer,sizeof(w->isolated_pointer));return 1;}
    SDL_GetGlobalMouseState(&x,&y);
    if(!isfinite(x) || !isfinite(y) || x<INT32_MIN || x>=2147483648.0f || y<INT32_MIN || y>=2147483648.0f)return 0;
    output[0]=(int32_t)x;output[1]=(int32_t)y;return game_menu_pointer_from_screen(a,output);
}
int game_level_viewport_position(GameApplication *a,uint32_t output[2]){
    int32_t point[2];GameplayViewportClampResult result;
    if(!output || !game_menu_pointer_read(a,point))return 0;
    result=gameplay_state_clamp_viewport_values(&a->memory,(uint32_t)point[0],(uint32_t)point[1]);
    output[0]=result.x;output[1]=result.y;return !a->memory.fault;
}
int game_menu_window_mode_restore(GameApplication *a){
    MenuWindow *w=a->menu_window;GameMemory *m=&a->memory;RECT bounds;unsigned i;
    if(!w || !game_menu_pointer_capture(a,0))return 0;
    while(w->text_surfaces)if(!game_menu_text_surface_release(a,w->text_surfaces->identity))return 0;
    window_surfaces_release(w);w->width=640;w->height=480;
    if(!create_surfaces(w)){window_surfaces_release(w);return 0;}
    if(!GetWindowRect(w->handle,&bounds))return 0;
    game_memory_write_u32(m,w->identity+0x4c,640);game_memory_write_u32(m,w->identity+0x50,480);
    game_memory_write_u32(m,w->identity+0x58,(uint32_t)bounds.left);game_memory_write_u32(m,w->identity+0x5c,(uint32_t)bounds.top);
    game_memory_write_u32(m,w->identity+0x60,(uint32_t)(bounds.right-bounds.left));
    game_memory_write_u32(m,w->identity+0x64,(uint32_t)(bounds.bottom-bounds.top));
    for(i=0;i<2;++i)game_memory_write_u32(m,w->identity+0x44+i*4,0);
    for(i=0;i<4;++i)game_memory_write_u32(m,w->identity+0x68+i*4,0);
    game_memory_write_u32(m,w->identity+0x128,w->identity+0x128);game_memory_write_u32(m,w->identity+0x12c,w->identity+0x12c);
    game_memory_write_u32(m,w->identity+0x138,640);game_memory_write_u32(m,w->identity+0x13c,480);
    game_memory_write_u32(m,0x626e78,w->identity+0x128);game_memory_write_u32(m,0x62707c,w->identity+0x12c);
    return (game_application_isolated_input(a) || SDL_ShowCursor()) && !m->fault;
}
void game_menu_window_destroy(GameApplication *a,GameAddress identity){
    MenuWindow *w=a->menu_window;if(!w || identity!=w->identity)return;
    (void)pointer_clip_release(w);(void)game_application_level_graphics_end(a);
    game_application_force_feedback_close(a,1);game_input_devices_end(a);
    while(w->text_surfaces)game_menu_text_surface_release(a,w->text_surfaces->identity);
    window_surfaces_release(w);SDL_DestroySurface(w->upload);game_menu_window_resources_end(a);
    if(w->context){
        if(SDL_GL_MakeCurrent(w->window,w->context) && w->menu_texture)glDeleteTextures(1,&w->menu_texture);
        SDL_GL_DestroyContext(w->context);
    }
    if(w->window)SDL_DestroyWindow(w->window);
    if(w->sdl_initialized)SDL_QuitSubSystem(SDL_INIT_VIDEO);
    game_memory_write_u32(&a->memory,0x627068,0);game_memory_write_u32(&a->memory,0x626e78,0);
    game_memory_write_u32(&a->memory,0x62707c,0);game_memory_write_u32(&a->memory,0x627904,0);
    scene_projection_storage_free(w->identity);free(w);a->menu_window=NULL;
}
/* 004aa780/004ab2f0/004aac70/004ab800: SDL window with borrowed Windows services.
 * Hidden execution uses actual indexed offscreen surfaces and the same C draw
 * consumers; it neither changes the desktop mode nor creates a visible window. */
GameAddress game_menu_window_construct(GameApplication *a,void *instance,unsigned width,unsigned height) {
    MenuWindow *w;RECT bounds;
    GameMemory *m=&a->memory;GameAddress p;
    if(a->menu_window || !width || !height || width>16384u || height>16384u)return 0;
    w=calloc(1,sizeof(*w));if(!w)return 0;
    p=scene_projection_storage_allocate(0x140u,"portable menu window");
    if(!p){free(w);return 0;}
    w->identity=p;w->instance=instance;w->width=width;w->height=height;a->menu_window=w;
    w->isolated_pointer[0]=(int32_t)(width/2);w->isolated_pointer[1]=(int32_t)(height/2);
    w->hidden=GetEnvironmentVariableA("UPRISING_RECOMP_HEADLESS",NULL,0)!=0;

    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"0");
    SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE,"1");
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN,game_application_isolated_input(a)?"0":"1");
    if(!SDL_InitSubSystem(SDL_INIT_VIDEO))goto fail;
    w->sdl_initialized=1;
    if(!SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1) ||
       !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24) || !SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8) ||
       !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2) || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1))goto fail;
    w->window=SDL_CreateWindow(UPRISING_WINDOW_TITLE,(int)width,(int)height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!w->window)goto fail;
    {int top,left;
     /* Preserve the previous outer-window origin, including for matched
      * performance runs: desktop occlusion can affect presentation cost. */
     if(!SDL_GetWindowBordersSize(w->window,&top,&left,NULL,NULL) ||
        !SDL_SetWindowPosition(w->window,left,top))goto fail;}
    w->handle=SDL_GetPointerProperty(SDL_GetWindowProperties(w->window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,NULL);
    if(!w->handle || !game_menu_window_resources_begin(a))goto fail;
    if(!SDL_StartTextInput(w->window))goto fail;
    SendMessageA(w->handle,WM_SETICON,ICON_BIG,(LPARAM)LoadIconA(instance,MAKEINTRESOURCEA(UPRISING_ICON_ID)));
    SendMessageA(w->handle,WM_SETICON,ICON_SMALL,(LPARAM)LoadImageA(instance,
        MAKEINTRESOURCEA(UPRISING_ICON_ID),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
    w->context=SDL_GL_CreateContext(w->window);if(!w->context)goto fail;
    SDL_GL_SetSwapInterval(0);
    game_application_splash_end(a);
    if(!create_surfaces(w))goto fail;
    /* Stable owner/member IDs replace the original handle publications. */
    game_memory_write_u32(m,p,p);game_memory_write_u32(m,p+0x54u,1);
    game_memory_write_u32(m,p+0x4cu,width);game_memory_write_u32(m,p+0x50u,height);
    if(!GetWindowRect(w->handle,&bounds))goto fail;
    game_memory_write_u32(m,p+0x58u,(uint32_t)bounds.left);game_memory_write_u32(m,p+0x5cu,(uint32_t)bounds.top);
    game_memory_write_u32(m,p+0x60u,(uint32_t)(bounds.right-bounds.left));
    game_memory_write_u32(m,p+0x64u,(uint32_t)(bounds.bottom-bounds.top));
    game_memory_write_u32(m,p+0x124u,p+0x124u);game_memory_write_u32(m,p+0x128u,p+0x128u);
    game_memory_write_u32(m,p+0x12cu,p+0x12cu);
    game_memory_write_u32(m,p+0x138u,width);game_memory_write_u32(m,p+0x13cu,height);
    game_memory_write_u32(m,0x627068u,p+0x124u);game_memory_write_u32(m,0x626e78u,p+0x128u);
    game_memory_write_u32(m,0x62707cu,p+0x12cu);game_memory_write_u32(m,0x627904u,p);
    game_memory_write_u32(m,0x627928u,width);game_memory_write_u32(m,0x62792cu,height);
    game_memory_write_u32(m,0x597f68u,width);game_memory_write_u32(m,0x597f6cu,height);
    game_memory_write_u32(m,0x597f50u,0);game_memory_write_u32(m,0x597f54u,0);
    game_memory_write_u32(m,0x597f58u,width);game_memory_write_u32(m,0x597f5cu,height);
    if(width==640u)game_memory_write_u32(m,0x597f80u,1);
    if(m->fault)goto fail;
    if(w->hidden || game_application_isolated_input(a))game_memory_write_u32(m,0x6270f4u,1);
    if(!w->hidden && !SDL_ShowWindow(w->window))goto fail;
    game_application_log(a,"SDL window ready: %s\n",SDL_GetCurrentVideoDriver());
    return p;
fail:game_application_log(a,"SDL window initialization failed: %s\n",SDL_GetError());game_menu_window_destroy(a,p);return 0;
}
int game_menu_surface_prepare(GameApplication *a,int *restored){
    MenuWindow *w=a->menu_window;
    if(!w || !w->front || !w->back || !restored)return 0;
    /* Owned CPU pixels survive minimize and GPU exposure. */
    *restored=0;return 1;
}
static int present_windowed_front(MenuWindow *w){
    int width,height,mode;GLenum error;
    if(!w || !w->front)return 0;
    if(w->hidden || (SDL_GetWindowFlags(w->window)&SDL_WINDOW_MINIMIZED))return 1;
    if(SDL_GL_GetCurrentContext()!=w->context && !SDL_GL_MakeCurrent(w->window,w->context))return 0;
    if(!SDL_GetWindowSizeInPixels(w->window,&width,&height))return 0;
    if(width<=0 || height<=0)return 1;
    if(w->upload && (w->upload->w!=(int)w->width || w->upload->h!=(int)w->height)){
        SDL_DestroySurface(w->upload);w->upload=NULL;
        if(w->menu_texture)glDeleteTextures(1,&w->menu_texture);w->menu_texture=0;
    }
    if(!w->upload)w->upload=SDL_CreateSurface((int)w->width,(int)w->height,SDL_PIXELFORMAT_RGBA32);
    if(!w->upload || !SDL_BlitSurface(w->front,NULL,w->upload,NULL))return 0;
    glGetIntegerv(GL_MATRIX_MODE,&mode);glPushAttrib(GL_ALL_ATTRIB_BITS);glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glDrawBuffer(GL_BACK);glViewport(0,0,width,height);
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);
    glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_FOG);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,w->upload->pitch/4);
    if(!w->menu_texture){
        glGenTextures(1,&w->menu_texture);glBindTexture(GL_TEXTURE_2D,w->menu_texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,(int)w->width,(int)w->height,0,GL_RGBA,GL_UNSIGNED_BYTE,w->upload->pixels);
    }else{
        glBindTexture(GL_TEXTURE_2D,w->menu_texture);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,(int)w->width,(int)w->height,GL_RGBA,GL_UNSIGNED_BYTE,w->upload->pixels);
    }
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,1,1,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2f(0,0);glTexCoord2f(1,0);glVertex2f(1,0);
    glTexCoord2f(1,1);glVertex2f(1,1);glTexCoord2f(0,1);glVertex2f(0,1);glEnd();
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(mode);
    glPopClientAttrib();glPopAttrib();error=glGetError();
    return error==GL_NO_ERROR && SDL_GL_SwapWindow(w->window);
}
static int copy_surface(SDL_Surface *to,SDL_Surface *from,const GameUiRect *src,const GameUiRect *dst,int keyed,uint32_t key,int mirror){
    GameMenuPixelSpan a,b;int result;
    if(!surface_span(to,&a) || !surface_span(from,&b) || !SDL_LockSurface(to))return 0;
    if(!SDL_LockSurface(from)){SDL_UnlockSurface(to);return 0;}
    result=game_ui_indexed_copy(&a,&b,src,dst,keyed && key<=255,(uint8_t)key,mirror);
    SDL_UnlockSurface(from);SDL_UnlockSurface(to);return result;
}
int game_menu_surface_present_rectangle(GameApplication *a,const GameUiRect *rectangle){
    MenuWindow *w=a->menu_window;
    return w && rectangle && copy_surface(w->front,w->back,rectangle,rectangle,0,0,0) && present_windowed_front(w);
}
int game_menu_present_frame(GameApplication *a,uint32_t interval){
    MenuWindow *w=a->menu_window;GameUiRect full;int restored;
    if(game_memory_read_u32(&a->memory,0x5840f0))return game_level_screen_present(a,interval);
    if(!w || !game_menu_surface_prepare(a,&restored))return 0;
    full.left=full.top=0;full.right=(int32_t)w->width;full.bottom=(int32_t)w->height;
    if(interval)SDL_Delay(interval);return game_menu_surface_present_rectangle(a,&full);
}
int game_menu_level_surface_flip(GameApplication *a){
    MenuWindow *w=a->menu_window;GameAddress surface=game_memory_read_u32(&a->memory,0x626e78);GameUiRect full;
    if(a->memory.fault)return 0;if(!surface)return 1;
    if(!w || !w->front || surface!=w->identity+0x128)return 0;
    full.left=full.top=0;full.right=(int32_t)w->width;full.bottom=(int32_t)w->height;
    return game_menu_surface_present_rectangle(a,&full);
}
int game_menu_screen_mask(GameApplication *a,int enabled){
    MenuWindow *w=a->menu_window;
    if(game_memory_read_u32(&a->memory,0x5840f0))return game_level_screen_clear(a,enabled);
    return w && w->back && SDL_FillSurfaceRect(w->back,NULL,(uint8_t)enabled);
}
int game_menu_palette_write_bytes(GameApplication *a,const uint8_t input[1024]){
    MenuWindow *w=a->menu_window;SDL_Color colors[256];unsigned i;
    if(!w || !w->front || !input)return 0;
    for(i=0;i<256;++i){colors[i].r=input[i*4];colors[i].g=input[i*4+1];colors[i].b=input[i*4+2];colors[i].a=input[i*4+3];}
    if(!palette_set(w,colors))return 0;
    game_memory_write_u32(&a->memory,0x597f64,w->identity+0x130);return !a->memory.fault;
}
int game_menu_palette_read_bytes(GameApplication *a,uint8_t output[1024]){
    MenuWindow *w=a->menu_window;unsigned i;if(!w || !w->palette || !w->palette_valid || !output)return 0;
    for(i=0;i<256;++i){output[i*4]=w->colors[i].r;output[i*4+1]=w->colors[i].g;output[i*4+2]=w->colors[i].b;output[i*4+3]=w->colors[i].a;}
    return 1;
}
int game_menu_palette(GameApplication *a,GameAddress source){
    uint8_t bytes[1024];
    if(game_memory_read_u32(&a->memory,0x5840f0))return game_level_screen_palette(a,source);
    return game_memory_read(&a->memory,source,bytes,sizeof(bytes)) && game_menu_palette_write_bytes(a,bytes);
}
int game_menu_fullscreen_image(GameApplication *a,GameAddress source,uint32_t stride){
    MenuWindow *w=a->menu_window;unsigned y;int ok=1;
    if(game_memory_read_u32(&a->memory,0x5840f0))return game_level_screen_image(a,source,stride);
    if(!w || !w->back || stride<w->width || (uint64_t)source+(uint64_t)stride*w->height>UINT32_MAX || !SDL_LockSurface(w->back))return 0;
    for(y=0;y<w->height;++y)if(!game_memory_read(&a->memory,source+y*stride,
        (uint8_t *)w->back->pixels+(size_t)y*w->back->pitch,w->width)){ok=0;break;}
    SDL_UnlockSurface(w->back);return ok;
}
static unsigned nearest_color(MenuWindow *w,int red,int green,int blue){
    unsigned i,chosen=0;uint32_t best=UINT32_MAX;
    for(i=0;i<256;++i){
        int r=(int)w->colors[i].r-red,g=(int)w->colors[i].g-green,b=(int)w->colors[i].b-blue;
        uint32_t distance=(uint32_t)(r*r+g*g+b*b);
        if(distance<best){best=distance;chosen=i;if(!distance)break;}
    }
    return chosen;
}
static int draw_bitmap_surface(GameApplication *a,SDL_Surface *surface,unsigned width,unsigned height,
        const AssetBitmap *image,const GameUiRect *source,const GameUiRect *destination,int transparent,uint32_t key,int mirror){
    MenuWindow *w=a->menu_window;GameMenuPixelSpan span;int64_t dx,dy,sx,sy;int32_t x,y;int ok=1;uint8_t mapping[256];unsigned i;
    if(!w || !surface || !image || !source || !destination || !surface_span(surface,&span))return 0;
    dx=(int64_t)destination->right-destination->left;dy=(int64_t)destination->bottom-destination->top;
    sx=(int64_t)source->right-source->left;sy=(int64_t)source->bottom-source->top;
    if(dx<=0 || dy<=0 || sx<=0 || sy<=0)return 1;
    if(source->left<0 || source->top<0 || (uint32_t)source->right>image->width || (uint32_t)source->bottom>image->height ||
       (!image->rgb24 && !image->palette_bgra) || image->palette_count>256)return 0;
    if(!w->palette_valid){
        SDL_Color colors[256]={{0}};if(image->rgb24)return 0;
        for(i=0;i<image->palette_count;++i){colors[i].r=image->palette_bgra[i*4+2];colors[i].g=image->palette_bgra[i*4+1];colors[i].b=image->palette_bgra[i*4];}
        if(!palette_set(w,colors))return 0;
    }
    for(i=0;!image->rgb24 && i<256;++i){
        if(i>=image->palette_count){mapping[i]=(uint8_t)i;continue;}
        if(w->colors[i].r==image->palette_bgra[i*4+2] && w->colors[i].g==image->palette_bgra[i*4+1] && w->colors[i].b==image->palette_bgra[i*4])mapping[i]=(uint8_t)i;
        else mapping[i]=(uint8_t)nearest_color(w,image->palette_bgra[i*4+2],image->palette_bgra[i*4+1],image->palette_bgra[i*4]);
    }
    if(!SDL_LockSurface(surface))return 0;
    for(y=destination->top<0?0:destination->top;y<destination->bottom && (uint32_t)y<height;++y){
        int64_t row=source->top+((int64_t)y-destination->top)*sy/dy;
        size_t row_offset;uint8_t *output=span.pixels+(size_t)y*span.stride;
        if(mirror)row=source->bottom-1-(row-source->top);
        row_offset=(size_t)(image->top_down?(uint32_t)row:image->height-1-(uint32_t)row)*image->stride;
        if(row_offset>image->pixel_bytes || (size_t)image->width*(image->rgb24?3u:1u)>image->pixel_bytes-row_offset){ok=0;break;}
        for(x=destination->left<0?0:destination->left;x<destination->right && (uint32_t)x<width;++x){
            size_t column=(size_t)(source->left+((int64_t)x-destination->left)*sx/dx);
            if(mirror)column=(size_t)source->right-1-(column-(size_t)source->left);
            if(image->rgb24){
                const uint8_t *pixel=image->pixels+row_offset+column*3;
                uint32_t value=(uint32_t)pixel[0]|(uint32_t)pixel[1]<<8|(uint32_t)pixel[2]<<16;
                if(!transparent || value!=(key&0xffffffu))output[x]=(uint8_t)nearest_color(w,pixel[2],pixel[1],pixel[0]);
            }else{uint8_t value=image->pixels[row_offset+column];if(!transparent || value!=(uint8_t)key)output[x]=mapping[value];}
        }
    }
    SDL_UnlockSurface(surface);return ok;
}
int game_menu_draw_bitmap(GameApplication *a,const AssetBitmap *image,const GameUiRect *source,
        const GameUiRect *destination,int transparent,uint32_t key) {
    MenuWindow *w=a->menu_window;
    if(game_memory_read_u32(&a->memory,0x5840f0)){
        GameTextTarget target;int32_t fallback;int ok,ended;
        int32_t src[4]={source->left,source->top,source->right,source->bottom};
        int32_t dst[4]={destination->left,destination->top,destination->right,destination->bottom};
        extern int game_application_glyph_begin(GameApplication *,GameTextTarget *,int32_t *);
        extern int game_application_glyph_end(GameApplication *);
        if(!game_application_glyph_begin(a,&target,&fallback))return 0;
        ok=target.rgb565 && target.stride>0 && asset_bitmap_blit_rgb565(image,src,dst,target.pixels,target.pixel_bytes,
            game_memory_read_u32(&a->memory,0x627928),game_memory_read_u32(&a->memory,0x62792c),(size_t)target.stride*2u,transparent,key,0);
        ended=game_application_glyph_end(a);return ok && ended && !a->memory.fault;
    }
    return w && draw_bitmap_surface(a,w->back,w->width,w->height,image,source,destination,transparent,key,0);
}

int game_menu_window_clear(GameApplication *a){
    MenuWindow *w=a->menu_window;int ok=1;if(!w)return 0;
    if(w->front)ok=SDL_FillSurfaceRect(w->front,NULL,0) && ok;
    if(w->back)ok=SDL_FillSurfaceRect(w->back,NULL,0) && ok;
    if(!w->hidden && !(SDL_GetWindowFlags(w->window)&SDL_WINDOW_MINIMIZED)){
        PFNGLBINDFRAMEBUFFERPROC bind;SDL_FunctionPointer entry;GLint previous;int swapped;
        if(SDL_GL_GetCurrentContext()!=w->context && !SDL_GL_MakeCurrent(w->window,w->context))return 0;
        entry=SDL_GL_GetProcAddress("glBindFramebuffer");
        if(!entry)entry=SDL_GL_GetProcAddress("glBindFramebufferEXT");
        if(!entry)return 0;memcpy(&bind,&entry,sizeof(bind));
        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);bind(GL_FRAMEBUFFER,0);
        glPushAttrib(GL_COLOR_BUFFER_BIT|GL_SCISSOR_BIT|GL_ENABLE_BIT);
        glDrawBuffer(GL_BACK);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        /* Palette index0 may be white on debrief pages. Window clearing is
         * physical black, independently of the retained indexed pixels. */
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glPopAttrib();
        swapped=SDL_GL_SwapWindow(w->window);bind(GL_FRAMEBUFFER,(GLuint)previous);
        ok=ok && swapped && glGetError()==GL_NO_ERROR;
    }
    return ok;
}
int game_menu_draw_bitmap_on_surface(GameApplication *a,GameAddress identity,const AssetBitmap *image,
        const GameUiRect *source,const GameUiRect *destination,int transparent,uint32_t key,int mirror){
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);
    return p && !p->locked_pixels && draw_bitmap_surface(a,p->surface,p->surface->w,p->surface->h,image,source,destination,transparent,key,mirror);
}
int game_menu_surface_compose_copy(GameApplication *a,GameAddress destination,GameAddress source,
        const GameUiRect *rectangle,int32_t x,int32_t y,int transparent,int mirror){
    MenuTextSurface *to=text_surface_find(a->menu_window,destination),*from=text_surface_find(a->menu_window,source);
    GameUiRect dst;int64_t right,bottom;
    if(!rectangle || !to || !from || to->locked_pixels || from->locked_pixels)return 0;
    right=(int64_t)x+rectangle->right-rectangle->left;bottom=(int64_t)y+rectangle->bottom-rectangle->top;
    if(right<INT32_MIN || right>INT32_MAX || bottom<INT32_MIN || bottom>INT32_MAX || (transparent && !from->key_set))return 0;
    dst.left=x;dst.top=y;dst.right=(int32_t)right;dst.bottom=(int32_t)bottom;
    return copy_surface(to->surface,from->surface,rectangle,&dst,transparent,from->key,mirror);
}
void game_menu_window_close(GameApplication *a,GameAddress identity){
    MenuWindow *w=a->menu_window;if(!w || identity!=w->identity)return;
    if(w->front && w->back){(void)game_menu_screen_mask(a,0);(void)game_menu_present_frame(a,0);}
}
void game_application_window_shutdown(GameApplication *a){if(a->menu_window)game_menu_window_destroy(a,a->menu_window->identity);}
static MenuTextSurface *text_surface_find(MenuWindow *w,GameAddress identity){
    MenuTextSurface *p=w?w->text_surfaces:NULL;while(p && p->identity!=identity)p=p->next;return p;
}
int game_menu_surface_dimensions(GameApplication *a,GameAddress identity,GameMenuPixelSpan *span){
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);return p && !p->locked_pixels && surface_span(p->surface,span);
}
int game_menu_surface_pixels_begin(GameApplication *a,GameAddress identity,GameMenuPixelSpan *span){
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);
    if(!p || p->locked_pixels || !surface_span(p->surface,span) || !SDL_LockSurface(p->surface))return 0;
    p->locked_pixels=p->surface->pixels;return 1;
}
int game_menu_surface_pixels_end(GameApplication *a,GameAddress identity){
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);
    if(!p || !p->locked_pixels)return 0;SDL_UnlockSurface(p->surface);p->locked_pixels=NULL;return 1;
}
int game_menu_frame_pixels_begin(GameApplication *a,GameMenuPixelSpan *span){
    MenuWindow *w=a->menu_window;
    if(!w || w->frame_pixels || !surface_span(w->front,span) || !SDL_LockSurface(w->front))return 0;
    w->frame_pixels=w->front->pixels;return 1;
}
int game_menu_frame_pixels_end(GameApplication *a){
    MenuWindow *w=a->menu_window;if(!w || !w->front || !w->frame_pixels)return 0;
    SDL_UnlockSurface(w->front);w->frame_pixels=NULL;return 1;
}
int game_menu_text_surface_release(GameApplication *a,GameAddress identity){
    MenuWindow *w=a->menu_window;MenuTextSurface **link;if(!w)return 0;
    for(link=&w->text_surfaces;*link;link=&(*link)->next)if((*link)->identity==identity){
        MenuTextSurface *p=*link;if(p->locked_pixels)SDL_UnlockSurface(p->surface);
        *link=p->next;SDL_DestroySurface(p->surface);free(p);return 1;
    }
    return 1;
}
int game_menu_text_measure(GameApplication *a,unsigned font,const char *text,int32_t *width,int32_t *height){
    return a->menu_window && game_menu_font_measure(a->menu_window->fonts,font,text,width,height);
}
int game_menu_text_wrapped_height(GameApplication *a,unsigned font,uint32_t width,const char *text,uint32_t *height){
    return a->menu_window && width<=INT32_MAX && game_menu_font_wrapped_height(a->menu_window->fonts,font,text,(int32_t)width,height);
}
int game_menu_text_surface_fill(GameApplication *a,GameAddress identity,uint32_t fill){
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);
    return p && !p->locked_pixels && SDL_FillSurfaceRect(p->surface,NULL,(uint8_t)fill);
}
int game_menu_text_surface_key(GameApplication *a,GameAddress identity,uint32_t value){
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);
    if(!p || p->locked_pixels || !p->surface->pixels)return 0;
    p->key=value?value:*(const uint8_t *)p->surface->pixels;p->key_set=1;return 1;
}
int game_menu_text_surface_restore(GameApplication *a,GameAddress identity){
    /* SDL CPU surfaces retain their contents across device/window events. */
    return text_surface_find(a->menu_window,identity)?0:-1;
}
int game_menu_text_surface_white(GameApplication *a,GameAddress identity){
    MenuWindow *w=a->menu_window;if(!w)return 0;
    return game_menu_text_surface_fill(a,identity,nearest_color(w,255,255,255));
}
int game_menu_text_surface_create(GameApplication *a,GameAddress identity,unsigned width,unsigned height,uint32_t fill){
    MenuWindow *w=a->menu_window;MenuTextSurface *p;
    if(!w || !identity)return 0;p=calloc(1,sizeof(*p));if(!p)return 0;
    p->surface=new_surface(w,width,height);if(!p->surface){free(p);return 0;}
    if(!game_menu_text_surface_release(a,identity)){SDL_DestroySurface(p->surface);free(p);return 0;}
    p->identity=identity;p->next=w->text_surfaces;w->text_surfaces=p;
    if(!game_menu_text_surface_fill(a,identity,fill))return 0;
    return fill || game_menu_text_surface_key(a,identity,0);
}
int game_menu_text_surface_paint(GameApplication *a,GameAddress identity,unsigned font,const char *text,
        uint32_t color,int32_t x,int32_t y,uint32_t flags,int alternate){
    MenuWindow *w=a->menu_window;GameMenuPixelSpan span;int result;
    if(!w || !game_menu_surface_pixels_begin(a,identity,&span))return 0;
    if(alternate){font=0;color=0x02ffffff;}
    result=game_menu_font_paint(w->fonts,font,text,span.pixels,span.bytes,span.width,span.height,span.stride,
        x,y,alternate?span.width/2:span.width,(uint8_t)nearest_color(w,color&255,(color>>8)&255,(color>>16)&255),flags,alternate?1:0);
    return game_menu_surface_pixels_end(a,identity) && result;
}
int game_menu_text_surface_draw(GameApplication *a,GameAddress identity,const GameUiRect *source,const GameUiRect *destination,int transparent){
    MenuWindow *w=a->menu_window;MenuTextSurface *p=text_surface_find(w,identity);
    if(!p || !source || !destination || p->locked_pixels || (transparent && !p->key_set))return 0;
    if(game_memory_read_u32(&a->memory,0x5840f0)){
        GameMenuPixelSpan span;GameTextTarget target;int32_t fallback;int ok=0,ended;int64_t dx,dy,sx,sy;
        int32_t x,y;uint16_t colors[256];unsigned i;uint32_t wide=game_memory_read_u32(&a->memory,0x627928),high=game_memory_read_u32(&a->memory,0x62792c);
        extern int game_application_glyph_begin(GameApplication *,GameTextTarget *,int32_t *);
        extern int game_application_glyph_end(GameApplication *);
        dx=(int64_t)destination->right-destination->left;dy=(int64_t)destination->bottom-destination->top;
        sx=(int64_t)source->right-source->left;sy=(int64_t)source->bottom-source->top;
        if(dx<=0 || dy<=0 || sx<=0 || sy<=0)return 1;
        if(!surface_span(p->surface,&span) || source->left<0 || source->top<0 || source->right>span.width || source->bottom>span.height)return 0;
        if(!game_application_glyph_begin(a,&target,&fallback))return 0;
        if(target.rgb565 && target.stride>0 && (uint64_t)(uint32_t)target.stride*2*high<=target.pixel_bytes && wide<=(uint32_t)target.stride){
            for(i=0;i<256;++i)colors[i]=(uint16_t)((w->colors[i].r>>3)<<11|(w->colors[i].g>>2)<<5|(w->colors[i].b>>3));
            for(y=destination->top<0?0:destination->top;y<destination->bottom && (uint32_t)y<high;++y){
                const uint8_t *row=span.pixels+(size_t)(source->top+((int64_t)y-destination->top)*sy/dy)*span.stride;
                uint8_t *out=target.pixels+(size_t)y*target.stride*2;
                for(x=destination->left<0?0:destination->left;x<destination->right && (uint32_t)x<wide;++x){
                    uint8_t value=row[source->left+((int64_t)x-destination->left)*sx/dx];uint16_t packed=colors[value];
                    if(!transparent || value!=p->key){out[x*2]=(uint8_t)packed;out[x*2+1]=(uint8_t)(packed>>8);}
                }
            }
            ok=1;
        }
        ended=game_application_glyph_end(a);return ok && ended && !a->memory.fault;
    }
    return w->back && copy_surface(w->back,p->surface,source,destination,transparent,p->key,0);
}
int game_menu_accelerated_text_begin(GameApplication *a,GameAddress identity,const GameUiRect *destination,
        const GameUiRect *clip,GameTextTarget *target,int32_t *fallback){
    MenuWindow *w=a->menu_window;GameUiRect visible,source;GameMenuPixelSpan span;
    int gameplay=game_memory_read_u32(&a->memory,0x5840f0)!=0;
    extern int game_application_glyph_begin(GameApplication *,GameTextTarget *,int32_t *);
    extern int game_application_glyph_end(GameApplication *);
    if(!w || !destination || !clip || !target || !fallback || w->text_lock_pixels)return 0;
    visible=*clip;if(visible.left<0)visible.left=0;if(visible.top<0)visible.top=0;
    if(visible.right>(int32_t)w->width)visible.right=(int32_t)w->width;
    if(visible.bottom>(int32_t)w->height)visible.bottom=(int32_t)w->height;
    if(identity){GameUiRect area=*destination;
        if(area.left<visible.left)area.left=visible.left;if(area.top<visible.top)area.top=visible.top;
        if(area.right>visible.right)area.right=visible.right;if(area.bottom>visible.bottom)area.bottom=visible.bottom;
        if(area.right>area.left && area.bottom>area.top){
            source.left=area.left-destination->left;source.top=area.top-destination->top;
            source.right=area.right-destination->left;source.bottom=area.bottom-destination->top;
            if(!game_menu_text_surface_draw(a,identity,&source,&area,0))return 0;
        }
    }
    if(gameplay){if(!game_application_glyph_begin(a,target,fallback))return 0;}
    else{
        if(!surface_span(w->back,&span) || !SDL_LockSurface(w->back))return 0;
        memset(target,0,sizeof(*target));target->stride=span.stride;target->pixels=span.pixels;target->pixel_bytes=span.bytes;
        w->text_lock_pixels=span.pixels;*fallback=108;
    }
    game_memory_write_u32(&a->memory,0x59d450,(uint32_t)visible.left);game_memory_write_u32(&a->memory,0x59d454,(uint32_t)visible.right);
    game_memory_write_u32(&a->memory,0x59d458,(uint32_t)visible.top);game_memory_write_u32(&a->memory,0x59d45c,(uint32_t)visible.bottom);
    if(a->memory.fault){
        if(gameplay)(void)game_application_glyph_end(a);else{SDL_UnlockSurface(w->back);w->text_lock_pixels=NULL;}
        return 0;
    }
    return 1;
}
int game_menu_accelerated_text_end(GameApplication *a){
    MenuWindow *w=a->menu_window;
    if(game_memory_read_u32(&a->memory,0x5840f0)){
        extern int game_application_glyph_end(GameApplication *);return game_application_glyph_end(a);
    }
    if(!w || !w->back || !w->text_lock_pixels)return 0;
    SDL_UnlockSurface(w->back);w->text_lock_pixels=NULL;return 1;
}
int game_menu_frame_rectangles(GameApplication *a,const GameUiRect *rectangles,const uint8_t *colors,size_t count){
    MenuWindow *w=a->menu_window;GameMenuPixelSpan span;int result;
    if(!w || !rectangles || !colors || count>4096 || w->text_lock_pixels || !surface_span(w->back,&span) || !SDL_LockSurface(w->back))return 0;
    result=game_ui_indexed_borders(span.pixels,span.bytes,span.width,span.height,span.stride,rectangles,colors,count);
    SDL_UnlockSurface(w->back);return result;
}
int game_menu_game_palette(GameApplication *a,const uint32_t values[256]){
    MenuWindow *w=a->menu_window;SDL_Color colors[256];unsigned i;if(!w || !values)return 0;
    if(!w->palette){w->palette=SDL_CreatePalette(256);if(!w->palette)return 0;}
    for(i=0;i<256;++i){colors[i].r=(uint8_t)(values[i]>>16);colors[i].g=(uint8_t)(values[i]>>8);colors[i].b=(uint8_t)values[i];colors[i].a=0;}
    return palette_set(w,colors);
}
