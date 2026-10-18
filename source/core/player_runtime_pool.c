#include "player_runtime_pool.h"

#include <string.h>
#include <stdlib.h>

static PlayerRuntimePoolSlot slots[PLAYER_RUNTIME_POOL_SLOTS];

int player_runtime_pool_bind(uint16_t index, GameAddress template_object) {
    PlayerRuntimePoolSlot *slot;
    if (index >= PLAYER_RUNTIME_POOL_SLOTS || !template_object)
        return 0;
    slot = &slots[index];
    if (slot->template_object == template_object) return 1;
    if (slot->template_object) return 0;
    slot->template_object = template_object;
    return 1;
}

int player_runtime_pool_attach(uint16_t index, GameAddress clone) {
    PlayerRuntimePoolSlot *slot;
    GameAddress *grown;
    if (index >= PLAYER_RUNTIME_POOL_SLOTS || !clone) return 0;
    slot = &slots[index];
    if (!slot->template_object) return 0;
    if (slot->ready_count == slot->ready_capacity) {
        uint32_t capacity = slot->ready_capacity ?
            slot->ready_capacity * 2u : 4u;
        grown = realloc(slot->ready, (size_t)capacity * sizeof(*grown));
        if (!grown) return 0;
        slot->ready = grown;
        slot->ready_capacity = capacity;
    }
    slot->ready[slot->ready_count++] = clone;
    return 1;
}

int player_runtime_pool_has_ready(uint16_t index) {
    return index < PLAYER_RUNTIME_POOL_SLOTS && slots[index].ready_count != 0u;
}

GameAddress player_runtime_pool_take(uint16_t index) {
    PlayerRuntimePoolSlot *slot;
    GameAddress clone;
    PlayerRuntimeActive *grown;
    if (index >= PLAYER_RUNTIME_POOL_SLOTS) return 0;
    slot = &slots[index];
    if (!slot->ready_count) return 0;
    if (slot->active_count == slot->active_capacity) {
        uint32_t capacity = slot->active_capacity ?
            slot->active_capacity * 2u : 4u;
        grown = realloc(slot->active, (size_t)capacity * sizeof(*grown));
        if (!grown) return 0;
        slot->active = grown;
        slot->active_capacity = capacity;
    }
    clone = slot->ready[--slot->ready_count];
    memset(&slot->active[slot->active_count], 0,
        sizeof(slot->active[slot->active_count]));
    slot->active[slot->active_count++].clone = clone;
    return clone;
}

GameAddress player_runtime_pool_begin_shutdown(uint16_t index) {
    PlayerRuntimePoolSlot *slot;
    uint32_t i;
    GameAddress clone;
    if (index >= PLAYER_RUNTIME_POOL_SLOTS) return 0;
    slot = &slots[index];
    for (i = 0; i < slot->active_count; ++i)
        if (slot->active[i].retiring) return slot->active[i].clone;
    clone = player_runtime_pool_take(index);
    if (!clone) return 0;
    for (i = 0; i < slot->active_count; ++i)
        if (slot->active[i].clone == clone) {
            slot->active[i].retiring = 1u;
            return clone;
        }
    return 0;
}

int player_runtime_pool_has_borrowed(uint16_t index) {
    uint32_t i;
    if (index >= PLAYER_RUNTIME_POOL_SLOTS) return 0;
    for (i = 0; i < slots[index].active_count; ++i)
        if (!slots[index].active[i].retiring) return 1;
    return 0;
}

int player_runtime_pool_bind_parts(GameMemory *memory, GameAddress clone) {
    uint16_t index;
    if (!memory || !clone) return 0;
    for (index = 0u; index < PLAYER_RUNTIME_POOL_SLOTS; ++index) {
        PlayerRuntimePoolSlot *slot = &slots[index];
        uint32_t entry;
        if (slot->template_object == clone) {
            for (entry = 0u; entry < PLAYER_RUNTIME_PARTS; ++entry)
                slot->template_parts[entry] = game_memory_read_u32(memory,
                    clone + 0x1d4u + entry * 4u);
            if (memory->fault) return 0;
            slot->template_auxiliary = game_memory_read_u32(memory,
                clone + 0x210u);
            if (memory->fault) return 0;
            slot->template_parts_bound = 1u;
            return 1;
        }
        for (entry = 0u; entry < slot->active_count; ++entry) {
            PlayerRuntimeActive *active = &slot->active[entry];
            uint32_t part;
            if (active->clone != clone) continue;
            for (part = 0u; part < PLAYER_RUNTIME_PARTS; ++part)
                active->parts[part] = game_memory_read_u32(memory,
                    clone + 0x1d4u + part * 4u);
            if (memory->fault) return 0;
            active->auxiliary = game_memory_read_u32(memory,
                clone + 0x210u);
            if (memory->fault) return 0;
            active->parts_bound = 1u;
            return 1;
        }
    }
    return 1;
}

