#ifndef UPRISING_PORTABLE_GAMEPLAY_NOTICES_H
#define UPRISING_PORTABLE_GAMEPLAY_NOTICES_H
#include "application_entry.h"
int game_level_vehicle_end_camera(GameApplication *,GameAddress vehicle);
int game_script_notice_publish(GameApplication *,GameAddress player,uint32_t kind,GameAddress text,uint32_t delay,uint32_t extra);
#endif
