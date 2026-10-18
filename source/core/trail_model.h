#ifndef UPRISING_TRAIL_MODEL_H
#define UPRISING_TRAIL_MODEL_H
#include "game_memory.h"

typedef struct TrailVector { float x, y, z; } TrailVector;
typedef struct TrailSpan {
    void *values;
    size_t size;
    GameAddress projection;
} TrailSpan;
/* The four spans are address views of these C arrays, never their owners. */
typedef struct TrailModel {
    GameAddress projection, methods, parent, descriptor, lighting;
    float local[12];
    uint32_t mode, packet_count, packet_flags, packet_tint;
    GameAddress packet_next;
    uint32_t packet_auxiliary[4];
    int32_t capacity, count;
    uint32_t color, end_color, color_step;
    int16_t color_duration;
    TrailVector *positions, *velocities, *transformed;
    uint8_t *active;
    TrailSpan spans[4];
} TrailModel;
TrailModel *trail_model_find(GameAddress);
GameAddress trail_model_create(uint32_t capacity);
GameAddress trail_model_clone(GameMemory *, GameAddress source);
int trail_model_release(GameAddress);
int trail_model_destruct(GameAddress, int release_object);
int trail_model_initialize(GameMemory *, TrailModel *, GameAddress recipe);
void trail_model_update(TrailModel *, uint32_t flags, uint32_t birth_tick,
    uint32_t tick, float damping, float maximum_squared_radius);
int trail_model_present(GameMemory *, TrailModel *);
#endif
