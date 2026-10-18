#ifndef UPRISING_LEVEL_PLAYER_FACTORY_H
#define UPRISING_LEVEL_PLAYER_FACTORY_H
#include "application_entry.h"
GameAddress game_level_player_factory_template(GameApplication *,uint16_t);
int game_level_player_factory_fill(GameApplication *,uint16_t,int32_t);
GameAddress game_level_player_factory_take(GameApplication *,uint16_t);
int game_level_player_factory_destroy(GameApplication *,GameAddress);
int game_level_player_clear_events(GameApplication *,GameAddress);
int game_level_player_factory_shutdown(GameApplication *);
#endif
