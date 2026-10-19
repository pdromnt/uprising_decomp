#include "sound_platform.h"
#include "portable_sdl.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define AUDIO_INVALID ((int32_t)-2147024809)
typedef struct SoundClip {
    uint8_t *bytes;uint32_t frames,rate,refs;uint16_t channels,bits,alignment;
} SoundClip;
typedef struct SoundSlot {
    SoundClip *clip;double cursor;float gain;uint32_t frequency;
    uint8_t kind,generation,silent,playing,loop;
} SoundSlot;
static SoundSlot *slots;
static uint32_t count,capacity;
static SoundPlatformStats stats;
static SDL_Mutex *sound_lock;
static SDL_AudioStream *output_stream;
static uint32_t master_volume=65535;
static int audio_initialized;
static void lock_sound(void){if(sound_lock)SDL_LockMutex(sound_lock);}
static void unlock_sound(void){if(sound_lock)SDL_UnlockMutex(sound_lock);}
static SoundSlot *find_sound(GameAddress id,unsigned kind){
    unsigned index=(id&65535u)-1u;
    if((id&0xff000000u)!=0xf9000000u || index>=count || slots[index].kind!=kind ||
       slots[index].generation!=((id>>16)&255u))return NULL;
    return slots+index;
}
static GameAddress own_sound(unsigned kind,int silent){
    uint32_t i;SoundSlot *slot;
    /* Retire exhausted generations so a stale ID never names a new voice. */
    for(i=0;i<count && (slots[i].kind || slots[i].generation==255);++i){}
    if(i==count){
        if(count==capacity){uint32_t next=capacity?capacity*2:256;SoundSlot *storage;
            if(next>65535)next=65535;
            if(next<=capacity)return 0;
            storage=realloc(slots,(size_t)next*sizeof(*slots));if(!storage)return 0;
            memset(storage+capacity,0,(next-capacity)*sizeof(*slots));slots=storage;capacity=next;}
        ++count;
    }
    slot=slots+i;{uint8_t generation=slot->generation+1;if(!generation)generation=1;
        memset(slot,0,sizeof(*slot));slot->generation=generation;}
    slot->kind=(uint8_t)kind;slot->silent=silent!=0;slot->gain=1;
    if(kind==1){++stats.devices;++stats.live_devices;}else{++stats.buffers;++stats.live_buffers;}
    return 0xf9000000u|((uint32_t)slot->generation<<16)|(i+1);
}
static float pcm_sample(const SoundClip *clip,uint32_t frame,unsigned channel){
    const uint8_t *p=clip->bytes+(size_t)frame*clip->alignment+(clip->channels==1?0:channel)*(clip->bits/8);
    if(clip->bits==8)return ((int)*p-128)/128.0f;
    {uint32_t value=p[0]|(uint32_t)p[1]<<8;int32_t signed_value=value<32768?(int32_t)value:(int32_t)value-65536;
     return signed_value/32768.0f;}
}
/* Called under the owner mutex, also used by focused sample/cursor checks.
 * Shared clips are immutable; duplicates own only playback state and a ref. */
