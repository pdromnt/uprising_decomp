#include "app_identity.h"
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include "menu_widgets.h"
#include "portable_input_devices.h"
#include "scene_projection_storage.h"
#include "game_text_rasterizer.h"
#include "ui_geometry.h"
#include "gameplay_state.h"
#include "level_world_surface.h"

typedef HRESULT (WINAPI *NativeDrawCreate)(GUID *,LPDIRECTDRAW *,IUnknown *);
typedef struct MenuTextSurface {
    GameAddress identity;LPDIRECTDRAWSURFACE surface;void *locked_pixels;struct MenuTextSurface *next;
} MenuTextSurface;
typedef struct MenuWindow {
    GameAddress identity;
    HWND handle;
    HINSTANCE instance;
    HMODULE library;
    LPDIRECTDRAW draw;
    LPDIRECTDRAWSURFACE front,back;
    LPDIRECTDRAWPALETTE palette;
    PALETTEENTRY colors[256];
    unsigned width,height;
    int hidden,exclusive,windowed,native_draw;
    int pointer_capture_requested,pointer_clipped;
    RECT pointer_clip_bounds;
    int32_t isolated_pointer[2];
    ATOM window_class;
    char class_name[128];
    HFONT fonts[7];
    MenuTextSurface *text_surfaces;
    void *frame_pixels;
    char font_paths[2][128];
    int font_added[2];
    void *text_lock_pixels;
} MenuWindow;
static int offscreen_window(const MenuWindow *w){return w->hidden || w->native_draw;}
static int present_windowed_front(MenuWindow *);
static int pointer_clip_release(MenuWindow *w){
    if(w && w->pointer_clipped){
        RECT current;
        if(!GetClipCursor(&current))return 0;
        if(EqualRect(&current,&w->pointer_clip_bounds) && !ClipCursor(NULL))return 0;
        w->pointer_clipped=0;
    }
    return 1;
}
/*The HWND owner also owns confinement. Never acquire it for background or
 *automated play, and never release another application's confinement. */
int game_menu_pointer_capture(GameApplication *a,int enabled){
    MenuWindow *w=a->menu_window;RECT client;POINT first,last;
    if(!w)return !enabled;
    if(game_application_isolated_input(a))return 1;
    if(game_memory_read_u32(&a->memory,0x5acbe4) || game_memory_read_u32(&a->memory,0x5910a4))enabled=0;
    w->pointer_capture_requested=enabled!=0;
    if(!enabled || w->hidden || GetForegroundWindow()!=w->handle || IsIconic(w->handle))
        return pointer_clip_release(w);
    if(!GetClientRect(w->handle,&client))return 0;
    if(client.right<=client.left || client.bottom<=client.top)return pointer_clip_release(w);
    first.x=client.left;first.y=client.top;last.x=client.right;last.y=client.bottom;
    if(!ClientToScreen(w->handle,&first) || !ClientToScreen(w->handle,&last))return 0;
    client.left=first.x;client.top=first.y;client.right=last.x;client.bottom=last.y;
    if(!ClipCursor(&client))return 0;
    w->pointer_clip_bounds=client;w->pointer_clipped=1;return 1;
}

/*418f20: post the actual level-loop termination message once. */
int game_level_request_end(GameApplication *a){
    if(game_memory_read_u32(&a->memory,0x5ac970))return !a->memory.fault;
    if(!a->menu_window || !a->menu_window->handle)return 0;
    game_memory_write_u32(&a->memory,0x5ac970,1);
    return !a->memory.fault && PostMessageA(a->menu_window->handle,WM_USER+7,0,0)!=0;
}

/* Actual shared input/MCI/font services; they are still mandatory native links. */


extern int game_menu_music_notify(GameApplication *,uint32_t,uintptr_t); /* 00542e60 */
extern void game_application_splash_end(GameApplication *);
static MenuTextSurface *text_surface_find(MenuWindow *,GameAddress);

static int window_name(GameMemory *m,GameAddress at,char name[128]) {
    unsigned i;
    for(i=0;i<128u;++i) {
        name[i]=(char)game_memory_read_u8(m,at+i);
        if(m->fault)return 0;
        if(!name[i])return 1;
    }
    return 0;
}
/* 004aae70 and the matching 004aa8c0 font teardown. Font objects remain native
 * pointers; legacy getters select them by their fixed-width member identities.
 * Private registration scopes the optional retail fonts to this process. */
