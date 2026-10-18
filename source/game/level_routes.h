#ifndef UPRISING_PORTABLE_LEVEL_ROUTES_H
#define UPRISING_PORTABLE_LEVEL_ROUTES_H
#include "application_entry.h"
int game_level_routes_load(GameApplication *,const char *,int32_t,uint32_t *);
int game_level_routes_release(GameApplication *);
int game_level_gate_restore_route_flags(GameApplication *);
/*Live typeA2 factory and placement consumers must be reconstructed before
 * this route can link. These are ordinary arguments, never a virtual CPU ABI. */
GameAddress game_level_gate_create(GameApplication *);
GameAddress game_level_gate_template_load(GameApplication *);
GameAddress game_level_teleporter_destination_template(GameApplication *);
GameAddress game_level_teleporter_factory(GameApplication *,uint8_t,uint16_t,unsigned,int32_t);
int game_level_gate_place(GameApplication *,GameAddress,uint32_t,const float[3],uint16_t,int);
int game_level_gate_destroy(GameApplication *,GameAddress);
int game_level_gate_frame(GameApplication *,GameAddress,int rendered);
int game_level_gate_controller(GameApplication *,GameAddress);
int game_level_gate_begin(GameApplication *,GameAddress,uint32_t state);
int game_level_gate_occupancy(GameApplication *,GameAddress,int enabled);
int game_level_gate_update(GameApplication *,GameAddress);
int game_level_gate_present(GameApplication *,GameAddress);
#endif
