#include "game_initializers.h"
#include <string.h>

void game_initializer_004129b0(GameMemory *memory, GameAddress object) {
    game_memory_write_u32(memory, object + 0xa0u, 0x11du);
}

void game_initializer_00519330(GameMemory *memory, uint32_t value) {
    game_memory_write_u32(memory, 0x00626e10u, value);
}

void game_initializer_004e9660(GameMemory *memory) {
    game_memory_write_u32(memory, 0x0059626cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00596270u, 0u);
}

uint32_t game_initializer_004ac1a0(GameMemory *memory) {
    uint32_t value;
    game_memory_write_u32(memory, 0x00603280u, 0u);
    value = game_memory_read_u32(memory, 0x0058fdf0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x0058fdf4u, value);
    return value;
}

void game_initializer_00413690(GameMemory *memory, GameAddress object,
    uint32_t value) {
    game_memory_write_u32(memory, object + 0x34u, value);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x38u, value);
}

void game_initializer_0049a6c0(GameMemory *memory, GameAddress object) {
    game_memory_write_u32(memory, object, 0x00572230u);
    if (!memory->fault) game_memory_write_u32(memory, object + 8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, 0u);
}

void game_initializer_0048fab0(GameMemory *memory, GameAddress object) {
    uint32_t offset;
    for (offset = 0x0cu; offset <= 0x18u && !memory->fault; offset += 4u)
        game_memory_write_u32(memory, object + offset, 0u);
}

void game_initializer_00548a90(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00639938u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00639950u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x0063993cu, 0u);
    if (!memory->fault) game_memory_write_u8(memory, 0x00639838u, 0u);
}

void game_initializer_004aa490(GameMemory *memory, GameAddress object) {
    uint32_t offset;
    for (offset = 0u; offset <= 0x0cu && !memory->fault; offset += 4u)
        game_memory_write_u32(memory, object + offset, 0xbf800000u);
}

void game_initializer_004d7660(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00594d6cu, 0u);
    if (!memory->fault) game_memory_write_u8(memory, 0x00594d70u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00594df0u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00594df4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00604c20u, 0u);
}

void game_initializer_004a3980(GameMemory *memory, GameAddress object) {
    game_memory_write_u32(memory, object, 0x005743bcu);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x18u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x24u, 0xffffffffu);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x20u, 0u);
}

void game_initializer_004b2d80(GameMemory *memory) {
    static const GameAddress source[] = {0x00603800u, 0x00603a40u, 0x006040d8u};
    static const GameAddress destination[] = {0x006040fcu, 0x006040a4u, 0x00604114u};
    unsigned index;
    for (index = 0u; index < 3u && !memory->fault; ++index) {
        uint32_t value = game_memory_read_u32(memory, source[index]);
        if (!memory->fault) game_memory_write_u32(memory, destination[index], value);
    }
}

void game_initializer_004cc490(GameMemory *memory) {
    static const GameAddress source[] = {0x0059443cu, 0x00594440u, 0x00594444u,
        0x00594448u, 0x0059444cu, 0x00594450u, 0x00594454u};
    static const GameAddress destination[] = {0x00604260u, 0x00604378u, 0x00604268u,
        0x00604480u, 0x00604264u, 0x00604270u, 0x0060426cu};
    unsigned index;
    for (index = 0u; index < 7u && !memory->fault; ++index) {
        uint32_t value = game_memory_read_u32(memory, source[index]);
        if (!memory->fault) game_memory_write_u32(memory, destination[index], value);
    }
    if (!memory->fault) game_memory_write_u32(memory, 0x00604484u, 0u);
}

void game_initializer_00452730(GameMemory *memory, GameAddress object) {
    game_memory_write_u8(memory, object + 0x10u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x0cu, 0u);
    if (!memory->fault) game_memory_write_u8(memory, object + 0x18u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1au, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1eu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x22u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x26u, 0u);
}

void game_initializer_00519ee0(GameMemory *memory) {
    unsigned index;
    for (index = 0u; index < 64u && !memory->fault; ++index)
        game_memory_write_u32(memory, 0x00612530u + index * 4u, 0xffffffffu);
    if (!memory->fault) game_memory_write_u8(memory, 0x0061262eu, 0u);
}

void game_initializer_00453eb0(GameMemory *memory, GameAddress object,
    uint8_t kind, uint8_t subtype, uint32_t payload) {
    game_memory_write_u32(memory, object, 0x00573558u);
    if (!memory->fault) game_memory_write_u8(memory, object + 0x0cu, kind);
    if (!memory->fault) game_memory_write_u8(memory, object + 0x0du, subtype);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x14u, payload);
    if (!memory->fault) game_memory_write_u32(memory, object + 8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x10u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1cu, 0u);
}

void game_initializer_004d8ae0(GameMemory *memory) {
    unsigned index;
    game_memory_write_u32(memory, 0x00605310u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00605314u, 0u);
    if (!memory->fault) game_memory_write_u8(memory, 0x00605323u, 5u);
    if (!memory->fault) game_memory_write_u8(memory, 0x00605320u, 3u);
    if (!memory->fault) game_memory_write_u8(memory, 0x00605321u, 3u);
    if (!memory->fault) game_memory_write_u8(memory, 0x00605322u, 3u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00605324u, 0x00604f10u);
    if (!memory->fault) game_memory_write_u32(memory, 0x0060531cu, 0x400u);
    for (index = 0u; index < 60u && !memory->fault; ++index)
        game_memory_write_u32(memory, 0x00605350u + index * 4u, 0u);
}

void game_initializer_004f0190(GameMemory *memory, int clear_buffer) {
    unsigned index;
    if (clear_buffer) {
        for (index = 0u; index < 512u && !memory->fault; ++index)
            game_memory_write_u32(memory, 0x00605900u + index * 4u, 0u);
    }
    if (!memory->fault) game_memory_write_u32(memory, 0x00606698u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00596278u, 0u);
}

uint32_t game_initializer_00518d10(GameMemory *memory) {
    uint32_t mode, count = 0u;
    game_memory_write_u32(memory, 0x00627064u, 1u);
    mode = game_memory_read_u32(memory, 0x00626e10u);
    if (memory->fault) return 0u;
    if (mode == 0u) {
        count = game_memory_read_u32(memory, 0x00627074u);
        if (!memory->fault && (int32_t)count > 0)
            game_memory_write_u32(memory, 0x00627074u, --count);
    } else if (mode == 1u) {
        count = game_memory_read_u32(memory, 0x00627080u);
        if (!memory->fault && (int32_t)count > 0)
            game_memory_write_u32(memory, 0x00627080u, --count);
    } else {
        count = mode;
    }
    return count;
}

static void write_affine(GameMemory *memory, GameAddress destination,
    uint32_t x, uint32_t y, uint32_t z) {
    static const uint32_t diagonal[] = {0u, 5u, 10u};
    unsigned index;
    for (index = 0u; index < 12u && !memory->fault; ++index) {
        uint32_t value = 0u;
        if (index == diagonal[0] || index == diagonal[1] || index == diagonal[2])
            value = 0x3f800000u;
        else if (index == 3u) value = x;
        else if (index == 7u) value = y;
        else if (index == 11u) value = z;
        game_memory_write_u32(memory, destination + index * 4u, value);
    }
}

void game_initializer_00551490(GameMemory *memory, GameAddress object) {
    uint32_t x = game_memory_read_u32(memory, object + 0x18u);
    uint32_t z = game_memory_read_u32(memory, object + 0x1cu);
    uint32_t y = game_memory_read_u32(memory, object + 0x20u);
    if (!memory->fault) write_affine(memory, object + 0x80u, x, z, y);
}

