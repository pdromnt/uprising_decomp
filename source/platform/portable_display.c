#include "portable_display.h"
#include "application_config.h"
#include "asset_bitmap.h"
#include "menu_widgets.h"
#include "portable_input_devices.h"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portable_fsr.inc"

typedef struct DisplayOptions {int width,height,fullscreen,fsr;} DisplayOptions;
typedef struct DisplayMode {int width,height;const SDL_DisplayMode *mode;} DisplayMode;
struct GameDisplay {
    GameApplication *application;SDL_Window *window;GameMenuFonts *fonts;
    DisplayOptions active,draft,previous;
    DisplayMode modes[96];int mode_count;
    SDL_DisplayMode **fullscreen_modes;
    int drawer_x,drawer_y,drawer_visible,drawer_enabled;
    int open,focus,armed,mouse_x,mouse_y;Uint64 deadline;
    char status[160];SDL_Surface *labels,*canvas;SDL_Palette *label_palette;
    ResourceBlob button_blob;AssetBitmap button;
    DisplayFsr fsr;
};
typedef struct GameDisplay GameDisplay;

void game_display_viewport(int width,int height,int sw,int sh,int r[4]){
    r[0]=r[1]=0;r[2]=width;r[3]=height;
    if(width<=0 || height<=0 || sw<=0 || sh<=0)return;
    if((int64_t)width*sh>(int64_t)height*sw)r[2]=(int)((int64_t)height*sw/sh);
    else r[3]=(int)((int64_t)width*sh/sw);
    r[0]=(width-r[2])/2;r[1]=(height-r[3])/2;
}
static void display_modes(GameDisplay *d){
    static const int common[][2]={{640,480},{800,600},{1024,768},{1280,720},{1280,960},
        {1280,1024},{1600,900},{1600,1200},{1920,1080},{1920,1440},{2560,1440},{3840,2160}};
    SDL_DisplayID id=SDL_GetDisplayForWindow(d->window);SDL_Rect usable;int count,i,j;
    d->mode_count=0;SDL_free(d->fullscreen_modes);d->fullscreen_modes=NULL;
    if(d->draft.fullscreen){
        d->fullscreen_modes=SDL_GetFullscreenDisplayModes(id,&count);
        for(i=0;d->fullscreen_modes && i<count && d->mode_count<96;++i){
            const SDL_DisplayMode *m=d->fullscreen_modes[i];
            if(m->w<640 || m->h<480 || m->w>8192 || m->h>8192)continue;
            for(j=0;j<d->mode_count;++j)if(d->modes[j].width==m->w && d->modes[j].height==m->h)break;
            if(j==d->mode_count)d->modes[d->mode_count++]=(DisplayMode){m->w,m->h,m};
        }
    }else{
        if(!SDL_GetDisplayUsableBounds(id,&usable))usable=(SDL_Rect){0,0,1920,1080};
        for(i=0;i<(int)(sizeof(common)/sizeof(*common));++i)
            if(common[i][0]<=usable.w && common[i][1]<=usable.h)
                d->modes[d->mode_count++]=(DisplayMode){common[i][0],common[i][1],NULL};
    }
    /* Always offer a recovery window, even on a very small desktop. */
    if(!d->mode_count){d->draft.fullscreen=0;d->modes[d->mode_count++]=(DisplayMode){640,480,NULL};}
}
static int display_mode_index(GameDisplay *d){
    int i;for(i=0;i<d->mode_count;++i)
        if(d->modes[i].width==d->draft.width && d->modes[i].height==d->draft.height)return i;
    return -1;
}
static int display_set(GameDisplay *d,DisplayOptions options){
    SDL_DisplayMode mode;const SDL_DisplayMode *selected=NULL;
    if(options.width<320 || options.height<200 || options.width>8192 || options.height>8192)return 0;
    if(options.fullscreen && game_application_isolated_input(d->application)){
        SDL_SetError("Fullscreen is disabled during isolated automation");return 0;
    }
    if(options.fullscreen){
        if(!SDL_GetClosestFullscreenDisplayMode(SDL_GetDisplayForWindow(d->window),options.width,options.height,0,false,&mode) ||
           mode.w!=options.width || mode.h!=options.height)return 0;
        selected=&mode;
    }
    if(!SDL_SetWindowFullscreen(d->window,false) || !SDL_SetWindowFullscreenMode(d->window,selected) ||
       !SDL_SetWindowSize(d->window,options.width,options.height) ||
       (options.fullscreen && !SDL_SetWindowFullscreen(d->window,true)))return 0;
    (void)SDL_SyncWindow(d->window);d->active=options;return 1;
}
static void display_restore(GameDisplay *d){
    if(!display_set(d,d->previous)){
        DisplayOptions recovery={640,480,0,0};(void)display_set(d,recovery);
    }
    d->deadline=0;d->draft=d->active;display_modes(d);
}
static void display_close(GameDisplay *d){
    if(d->deadline)display_restore(d);
    d->open=0;d->armed=-1;
    (void)game_menu_input_acquire(d->application,0);(void)game_menu_input_acquire(d->application,1);
}
void game_display_tick(GameApplication *a){
    GameDisplay *d=a->display;
    if(d && d->deadline && SDL_GetTicks()>=d->deadline)display_restore(d);
}
static int display_configure(GameDisplay *d,DisplayOptions options){
    GameConfig *c=game_application_config(d->application);
    return c && game_config_set_number(c,"display","width",options.width) &&
        game_config_set_number(c,"display","height",options.height) &&
        game_config_set_number(c,"display","fullscreen",options.fullscreen) &&
        game_config_set_number(c,"display","fsr1",options.fsr);
}
static int display_persist(GameDisplay *d){
    if(display_configure(d,d->active) && game_application_config_flush(d->application))return 1;
    /* A later unrelated settings flush must not publish a rejected mode. */
    (void)display_configure(d,d->previous);return 0;
}
int game_display_begin(GameApplication *a,SDL_Window *window,GameMenuFonts *fonts){
    GameConfig *c=game_application_config(a);GameDisplay *d=calloc(1,sizeof(*d));SDL_Color green[2]={{0,0,0,0},{88,255,38,255}};
    if(!d)return 0;
    a->display=d;d->application=a;d->window=window;d->fonts=fonts;d->armed=-1;
    d->active=(DisplayOptions){640,480,0,0};display_fsr_begin(&d->fsr,a);
    d->canvas=SDL_CreateSurface(640,480,SDL_PIXELFORMAT_RGBA32);
    d->labels=SDL_CreateSurface(640,480,SDL_PIXELFORMAT_INDEX8);d->label_palette=SDL_CreatePalette(2);
    if(!d->canvas || !d->labels || !d->label_palette || !SDL_SetPaletteColors(d->label_palette,green,0,2) ||
       !SDL_SetSurfacePalette(d->labels,d->label_palette) || !SDL_SetSurfaceColorKey(d->labels,true,0))return 0;
    if(game_application_load_blob(a,"media\\dibs\\opts\\grx\\okoff.bmp",&d->button_blob))
        (void)asset_bitmap_menu_parse(&d->button_blob,&d->button);
    if(!game_application_isolated_input(a) && !SDL_getenv("UPRISING_RECOMP_HEADLESS")){
        DisplayOptions saved={(int)game_config_number(c,"display","width",640),
            (int)game_config_number(c,"display","height",480),
            game_config_number(c,"display","fullscreen",0)==1,
            game_config_number(c,"display","fsr1",0)==1 && d->fsr.available};
        if(!display_set(d,saved)){(void)display_set(d,d->active);SDL_ClearError();}
    }
    d->draft=d->active;display_modes(d);return 1;
}
void game_display_end(GameApplication *a){
    GameDisplay *d=a->display;if(!d)return;a->display=NULL;
    /* Restore the desktop before destroying the window/context. */
    (void)SDL_SetWindowFullscreen(d->window,false);
    display_fsr_end(&d->fsr);SDL_DestroySurface(d->labels);SDL_DestroySurface(d->canvas);SDL_DestroyPalette(d->label_palette);
    resource_blob_release(&d->button_blob);SDL_free(d->fullscreen_modes);free(d);
}
void game_display_graphics_drawer(GameApplication *a,int x,int y,int visible,int enabled){
    GameDisplay *d=a->display;if(!d)return;
    d->drawer_x=x+330;d->drawer_y=y+3;d->drawer_visible=visible;if(enabled>=0)d->drawer_enabled=enabled;
    if(!visible && d->open)display_close(d);
}
static int inside(int x,int y,int left,int top,int width,int height){return x>=left && y>=top && x<left+width && y<top+height;}
static int display_hit(GameDisplay *d,int x,int y){
    if(!d->open)return d->drawer_visible && d->drawer_enabled && inside(x,y,d->drawer_x,d->drawer_y,112,26)?7:-1;
    if(d->deadline){if(inside(x,y,170,333,140,30))return 5;if(inside(x,y,330,333,140,30))return 6;return -1;}
    if(inside(x,y,322,176,180,30))return 0;
    if(inside(x,y,322,222,180,30))return 1;
    if(inside(x,y,322,268,180,30))return 2;
    if(inside(x,y,170,333,140,30))return 3;
    if(inside(x,y,330,333,140,30))return 4;
    return -1;
}
static void display_action(GameDisplay *d,int action,int direction){
    int index;
    switch(action){
    case 7:
        d->open=1;d->draft=d->active;d->status[0]=0;d->focus=0;display_modes(d);
        (void)game_menu_slider_cancel(d->application);(void)game_menu_input_unacquire(d->application);
        (void)SDL_SetWindowRelativeMouseMode(d->window,false);(void)SDL_SetWindowMouseGrab(d->window,false);break;
    case 0:
        d->draft.fullscreen=!d->draft.fullscreen;display_modes(d);
        if(display_mode_index(d)<0){d->draft.width=d->modes[0].width;d->draft.height=d->modes[0].height;}break;
    case 1:
        index=display_mode_index(d);index=(index+direction+d->mode_count)%d->mode_count;
        d->draft.width=d->modes[index].width;d->draft.height=d->modes[index].height;break;
    case 2:if(d->fsr.available)d->draft.fsr=!d->draft.fsr;break;
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
        if(key==SDL_SCANCODE_ESCAPE)display_action(d,d->deadline?6:4,1);
        else if(key==SDL_SCANCODE_TAB || key==SDL_SCANCODE_DOWN || key==SDL_SCANCODE_UP){
            int first=d->deadline?5:0,count=d->deadline?2:5;
            d->focus=first+(d->focus-first+count+(key==SDL_SCANCODE_UP?-1:1))%count;
        }else if(key==SDL_SCANCODE_RETURN || key==SDL_SCANCODE_KP_ENTER || key==SDL_SCANCODE_SPACE ||
                 key==SDL_SCANCODE_LEFT || key==SDL_SCANCODE_RIGHT)
            display_action(d,d->focus,key==SDL_SCANCODE_LEFT?-1:1);
        return 1;
    }
    return d->open && (event->type==SDL_EVENT_KEY_UP || event->type==SDL_EVENT_TEXT_INPUT || event->type==SDL_EVENT_MOUSE_WHEEL);
}
static Uint32 display_color(SDL_Surface *s,int r,int g,int b){return SDL_MapSurfaceRGB(s,(Uint8)r,(Uint8)g,(Uint8)b);}
static void display_fill(SDL_Surface *s,int x,int y,int w,int h,Uint32 color){SDL_Rect r={x,y,w,h};(void)SDL_FillSurfaceRect(s,&r,color);}
static void display_text(GameDisplay *d,const char *text,int x,int y,int right,unsigned font){
    (void)game_menu_font_paint(d->fonts,font,text,d->labels->pixels,(size_t)d->labels->pitch*d->labels->h,
        640,480,d->labels->pitch,x,y,right,1,0,0);
}
static void display_button(GameDisplay *d,SDL_Surface *s,int x,int y,int w,int h,const char *text,int focused){
    int i,j;Uint32 *row;AssetBitmap *b=&d->button;
    display_fill(s,x,y,w,h,display_color(s,58,52,37));
    /* Reuse the original recessed metal border; replace only its label area. */
    if(b->pixels)for(j=0;j<h;++j){
        row=(Uint32 *)((Uint8 *)s->pixels+(y+j)*s->pitch);
        for(i=0;i<w;++i){
            unsigned bx=(unsigned)i*b->width/w,by=(unsigned)j*b->height/h;uint32_t c;
            if(bx>b->width/4 && bx<b->width*3/4 && by>b->height/3 && by<b->height*2/3)
                bx=b->width/4; /* Erase the baked-in OK label using its own background. */
            c=asset_bitmap_pixel(b,bx,by);
            if(!b->rgb24){const uint8_t *p=b->palette_bgra+(c&255)*4;c=(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16;}
            if(x+i>=0 && x+i<s->w)row[x+i]=display_color(s,(c>>16)&255,(c>>8)&255,c&255);
        }
    }
    if(focused){Uint32 green=display_color(s,65,150,28);
        display_fill(s,x,y,w,1,green);display_fill(s,x,y+h-1,w,1,green);}
    {int32_t tw,th;if(game_menu_font_measure(d->fonts,2,text,&tw,&th))display_text(d,text,x+(w-tw)/2,y+(h-th)/2,x+w,2);}
}
int game_display_overlay(GameApplication *a,SDL_Surface *rgba){
    GameDisplay *d=a->display;SDL_Surface *s;SDL_Rect target;char text[160];int x,y,hover;
    if(!d || !rgba || !d->drawer_visible)return 1;
    if(d->deadline && SDL_GetTicks()>=d->deadline)display_restore(d);
    s=d->canvas;if(!s)return 0;
    (void)SDL_SetSurfaceBlendMode(s,SDL_BLENDMODE_BLEND);(void)SDL_FillSurfaceRect(s,NULL,0);
    (void)SDL_FillSurfaceRect(d->labels,NULL,0);
    if(!d->open && d->drawer_enabled && d->drawer_x>=0 && d->drawer_x+112<=640 && d->drawer_y>=0 && d->drawer_y+26<=480)
        display_button(d,s,d->drawer_x,d->drawer_y,112,26,"ADVANCED",inside(d->mouse_x,d->mouse_y,d->drawer_x,d->drawer_y,112,26));
    if(d->open){
        display_fill(s,0,0,640,480,SDL_MapSurfaceRGBA(s,0,0,0,160));
        display_fill(s,115,108,410,280,display_color(s,18,16,12));
        /* Deterministic weathered metal, with recessed black control wells. */
        for(y=112;y<384;++y)for(x=119;x<521;++x){
            int grain=(int)(((unsigned)x*17u+(unsigned)y*29u+((unsigned)x*(unsigned)y)%31u)%11u);
            Uint32 *p=(Uint32 *)((Uint8 *)s->pixels+y*s->pitch);p[x]=display_color(s,61+grain,56+grain,41+grain);
        }
        display_fill(s,128,152,384,2,display_color(s,18,17,11));
        display_text(d,"ADVANCED GRAPHICS",138,123,510,4);
        if(d->deadline){
            display_text(d,"KEEP THESE DISPLAY SETTINGS?",152,195,510,3);
            snprintf(text,sizeof(text),"Reverting in %u seconds",(unsigned)((d->deadline-SDL_GetTicks()+999)/1000));
            display_text(d,text,175,239,510,2);
            display_button(d,s,170,333,140,30,"KEEP",d->focus==5);
            display_button(d,s,330,333,140,30,"REVERT",d->focus==6);
        }else{
            display_text(d,"DISPLAY MODE",138,185,315,2);display_text(d,"RESOLUTION",138,231,315,2);
            display_text(d,"FSR 1 UPSCALING",138,277,315,2);
            hover=display_hit(d,d->mouse_x,d->mouse_y);
            display_button(d,s,322,176,180,30,d->draft.fullscreen?"< FULLSCREEN >":"< WINDOWED >",d->focus==0 || hover==0);
            snprintf(text,sizeof(text),"< %d x %d >",d->draft.width,d->draft.height);
            display_button(d,s,322,222,180,30,text,d->focus==1 || hover==1);
            display_button(d,s,322,268,180,30,!d->fsr.available?"UNAVAILABLE":d->draft.fsr?"< ON >":"< OFF >",d->focus==2 || hover==2);
            display_button(d,s,170,333,140,30,"APPLY",d->focus==3 || hover==3);
            display_button(d,s,330,333,140,30,"CANCEL",d->focus==4 || hover==4);
            display_text(d,d->status[0]?d->status:"Original aspect ratio preserved. FSR affects gameplay.",138,310,505,0);
        }
    }
    (void)SDL_BlitSurface(d->labels,NULL,s,NULL);target=(SDL_Rect){0,0,rgba->w,rgba->h};
    x=SDL_BlitSurfaceScaled(s,NULL,rgba,&target,SDL_SCALEMODE_NEAREST);return x;
}
int game_display_present(GameApplication *a,unsigned texture,int sw,int sh,int top_down,int menu){
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
    if(!menu && d->active.fsr && (r[2]>sw || r[3]>sh)){
        ok=display_fsr_draw(&d->fsr,texture,sw,sh,r);
        if(!ok){d->active.fsr=0;d->draft.fsr=0;snprintf(d->status,sizeof(d->status),"FSR failed; original scaling restored.");
            game_application_log(a,"%s\n",d->status);}
    }else ok=0;
    if(!ok){glViewport(r[0],r[1],r[2],r[3]);glBindTexture(GL_TEXTURE_2D,texture);display_quad(top_down);}
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(mode);glPopAttrib();
    return glGetError()==GL_NO_ERROR;
}
