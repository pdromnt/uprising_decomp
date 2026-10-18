#ifndef UPRISING_MESH_BUFFER_H
#define UPRISING_MESH_BUFFER_H
#include "game_memory.h"
/* Mutable triangle meshes retain the verified 64h/4ch records. Allocations
 * belong to the supplied store; addresses are handles, never native pointers. */
typedef struct MeshBufferServices {
    void *context;
    GameAddress (*allocate)(void *,uint32_t);
    int (*release)(void *,GameAddress);
} MeshBufferServices;
int mesh_buffer_construct(GameMemory *,GameAddress,int16_t,int16_t,
    GameAddress,GameAddress,const MeshBufferServices *);
int mesh_buffer_bind(GameMemory *,GameAddress,GameAddress);
int mesh_buffer_clear(GameMemory *,GameAddress);
int mesh_buffer_release(GameMemory *,GameAddress,const MeshBufferServices *);
GameAddress mesh_buffer_clone(GameMemory *,GameAddress,const MeshBufferServices *);
const MeshBufferServices *mesh_buffer_owned_services(void);
int mesh_buffer_is_native(GameMemory *,GameAddress);
#endif