static void sound_mix(float *output,unsigned frames){
    unsigned i,j,c;float master=master_volume/65535.0f;
    memset(output,0,(size_t)frames*2*sizeof(float));
    for(i=0;i<count;++i){SoundSlot *s=slots+i;SoundClip *clip=s->clip;double step;
        if(s->kind!=2 || !s->playing || !clip)continue;
        step=s->frequency/48000.0;
        for(j=0;j<frames;++j){uint32_t at,next;float fraction,gain;
            if(s->cursor>=clip->frames){if(s->loop)s->cursor=fmod(s->cursor,clip->frames);else{s->playing=0;break;}}
            at=(uint32_t)s->cursor;next=at+1;if(next>=clip->frames)next=s->loop?0:at;
            fraction=(float)(s->cursor-at);gain=s->silent?0:s->gain*master;
            if(gain)for(c=0;c<2;++c){float first=pcm_sample(clip,at,c),last=pcm_sample(clip,next,c);
                output[j*2+c]+=(first+(last-first)*fraction)*gain;}
            s->cursor+=step;
        }
        if(s->cursor>=clip->frames){if(s->loop)s->cursor=fmod(s->cursor,clip->frames);else{s->cursor=clip->frames;s->playing=0;}}
    }
    for(i=0;i<frames*2;++i){if(output[i]>1)output[i]=1;else if(output[i]<-1)output[i]=-1;}
}
static void SDLCALL sound_callback(void *context,SDL_AudioStream *stream,int additional,int total){
    float pcm[2048];(void)context;(void)total;
    while(additional>0){unsigned frames=(unsigned)(additional+7)/8;if(frames>1024)frames=1024;
        lock_sound();sound_mix(pcm,frames);unlock_sound();
        if(!SDL_PutAudioStreamData(stream,pcm,(int)frames*8))break;
        additional-=(int)frames*8;}
}
static int audio_open(void){
    SDL_AudioSpec format={SDL_AUDIO_F32,2,48000};
    if(output_stream)return 1;
    if(!sound_lock){sound_lock=SDL_CreateMutex();if(!sound_lock)return 0;}
    if(!audio_initialized){if(!SDL_InitSubSystem(SDL_INIT_AUDIO))return 0;audio_initialized=1;}
    output_stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&format,sound_callback,NULL);
    if(!output_stream)return 0;
    if(!SDL_ResumeAudioStreamDevice(output_stream)){SDL_DestroyAudioStream(output_stream);output_stream=NULL;return 0;}
    return 1;
}
int sound_platform_owns(GameAddress id){int result;lock_sound();result=find_sound(id,1)!=NULL || find_sound(id,2)!=NULL;unlock_sound();return result;}
GameAddress sound_platform_device(void *window,const uint8_t *guid,int muted){
    GameAddress id;const char *silent=getenv("UPRISING_RECOMP_SILENT_NATIVE_SOUND");(void)window;(void)guid;
    if(!audio_open())return 0;
    lock_sound();id=own_sound(1,muted || (silent && !strcmp(silent,"1")));unlock_sound();return id;
}
GameAddress sound_platform_create_buffer(GameAddress device_id,const SoundWave *wave){
    SoundClip *clip;SoundSlot *device,*buffer;GameAddress id=0;
    if(!wave || !wave->pcm || !wave->pcm_bytes || !wave->frequency || wave->format!=1 ||
       (wave->channels!=1 && wave->channels!=2) || (wave->bits_per_sample!=8 && wave->bits_per_sample!=16) ||
       wave->block_alignment!=wave->channels*(wave->bits_per_sample/8) || wave->pcm_bytes%wave->block_alignment)return 0;
    clip=calloc(1,sizeof(*clip));if(!clip)return 0;clip->bytes=malloc(wave->pcm_bytes);
    if(!clip->bytes){free(clip);return 0;}memcpy(clip->bytes,wave->pcm,wave->pcm_bytes);
    clip->frames=wave->pcm_bytes/wave->block_alignment;clip->rate=wave->frequency;
    clip->channels=wave->channels;clip->bits=wave->bits_per_sample;clip->alignment=wave->block_alignment;clip->refs=1;
    lock_sound();device=find_sound(device_id,1);
    if(device){int silent=device->silent;id=own_sound(2,silent);
        if(id){buffer=find_sound(id,2);buffer->clip=clip;buffer->frequency=clip->rate;}}
    unlock_sound();if(!id){free(clip->bytes);free(clip);}return id;
}
GameAddress sound_platform_duplicate(GameAddress device_id,GameAddress buffer_id){
    SoundSlot *source,*copy;SoundClip *clip;GameAddress id=0;int silent;
    lock_sound();source=find_sound(buffer_id,2);
    if(find_sound(device_id,1) && source){clip=source->clip;silent=source->silent;
        id=own_sound(2,silent);if(id){copy=find_sound(id,2);copy->clip=clip;copy->frequency=clip->rate;++clip->refs;}}
    unlock_sound();return id;
}
static void release_slot(SoundSlot *slot){
    if(slot->kind==2){SoundClip *clip=slot->clip;if(clip && !--clip->refs){free(clip->bytes);free(clip);}
        slot->clip=NULL;slot->playing=0;++stats.buffer_releases;--stats.live_buffers;
    }else{++stats.device_releases;--stats.live_devices;}slot->kind=0;
}
int sound_platform_release_buffer(GameAddress id){SoundSlot *s;int ok=0;lock_sound();s=find_sound(id,2);if(s){release_slot(s);ok=1;}unlock_sound();return ok;}
int sound_platform_release_device(GameAddress id){SoundSlot *s;int ok=0;lock_sound();s=find_sound(id,1);if(s){release_slot(s);ok=1;}unlock_sound();return ok;}
int32_t sound_platform_status(GameAddress id,uint32_t *value){SoundSlot *s;int32_t code=AUDIO_INVALID;
    lock_sound();s=find_sound(id,2);*value=0;if(s){*value=s->playing?(1u|(s->loop?4u:0u)):0;code=0;}unlock_sound();return code;}
