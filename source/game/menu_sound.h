#ifndef UPRISING_PORTABLE_MENU_SOUND_H
#define UPRISING_PORTABLE_MENU_SOUND_H
#include "application_entry.h"
#include "sound_playback.h"
int game_menu_sound_play(GameApplication *,const SoundPlaybackRequest *);
int game_menu_sound_stop(GameApplication *,int16_t,uint32_t);
int game_menu_sound_stop_all(GameApplication *); /*52e230*/
int game_menu_sound_update(GameApplication *);
int game_menu_sound_frequency(GameApplication *,GameAddress,int16_t,uint32_t,float);
int game_menu_sound_playing(GameApplication *,int16_t,uint32_t,int *);
int game_menu_sound_volume(GameApplication *,int16_t,uint32_t,float);
int game_menu_sound_bank_begin(GameApplication *);
int game_menu_sound_bank_load(GameApplication *,uint32_t category);
int game_menu_sound_bank_end(GameApplication *);
int game_menu_sound_sample_begin(GameApplication *,GameAddress sample,GameAddress bank,
    GameAddress name,uint32_t voices,float radius);
int game_menu_sound(GameApplication *,uint32_t);
int game_menu_game_start_sound(GameApplication *);
int game_menu_sound_loop_begin(GameApplication *,uint32_t,GameAddress publication);
int game_menu_sound_loop_end(GameApplication *,uint32_t,uint32_t voice);
int game_menu_sound_drag_begin(GameApplication *,uint32_t,float);
int game_menu_sound_drag_pitch(GameApplication *,uint32_t,float);
int game_menu_sound_mode(GameApplication *,uint32_t);
int game_menu_sound_sequence(GameApplication *,const int16_t[8],int *);
int game_menu_sound_queue_clear(GameApplication *,GameAddress);
int game_menu_sound_queue_submit(GameApplication *,GameAddress,const int16_t[8],int *);
#endif
