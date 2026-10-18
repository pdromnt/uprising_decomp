#include "crt_handles.h"

int game_crt_handle_initialize(GameMemory *memory, GameAddress record) {
    if (!memory) return 0;
    game_memory_write_u8(memory, record + 4u, 0u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, record, 0xffffffffu);
    if (memory->fault) return 0;
    game_memory_write_u8(memory, record + 5u, 10u);
    return !memory->fault;
}
