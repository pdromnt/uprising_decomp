#ifndef UPRISING_PORTABLE_FONT_H
#define UPRISING_PORTABLE_FONT_H
#include <stddef.h>
#include <stdint.h>
typedef struct GameMenuFonts GameMenuFonts;
GameMenuFonts *game_menu_fonts_open(void);
void game_menu_fonts_close(GameMenuFonts *);
int game_menu_font_measure(GameMenuFonts *,unsigned,const char *,int32_t *,int32_t *);
int game_menu_font_wrapped_height(GameMenuFonts *,unsigned,const char *,int32_t,uint32_t *);
int game_menu_font_paint(GameMenuFonts *,unsigned,const char *,uint8_t *,size_t,
    int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,uint8_t,uint32_t,int);
#endif
