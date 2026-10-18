#include "level_effects.h"
#include <stdlib.h>
#include <string.h>

static LevelEffect *effects;
static size_t count, capacity;
static LevelEffectsStats stats;

static size_t find(uint32_t object) {
    size_t i;
    for (i = 0; i < count; ++i)
        if (effects[i].object == object) return i;
    return count;
}

int level_effects_begin(const LevelEffect *effect) {
    size_t slot;
    void *grown;
    if (!effect || !effect->object) return 0;
    slot = find(effect->object);
    if (slot == count) {
        if (count == capacity) {
            size_t next = capacity ? capacity * 2u : 64u;
            if (next < capacity || next > SIZE_MAX / sizeof(*effects)) return 0;
            grown = realloc(effects, next * sizeof(*effects));
            if (!grown) return 0;
            effects = grown;
            capacity = next;
        }
        ++count;
    }
    effects[slot] = *effect;
    stats.created++;
    stats.live = (uint32_t)count;
    return 1;
}

int level_effects_get(uint32_t object, LevelEffect *effect) {
    size_t slot = find(object);
    if (slot == count || !effect) return 0;
    *effect = effects[slot];
    return 1;
}

LevelEffectStep level_effects_step(uint32_t object, uint32_t tick,
    float damping, float gravity, LevelEffect *effect) {
    size_t slot = find(object);
    LevelEffect *live;
    unsigned i;
    if (slot == count || !effect) return LEVEL_EFFECT_MISSING;
    live = &effects[slot];
    if ((int32_t)(live->birth_tick + live->lifetime) <= (int32_t)tick) {
        stats.expired++;
        return LEVEL_EFFECT_EXPIRED;
    }
    for (i = 0; i < 3u; ++i)
        live->position[i] += live->velocity[i];
    if (live->flags & 2u)
        for (i = 0; i < 3u; ++i) live->velocity[i] *= damping;
    if (live->flags & 1u) live->velocity[1] -= gravity;
    *effect = *live;
    stats.updates++;
    return LEVEL_EFFECT_ALIVE;
}

void level_effects_retire(uint32_t object) {
    size_t slot = find(object);
    if (slot == count) return;
    --count;
    memmove(&effects[slot], &effects[slot + 1u],
        (count - slot) * sizeof(*effects));
    stats.retired++;
    stats.live = (uint32_t)count;
}

void level_effects_clear(void) {
    stats.discarded += (uint32_t)count;
    free(effects);
    effects = NULL;
    count = capacity = 0;
    stats.live = 0;
}

void level_effects_stats(LevelEffectsStats *output) {
    if (output) *output = stats;
}
