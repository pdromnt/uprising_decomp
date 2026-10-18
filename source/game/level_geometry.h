#ifndef UPRISING_PORTABLE_LEVEL_GEOMETRY_H
#define UPRISING_PORTABLE_LEVEL_GEOMETRY_H
#include "application_entry.h"
int game_level_geometry_load(GameApplication *,GameAddress); /*49ca40*/
int game_level_geometry_release(GameApplication *); /*49ec40*/
/*The remaining connected control constructors/retirement are mandatory native
 *dependencies. They must build actual controls, never successful placeholders. */
int game_level_geometry_controls_initialize(GameApplication *);
int game_level_geometry_controls_release(GameApplication *);
int game_level_deployment_menu_begin(GameApplication *); /*434ae0*/
int game_level_deployment_actions_begin(GameApplication *); /*4836a0*/
#endif
