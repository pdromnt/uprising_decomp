#ifndef UPRISING_CAMPAIGN_WORLD_H
#define UPRISING_CAMPAIGN_WORLD_H
#include "campaign_planet.h"
int game_campaign_new(GameApplication *);
int game_campaign_destroy(GameApplication *);
GameAddress game_campaign_system(GameApplication *,uint32_t);
GameAddress game_campaign_mission(GameApplication *,uint32_t,uint32_t);
int game_campaign_complete(GameApplication *,uint32_t,uint32_t);
int game_campaign_load(GameApplication *,const char *path);
int game_campaign_save(GameApplication *,const char *path);
void game_campaign_factions_default(GameApplication *);
int game_campaign_level_value(GameApplication *,const char *,const char *,char *,size_t);
#endif
