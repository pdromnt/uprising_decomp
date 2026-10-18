#ifndef UPRISING_VEHICLE_DEATH_H
#define UPRISING_VEHICLE_DEATH_H
#include "game_memory.h"
#include "attachment_effect.h"
typedef struct VehicleDeathServices {
    void *context;
    int (*begin)(void *,GameAddress object,GameAddress other,GameAddress table);
    int (*sound)(void *,const float[3],int16_t);
    int (*mesh)(void *,GameAddress object,GameAddress method,GameAddress header,const float[12],const float[3]);
    int (*variant)(void *,GameAddress object,GameAddress catalog,uint32_t *);
    int (*anchors)(void *,GameAddress object,GameAddress method,int32_t,GameAddress,const float[3]);
    GameAddress (*acquire)(void *,uint16_t);
    int (*emit)(void *,GameAddress catalog,GameAddress child,AttachmentEmission *);
    int (*discard)(void *,GameAddress child);
    int (*terrain)(void *,float,float,uint32_t);
    int (*group_remove)(void *,GameAddress,uint32_t);
    int (*stop_voice)(void *,uint32_t voice);
} VehicleDeathServices;
int vehicle_presentation_death(GameMemory *,GameAddress,GameAddress,const VehicleDeathServices *);
#endif
