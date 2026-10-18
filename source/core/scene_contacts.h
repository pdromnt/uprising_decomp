#ifndef UPRISING_SCENE_CONTACTS_H
#define UPRISING_SCENE_CONTACTS_H
#include "game_memory.h"

typedef struct SceneContactQueryServices {
    void *context;
    int (*intersect)(void *,GameAddress candidate,GameAddress source,GameAddress method,float contact[3],int *);
    GameAddress (*allocate)(void *);
} SceneContactQueryServices;
/*534cd0: gather every accepted contact and separately decide rollback. */
int scene_contacts_gather(GameMemory *,GameAddress,const float previous[3],const SceneContactQueryServices *,int *);
int scene_contacts_class27_gather(GameMemory *,GameAddress,const SceneContactQueryServices *,int *); /*543f90*/

typedef struct SceneContactResponseServices {
    void *context;
    int (*settle)(void *,GameAddress,GameAddress method);
    int (*avoid)(void *,GameAddress,GameAddress other,GameAddress method,const float[3]);
    int (*damage)(void *,GameAddress,GameAddress,int32_t *);
    int (*killed)(void *,GameAddress,GameAddress other,GameAddress method);
    int (*reset)(void *,GameAddress,GameAddress method);
    int (*notice)(void *,GameAddress,GameAddress method,uint32_t);
    int (*credit)(void *,GameAddress,GameAddress attacker,GameAddress method,int32_t);
} SceneContactResponseServices;
/*53a130: ordinary obstacle/avoidance and damage/death/attacker behavior. */
int scene_contact_response(GameMemory *,GameAddress,GameAddress,const float position[3],const SceneContactResponseServices *);
#endif
