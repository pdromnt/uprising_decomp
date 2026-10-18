#include "game_screen_circle.h"

#include <stdint.h>
#include <string.h>

static float screen_circle_bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float screen_circle_i32_to_float(int32_t value) {
    volatile float rounded = (float)value;
    return rounded;
}

static int32_t screen_circle_offset(GameMemory *memory, unsigned angle,
    int32_t radius) {
    float sample = screen_circle_bits_float(game_memory_read_u32(memory,
        0x006298a8u + (angle & 0x7ffu) * 4u));
    volatile float rounded_radius = (float)radius;
    long double product = (long double)sample * (long double)rounded_radius;
    return (int32_t)product;
}

int game_screen_circle(GameMemory *memory, GameAddress stack,
    int32_t center_x, int32_t center_y, int32_t radius, uint8_t color_index,
    GameScreenCircleBegin begin, GameScreenCircleSegment segment,
    void *context) {
    GameAddress local_base = stack - 0xa4u;
    GameAddress color = 0x005ad3a8u + (uint32_t)color_index * 4u;
    GameAddress coordinates[4] = {
        local_base + 0x20u, local_base + 0x24u,
        local_base + 0x18u, local_base + 0x1cu
    };
    RuntimeRenderVertex vertices[2];
    int32_t start_x;
    int32_t previous_x;
    int32_t previous_y;
    unsigned angle;
    uint32_t offset;

    if (memory->fault) return 0;
    for (offset = 0; offset < sizeof(vertices); offset += 4u) {
        uint32_t bits = game_memory_read_u32(memory,
            stack - 0x78u + offset);
        if (memory->fault) return 0;
        memcpy((uint8_t *)vertices + offset, &bits, sizeof(bits));
    }
    vertices[0].r = vertices[1].r = screen_circle_i32_to_float(
        (int32_t)game_memory_read_u8(memory, color + 2u));
    vertices[0].g = vertices[1].g = screen_circle_i32_to_float(
        (int32_t)game_memory_read_u8(memory, color + 1u));
    vertices[0].b = vertices[1].b = screen_circle_i32_to_float(
        (int32_t)game_memory_read_u8(memory, color));
    vertices[0].oow = vertices[1].oow = 0.5f;
    start_x = (int32_t)((uint32_t)center_x + (uint32_t)radius);
    previous_x = start_x;
    previous_y = center_y;
    if (memory->fault || !begin(context)) return 0;

    for (angle = 64u; angle < 2048u; angle += 64u) {
        int32_t next_x = (int32_t)((uint32_t)center_x + (uint32_t)
            screen_circle_offset(memory, angle + 512u, radius));
        int32_t next_y = (int32_t)((uint32_t)center_y + (uint32_t)
            screen_circle_offset(memory, angle, radius));
        game_memory_write_u32(memory, coordinates[0], (uint32_t)previous_x);
        game_memory_write_u32(memory, coordinates[1], (uint32_t)previous_y);
        game_memory_write_u32(memory, coordinates[2], (uint32_t)next_x);
        game_memory_write_u32(memory, coordinates[3], (uint32_t)next_y);
        if (memory->fault || !segment(context, coordinates, vertices))
            return 0;
        previous_x = (int32_t)game_memory_read_u32(memory, coordinates[2]);
        previous_y = (int32_t)game_memory_read_u32(memory, coordinates[3]);
        if (memory->fault) return 0;
    }
    game_memory_write_u32(memory, coordinates[0], (uint32_t)previous_x);
    game_memory_write_u32(memory, coordinates[1], (uint32_t)previous_y);
    game_memory_write_u32(memory, coordinates[2], (uint32_t)start_x);
    game_memory_write_u32(memory, coordinates[3], (uint32_t)center_y);
    return !memory->fault && segment(context, coordinates, vertices);
}
