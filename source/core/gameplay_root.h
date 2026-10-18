#ifndef UPRISING_GAMEPLAY_ROOT_H
#define UPRISING_GAMEPLAY_ROOT_H
#include "game_memory.h"
/*47a980. Services are distinct native programs with real arguments; method
 * IDs select ordinary behavior. There is no register/stack callback adapter. */
typedef struct GameplayRootServices {
    void *context;
    int (*idle)(void *,GameAddress);
    int (*camera)(void *,GameAddress,GameAddress);
    int (*sound_update)(void *);
    int (*notice)(void *,GameAddress);
    int (*network_update)(void *,GameAddress);
    int (*group_value)(void *,GameAddress,uint32_t *);
    int (*credit_group)(void *,GameAddress,GameAddress,uint32_t);
    int (*spawn)(void *,GameAddress,uint32_t,uint32_t,uint32_t,uint32_t,int *);
    int (*retire_event)(void *,GameAddress,GameAddress);
    int (*update)(void *,GameAddress,GameAddress);
    int (*next)(void *,GameAddress,GameAddress,GameAddress *);
    int (*finish_group)(void *,GameAddress);
} GameplayRootServices;
int gameplay_root_update(GameMemory *,GameAddress,const GameplayRootServices *);
int gameplay_root_choose_pickup(GameMemory *,GameAddress,int check_inventory,
    uint32_t *variant,uint32_t *payload,int *selected);
/*481430: retained-target decisions, region scan and seven-entry priorities. */
typedef int (*GameplayRootMaximum)(void *,GameAddress,uint32_t *);
int gameplay_root_choose_action(GameMemory *,GameAddress,void *,GameplayRootMaximum);
typedef struct GameplayControllerActions {
    void *context;
    int (*player_service)(void *,GameAddress,unsigned);
    int (*history_order)(void *,GameAddress,unsigned,GameAddress,const float[3]);
    int (*repair)(void *,GameAddress,uint8_t,uint8_t);
    int (*produce)(void *,GameAddress,uint8_t,uint32_t,uint32_t);
    GameplayRootMaximum maximum;
    int (*sound)(void *,uint32_t);
    int (*message)(void *,GameAddress);
} GameplayControllerActions;
int gameplay_root_controller_action(GameMemory *,GameAddress,const uint16_t[4],const GameplayControllerActions *);
int gameplay_root_history_toggle(GameMemory *,GameAddress,const GameplayControllerActions *);
typedef struct GameplayRootSpawnServices {
    void *context;
    GameAddress (*acquire)(void *);
    int (*initialize)(void *,GameAddress,uint32_t,const float[3],uint32_t,uint32_t,uint32_t);
    int (*publish)(void *,GameAddress,GameAddress);
    int (*recycle)(void *,GameAddress);
    int (*sound)(void *);
} GameplayRootSpawnServices;
int gameplay_root_random_spawn(GameMemory *,GameAddress,uint32_t mode,uint32_t angle_width,
    uint32_t minimum_radius,uint32_t maximum_radius,uint32_t variant,uint32_t payload,
    const GameplayRootSpawnServices *,GameAddress *result);
typedef struct GameplayGroundProducerServices {
    void *context;
    int (*base_update)(void *,GameAddress);
    int (*controller_refresh)(void *,GameAddress);
    int (*worker)(void *,GameAddress,GameAddress,int32_t *);
    GameAddress (*acquire)(void *,GameAddress);
    int (*clear)(void *,GameAddress,const float[3],int *);
    int (*deploy)(void *,GameAddress,GameAddress,const float[3]);
    int (*recycle)(void *,GameAddress);
    int (*aircraft_deploy)(void *,GameAddress,GameAddress,const float[3],const float[2]);
} GameplayGroundProducerServices;
/*533730 and the60/62/63 clock gates use the existing presentation record.
 *No shadow copy: a0 is last accepted production, a4 is the produced count. */
int gameplay_ground_producer_update(GameMemory *,GameAddress,GameAddress method,
    const GameplayGroundProducerServices *);
int gameplay_ground_aircraft_update(GameMemory *,GameAddress,const GameplayGroundProducerServices *);
#endif