GameAddress game_initializer_005514f0(GameMemory *memory, GameAddress object) {
    GameAddress destination = game_memory_read_u32(memory, object + 0x54u) + 4u;
    uint32_t x = game_memory_read_u32(memory, object + 0x64u);
    uint32_t y = game_memory_read_u32(memory, object + 0x68u);
    uint32_t z = game_memory_read_u32(memory, object + 0x6cu);
    if (!memory->fault) write_affine(memory, destination, x, y, z);
    return destination;
}

void game_initializer_00549710(GameMemory *memory, GameAddress object) {
    static const uint8_t negative_one_offsets[] = {0x10u, 0x0cu, 0x08u, 0x04u,
        0x44u, 0x40u, 0x3cu, 0x38u};
    static const uint8_t zero_offsets[] = {0x5cu, 0x60u, 0x64u, 0x68u,
        0x1cu, 0x20u, 0x18u, 0x30u};
    unsigned index;
    for (index = 0u; index < sizeof(negative_one_offsets) && !memory->fault; ++index)
        game_memory_write_u32(memory, object + negative_one_offsets[index], 0xbf800000u);
    if (!memory->fault) game_memory_write_u32(memory, object, 0xffffffffu);
    for (index = 0u; index < sizeof(zero_offsets) && !memory->fault; ++index)
        game_memory_write_u32(memory, object + zero_offsets[index], 0u);
}

GameAddress game_initializer_00519230(GameMemory *memory) {
    uint32_t mode = game_memory_read_u32(memory, 0x00626e10u);
    uint32_t index;
    GameAddress destination;
    game_memory_write_u32(memory, 0x00627064u, 1u);
    if (memory->fault || mode > 1u) return 0u;
    index = game_memory_read_u32(memory, mode == 0u ? 0x00627074u : 0x00627080u);
    destination = (mode == 0u ? 0x00626768u : 0x00626e80u) + index * 48u;
    write_affine(memory, destination, 0u, 0u, 0u);
    return destination;
}

static GameAddress active_transform(GameMemory *memory) {
    uint32_t mode = game_memory_read_u32(memory, 0x00626e10u);
    uint32_t index;
    if (memory->fault || mode > 1u) return 0u;
    index = game_memory_read_u32(memory,
        mode == 0u ? 0x00627074u : 0x00627080u);
    return (mode == 0u ? 0x00626768u : 0x00626e80u) + index * 48u;
}

void game_initializer_00518d50(GameMemory *memory, GameAddress destination) {
    GameAddress source = active_transform(memory);
    unsigned index;
    if (memory->fault || source == 0u) return;
    for (index = 0u; index < 12u && !memory->fault; ++index)
        game_memory_write_u32(memory, destination + index * 4u,
            game_memory_read_u32(memory, source + index * 4u));
}

GameAddress game_initializer_00518c00(GameMemory *memory) {
    uint32_t first_count = game_memory_read_u32(memory, 0x00627074u);
    uint32_t second_count = game_memory_read_u32(memory, 0x00627080u);
    uint32_t mode, index;
    GameAddress source, destination;
    unsigned word;
    if (memory->fault || (int32_t)first_count >= 9 ||
        (int32_t)second_count >= 9) return 0u;
    mode = game_memory_read_u32(memory, 0x00626e10u);
    if (memory->fault || mode > 1u) return 0u;
    index = (mode == 0u ? first_count : second_count) + 1u;
    game_memory_write_u32(memory,
        mode == 0u ? 0x00627074u : 0x00627080u, index);
    source = (mode == 0u ? 0x00626738u : 0x00626e50u) + index * 48u;
    destination = source + 48u;
    for (word = 0u; word < 12u && !memory->fault; ++word)
        game_memory_write_u32(memory, destination + word * 4u,
            game_memory_read_u32(memory, source + word * 4u));
    return destination;
}

void game_initializer_0051bec0(GameMemory *memory, GameAddress object,
    uint32_t first, uint32_t second, uint32_t third, uint32_t fourth) {
    game_memory_write_u32(memory, object, 0x005758a0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, first);
    if (!memory->fault) game_memory_write_u32(memory, object + 8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1cu, second);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x20u, third);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x24u, fourth);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x28u, 0u);
}

static void initialize_command(GameMemory *memory, GameAddress object) {
    static const uint8_t dwords[] = {4u,8u,0x10u,0x14u,0x18u,0x1cu,0x20u,
        0x2cu,0x30u,0x34u,0x38u,0x3cu,0x40u,0x48u,0x4cu,0x50u,0x54u,
        0x58u,0x5cu,0x60u};
    unsigned index;
    game_memory_write_u32(memory, object, 0x005721b8u);
    if (!memory->fault) game_memory_write_u8(memory, object + 0x0cu, 0u);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x24u, 0u);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x26u, 0u);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x28u, 0u);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x44u, 0u);
    for (index = 0u; index < sizeof(dwords) && !memory->fault; ++index)
        game_memory_write_u32(memory, object + dwords[index], 0u);
}

void game_initializer_0042cfb0(GameMemory *memory, GameAddress object) {
    initialize_command(memory, object);
}

void game_initializer_0042d180(GameMemory *memory, GameAddress object,
    GameAddress vector, uint32_t second, uint32_t third, uint32_t fourth,
    uint32_t fifth) {
    initialize_command(memory, object);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x2cu,
        game_memory_read_u32(memory, vector));
    if (!memory->fault) game_memory_write_u32(memory, object + 0x30u,
        game_memory_read_u32(memory, vector + 4u));
    if (!memory->fault) game_memory_write_u32(memory, object + 0x34u,
        game_memory_read_u32(memory, vector + 8u));
    if (!memory->fault) game_memory_write_u32(memory, object + 0x38u, second);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x3cu, third);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x44u,
        (uint16_t)fourth);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x54u, fifth);
}

void game_initializer_five_field_record(GameMemory *memory,
    GameAddress object, uint32_t vtable, const uint32_t values[5],
    int first_layout) {
    game_memory_write_u32(memory, object, vtable);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 8u, values[0]);
    if (first_layout) {
        if (!memory->fault) game_memory_write_u32(memory, object + 0x0cu, values[2]);
        if (!memory->fault) game_memory_write_u32(memory, object + 0x10u, values[4]);
        if (!memory->fault) game_memory_write_u32(memory, object + 0x14u, values[1]);
        if (!memory->fault) game_memory_write_u32(memory, object + 0x18u, values[3]);
    } else {
        if (!memory->fault) game_memory_write_u32(memory, object + 0x0cu, values[1]);
        if (!memory->fault) game_memory_write_u32(memory, object + 0x10u, values[2]);
        if (!memory->fault) game_memory_write_u32(memory, object + 0x14u, values[3]);
        if (!memory->fault) game_memory_write_u32(memory, object + 0x18u, values[4]);
    }
}

void game_initializer_0048f320(GameMemory *memory, GameAddress object) {
    game_memory_write_u16(memory, object + 8u, 0xffffu);
    if (!memory->fault) game_memory_write_u32(memory, object, 0u);
}

void game_initializer_004dbca0(GameMemory *memory, GameAddress object,
    uint32_t value) {
    game_memory_write_u32(memory, object, 0x00574b38u);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, value);
}

void game_initializer_004df740(GameMemory *memory, GameAddress object,
    uint32_t value) {
    game_memory_write_u32(memory, object, 0x00574c68u);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 8u, value);
}

void game_initializer_0051bf40(GameMemory *memory, GameAddress object,
    uint32_t first, uint32_t second) {
    game_memory_write_u32(memory, object, 0x005758a0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, first);
    if (!memory->fault) game_memory_write_u32(memory, object + 8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x20u, 0xffffffffu);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x28u, second);
}