static void game_menu_window_resources_end(GameApplication *a) {
    MenuWindow *w=a->menu_window;unsigned i;
    if(!w)return;
    for(i=0;i<7u;++i) {
        if(w->fonts[i])DeleteObject(w->fonts[i]);
        w->fonts[i]=NULL;game_memory_write_u32(&a->memory,w->identity+0x80u+i*4u,0);
    }
    for(i=0;i<2u;++i) {
        if(w->font_added[i])RemoveFontResourceExA(w->font_paths[i],FR_PRIVATE,NULL);
        w->font_added[i]=0;game_memory_write_u32(&a->memory,0x58fd34u+i*4u,0);
    }
}
static int game_menu_window_resources_begin(GameApplication *a) {
    static const GameAddress names[7]={0x58fd98u,0x58fdacu,0x58fda0u,0x58fdb4u,0x58fdc0u,0x58fdccu,0x58fddcu};
    static const int heights[7]={-9,-10,-9,-12,-14,-15,-50};
    MenuWindow *w=a->menu_window;unsigned i;LOGFONTA font;char face[128];
    if(!w)return 0;
    for(i=0;i<2u;++i) {
        if(!window_name(&a->memory,i?0x58fd84u:0x58fd70u,w->font_paths[i]))return 0;
        w->font_added[i]=AddFontResourceExA(w->font_paths[i],FR_PRIVATE,NULL)!=0;
        game_memory_write_u32(&a->memory,0x58fd34u+i*4u,(uint32_t)w->font_added[i]);
    }
    for(i=0;i<7u;++i) {
        if(!window_name(&a->memory,names[i],face) || strlen(face)>=LF_FACESIZE)goto fail;
        memset(&font,0,sizeof(font));font.lfHeight=heights[i];font.lfWeight=i<4u?400:700;
        font.lfCharSet=DEFAULT_CHARSET;font.lfOutPrecision=OUT_TT_ONLY_PRECIS;
        memcpy(font.lfFaceName,face,strlen(face)+1u);
        w->fonts[i]=CreateFontIndirectA(&font);if(!w->fonts[i])goto fail;
        game_memory_write_u32(&a->memory,w->identity+0x80u+i*4u,w->identity+0x80u+i*4u);
    }
    return !a->memory.fault;
fail:game_menu_window_resources_end(a);return 0;
}
void *game_menu_window_font(GameApplication *a,unsigned index) {
    MenuWindow *w=a->menu_window;return w && index<7u?w->fonts[index]:NULL;
}
static void acquire_input(GameApplication *a) {
    if(!game_menu_input_acquire(a,0) || !game_menu_input_acquire(a,1) ||
       (game_memory_read_u32(&a->memory,0x604768u) && !game_menu_input_acquire(a,2)))a->memory.fault=1;
}
/* 00505f00: native MSG payloads and window handles remain full-width. */
static LRESULT CALLBACK window_procedure(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
    GameApplication *a=(GameApplication *)GetWindowLongPtrA(hwnd,GWLP_USERDATA);
    if(message==WM_NCCREATE) {
        a=((CREATESTRUCTA *)lparam)->lpCreateParams;
        SetWindowLongPtrA(hwnd,GWLP_USERDATA,(LONG_PTR)a);
    }
    if(a) {
        GameMemory *m=&a->memory;
        switch(message) {
        case WM_SYSCOMMAND:
            if(a->menu_window && a->menu_window->windowed &&
               ((wparam&0xfff0u)==SC_SIZE || (wparam&0xfff0u)==SC_MAXIMIZE))return 0;
            break;
        case WM_CLOSE:
            /* Let the application unwind gameplay/SDK resources before the
             * window owner destroys its HWND and DirectDraw surfaces. */
            (void)pointer_clip_release(a->menu_window);
            game_memory_write_u32(m,0x59d4c8,1);PostQuitMessage(0);return 0;
        case WM_DESTROY:(void)pointer_clip_release(a->menu_window);game_memory_write_u32(m,0x59d4c8,1);PostQuitMessage(0);break;
        case WM_USER+7:game_memory_write_u32(m,0x5ac970,1);return 0;
        case WM_SETFOCUS:
            acquire_input(a);
            if(a->menu_window && !game_menu_pointer_capture(a,a->menu_window->pointer_capture_requested))m->fault=1;
            break;
        case WM_KILLFOCUS:
            if(!pointer_clip_release(a->menu_window))m->fault=1;
            break;
        case WM_MOVE:case WM_SIZE:
            if(a->menu_window && a->menu_window->pointer_capture_requested &&
               !game_menu_pointer_capture(a,1))m->fault=1;
            break;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:
            /*The native GDI destination is disposable; expose/resize must
             *repaint from the existing owned front surface even when no menu
             *widget is dirty. Accelerated gameplay keeps its own presenter. */
            if(a->menu_window && a->menu_window->native_draw && a->menu_window->front &&
               !game_memory_read_u32(m,0x5840f0)){
                PAINTSTRUCT paint;
                BeginPaint(hwnd,&paint);EndPaint(hwnd,&paint);
                if(!present_windowed_front(a->menu_window))m->fault=1;
                return 0;
            }
            break;
        case WM_ACTIVATEAPP: {
            uint32_t active=game_memory_read_u32(m,0x6270f4u);
            /* Automated input owns its simulation activation as well as its
             * cursor. Working in another app must not pause the test or require
             * stealing desktop focus. Interactive play retains focus behavior. */
            if(a->menu_window && (a->menu_window->hidden || game_application_isolated_input(a))){
                game_memory_write_u32(m,0x6270f4u,1);break;
            }
            if(!active && wparam)acquire_input(a);
            else if(active && !wparam && !game_menu_input_unacquire(a))m->fault=1;
            game_memory_write_u32(m,0x6270f4u,wparam!=0);
            if(a->menu_window && !game_menu_pointer_capture(a,a->menu_window->pointer_capture_requested))m->fault=1;
            if(!wparam && (!a->menu_window || !a->menu_window->windowed)) {
                uint32_t state=game_memory_read_u32(m,0x624f8cu);
                if(state==8u || state==10u){CloseWindow(hwnd);ShowWindow(hwnd,SW_MINIMIZE);}
            }
            break;
        }
        case WM_SETCURSOR:
            if(game_application_isolated_input(a))break;
            SetCursor(game_memory_read_u32(m,0x597f8cu)?LoadCursorA(NULL,IDC_ARROW):NULL);return 1;
        case WM_DISPLAYCHANGE:
            if((!a->menu_window || !a->menu_window->windowed) && game_memory_read_u32(m,0x6270f4u) && !IsIconic(hwnd)) {
                RECT bounds={0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
                AdjustWindowRectEx(&bounds,WS_POPUP,0,0);
                SetWindowPos(hwnd,NULL,bounds.left,bounds.top,bounds.right-bounds.left,
                    bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);
            }
            break;
        case MM_MCINOTIFY:
            if(game_memory_read_u32(m,0x59d828u) &&
                !game_menu_music_notify(a,(uint32_t)wparam,(uintptr_t)lparam))m->fault=1;
            break;
        case WM_USER+8:
            if(!game_menu_input_acquire(a,(unsigned)wparam))m->fault=1;
            break;
        default:break;
        }
    }
    return DefWindowProcA(hwnd,message,wparam,lparam);
}
static int create_surfaces(MenuWindow *w) {
    DDSURFACEDESC desc;DDSCAPS caps;HRESULT result;
    memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
    if(offscreen_window(w)) {
        desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
        desc.dwWidth=w->width;desc.dwHeight=w->height;
        desc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;
        desc.ddpfPixelFormat.dwSize=sizeof(desc.ddpfPixelFormat);
        desc.ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_PALETTEINDEXED8;
        desc.ddpfPixelFormat.dwRGBBitCount=8;
        if(FAILED(IDirectDraw_CreateSurface(w->draw,&desc,&w->front,NULL)))return 0;
        if(FAILED(IDirectDraw_CreateSurface(w->draw,&desc,&w->back,NULL)))return 0;
    } else {
        desc.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT;desc.dwBackBufferCount=1;
        desc.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX|DDSCAPS_SYSTEMMEMORY;
        result=IDirectDraw_CreateSurface(w->draw,&desc,&w->front,NULL);
        if(FAILED(result))return 0;
        caps.dwCaps=DDSCAPS_BACKBUFFER;
        if(FAILED(IDirectDrawSurface_GetAttachedSurface(w->front,&caps,&w->back)))return 0;
    }
    return 1;
}
static void window_surfaces_release(MenuWindow *w) {
    if(w->frame_pixels && w->front){IDirectDrawSurface_Unlock(w->front,w->frame_pixels);w->frame_pixels=NULL;}
    if(w->text_lock_pixels && w->back){IDirectDrawSurface_Unlock(w->back,w->text_lock_pixels);w->text_lock_pixels=NULL;}
    if(w->palette){IDirectDrawPalette_Release(w->palette);w->palette=NULL;}
    if(w->back){IDirectDrawSurface_Release(w->back);w->back=NULL;}
    if(w->front){IDirectDrawSurface_Release(w->front);w->front=NULL;}
}
void *game_menu_window_handle(GameApplication *a) {return a->menu_window?a->menu_window->handle:NULL;}
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
extern int game_application_level_graphics_begin(GameApplication *);
/*4176e0 root creation/normal cooperative level followed by439930. The window
 * continues to own DirectDraw; no COM pointer is stored in a game word. */
/*418200 final516d20/4ac160 transition: restore the existing window owner's
 *exclusive menu device. Native COM pointers stay in MenuWindow. */
int game_level_display_restore_menu(GameApplication *a){
    MenuWindow *w=a->menu_window;NativeDrawCreate create;FARPROC entry;
    if(!w || !w->library || !game_level_display_end(a))return 0;
    if(w->draw){IDirectDraw_Release(w->draw);w->draw=NULL;}
    game_memory_write_u32(&a->memory,0x627068,0);
    game_memory_write_u32(&a->memory,w->identity+0x124,0);
    entry=GetProcAddress(w->library,"DirectDrawCreate");if(!entry)return 0;
    memcpy(&create,&entry,sizeof(create));
    if(FAILED(create(NULL,&w->draw,NULL)))return 0;
    if(FAILED(IDirectDraw_SetCooperativeLevel(w->draw,w->handle,
        offscreen_window(w)?DDSCL_NORMAL:DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT))){
        IDirectDraw_Release(w->draw);w->draw=NULL;return 0;
    }
    w->exclusive=!offscreen_window(w);
    game_memory_write_u32(&a->memory,0x627068,w->identity+0x124);
    game_memory_write_u32(&a->memory,w->identity+0x124,w->identity+0x124);
    return !a->memory.fault;
}
int game_level_display_begin(GameApplication *a) {
    MenuWindow *w=a->menu_window;NativeDrawCreate create;FARPROC entry;HRESULT result;char diagnostic[192];
    if(!w || !w->library || w->front || w->back || w->text_surfaces){
        snprintf(diagnostic,sizeof(diagnostic),"native display initialization has live menu surfaces: window=%p front=%p back=%p text=%p",
            (void *)w,w?(void *)w->front:NULL,w?(void *)w->back:NULL,w?(void *)w->text_surfaces:NULL);game_application_log(a,"%s\n",diagnostic);return 0;
    }
    if(w->draw) {IDirectDraw_Release(w->draw);w->draw=NULL;}
    game_memory_write_u32(&a->memory,0x627068,0);
    game_memory_write_u32(&a->memory,w->identity+0x124,0);
    entry=GetProcAddress(w->library,"DirectDrawCreate");
    if(!entry){snprintf(diagnostic,sizeof(diagnostic),"native display export unavailable: module=%p error=%lu",(void *)w->library,(unsigned long)GetLastError());game_application_log(a,"%s\n",diagnostic);return 0;}
    memcpy(&create,&entry,sizeof(create));
    result=create(NULL,&w->draw,NULL);
    if(FAILED(result)){snprintf(diagnostic,sizeof(diagnostic),"native display device creation failed: result=%08lx",(unsigned long)result);game_application_log(a,"%s\n",diagnostic);return 0;}
    result=IDirectDraw_SetCooperativeLevel(w->draw,w->handle,DDSCL_NORMAL);
    if(FAILED(result)) {
        snprintf(diagnostic,sizeof(diagnostic),"native display cooperative level failed: result=%08lx",(unsigned long)result);game_application_log(a,"%s\n",diagnostic);
        IDirectDraw_Release(w->draw);w->draw=NULL;return 0;
    }
    w->exclusive=0;
    game_memory_write_u32(&a->memory,0x627068,w->identity+0x124);
    game_memory_write_u32(&a->memory,w->identity+0x124,w->identity+0x124);
    return !a->memory.fault && game_application_level_graphics_begin(a);
}
/*The presenter stretches the owned game surface into its client rectangle.
 *Mouse messages, polled positions and warps must use that same mapping. */
static int pointer_scale(MenuWindow *w,POINT *point,int to_client){
    RECT client;int64_t x,y,wide,high;
    if(!GetClientRect(w->handle,&client) || !w->width || !w->height ||
       w->width>INT32_MAX || w->height>INT32_MAX)return 0;
    wide=(int64_t)client.right-client.left;high=(int64_t)client.bottom-client.top;
    if(wide<=0 || high<=0)return 0;
    if(to_client){
        x=(int64_t)point->x*wide/w->width+client.left;
        y=(int64_t)point->y*high/w->height+client.top;
    }else{
        x=((int64_t)point->x-client.left)*w->width/wide;
        y=((int64_t)point->y-client.top)*w->height/high;
    }
    if(x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX)return 0;
    point->x=(LONG)x;point->y=(LONG)y;return 1;
}
int game_menu_pointer_from_screen(GameApplication *a,int32_t coordinates[2]){
    MenuWindow *w=a->menu_window;POINT point;
    if(!coordinates)return 0;
    point.x=coordinates[0];point.y=coordinates[1];
    if(w && w->windowed && (!ScreenToClient(w->handle,&point) || !pointer_scale(w,&point,0)))return 0;
    coordinates[0]=(int32_t)point.x;coordinates[1]=(int32_t)point.y;return 1;
}
int game_menu_pointer_position(GameApplication *a,int32_t x,int32_t y){
    MenuWindow *w=a->menu_window;POINT point={(LONG)x,(LONG)y};
    if(game_application_isolated_input(a)){
        if(!w)return 0;
        w->isolated_pointer[0]=x;w->isolated_pointer[1]=y;return 1;
    }
    if(w && w->windowed && GetForegroundWindow()!=w->handle)return 1;
    if(w && w->windowed && (!pointer_scale(w,&point,1) || !ClientToScreen(w->handle,&point)))return 0;
    return SetCursorPos(point.x,point.y)!=0;
}
int game_menu_pointer_read(GameApplication *a,int32_t output[2]){
    MenuWindow *w=a->menu_window;POINT point;
    if(game_application_isolated_input(a)){
        if(!w || !output)return 0;
        memcpy(output,w->isolated_pointer,sizeof(w->isolated_pointer));return 1;
    }
    if(!output || !GetCursorPos(&point))return 0;
    output[0]=(int32_t)point.x;output[1]=(int32_t)point.y;
    return game_menu_pointer_from_screen(a,output);
}
/*518b40: the platform supplies a full-width POINT; ordinary game arithmetic
 *subtracts the viewport origin and clamps it without guest output pointers. */
int game_level_viewport_position(GameApplication *a,uint32_t output[2]){
    int32_t point[2];GameplayViewportClampResult result;
    if(!output || !game_menu_pointer_read(a,point))return 0;
    result=gameplay_state_clamp_viewport_values(&a->memory,(uint32_t)point[0],(uint32_t)point[1]);
    output[0]=result.x;output[1]=result.y;return !a->memory.fault;
}
/*004aba60 with004aac70/004ab800: return the existing window owner to menu
 * mode. Retire its old surfaces before changing mode; no second window or
 * competing pixel store is created. Fonts survive the display transition. */
int game_menu_window_mode_restore(GameApplication *a) {
    MenuWindow *w=a->menu_window;GameMemory *m=&a->memory;RECT bounds;unsigned i;
    if(!w || !w->draw)return 0;
    if(!game_menu_pointer_capture(a,0))return 0;
    while(w->text_surfaces)
        if(!game_menu_text_surface_release(a,w->text_surfaces->identity))return 0;
    window_surfaces_release(w);
    w->width=640;w->height=480;
    if(FAILED(IDirectDraw_SetCooperativeLevel(w->draw,w->handle,offscreen_window(w)?DDSCL_NORMAL:DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN)))return 0;
    w->exclusive=!offscreen_window(w);
    if(!offscreen_window(w) && FAILED(IDirectDraw_SetDisplayMode(w->draw,640,480,8)))return 0;
    if(!create_surfaces(w)) {window_surfaces_release(w);return 0;}
    if(!GetWindowRect(w->handle,&bounds))return 0;
    game_memory_write_u32(m,w->identity+0x4c,640);
    game_memory_write_u32(m,w->identity+0x50,480);
    game_memory_write_u32(m,w->identity+0x58,(uint32_t)bounds.left);
    game_memory_write_u32(m,w->identity+0x5c,(uint32_t)bounds.top);
    game_memory_write_u32(m,w->identity+0x60,(uint32_t)(bounds.right-bounds.left));
    game_memory_write_u32(m,w->identity+0x64,(uint32_t)(bounds.bottom-bounds.top));
    for(i=0;i<2;++i)game_memory_write_u32(m,w->identity+0x44+i*4,0);
    for(i=0;i<4;++i)game_memory_write_u32(m,w->identity+0x68+i*4,0);
    game_memory_write_u32(m,w->identity+0x128,w->identity+0x128);
    game_memory_write_u32(m,w->identity+0x12c,w->identity+0x12c);
    game_memory_write_u32(m,w->identity+0x138,640);
    game_memory_write_u32(m,w->identity+0x13c,480);
    game_memory_write_u32(m,0x626e78,w->identity+0x128);
    game_memory_write_u32(m,0x62707c,w->identity+0x12c);
    if(!game_application_isolated_input(a)){
        SetCursor(LoadCursorA(NULL,IDC_ARROW));
    }
    return !m->fault;
}
void game_menu_window_destroy(GameApplication *a,GameAddress identity) {
    MenuWindow *w=a->menu_window;
    if(!w || identity!=w->identity)return;
    (void)pointer_clip_release(w);
    /*Cooperative input and force-feedback services borrow this HWND. Retire
     *their owners while it is still valid; platform-stop repeats are harmless. */
    game_application_force_feedback_close(a,1);
    game_input_devices_end(a);
    while(w->text_surfaces)game_menu_text_surface_release(a,w->text_surfaces->identity);
    window_surfaces_release(w);
    if(w->draw) {
        if(w->exclusive)IDirectDraw_RestoreDisplayMode(w->draw);
        IDirectDraw_Release(w->draw);
    }
    game_menu_window_resources_end(a);
    if(w->handle)DestroyWindow(w->handle);
    if(w->window_class)UnregisterClassA(w->class_name,w->instance);
    if(w->library)FreeLibrary(w->library);
    game_memory_write_u32(&a->memory,0x627068u,0);
    game_memory_write_u32(&a->memory,0x626e78u,0);
    game_memory_write_u32(&a->memory,0x62707cu,0);
    game_memory_write_u32(&a->memory,0x627904u,0);
    scene_projection_storage_free(w->identity);free(w);a->menu_window=NULL;
}
/* 004aa780/004ab2f0/004aac70/004ab800: real Win32 and DirectDraw objects.
 * Hidden execution uses actual indexed offscreen surfaces and the same C draw
 * consumers; it neither changes the desktop mode nor creates a visible window. */
GameAddress game_menu_window_construct(GameApplication *a,void *instance,unsigned width,unsigned height) {
    MenuWindow *w;WNDCLASSA cls;char title[128];NativeDrawCreate create;RECT bounds;
    GameMemory *m=&a->memory;GameAddress p;
    if(a->menu_window || !width || !height || width>16384u || height>16384u)return 0;
    w=calloc(1,sizeof(*w));if(!w)return 0;
    p=scene_projection_storage_allocate(0x140u,"portable menu window");
    if(!p){free(w);return 0;}
    w->identity=p;w->instance=instance;w->width=width;w->height=height;a->menu_window=w;
    w->isolated_pointer[0]=(int32_t)(width/2);w->isolated_pointer[1]=(int32_t)(height/2);
    w->hidden=GetEnvironmentVariableA("UPRISING_RECOMP_HEADLESS",NULL,0)!=0;
    w->windowed=GetEnvironmentVariableA("UPRISING_RECOMP_WINDOWED",NULL,0)!=0;
    w->native_draw=w->windowed && GetEnvironmentVariableA("UPRISING_RECOMP_NATIVE_WINDOWED_DDRAW",NULL,0)!=0;
    if(w->native_draw)game_application_log(a,"native windowed DirectDraw: %s\n","system offscreen surfaces");
    if(!window_name(m,0x58fd18u,w->class_name))goto fail;
    strcpy(title,UPRISING_WINDOW_TITLE);
    memset(&cls,0,sizeof(cls));cls.lpfnWndProc=window_procedure;cls.hInstance=instance;
    cls.hIcon=LoadIconA(instance,MAKEINTRESOURCEA(UPRISING_ICON_ID));cls.hbrBackground=GetStockObject(BLACK_BRUSH);
    cls.lpszClassName=w->class_name;
    w->window_class=RegisterClassA(&cls);if(!w->window_class)goto fail;
    {DWORD style=w->windowed?(WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX):WS_POPUP|WS_SYSMENU;
     RECT client={0,0,(LONG)width,(LONG)height};
     if(w->windowed && !AdjustWindowRectEx(&client,style,FALSE,0))goto fail;
     w->handle=CreateWindowExA(w->hidden || w->windowed?0:WS_EX_TOPMOST,w->class_name,title,style,
        0,0,client.right-client.left,client.bottom-client.top,NULL,NULL,instance,a);}
    if(!w->handle || !game_menu_window_resources_begin(a))goto fail;
    SendMessageA(w->handle,WM_SETICON,ICON_BIG,(LPARAM)cls.hIcon);
    SendMessageA(w->handle,WM_SETICON,ICON_SMALL,(LPARAM)LoadImageA(instance,
        MAKEINTRESOURCEA(UPRISING_ICON_ID),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
    w->library=game_application_backend_library(a,"ddraw.dll");if(!w->library)goto fail;
    {
        FARPROC procedure=GetProcAddress(w->library,"DirectDrawCreate");
        typedef char NativeProcedureWidth[(sizeof(procedure)==sizeof(create))?1:-1];
        (void)sizeof(NativeProcedureWidth);memcpy(&create,&procedure,sizeof(create));
    }
    if(!create || FAILED(create(NULL,&w->draw,NULL)))goto fail;
    if(FAILED(IDirectDraw_SetCooperativeLevel(w->draw,w->handle,offscreen_window(w)?DDSCL_NORMAL:
        DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT)))goto fail;
    if(!offscreen_window(w)) {
        if(FAILED(IDirectDraw_SetDisplayMode(w->draw,width,height,8u)))goto fail;
        w->exclusive=1;
    }
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
    if(!w->hidden){ShowWindow(w->handle,w->windowed?SW_SHOWNOACTIVATE:SW_SHOWMAXIMIZED);UpdateWindow(w->handle);}
    return p;
fail:game_menu_window_destroy(a,p);return 0;
}
int game_menu_surface_prepare(GameApplication *a,int *restored) {
    MenuWindow *w=a->menu_window;HRESULT result;
    if(!w || !w->front || !w->back || !restored)return 0;
    *restored=0;result=IDirectDrawSurface_IsLost(w->front);
    if(result==DDERR_SURFACELOST) {
        if(FAILED(IDirectDrawSurface_Restore(w->front)) || FAILED(IDirectDrawSurface_Restore(w->back)))return 0;
        if(w->palette && (FAILED(IDirectDrawSurface_SetPalette(w->front,w->palette)) ||
            FAILED(IDirectDrawSurface_SetPalette(w->back,w->palette))))return 0;
        *restored=1;
    } else if(FAILED(result))return 0;
    return 1;
}
/*Native indexed offscreen surfaces already provide palette-aware GDI DCs.
 *Present the existing front pixels directly; no second framebuffer owner or
 *desktop display-mode transition is needed for the native windowed service. */
static int present_windowed_front(MenuWindow *w){
    HDC source=NULL,destination=NULL;RECT client;int copied=0,released;
    if(!w->native_draw || w->hidden || !IsWindowVisible(w->handle) || IsIconic(w->handle))return 1;
    if(!GetClientRect(w->handle,&client))return 0;
    if(client.right<=client.left || client.bottom<=client.top)return 1;
    if(FAILED(IDirectDrawSurface_GetDC(w->front,&source)))return 0;
    destination=GetDC(w->handle);
    if(destination){
        SetStretchBltMode(destination,COLORONCOLOR);
        copied=StretchBlt(destination,0,0,client.right-client.left,client.bottom-client.top,
            source,0,0,(int)w->width,(int)w->height,SRCCOPY)!=0;
        ReleaseDC(w->handle,destination);
    }
    released=SUCCEEDED(IDirectDrawSurface_ReleaseDC(w->front,source));
    return copied && released;
}
int game_menu_surface_present_rectangle(GameApplication *a,const GameUiRect *rectangle) {
    MenuWindow *w=a->menu_window;RECT clip,full;HRESULT result;
    if(!w || !rectangle)return 0;
    clip.left=rectangle->left;clip.top=rectangle->top;clip.right=rectangle->right;clip.bottom=rectangle->bottom;
    result=IDirectDrawSurface_BltFast(w->front,(DWORD)clip.left,(DWORD)clip.top,w->back,&clip,DDBLTFAST_WAIT);
    if(SUCCEEDED(result))return present_windowed_front(w);
    /* Original preserves the back contents on ordinary presentation; flipping
     * before these copy attempts breaks the dirty-region algorithm. */
    full.left=full.top=0;full.right=(LONG)w->width;full.bottom=(LONG)w->height;
    if(SUCCEEDED(IDirectDrawSurface_Blt(w->front,&full,w->back,&full,DDBLT_WAIT,NULL)))return present_windowed_front(w);
    return !offscreen_window(w) && SUCCEEDED(IDirectDrawSurface_Flip(w->front,NULL,DDFLIP_WAIT));
}
int game_menu_present_frame(GameApplication *a,uint32_t interval) {
    MenuWindow *w=a->menu_window;GameUiRect full;int restored;
    if(game_memory_read_u32(&a->memory,0x5840f0))return game_level_screen_present(a,interval);
    if(!w || !game_menu_surface_prepare(a,&restored))return 0;
    full.left=full.top=0;full.right=(int32_t)w->width;full.bottom=(int32_t)w->height;
    if(interval)Sleep(interval);
    return game_menu_surface_present_rectangle(a,&full);
}
int game_menu_level_surface_flip(GameApplication *a){
    MenuWindow *w=a->menu_window;GameAddress surface=game_memory_read_u32(&a->memory,0x626e78);
    if(a->memory.fault)return 0;
    if(!surface)return 1;
    if(!w || !w->front || surface!=w->identity+0x128)return 0;
    if(w->native_draw){GameUiRect full={0,0,(int32_t)w->width,(int32_t)w->height};
        return game_menu_surface_present_rectangle(a,&full);}
    return SUCCEEDED(IDirectDrawSurface_Flip(w->front,NULL,DDFLIP_WAIT));
}
static int lock_back(MenuWindow *w,DDSURFACEDESC *desc) {
    memset(desc,0,sizeof(*desc));desc->dwSize=sizeof(*desc);
    if(!w || !w->back || FAILED(IDirectDrawSurface_Lock(w->back,NULL,desc,DDLOCK_WAIT,NULL)))return 0;
    if(!desc->lpSurface || desc->ddpfPixelFormat.dwRGBBitCount!=8u ||
        (int64_t)desc->lPitch<(int64_t)w->width) {
        IDirectDrawSurface_Unlock(w->back,desc->lpSurface);return 0;
    }
    return 1;
}
int game_menu_screen_mask(GameApplication *a,int enabled) {
    MenuWindow *w=a->menu_window;DDBLTFX fx;
    if(game_memory_read_u32(&a->memory,0x5840f0))return game_level_screen_clear(a,enabled);
    if(!w || !w->back)return 0;
    memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwFillColor=(DWORD)enabled;
    return SUCCEEDED(IDirectDrawSurface_Blt(w->back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx));
}
int game_menu_palette(GameApplication *a,GameAddress source) {
    MenuWindow *w=a->menu_window;MenuTextSurface *text;PALETTEENTRY entries[256];unsigned i;
    if(game_memory_read_u32(&a->memory,0x5840f0))return game_level_screen_palette(a,source);
    if(!w)return 0;
    for(i=0;i<256u;++i) {
        entries[i].peRed=game_memory_read_u8(&a->memory,source+i*4u);
        entries[i].peGreen=game_memory_read_u8(&a->memory,source+i*4u+1u);
        entries[i].peBlue=game_memory_read_u8(&a->memory,source+i*4u+2u);
        entries[i].peFlags=game_memory_read_u8(&a->memory,source+i*4u+3u);
    }
    if(a->memory.fault)return 0;
    if(w->palette) {
        if(FAILED(IDirectDrawPalette_SetEntries(w->palette,0,0,256u,entries)))return 0;
    } else if(FAILED(IDirectDraw_CreatePalette(w->draw,DDPCAPS_8BIT|DDPCAPS_ALLOW256,entries,&w->palette,NULL)))return 0;
    if(FAILED(IDirectDrawSurface_SetPalette(w->front,w->palette)) ||
        FAILED(IDirectDrawSurface_SetPalette(w->back,w->palette)))return 0;
    for(text=w->text_surfaces;text;text=text->next)
        if(FAILED(IDirectDrawSurface_SetPalette(text->surface,w->palette)))return 0;
    game_memory_write_u32(&a->memory,0x597f64,w->identity+0x130);
    memcpy(w->colors,entries,sizeof(entries));return !a->memory.fault;
}
/* Full-width palette owner; byte arguments use the verified PALETTEENTRY
 * wire layout and never expose a COM pointer to the game records. */
int game_menu_palette_read_bytes(GameApplication *a,uint8_t output[1024]) {
    MenuWindow *w=a->menu_window;PALETTEENTRY entries[256];unsigned i;
    if(!w || !w->palette || FAILED(IDirectDrawPalette_GetEntries(w->palette,0,0,256,entries)))return 0;
    for(i=0;i<256;++i){output[i*4]=entries[i].peRed;output[i*4+1]=entries[i].peGreen;
        output[i*4+2]=entries[i].peBlue;output[i*4+3]=entries[i].peFlags;}
    return 1;
}
int game_menu_palette_write_bytes(GameApplication *a,const uint8_t input[1024]) {
    MenuWindow *w=a->menu_window;PALETTEENTRY entries[256];unsigned i;
    if(!w || !w->draw || !w->front)return 0;
    for(i=0;i<256;++i){entries[i].peRed=input[i*4];entries[i].peGreen=input[i*4+1];
        entries[i].peBlue=input[i*4+2];entries[i].peFlags=input[i*4+3];}
    if(w->palette) {
        if(FAILED(IDirectDrawPalette_SetEntries(w->palette,0,0,256,entries)))return 0;
    } else {
        /* Match initial creation after gameplay retires the palette too.
         * Debrief uses white at0 and black at255. Without ALLOW256, DirectDraw
         * reserves those endpoints while the bitmap mapper assumes our colors. */
        if(FAILED(IDirectDraw_CreatePalette(w->draw,DDPCAPS_8BIT|DDPCAPS_ALLOW256,entries,&w->palette,NULL)))return 0;
        if(FAILED(IDirectDrawSurface_SetPalette(w->front,w->palette)) ||
            (w->back && FAILED(IDirectDrawSurface_SetPalette(w->back,w->palette)))) {
            IDirectDrawPalette_Release(w->palette);w->palette=NULL;return 0;
        }
    }
    game_memory_write_u32(&a->memory,0x597f64,w->identity+0x130);
    memcpy(w->colors,entries,sizeof(entries));return !a->memory.fault;
}
int game_menu_fullscreen_image(GameApplication *a,GameAddress source,uint32_t stride) {
    MenuWindow *w=a->menu_window;DDSURFACEDESC desc;unsigned y;int ok=1;
    if(game_memory_read_u32(&a->memory,0x5840f0))return game_level_screen_image(a,source,stride);
    if(!w || stride<w->width || (uint64_t)source+(uint64_t)stride*w->height>UINT32_MAX || !lock_back(w,&desc))return 0;
    for(y=0;y<w->height;++y)if(!game_memory_read(&a->memory,source+y*stride,
        (uint8_t *)desc.lpSurface+(ptrdiff_t)y*desc.lPitch,w->width)){ok=0;break;}
    return SUCCEEDED(IDirectDrawSurface_Unlock(w->back,desc.lpSurface)) && ok;
}
static int draw_bitmap_surface(GameApplication *a,LPDIRECTDRAWSURFACE surface,unsigned width,unsigned height,
        const AssetBitmap *image,const GameUiRect *source,const GameUiRect *destination,int transparent,uint32_t key,int mirror) {
    MenuWindow *w=a->menu_window;DDSURFACEDESC desc;int64_t dx,dy,sx,sy;int32_t x,y;int ok=1;
    uint8_t mapping[256];unsigned i,j;
    if(!w || !surface || !image || !source || !destination)return 0;
    dx=(int64_t)destination->right-destination->left;dy=(int64_t)destination->bottom-destination->top;
    sx=(int64_t)source->right-source->left;sy=(int64_t)source->bottom-source->top;
    if(dx<=0 || dy<=0 || sx<=0 || sy<=0)return 1;
    if(source->left<0 || source->top<0 || (uint32_t)source->right>image->width ||
        (uint32_t)source->bottom>image->height ||
        (!image->rgb24 && !image->palette_bgra) || image->palette_count>256u)return 0;
    memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
    if(FAILED(IDirectDrawSurface_GetSurfaceDesc(surface,&desc)))return 0;
    if(desc.ddpfPixelFormat.dwRGBBitCount==16 && desc.ddpfPixelFormat.dwRBitMask==0xf800 &&
       desc.ddpfPixelFormat.dwGBitMask==0x7e0 && desc.ddpfPixelFormat.dwBBitMask==31){
        int32_t from[4]={source->left,source->top,source->right,source->bottom};
        int32_t to[4]={destination->left,destination->top,destination->right,destination->bottom};
        if(FAILED(IDirectDrawSurface_Lock(surface,NULL,&desc,DDLOCK_WAIT,NULL)))return 0;
        ok=desc.lPitch>0 && (uint64_t)(uint32_t)desc.lPitch*height<=SIZE_MAX &&
            asset_bitmap_blit_rgb565(image,from,to,desc.lpSurface,(size_t)desc.lPitch*height,width,height,(size_t)desc.lPitch,transparent,key,mirror);
        return SUCCEEDED(IDirectDrawSurface_Unlock(surface,desc.lpSurface)) && ok;
    }
    if(image->rgb24 && !w->palette)return 0;
    if(!w->palette) {
        MenuTextSurface *text;PALETTEENTRY entries[256];memset(entries,0,sizeof(entries));
        for(i=0;i<image->palette_count;++i) {
            entries[i].peRed=image->palette_bgra[i*4u+2u];entries[i].peGreen=image->palette_bgra[i*4u+1u];
            entries[i].peBlue=image->palette_bgra[i*4u];
        }
        if(FAILED(IDirectDraw_CreatePalette(w->draw,DDPCAPS_8BIT|DDPCAPS_ALLOW256,entries,&w->palette,NULL)) ||
            (w->front && FAILED(IDirectDrawSurface_SetPalette(w->front,w->palette))) ||
            (w->back && FAILED(IDirectDrawSurface_SetPalette(w->back,w->palette))))return 0;
        for(text=w->text_surfaces;text;text=text->next)
            if(FAILED(IDirectDrawSurface_SetPalette(text->surface,w->palette)))return 0;
        memcpy(w->colors,entries,sizeof(entries));
    }
    /* DIB colors select the existing surface palette. Equal entries retain
     * their index; otherwise use the native indexed nearest-color rule. */
    for(i=0;!image->rgb24 && i<256u;++i) {
        uint32_t best=UINT32_MAX;unsigned chosen=0;
        if(i>=image->palette_count){mapping[i]=(uint8_t)i;continue;}
        if(w->colors[i].peRed==image->palette_bgra[i*4u+2u] &&
            w->colors[i].peGreen==image->palette_bgra[i*4u+1u] &&
            w->colors[i].peBlue==image->palette_bgra[i*4u]) {mapping[i]=(uint8_t)i;continue;}
        for(j=0;j<256u;++j) {
            int r=(int)w->colors[j].peRed-image->palette_bgra[i*4u+2u];
            int g=(int)w->colors[j].peGreen-image->palette_bgra[i*4u+1u];
            int b=(int)w->colors[j].peBlue-image->palette_bgra[i*4u];
            uint32_t distance=(uint32_t)(r*r+g*g+b*b);
            if(distance<best){best=distance;chosen=j;}
        }
        mapping[i]=(uint8_t)chosen;
    }
    memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
    if(FAILED(IDirectDrawSurface_Lock(surface,NULL,&desc,DDLOCK_WAIT,NULL)))return 0;
    if(!desc.lpSurface || desc.ddpfPixelFormat.dwRGBBitCount!=8u || (int64_t)desc.lPitch<(int64_t)width) {
        IDirectDrawSurface_Unlock(surface,desc.lpSurface);return 0;
    }
    for(y=destination->top<0?0:destination->top;y<destination->bottom && (uint32_t)y<height;++y) {
        int64_t row=source->top+((int64_t)y-destination->top)*sy/dy;
        if(mirror)row=source->bottom-1-(row-source->top);
        uint8_t *output=(uint8_t *)desc.lpSurface+(ptrdiff_t)y*desc.lPitch;
        size_t row_offset=(size_t)(image->top_down?(uint32_t)row:image->height-1u-(uint32_t)row)*image->stride;
        if(row_offset>image->pixel_bytes ||
            (size_t)image->width*(image->rgb24?3u:1u)>image->pixel_bytes-row_offset){ok=0;break;}
        for(x=destination->left<0?0:destination->left;x<destination->right && (uint32_t)x<width;++x) {
            size_t column=(size_t)(source->left+((int64_t)x-destination->left)*sx/dx);
            if(mirror)column=(size_t)source->right-1u-(column-(size_t)source->left);
            if(image->rgb24) {
                const uint8_t *pixel=image->pixels+row_offset+column*3u;
                uint32_t value=(uint32_t)pixel[0]|(uint32_t)pixel[1]<<8|(uint32_t)pixel[2]<<16;
                uint32_t best=UINT32_MAX;unsigned chosen=0;
                if(transparent && value==(key&0xffffffu))continue;
                for(j=0;j<256u;++j) {
                    int r=(int)w->colors[j].peRed-pixel[2];
                    int g=(int)w->colors[j].peGreen-pixel[1];
                    int b=(int)w->colors[j].peBlue-pixel[0];
                    uint32_t distance=(uint32_t)(r*r+g*g+b*b);
                    if(distance<best){best=distance;chosen=j;if(!distance)break;}
                }
                output[x]=(uint8_t)chosen;
            } else {
                uint8_t value=image->pixels[row_offset+column];
                if(!transparent || value!=(uint8_t)key)output[x]=mapping[value];
            }
        }
    }
    return SUCCEEDED(IDirectDrawSurface_Unlock(surface,desc.lpSurface)) && ok;
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
/*004ab730: clear the native window and its existing front/back surfaces before
 * video/game transitions. No guest COM vtables or extra renderer owner. */
int game_menu_window_clear(GameApplication *a) {
    MenuWindow *w=a->menu_window;HDC dc;RECT area;HGDIOBJ old;DDBLTFX fx={0};int ok=1;
    if(!w)return 0;
    dc=GetDC(w->handle);
    if(dc) {
        old=SelectObject(dc,GetStockObject(BLACK_BRUSH));
        if(GetClientRect(w->handle,&area))Rectangle(dc,area.left,area.top,area.right,area.bottom);
        if(old && old!=HGDI_ERROR)SelectObject(dc,old);
        ReleaseDC(w->handle,dc);
    }
    fx.dwSize=sizeof(fx);fx.dwFillColor=0;
    if(w->front && FAILED(IDirectDrawSurface_Blt(w->front,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx)))ok=0;
    if(w->back && FAILED(IDirectDrawSurface_Blt(w->back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx)))ok=0;
    if(w->draw && !w->native_draw)IDirectDraw_FlipToGDISurface(w->draw);
    return ok;
}
/* Composed controls reuse their existing native DDS owner and indexed mapper;
 * no persistent software bitmap or serialized surface mirror is introduced. */
int game_menu_draw_bitmap_on_surface(GameApplication *a,GameAddress identity,const AssetBitmap *image,
    const GameUiRect *source,const GameUiRect *destination,int transparent,uint32_t key,int mirror) {
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);DDSURFACEDESC desc={0};
    if(!p)return 0;
    desc.dwSize=sizeof(desc);if(FAILED(IDirectDrawSurface_GetSurfaceDesc(p->surface,&desc)))return 0;
    return draw_bitmap_surface(a,p->surface,desc.dwWidth,desc.dwHeight,image,source,destination,transparent,key,mirror);
}
int game_menu_surface_compose_copy(GameApplication *a,GameAddress destination,GameAddress source,
    const GameUiRect *rectangle,int32_t x,int32_t y,int transparent,int mirror) {
    MenuTextSurface *to=text_surface_find(a->menu_window,destination),*from=text_surface_find(a->menu_window,source);
    RECT src,dst;DDBLTFX fx={0};DWORD flags=DDBLT_WAIT;
    int64_t width,height;
    if(!rectangle)return 0;
    width=(int64_t)rectangle->right-rectangle->left;height=(int64_t)rectangle->bottom-rectangle->top;
    if(!to || !from || width<=0 || height<=0 || (int64_t)x+width>INT_MAX || (int64_t)y+height>INT_MAX)return 0;
    src.left=rectangle->left;src.top=rectangle->top;src.right=rectangle->right;src.bottom=rectangle->bottom;
    dst.left=x;dst.top=y;dst.right=(LONG)((int64_t)x+width);dst.bottom=(LONG)((int64_t)y+height);
    if(transparent)flags|=DDBLT_KEYSRC;
    if(mirror){flags|=DDBLT_DDFX;fx.dwSize=sizeof(fx);fx.dwDDFX=DDBLTFX_MIRRORLEFTRIGHT|DDBLTFX_MIRRORUPDOWN;}
    return SUCCEEDED(IDirectDrawSurface_Blt(to->surface,&dst,from->surface,&src,flags,mirror?&fx:NULL));
}
void game_menu_window_close(GameApplication *a,GameAddress identity) {
    MenuWindow *w=a->menu_window;HDC dc;RECT bounds;HGDIOBJ old;
    if(!w || w->identity!=identity)return;
    dc=GetDC(w->handle);
    if(dc) {old=SelectObject(dc,GetStockObject(BLACK_BRUSH));GetClientRect(w->handle,&bounds);
        PatBlt(dc,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,PATCOPY);
        SelectObject(dc,old);ReleaseDC(w->handle,dc);}
    /* Level display teardown may already have retired these surfaces. Closing
     * the window then has no presentation work left, only owner retirement. */
    if(w->front && w->back){
        (void)game_menu_screen_mask(a,0);(void)game_menu_present_frame(a,0);
        (void)game_menu_screen_mask(a,0);(void)game_menu_present_frame(a,0);
    }
    if(w->draw)IDirectDraw_FlipToGDISurface(w->draw);
}
void game_application_window_shutdown(GameApplication *a) {
    if(a->menu_window)game_menu_window_destroy(a,a->menu_window->identity);
}

/* 0040b020/0040b4a0/0040bd30 text-image backend. DDS/GDI handles are private
 * native pointers. The record identity selects its one owned offscreen surface. */
static MenuTextSurface *text_surface_find(MenuWindow *w,GameAddress identity) {
    MenuTextSurface *p=w?w->text_surfaces:0;while(p && p->identity!=identity)p=p->next;return p;
}
static int surface_span(const DDSURFACEDESC *desc,GameMenuPixelSpan *span){
    uint64_t bytes;
    if(!span || !desc->dwWidth || !desc->dwHeight || desc->dwWidth>8192 || desc->dwHeight>8192 ||
       desc->ddpfPixelFormat.dwRGBBitCount!=8 || desc->lPitch<(int32_t)desc->dwWidth)return 0;
    bytes=(uint64_t)(uint32_t)desc->lPitch*desc->dwHeight;if(bytes>SIZE_MAX)return 0;
    span->pixels=desc->lpSurface;span->bytes=(size_t)bytes;span->width=(int32_t)desc->dwWidth;
    span->height=(int32_t)desc->dwHeight;span->stride=desc->lPitch;return 1;
}
int game_menu_surface_dimensions(GameApplication *a,GameAddress identity,GameMenuPixelSpan *span){
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);DDSURFACEDESC desc={0};
    if(!p || p->locked_pixels)return 0;
    desc.dwSize=sizeof(desc);
    return SUCCEEDED(IDirectDrawSurface_GetSurfaceDesc(p->surface,&desc)) && surface_span(&desc,span);
}
int game_menu_surface_pixels_begin(GameApplication *a,GameAddress identity,GameMenuPixelSpan *span){
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);DDSURFACEDESC desc={0};
    if(!p || p->locked_pixels)return 0;
    desc.dwSize=sizeof(desc);
    if(FAILED(IDirectDrawSurface_Lock(p->surface,NULL,&desc,DDLOCK_WAIT,NULL)))return 0;
    if(!surface_span(&desc,span) || !span->pixels){IDirectDrawSurface_Unlock(p->surface,desc.lpSurface);return 0;}
    p->locked_pixels=desc.lpSurface;return 1;
}
/*Borrow the existing presented front buffer; no capture surface or duplicate
 *pixel owner is created. The native pointer stays inside this platform service. */
int game_menu_frame_pixels_begin(GameApplication *a,GameMenuPixelSpan *span){
    MenuWindow *w=a->menu_window;DDSURFACEDESC desc={0};
    if(!w || !w->front || w->frame_pixels)return 0;desc.dwSize=sizeof(desc);
    if(FAILED(IDirectDrawSurface_Lock(w->front,NULL,&desc,DDLOCK_WAIT,NULL)))return 0;
    if(!surface_span(&desc,span) || !span->pixels){IDirectDrawSurface_Unlock(w->front,desc.lpSurface);return 0;}
    w->frame_pixels=desc.lpSurface;return 1;
}
int game_menu_frame_pixels_end(GameApplication *a){MenuWindow *w=a->menu_window;void *pixels;
    if(!w || !w->front || !(pixels=w->frame_pixels))return 0;
    w->frame_pixels=NULL;return SUCCEEDED(IDirectDrawSurface_Unlock(w->front,pixels));
}
int game_menu_surface_pixels_end(GameApplication *a,GameAddress identity){
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);HRESULT result;void *pixels;
    if(!p || !(pixels=p->locked_pixels))return 0;
    result=IDirectDrawSurface_Unlock(p->surface,pixels);p->locked_pixels=NULL;return SUCCEEDED(result);
}
int game_menu_text_surface_release(GameApplication *a,GameAddress identity) {
    MenuWindow *w=a->menu_window;MenuTextSurface **link;
    if(!w)return 0;
    link=&w->text_surfaces;
    while(*link) {
        MenuTextSurface *p=*link;
        if(p->identity==identity){
            if(p->locked_pixels && !game_menu_surface_pixels_end(a,identity))return 0;
            *link=p->next;IDirectDrawSurface_Release(p->surface);free(p);return 1;
        }
        link=&p->next;
    }
    return 1; /* Empty/previously retired text surface. */
}
int game_menu_text_measure(GameApplication *a,unsigned font,const char *text,int32_t *width,int32_t *height) {
    MenuWindow *w=a->menu_window;HDC dc;HGDIOBJ old;SIZE extent;int result;
    size_t length=text?strlen(text):0u;
    if(!w || font>=7u || !w->fonts[font] || !width || !height || length>INT_MAX)return 0;
    dc=GetDC(w->handle);if(!dc)return 0;
    old=SelectObject(dc,w->fonts[font]);
    if(!old || old==HGDI_ERROR){ReleaseDC(w->handle,dc);return 0;}
    result=GetTextExtentPoint32A(dc,text,(int)length,&extent);
    SelectObject(dc,old);ReleaseDC(w->handle,dc);
    if(!result)return 0;
    *width=extent.cx;*height=extent.cy;return 1;
}
/*4ac0b0: same selected native font and DrawText rectangle/flags as painting.
 * This platform service returns a height; no guest GDI pointer is published. */
