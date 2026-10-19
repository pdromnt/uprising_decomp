#ifndef UPRISING_MENU_MUSIC_H
#define UPRISING_MENU_MUSIC_H
#include "application_entry.h"
/* Typed platform boundary; all callback/thread/window pointers stay native. */
uint32_t game_music_device_open(GameApplication *,uint32_t *device,uint32_t *volume);
uint32_t game_music_device_status(GameApplication *,uint32_t device,uint32_t item,uint32_t track,uint32_t *value);
uint32_t game_music_device_play(GameApplication *,uint32_t device,uint32_t first,uint32_t last,int has_last);
uint32_t game_music_device_stop(GameApplication *,uint32_t device);
uint32_t game_music_device_close(GameApplication *,uint32_t device);
void game_music_device_error(uint32_t,char *,size_t);
int game_music_worker_start(GameApplication *);
void game_music_worker_join(GameApplication *);
void game_music_shutdown(GameApplication *);
void game_music_volume(uint32_t);
uint32_t game_music_volume_read(void);
int game_menu_music_refresh(GameApplication *);
int game_menu_music_action(GameApplication *,uint32_t,uint32_t);
int game_menu_music_title(GameApplication *,char *,size_t);
int game_menu_music_start(GameApplication *,uint32_t);
int game_menu_music_play(GameApplication *,GameAddress,int32_t);
int game_menu_music_stop(GameApplication *,GameAddress);
int game_menu_music_notify(GameApplication *,uint32_t,uintptr_t);
int game_menu_shutdown_systems(GameApplication *);
#endif
