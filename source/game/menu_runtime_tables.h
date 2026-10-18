#ifndef UPRISING_MENU_RUNTIME_TABLES_H
#define UPRISING_MENU_RUNTIME_TABLES_H
#include "game_memory.h"
float game_menu_runtime_float(GameMemory *,GameAddress);
void game_menu_runtime_store_float(GameMemory *,GameAddress,float);
void game_menu_character_tables(GameMemory *);
void game_menu_curve_tables(GameMemory *);
void game_menu_view_tables(GameMemory *);
int game_menu_reciprocal_table(GameMemory *);
int game_menu_trig_tables(GameMemory *);
int game_menu_scalar_tables(GameMemory *);
int game_menu_neighborhood_tables(GameMemory *);
int game_menu_pair_heading(GameMemory *,float x,float z,float scale,uint16_t *);
#endif
