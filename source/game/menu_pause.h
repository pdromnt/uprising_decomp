#ifndef UPRISING_PORTABLE_MENU_PAUSE_H
#define UPRISING_PORTABLE_MENU_PAUSE_H
#include "application_entry.h"
int game_menu_ingame_pause_state(GameApplication *,uint32_t);
int game_menu_pause_action(GameApplication *,uint32_t);
int game_input_pause(GameApplication *,int);
int game_level_pause_begin(GameApplication *);
int game_level_pause_resume(GameApplication *,uint32_t);
void game_level_pause_reset(GameApplication *);
void game_level_pause_mark(GameApplication *);
uint32_t game_level_pause_elapsed(GameApplication *);
int game_level_resume_presentation(GameApplication *);
int game_level_clock_pause_request(GameApplication *,int resume,int end_game);
#endif