void game_initializer_0043ca30(GameMemory *memory, GameAddress object,
    uint32_t first, GameAddress source) {
    uint32_t linked = game_memory_read_u32(memory, source + 0x28u);
    if (memory->fault) return;
    game_memory_write_u32(memory, object, 0x00572e38u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, first);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x0cu, linked);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x14u, source);
    if (!memory->fault) game_memory_write_u32(memory, object + 8u, 0u);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x10u, 0xffffu);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x18u, 0u);
}

void game_initializer_004a6d90(GameMemory *memory) {
    static const GameAddress sources[] = {0x00602f78u,0x00602fa0u,0x00602fd4u};
    static const uint32_t sizes[] = {10u,13u,17u};
    unsigned table, index;
    for (table = 0u; table < 3u && !memory->fault; ++table) {
        GameAddress destination = table == 0u ? 0x005ac678u :
            game_memory_read_u32(memory, table == 1u ? 0x005f0224u : 0x005f0220u) +
            (table == 1u ? 0x1fcu : 0x29cu);
        for (index = 0u; index < sizes[table] && !memory->fault; ++index) {
            uint32_t value = game_memory_read_u32(memory, sources[table] + index * 4u);
            if (!memory->fault && value != 0xffffffbbu)
                game_memory_write_u32(memory, destination + index * 4u, value);
        }
    }
}

void game_initializer_004a6df0(GameMemory *memory) {
    uint32_t block, index;
    for (block = 0u; block < 4u && !memory->fault; ++block) {
        GameAddress offset = block * 0xf0u;
        for (index = 0u; index < 32u && !memory->fault; ++index) {
            game_memory_write_u32(memory, 0x00602bb8u + offset + index * 4u,
                0xffffffbbu);
            if (!memory->fault) game_memory_write_u32(memory,
                0x006027f0u + offset + index * 4u, 0xffffffbbu);
        }
        for (index = 0x80u; index <= 0xe0u && !memory->fault; index += 4u)
            game_memory_write_u32(memory, 0x006027f0u + offset + index, 0xffffffbbu);
        for (index = 0u; index < 6u && !memory->fault; ++index)
            game_memory_write_u32(memory, 0x00602c38u + offset + index * 4u,
                0xffffffbbu);
        if (!memory->fault) game_memory_write_u32(memory,
            0x006028d0u + offset, 0xffffffbbu);
        if (!memory->fault) game_memory_write_u32(memory,
            0x006027e4u + (block + 1u) * 0xf0u, 0xffffffbbu);
        if (!memory->fault) game_memory_write_u32(memory,
            0x006027e8u + (block + 1u) * 0xf0u, 0xffffffbbu);
        if (!memory->fault) game_memory_write_u32(memory,
            0x006027ecu + (block + 1u) * 0xf0u, 0xffffffbbu);
    }
}

void game_initializer_004b33a0(GameMemory *memory) {
    GameAddress destination = game_memory_read_u32(memory, 0x0060386cu);
    uint32_t width = game_memory_read_u32(memory, 0x00627928u);
    uint32_t height = game_memory_read_u32(memory, 0x0062792cu);
    uint32_t value = game_memory_read_u32(memory, 0x005ac6acu);
    uint32_t offset;
    if (memory->fault) return;
    if (destination != 0u) {
        for (offset = 0u; offset < 0x100u && !memory->fault; offset += 0x10u) {
            game_memory_write_u32(memory, destination + offset,
                (uint32_t)(((int32_t)width) >> 1u));
            if (!memory->fault) game_memory_write_u32(memory, destination + offset + 4u,
                (uint32_t)(((int32_t)height) >> 1u));
            if (!memory->fault) game_memory_write_u32(memory, destination + offset + 8u,
                value);
        }
    }
    if (!memory->fault) game_memory_write_u32(memory, 0x006039a8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00603874u, 0u);
}

void game_initializer_004b3960(GameMemory *memory) {
    static const struct { GameAddress address; uint32_t value; } defaults[] = {
        {0x005ac678u,0u},{0x006041c4u,0x280u},{0x00604144u,0x280u},
        {0x0060413cu,0x280u},{0x00604124u,0u},{0x00604170u,2u},
        {0x00604180u,1u},{0x00604120u,1u},{0x00604188u,0u},
        {0x00604164u,10u},{0x00604160u,10u},{0x00604138u,0x780u},
        {0x00604130u,0u},{0x00604194u,1u},{0x00604128u,0x1e0u},
        {0x00604178u,0u},{0x006041c8u,2u},{0x00604190u,0x5a0u},
        {0x006041b8u,2u},{0x00604134u,1u},{0x00604140u,0u},
        {0x006041a4u,0x3480u},{0x0060415cu,4u},{0x00604198u,4u},
        {0x006041b0u,2u},{0x00604154u,1u},{0x00604150u,1u},
        {0x006041c0u,1u},{0x006041a0u,1u},{0x0060418cu,2u},
        {0x006041acu,0x2328u},{0x00604148u,6u},{0x006041a8u,3u},
        {0x006041b4u,0x40200000u},{0x00604184u,0x3f000000u},
        {0x0060412cu,0u},{0x0060417cu,0x40c00000u},
        {0x0060419cu,0x3c0u},{0x0060414cu,6u}
    };
    unsigned index;
    for (index = 0u; index < sizeof(defaults) / sizeof(defaults[0]) &&
        !memory->fault; ++index)
        game_memory_write_u32(memory, defaults[index].address, defaults[index].value);
}

static void copy_string(GameMemory *memory, GameAddress destination,
    GameAddress source) {
    uint8_t value;
    do {
        value = game_memory_read_u8(memory, source++);
        if (!memory->fault) game_memory_write_u8(memory, destination++, value);
    } while (!memory->fault && value != 0u);
}

void game_initializer_0047afd0(GameMemory *memory, GameAddress object,
    uint32_t selection, GameAddress first, GameAddress second) {
    copy_string(memory, object + 0x39cu, first);
    if (memory->fault) return;
    if (second != 0u) copy_string(memory, object + 0x31cu, second);
    else game_memory_write_u8(memory, object + 0x31cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x468u, 1u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x3c0u, selection);
}

static GameAddress resolve_provider(GameMemory *memory, uint32_t index,
    uint32_t limit) {
    uint32_t mapped;
    GameAddress provider;
    if (index == 0xffffffffu) return 0u;
    mapped = game_memory_read_u32(memory, 0x005f01f0u + index * 4u);
    if (memory->fault || (int32_t)mapped < 0 || mapped >= limit) return 0u;
    provider = game_memory_read_u32(memory, 0x005f0210u + mapped * 4u);
    if (memory->fault || game_memory_read_u32(memory, provider + 0x14u) == 0u)
        return 0u;
    return provider;
}

void game_initializer_004dc380(GameMemory *memory, GameAddress object,
    uint32_t first, uint32_t second, uint32_t third, uint32_t provider_index,
    uint32_t fallback_index) {
    GameAddress provider;
    if (provider_index != 0xffffffffu) {
        provider = resolve_provider(memory, provider_index, 6u);
    } else {
        game_memory_write_u32(memory, object + 0x14u, fallback_index);
        provider = fallback_index == 0xffffffffu ?
            game_memory_read_u32(memory, 0x005f0208u) : 0u;
    }
    if (!memory->fault) game_memory_write_u32(memory, object + 0x10u, provider);
    if (!memory->fault) game_memory_write_u32(memory, object, 0x00574b60u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x0cu, first);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, second);
    if (!memory->fault) game_memory_write_u32(memory, object + 8u, third);
}

