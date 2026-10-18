#include "resource_refresh.h"

int game_resource_refresh(GameMemory *memory,
    const GameResourceRefreshOps *ops) {
    uint32_t manager;
    uint32_t current;
    uint32_t resource;
    if (!memory || !ops || !ops->prepare || !ops->release || !ops->load)
        return 0;
    manager = game_memory_read_u32(memory, 0x0059d4c4u);
    if (memory->fault) return 0;
    current = game_memory_read_u32(memory, manager + 0x170u);
    if (memory->fault || !ops->prepare(ops->context, manager, current))
        return 0;
    if (current && !ops->release(ops->context)) return 0;
    game_memory_write_u32(memory, 0x0059811cu, 0u);
    if (memory->fault || !ops->load(ops->context, &resource))
        return 0;
    game_memory_write_u32(memory, 0x00598120u, resource);
    return !memory->fault;
}
