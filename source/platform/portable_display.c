#include "portable_display.h"
#include "application_config.h"
#include "menu_widgets.h"
#include "portable_input_devices.h"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portable_fsr.inc"
#ifdef UPRISING_FSR3_UPSCALER
#include "vulkan_menu.h"
#include "vulkan_device.h"
#endif

typedef struct DisplayOptions {int width,height,fullscreen,fsr,aa,sharpness;} DisplayOptions;
typedef struct DisplayMode {int width,height;} DisplayMode;
struct GameDisplay {
    GameApplication *application;SDL_Window *window;GameMenuFonts *fonts;
    DisplayOptions active,draft,previous;
    DisplayMode modes[96];int mode_count;
    int drawer_x,drawer_y,drawer_visible,drawer_enabled;
    int open,focus,armed,mouse_x,mouse_y,countdown_pixels,aa_limit;Uint64 deadline;
    char status[160];SDL_Surface *labels,*canvas;SDL_Palette *label_palette;
    DisplayFsr fsr;int vulkan,fsr_available;
};
typedef struct GameDisplay GameDisplay;
static void display_redraw(GameDisplay *d){game_memory_write_u32(&d->application->memory,0x58fd3cu,1);}
int game_display_antialias_samples(GameApplication *a){return a->display?a->display->active.aa:0;}
int game_display_temporal_native(GameApplication *a){return a->display && a->display->active.fsr==2;}
float game_display_sharpness(GameApplication *a){return a->display?a->display->active.sharpness/100.0f:.2f;}
int game_display_temporal_output(GameApplication *a,uint32_t size[2]){
    GameDisplay *d=a->display;int width,height,viewport[4];size[0]=size[1]=0;
    if(!d || !d->vulkan || ((!d->fsr_available || !d->active.fsr) && !d->active.aa))return 0;
    if(!SDL_GetWindowSizeInPixels(d->window,&width,&height) || width<=0 || height<=0)return d->active.fsr!=0;
    game_display_viewport(width,height,640,480,viewport);
    size[0]=(uint32_t)(viewport[2]<640?640:viewport[2]);size[1]=(uint32_t)(viewport[3]<480?480:viewport[3]);
    return d->active.fsr!=0;
}
void game_display_antialias_unavailable(GameApplication *a){
    GameDisplay *d=a->display;if(!d)return;
    d->active.aa=d->draft.aa=d->aa_limit=0;
    snprintf(d->status,sizeof(d->status),"MSAA unavailable; disabled for this session.");display_redraw(d);
}