int game_menu_text_wrapped_height(GameApplication *a,unsigned font,uint32_t width,const char *text,uint32_t *height) {
    MenuWindow *w=a->menu_window;HDC dc;HGDIOBJ old;RECT bounds;int result;size_t length=text?strlen(text):0;
    if(!w || font>=7 || !w->fonts[font] || !width || width>INT_MAX || !text || !height || length>INT_MAX)return 0;
    dc=GetDC(w->handle);if(!dc)return 0;old=SelectObject(dc,w->fonts[font]);
    if(!old || old==HGDI_ERROR){ReleaseDC(w->handle,dc);return 0;}
    bounds.left=bounds.top=0;bounds.right=(LONG)width-1;bounds.bottom=10;SetBkMode(dc,TRANSPARENT);
    result=DrawTextA(dc,text,(int)length,&bounds,DT_CALCRECT|DT_EXPANDTABS|DT_WORDBREAK);
    SelectObject(dc,old);ReleaseDC(w->handle,dc);if(result<=0)return 0;*height=(uint32_t)result;return 1;
}
int game_menu_text_surface_fill(GameApplication *a,GameAddress identity,uint32_t fill) {
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);DDBLTFX fx={0};
    if(!p)return 0;
    fx.dwSize=sizeof(fx);fx.dwFillColor=fill;
    return SUCCEEDED(IDirectDrawSurface_Blt(p->surface,0,0,0,DDBLT_COLORFILL|DDBLT_WAIT,&fx));
}
/*40b0e0/4db630: source-key state belongs to the existing native surface.
 *A zero record key selects its first raw pixel, not a guessed palette entry. */
