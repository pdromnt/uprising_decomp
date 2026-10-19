#include "portable_sdl.h"
#include "menu_music.h"
#include <stdio.h>
#include <string.h>
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_INTEGER_CONVERSION
#include "../third_party/stb_vorbis.c"
extern int game_application_audio_muted(GameApplication *);

/* The menu's track IDs remain stable; there is no CD device or MCI command
 * execution. Decoder/file/stream ownership stays in this one platform object. */
static struct {
    GameApplication *app;SDL_AudioStream *stream;stb_vorbis *decoder;
    uint32_t samples[100],rates[100],last,track,volume;int initialized,playing,finished,retry;
    uint64_t retry_at;uint64_t decoded_frames;
} music={.volume=65535};
uint32_t game_menu_milliseconds(GameApplication *a){(void)a;return (uint32_t)SDL_GetTicks();}
static void SDLCALL music_callback(void *context,SDL_AudioStream *stream,int additional,int total){
    float pcm[4096];stb_vorbis_info info;(void)context;(void)total;
    if(!music.decoder || !music.playing)return;
    info=stb_vorbis_get_info(music.decoder);
    while(additional>0){int frames=additional/(int)(sizeof(float)*info.channels);
        int decoded;if(frames<1)frames=1;if(frames>2048)frames=2048;
        decoded=stb_vorbis_get_samples_float_interleaved(music.decoder,info.channels,pcm,frames*info.channels);
        if(!decoded){music.playing=0;music.finished=stb_vorbis_get_error(music.decoder)?-1:1;break;}
        music.decoded_frames+=(unsigned)decoded;
        if(!SDL_PutAudioStreamData(stream,pcm,decoded*info.channels*(int)sizeof(float))){music.playing=0;music.finished=-1;break;}
        additional-=decoded*info.channels*(int)sizeof(float);
    }
}
static void stop_track(void){
    SDL_AudioStream *stream=music.stream;
    if(stream){SDL_PauseAudioStreamDevice(stream);SDL_LockAudioStream(stream);}
    music.playing=music.finished=0;
    if(music.decoder){stb_vorbis_close(music.decoder);music.decoder=NULL;}
    if(stream){SDL_UnlockAudioStream(stream);SDL_DestroyAudioStream(stream);music.stream=NULL;}
}
uint32_t game_music_device_open(GameApplication *a,uint32_t *device,uint32_t *volume){
    unsigned i;char path[64];int error;*device=*volume=0;
    if(music.app && music.app!=a)return 1;
    if(music.app)game_music_device_close(a,1);
    music.app=a;music.last=0;memset(music.samples,0,sizeof(music.samples));memset(music.rates,0,sizeof(music.rates));
    for(i=2;i<100;++i){stb_vorbis *decoder;stb_vorbis_info info;
        snprintf(path,sizeof(path),"Music/Track%02u.ogg",i);decoder=stb_vorbis_open_filename(path,&error,NULL);if(!decoder)continue;
        info=stb_vorbis_get_info(decoder);
        if(info.channels>=1 && info.channels<=2 && info.sample_rate){
            music.samples[i]=stb_vorbis_stream_length_in_samples(decoder);music.rates[i]=info.sample_rate;
            if(music.samples[i])music.last=i;}
        stb_vorbis_close(decoder);
    }
    if(!music.initialized){if(!SDL_InitSubSystem(SDL_INIT_AUDIO)){music.app=NULL;return 1;}music.initialized=1;}
    *device=1;*volume=1;return 0;
}
uint32_t game_music_device_status(GameApplication *a,uint32_t device,uint32_t item,uint32_t track,uint32_t *value){
    uint32_t seconds;(void)a;*value=0;if(device!=1 || !music.app)return 1;
    switch(item){
    case 1:if(track>=100 || !music.rates[track])return 1;
        seconds=music.samples[track]/music.rates[track];*value=(seconds/60)|((seconds%60)<<8);break;
    case 3:*value=music.last;break;
    case 4:if(music.stream)SDL_LockAudioStream(music.stream);*value=music.playing?0x20e:0x20d;
        if(music.stream)SDL_UnlockAudioStream(music.stream);
        break;
    case 5:case 7:*value=music.last!=0;break;
    case 0x4001:*value=track<100 && music.samples[track]?0x440:0;break;
    default:return 1;
    }return 0;
}
uint32_t game_music_device_play(GameApplication *a,uint32_t device,uint32_t first,uint32_t last,int has_last){
    char path[64];int error;stb_vorbis_info info;SDL_AudioSpec format;(void)last;(void)has_last;
    if(device!=1 || music.app!=a || first>=100 || !music.samples[first])return 1;
    stop_track();snprintf(path,sizeof(path),"Music/Track%02u.ogg",first);
    music.decoder=stb_vorbis_open_filename(path,&error,NULL);if(!music.decoder)return 1;
    info=stb_vorbis_get_info(music.decoder);
    if(info.channels<1 || info.channels>2 || !info.sample_rate || info.sample_rate>384000){stop_track();return 1;}
    format.format=SDL_AUDIO_F32;format.channels=info.channels;format.freq=info.sample_rate;
    music.stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&format,music_callback,NULL);
    if(!music.stream){stop_track();return 1;}
    music.track=first;music.decoded_frames=0;music.playing=1;
    if(game_application_audio_muted(a)){
        static int reported;if(!reported){fputs("native CD music: playback suppressed (muted)\n",stderr);reported=1;}}
    SDL_SetAudioStreamGain(music.stream,game_application_audio_muted(a)?0:music.volume/65535.0f);
    if(!SDL_ResumeAudioStreamDevice(music.stream)){stop_track();return 1;}return 0;
}
uint32_t game_music_device_stop(GameApplication *a,uint32_t device){if(device!=1 || music.app!=a)return 1;
    stop_track();return 0;}
uint32_t game_music_device_close(GameApplication *a,uint32_t device){
    if(device!=1 || music.app!=a)return 1;
    stop_track();
    if(music.initialized){SDL_QuitSubSystem(SDL_INIT_AUDIO);music.initialized=0;}
    music.app=NULL;music.last=music.track=0;music.retry=0;return 0;
}
void game_music_shutdown(GameApplication *a){if(music.app==a)(void)game_music_device_close(a,1);}
void game_music_device_error(uint32_t code,char *text,size_t capacity){snprintf(text,capacity,"SDL/OGG playback error %u: %s",code,SDL_GetError());}
void game_music_volume(uint32_t value){music.volume=value&65535;
    if(music.stream)SDL_SetAudioStreamGain(music.stream,game_application_audio_muted(music.app)?0:music.volume/65535.0f);}
uint32_t game_music_volume_read(void){return music.volume;}
int game_music_worker_start(GameApplication *a){if(music.app && music.app!=a)return 0;music.retry=1;music.retry_at=SDL_GetTicks()+500;return 1;}
void game_music_worker_join(GameApplication *a){if(music.app==a)music.retry=0;}
/* Only the window thread touches menu records. Audio callbacks publish a scalar
 * completion under SDL's stream lock; playlist changes happen here. */
int game_music_update(GameApplication *a){
    int finished=0;
    if(music.app!=a)return 1;
    if(music.stream){SDL_LockAudioStream(music.stream);finished=music.finished;music.finished=0;SDL_UnlockAudioStream(music.stream);}
    if(finished && !game_menu_music_notify(a,finished>0?1:8,1))return 0;
    if(music.retry && SDL_GetTicks()>=music.retry_at){music.retry_at=SDL_GetTicks()+500;if(game_menu_music_refresh(a))music.retry=0;}
    return !a->memory.fault;
}
