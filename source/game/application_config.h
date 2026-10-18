#ifndef UPRISING_APPLICATION_CONFIG_H
#define UPRISING_APPLICATION_CONFIG_H
#include "application_entry.h"
#include "game_config.h"
GameConfig *game_application_config(GameApplication *);
int game_application_config_flush(GameApplication *);
#endif
