#ifndef UPRISING_MENU_CHOICE_CONTROLS_H
#define UPRISING_MENU_CHOICE_CONTROLS_H
#include "menu_widgets.h"
GameAddress game_menu_choice_create(GameApplication *,uint32_t,GameAddress,GameAddress,uint32_t,uint32_t,const GameAddress[3]);
int game_menu_choice_option(GameApplication *,GameAddress,unsigned,GameAddress,uint32_t);
int game_menu_choice_charge(GameApplication *,GameAddress,int32_t,uint32_t *);
int game_menu_choice_release(GameApplication *,GameAddress);
int game_menu_research_popup_refresh(GameApplication *);
int game_menu_research_purchase(GameApplication *);
int game_menu_faction_budget_feedback(GameApplication *);
int game_menu_research_catalog_build(GameApplication *,GameAddress,uint32_t);
int game_menu_research_hover(GameApplication *,uint32_t,GameAddress,uint32_t,GameAddress);
#endif
