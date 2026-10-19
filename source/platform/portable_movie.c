

#include "menu_movie.h"
#include "sound_platform.h"
#include "../third_party/libsmacker/smacker.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
extern int game_application_audio_muted(GameApplication *);
extern uint32_t game_menu_milliseconds(GameApplication *);

typedef struct MoviePcm {uint8_t *bytes;size_t size,capacity;} MoviePcm;
struct GameMovieDecoder {
    GameApplication *app;smk codec;AssetBitmap image;
    uint8_t palette[1024],audio_mask;
    uint8_t *scaled_pixels;unsigned long raw_height;
    GameAddress audio[7];
    unsigned long frame_count,next_frame;double microseconds;
    uint32_t started_at,volume;int started,failed,palette_valid;
};
static int pcm_append(MoviePcm *pcm,const uint8_t *bytes,size_t size){
    size_t needed,capacity;uint8_t *next;
    if(!size)return 1;
    if(!bytes || size>0x0fffffffu-pcm->size)return 0;
    needed=pcm->size+size;
    if(needed>pcm->capacity){
        capacity=pcm->capacity?pcm->capacity:65536;
        while(capacity<needed){if(capacity>0x0fffffffu/2){capacity=needed;break;}capacity*=2;}
        next=realloc(pcm->bytes,capacity);if(!next)return 0;
        pcm->bytes=next;pcm->capacity=capacity;
    }
    memcpy(pcm->bytes+pcm->size,bytes,size);pcm->size=needed;return 1;
}
/* Decode PCM once before playback and place it in the existing sound owner.
 * This avoids per-frame buffer restarts/stream gaps. Temporary PCM is released
 * after the device copies it; video keeps only libsmacker's one decoded frame. */