int player_runtime_pool_parts(GameAddress clone,
        GameAddress output[PLAYER_RUNTIME_PARTS]) {
    uint16_t index;
    if (!clone || !output) return 0;
    for (index = 0u; index < PLAYER_RUNTIME_POOL_SLOTS; ++index) {
        PlayerRuntimePoolSlot *slot = &slots[index];
        uint32_t entry;
        if (slot->template_object == clone && slot->template_parts_bound) {
            memcpy(output, slot->template_parts, sizeof(slot->template_parts));
            return 1;
        }
        for (entry = 0u; entry < slot->active_count; ++entry) {
            PlayerRuntimeActive *active = &slot->active[entry];
            if (active->clone != clone || !active->parts_bound) continue;
            memcpy(output, active->parts, sizeof(active->parts));
            return 1;
        }
    }
    return 0;
}

int player_runtime_pool_auxiliary(GameAddress clone, GameAddress *output) {
    uint16_t index;
    if (!clone || !output) return 0;
    for (index = 0u; index < PLAYER_RUNTIME_POOL_SLOTS; ++index) {
        PlayerRuntimePoolSlot *slot = &slots[index];
        uint32_t entry;
        if (slot->template_object == clone && slot->template_parts_bound) {
            *output = slot->template_auxiliary;
            return 1;
        }
        for (entry = 0u; entry < slot->active_count; ++entry) {
            PlayerRuntimeActive *active = &slot->active[entry];
            if (active->clone != clone || !active->parts_bound) continue;
            *output = active->auxiliary;
            return 1;
        }
    }
    return 0;
}

int player_runtime_pool_destroy_parts(GameAddress clone,
        const PlayerRuntimeDestroyServices *services) {
    uint16_t index;
    if (!clone || !services || !services->release_child ||
        !services->clear_projection || !services->clear_events ||
        !services->destroy_base) return 0;
    for (index = 0u; index < PLAYER_RUNTIME_POOL_SLOTS; ++index) {
        PlayerRuntimePoolSlot *slot = &slots[index];
        GameAddress *parts = NULL, *auxiliary = NULL;
        uint8_t *cursor = NULL, *phase = NULL;
        uint32_t entry;
        if (slot->template_object == clone && slot->template_parts_bound) {
            parts = slot->template_parts;
            auxiliary = &slot->template_auxiliary;
            cursor = &slot->template_destroy_cursor;
            phase = &slot->template_destroy_phase;
        }
        for (entry = 0u; !parts && entry < slot->active_count; ++entry) {
            PlayerRuntimeActive *active = &slot->active[entry];
            if (active->clone != clone || !active->parts_bound) continue;
            parts = active->parts;
            auxiliary = &active->auxiliary;
            cursor = &active->destroy_cursor;
            phase = &active->destroy_phase;
        }
        if (!parts) continue;
        while (*cursor < PLAYER_RUNTIME_PARTS) {
            unsigned part = *cursor;
            if (parts[part] && !services->release_child(services->context,
                    parts[part], part)) return 0;
            parts[part] = 0u;
            ++*cursor;
        }
        if (*phase == 0u) {
            if (*auxiliary && !services->release_child(services->context,
                    *auxiliary, PLAYER_RUNTIME_PARTS)) return 0;
            *auxiliary = 0u;
            *phase = 1u;
        }
        if (*phase == 1u) {
            if (!services->clear_projection(services->context, clone))
                return 0;
            *phase = 2u;
        }
        if (*phase == 2u) {
            if (!services->clear_events(services->context, clone))
                return 0;
            *phase = 3u;
        }
        if (*phase == 3u) {
            if (!services->destroy_base(services->context, clone))
                return 0;
            *phase = 4u;
        }
        return 1;
    }
    return 0;
}

int player_runtime_pool_retire(GameAddress clone) {
    uint16_t index;
    if (!clone) return 0;
    for (index = 0u; index < PLAYER_RUNTIME_POOL_SLOTS; ++index) {
        PlayerRuntimePoolSlot *slot = &slots[index];
        uint32_t entry;
        for (entry = 0u; entry < slot->active_count; ++entry) {
            if (slot->active[entry].clone == clone) {
                slot->active[entry] = slot->active[--slot->active_count];
                return 1;
            }
        }
    }
    return 0;
}

int player_runtime_pool_slot(uint16_t index, PlayerRuntimePoolSlot *output) {
    if (!output || index >= PLAYER_RUNTIME_POOL_SLOTS ||
        !slots[index].template_object) return 0;
    *output = slots[index];
    return 1;
}

void player_runtime_pool_forget(uint16_t index) {
    if (index < PLAYER_RUNTIME_POOL_SLOTS) {
        free(slots[index].ready);
        free(slots[index].active);
        memset(&slots[index], 0, sizeof(slots[index]));
    }
}