void game_initializer_004dfbb0(GameMemory *memory, GameAddress object,
    uint32_t provider_index, uint32_t fallback_index, uint32_t first,
    uint32_t second, GameAddress name) {
    GameAddress provider;
    game_memory_write_u32(memory, object + 4u, 0u);
    if (provider_index != 0xffffffffu) {
        provider = resolve_provider(memory, provider_index, 4u);
    } else {
        game_memory_write_u32(memory, object + 0x0cu, fallback_index);
        provider = fallback_index == 0xffffffffu ?
            game_memory_read_u32(memory, 0x005f0208u) : 0u;
    }
    if (!memory->fault) game_memory_write_u32(memory, object + 8u, provider);
    if (!memory->fault) game_memory_write_u32(memory, object, 0x00574c88u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x10u, first);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x14u, second);
    if (!memory->fault) copy_string(memory, object + 0x18u, name);
}

uint32_t game_initializer_clear_mask(GameMemory *memory, GameAddress object,
    uint32_t mask) {
    uint32_t value = game_memory_read_u32(memory, object + 0xa0u) & ~mask;
    if (!memory->fault) game_memory_write_u32(memory, object + 0xa0u, value);
    return value;
}

void game_initializer_004cec80(GameMemory *memory, GameAddress object) {
    static const uint32_t zero_offsets[] = {
        0x28u, 0x10u, 0x14u, 0x24u, 0x2cu
    };
    unsigned index;
    for (index = 0u; index < sizeof(zero_offsets) / sizeof(zero_offsets[0]) &&
        !memory->fault; ++index)
        game_memory_write_u32(memory, object + zero_offsets[index], 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x0cu, 3u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x18u,
        game_memory_read_u32(memory, 0x00594470u));
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1cu,
        game_memory_read_u32(memory, 0x00594474u));
    if (!memory->fault) game_memory_write_u32(memory, object + 0x20u,
        game_memory_read_u32(memory, 0x00594478u));
}

uint32_t game_initializer_object_type(GameMemory *memory, GameAddress object,
    GameAddress *state) {
    GameAddress value = game_memory_read_u32(memory, object + 0xdcu);
    if (state) *state = value;
    return game_memory_read_u32(memory, value + 0x0cu);
}

static void game_initializer_write_defaults(GameMemory *memory,
        const GameAddress *addresses, const uint32_t *values,
        unsigned count) {
    unsigned index;
    for (index = 0u; index < count && !memory->fault; ++index)
        game_memory_write_u32(memory, addresses[index], values[index]);
}

void game_initializer_game_defaults_before_callback(GameMemory *memory) {
    static const GameAddress byte_addresses[] = {
        0x00639a78u, 0x005ac930u, 0x00626dd0u, 0x005acbf8u,
        0x005ac8c0u, 0x005acc40u, 0x005acc90u
    };
    static const GameAddress addresses[] = {
        0x005ac688u,0x005ac908u,0x005acdb4u,0x00626718u,
        0x005acd64u,0x005ac67cu,0x005ac68cu,0x005ac684u,
        0x005ac690u,0x00626d50u,0x005ac694u,0x0059bd98u,
        0x005accdcu,0x005ac698u,0x005ac69cu
    };
    static const uint32_t values[] = {
        0u,0u,1u,1u,0u,1u,1u,0u,0x244u,0x3f800000u,0x244u,
        0u,0xffffffffu,0x320u,0x488u
    };
    unsigned index;
    for (index = 0u; index < sizeof(byte_addresses) /
            sizeof(byte_addresses[0]) && !memory->fault; ++index)
        game_memory_write_u8(memory, byte_addresses[index], 0u);
    game_initializer_write_defaults(memory, addresses, values,
        (unsigned)(sizeof(addresses) / sizeof(addresses[0])));
}

void game_initializer_game_defaults_after_callback(GameMemory *memory) {
    static const GameAddress addresses[] = {
        0x00604250u,0x005e1218u,0x00629730u,0x005e1210u,
        0x005e1224u,0x005e1214u,0x005e122cu,0x005acc8cu,
        0x005ac98cu,0x005ac974u,0x005ac914u,0x005ac6a8u,
        0x005accd8u,0x005acc3cu,0x00596a20u,0x00596a2cu,
        0x00596a34u,0x00596a38u,0x005e1230u,0x006027e8u,
        0x005e120cu,0x005e1220u,0x005e1234u,0x005e1228u,
        0x005e121cu,0x005e1238u,0x005e123cu,0x0058de70u,
        0x005ac9c0u,0x005acc84u,0x005ac904u,0x005acbf0u,
        0x005ac900u,0x005ac9d8u,0x005ac978u,0x005ac97cu,
        0x005ac980u,0x005ac984u,0x005ac988u,0x005ac990u,
        0x005ac994u,0x005ac998u,0x005ac99cu,0x005ac9a0u,
        0x005ac9a4u,0x005ac9a8u,0x005ac9acu,0x005ac9b0u,
        0x005ac9b4u,0x005ac9b8u,0x005ac9bcu,0x00596a00u,
        0x00596a04u,0x00596a08u,0x00596a0cu,0x00596a10u,
        0x00596a14u,0x00596a18u,0x00596a1cu,0x00596a24u,
        0x00596a28u,0x00596a30u
    };
    static const uint32_t values[] = {
        0x42u,0x558cu,0x20000u,2u,3u,3u,12u,2000u,1000u,6000u,
        12000u,3125u,6250u,3750u,25u,15u,3u,10u,1u,50u,1u,1u,
        5u,1u,1u,1u,5u,0u,5u,0u,0u,0u,0u,0u,1u,1u,1u,100u,
        1u,1u,0u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,100u,100u,
        1u,200u,100u,100u,20u,100u,200u,20u,50u
    };
    game_initializer_write_defaults(memory, addresses, values,
        (unsigned)(sizeof(addresses) / sizeof(addresses[0])));
}

static uint8_t game_initializer_encoded_character(unsigned character) {
    if (character >= 'a' && character <= 'z')
        return (uint8_t)(character - 'a');
    if (character >= 'A' && character <= 'Z')
        return (uint8_t)(character - 'A');
    if (character >= '0' && character <= '9')
        return (uint8_t)(character - 22u);
    switch (character) {
        case '.': return 36u;
        case '!': return 37u;
        case '?': return 38u;
        case ' ': return 39u;
        case '(': return 40u;
        case ')': return 41u;
        case '/': return 42u;
        case ',': return 43u;
        case ':': return 44u;
        case '-': return 45u;
        case '*': return 46u;
        case '[': return 47u;
        case ']': return 48u;
        case '\\': return 49u;
        case '<': return 50u;
        case '>': return 51u;
        case '_': return 52u;
        case '"': return 53u;
        case '=': return 54u;
        case '+': return 55u;
        case '%': return 56u;
        case '$': return 57u;
        case '#': return 58u;
        case '\'': return 59u;
        default: return 60u;
    }
}

void game_initializer_character_alphabet(GameMemory *memory) {
    unsigned character;
    for (character = 0u; character < 256u && !memory->fault; ++character)
        game_memory_write_u8(memory, 0x006290f8u + character,
            game_initializer_encoded_character(character));
}

uint8_t game_initializer_character_noise_value(uint32_t random_value,
        uint32_t multiplier, uint8_t base) {
    uint32_t product = random_value * multiplier;
    uint32_t adjustment = (product & 0x80000000u) ? 0x7fffu : 0u;
    uint32_t biased = product + adjustment;
    uint32_t scaled = biased >> 15u;
    if ((biased & 0x80000000u) != 0u) scaled |= 0xfffe0000u;
    return (uint8_t)((uint8_t)scaled + base - 0x60u);
}

void game_initializer_record_catalog_table(GameMemory *memory,
        GameAddress self, uint32_t offset, GameAddress table) {
    game_memory_write_u32(memory, self + offset, table);
}

