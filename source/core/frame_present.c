#include "frame_present.h"

int game_frame_present(GameMemory *memory, const GameFramePresentOps *ops,
    GameFramePresentResult *result) {
    GameAddress surface;
    GameAddress target;
    uint32_t arguments[3];
    if (!memory || !ops || !ops->present || !ops->call || !result) return 0;
    if (!ops->present(ops->context, 1u)) return 0;
    surface = game_memory_read_u32(memory, 0x00626e78u);
    if (memory->fault) return 0;
    if (surface) {
        GameAddress vtable = game_memory_read_u32(memory, surface);
        if (memory->fault) return 0;
        target = game_memory_read_u32(memory, vtable + 0x2cu);
        if (memory->fault) return 0;
        arguments[0] = surface;
        arguments[1] = 0u;
        arguments[2] = 1u;
        if (!ops->call(ops->context, target, arguments, 3u, 0)) return 0;
    }
    target = game_memory_read_u32(memory, 0x0063c948u);
    if (memory->fault || !ops->call(ops->context, target, 0, 0u,
            &result->now)) return 0;
    result->start = game_memory_read_u32(memory, 0x00626d54u);
    return !memory->fault;
}