int game_menu_text_surface_key(GameApplication *a,GameAddress identity,uint32_t value){
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);DDCOLORKEY key;
    if(!p)return 0;
    if(!value){
        DDSURFACEDESC desc={0};unsigned bytes,i;int valid;
        desc.dwSize=sizeof(desc);
        if(FAILED(IDirectDrawSurface_Lock(p->surface,NULL,&desc,DDLOCK_WAIT|DDLOCK_READONLY,NULL)))return 0;
        bytes=desc.ddpfPixelFormat.dwRGBBitCount/8u;
        valid=desc.lpSurface && desc.dwWidth && desc.dwHeight && bytes>=1 && bytes<=4 &&
            desc.ddpfPixelFormat.dwRGBBitCount==bytes*8u;
        if(valid)for(i=0;i<bytes;++i)value|=(uint32_t)((const uint8_t *)desc.lpSurface)[i]<<(i*8u);
        if(FAILED(IDirectDrawSurface_Unlock(p->surface,desc.lpSurface)) || !valid)return 0;
    }
    key.dwColorSpaceLowValue=key.dwColorSpaceHighValue=value;
    return SUCCEEDED(IDirectDrawSurface_SetColorKey(p->surface,DDCKEY_SRCBLT,&key));
}
int game_menu_text_surface_restore(GameApplication *a,GameAddress identity) {
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);HRESULT status;
    if(!p)return -1;
    status=IDirectDrawSurface_IsLost(p->surface);
    if(status==DD_OK)return 0;
    if(status!=DDERR_SURFACELOST || FAILED(IDirectDrawSurface_Restore(p->surface)))return -1;
    return 1;
}
int game_menu_text_surface_white(GameApplication *a,GameAddress identity) {
    MenuTextSurface *p=text_surface_find(a->menu_window,identity);DDSURFACEDESC desc={0};
    HDC dc;HBRUSH brush;HGDIOBJ old;int result;HRESULT released;
    if(!p)return 0;
    desc.dwSize=sizeof(desc);if(FAILED(IDirectDrawSurface_GetSurfaceDesc(p->surface,&desc)))return 0;
    brush=CreateSolidBrush(PALETTERGB(255,255,255));if(!brush)return 0;
    if(FAILED(IDirectDrawSurface_GetDC(p->surface,&dc))){DeleteObject(brush);return 0;}
    old=SelectObject(dc,brush);
    result=old && old!=HGDI_ERROR && PatBlt(dc,0,0,desc.dwWidth,desc.dwHeight,PATCOPY);
    if(old && old!=HGDI_ERROR)SelectObject(dc,old);
    DeleteObject(brush);released=IDirectDrawSurface_ReleaseDC(p->surface,dc);
    return result && SUCCEEDED(released);
}
int game_menu_text_surface_create(GameApplication *a,GameAddress identity,unsigned width,unsigned height,uint32_t fill) {
    MenuWindow *w=a->menu_window;MenuTextSurface *p;DDSURFACEDESC desc={0};
    if(!w || !w->draw || !identity || !width || !height || width>8192u || height>8192u)return 0;
    p=calloc(1,sizeof(*p));if(!p)return 0;
    desc.dwSize=sizeof(desc);desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;
    desc.dwWidth=width;desc.dwHeight=height;desc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;
    /*Normal cooperative mode inherits the desktop format unless specified.
     *Text and image surfaces share the indexed menu palette, not desktop RGB. */
    if(w->native_draw){
        desc.dwFlags|=DDSD_PIXELFORMAT;
        desc.ddpfPixelFormat.dwSize=sizeof(desc.ddpfPixelFormat);
        desc.ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_PALETTEINDEXED8;
        desc.ddpfPixelFormat.dwRGBBitCount=8;
    }
    if(FAILED(IDirectDraw_CreateSurface(w->draw,&desc,&p->surface,0))){free(p);return 0;}
    if(w->palette && FAILED(IDirectDrawSurface_SetPalette(p->surface,w->palette))) {
        IDirectDrawSurface_Release(p->surface);free(p);return 0;
    }
    game_menu_text_surface_release(a,identity);p->identity=identity;p->next=w->text_surfaces;w->text_surfaces=p;
    if(!game_menu_text_surface_fill(a,identity,fill))return 0;
    /*40ba80 selects transparency before painting. Its zero record key uses
     *the initialized surface's first pixel through4db630, not literal UINT_MAX. */
    return fill || game_menu_text_surface_key(a,identity,0);
}
int game_menu_text_surface_paint(GameApplication *a,GameAddress identity,unsigned font,const char *text,
    uint32_t color,int32_t x,int32_t y,uint32_t flags,int alternate) {
    MenuWindow *w=a->menu_window;MenuTextSurface *p=text_surface_find(w,identity);
    DDSURFACEDESC desc={0};HDC dc;HGDIOBJ old;RECT rect;int result;HRESULT release;
    size_t length=text?strlen(text):0u;
    if(!p || font>=7u || length>INT_MAX)return 0;
    desc.dwSize=sizeof(desc);if(FAILED(IDirectDrawSurface_GetSurfaceDesc(p->surface,&desc)))return 0;
    if(FAILED(IDirectDrawSurface_GetDC(p->surface,&dc)))return 0;
    rect.left=x;rect.top=y;rect.right=(LONG)(alternate?desc.dwWidth/2u:desc.dwWidth);rect.bottom=(LONG)desc.dwHeight;
    if(alternate){font=0;color=0x02ffffffu;}
    old=SelectObject(dc,w->fonts[font]);
    if(!old || old==HGDI_ERROR){IDirectDrawSurface_ReleaseDC(p->surface,dc);return 0;}
    SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);
    if(alternate)SetTextCharacterExtra(dc,1);
    result=DrawTextA(dc,text,(int)length,&rect,flags|DT_WORDBREAK|DT_EXPANDTABS);
    SelectObject(dc,old);release=IDirectDrawSurface_ReleaseDC(p->surface,dc);
    return (result!=0 || length==0u) && SUCCEEDED(release);
}
int game_menu_text_surface_draw(GameApplication *a,GameAddress identity,const GameUiRect *source,
    const GameUiRect *destination,int transparent) {
    MenuWindow *w=a->menu_window;MenuTextSurface *p=text_surface_find(w,identity);RECT src,dest;DWORD flags=DDBLT_WAIT;
    if(!p)return 0;
    if(game_memory_read_u32(&a->memory,0x5840f0)){
        DDSURFACEDESC desc={0};DDCOLORKEY key={0};GameTextTarget target;int32_t fallback;int ok=0,ended;
        int32_t from[4]={source->left,source->top,source->right,source->bottom};
        int32_t to[4]={destination->left,destination->top,destination->right,destination->bottom};
        extern int game_application_glyph_begin(GameApplication *,GameTextTarget *,int32_t *);
        extern int game_application_glyph_end(GameApplication *);
        if(transparent && FAILED(IDirectDrawSurface_GetColorKey(p->surface,DDCKEY_SRCBLT,&key))){fprintf(stderr,"native text surface has no color key: %08x\n",(unsigned)identity);return 0;}
        desc.dwSize=sizeof(desc);
        {HRESULT locked=IDirectDrawSurface_Lock(p->surface,NULL,&desc,DDLOCK_WAIT|DDLOCK_READONLY,NULL);
         if(FAILED(locked)){fprintf(stderr,"native text surface lock failed: %08x result=%08lx\n",(unsigned)identity,(unsigned long)locked);return 0;}}
        if(desc.lpSurface && desc.lPitch>0 && desc.dwHeight && (uint64_t)(uint32_t)desc.lPitch*desc.dwHeight<=SIZE_MAX &&
           desc.ddpfPixelFormat.dwRGBBitCount==16 && desc.ddpfPixelFormat.dwRBitMask==0xf800 &&
           desc.ddpfPixelFormat.dwGBitMask==0x7e0 && desc.ddpfPixelFormat.dwBBitMask==31 &&
           game_application_glyph_begin(a,&target,&fallback)){
            ok=target.rgb565 && target.stride>0 && asset_rgb565_blit(desc.lpSurface,(size_t)desc.lPitch*desc.dwHeight,
                desc.dwWidth,desc.dwHeight,(size_t)desc.lPitch,from,to,target.pixels,target.pixel_bytes,
                game_memory_read_u32(&a->memory,0x627928),game_memory_read_u32(&a->memory,0x62792c),(size_t)target.stride*2,
                transparent,(uint16_t)key.dwColorSpaceLowValue,(uint16_t)key.dwColorSpaceHighValue);
            ended=game_application_glyph_end(a);if(!ended)ok=0;
        }
        if(FAILED(IDirectDrawSurface_Unlock(p->surface,desc.lpSurface)))ok=0;
        if(!ok)fprintf(stderr,"native text surface draw failed: %08x size=%lux%lu pitch=%ld bpp=%lu masks=%08lx/%08lx/%08lx source=%d,%d,%d,%d destination=%d,%d,%d,%d\n",
            (unsigned)identity,(unsigned long)desc.dwWidth,(unsigned long)desc.dwHeight,(long)desc.lPitch,(unsigned long)desc.ddpfPixelFormat.dwRGBBitCount,
            (unsigned long)desc.ddpfPixelFormat.dwRBitMask,(unsigned long)desc.ddpfPixelFormat.dwGBitMask,(unsigned long)desc.ddpfPixelFormat.dwBBitMask,
            from[0],from[1],from[2],from[3],to[0],to[1],to[2],to[3]);
        return ok && !a->memory.fault;
    }
    if(!w->back)return 0;
    src.left=source->left;src.top=source->top;src.right=source->right;src.bottom=source->bottom;
    dest.left=destination->left;dest.top=destination->top;dest.right=destination->right;dest.bottom=destination->bottom;
    if(transparent)flags|=DDBLT_KEYSRC;
    return SUCCEEDED(IDirectDrawSurface_Blt(w->back,&dest,p->surface,&src,flags,0));
}
/*40be40/532810: the existing glyph algorithm writes directly to the locked
 * native back surface. Optional white text background is blitted first. */
