#ifndef UPRISING_EFFECT_PROFILE_H
#define UPRISING_EFFECT_PROFILE_H
#include "game_memory.h"
#include "resource_blob.h"
#include "asset_bitmap.h"
typedef struct EffectProfileRequest {
    const char *name;
    int16_t frames,directions;
    int32_t duration;
    float scale;
    uint8_t flags;
} EffectProfileRequest;
typedef struct EffectProfileServices {
    void *context;
    int (*load)(void *,const char *,ResourceBlob *);
    uint32_t (*texture)(void *,const uint8_t *,uint32_t,uint32_t,uint32_t);
} EffectProfileServices;
/* The 48-byte record and its frame table have one native storage owner. Texture
 * handles borrow the shared texture store, as in the original resource lifetime. */
int effect_profile_construct(GameMemory *,GameAddress,const EffectProfileRequest *,const EffectProfileServices *);
GameAddress effect_profile_create(GameMemory *,const EffectProfileRequest *,const EffectProfileServices *);
int effect_profile_release(GameMemory *,GameAddress);
typedef struct EffectProfileSample { uint32_t texture;uint8_t flags;int mirrored; } EffectProfileSample;
int effect_profile_sample(GameMemory *,GameAddress,uint16_t,uint32_t,EffectProfileSample *,int *);
int effect_profile_apply(GameMemory *,GameAddress,GameAddress,uint16_t,uint32_t,int *);
int effect_profile_family_load(GameMemory *,unsigned,const EffectProfileServices *);
int effect_profile_family_release(GameMemory *,unsigned);
#endif