static void game_initializer_write_record(GameMemory *memory,
        GameAddress record, uint8_t kind, uint16_t first, uint16_t second,
        uint16_t third, uint16_t fourth, uint32_t flags) {
    game_memory_write_u8(memory, record, kind);
    if (!memory->fault) game_memory_write_u16(memory, record + 2u, first);
    if (!memory->fault) game_memory_write_u16(memory, record + 4u, second);
    if (!memory->fault) game_memory_write_u16(memory, record + 6u, third);
    if (!memory->fault) game_memory_write_u16(memory, record + 8u, fourth);
    if (!memory->fault) game_memory_write_u32(memory, record + 0x20u, flags);
    if (!memory->fault) game_memory_write_u32(memory, record + 0x24u, 0u);
}

void game_initializer_record_catalog_records(GameMemory *memory,
        GameAddress records) {
    static const uint8_t kind[6] = {4u, 4u, 4u, 4u, 4u, 4u};
    static const uint16_t fields[6][4] = {
        {7u,0u,1u,8u}, {8u,1u,2u,9u}, {9u,2u,3u,10u},
        {4u,11u,10u,3u}, {5u,12u,11u,4u}, {6u,13u,12u,5u}
    };
    static const uint32_t flags[6] = {
        0x00000818u,0x20000018u,0x20000018u,
        0x20000018u,0x20000018u,0x00000818u
    };
    unsigned index;
    for (index = 0u; index < 6u && !memory->fault; ++index)
        game_initializer_write_record(memory, records + index * 0x28u,
            kind[index], fields[index][0], fields[index][1],
            fields[index][2], fields[index][3], flags[index]);
    if (!memory->fault)
        game_memory_write_u32(memory, records + 0xecu, 0u);
}

void game_initializer_resource_owner_begin(GameMemory *memory,
        GameAddress self, uint32_t first_selector, uint32_t second_selector,
        uint32_t first_index, uint32_t second_index, uint32_t trailing_value) {
    game_memory_write_u32(memory, self, 0x00574bb8u);
    if (!memory->fault) game_memory_write_u32(memory, self + 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self, 0x00574ca0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 8u,
        first_selector);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x0cu,
        second_selector);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10u,
        first_index);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x14u,
        second_index);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x18u,
        trailing_value);
}

void game_initializer_resource_owner_reference(GameMemory *memory,
        GameAddress self, uint32_t offset, GameAddress reference) {
    game_memory_write_u32(memory, self + offset, reference);
}

void game_initializer_presentation_registry_begin(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00639b00u, 36u);
}

void game_initializer_presentation_registry_primary(GameMemory *memory,
        GameAddress primary) {
    game_memory_write_u32(memory, 0x00639b04u, primary);
}

void game_initializer_presentation_registry_finish(GameMemory *memory,
        GameAddress primary, GameAddress secondary) {
    unsigned index;
    game_memory_write_u32(memory, 0x00639afcu, secondary);
    for (index = 0u; index < 36u && !memory->fault; ++index)
        game_memory_write_u32(memory, primary + index * 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00639af8u, 0u);
}

uint32_t game_initializer_presentation_controller(GameMemory *memory,
        GameAddress self) {
    uint32_t flags = game_memory_read_u32(memory, self + 0x10u) | 1u;
    game_memory_write_u32(memory, self, 0x005764f8u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xb0u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10u, flags);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x70u, 0u);
    return flags;
}

void game_initializer_presentation_variant(GameMemory *memory,
        GameAddress self, GameAddress vtable, uint8_t kind,
        GameAddress first_callback, GameAddress second_callback) {
    game_memory_write_u32(memory, self, vtable);
    if (!memory->fault) game_memory_write_u8(memory, self + 0x0cu, kind);
    if (!memory->fault)
        game_memory_write_u32(memory, self + 0x8cu, first_callback);
    if (!memory->fault)
        game_memory_write_u32(memory, self + 0x90u, second_callback);
}

void game_initializer_presentation_node(GameMemory *memory,
        GameAddress self, uint32_t flags, GameAddress vtable, uint8_t kind,
        GameAddress first_callback, GameAddress second_callback) {
    game_memory_write_u32(memory, self, 0x00576238u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xa8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xacu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10u, flags);
    if (!memory->fault)
        game_initializer_presentation_variant(memory, self, vtable, kind,
            first_callback, second_callback);
}

void game_initializer_presentation_runtime(GameMemory *memory,
        GameAddress self, uint32_t flags) {
    game_memory_write_u32(memory, self, 0x00576758u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xdcu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10u, flags);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xc8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x108u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x138u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x104u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xe0u, 5u);
    if (!memory->fault) game_memory_write_u8(memory, self + 0xd8u, 0u);
}

void game_initializer_type_004528_runtime(GameMemory *memory) {
    unsigned index;
    game_memory_write_u32(memory, 0x005d0848u, 0x00573550u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005d084cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005d0850u, 0u);
    if (!memory->fault) game_memory_write_u8(memory, 0x005d0854u, 0u);
    if (!memory->fault) game_memory_write_u8(memory, 0x005d0855u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005d0858u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005d085cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005d0864u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005d0838u, 1u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005d083cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005e106cu,
        0xffffffffu);
    if (!memory->fault) game_memory_write_u32(memory, 0x005e0868u,
        0x40800000u);
    for (index = 0u; index < 100u && !memory->fault; ++index)
        game_memory_write_u32(memory, 0x005e1074u + index * 4u, 0u);
}

void game_initializer_runtime_descriptor(GameMemory *memory,
        GameAddress descriptor) {
    unsigned index;
    game_memory_write_u32(memory, 0x00597068u, descriptor);
    for (index = 0u; index < 4u && !memory->fault; ++index)
        game_memory_write_u32(memory, 0x00624ee8u + index * 4u,
            game_memory_read_u8(memory, descriptor + 0x10u + index));
    if (!memory->fault) game_memory_write_u32(memory, 0x00624ef8u,
        game_memory_read_u32(memory, descriptor + 0x14u));
}

/*53cc40:1024 effect samples. Each attempt advances the wrapping source
 *sequence by7*x+tick, then maps16807*x modulo2^31-1 into the configured scale.
 *The first sample is used only for the strict upper clamp; otherwise the
 *second sample is stored, and the source cursor is retained for consumers. */
int game_initializer_effect_random_table(GameMemory *memory) {
    uint32_t scale_bits=game_memory_read_u32(memory,0x576960u),limit_bits=game_memory_read_u32(memory,0x576964u);
    uint32_t cursor=0x00bc4ff2u,index;float scale,limit;
    memcpy(&scale,&scale_bits,4u);memcpy(&limit,&limit_bits,4u);
    game_memory_write_u32(memory,0x6308c0u,0u);
    for(index=0;index<1024u && !memory->fault;index++) {
        uint32_t mapped,bits;long double sample;float value;
        cursor=cursor*7u+game_memory_read_u32(memory,0x5ac6acu);
        mapped=(uint32_t)(((uint64_t)cursor*16807u)%2147483647u);
        sample=(long double)(int32_t)mapped*scale;
        if(sample>(long double)limit)bits=0x3f7ffffeu;
        else {
            cursor=cursor*7u+game_memory_read_u32(memory,0x5ac6acu);
            mapped=(uint32_t)(((uint64_t)cursor*16807u)%2147483647u);
            value=(float)((long double)(int32_t)mapped*scale);memcpy(&bits,&value,4u);
        }
        game_memory_write_u32(memory,0x59d558u,cursor);
        game_memory_write_u32(memory,0x6308c8u+index*4u,bits);
    }
    return !memory->fault;
}
void game_initializer_large_work_area(GameMemory *memory, GameAddress self) {
    unsigned index;
    for (index = 0u; index < 0x800u && !memory->fault; ++index)
        game_memory_write_u32(memory, self + index * 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x2000u, 1u);
    for (index = 0u; index < 9u && !memory->fault; ++index)
        game_memory_write_u32(memory, self + 0x2004u + index * 4u, 0u);
    for (index = 0u; index < 0x10u && !memory->fault; ++index)
        game_memory_write_u32(memory, 0x00596f20u + index * 4u, 0u);
}

