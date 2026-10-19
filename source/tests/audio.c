/* Focused mixer/owner regression. Use SDL's dummy device, never speakers. */
#include <assert.h>
#include <math.h>
#include "../platform/sound_platform.c"
int main(void){
    uint8_t bytes[]={128,255,0,128};SoundWave wave={0};GameAddress device,buffer,copy,old;float mixed[16];unsigned i;
    assert(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"SDL_AUDIODRIVER","dummy",true));
    wave.pcm=bytes;wave.pcm_bytes=4;wave.frequency=48000;wave.channels=1;
    wave.format=1;wave.bits_per_sample=8;wave.block_alignment=1;wave.bytes_per_second=48000;
    device=sound_platform_device(NULL,NULL,0);assert(device);
    assert(SDL_PauseAudioStreamDevice(output_stream));
    buffer=sound_platform_create_buffer(device,&wave);assert(buffer);
    copy=sound_platform_duplicate(device,buffer);assert(copy && find_sound(copy,2)->clip==find_sound(buffer,2)->clip);
    assert(!sound_platform_command(buffer,SOUND_BUFFER_PLAY,0));
    lock_sound();sound_mix(mixed,4);unlock_sound();
    assert(mixed[0]==0 && mixed[1]==0 && fabsf(mixed[2]-127.0f/128)<0.00001f && mixed[4]==-1);
    assert(!find_sound(buffer,2)->playing && find_sound(buffer,2)->cursor==4);
    assert(!sound_platform_command(buffer,SOUND_BUFFER_SEEK,0));
    assert(!sound_platform_command(buffer,SOUND_BUFFER_FREQUENCY,24000));
    assert(!sound_platform_command(buffer,SOUND_BUFFER_VOLUME,(uint32_t)-2000));
    assert(!sound_platform_command(buffer,SOUND_BUFFER_PLAY,1));
    lock_sound();sound_mix(mixed,2);unlock_sound();
    assert(fabsf(mixed[2]-(127.0f/128)*0.05f)<0.00001f);
    assert(!sound_platform_command(buffer,SOUND_BUFFER_STOP,0));
    assert(sound_platform_release_buffer(buffer));old=buffer;
    assert(find_sound(copy,2)->clip->refs==1);assert(sound_platform_release_buffer(copy));
    for(i=0;i<512;++i){buffer=sound_platform_create_buffer(device,&wave);assert(buffer && buffer!=old);assert(sound_platform_release_buffer(buffer));}
    assert(count<=5 && stats.live_buffers==0);
    buffer=sound_platform_create_buffer(device,&wave);assert(buffer);
    assert(sound_platform_command(buffer,SOUND_BUFFER_SEEK,4)<0);
    assert(sound_platform_command(buffer,SOUND_BUFFER_VOLUME,1)<0);
    assert(sound_platform_release_buffer(buffer));assert(sound_platform_release_device(device));
    {uint8_t stereo[]={0,128,255,127,0,0,0,0};
     wave.pcm=stereo;wave.pcm_bytes=8;wave.channels=2;wave.bits_per_sample=16;
     wave.block_alignment=4;wave.bytes_per_second=192000;
     device=sound_platform_device(NULL,NULL,0);assert(device);
     buffer=sound_platform_create_buffer(device,&wave);assert(buffer);
     sound_platform_master_volume(32768);
     assert(!sound_platform_command(buffer,SOUND_BUFFER_PLAY,0));
     lock_sound();sound_mix(mixed,1);unlock_sound();
     assert(fabsf(mixed[0]+32768.0f/65535)<0.00001f && fabsf(mixed[1]-32767.0f/65535)<0.00001f);
     assert(sound_platform_release_buffer(buffer));assert(sound_platform_release_device(device));
     device=sound_platform_device(NULL,NULL,1);buffer=sound_platform_create_buffer(device,&wave);assert(buffer);
     assert(!sound_platform_command(buffer,SOUND_BUFFER_PLAY,1));
     lock_sound();sound_mix(mixed,4);unlock_sound();
     for(i=0;i<8;++i)assert(mixed[i]==0);
     assert(sound_platform_release_buffer(buffer));assert(sound_platform_release_device(device));}
    sound_platform_shutdown();assert(!stats.live_buffers && !stats.live_devices && !output_stream && !sound_lock);
    assert(stats.buffers==stats.buffer_releases && stats.devices==stats.device_releases);
    return 0;
}