int32_t sound_platform_position(GameAddress id,uint32_t *value){SoundSlot *s;int32_t code=AUDIO_INVALID;
    lock_sound();s=find_sound(id,2);*value=0;if(s){*value=(uint32_t)s->cursor*s->clip->alignment;code=0;}unlock_sound();return code;}
int32_t sound_platform_format(GameAddress id,uint32_t *frequency,uint16_t *bits){SoundSlot *s;int32_t code=AUDIO_INVALID;
    lock_sound();s=find_sound(id,2);*frequency=0;*bits=0;if(s){*frequency=s->clip->rate;*bits=s->clip->bits;code=0;}unlock_sound();return code;}
int32_t sound_platform_frequency(GameAddress id,uint32_t *frequency){SoundSlot *s;int32_t code=AUDIO_INVALID;
    lock_sound();s=find_sound(id,2);*frequency=0;if(s){*frequency=s->frequency;code=0;}unlock_sound();return code;}
int32_t sound_platform_command(GameAddress id,SoundBufferCommand command,uint32_t value){
    SoundSlot *s;int32_t code=AUDIO_INVALID;lock_sound();s=find_sound(id,2);
    if(s){code=0;switch(command){
    case SOUND_BUFFER_VOLUME:{int32_t v=value<=INT32_MAX?(int32_t)value:(int32_t)((int64_t)value-4294967296LL);
        if(v>0 || v< -10000)code=AUDIO_INVALID;else s->gain=v<=-10000?0:powf(10,v/2000.0f);break;}
    case SOUND_BUFFER_FREQUENCY:if(!value)s->frequency=s->clip->rate;else if(value<100 || value>200000)code=AUDIO_INVALID;else s->frequency=value;break;
    case SOUND_BUFFER_PLAY:s->playing=1;s->loop=(value&1)!=0;if(s->cursor>=s->clip->frames)s->cursor=0;break;
    case SOUND_BUFFER_STOP:s->playing=0;break;
    case SOUND_BUFFER_SEEK:if(value>=(uint64_t)s->clip->frames*s->clip->alignment)code=AUDIO_INVALID;else s->cursor=value/s->clip->alignment;break;
    default:code=AUDIO_INVALID;break;}}
    unlock_sound();return code;
}
void sound_platform_master_volume(uint32_t volume){lock_sound();master_volume=volume&65535u;unlock_sound();}
uint32_t sound_platform_master_volume_read(void){uint32_t v;lock_sound();v=master_volume;unlock_sound();return v;}
void sound_platform_shutdown(void){
    uint32_t i;SDL_AudioStream *stream=output_stream;output_stream=NULL;
    /* Stop/join callbacks before freeing their mutex or clips. Never destroy a
     * stream while holding the mutex its callback needs. */
    if(stream)SDL_DestroyAudioStream(stream);
    lock_sound();for(i=0;i<count;++i)if(slots[i].kind)release_slot(slots+i);
    free(slots);slots=NULL;count=capacity=0;unlock_sound();
    if(audio_initialized){SDL_QuitSubSystem(SDL_INIT_AUDIO);audio_initialized=0;}
    if(sound_lock){SDL_DestroyMutex(sound_lock);sound_lock=NULL;}
}
void sound_platform_stats(SoundPlatformStats *output){lock_sound();*output=stats;unlock_sound();}
