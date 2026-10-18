#ifndef UPRISING_LEVEL_SETTINGS_H
#define UPRISING_LEVEL_SETTINGS_H
#include "application_entry.h"
int game_level_settings_defaults(GameApplication *);
int game_level_settings_line(GameApplication *,const char *);
int game_level_settings_read(GameApplication *,const char *);
int game_network_level_capacity(GameApplication *,GameAddress,uint32_t *);
int game_level_player_slot_bindings(GameApplication *);
int game_level_player_configuration_publish(GameApplication *);
#endif
