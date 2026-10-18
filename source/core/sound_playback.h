#ifndef UPRISING_SOUND_PLAYBACK_H
#define UPRISING_SOUND_PLAYBACK_H
#include "game_memory.h"

/* Borrowed buffer identities, not host pointers. The device owns the buffers;
 * the existing bank/sample records retain their verified storage layout. */
typedef enum SoundBufferCommand {
    SOUND_BUFFER_VOLUME, SOUND_BUFFER_FREQUENCY, SOUND_BUFFER_PLAY,
    SOUND_BUFFER_STOP, SOUND_BUFFER_SEEK
} SoundBufferCommand;
typedef struct SoundPlaybackServices {
    void *context;
    int32_t (*status)(void *,GameAddress,uint32_t *);
    int32_t (*position)(void *,GameAddress,uint32_t *);
    int32_t (*command)(void *,GameAddress,SoundBufferCommand,uint32_t);
    int32_t (*format)(void *,GameAddress,uint32_t *samples_per_second,uint16_t *bits_per_sample);
} SoundPlaybackServices;
typedef struct SoundPlaybackRequest {
    int16_t sound;
    float distance_squared,volume,pitch;
    uint32_t flags;
    int force;
} SoundPlaybackRequest;
GameAddress sound_playback_select(GameMemory *,GameAddress sample,const SoundPlaybackServices *);
float sound_playback_attenuation(GameMemory *,GameAddress sample,float distance_squared);
int sound_playback_play(GameMemory *,GameAddress bank,const SoundPlaybackRequest *,const SoundPlaybackServices *);
int sound_playback_volume(GameMemory *,GameAddress bank,int16_t sound,uint32_t voice,float volume,const SoundPlaybackServices *);
int sound_playback_frequency(GameMemory *,GameAddress bank,int16_t sound,uint32_t voice,float pitch,const SoundPlaybackServices *);
int sound_playback_stop(GameMemory *,GameAddress bank,int16_t sound,uint32_t voice,const SoundPlaybackServices *);
long double sound_playback_remaining(GameMemory *,GameAddress bank,int16_t sound,uint32_t voice,const SoundPlaybackServices *);
int sound_playback_queue_update(GameMemory *,GameAddress bank,const SoundPlaybackServices *);
int sound_playback_queue_submit(GameMemory *,GameAddress bank,const int16_t sounds[8],const SoundPlaybackServices *);
int sound_playback_queue_clear(GameMemory *,GameAddress bank,const SoundPlaybackServices *);
int sound_playback_stop_all(GameMemory *,GameAddress bank,const SoundPlaybackServices *);
int sound_playback_playing(GameMemory *,GameAddress bank,int16_t sound,uint32_t voice,const SoundPlaybackServices *);
long double sound_playback_duration(GameMemory *,GameAddress bank,int16_t sound,const SoundPlaybackServices *);
#endif
