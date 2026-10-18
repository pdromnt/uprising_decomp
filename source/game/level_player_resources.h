#ifndef UPRISING_LEVEL_PLAYER_RESOURCES_H
#define UPRISING_LEVEL_PLAYER_RESOURCES_H
#include "application_entry.h"
int game_level_player_base_prepare(GameApplication *,GameAddress); /*4769a0/477df0*/
int game_level_player_base_resources_release(GameApplication *,GameAddress);
int game_level_player_prepare(GameApplication *,GameAddress);
int game_level_player_resources_release(GameApplication *,GameAddress);
int game_level_player_command_models_release(GameApplication *);
int game_level_player_catalog_value(GameApplication *,GameAddress,uint8_t,unsigned,uint32_t,uint32_t *);
int game_level_player_catalog_record(GameApplication *,GameAddress,uint8_t,GameAddress *);
int game_level_player_catalog_choice(GameApplication *,GameAddress,uint8_t,uint32_t *);
int game_level_player_catalog_mount(GameApplication *,GameAddress,uint8_t,uint8_t,uint32_t,uint32_t *);
int game_level_player_catalog_unit_price(GameApplication *,GameAddress,int16_t,uint32_t *);
#endif
