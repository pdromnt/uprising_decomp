#include "application_entry.h"
#include "game_state_ops.h"

/* Remaining static service constructors must resolve to native implementations.
 * CRT setup belongs to the host; game-owned globals and destructors do not. */
extern int game_application_static_constructor(GameApplication *, uint32_t);

typedef struct StartupPairArray {
    GameAddress base;
    uint32_t count, width;
} StartupPairArray;
static const StartupPairArray pair_arrays[] = {
#include "startup_pair_arrays.inc"
};

/* 00560ad0/00560bf0: retain constructor order, using addresses only as IDs.
 * 004602a0..00460e40 are a single connected static record family. Metadata was
 * recovered from the preserved constructors; no instruction bytes ship here. */
int game_application_initialize_globals(GameApplication *app) {
    GameMemory *m = &app->memory;
    GameAddress cursor;
    for (cursor = 0x5831bcu; cursor < 0x5831c8u; cursor += 4u) {
        uint32_t method = game_memory_read_u32(m, cursor);
        if (m->fault || (method && !game_application_static_constructor(app, method))) return 0;
    }
    for (cursor = 0x583000u; cursor < 0x5831b8u; cursor += 4u) {
        uint32_t method = game_memory_read_u32(m, cursor);
        if (method >= 0x4602a0u && method <= 0x460e40u && !(method & 31u)) {
            const StartupPairArray *spec = &pair_arrays[(method - 0x4602a0u)/32u];
            uint32_t index, stride = spec->width * 8u + 8u;
            for (index = 0; index < spec->count && !m->fault; ++index)
                game_state_reset_pair_array(m, spec->base + index * stride,
                    spec->width, spec->width * 4u);
        } else if (method && !game_application_static_constructor(app, method)) return 0;
        if (m->fault) return 0;
    }
    return 1;
}
