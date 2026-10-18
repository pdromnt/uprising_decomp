#ifndef UPRISING_PRESENTATION_LIFECYCLE_H
#define UPRISING_PRESENTATION_LIFECYCLE_H
#include "game_memory.h"
#include "presentation_pose.h"
typedef struct PresentationMembership {
    GameAddress previous,next,spatial_previous,spatial_next,source;
    uint32_t selector;
} PresentationMembership;
typedef struct PresentationRuntime {
    GameAddress queued_handle,packet_chain,peers[2];
    uint32_t status,mode,random_phase,activation,pending[2];
    uint32_t counters[9],properties[5];
    float fractions[2];
    float command_position[3];
    uint32_t command_identity,secondary_identity;
} PresentationRuntime;
typedef struct PresentationLifecycle {
    PresentationMembership membership;
    PresentationRuntime runtime;
    uint8_t has_runtime,reset_phase,reset_kind,resetting,detached_peers;
    uint8_t initialize_phase,property_index,refreshing;
} PresentationLifecycle;
int presentation_lifecycle_import(GameMemory *,GameAddress,PresentationLifecycle *,int runtime);
int presentation_lifecycle_projection_read(PresentationLifecycle *,uint32_t,void *,size_t);
int presentation_lifecycle_projection_write(PresentationLifecycle *,uint32_t,const void *,size_t);
typedef struct PresentationResetServices {
    void *context;
    int (*deactivate)(void *,GameAddress object);
    int (*detach_peer)(void *,GameAddress peer,GameAddress object);
    int (*detach_chain)(void *,GameAddress chain);
    int (*release_queue)(void *,GameAddress handle);
    int (*notify_catalog)(void *,uint32_t identity,uint8_t type);
} PresentationResetServices;
/* kind 0 base reset; 1 includes queued-handle retirement; 2 includes runtime
 * deactivation and packet cleanup. Successful steps survive a callback fault. */
int presentation_lifecycle_reset(GameMemory *,GameAddress,PresentationPose *,
    PresentationLifecycle *,unsigned kind,const PresentationResetServices *);
void presentation_lifecycle_initialize_runtime(PresentationLifecycle *,uint32_t random_phase);
typedef struct PresentationInitializeServices {
    void *context;
    int (*allocate_packet)(void *,GameAddress *packet);
    int (*prepare_packet)(void *,GameAddress packet);
    int (*next_phase)(void *,uint32_t *phase);
    int (*refresh_properties)(void *,GameAddress object);
} PresentationInitializeServices;
typedef int (*PresentationPropertyGetter)(void *,unsigned index,uint32_t *bits);
int presentation_lifecycle_initialize(GameMemory *,GameAddress,PresentationLifecycle *,
    const PresentationInitializeServices *);
int presentation_lifecycle_refresh_properties(GameMemory *,GameAddress,PresentationLifecycle *,
    void *,PresentationPropertyGetter);
uint32_t presentation_lifecycle_phase(float sample,float scale);
#endif
