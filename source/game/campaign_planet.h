#ifndef UPRISING_CAMPAIGN_PLANET_H
#define UPRISING_CAMPAIGN_PLANET_H
#include "application_entry.h"
GameAddress game_campaign_planet_create(GameApplication *,const char *,const float[3],float,uint16_t,float,uint32_t,float,float);
int game_campaign_planet_destroy(GameApplication *,GameAddress);
int game_campaign_planet_materials_prepare(GameApplication *,GameAddress);
int game_campaign_planet_bind(GameApplication *,GameAddress,GameAddress);
int game_campaign_planet_texture_advance(GameApplication *,GameAddress);
int game_campaign_planet_present(GameApplication *,GameAddress);
#endif