void game_initializer_type_004fd2_service(GameMemory *memory) {
    game_memory_write_u32(memory, 0x0060c4f0u, 0x00575210u);
    if (!memory->fault) game_memory_write_u32(memory, 0x0060c4c8u, 1u);
    if (!memory->fault) game_memory_write_u32(memory, 0x0060c4ccu, 0u);
}

void game_initializer_render_resource_holder(GameMemory *memory,
        GameAddress self) {
    static const uint32_t offsets[6] = {
        0x4cu, 0x48u, 0x44u, 0x40u, 0x50u, 0x58u
    };
    unsigned index;
    game_memory_write_u32(memory, self, 0x00573220u);
    for (index = 0u; index < 6u && !memory->fault; ++index)
        game_memory_write_u32(memory, self + offsets[index], 0u);
    if (!memory->fault) game_memory_write_u8(memory, self + 0x54u, 0u);
}

void game_initializer_flagged_object(GameMemory *memory, GameAddress self,
        GameAddress vtable, uint32_t flags, uint8_t type, int set_type,
        uint32_t extra_byte_offset, uint8_t extra_byte_value) {
    game_memory_write_u32(memory, self, vtable);
    if (set_type && !memory->fault)
        game_memory_write_u8(memory, self + 0x0cu, type);
    if (extra_byte_offset != 0u && !memory->fault)
        game_memory_write_u8(memory, self + extra_byte_offset,
            extra_byte_value);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10u, flags);
}

void game_initializer_type45_runtime(GameMemory *memory, GameAddress self) {
    game_memory_write_u32(memory, self, 0x00573028u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xf4u, 0u);
    if (!memory->fault) game_memory_write_u8(memory, self + 0x0cu, 0x45u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xf8u, 0u);
}

void game_initializer_object_identity(GameMemory *memory, GameAddress self,
        GameAddress vtable, uint8_t type, int set_type) {
    game_memory_write_u32(memory, self, vtable);
    if (set_type && !memory->fault)
        game_memory_write_u8(memory, self + 0x0cu, type);
}

void game_initializer_type22_identity(GameMemory *memory, GameAddress self,
        GameAddress vtable, uint8_t type, GameAddress primary_store,
        GameAddress secondary_store) {
    game_initializer_object_identity(memory, self, vtable, type, 1);
    if (!memory->fault)
        game_memory_write_u32(memory, self + 0x258u, primary_store);
    if (!memory->fault)
        game_memory_write_u32(memory, self + 0x25cu, secondary_store);
}

void game_initializer_linked_object(GameMemory *memory, GameAddress self,
        GameAddress vtable, uint8_t type, uint32_t flags,
        uint32_t first_clear_offset, uint32_t second_clear_offset,
        uint32_t byte_offset, uint8_t byte_value) {
    GameAddress link;
    game_initializer_object_identity(memory, self, vtable, type, 1);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10u, flags);
    if (first_clear_offset != 0u && !memory->fault)
        game_memory_write_u32(memory, self + first_clear_offset, 0u);
    if (second_clear_offset != 0u && !memory->fault)
        game_memory_write_u32(memory, self + second_clear_offset, 0u);
    if (byte_offset != 0u && !memory->fault)
        game_memory_write_u8(memory, self + byte_offset, byte_value);
    if (memory->fault) return;
    link = game_memory_read_u32(memory, self + 0x54u);
    if (!memory->fault) game_memory_write_u32(memory, link + 0x40u, self);
}

void game_initializer_type40_object(GameMemory *memory, GameAddress self,
        GameAddress vtable, uint8_t type, uint32_t flags) {
    game_initializer_linked_object(memory, self, vtable, type, flags,
        0x150u, 0x154u, 0u, 0u);
}

void game_initializer_multistage_object(GameMemory *memory, GameAddress self,
        GameAddress first_vtable, GameAddress second_vtable,
        GameAddress final_vtable, uint8_t type, uint32_t flags) {
    game_memory_write_u32(memory, self, first_vtable);
    if (second_vtable != 0u && !memory->fault)
        game_memory_write_u32(memory, self, second_vtable);
    if (!memory->fault) game_memory_write_u32(memory, self, final_vtable);
    if (!memory->fault) game_memory_write_u8(memory, self + 0x0cu, type);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10u, flags);
}

void game_initializer_base_gameplay_record(GameMemory *memory,
        GameAddress self, uint32_t flags) {
    game_memory_write_u32(memory, self, 0x00575040u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xa0u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10u, flags);
}

void game_initializer_presentation_object(GameMemory *memory,
        GameAddress self, uint32_t flags) {
    static const uint32_t clear_offsets[7] = {
        0x13cu, 0x144u, 0x140u, 0x190u, 0x194u, 0x198u, 0x17cu
    };
    unsigned index;
    game_initializer_object_identity(memory, self, 0x00577050u, 0x27u, 1);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10u, flags);
    for (index = 0u; index < 7u && !memory->fault; ++index)
        game_memory_write_u32(memory, self + clear_offsets[index], 0u);
}

void game_initializer_source_copy_object(GameMemory *memory,
        GameAddress self, GameAddress source, uint32_t flags) {
    uint32_t owner;
    game_initializer_flagged_object(memory, self, 0x00572b48u, flags,
        0xb0u, 1, 0u, 0u);
    if (memory->fault) return;
    owner = game_memory_read_u32(memory, source + 0x64u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x64u, owner);
}

void game_initializer_gameplay_controller(GameMemory *memory,
        GameAddress self) {
    game_memory_write_u32(memory, self, 0x00575b08u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xe8u, 0u);
}

void game_initializer_inline_owner_object(GameMemory *memory,
        GameAddress self, GameAddress vtable, uint8_t type, uint32_t flags,
        GameAddress owner) {
    game_initializer_flagged_object(memory, self, vtable, flags, type, 1,
        0u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x64u, owner);
}

void game_initializer_type0c_runtime(GameMemory *memory, GameAddress self) {
    static const uint32_t offsets[4] = { 0xe4u, 0xecu, 0xe8u, 0xfcu };
    unsigned index;
    game_initializer_object_identity(memory, self, 0x00573fb8u, 0x0cu, 1);
    for (index = 0u; index < 4u && !memory->fault; ++index)
        game_memory_write_u32(memory, self + offsets[index], 0u);
}

void game_initializer_type28_runtime(GameMemory *memory, GameAddress self,
        uint32_t flags) {
    game_initializer_type22_identity(memory, self, 0x005716b8u, 0x28u,
        0x005ab0a8u, 0x005ab0d4u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x10u, flags);
}

void game_initializer_runtime_descriptor_object(GameMemory *memory,
        GameAddress self) {
    GameAddress fourth = self + 0x90u;
    game_memory_write_u32(memory, self + 0x94u, 0u);
    if (!memory->fault) game_memory_write_u16(memory, fourth, 0u);
    if (!memory->fault) game_memory_write_u16(memory, self + 0x92u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xa0u, 0x11du);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x40u, self);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x44u,
        self + 0x10u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x48u,
        self + 0x20u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x4cu,
        self + 0x30u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xacu, fourth);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xa4u, 0u);
}

void game_initializer_pointer_ring(GameMemory *memory, GameAddress self) {
    GameAddress ring = self + 0x84u;
    game_memory_write_u32(memory, self, 0x00571ab8u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xe0u, ring);
    if (!memory->fault) game_memory_write_u32(memory, ring, self + 0x44u);
    if (!memory->fault) game_memory_write_u32(memory, ring + 4u,
        self + 0x54u);
    if (!memory->fault) game_memory_write_u32(memory, ring + 8u,
        self + 0x64u);
    if (!memory->fault) game_memory_write_u32(memory, ring + 0x0cu,
        self + 0x74u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xecu,
        self + 0x94u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xf0u,
        self + 0xd4u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0xe8u, 0u);
}

