#ifndef UPRISING_CARRIER_EFFECT_H
#define UPRISING_CARRIER_EFFECT_H
#include "game_memory.h"
#include "sound_playback.h"
/*0x2a carrier: the existing pose owns0..b3; its allocation owns b4..107.
 *Callbacks are ordinary services, never register/stack dispatch. */
typedef struct CarrierServices {
    void *context;
    int (*place)(void *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
    int (*reset)(void *,GameAddress);
    int (*play)(void *,const SoundPlaybackRequest *);
    int (*stop)(void *,int16_t,uint32_t);
    int (*playing)(void *,int16_t,uint32_t,int *);
    int (*frequency)(void *,int16_t,uint32_t,float);
    int (*distance_volume)(void *,int16_t,uint32_t,float);
} CarrierServices;
int carrier_effect_place(GameMemory *,GameAddress,uint32_t,const float[3],const uint16_t[3],const CarrierServices *);
int carrier_effect_update(GameMemory *,GameAddress,const CarrierServices *);
int carrier_effect_reset(GameMemory *,GameAddress,const CarrierServices *);
#endif