void game_display_viewport(int width,int height,int sw,int sh,int r[4]){
    r[0]=r[1]=0;r[2]=width;r[3]=height;
    if(width<=0 || height<=0 || sw<=0 || sh<=0)return;
    if((int64_t)width*sh>(int64_t)height*sw)r[2]=(int)((int64_t)height*sw/sh);
    else r[3]=(int)((int64_t)width*sh/sw);
    r[0]=(width-r[2])/2;r[1]=(height-r[3])/2;
}
static void display_modes(GameDisplay *d){
    static const int common[][2]={{640,480},{800,600},{960,720},{1024,768},{1152,864},
        {1280,960},{1400,1050},{1600,1200},{1920,1440},{2048,1536},{2560,1920},{2880,2160}};
    SDL_DisplayID id=SDL_GetDisplayForWindow(d->window);SDL_Rect usable;int i;
    d->mode_count=0;
    if(!SDL_GetDisplayUsableBounds(id,&usable))usable=(SDL_Rect){0,0,1920,1080};
    for(i=0;i<(int)(sizeof(common)/sizeof(*common));++i)
        if(common[i][0]<=usable.w && common[i][1]<=usable.h)
            d->modes[d->mode_count++]=(DisplayMode){common[i][0],common[i][1]};
    /* Always offer a recovery window, even on a very small desktop. */
    if(!d->mode_count)d->modes[d->mode_count++]=(DisplayMode){640,480};
}
static int display_mode_index(GameDisplay *d){
    int i;for(i=0;i<d->mode_count;++i)
        if(d->modes[i].width==d->draft.width && d->modes[i].height==d->draft.height)return i;
    return -1;
}
static int display_set(GameDisplay *d,DisplayOptions options){
    if(options.width<320 || options.height<200 || options.width>8192 || options.height>8192)return 0;
    if(options.fullscreen && game_application_isolated_input(d->application)){
        SDL_SetError("Fullscreen is disabled during isolated automation");return 0;
    }
    if(options.width*3!=options.height*4){SDL_SetError("Only 4:3 output sizes are supported");return 0;}
    /* Desktop fullscreen keeps the display mode intact. The presenter clears
     * the whole canvas black and draws only the fitted 4:3 viewport. Width/
     * height remain the preferred window size for the return to windowed mode. */
    if(!SDL_SetWindowFullscreen(d->window,false) || !SDL_SetWindowFullscreenMode(d->window,NULL) ||
       !SDL_SetWindowSize(d->window,options.width,options.height) ||
       (options.fullscreen && !SDL_SetWindowFullscreen(d->window,true)))return 0;
    (void)SDL_SyncWindow(d->window);d->active=options;return 1;
}
static void display_restore(GameDisplay *d){
    if(!display_set(d,d->previous)){
        DisplayOptions recovery={640,480,0,0,0};(void)display_set(d,recovery);
    }
    d->deadline=0;d->draft=d->active;display_modes(d);
    display_redraw(d);
}
static void display_close(GameDisplay *d){
    if(d->deadline)display_restore(d);
    d->open=0;d->armed=-1;
    display_redraw(d);
    (void)game_menu_input_acquire(d->application,0);(void)game_menu_input_acquire(d->application,1);
}
void game_display_tick(GameApplication *a){
    GameDisplay *d=a->display;
    if(d && d->deadline){
        Uint64 now=SDL_GetTicks();
        if(now>=d->deadline)display_restore(d);
        else {
            int pixels=(int)(300*(d->deadline-now)/20000);
            if(pixels!=d->countdown_pixels){d->countdown_pixels=pixels;display_redraw(d);}
        }
    }
}
static int display_configure(GameDisplay *d,DisplayOptions options){
    GameConfig *c=game_application_config(d->application);
    return c && game_config_set_number(c,"display","width",options.width) &&
        game_config_set_number(c,"display","height",options.height) &&
        game_config_set_number(c,"display","fullscreen",options.fullscreen) &&
        game_config_set_number(c,"display",d->vulkan?"fsr3":"fsr1",options.fsr) &&
        game_config_set_number(c,"display",d->vulkan?"vulkan_aa":"msaa_samples",options.aa) &&
        (!d->vulkan || game_config_set_number(c,"display","fsr3_sharpness",options.sharpness));
}
static int display_persist(GameDisplay *d){
    if(display_configure(d,d->active) && game_application_config_flush(d->application))return 1;
    /* A later unrelated settings flush must not publish a rejected mode. */
    (void)display_configure(d,d->previous);return 0;
}
int game_display_begin(GameApplication *a,SDL_Window *window,GameMenuFonts *fonts){
    GameConfig *c=game_application_config(a);GameDisplay *d=calloc(1,sizeof(*d));
    SDL_Color colors[4]={{0,0,0,0},{148,255,72,255},{148,157,129,255},{68,103,60,255}};
    if(!d)return 0;
    a->display=d;d->application=a;d->window=window;d->fonts=fonts;d->armed=-1;
    d->active=(DisplayOptions){640,480,0,0,0,20};
    d->vulkan=(SDL_GetWindowFlags(window)&SDL_WINDOW_VULKAN)!=0;
    if(!d->vulkan){
    display_fsr_begin(&d->fsr,a);
    d->fsr_available=d->fsr.available;
    if((SDL_GL_ExtensionSupported("GL_ARB_framebuffer_object") ||
        (SDL_GL_ExtensionSupported("GL_EXT_framebuffer_multisample") && SDL_GL_ExtensionSupported("GL_EXT_framebuffer_blit"))) &&
       (SDL_GL_GetProcAddress("glRenderbufferStorageMultisample") || SDL_GL_GetProcAddress("glRenderbufferStorageMultisampleEXT")) &&
       (SDL_GL_GetProcAddress("glBlitFramebuffer") || SDL_GL_GetProcAddress("glBlitFramebufferEXT")))
        glGetIntegerv(GL_MAX_SAMPLES,&d->aa_limit);
    if(d->aa_limit>8)d->aa_limit=8;
    }
#ifdef UPRISING_FSR3_UPSCALER
    else {VulkanMenu *menu=game_menu_vulkan(a);d->fsr_available=menu && vulkan_menu_device(menu)->fsr3_storage_supported;}
#endif
    if(d->vulkan)d->aa_limit=1; /* World-only spatial edge AA, independent of the SDK. */
    d->canvas=SDL_CreateSurface(640,480,SDL_PIXELFORMAT_RGBA32);
    d->labels=SDL_CreateSurface(640,480,SDL_PIXELFORMAT_INDEX8);d->label_palette=SDL_CreatePalette(4);
    if(!d->canvas || !d->labels || !d->label_palette || !SDL_SetPaletteColors(d->label_palette,colors,0,4) ||
       !SDL_SetSurfacePalette(d->labels,d->label_palette) || !SDL_SetSurfaceColorKey(d->labels,true,0))return 0;
    if(!game_application_isolated_input(a) && !SDL_getenv("UPRISING_RECOMP_HEADLESS")){
        DisplayOptions saved={(int)game_config_number(c,"display","width",640),
            (int)game_config_number(c,"display","height",480),
            game_config_number(c,"display","fullscreen",0)==1,
            d->fsr_available?(int)game_config_number(c,"display",d->vulkan?"fsr3":"fsr1",0):0,
            (int)game_config_number(c,"display",d->vulkan?"vulkan_aa":"msaa_samples",0),
            (int)game_config_number(c,"display","fsr3_sharpness",20)};
        if(saved.fsr<0 || saved.fsr>(d->vulkan?2:1))saved.fsr=0;
        if(d->vulkan){if(saved.aa!=1)saved.aa=0;}
        else if((saved.aa!=2 && saved.aa!=4 && saved.aa!=8) || saved.aa>d->aa_limit)saved.aa=0;
        if(saved.sharpness<0 || saved.sharpness>100)saved.sharpness=20;
        if(!display_set(d,saved)){(void)display_set(d,d->active);SDL_ClearError();}
    }
    if(d->vulkan){const char *force=SDL_getenv("UPRISING_VULKAN_FSR3");
        if(force && !strcmp(force,"1") && !d->fsr_available){SDL_SetError("FSR3 host unavailable on this architecture/device");return 0;}
        if(force && (!strcmp(force,"1") || !strcmp(force,"0")))d->active.fsr=!strcmp(force,"1");
    }
    d->draft=d->active;display_modes(d);return 1;
}
void game_display_end(GameApplication *a){
    GameDisplay *d=a->display;if(!d)return;a->display=NULL;
    /* Restore the desktop before destroying the window/context. */
    (void)SDL_SetWindowFullscreen(d->window,false);
    display_fsr_end(&d->fsr);SDL_DestroySurface(d->labels);SDL_DestroySurface(d->canvas);SDL_DestroyPalette(d->label_palette);
    free(d);
}
void game_display_graphics_drawer(GameApplication *a,int x,int y,int visible,int enabled){
    GameDisplay *d=a->display;if(!d)return;
    d->drawer_x=x+255;d->drawer_y=y+3;d->drawer_visible=visible;if(enabled>=0)d->drawer_enabled=enabled;
    if(!visible && d->open)display_close(d);
}
static int inside(int x,int y,int left,int top,int width,int height){return x>=left && y>=top && x<left+width && y<top+height;}
static int display_hit(GameDisplay *d,int x,int y){
    if(!d->open)return d->drawer_visible && d->drawer_enabled && inside(x,y,d->drawer_x,d->drawer_y,112,26)?7:-1;
    if(d->deadline){if(inside(x,y,170,357,140,30))return 5;if(inside(x,y,330,357,140,30))return 6;return -1;}
    if(d->vulkan && inside(x,y,322,316,180,30))return x<348?14:13;
    {int row;for(row=0;row<4;++row)if(inside(x,y,322,176+row*(d->vulkan?35:38),180,30))
        return row==3?(x<348?12:11):(x<348?8+row:row);}
    if(inside(x,y,170,357,140,30))return 3;
    if(inside(x,y,330,357,140,30))return 4;
    return -1;
}
static void display_action(GameDisplay *d,int action,int direction){
    int index;
    display_redraw(d);
    if(action>=8 && action<=10){action-=8;direction=-1;}
    if(action==12){action=11;direction=-1;}
    if(action==14){action=13;direction=-1;}
    if(action>=0 && action<=2)d->focus=action;
    switch(action){
    case 7:
        d->open=1;d->draft=d->active;d->status[0]=0;d->focus=0;display_modes(d);
        (void)game_menu_slider_cancel(d->application);(void)game_menu_input_unacquire(d->application);
        (void)SDL_SetWindowRelativeMouseMode(d->window,false);(void)SDL_SetWindowMouseGrab(d->window,false);break;
    case 0:
        d->draft.fullscreen=!d->draft.fullscreen;display_modes(d);
        if(display_mode_index(d)<0){d->draft.width=d->modes[0].width;d->draft.height=d->modes[0].height;}break;
    case 1:
        if(d->draft.fullscreen)break;
        index=display_mode_index(d);index=(index+direction+d->mode_count)%d->mode_count;
        d->draft.width=d->modes[index].width;d->draft.height=d->modes[index].height;break;
    case 2:if(d->fsr_available)d->draft.fsr=(d->draft.fsr+direction+(d->vulkan?3:2))%(d->vulkan?3:2);break;
    case 11:{
        if(d->vulkan){if(!d->draft.fsr)d->draft.aa=!d->draft.aa;d->focus=11;break;}
        static const int samples[]={0,2,4,8};int count=1;
        while(count<4 && samples[count]<=d->aa_limit)++count;
        for(index=0;index<count;++index)if(samples[index]==d->draft.aa)break;
        index=(index+direction+count)%count;d->draft.aa=samples[index];d->focus=11;break;
    }
    case 13:if(d->vulkan && d->draft.fsr){d->draft.sharpness=((d->draft.sharpness/20+direction+6)%6)*20;d->focus=13;}break;
    case 3:
        d->previous=d->active;
        if(display_set(d,d->draft)){d->deadline=SDL_GetTicks()+20000;d->focus=5;d->status[0]=0;}
        else {snprintf(d->status,sizeof(d->status),"Cannot apply display mode: %.110s",SDL_GetError());display_restore(d);}break;
    case 4:display_close(d);break;
    case 5:
        if(display_persist(d)){d->deadline=0;display_close(d);}
        else {display_restore(d);snprintf(d->status,sizeof(d->status),"Could not save display settings.");}break;
    case 6:display_restore(d);break;
    }
}
int game_display_event(GameApplication *a,const SDL_Event *event){
    GameDisplay *d=a->display;int x,y,hit;int32_t p[2];
    if(!d)return 0;
    if(d->deadline && SDL_GetTicks()>=d->deadline)display_restore(d);
    if(d->open && (event->type==SDL_EVENT_WINDOW_FOCUS_LOST || event->type==SDL_EVENT_WINDOW_MINIMIZED)){
        /* Mode changes can themselves produce focus events. The independent
         * timeout still rolls back while minimized or without input. */
        d->armed=-1;return 0;
    }
    if(event->type==SDL_EVENT_MOUSE_MOTION || event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP){
        p[0]=(int32_t)(event->type==SDL_EVENT_MOUSE_MOTION?event->motion.x:event->button.x);
        p[1]=(int32_t)(event->type==SDL_EVENT_MOUSE_MOTION?event->motion.y:event->button.y);
        if(!game_menu_pointer_from_client(a,p))return d->open;
        x=p[0];y=p[1];d->mouse_x=x;d->mouse_y=y;hit=display_hit(d,x,y);
        if(d->drawer_visible)display_redraw(d);
        if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button==SDL_BUTTON_LEFT && (d->open || hit==7)){
            d->armed=hit;return 1;
        }
        if(event->type==SDL_EVENT_MOUSE_BUTTON_UP && event->button.button==SDL_BUTTON_LEFT && d->armed>=0){
            int armed=d->armed;d->armed=-1;if(armed==hit)display_action(d,hit,1);return 1;
        }
        return d->open;
    }
    if(d->open && event->type==SDL_EVENT_KEY_DOWN){
        SDL_Scancode key=event->key.scancode;if(event->key.repeat)return 1;
        display_redraw(d);
        if(key==SDL_SCANCODE_ESCAPE)display_action(d,d->deadline?6:4,1);
        else if(key==SDL_SCANCODE_TAB || key==SDL_SCANCODE_DOWN || key==SDL_SCANCODE_UP){
            int controls[]={0,1,2,11,d->vulkan?13:3,d->vulkan?3:4,4};int i,count=d->vulkan?7:6;
            if(d->deadline)d->focus=d->focus==5?6:5;
            else {
                for(i=0;i<count;++i)if(controls[i]==d->focus)break;
                d->focus=controls[(i+count+(key==SDL_SCANCODE_UP?-1:1))%count];
            }
        }else if(key==SDL_SCANCODE_RETURN || key==SDL_SCANCODE_KP_ENTER || key==SDL_SCANCODE_SPACE ||
                 key==SDL_SCANCODE_LEFT || key==SDL_SCANCODE_RIGHT)
            display_action(d,d->focus,key==SDL_SCANCODE_LEFT?-1:1);
        return 1;
    }
    return d->open && (event->type==SDL_EVENT_KEY_UP || event->type==SDL_EVENT_TEXT_INPUT || event->type==SDL_EVENT_MOUSE_WHEEL);
}
static Uint32 display_color(SDL_Surface *s,int r,int g,int b){return SDL_MapSurfaceRGB(s,(Uint8)r,(Uint8)g,(Uint8)b);}
static void display_fill(SDL_Surface *s,int x,int y,int w,int h,Uint32 color){SDL_Rect r={x,y,w,h};(void)SDL_FillSurfaceRect(s,&r,color);}
static void display_label(GameDisplay *d,const char *text,int x,int y,int right,unsigned font,uint8_t color){
    (void)game_menu_font_paint(d->fonts,font,text,d->labels->pixels,(size_t)d->labels->pitch*d->labels->h,
        640,480,d->labels->pitch,x,y,right,color,0,0);
}
static void display_center(GameDisplay *d,const char *text,int x,int y,int w,int h,unsigned font,uint8_t color){
    int32_t tw,th;
    if(game_menu_font_measure(d->fonts,font,text,&tw,&th))
        display_label(d,text,x+(w-tw)/2,y+(h-th)/2,x+w,font,color);
}
/* Quiet bronze trim, drawn at native pixel size rather than stretching a
 * labelled bitmap. A fixed hash supplies fine grain without a tiled pattern. */
