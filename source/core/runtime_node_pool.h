#ifndef UPRISING_RUNTIME_NODE_POOL_H
#define UPRISING_RUNTIME_NODE_POOL_H
#include "game_memory.h"
#include "game_state_ops.h"
typedef struct RuntimeNode {
    struct RuntimeNode *next,*previous;
    GameAddress projection,target;
    uint32_t kind,target_identity,parameter,sequence,flags;
    float position[3];
    uint8_t live,in_pool;
} RuntimeNode;
typedef struct RuntimeNodePoolStats {
    uint32_t generations,allocated,released,live,acquired,returned;
} RuntimeNodePoolStats;
/* Pool linkage is a host graph; addresses exist only at compatibility edges. */
RuntimeNode *runtime_node_pool_find(GameAddress);
int runtime_node_pool_ensure(GameMemory *);
GameAddress runtime_node_pool_acquire(GameMemory *);
int runtime_node_pool_prepare(GameMemory *,GameAddress);
int runtime_node_pool_return_chain(GameMemory *,GameAddress);
int runtime_node_pool_advance(GameMemory *,RuntimeNode *,GameStateRelinkResult *);
int runtime_node_pool_shutdown(GameMemory *);
int runtime_node_pool_projection_read(GameAddress,void *,size_t);
int runtime_node_pool_projection_write(GameAddress,const void *,size_t);
void runtime_node_pool_stats(RuntimeNodePoolStats *);
#endif
