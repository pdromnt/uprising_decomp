#ifndef UPRISING_MENU_LEVEL_CATALOG_H
#define UPRISING_MENU_LEVEL_CATALOG_H
#include "application_entry.h"
typedef struct GameLevelNames {char names[100][128];unsigned count;} GameLevelNames;
/*Read-only native directory service. Names exclude the .lfl extension; filters
 * are case-sensitive substrings of the actual level file, as00470090. */
int game_menu_level_files(GameApplication *,GameLevelNames *,const char *pattern,const char *extension,const char *const *,unsigned);
GameAddress game_menu_level_row_create(GameApplication *,const char *,uint32_t width,uint32_t height,uint32_t font,uint32_t x,uint32_t y,uint32_t callback);
GameAddress game_menu_text_button_create(GameApplication *,const char *,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
#endif
