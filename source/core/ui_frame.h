#ifndef UPRISING_UI_FRAME_H
#define UPRISING_UI_FRAME_H

#include "game_memory.h"

typedef enum GameUiFrameKind {
    GAME_UI_FRAME_CLEAR,
    GAME_UI_FRAME_PALETTE_TRANSITION,
    GAME_UI_FRAME_WORK_IMAGE,
    GAME_UI_FRAME_MARKER_IMAGE
} GameUiFrameKind;

typedef struct GameUiFrameOps {
    void *context;
    int (*screen_mask)(void *, int);
    int (*present)(void *, uint32_t);
    int (*palette)(void *, GameAddress);
    int (*fullscreen)(void *, GameAddress, uint32_t);
} GameUiFrameOps;

typedef enum GameUiPaletteRole {
    GAME_UI_PALETTE_WORK,
    GAME_UI_PALETTE_WORLD
} GameUiPaletteRole;

/* Image is an opaque resource handle supplied by the platform edge. */
typedef struct GameUiFrameOwnedOps {
    void *context;
    int (*screen_mask)(void *, int);
    int (*present)(void *, uint32_t);
    int (*palette)(void *, GameUiPaletteRole);
    int (*fullscreen)(void *, uint32_t image, uint32_t stride);
} GameUiFrameOwnedOps;

typedef struct GameUiFrameOwnedResult {
    int reset_markers;
} GameUiFrameOwnedResult;

int game_ui_frame_run_owned(GameUiFrameKind, uint32_t image,
    const GameUiFrameOwnedOps *, GameUiFrameOwnedResult *);

int game_ui_frame_run(GameMemory *, GameUiFrameKind, const GameUiFrameOps *);
int game_ui_release_resources(GameMemory *, void *, int (*)(void *, GameAddress));

#endif
