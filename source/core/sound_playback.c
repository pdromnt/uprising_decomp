#include "sound_playback.h"
#include <math.h>
#include <string.h>

static float sound_real(GameMemory *m,GameAddress address){
    uint32_t bits=game_memory_read_u32(m,address);float value;memcpy(&value,&bits,4u);return value;
}
static uint32_t sound_integer(long double value){
    if(!isfinite(value) || value < -0x1p63L || value >= 0x1p63L)return 0u;
    return (uint32_t)(uint64_t)(int64_t)value;
}
static int sound_failure(GameMemory *m,GameAddress address){
    m->fault=1;m->fault_address=address;m->fault_size=4u;return 0;
}
static GameAddress sound_sample(GameMemory *m,GameAddress bank,int16_t sound){
    return game_memory_read_u32(m,bank+12u+(uint32_t)(int32_t)sound*4u);
}
static GameAddress sound_voice(GameMemory *m,GameAddress sample,uint32_t voice){
    return game_memory_read_u32(m,game_memory_read_u32(m,sample+12u)+voice*4u);
}
GameAddress sound_playback_select(GameMemory *m,GameAddress sample,const SoundPlaybackServices *s){
    uint32_t count=game_memory_read_u32(m,sample+8u),index,selected=0u,furthest=0u;
    GameAddress buffer=0u,bank;
    if(m->fault)return 0u;
    if(!count || count>INT32_MAX){sound_failure(m,sample+8u);return 0u;}
    for(index=0u;index<count && !m->fault;index++){
        uint32_t status=0u,position=0u;
        buffer=sound_voice(m,sample,index);if(m->fault)return 0u;
        if(s->status(s->context,buffer,&status)<0)status=0u;
        if(m->fault)return 0u;
        if(!(status&1u))break;
        /* Device-position failure uses zero rather than an uninitialized
         * original stack value; ordinary successful cursor behavior is exact. */
        (void)s->position(s->context,buffer,&position);
        if(position>=furthest){furthest=position;selected=index;}
    }
    if(m->fault)return 0u;
    if(index==count){
        index=selected;buffer=sound_voice(m,sample,index);if(m->fault)return 0u;
        (void)s->command(s->context,buffer,SOUND_BUFFER_STOP,0u);
        if(m->fault)return 0u;
        (void)s->command(s->context,buffer,SOUND_BUFFER_SEEK,0u);
    }
    bank=game_memory_read_u32(m,0x628a90u);
    game_memory_write_u32(m,bank,index);
    return m->fault?0u:buffer;
}
float sound_playback_attenuation(GameMemory *m,GameAddress sample,float distance){
    uint32_t bits=game_memory_read_u32(m,sample+24u);float range;long double ratio;
    if(!(bits&0x7fffffffu))return sound_real(m,0x575bf8u);
    memcpy(&range,&bits,4u);ratio=((long double)range-distance)/range;
    return ratio>sound_real(m,0x575c00u)?(float)ratio:sound_real(m,0x575bfcu);
}
int sound_playback_play(GameMemory *m,GameAddress bank,const SoundPlaybackRequest *r,const SoundPlaybackServices *s){
    GameAddress sample,buffer;float volume=r->volume,attenuation;uint32_t bits,frequency;
    long double wide;float threshold;
    if(r->sound==-1)return 1;
    memcpy(&bits,&volume,4u);if(bits==0x3f800000u)volume=sound_real(m,bank+8u);
    if(!game_memory_read_u32(m,bank+0x468u))return !m->fault;
    sample=sound_sample(m,bank,r->sound);if(!sample)return !m->fault;
    bits=game_memory_read_u32(m,sample+24u);
    attenuation=(bits&0x7fffffffu)?sound_playback_attenuation(m,sample,r->distance_squared):1.0f;
    wide=(long double)volume*attenuation;threshold=sound_real(m,0x575bfcu);
    volume=(float)wide;
    if(m->fault)return 0;
    if((wide==threshold || isnan(wide) || isnan(threshold)) && !r->force)return 1;
    buffer=sound_playback_select(m,sample,s);if(!buffer)return 0;
    bits=sound_integer(((long double)sound_real(m,0x575bf8u)-volume)*sound_real(m,0x575c04u));
    (void)s->command(s->context,buffer,SOUND_BUFFER_VOLUME,bits);if(m->fault)return 0;
    frequency=game_memory_read_u32(m,sample+16u);
    bits=sound_integer((long double)frequency*r->pitch);
    (void)s->command(s->context,buffer,SOUND_BUFFER_FREQUENCY,bits);if(m->fault)return 0;
    (void)s->command(s->context,buffer,SOUND_BUFFER_PLAY,r->flags);return !m->fault;
}
int sound_playback_volume(GameMemory *m,GameAddress bank,int16_t sound,uint32_t voice,float volume,const SoundPlaybackServices *s){
    GameAddress buffer;uint32_t bits;long double gain;
    if(!game_memory_read_u32(m,bank+0x468u))return !m->fault;
    gain=(long double)sound_real(m,bank+8u)*volume;
    bits=sound_integer(((long double)sound_real(m,0x575bf8u)-gain)*sound_real(m,0x575c04u));
    buffer=sound_voice(m,sound_sample(m,bank,sound),voice);if(m->fault)return 0;
    (void)s->command(s->context,buffer,SOUND_BUFFER_VOLUME,bits);return !m->fault;
}
int sound_playback_frequency(GameMemory *m,GameAddress bank,int16_t sound,uint32_t voice,float pitch,const SoundPlaybackServices *s){
    GameAddress sample,buffer;uint32_t bits;
    if(!game_memory_read_u32(m,bank+0x468u))return !m->fault;
    sample=sound_sample(m,bank,sound);
    bits=sound_integer((long double)game_memory_read_u32(m,sample+16u)*pitch);
    buffer=sound_voice(m,sample,voice);if(m->fault)return 0;
    (void)s->command(s->context,buffer,SOUND_BUFFER_FREQUENCY,bits);return !m->fault;
}
int sound_playback_stop(GameMemory *m,GameAddress bank,int16_t sound,uint32_t voice,const SoundPlaybackServices *s){
    GameAddress buffer;
    if(!game_memory_read_u32(m,bank+0x468u))return !m->fault;
    buffer=sound_voice(m,sound_sample(m,bank,sound),voice);if(m->fault)return 0;
    (void)s->command(s->context,buffer,SOUND_BUFFER_STOP,0u);if(m->fault)return 0;
    (void)s->command(s->context,buffer,SOUND_BUFFER_SEEK,0u);return !m->fault;
}
long double sound_playback_remaining(GameMemory *m,GameAddress bank,int16_t sound,uint32_t voice,const SoundPlaybackServices *s){
    GameAddress sample,buffer;uint32_t status=0u,cursor=0u,frequency=0u,remaining;uint16_t bits=0u;int32_t signed_remaining;
    if(!game_memory_read_u32(m,bank+0x468u))return sound_real(m,0x575bfcu);
    sample=sound_sample(m,bank,sound);buffer=sound_voice(m,sample,voice);
    if(m->fault)return 0.0L;
    if(s->status(s->context,buffer,&status)!=0 || !(status&1u))return sound_real(m,0x575bfcu);
    if(s->position(s->context,buffer,&cursor)!=0)return sound_real(m,0x575bfcu);
    /* The original ignores GetFormat failure. Zeroed outputs avoid reading
     * indeterminate bytes; failure parity remains explicitly unverified. */
    (void)s->format(s->context,buffer,&frequency,&bits);
    remaining=(game_memory_read_u32(m,sample+4u)-cursor)>>(bits!=8u);
    signed_remaining=remaining<=INT32_MAX?(int32_t)remaining:(int32_t)((int64_t)remaining-4294967296LL);
    return (long double)signed_remaining/frequency;
}
static int sound_queue_start(GameMemory *m,GameAddress bank,int16_t sound,const SoundPlaybackServices *s){
    GameAddress sample,buffer;float volume;
    if(sound!=-1){
        sample=sound_sample(m,bank,sound);volume=sound_real(m,bank+8u);
        if(sample && volume!=sound_real(m,0x575bfcu) && !isnan(volume)){
            buffer=sound_playback_select(m,sample,s);if(!buffer)return 0;
            (void)s->command(s->context,buffer,SOUND_BUFFER_VOLUME,
                sound_integer(((long double)sound_real(m,0x575bf8u)-volume)*sound_real(m,0x575c04u)));
            if(m->fault)return 0;
            (void)s->command(s->context,buffer,SOUND_BUFFER_FREQUENCY,0u);if(m->fault)return 0;
            (void)s->command(s->context,buffer,SOUND_BUFFER_PLAY,0u);if(m->fault)return 0;
        }
    }
    game_memory_write_u32(m,bank+4u,game_memory_read_u32(m,bank));return !m->fault;
}
int sound_playback_queue_update(GameMemory *m,GameAddress bank,const SoundPlaybackServices *s){
    uint16_t sound;unsigned index;float threshold;long double remaining;
    if(!game_memory_read_u32(m,bank+0x468u))return !m->fault;
    sound=game_memory_read_u16(m,bank+0x46cu);if(sound==0xffffu)return !m->fault;
    remaining=sound_playback_remaining(m,bank,(int16_t)sound,game_memory_read_u32(m,bank+4u),s);threshold=sound_real(m,0x575c08u);
    /* The original C0 branch advances on both less-than and unordered. */
    if(!(remaining<threshold || isnan(remaining) || isnan(threshold)))return !m->fault;
    for(index=0u;index<7u && !m->fault;index++){
        uint16_t next=game_memory_read_u16(m,bank+0x46eu+index*2u);
        game_memory_write_u16(m,bank+0x46cu+index*2u,next);if(next==0xffffu)break;
    }
    sound=game_memory_read_u16(m,bank+0x46cu);if(sound==0xffffu)return !m->fault;
    /* Unlike positional playback the queue applies no distance attenuation. */
    return sound_queue_start(m,bank,(int16_t)sound,s);
}
int sound_playback_queue_submit(GameMemory *m,GameAddress bank,const int16_t sounds[8],const SoundPlaybackServices *s){
    unsigned i;
    if(!game_memory_read_u32(m,bank+0x468u) || game_memory_read_u16(m,bank+0x46cu)!=0xffffu)return 0;
    for(i=0;i<8u;i++)game_memory_write_u16(m,bank+0x46cu+i*2u,(uint16_t)sounds[i]);
    return !m->fault && sound_queue_start(m,bank,sounds[0],s);
}
int sound_playback_queue_clear(GameMemory *m,GameAddress bank,const SoundPlaybackServices *s){
    unsigned i;uint16_t sound;
    if(!game_memory_read_u32(m,bank+0x468u))return !m->fault;
    sound=game_memory_read_u16(m,bank+0x46cu);if(sound==0xffffu)return !m->fault;
    if(!sound_playback_stop(m,bank,(int16_t)sound,game_memory_read_u32(m,bank+4u),s))return 0;
    for(i=0;i<9u;i++)game_memory_write_u16(m,bank+0x46cu+i*2u,0xffffu);
    return !m->fault;
}
int sound_playback_stop_all(GameMemory *m,GameAddress bank,const SoundPlaybackServices *s){
    unsigned i;uint32_t voice;
    if(!game_memory_read_u32(m,bank+0x468u))return !m->fault;
    for(i=0;i<276u && !m->fault;i++){
        GameAddress sample=game_memory_read_u32(m,bank+12u+i*4u);uint32_t count;
        if(!sample)continue;
        count=game_memory_read_u32(m,sample+8u);if(count>INT32_MAX)continue;
        for(voice=0;voice<count && !m->fault;voice++){
            GameAddress buffer=sound_voice(m,sample,voice);
            (void)s->command(s->context,buffer,SOUND_BUFFER_STOP,0u);if(m->fault)return 0;
            (void)s->command(s->context,buffer,SOUND_BUFFER_SEEK,0u);
        }
    }
    for(i=0;i<9u;i++)game_memory_write_u16(m,bank+0x46cu+i*2u,0xffffu);
    return !m->fault;
}
int sound_playback_playing(GameMemory *m,GameAddress bank,int16_t sound,uint32_t voice,const SoundPlaybackServices *s){
    GameAddress buffer;uint32_t status=0u;
    if(!game_memory_read_u32(m,bank+0x468u))return 0;
    buffer=sound_voice(m,sound_sample(m,bank,sound),voice);if(m->fault)return 0;
    return s->status(s->context,buffer,&status)==0 && (status&1u)!=0u;
}
long double sound_playback_duration(GameMemory *m,GameAddress bank,int16_t sound,const SoundPlaybackServices *s){
    GameAddress sample,buffer;uint32_t frequency=0u,samples;uint16_t bits=0u;int32_t signed_samples;
    if(!game_memory_read_u32(m,bank+0x468u))return sound_real(m,0x575bfcu);
    sample=sound_sample(m,bank,sound);buffer=sound_voice(m,sample,0u);if(m->fault)return 0.0L;
    (void)s->format(s->context,buffer,&frequency,&bits);
    samples=game_memory_read_u32(m,sample+4u)>>(bits!=8u);
    signed_samples=samples<=INT32_MAX?(int32_t)samples:(int32_t)((int64_t)samples-4294967296LL);
    return (long double)signed_samples/frequency;
}