static void display_metal(SDL_Surface *s,int x,int y,int w,int h){
    int i,j;
    for(j=0;j<h;++j){
        Uint32 *row=(Uint32 *)((Uint8 *)s->pixels+(y+j)*s->pitch);
        for(i=0;i<w;++i){
            uint32_t noise=(uint32_t)(x+i)*374761393u+(uint32_t)(y+j)*668265263u;
            int shade;noise=(noise^(noise>>13))*1274126177u;
            shade=55+(h-j)*15/h+(int)((noise>>24)%5u);
            row[x+i]=display_color(s,shade+8,shade+3,shade-11);
        }
    }
    display_fill(s,x,y,w,1,display_color(s,124,119,95));
    display_fill(s,x,y,1,h,display_color(s,103,99,78));
    display_fill(s,x,y+h-1,w,1,display_color(s,16,16,12));
    display_fill(s,x+w-1,y,1,h,display_color(s,16,16,12));
}
static void display_button(GameDisplay *d,SDL_Surface *s,int x,int y,int w,int h,const char *text,int focused,int pressed){
    display_metal(s,x,y,w,h);
    display_fill(s,x+3,y+3,w-6,h-6,display_color(s,pressed?12:27,pressed?19:30,pressed?13:23));
    display_fill(s,x+4,y+4,w-8,1,display_color(s,12,15,10));
    display_fill(s,x+4,y+h-5,w-8,1,display_color(s,focused?77:48,focused?111:54,focused?48:36));
    if(focused)display_fill(s,x+7,y+h-3,w-14,1,display_color(s,99,164,57));
    display_center(d,text,x,y+pressed,w,h,2,focused?1:2);
}
static void display_selector(GameDisplay *d,SDL_Surface *s,int row,const char *text,int enabled){
    int x=322,y=176+(d->vulkan?35:38)*row,i,hover=display_hit(d,d->mouse_x,d->mouse_y);
    int action=row==4?13:row==3?11:row,previous=row==4?14:row==3?12:row+8;
    int focused=enabled && (d->focus==action || hover==action || hover==previous);
    Uint32 arrow=display_color(s,focused?148:83,focused?255:116,focused?72:63);
    display_fill(s,x,y,180,30,display_color(s,6,9,6));
    display_fill(s,x+1,y+1,178,1,display_color(s,2,4,2));
    display_fill(s,x+1,y+29,178,1,display_color(s,focused?61:39,focused?89:49,focused?43:31));
    display_metal(s,x+2,y+2,22,26);display_metal(s,x+156,y+2,22,26);
    display_fill(s,x+5,y+5,16,20,display_color(s,23,27,19));
    display_fill(s,x+159,y+5,16,20,display_color(s,23,27,19));
    if(enabled)for(i=0;i<5;++i){
        display_fill(s,x+10+i,y+14-i,1,1+i*2,arrow);
        display_fill(s,x+169-i,y+14-i,1,1+i*2,arrow);
    }
    display_center(d,text,x+25,y,130,30,2,focused?1:enabled?2:3);
}
static void display_rivet(SDL_Surface *s,int x,int y){
    display_fill(s,x-2,y-2,5,5,display_color(s,12,14,9));
    display_fill(s,x-1,y-1,3,3,display_color(s,106,102,79));
    display_fill(s,x-1,y,3,1,display_color(s,32,34,24));
}
int game_display_overlay(GameApplication *a,SDL_Surface *rgba){
    GameDisplay *d=a->display;SDL_Surface *s;SDL_Rect target;char text[160];int x,hover;
    if(!d || !rgba || !d->drawer_visible)return 1;
    if(d->deadline && SDL_GetTicks()>=d->deadline)display_restore(d);
    s=d->canvas;if(!s)return 0;
    (void)SDL_SetSurfaceBlendMode(s,SDL_BLENDMODE_BLEND);(void)SDL_FillSurfaceRect(s,NULL,0);
    (void)SDL_FillSurfaceRect(d->labels,NULL,0);
    if(!d->open && d->drawer_enabled && d->drawer_x>=0 && d->drawer_x+112<=640 && d->drawer_y>=0 && d->drawer_y+26<=480)
        display_button(d,s,d->drawer_x,d->drawer_y,112,26,"ADVANCED",inside(d->mouse_x,d->mouse_y,d->drawer_x,d->drawer_y,112,26),d->armed==7);
    if(d->open){
        display_fill(s,0,0,640,480,SDL_MapSurfaceRGBA(s,0,0,0,176));
        display_fill(s,119,113,410,296,SDL_MapSurfaceRGBA(s,0,0,0,150));
        display_metal(s,115,108,410,296);
        display_fill(s,123,116,394,280,display_color(s,9,14,11));
        display_fill(s,126,119,388,274,display_color(s,15,22,17));
        display_fill(s,128,121,384,49,display_color(s,30,34,24));
        display_fill(s,128,170,384,1,display_color(s,66,83,47));
        display_label(d,"ADVANCED GRAPHICS",140,130,485,3,2);
        display_label(d,d->vulkan?"AA affects the world; menus and HUD stay separate.":
            "3D AA: next mission load. FSR scales all screens.",140,155,505,0,2);
        display_fill(s,493,131,7,7,display_color(s,27,49,16));
        display_fill(s,495,133,3,3,display_color(s,148,255,72));
        display_rivet(s,119,112);display_rivet(s,520,112);
        display_rivet(s,119,399);display_rivet(s,520,399);
        hover=display_hit(d,d->mouse_x,d->mouse_y);
        if(d->deadline){
            Uint64 now=SDL_GetTicks(),remaining=now<d->deadline?d->deadline-now:0;
            if(remaining>20000)remaining=20000;
            display_center(d,"KEEP THESE DISPLAY SETTINGS?",128,187,384,24,3,1);
            snprintf(text,sizeof(text),"Reverting in %u seconds",(unsigned)((remaining+999)/1000));
            display_center(d,text,128,233,384,20,2,2);
            display_fill(s,167,269,306,9,display_color(s,4,8,5));
            display_fill(s,170,272,(int)(300*remaining/20000),3,display_color(s,104,192,52));
            display_button(d,s,170,357,140,30,"KEEP",d->focus==5 || hover==5,d->armed==5);
            display_button(d,s,330,357,140,30,"REVERT",d->focus==6 || hover==6,d->armed==6);
        }else{
            display_label(d,"DISPLAY MODE",138,185,315,2,2);
            display_label(d,d->draft.fullscreen?"OUTPUT SIZE":"RESOLUTION",138,d->vulkan?220:223,315,2,2);
            display_label(d,d->vulkan?"FSR 3.1 (3D)":"FSR 1 UPSCALING",138,d->vulkan?255:261,315,2,2);
            display_label(d,"3D ANTI-ALIASING",138,d->vulkan?290:299,315,2,2);
            if(d->vulkan)display_label(d,"FSR SHARPNESS",138,325,315,2,2);
            display_fill(s,138,210,364,1,display_color(s,29,42,29));
            display_fill(s,138,d->vulkan?245:248,364,1,display_color(s,29,42,29));
            display_fill(s,138,d->vulkan?280:286,364,1,display_color(s,29,42,29));
            if(d->vulkan)display_fill(s,138,315,364,1,display_color(s,29,42,29));
            display_selector(d,s,0,d->draft.fullscreen?"FULLSCREEN":"WINDOWED",1);
            snprintf(text,sizeof(text),"%d x %d",d->draft.width,d->draft.height);
            if(d->draft.fullscreen){
                SDL_Rect bounds;int viewport[4];
                if(SDL_GetDisplayBounds(SDL_GetDisplayForWindow(d->window),&bounds)){
                    game_display_viewport(bounds.w,bounds.h,640,480,viewport);
                    snprintf(text,sizeof(text),"%d x %d",viewport[2],viewport[3]);
                }else snprintf(text,sizeof(text),"DESKTOP 4:3");
            }
            display_selector(d,s,1,text,!d->draft.fullscreen);
            display_selector(d,s,2,!d->fsr_available?(d->vulkan?(sizeof(void*)==4?"64-BIT ONLY":"GPU UNSUPPORTED"):"UNAVAILABLE"):d->draft.fsr==2?"NATIVE AA":d->draft.fsr?(d->vulkan?"UPSCALE":"ON"):"OFF",d->fsr_available);
            if(d->draft.aa)snprintf(text,sizeof(text),"MSAA %dx",d->draft.aa);else snprintf(text,sizeof(text),"OFF");
            if(d->vulkan){
                display_selector(d,s,3,d->draft.fsr?"TEMPORAL":d->draft.aa?"EDGE AA":"OFF",!d->draft.fsr);
                snprintf(text,sizeof(text),"%d%%",d->draft.sharpness);
                display_selector(d,s,4,text,d->fsr_available && d->draft.fsr);
            }else display_selector(d,s,3,d->aa_limit>=2?text:"UNAVAILABLE",d->aa_limit>=2);
            display_button(d,s,170,357,140,30,"APPLY",d->focus==3 || hover==3,d->armed==3);
            display_button(d,s,330,357,140,30,"CANCEL",d->focus==4 || hover==4,d->armed==4);
            if(d->status[0])display_label(d,d->status,138,d->vulkan?348:334,505,0,2);
        }
    }
    (void)SDL_BlitSurface(d->labels,NULL,s,NULL);target=(SDL_Rect){0,0,rgba->w,rgba->h};
    x=SDL_BlitSurfaceScaled(s,NULL,rgba,&target,SDL_SCALEMODE_NEAREST);return x;
}
int game_display_present(GameApplication *a,unsigned texture,int sw,int sh,int top_down){
    GameDisplay *d=a->display;int width,height,r[4],mode,ok=1;
    if(!d || !SDL_GetWindowSizeInPixels(d->window,&width,&height))return 0;
    if(width<=0 || height<=0)return 1;
    game_display_viewport(width,height,sw,sh,r);
    glGetIntegerv(GL_MATRIX_MODE,&mode);glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);
    glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_FOG);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glColor4f(1,1,1,1);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,1,1,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();glViewport(r[0],r[1],r[2],r[3]);
    if(d->active.fsr && (r[2]>sw || r[3]>sh)){
        ok=display_fsr_draw(&d->fsr,texture,sw,sh,r,top_down);
        if(!ok){d->active.fsr=0;d->draft.fsr=0;snprintf(d->status,sizeof(d->status),"FSR failed; original scaling restored.");
            game_application_log(a,"%s\n",d->status);}
    }else ok=0;
    if(!ok){glViewport(r[0],r[1],r[2],r[3]);display_pixel_art_draw(&d->fsr,texture,sw,sh,r,top_down);}
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(mode);glPopAttrib();
    return glGetError()==GL_NO_ERROR;
}
