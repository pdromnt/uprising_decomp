#ifndef UPRISING_PORTABLE_MENU_SETTINGS_H
#define UPRISING_PORTABLE_MENU_SETTINGS_H
#include "application_entry.h"
int game_menu_settings_begin(GameApplication *);
int game_menu_settings_load(GameApplication *, GameAddress, ResourceFile *);
int game_menu_settings_save(GameApplication *, GameAddress, ResourceFile *);
int game_menu_settings_end(GameApplication *);
int game_menu_settings_checkpoint(GameApplication *);
int game_menu_settings_string(GameApplication *,GameAddress,const uint8_t *,size_t);
int game_menu_option_set(GameApplication *, uint32_t option, uint32_t value);
uint32_t game_menu_option_get(GameApplication *, uint32_t option);
void game_menu_options_default(GameApplication *);
ResourceFile *game_application_settings_file(GameApplication *, int write);
/* Actual 004afc00 render-quality consumer must be reconstructed, not dropped. */
int game_menu_render_quality(GameApplication *, uint32_t);
#endif
