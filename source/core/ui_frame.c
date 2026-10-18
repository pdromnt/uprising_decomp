#include "ui_frame.h"

int game_ui_frame_run_owned(GameUiFrameKind kind, uint32_t image,
    const GameUiFrameOwnedOps *ops, GameUiFrameOwnedResult *result) {
    if (!ops || !result || !ops->screen_mask || !ops->present ||
        !ops->palette || !ops->fullscreen || kind > GAME_UI_FRAME_MARKER_IMAGE)
        return 0;
    result->reset_markers = 0;
    if (!ops->screen_mask(ops->context, 0) ||
        !ops->present(ops->context, 0u) ||
        !ops->screen_mask(ops->context, 0)) return 0;
    if (kind == GAME_UI_FRAME_CLEAR) return 1;
    if ((kind == GAME_UI_FRAME_PALETTE_TRANSITION ||
         kind == GAME_UI_FRAME_WORK_IMAGE) &&
        !ops->palette(ops->context, GAME_UI_PALETTE_WORK)) return 0;
    if (!ops->fullscreen(ops->context, image, 640u) ||
        !ops->present(ops->context, 0u) ||
        !ops->fullscreen(ops->context, image, 640u) ||
        !ops->palette(ops->context, GAME_UI_PALETTE_WORLD)) return 0;
    result->reset_markers = kind == GAME_UI_FRAME_MARKER_IMAGE;
    return 1;
}

static int clear_present_clear(const GameUiFrameOps *ops) {
    return ops->screen_mask(ops->context, 0) &&
        ops->present(ops->context, 0u) &&
        ops->screen_mask(ops->context, 0);
}

static int double_fullscreen(const GameUiFrameOps *ops, GameAddress source,
    int work_palette) {
    if (work_palette && !ops->palette(ops->context, 0x00605460u)) return 0;
    return ops->fullscreen(ops->context, source, 640u) &&
        ops->present(ops->context, 0u) &&
        ops->fullscreen(ops->context, source, 640u) &&
        ops->palette(ops->context, 0x005ad3a8u);
}

int game_ui_frame_run(GameMemory *memory, GameUiFrameKind kind,
    const GameUiFrameOps *ops) {
    GameAddress source;
    if (!ops || !ops->screen_mask || !ops->present || !ops->palette ||
        !ops->fullscreen)
        return 0;
    if (kind == GAME_UI_FRAME_CLEAR) return clear_present_clear(ops);
    if (kind == GAME_UI_FRAME_PALETTE_TRANSITION) {
        source = game_memory_read_u32(memory, 0x005960e8u);
        if (memory->fault) return 0;
        return ops->screen_mask(ops->context, 0) &&
            ops->present(ops->context, 0u) &&
            ops->screen_mask(ops->context, 0) &&
            ops->palette(ops->context, 0x00605460u) &&
            ops->fullscreen(ops->context, source, 640u) &&
            ops->present(ops->context, 0u) &&
            ops->fullscreen(ops->context, source, 640u) &&
            ops->palette(ops->context, 0x005ad3a8u);
    }
    if (!clear_present_clear(ops)) return 0;
    source = game_memory_read_u32(memory, kind == GAME_UI_FRAME_WORK_IMAGE ?
        0x005960ecu : 0x005960f0u);
    if (memory->fault || !double_fullscreen(ops, source,
            kind == GAME_UI_FRAME_WORK_IMAGE))
        return 0;
    if (kind == GAME_UI_FRAME_MARKER_IMAGE) {
        game_memory_write_u32(memory, 0x00605448u, 100u);
        game_memory_write_u32(memory, 0x0060544cu, 100u);
        game_memory_write_u32(memory, 0x00605450u, 100u);
        game_memory_write_u32(memory, 0x00605454u, 100u);
        game_memory_write_u32(memory, 0x0060545cu, 0u);
    }
    return !memory->fault;
}

int game_ui_release_resources(GameMemory *memory, void *context,
    int (*release)(void *, GameAddress)) {
    static const GameAddress globals[6] = {
        0x00605344u, 0x00596108u, 0x0059610cu,
        0x00596110u, 0x00596114u, 0x00596118u
    };
    unsigned i;
    if (!release) return 0;
    for (i = 0u; i < 6u; ++i) {
        GameAddress resource = game_memory_read_u32(memory, globals[i]);
        if (memory->fault) return 0;
        if (resource != 0u) {
            if (!release(context, resource)) return 0;
            game_memory_write_u32(memory, globals[i], 0u);
            if (memory->fault) return 0;
        }
    }
    return 1;
}
