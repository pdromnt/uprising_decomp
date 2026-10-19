#include "application_entry.h"
#include "sound_platform.h"
extern void game_music_volume(uint32_t);
extern uint32_t game_music_volume_read(void);
/* App-local gains replace AUX/WAVE device volume. Never alter desktop volume. */
int game_menu_audio_initialize(GameApplication *a){
    game_memory_write_u32(&a->memory,0x6319a0,1);game_memory_write_u32(&a->memory,0x6319b0,1);
    game_memory_write_u32(&a->memory,0x6319a8,game_music_volume_read());
    game_memory_write_u32(&a->memory,0x6319a4,sound_platform_master_volume_read());return !a->memory.fault;
}
int game_menu_aux_volume(GameApplication *a,uint32_t volume){game_music_volume(volume);return !a->memory.fault;}
int game_menu_wave_volume(GameApplication *a,uint32_t volume){sound_platform_master_volume(volume);return !a->memory.fault;}
uint32_t game_menu_aux_volume_read(GameApplication *a){(void)a;return game_music_volume_read();}
uint32_t game_menu_wave_volume_read(GameApplication *a){(void)a;return sound_platform_master_volume_read();}
int game_menu_audio_restore(GameApplication *a){return !a->memory.fault;}
