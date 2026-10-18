#include "trail_model.h"
#include "scene_projection_storage.h"
#include "game_matrix.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint32_t bits(float value) {
    uint32_t result; memcpy(&result, &value, 4u); return result;
}
static float from_bits(uint32_t value) {
    float result; memcpy(&result, &value, 4u); return result;
}
static float read_float(GameMemory *memory, GameAddress address) {
    return from_bits(game_memory_read_u32(memory, address));
}
static uint32_t model_word(const TrailModel *model, unsigned offset) {
    if (offset >= 4u && offset <= 0x30u)
        return bits(model->local[(offset - 4u) / 4u]);
    if (offset >= 0x5cu && offset <= 0x68u)
        return model->packet_auxiliary[(offset - 0x5cu) / 4u];
    switch (offset) {
    case 0: return model->methods;
    case 0x34: return model->descriptor;
    case 0x38: return model->lighting;
    case 0x3c: return model->mode;
    case 0x40: return model->parent;
    case 0x44: return model->packet_count;
    case 0x48: return model->spans[2].projection;
    case 0x4c: return model->packet_flags;
    case 0x50: return model->packet_next;
    case 0x54: return model->spans[3].projection;
    case 0x58: return model->packet_tint;
    case 0x6c: return (uint32_t)model->capacity;
    case 0x70: return (uint32_t)model->count;
    case 0x74: return model->spans[0].projection;
    case 0x78: return model->spans[1].projection;
    case 0x7c: return model->spans[2].projection;
    case 0x80: return model->spans[3].projection;
    case 0x84: return model->color;
    case 0x88: return model->end_color;
    case 0x8c: return model->color_step;
    default: return 0;
    }
}
static void model_set_word(TrailModel *model, unsigned offset, uint32_t value) {
    if (offset >= 4u && offset <= 0x30u) {
        model->local[(offset - 4u) / 4u] = from_bits(value); return;
    }
    if (offset >= 0x5cu && offset <= 0x68u) {
        model->packet_auxiliary[(offset - 0x5cu) / 4u] = value; return;
    }
    switch (offset) {
    case 0: model->methods = value; break;
    case 0x34: model->descriptor = value; break;
    case 0x38: model->lighting = value; break;
    case 0x3c: model->mode = value; break;
    case 0x40: model->parent = value; break;
    case 0x44: model->packet_count = value; break;
    case 0x4c: model->packet_flags = value; break;
    case 0x50: model->packet_next = value; break;
    case 0x58: model->packet_tint = value; break;
    case 0x70: model->count = (int32_t)value; break;
    case 0x84: model->color = value; break;
    case 0x88: model->end_color = value; break;
    case 0x8c: model->color_step = value; break;
    }
}
static int model_transfer(void *context, uint32_t offset, void *data,
    size_t size, int writing) {
    TrailModel *model = context;
    uint8_t *bytes = data;
    size_t i;
    if (!data || offset > 0x90u || size > 0x90u - offset) return 0;
    for (i = 0; i < size; ++i) {
        unsigned location = offset + (unsigned)i;
        unsigned word = location & ~3u, shift = (location & 3u) * 8u;
        uint32_t value = model_word(model, word);
        if (writing)
            model_set_word(model, word, (value & ~(255u << shift)) |
                ((uint32_t)bytes[i] << shift));
        else bytes[i] = (uint8_t)(value >> shift);
    }
    return 1;
}
static int model_read(void *c, uint32_t o, void *b, size_t n) {
    return model_transfer(c, o, b, n, 0);
}
static int model_write(void *c, uint32_t o, const void *b, size_t n) {
    return model_transfer(c, o, (void *)b, n, 1);
}
static int span_read(void *context, uint32_t offset, void *data, size_t size) {
    TrailSpan *span = context;
    if (offset > span->size || size > span->size - offset) return 0;
    memcpy(data, (uint8_t *)span->values + offset, size); return 1;
}
static int span_write(void *context, uint32_t offset, const void *data, size_t size) {
    TrailSpan *span = context;
    if (offset > span->size || size > span->size - offset) return 0;
    memcpy((uint8_t *)span->values + offset, data, size); return 1;
}
TrailModel *trail_model_find(GameAddress address) {
    return scene_projection_storage_state(address, model_read);
}
static void discard(TrailModel *model) {
    unsigned i;
    for (i = 0; i < 4u; ++i) {
        if (model->spans[i].projection)
            scene_projection_storage_free(model->spans[i].projection);
        free(model->spans[i].values);
    }
    if (model->projection) scene_projection_storage_free(model->projection);
    free(model);
}
GameAddress trail_model_create(uint32_t capacity) {
    TrailModel *model;
    unsigned i;
    if (capacity > UINT32_MAX / sizeof(TrailVector)) return 0;
    model = calloc(1, sizeof(*model));
    if (!model) return 0;
    model->capacity = (int32_t)capacity;
    model->methods = 0x574b10u;
    model->local[0] = model->local[5] = model->local[10] = 1.0f;
    model->packet_flags = 0x1000u;
    model->projection = scene_projection_storage_allocate(0x90u, "trail model");
    if (!model->projection) { discard(model); return 0; }
    for (i = 0; i < 4u; ++i) {
        TrailSpan *span = &model->spans[i];
        span->size = (size_t)capacity * (i < 3u ? sizeof(TrailVector) : 1u);
        span->values = calloc(1, span->size ? span->size : 1u);
        span->projection = scene_projection_storage_allocate(span->size ? span->size : 1u,
            "trail array view");
        if (!span->values || !span->projection ||
            !scene_projection_storage_bind_state(span->projection, span, span_read, span_write)) {
            discard(model); return 0;
        }
    }
    model->positions = model->spans[0].values;
    model->velocities = model->spans[1].values;
    model->transformed = model->spans[2].values;
    model->active = model->spans[3].values;
    if (!scene_projection_storage_bind_state(model->projection, model, model_read, model_write)) {
        discard(model); return 0;
    }
    return model->projection;
}
GameAddress trail_model_clone(GameMemory *memory, GameAddress source) {
    uint32_t capacity = game_memory_read_u32(memory, source + 0x6cu);
    return memory->fault ? 0u : trail_model_create(capacity);
}
int trail_model_release(GameAddress address) {
    TrailModel *model = trail_model_find(address);
    if (!model) return 0;
    discard(model); return 1;
}
int trail_model_destruct(GameAddress address, int release_object) {
    TrailModel *model = trail_model_find(address);
    unsigned i;
    if (!model) return 0;
    if (release_object) { discard(model); return 1; }
    for (i = 0; i < 4u; ++i) {
        if (model->spans[i].projection) scene_projection_storage_free(model->spans[i].projection);
        free(model->spans[i].values);
        memset(&model->spans[i], 0, sizeof(model->spans[i]));
    }
    model->positions = model->velocities = model->transformed = NULL;
    model->active = NULL;
    model->methods = 0x571fa0u;
    return 1;
}
static void transform_vector(TrailVector *output, const float matrix[12], const TrailVector *input) {
    float source[3] = {input->x, input->y, input->z}, result[3];
    game_matrix_transform_point_values(result, matrix, source);
    output->x = result[0]; output->y = result[1]; output->z = result[2];
}
static unsigned angle_index(long double value) {
    int64_t integer = !isfinite(value) || value < -0x1p63L || value >= 0x1p63L ?
        INT64_MIN : (int64_t)value;
    return (unsigned)((uint64_t)integer & 2047u);
}
int trail_model_initialize(GameMemory *memory, TrailModel *model, GameAddress recipe) {
    float ranges[8], angle_scale;
    int32_t requested, delta;
    unsigned i;
    model->color = (uint32_t)game_memory_read_u8(memory, recipe + 0x2au) << 16;
    model->end_color = (uint32_t)game_memory_read_u8(memory, recipe + 0x2bu) << 16;
    model->color_duration = (int16_t)game_memory_read_u16(memory, recipe + 0x2cu);
    delta = (int32_t)(model->end_color - model->color);
    if (memory->fault) return 0;
    if (!model->color_duration) { memory->fault = 1; return 0; }
    model->color_step = (uint32_t)(delta / model->color_duration);
    requested = (int32_t)game_memory_read_u32(memory, recipe + 0x24u);
    model->count = requested < model->capacity ? requested : model->capacity;
    model->packet_count = (uint32_t)model->count;
    if (model->count <= 0) return !memory->fault;
    for (i = 0; i < 8u; ++i) ranges[i] = read_float(memory, recipe + 4u + i * 4u);
    angle_scale = read_float(memory, 0x574b04u);
    for (i = 0; i < (unsigned)model->count && !memory->fault; ++i) {
        uint32_t cursor = game_memory_read_u32(memory, 0x605864u);
        float random[4], rotation[12], yaw[12] = {0}, pitch[12] = {0};
        float point[3], velocity[3];
        unsigned a, b, j;
        for (j = 0; j < 4u; ++j) random[j] = read_float(memory, 0x6308c8u + (cursor + j) * 4u);
        a = angle_index((((long double)ranges[5] - ranges[4]) * random[0] + ranges[4]) * angle_scale);
        b = angle_index((((long double)ranges[7] - ranges[6]) * random[1] + ranges[6]) * angle_scale);
        yaw[0] = yaw[10] = read_float(memory, 0x6298a8u + ((a + 512u) & 2047u) * 4u);
        yaw[2] = read_float(memory, 0x6298a8u + a * 4u);
        yaw[8] = -yaw[2]; yaw[5] = 1.0f;
        pitch[0] = 1.0f;
        pitch[5] = pitch[10] = read_float(memory, 0x6298a8u + ((b + 512u) & 2047u) * 4u);
        pitch[9] = read_float(memory, 0x6298a8u + b * 4u);
        pitch[6] = -pitch[9];
        game_matrix_compose_values(rotation, yaw, pitch);
        point[0] = point[1] = velocity[0] = velocity[1] = 0.0f;
        point[2] = (float)(((long double)ranges[3] - ranges[2]) * random[2] + ranges[2]);
        velocity[2] = (float)(((long double)ranges[1] - ranges[0]) * random[3] + ranges[0]);
        {
            TrailVector p={point[0],point[1],point[2]},v={velocity[0],velocity[1],velocity[2]};
            transform_vector(&model->positions[i], rotation, &p);
            transform_vector(&model->velocities[i], rotation, &v);
        }
        model->active[i] = 255u;
        ++cursor;
        game_memory_write_u32(memory, 0x605864u, (int32_t)cursor > 1020 ? 0u : cursor);
    }
    return !memory->fault;
}
void trail_model_update(TrailModel *model, uint32_t flags, uint32_t birth_tick,
    uint32_t tick, float damping, float maximum_squared_radius) {
    int32_t i;
    for (i = 0; i < model->count; ++i) {
        TrailVector *p = &model->positions[i], *v = &model->velocities[i];
        long double squared;
        if (!model->active[i]) continue;
        p->x = (float)((long double)v->x + p->x);
        p->y = (float)((long double)v->y + p->y);
        p->z = (float)((long double)v->z + p->z);
        squared = ((long double)p->z * p->z + (long double)p->y * p->y) + (long double)p->x * p->x;
        if ((!isnan(squared) && !isnan(maximum_squared_radius) &&
                !(squared < maximum_squared_radius)) || bits((float)squared) > 0x80000000u) {
            model->active[i] = 0; continue;
        }
        if (flags & 2u) {
            v->x = (float)((long double)v->x * damping);
            v->y = (float)((long double)v->y * damping);
            v->z = (float)((long double)v->z * damping);
        }
    }
    /* Signed comparison follows the wrapped original deadline. */
    if ((int32_t)(birth_tick + (uint32_t)(int32_t)model->color_duration) > (int32_t)tick)
        model->color += model->color_step;
}
int trail_model_present(GameMemory *memory, TrailModel *model) {
    float view[12], composed[12];
    unsigned i;
    model->packet_tint = (model->color & 0xff0000u) >> 16;
    for (i = 0; i < 12u; ++i) view[i] = read_float(memory, 0x5cedf0u + i * 4u);
    if (memory->fault) return 0;
    game_matrix_compose_values(composed, view, model->local);
    for (i = 0; (int32_t)i < model->count; ++i)
        if (model->active[i]) transform_vector(&model->transformed[i],composed,&model->positions[i]);
    model->packet_next = game_memory_read_u32(memory, 0x604484u);
    game_memory_write_u32(memory, 0x604484u, model->projection + 0x44u);
    return !memory->fault;
}
