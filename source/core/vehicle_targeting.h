#ifndef UPRISING_VEHICLE_TARGETING_H
#define UPRISING_VEHICLE_TARGETING_H
#include "game_memory.h"
/*404950/407410 use the existing command, target and auxiliary owners.
 *Every callback receives ordinary arguments; address IDs select behavior. */
typedef struct VehicleTargetServices {
    void *context;
    int (*acquire)(void *,GameAddress,GameAddress,uint32_t,GameAddress *);
    int (*settle)(void *,GameAddress,GameAddress);
    int (*limit)(void *,GameAddress,GameAddress,int16_t *);
    int (*rate)(void *,GameAddress,GameAddress,int16_t *);
    int (*directions)(void *,GameAddress,GameAddress,float,float);
    int (*blocked)(void *,GameAddress,GameAddress,int *);
    int (*line)(void *,const float[3],const float[3],int *);
    int (*fire)(void *,GameAddress,GameAddress,GameAddress,int);
    int (*auxiliary)(void *,GameAddress,GameAddress,const uint16_t[3],const float[12]);
} VehicleTargetServices;
int vehicle_targeting_pitch_limit(GameAddress method,int16_t *);
int vehicle_targeting_frame(GameMemory *,GameAddress,const VehicleTargetServices *);
int vehicle_targeting_tick(GameMemory *,GameAddress,const VehicleTargetServices *);
int vehicle_targeting_transient_reset(GameMemory *,GameAddress,const VehicleTargetServices *);
int vehicle_targeting_aim(GameMemory *,GameAddress,uint16_t,int16_t,const VehicleTargetServices *);
int vehicle_targeting_idle(GameMemory *,GameAddress,const VehicleTargetServices *);
int vehicle_targeting_point_angles(GameMemory *,GameAddress,const float[3],uint16_t *,int16_t *);
int vehicle_targeting_obstructed_arc(GameMemory *,GameAddress,int *);
/*410670 chooses one of eight directions; it is not the continuous atan table. */
uint16_t vehicle_targeting_octant(GameMemory *,float x,float z);
uint16_t vehicle_targeting_vector_angle(GameMemory *,float x,float z,float scale);
#endif
