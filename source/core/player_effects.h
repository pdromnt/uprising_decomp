#ifndef UPRISING_PLAYER_EFFECTS_H
#define UPRISING_PLAYER_EFFECTS_H
#include "attachment_effect.h"
typedef struct PlayerDeathEffectServices {
    void *context;
    int (*begin)(void *,GameAddress,GameAddress,GameAddress,GameAddress);
    int (*variant)(void *,GameAddress,uint8_t,uint32_t *);
    int (*height)(void *,float,float,long double *,long double *);
    GameAddress (*acquire)(void *,uint16_t);
    int (*emit)(void *,GameAddress,GameAddress,AttachmentEmission *);
    int (*anchors)(void *,GameAddress,GameAddress,int32_t,GameAddress,const float[3]);
    int (*discard)(void *,GameAddress);
} PlayerDeathEffectServices;
/*4f9080: four transformed terrain samples, adjacent triangle and raw normal-Y
 *clamp. Numeric locals are ordinary C; no instruction/register state is kept.*/
int player_effect_clearance(GameMemory *,GameAddress,const PlayerDeathEffectServices *,int *);
/*4f6f90: opening feedback/fragments, primary and both attachment consumers.*/
int player_death_effect(GameMemory *,GameAddress,GameAddress,const PlayerDeathEffectServices *);
#endif
