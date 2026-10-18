#ifndef UPRISING_UI_TILED_IMAGE_H
#define UPRISING_UI_TILED_IMAGE_H
#include "screen_primitives.h"
#include <stddef.h>
#ifndef UPRISING_GAME_UI_RECT_DEFINED
#define UPRISING_GAME_UI_RECT_DEFINED
typedef struct GameUiRect { int32_t left,top,right,bottom; } GameUiRect;
#endif
typedef struct GameUiIndexedImage { const uint8_t *pixels;size_t size;uint32_t width,height,stride; } GameUiIndexedImage;
typedef struct GameUiTileOps {
    void *context;
    int (*begin)(void *);
    int (*draw_tile)(void *,const uint8_t pixels[1024],const GameRenderVertex vertices[4]);
    int (*finish)(void *);
} GameUiTileOps;
typedef enum GameUiTiledImageResult { GAME_UI_TILED_IMAGE_FAULT=0,GAME_UI_TILED_IMAGE_OK=1,GAME_UI_TILED_IMAGE_INVALID=2 } GameUiTiledImageResult;
void game_ui_tile_vertices(GameRenderVertex output[4],float x1,float y1,float x2,float y2,float s,float t,float oow,float epsilon);
GameUiTiledImageResult game_ui_draw_tiled_image(const GameUiIndexedImage *,const GameUiRect *source,
    const GameUiRect *destination,float oow,float epsilon,const GameUiTileOps *);
GameUiTiledImageResult game_ui_draw_fullscreen_image(const GameUiIndexedImage *,const GameUiTileOps *);
#endif
