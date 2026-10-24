#ifndef UPRISING_PORTABLE_DISPLAY_H
#define UPRISING_PORTABLE_DISPLAY_H
#include "portable_sdl.h"
#include "portable_font.h"

int game_display_begin(GameApplication *,SDL_Window *,GameMenuFonts *);
void game_display_end(GameApplication *);
void game_display_graphics_drawer(GameApplication *,int x,int y,int visible,int enabled);
int game_display_event(GameApplication *,const SDL_Event *); /* Consumed by modal UI. */
void game_display_tick(GameApplication *);
int game_display_overlay(GameApplication *,SDL_Surface *rgba);
void game_display_viewport(int width,int height,int source_width,int source_height,int rect[4]);
int game_display_antialias_samples(GameApplication *);
/* Returns whether temporal reconstruction is enabled. Size is also supplied
 * for spatial AA; zero when both scene filters are disabled/unavailable.
 * Logical game coordinates remain640x480; sub-native outputs use native size. */
int game_display_temporal_output(GameApplication *,uint32_t size[2]);
int game_display_temporal_native(GameApplication *);
float game_display_sharpness(GameApplication *);
#endif
