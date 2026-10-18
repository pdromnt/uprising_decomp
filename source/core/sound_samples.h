#ifndef UPRISING_SOUND_SAMPLES_H
#define UPRISING_SOUND_SAMPLES_H
#include "sound_playback.h"
#include "resource_blob.h"

/* Borrowed views exist only while a sample is being constructed. Devices copy
 * PCM into their owned buffers. No file pointer or host object enters a record. */
typedef struct SoundWave {
    const uint8_t *pcm,*format_data;
    uint32_t format_bytes;
    uint32_t pcm_bytes,frequency,bytes_per_second;
    uint16_t format,channels,block_alignment,bits_per_sample;
} SoundWave;
typedef struct SoundSampleServices {
    void *context;
    GameAddress (*allocate)(void *,uint32_t);
    int (*free_record)(void *,GameAddress);
    GameAddress (*create_buffer)(void *,GameAddress device,const SoundWave *);
    GameAddress (*duplicate_buffer)(void *,GameAddress device,GameAddress buffer);
    int (*release_buffer)(void *,GameAddress);
    int (*release_device)(void *,GameAddress);
    int32_t (*frequency)(void *,GameAddress,uint32_t *);
} SoundSampleServices;
int sound_wave_read(const ResourceBlob *,SoundWave *);
void sound_bank_initialize(GameMemory *,GameAddress bank,GameAddress device);
int sound_sample_initialize(GameMemory *,GameAddress sample,GameAddress bank,
    uint32_t voices,float radius,const ResourceBlob *,const SoundSampleServices *);
int sound_sample_release(GameMemory *,GameAddress sample,const SoundSampleServices *);
int sound_bank_unload(GameMemory *,GameAddress bank,int category,const SoundSampleServices *);
#endif
