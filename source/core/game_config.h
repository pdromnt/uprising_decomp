#ifndef UPRISING_GAME_CONFIG_H
#define UPRISING_GAME_CONFIG_H
#include <stdint.h>
#include <stdio.h>
typedef struct GameConfig GameConfig;
GameConfig *game_config_create(void);
void game_config_free(GameConfig *);
const char *game_config_get(const GameConfig *,const char *section,const char *key);
int game_config_set(GameConfig *,const char *section,const char *key,const char *value);
int game_config_remove(GameConfig *,const char *section,const char *key);
uint32_t game_config_number(const GameConfig *,const char *section,const char *key,uint32_t fallback);
int game_config_set_number(GameConfig *,const char *section,const char *key,uint32_t value);
int game_config_read(GameConfig *,FILE *);
int game_config_write(const GameConfig *,FILE *);
int game_config_dirty(const GameConfig *);
void game_config_saved(GameConfig *);
#endif
