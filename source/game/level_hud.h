#ifndef UPRISING_PORTABLE_LEVEL_HUD_H
#define UPRISING_PORTABLE_LEVEL_HUD_H
#include "application_entry.h"
int game_level_window_initialize(GameApplication *); /*4ac830*/
int game_level_hud_assets_release(GameApplication *);
GameAddress game_level_hud_quad_create(GameApplication *); /*412790*/
int game_level_hud_quad_image(GameApplication *,GameAddress,float,GameAddress); /*412800*/
int game_level_hud_quad_release(GameApplication *,GameAddress);
#endif
