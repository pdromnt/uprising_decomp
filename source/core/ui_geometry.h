#ifndef UPRISING_UI_GEOMETRY_H
#define UPRISING_UI_GEOMETRY_H

#include "game_memory.h"
#include "screen_primitives.h"

#ifndef UPRISING_GAME_UI_RECT_DEFINED
#define UPRISING_GAME_UI_RECT_DEFINED
typedef struct GameUiRect { int32_t left, top, right, bottom; } GameUiRect;
#endif
typedef struct GameUiImage { uint16_t pitch, height; GameAddress pixels; } GameUiImage;
typedef struct GameMenuPixelSpan {uint8_t *pixels;size_t bytes;int32_t width,height,stride;} GameMenuPixelSpan;
/* Bounded indexed copy/stretch, with optional source key and two-axis mirror. */
int game_ui_indexed_copy(const GameMenuPixelSpan *,const GameMenuPixelSpan *,
    const GameUiRect *,const GameUiRect *,int,uint8_t,int);
typedef struct GameUiImageOps {
    void *context;
    int (*draw)(void *, const GameUiRect *, const GameUiRect *,
        const GameUiImage *, uint32_t, int);
} GameUiImageOps;
typedef struct GameUiMaskOps {
    void *context;
    int (*clear)(void *, uint32_t, uint32_t);
    int (*begin)(void *);
    int (*draw)(void *, GameRenderVertex[4]);
    int (*publish)(void *,const GameRenderVertex[4]);
} GameUiMaskOps;
typedef enum GameUiResult { GAME_UI_RESULT_FAULT = 0, GAME_UI_RESULT_OK = 1 } GameUiResult;

typedef struct GameUiMaskFrame {
    float bounds[4];
    int32_t overlays[2][4];
    uint32_t mode;
} GameUiMaskFrame;
GameUiResult game_ui_draw_screen_mask_owned(int enabled,
    const GameUiMaskFrame *,GameRenderVertex vertices[4],
    const GameUiMaskOps *,int *used_last_corner);
void game_ui_mask_stats(uint32_t *owned,uint32_t *memory_based);

GameUiResult game_ui_draw_screen_mask(GameMemory *, int, GameAddress,
    const GameUiMaskOps *, uint32_t *);
GameUiResult game_ui_draw_marker(GameMemory *, GameAddress,
    const GameUiImageOps *);
GameUiResult game_ui_draw_image_rectangle(int32_t, int32_t, int32_t, int32_t,
    uint32_t, uint32_t, GameAddress, const GameUiImageOps *);
GameUiResult game_ui_draw_image_list(GameMemory *, GameAddress,
    const GameUiImageOps *);
/*Borrowed indexed surface; rectangle edges use inclusive pixel endpoints. */
int game_ui_indexed_borders(uint8_t *,size_t,int32_t,int32_t,int32_t,
    const GameUiRect *,const uint8_t *,size_t);

#endif