static int movie_audio(GameMovieDecoder *d){
    MoviePcm pcm[7]={{0}};unsigned char channels[7],depth[7];unsigned long rate[7],frame;
    GameAddress bank,device;unsigned i;int result=0,status;
    if(smk_info_audio(d->codec,&d->audio_mask,channels,depth,rate))goto done;
    if(!d->audio_mask){result=1;goto done;}
    if(smk_enable_all(d->codec,d->audio_mask))goto done;
    status=smk_first(d->codec);
    for(frame=0;frame<d->frame_count;++frame){
        if(status!=SMK_MORE && status!=SMK_LAST)goto done;
        for(i=0;i<7;++i)if((d->audio_mask&(1u<<i)) &&
            !pcm_append(pcm+i,smk_get_audio(d->codec,(unsigned char)i),smk_get_audio_size(d->codec,(unsigned char)i)))goto done;
        if(frame+1<d->frame_count)status=smk_next(d->codec);
    }
    /* Muted devices still own real PCM/cursors; the mixer guarantees silence. */
    {
        bank=game_memory_read_u32(&d->app->memory,0x628a90);
        device=bank?game_memory_read_u32(&d->app->memory,bank+0x45c):0;
        if(!device || d->app->memory.fault)goto done;
        for(i=0;i<7;++i)if(pcm[i].size){
            SoundWave wave={0};
            if((channels[i]!=1 && channels[i]!=2) || (depth[i]!=8 && depth[i]!=16) ||
               !rate[i] || rate[i]>UINT32_MAX/(channels[i]*(depth[i]/8)))goto done;
            wave.format=1;wave.channels=channels[i];wave.bits_per_sample=depth[i];
            wave.frequency=(uint32_t)rate[i];wave.block_alignment=(uint16_t)(channels[i]*(depth[i]/8));
            wave.bytes_per_second=wave.frequency*wave.block_alignment;
            wave.pcm=pcm[i].bytes;wave.pcm_bytes=(uint32_t)pcm[i].size;
            if(wave.pcm_bytes%wave.block_alignment)goto done;
            d->audio[i]=sound_platform_create_buffer(device,&wave);if(!d->audio[i])goto done;
        }
    }
    result=1;
done:for(i=0;i<7;++i)free(pcm[i].bytes);return result;
}
GameMovieDecoder *game_movie_decoder_open(GameApplication *a,const char *name){
    ResourceBlob encoded={0};GameMovieDecoder *d=NULL;unsigned long width,height;unsigned char scale;unsigned i;
    if(!resource_blob_open_file(name,&encoded) || encoded.size<104)goto fail;
    /* The supported movies use Smacker DPCM. Reject unsupported perceptual
     * tracks explicitly, rather than presenting a silent successful movie. */
    for(i=0;i<7;++i)if(encoded.data[72+i*4+3]&12u)goto fail;
    d=calloc(1,sizeof(*d));if(!d)goto fail;d->app=a;d->volume=32768;
    d->codec=smk_open_memory(encoded.data,(unsigned long)encoded.size);
    resource_blob_release(&encoded);
    if(!d->codec || smk_info_video(d->codec,&width,&height,&scale) ||
       smk_info_all(d->codec,NULL,&d->frame_count,&d->microseconds) || !d->frame_count ||
       !width || !height || width>INT32_MAX || height>INT32_MAX || height>SIZE_MAX/width ||
       !isfinite(d->microseconds) || d->microseconds<=0)goto fail;
    d->raw_height=height;
    if(scale==SMK_FLAG_Y_DOUBLE){
        if(height>INT32_MAX/2 || height>SIZE_MAX/width/2)goto fail;
        height*=2;d->scaled_pixels=calloc((size_t)width,height);if(!d->scaled_pixels)goto fail;
    }else if(scale!=SMK_FLAG_Y_NONE)goto fail;
    d->image.width=(uint32_t)width;d->image.height=(uint32_t)height;d->image.stride=(uint32_t)width;
    d->image.pixel_bytes=(size_t)width*height;d->image.top_down=1;
    d->image.pixels=d->scaled_pixels?d->scaled_pixels:smk_get_video(d->codec);
    d->image.palette_bgra=d->palette;d->image.palette_count=256;
    if(!movie_audio(d) || smk_enable_all(d->codec,SMK_VIDEO_TRACK))goto fail;
    return d;
fail:resource_blob_release(&encoded);game_movie_decoder_close(d);return NULL;
}
void game_movie_decoder_close(GameMovieDecoder *d){
    unsigned i;if(!d)return;
    for(i=0;i<7;++i)if(d->audio[i]){
        (void)sound_platform_command(d->audio[i],SOUND_BUFFER_STOP,0);
        (void)sound_platform_release_buffer(d->audio[i]);
    }
    if(d->codec)smk_close(d->codec);
    free(d->scaled_pixels);
    free(d);
}
const AssetBitmap *game_movie_decoder_frame(GameMovieDecoder *d){return d?&d->image:NULL;}
int game_movie_decoder_ready(GameMovieDecoder *d){
    if(!d)return 0;
    return d->failed || !d->started ||
        (double)(uint32_t)(game_menu_milliseconds(d->app)-d->started_at)*1000.0>=d->next_frame*d->microseconds;
}
int game_movie_decoder_decode(GameMovieDecoder *d,int *last,int *changed){
    int status;unsigned i;uint8_t palette[1024];const unsigned char *rgb;
    if(!d || !last || !changed || d->failed || d->next_frame>=d->frame_count)return 0;
    status=d->next_frame?smk_next(d->codec):smk_first(d->codec);
    if(status!=SMK_MORE && status!=SMK_LAST){d->failed=1;return 0;}
    /*Smacker32 exposes double-Y movies at twice their encoded height. With
     *the original ToBuffer flags0 it writes only even rows. Keep untouched
     *rows initialized instead of depending on previous heap contents. Ordinary
     *frames borrow decoder pixels without a second frame copy. */
    if(d->scaled_pixels){
        const uint8_t *raw=smk_get_video(d->codec);size_t width=d->image.width;
        for(i=0;i<d->raw_height;++i){
            memcpy(d->scaled_pixels+(size_t)i*width*2,raw+(size_t)i*width,width);
        }
    }
    rgb=smk_get_palette(d->codec);if(!rgb)return 0;
    for(i=0;i<256;++i){palette[i*4]=rgb[i*3+2];palette[i*4+1]=rgb[i*3+1];palette[i*4+2]=rgb[i*3];palette[i*4+3]=0;}
    *changed=!d->palette_valid || memcmp(d->palette,palette,sizeof(palette))!=0;
    memcpy(d->palette,palette,sizeof(palette));d->palette_valid=1;
    if(!d->started){
        d->started=1;d->started_at=game_menu_milliseconds(d->app);
        for(i=0;i<7;++i)if(d->audio[i] &&
            sound_platform_command(d->audio[i],SOUND_BUFFER_PLAY,0)){d->failed=1;return 0;}
    }
    ++d->next_frame;*last=d->next_frame==d->frame_count;return 1;
}
void game_movie_decoder_volume(GameMovieDecoder *d,uint32_t value){
    unsigned i;int32_t volume;if(!d)return;
    d->volume=value>32768?32768:value;
    volume=!d->volume || game_application_audio_muted(d->app)?-10000:
        (int32_t)(2000.0*log10((double)d->volume/32768.0));
    if(volume< -10000)volume=-10000;
    for(i=0;i<7;++i)if(d->audio[i] && sound_platform_command(d->audio[i],SOUND_BUFFER_VOLUME,(uint32_t)volume))d->failed=1;
}
/* Release data is beside the executable; no drive-letter or CD-label probing. */
int game_menu_movie_path(GameApplication *a,const char *name,char *output,size_t capacity){
    ResourceFile *file;int size;(void)a;
    if(!name || !output || !capacity)return 0;
    size=snprintf(output,capacity,"VIDEO/%s",name);if(size<0 || (size_t)size>=capacity)return 0;
    file=resource_file_open(output);if(!file)return 0;
    resource_file_close(file);return 1;
}