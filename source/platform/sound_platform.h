#ifndef UPRISING_SOUND_PLATFORM_H
#define UPRISING_SOUND_PLATFORM_H
#include "sound_samples.h"
/* Platform objects remain private pointers. These identities select native
 * device behavior; they are never vtable addresses or guest call targets. */
GameAddress sound_platform_device(int muted);
int sound_platform_owns(GameAddress);
static inline int sound_platform_identity(GameAddress id){return (id&0xff000000u)==0xf9000000u;}
GameAddress sound_platform_create_buffer(GameAddress,const SoundWave *);
GameAddress sound_platform_duplicate(GameAddress,GameAddress);
int sound_platform_release_buffer(GameAddress);
int sound_platform_release_device(GameAddress);
void sound_platform_master_volume(uint32_t);
uint32_t sound_platform_master_volume_read(void);
int32_t sound_platform_status(GameAddress,uint32_t *);
int32_t sound_platform_position(GameAddress,uint32_t *);
int32_t sound_platform_format(GameAddress,uint32_t *,uint16_t *);
int32_t sound_platform_frequency(GameAddress,uint32_t *);
int32_t sound_platform_command(GameAddress,SoundBufferCommand,uint32_t);
void sound_platform_shutdown(void);
typedef struct SoundPlatformStats {uint32_t devices,device_releases,buffers,buffer_releases,live_devices,live_buffers;} SoundPlatformStats;
void sound_platform_stats(SoundPlatformStats *);
#endif