int game_menu_accelerated_text_begin(GameApplication *a,GameAddress identity,const GameUiRect *destination,
    const GameUiRect *clip,GameTextTarget *target,int32_t *fallback) {
    MenuWindow *w=a->menu_window;DDSURFACEDESC desc;GameUiRect visible,source;
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
        if(area.right>area.left && area.bottom>area.top){source.left=area.left-destination->left;source.top=area.top-destination->top;
            source.right=area.right-destination->left;source.bottom=area.bottom-destination->top;
            if(!game_menu_text_surface_draw(a,identity,&source,&area,0))return 0;}}
    if(gameplay){if(!game_application_glyph_begin(a,target,fallback))return 0;}
    else{
        if(!lock_back(w,&desc))return 0;
        memset(target,0,sizeof(*target));target->stride=desc.lPitch;target->pixels=desc.lpSurface;
        if((uint64_t)(uint32_t)desc.lPitch*w->height>SIZE_MAX){IDirectDrawSurface_Unlock(w->back,desc.lpSurface);return 0;}
        target->pixel_bytes=(size_t)desc.lPitch*w->height;w->text_lock_pixels=desc.lpSurface;
        *fallback=108; /*Original software descriptor's fixed serialized size. */
    }
    game_memory_write_u32(&a->memory,0x59d450,(uint32_t)visible.left);
    game_memory_write_u32(&a->memory,0x59d454,(uint32_t)visible.right);
    game_memory_write_u32(&a->memory,0x59d458,(uint32_t)visible.top);
    game_memory_write_u32(&a->memory,0x59d45c,(uint32_t)visible.bottom);
    if(a->memory.fault){
        if(gameplay)(void)game_application_glyph_end(a);
        else{IDirectDrawSurface_Unlock(w->back,w->text_lock_pixels);w->text_lock_pixels=NULL;}
        return 0;
    }return 1;
}
int game_menu_accelerated_text_end(GameApplication *a) {
    MenuWindow *w=a->menu_window;void *pixels;HRESULT result;
    if(game_memory_read_u32(&a->memory,0x5840f0)){
        extern int game_application_glyph_end(GameApplication *);
        return game_application_glyph_end(a);
    }
    if(!w || !w->back || !(pixels=w->text_lock_pixels))return 0;
    result=IDirectDrawSurface_Unlock(w->back,pixels);w->text_lock_pixels=NULL;return SUCCEEDED(result);
}
/*Axis-aligned indexed borders share one native surface lock. Coordinates are
 * inclusive endpoints, as in the original four 515e20 calls per rectangle. */
int game_menu_frame_rectangles(GameApplication *a,const GameUiRect *rectangles,const uint8_t *colors,size_t count) {
    MenuWindow *w=a->menu_window;DDSURFACEDESC desc;int ok;
    if(!w || !rectangles || !colors || count>4096 || w->text_lock_pixels || !lock_back(w,&desc))return 0;
    if((uint64_t)(uint32_t)desc.lPitch*w->height>SIZE_MAX){IDirectDrawSurface_Unlock(w->back,desc.lpSurface);return 0;}
    ok=game_ui_indexed_borders(desc.lpSurface,(size_t)desc.lPitch*w->height,(int32_t)w->width,(int32_t)w->height,desc.lPitch,rectangles,colors,count);
    return SUCCEEDED(IDirectDrawSurface_Unlock(w->back,desc.lpSurface)) && ok;
}
