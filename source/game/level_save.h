#ifndef UPRISING_LEVEL_SAVE_H
#define UPRISING_LEVEL_SAVE_H
#include "application_entry.h"
int game_level_save_scenario(GameApplication *,const char *);
int game_level_save_footer(GameApplication *,ResourceFile *);
int game_menu_saved_snapshot_export(GameApplication *,const char *);
/* Shared 454570/4577c0/4fe650 writers borrow existing path-state records. */
int game_level_save_path(GameApplication *,ResourceFile *,GameAddress);
int game_level_save_navigation(GameApplication *,ResourceFile *,GameAddress);
int game_level_save_motion(GameApplication *,ResourceFile *,GameAddress);
int game_level_save_script(GameApplication *,ResourceFile *);
int game_level_save_spatial(GameApplication *,ResourceFile *);
int game_level_catalog_save(GameApplication *,GameAddress,ResourceFile *);
int game_level_entity_save(GameApplication *,GameAddress,ResourceFile *);
int game_level_entity_restore(GameApplication *,GameAddress,ResourceFile *);
int game_level_entity_restore_fix(GameApplication *,GameAddress);
int game_level_catalog_restore(GameApplication *,GameAddress,ResourceFile *);
int game_level_command_save(GameApplication *,GameAddress,ResourceFile *);
#endif
