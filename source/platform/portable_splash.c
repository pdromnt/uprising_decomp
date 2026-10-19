#include "app_identity.h"
#include "portable_sdl.h"
#include "portable_input_devices.h"
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include <stdio.h>
#include "asset_bitmap.h"
#include "scene_projection_storage.h"

typedef struct MenuSplash {SDL_Window *window;GameAddress identity;int video;} MenuSplash;
static int splash_name(GameMemory *m,GameAddress address,char text[260]){
    unsigned i;for(i=0;i<260;++i){text[i]=(char)game_memory_read_u8(m,address+i);if(m->fault)return 0;if(!text[i])return 1;}
    return 0;
}
static void splash_release(MenuSplash *splash){
    if(!splash)return;
    SDL_DestroyWindow(splash->window);
    if(splash->video)SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if(splash->identity)scene_projection_storage_free(splash->identity);
    free(splash);
}
void game_application_splash_end(GameApplication *a){
    splash_release(a->menu_splash);a->menu_splash=NULL;
    game_memory_write_u32(&a->memory,0x624f88,0);game_memory_write_u32(&a->memory,0x6270f4,1);
}
void game_application_asset_shutdown(GameApplication *a){game_application_splash_end(a);}
/* Bootstrap uses the first archive match and its existing bounded DIB parser.
 * SDL owns the temporary software window; it never changes a display mode. */
int game_application_asset_bootstrap(GameApplication *a){
    char archive[260],resource[260];ResourceBlob blob={0};AssetBitmap bitmap;
    uint8_t encoded[1028];size_t written;MenuSplash *splash=NULL;SDL_Surface *image=NULL,*front;
    unsigned x,y;int found,ok=1;const char *stage="startup bitmap";
    if(a->menu_splash){fprintf(stderr,"startup splash failed: splash already owned\n");return 0;}
    if(!splash_name(&a->memory,0x59735c,archive)){
        fprintf(stderr,"startup splash failed: invalid archive name\n");return 0;
    }
    if(!splash_name(&a->memory,0x59736c,resource)){
        fprintf(stderr,"startup splash failed: invalid bitmap name\n");return 0;
    }
    game_application_log(a,"startup splash archive: %s\n",archive);
    game_application_log(a,"startup splash bitmap: %s\n",resource);
    found=resource_blob_open_named(archive,resource,&blob,NULL);if(!found)return 1;
    if(found<0 || !asset_bitmap_startup_palette(&blob,&bitmap,encoded,sizeof(encoded),&written)){ok=0;goto done;}
    if(getenv("UPRISING_RECOMP_HEADLESS"))goto done;
    if(!asset_bitmap_menu_parse(&blob,&bitmap) || bitmap.width>16384 || bitmap.height>16384){ok=0;goto done;}
    splash=calloc(1,sizeof(*splash));if(!splash){ok=0;goto done;}
    splash->identity=scene_projection_storage_allocate(4,"SDL startup window");if(!splash->identity){ok=0;goto done;}
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN,game_application_isolated_input(a)?"0":"1");
    SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE,"0");
    stage="SDL video initialization";
    if(!SDL_InitSubSystem(SDL_INIT_VIDEO)){ok=0;goto done;}splash->video=1;
    stage="splash window creation";
    splash->window=SDL_CreateWindow(UPRISING_WINDOW_TITLE,(int)bitmap.width,(int)bitmap.height,SDL_WINDOW_BORDERLESS|SDL_WINDOW_HIDDEN);
    if(!splash->window){ok=0;goto done;}
    stage="splash image creation";
    image=SDL_CreateSurface((int)bitmap.width,(int)bitmap.height,SDL_PIXELFORMAT_RGBA32);
    if(!image){ok=0;goto done;}
    for(y=0;y<bitmap.height;++y)for(x=0;x<bitmap.width;++x){
        uint32_t color=asset_bitmap_pixel(&bitmap,x,y);uint8_t *pixel=(uint8_t *)image->pixels+(size_t)y*image->pitch+x*4;
        if(!bitmap.rgb24){
            const uint8_t *entry=bitmap.palette_bgra+(size_t)color*4;
            if(color>=bitmap.palette_count){ok=0;goto done;}
            color=(uint32_t)entry[2]<<16|(uint32_t)entry[1]<<8|entry[0];
        }
        pixel[0]=(uint8_t)(color>>16);pixel[1]=(uint8_t)(color>>8);pixel[2]=(uint8_t)color;pixel[3]=255;
    }
    stage="splash window surface/presentation";
    front=SDL_GetWindowSurface(splash->window);
    if(!front || !SDL_BlitSurface(image,NULL,front,NULL) || !SDL_ShowWindow(splash->window) || !SDL_UpdateWindowSurface(splash->window)){ok=0;goto done;}
    a->menu_splash=splash;game_memory_write_u32(&a->memory,0x624f88,splash->identity);
 done:
    if(!ok)fprintf(stderr,"startup splash failed: %s; SDL: %s\n",stage,SDL_GetError());
    SDL_DestroySurface(image);resource_blob_release(&blob);
    if(splash && a->menu_splash!=splash)splash_release(splash);
    return ok && !a->memory.fault;
}
