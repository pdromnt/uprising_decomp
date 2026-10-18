#include "game_skin_transform.h"

#include <stdint.h>
#include <string.h>

static float skin_read_f32(GameMemory *memory, GameAddress address) {
    uint32_t bits = game_memory_read_u32(memory, address);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void skin_write_f32(GameMemory *memory, GameAddress address, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    game_memory_write_u32(memory, address, bits);
}

/* Retail x87 combines X and Z before Y, then applies translation. */
static float skin_component(float mx, float x, float my, float y,
    float mz, float z, float translation) {
    return (float)((((long double)mx * (long double)x +
        (long double)mz * (long double)z) +
        (long double)my * (long double)y) +
        (long double)translation);
}

static float skin_dot(float x, float y, float z,
    float vx, float vy, float vz) {
    return (float)((((long double)y * (long double)vy +
        (long double)x * (long double)vx) +
        (long double)z * (long double)vz));
}

static int skin_load_vector(GameMemory *memory, GameAddress address,
    float out[3]) {
    out[0] = skin_read_f32(memory, address);
    out[1] = skin_read_f32(memory, address + 4u);
    out[2] = skin_read_f32(memory, address + 8u);
    return !memory->fault;
}

static int skin_store_vector(GameMemory *memory, GameAddress address,
    const float value[3]) {
    skin_write_f32(memory, address, value[0]);
    skin_write_f32(memory, address + 4u, value[1]);
    skin_write_f32(memory, address + 8u, value[2]);
    return !memory->fault;
}

static int skin_transform_values(GameMemory *memory, GameAddress matrix,
    GameAddress source, int translate, float result[3]) {
    float point[3];
    unsigned row;
    if (!skin_load_vector(memory, source, point)) return 0;
    for (row = 0u; row < 3u; row++) {
        GameAddress base = matrix + row * 16u;
        float mx = skin_read_f32(memory, base);
        float my = skin_read_f32(memory, base + 4u);
        float mz = skin_read_f32(memory, base + 8u);
        float offset = translate ? skin_read_f32(memory, base + 12u) : 0.0f;
        if (memory->fault) return 0;
        result[row] = skin_component(mx, point[0], my, point[1], mz,
            point[2], offset);
    }
    return 1;
}

static int skin_transform(GameMemory *memory, GameAddress matrix,
    GameAddress source, GameAddress destination, int translate) {
    float result[3];
    return skin_transform_values(memory, matrix, source, translate, result) &&
        skin_store_vector(memory, destination, result);
}

/* The common prefix consumes packed 28-byte vertices at +12. Its retail
 * accumulation order differs from the skin records below: X adds Y/Z before
 * X, while Y and Z add X/Y before Z. Keep those intermediate sums extended. */
int game_presentation_transform_vertices(GameMemory *memory, GameAddress object) {
    uint32_t count=game_memory_read_u16(memory,object+0x44u),index;
    GameAddress source,output,matrix;
    if(memory->fault)return 0;
    if(count>=0x8000u || !count)return 1;
    source=game_memory_read_u32(memory,object+0x3cu);
    output=game_memory_read_u32(memory,object+0x40u);
    matrix=game_memory_read_u32(memory,object+0x54u)+4u;
    for(index=0u;index<count && !memory->fault;index++) {
        float point[3],result[3];unsigned row;
        if(!skin_load_vector(memory,source+index*28u+12u,point))return 0;
        for(row=0u;row<3u;row++) {
            GameAddress base=matrix+row*16u;
            long double x=(long double)skin_read_f32(memory,base)*(long double)point[0];
            long double y=(long double)skin_read_f32(memory,base+4u)*(long double)point[1];
            long double z=(long double)skin_read_f32(memory,base+8u)*(long double)point[2];
            long double sum=row==0u?(y+z)+x:(x+y)+z;
            result[row]=(float)(sum+(long double)skin_read_f32(memory,base+12u));
        }
        if(!skin_store_vector(memory,output+index*12u,result))return 0;
    }
    return !memory->fault;
}

int game_presentation_transform(GameMemory *memory, GameAddress object) {
    if(!game_presentation_transform_vertices(memory,object))return 0;
    game_skin_transform(memory,object);
    return !memory->fault;
}

void game_skin_transform(GameMemory *memory, GameAddress model) {
    GameAddress matrix = game_memory_read_u32(memory, model + 0x54u) + 4u;
    GameAddress records = game_memory_read_u32(memory, model + 0x68u);
    int32_t record_count = (int16_t)game_memory_read_u16(memory, model + 0x64u);
    int32_t record_index;
    if (memory->fault) return;

    for (record_index = 0; record_index < record_count; record_index++) {
        GameAddress record = records + (uint32_t)record_index * 0x30u;
        GameAddress input_directions = game_memory_read_u32(memory, record + 0x14u);
        GameAddress output_directions = game_memory_read_u32(memory, record + 0x24u);
        GameAddress input_points = game_memory_read_u32(memory, record + 0x10u);
        GameAddress output_scalars = game_memory_read_u32(memory, record + 0x2cu);
        int32_t influence_count = (int16_t)game_memory_read_u16(memory, record);
        int32_t influence_index;
        float bone_direction[3];
        if (memory->fault || !skin_transform(memory, matrix, record + 4u,
                record + 0x18u, 0) ||
                !skin_load_vector(memory, record + 0x18u, bone_direction))
            return;

        for (influence_index = 0; influence_index < influence_count;
                influence_index++) {
            uint32_t offset = (uint32_t)influence_index * 12u;
            float direction[3], point[3];
            if (!skin_transform(memory, matrix, input_directions + offset,
                    output_directions + offset, 0) ||
                !skin_transform_values(memory, matrix, input_points + offset,
                    1, point) ||
                !skin_load_vector(memory, output_directions + offset,
                    direction))
                return;
            if (influence_index == 0)
                skin_write_f32(memory, record + 0x28u,
                    skin_dot(bone_direction[0], bone_direction[1],
                        bone_direction[2], point[0], point[1], point[2]));
            skin_write_f32(memory, output_scalars +
                (uint32_t)influence_index * 4u,
                skin_dot(direction[0], direction[1], direction[2],
                    point[0], point[1], point[2]));
            if (memory->fault) return;
        }
    }

    {
        int32_t count = (int32_t)game_memory_read_u32(memory, model + 0x98u);
        GameAddress source_owner = game_memory_read_u32(memory, model + 0x50u);
        GameAddress destination = game_memory_read_u32(memory, model + 0x94u);
        GameAddress source;
        int32_t index;
        if (memory->fault) return;
        source = game_memory_read_u32(memory, source_owner + 0x94u);
        if (memory->fault) return;
        for (index = 0; index < count; index++)
            if (!skin_transform(memory, matrix,
                    source + (uint32_t)index * 12u,
                    destination + (uint32_t)index * 12u, 1))
                return;
    }

    {
        int32_t count = (int32_t)game_memory_read_u32(memory, model + 0x80u);
        GameAddress source_owner = game_memory_read_u32(memory, model + 0x50u);
        GameAddress destination = game_memory_read_u32(memory, model + 0x7cu);
        GameAddress source;
        int32_t index;
        if (memory->fault) return;
        source = game_memory_read_u32(memory, source_owner + 0x7cu);
        if (memory->fault) return;
        for (index = 0; index < count; index++)
            if (!skin_transform(memory, matrix,
                    source + (uint32_t)index * 12u,
                    destination + (uint32_t)index * 12u, 1))
                return;
    }
}