void game_initializer_pointer_ring_finish(GameMemory *memory,
        GameAddress self) {
    game_memory_write_u32(memory, self + 0x40u, 0u);
}

void game_initializer_type00416x(GameMemory *memory, GameAddress self,
        GameAddress vtable, unsigned variant, uint32_t value) {
    game_memory_write_u32(memory, self, vtable);
    if (memory->fault) return;
    if (variant == 1u) {
        game_memory_write_u32(memory, self + 0x12cu, 0u);
        if (!memory->fault) game_memory_write_u32(memory, self + 0x128u, 0u);
    } else if (variant == 2u) {
        game_memory_write_u32(memory, self + 4u, value);
    }
}

void game_initializer_presentation_buffer_controller(GameMemory *memory,
        GameAddress self) {
    game_memory_write_u32(memory, self, 0x00572038u);
    if (!memory->fault) game_memory_write_u8(memory, self + 0x0cu, 0x10u);
}

void game_initializer_store_presentation_buffer(GameMemory *memory,
        GameAddress self, unsigned index, GameAddress buffer) {
    game_memory_write_u32(memory, self + (index == 0u ? 0x40u : 0x70u),
        buffer);
}

uint32_t game_initializer_global_backing_size(unsigned index) {
    static const uint32_t sizes[4] = {
        0x27100u, 0x2bf20u, 0x57e40u, 0x927c0u
    };
    return index < 4u ? sizes[index] : 0u;
}

void game_initializer_store_global_backing(GameMemory *memory,
        unsigned index, GameAddress buffer) {
    static const GameAddress slots[4] = {
        0x00597724u, 0x0059772cu, 0x00597734u, 0x0059773cu
    };
    if (index < 4u) game_memory_write_u32(memory, slots[index], buffer);
}

void game_initializer_type0045fd(GameMemory *memory, GameAddress self,
        uint32_t flags, GameAddress link) {
    game_initializer_flagged_object(memory, self, 0x00573918u, flags,
        0xa0u, 1, 0u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, link + 0x40u, self);
}

void game_initializer_teleporter_scene_object(GameMemory *memory,
        GameAddress object, GameAddress prototype, GameAddress buffer,
        GameAddress resource, int destination) {
    static const uint32_t copied[] = {
        0x2cu, 0x30u, 0x34u, 0x38u, 0x3cu
    };
    uint32_t flags;
    unsigned i;
    game_memory_write_u32(memory, object + 4u, 0u);
    game_memory_write_u32(memory, object + 8u, 0u);
    game_memory_write_u8(memory, object + 0x0cu,
        game_memory_read_u8(memory, prototype + 0x0cu));
    flags = game_memory_read_u32(memory, prototype + 0x10u) & ~1u;
    if (destination) flags |= 0x0c00u;
    game_memory_write_u32(memory, object + 0x10u, flags);
    for (i = 0; i < 6u && !memory->fault; ++i)
        game_memory_write_u32(memory, object + 0x14u + i * 4u, 0u);
    for (i = 0; i < 3u && !memory->fault; ++i)
        game_memory_write_u16(memory, object + 0x24u + i * 2u, 0u);
    for (i = 0; i < sizeof(copied) / sizeof(copied[0]) &&
            !memory->fault; ++i)
        game_memory_write_u32(memory, object + copied[i],
            game_memory_read_u32(memory, prototype + copied[i]));
    if (memory->fault) return;
    game_memory_write_u16(memory, object + 0x44u,
        game_memory_read_u16(memory, prototype + 0x44u));
    game_memory_write_u32(memory, object + 0x40u, buffer);
    game_memory_write_u32(memory, object + 0x48u, 0u);
    game_memory_write_u32(memory, object + 0x4cu, 0u);
    game_memory_write_u32(memory, object + 0x50u, prototype);
    game_memory_write_u32(memory, object + 0x54u, resource);
    game_memory_write_u32(memory, object + 0x58u, 0u);
    game_memory_write_u32(memory, object + 0x5cu, 0u);
    game_memory_write_u32(memory, object + 0x60u, 0u);
    if (memory->fault) return;
    game_memory_write_u32(memory, object,
        destination ? 0x00573918u : 0x005738a0u);
    game_memory_write_u8(memory, object + 0x0cu,
        destination ? 0xa0u : 0xa1u);
    if (destination && !memory->fault)
        game_memory_write_u32(memory, resource + 0x40u, object);
}

void game_initializer_effect_object_begin(GameMemory *memory,
        GameAddress self, GameAddress source) {
    game_memory_write_u32(memory, self, 0x005743bcu);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x18u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x1cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x20u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x24u,
        0xffffffffu);
    if (!memory->fault) game_memory_write_u32(memory, self, 0x005743c4u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x28u, source);
}

void game_initializer_effect_object_finish(GameMemory *memory,
        GameAddress self) {
    game_memory_write_u16(memory, self + 8u, 0x71u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x2cu, 0u);
    if (!memory->fault) game_memory_write_u16(memory, self + 4u, 1u);
}

void game_initializer_store_type0049_runtime(GameMemory *memory,
        uint32_t index, GameAddress object) {
    game_memory_write_u32(memory, 0x006020d8u + index * 4u, object);
}

void game_initializer_type0049ad(GameMemory *memory, GameAddress self,
        uint32_t first, uint16_t second) {
    game_memory_write_u16(memory, self + 0x114u, second);
    if (!memory->fault) game_memory_write_u32(memory, self, 0x00574288u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x120u, 1u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x110u, first);
    if (!memory->fault) game_initializer_pointer_ring(memory, self);
    if (!memory->fault) game_memory_write_u32(memory, self, 0x00574288u);
}

void game_initializer_reciprocal_table(GameMemory *memory,
        GameAddress table) {
    uint32_t index;
    game_memory_write_u32(memory, 0x00596268u, table);
    if (memory->fault || table == 0u) return;
    game_memory_write_u32(memory, table, 0u);
    for (index = 1u; index < 0x400u && !memory->fault; ++index)
        game_memory_write_u32(memory, table + index * 4u, 0x10000u / index);
}

void game_initializer_scratch_record(GameMemory *memory, uint32_t index,
        GameAddress record) {
    game_memory_write_u32(memory, 0x00629890u + index * 4u, record);
    if (!memory->fault && record != 0u)
        game_memory_write_u8(memory, record, 0u);
}

void game_initializer_scratch_records_finish(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00629888u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x006298a0u, 0u);
}

void game_initializer_shared_presentation_header(GameMemory *memory,
        GameAddress self) {
    GameAddress state = self + 0x46cu;
    game_memory_write_u32(memory, self + 0x460u, 0x0059c68cu);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x468u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, state, UINT32_MAX);
    if (!memory->fault) game_memory_write_u32(memory, state + 4u, UINT32_MAX);
    if (!memory->fault) game_memory_write_u32(memory, state + 8u, UINT32_MAX);
    if (!memory->fault) game_memory_write_u32(memory, state + 0x0cu,
        UINT32_MAX);
    if (!memory->fault) game_memory_write_u16(memory, state + 0x10u,
        UINT16_MAX);
}

GamePresentationDescriptorEntry game_initializer_presentation_descriptor_entry(
        GameMemory *memory, GameAddress self, unsigned index, int payload) {
    GamePresentationDescriptorEntry entry = {0};
    entry.slot = self + 0x0cu + index * 4u;
    entry.descriptor = 0x0059ac64u + index * 0x10u;
    entry.current = game_memory_read_u32(memory, entry.slot);
    if (!memory->fault)
        entry.type = game_memory_read_u8(memory, entry.descriptor);
    if (payload && !memory->fault) {
        entry.primary = game_memory_read_u32(memory, entry.descriptor - 0x0cu);
        entry.subtype = game_memory_read_u8(memory, entry.descriptor - 8u);
        entry.payload = game_memory_read_u32(memory, entry.descriptor - 4u);
    }
    return entry;
}

