#ifndef UPRISING_PRESENTATION_COLLISION_H
#define UPRISING_PRESENTATION_COLLISION_H
#include "game_memory.h"
#include "presentation_pose.h"
int presentation_terrain_contact(GameMemory *,const PresentationPose *,float normal[3],int *hit);
int presentation_spawn_collision(GameMemory *,GameAddress,GameAddress,float contact[3],int *hit);
/* World overlap or controller overlap plus swept contact rejection. The
 * Return 2 when a contact was written, 1 otherwise, 0 on failure. The
 * caller supplies the previous position for swept queries: mode 1 uses both
 * controller poses (535b30), mode 2 world first/controller second (42e300). */
int presentation_collision(GameMemory *,GameAddress,GameAddress,int,
    const float previous[3],float contact[3],int *hit);
/* Shared skinned-record query: 2 writes contact, 1 leaves it alone, 0 faults.
 * Contact can change even when hit is false. */
int presentation_record_collision(GameMemory *,GameAddress,GameAddress,float contact[3],int *hit);
int presentation_record_controller_collision(GameMemory *,GameAddress,GameAddress,float contact[3],int *hit);
#endif
