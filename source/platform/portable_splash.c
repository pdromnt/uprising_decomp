#include "app_identity.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "application_entry.h"
#include "asset_bitmap.h"
#include "scene_projection_storage.h"

typedef struct MenuSplash {HWND window;GameAddress identity;} MenuSplash;
static int splash_name(GameMemory *m,GameAddress address,char text[260]) {
    unsigned i;
    for(i=0;i<260u;++i) {
        text[i]=(char)game_memory_read_u8(m,address+i);
        if(m->fault)return 0;
        if(!text[i])return 1;
    }
    return 0;
}
/* 005069b0: close the optional startup window and publish application activation. */
void game_application_splash_end(GameApplication *a) {
    MenuSplash *splash=a->menu_splash;
    if(splash) {
        if(splash->window)DestroyWindow(splash->window);
        scene_projection_storage_free(splash->identity);free(splash);a->menu_splash=NULL;
    }
    game_memory_write_u32(&a->memory,0x624f88u,0);
    game_memory_write_u32(&a->memory,0x6270f4u,1);
}
void game_application_asset_shutdown(GameApplication *a) {game_application_splash_end(a);}
/* 00506640: bootstrap uses the FIRST archive match, before the shared catalog.
 * Bitmap bytes/palette are owned ResourceBlob views; GDI receives native structs.
 * The original missing-splash path continues startup. Headless decoding creates
 * no desktop window, DC, bitmap or audible device. */
int game_application_asset_bootstrap(GameApplication *a,const char *command) {
    char archive[260],resource[260],class_name[260],title[260];
    ResourceBlob blob={0};AssetBitmap bitmap;uint8_t encoded[1028];size_t written;unsigned i;
    LOGPALETTE *palette=NULL;HPALETTE logical=NULL,previous_palette=NULL;HBITMAP image=NULL;
    PALETTEENTRY entries[256];
    struct {BITMAPINFOHEADER header;RGBQUAD colors[256];} dib;
    HDC screen=NULL,dc=NULL,source=NULL;HGDIOBJ previous_image=NULL;MenuSplash *splash=NULL;
    int ok=1,found;
    (void)command;
    if(a->menu_splash)return 0;
    if(!splash_name(&a->memory,0x59735cu,archive) || !splash_name(&a->memory,0x59736cu,resource))return 0;
    found=resource_blob_open_named(archive,resource,&blob,NULL);
    if(found==0)return 1;
    if(found<0 || !asset_bitmap_startup_palette(&blob,&bitmap,encoded,sizeof(encoded),&written)) {ok=0;goto done;}
    if(GetEnvironmentVariableA("UPRISING_RECOMP_HEADLESS",NULL,0))goto done;
    if(!asset_bitmap_menu_parse(&blob,&bitmap) || !splash_name(&a->memory,0x597398u,title) ||
        !splash_name(&a->memory,0x5973a4u,class_name)) {ok=0;goto done;}
    palette=malloc(offsetof(LOGPALETTE,palPalEntry)+sizeof(entries));if(!palette){ok=0;goto done;}
    palette->palVersion=0x300u;palette->palNumEntries=(WORD)bitmap.palette_count;
    for(i=0;i<bitmap.palette_count;++i) {
        entries[i].peRed=encoded[4u+i*4u];entries[i].peGreen=encoded[5u+i*4u];
        entries[i].peBlue=encoded[6u+i*4u];entries[i].peFlags=encoded[7u+i*4u];
    }
    memcpy((uint8_t *)palette+offsetof(LOGPALETTE,palPalEntry),entries,bitmap.palette_count*sizeof(*entries));
    logical=CreatePalette(palette);
    /*The loading image is an archive view, not a loose host file. Build native
     *GDI metadata explicitly; never cast the packed file header to a host struct. */
    memset(&dib,0,sizeof(dib));dib.header.biSize=sizeof(dib.header);
    dib.header.biWidth=(LONG)bitmap.width;
    dib.header.biHeight=bitmap.top_down?-(LONG)bitmap.height:(LONG)bitmap.height;
    dib.header.biPlanes=1;dib.header.biBitCount=bitmap.rgb24?24:8;
    dib.header.biCompression=BI_RGB;dib.header.biClrUsed=bitmap.palette_count;
    if(bitmap.palette_count)memcpy(dib.colors,bitmap.palette_bgra,bitmap.palette_count*sizeof(RGBQUAD));
    screen=GetDC(NULL);if(!screen){ok=0;goto done;}
    image=CreateDIBitmap(screen,&dib.header,CBM_INIT,bitmap.pixels,(BITMAPINFO *)&dib,DIB_RGB_COLORS);
    if(!logical || !image) {ok=0;goto done;}
    splash=calloc(1,sizeof(*splash));if(!splash){ok=0;goto done;}
    splash->identity=scene_projection_storage_allocate(4u,"portable startup window identity");
    if(!splash->identity){ok=0;goto done;}
    splash->window=CreateWindowExA(0,class_name,UPRISING_WINDOW_TITLE,WS_POPUP|WS_VISIBLE|WS_SYSMENU,
        (GetDeviceCaps(screen,HORZRES)-(int)bitmap.width)/2,
        (GetDeviceCaps(screen,VERTRES)-(int)bitmap.height)/2,(int)bitmap.width,(int)bitmap.height,
        NULL,NULL,game_application_instance(a),NULL);
    if(!splash->window){ok=0;goto done;}
    dc=GetDC(splash->window);if(!dc){ok=0;goto done;}
    previous_palette=SelectPalette(dc,logical,FALSE);RealizePalette(dc);
    source=CreateCompatibleDC(dc);if(!source){ok=0;goto done;}
    previous_image=SelectObject(source,image);
    if(!previous_image || previous_image==HGDI_ERROR ||
        !BitBlt(dc,0,0,(int)bitmap.width,(int)bitmap.height,source,0,0,SRCCOPY)){ok=0;goto done;}
    a->menu_splash=splash;game_memory_write_u32(&a->memory,0x624f88u,splash->identity);
done:
    if(source) {if(previous_image && previous_image!=HGDI_ERROR)SelectObject(source,previous_image);DeleteDC(source);}
    if(dc) {if(previous_palette)SelectPalette(dc,previous_palette,FALSE);ReleaseDC(splash->window,dc);}
    if(screen)ReleaseDC(NULL,screen);
    if(image)DeleteObject(image);
    if(logical)DeleteObject(logical);
    free(palette);resource_blob_release(&blob);
    if(splash && a->menu_splash!=splash) {
        if(splash->window)DestroyWindow(splash->window);
        if(splash->identity)scene_projection_storage_free(splash->identity);
        free(splash);
    }
    return ok && !a->memory.fault;
}