uint32_t game_initializer_shared_presentation_active(GameMemory *memory,
        GameAddress self) {
    return game_memory_read_u32(memory, self + 0x468u);
}

void game_initializer_store_presentation_descriptor(GameMemory *memory,
        const GamePresentationDescriptorEntry *entry, GameAddress object) {
    game_memory_write_u32(memory, entry->slot, object);
}

void game_initializer_publish_shared_presentation(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, 0x00628a90u, object);
    if (!memory->fault)
        game_memory_write_u32(memory, object + 8u, 0x3f800000u);
}

void game_initializer_set_shared_presentation_active(GameMemory *memory,
        GameAddress self, uint32_t active) {
    game_memory_write_u32(memory, self + 0x468u, active);
}

void game_initializer_store_shared_presentation_interface(GameMemory *memory,
        GameAddress self, GameAddress interface_object) {
    game_memory_write_u32(memory, self + 0x45cu, interface_object);
}

GamePresentationResourceBucket game_initializer_presentation_resource_bucket(
        GameMemory *memory, GameAddress self, unsigned index) {
    GamePresentationResourceBucket bucket = {0};
    GameAddress descriptor = 0x0059ac64u + index * 0x10u;
    bucket.slot = self + 0x0cu + index * 4u;
    bucket.bucket = game_memory_read_u32(memory, bucket.slot);
    if (!memory->fault) bucket.type = game_memory_read_u8(memory, descriptor);
    if (!memory->fault && bucket.bucket != 0u) {
        bucket.count = game_memory_read_u32(memory, bucket.bucket + 8u);
        if (!memory->fault)
            bucket.array = game_memory_read_u32(memory, bucket.bucket + 0x0cu);
    }
    return bucket;
}

GameAddress game_initializer_presentation_bucket_object(GameMemory *memory,
        const GamePresentationResourceBucket *bucket, unsigned index) {
    return game_memory_read_u32(memory, bucket->array + index * 4u);
}

void game_initializer_clear_presentation_bucket_object(GameMemory *memory,
        const GamePresentationResourceBucket *bucket, unsigned index) {
    game_memory_write_u32(memory, bucket->array + index * 4u, 0u);
}

void game_initializer_clear_presentation_bucket(GameMemory *memory,
        const GamePresentationResourceBucket *bucket) {
    game_memory_write_u32(memory, bucket->slot, 0u);
}

void game_initializer_type004793_finish(GameMemory *memory,
        GameAddress self) {
    game_memory_write_u32(memory, self, 0x00573d20u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x1dcu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x1fcu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self, 0x00573da8u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x2e0u, 0u);
}

void game_initializer_gameplay_presentation_controller(GameMemory *memory,
        GameAddress self) {
    static const uint16_t zero_offsets[] = {
        0x0cu, 0x10u, 0x14u, 0x18u, 0x1cu, 0x20u, 0x28u, 0x24u,
        0x2cu, 0x30u, 0x34u, 0x38u, 0x3cu, 0x78u, 0x7cu, 0x80u,
        0x88u, 0x8cu, 0x68u, 0x6cu, 0x70u, 0x74u, 0x84u
    };
    unsigned index;
    game_memory_write_u32(memory, self, 0x005730e0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 4u, 1u);
    if (!memory->fault) game_memory_write_u32(memory, self + 8u, 1u);
    for (index = 0u; index < sizeof(zero_offsets) / sizeof(zero_offsets[0]) &&
            !memory->fault; ++index)
        game_memory_write_u32(memory, self + zero_offsets[index], 0u);
}

void game_initializer_clear_gameplay_presentation_link(GameMemory *memory,
        GameAddress self) {
    game_memory_write_u32(memory, self + 0x40u, 0u);
}

GameAddress game_initializer_shared_presentation_resource(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00597f64u);
}

void game_initializer_store_shared_presentation_resource(GameMemory *memory,
        GameAddress resource) {
    game_memory_write_u32(memory, 0x00597f64u, resource);
}

GameAddress game_initializer_shared_presentation_source(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00627068u);
}

GameAddress game_initializer_shared_presentation_owner(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00626e78u);
}

GameAddress game_initializer_descriptor_resource_begin(GameMemory *memory,
        GameAddress self, GameAddress descriptor) {
    game_memory_write_u32(memory, self, 0x00576bc8u);
    if (!memory->fault) game_memory_write_u16(memory, self + 0x14u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 4u, descriptor);
    return memory->fault ? 0u :
        game_memory_read_u32(memory, descriptor + 0x54u);
}

void game_initializer_descriptor_resource_activate(GameMemory *memory,
        GameAddress self) {
    game_memory_write_u32(memory, self, 0x00576c68u);
    if (!memory->fault) game_memory_write_u16(memory, self + 8u, 0u);
}

void game_initializer_store_descriptor_resource_handle(GameMemory *memory,
        GameAddress self, GameAddress handle) {
    game_memory_write_u32(memory, self + 0x48u, handle);
}

GameAddress game_initializer_presentation_selector_record(GameMemory *memory,
        uint32_t selector) {
    return game_memory_read_u32(memory, 0x005adb68u + selector * 4u);
}

GameAddress game_initializer_presentation_descriptor_payload(
        GameMemory *memory, GameAddress descriptor) {
    return game_memory_read_u32(memory, descriptor + 0x1dcu);
}

void game_initializer_store_presentation_object(GameMemory *memory,
        GameAddress self, GameAddress object) {
    game_memory_write_u32(memory, self + 0x21cu, object);
}

void game_initializer_resource_stream_begin(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00585334u, 0u);
}

GameAddress game_initializer_resource_stream(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00585328u);
}

GameAddress game_initializer_resource_stream_path(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00585330u);
}

void game_initializer_resource_stream_opening(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00585334u, 1u);
}

void game_initializer_store_resource_stream(GameMemory *memory,
        GameAddress stream) {
    game_memory_write_u32(memory, 0x00585328u, stream);
}

void game_initializer_store_resource_record_size(GameMemory *memory,
        GameAddress destination, uint32_t size) {
    game_memory_write_u32(memory, destination, size);
}

void game_initializer_named_record_begin(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x18u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x48u, 1u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x14u, 0u);
}

void game_initializer_store_named_record_label(GameMemory *memory,
        GameAddress object, GameAddress label) {
    game_memory_write_u32(memory, object + 0x30u, label);
}

void game_initializer_named_record_finish(GameMemory *memory,
        GameAddress object) {
    static const uint32_t offsets[] = {
        0x20u, 0x24u, 0x28u, 0x2cu, 0x34u, 0x38u,
        0x3cu, 0x40u, 0x44u
    };
    unsigned index;
    for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]) &&
            !memory->fault; ++index)
        game_memory_write_u32(memory, object + offsets[index], 0u);
}

void game_initializer_gameplay_service_tables(GameMemory *memory,
        int backwards) {
    static const GameAddress starts[3] = {
        0x00602f78u, 0x00602fd4u, 0x00602fa0u
    };
    static const unsigned counts[3] = {10u, 17u, 13u};
    unsigned group;
    for (group = 0u; group < 3u && !memory->fault; ++group) {
        unsigned index;
        for (index = 0u; index < counts[group] && !memory->fault; ++index)
            game_memory_write_u32(memory, starts[group] +
                (backwards ? (uint32_t)(0u - index * 4u) : index * 4u),
                0xffffffbbu);
    }
}
