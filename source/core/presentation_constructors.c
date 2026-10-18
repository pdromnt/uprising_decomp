#include "presentation_constructors.h"

typedef struct InitialField {
    uint16_t offset;
    uint32_t value;
} InitialField;

static int write_fields(GameMemory *memory, GameAddress owner,
        const InitialField *fields, unsigned count) {
    unsigned i;
    if (!memory) return 0;
    for (i = 0u; i < count; ++i) {
        game_memory_write_u32(memory, owner + fields[i].offset,
            fields[i].value);
        if (memory->fault) return 0;
    }
    return 1;
}

int game_presentation_layout_initialize(GameMemory *memory,
        GameAddress owner) {
    static const InitialField fields[] = {
        {0u, 0x00571c58u}, {0xb4u, 0u},
        {0x13cu, 0u}, {0x140u, 0u}
    };
    return write_fields(memory, owner, fields,
        sizeof(fields) / sizeof(fields[0]));
}

int game_presentation_layout_bind(GameMemory *memory,
        GameAddress owner, uint32_t resource) {
    static const InitialField fields[] = {
        {0x164u, 0u}, {0xb0u, 100u}
    };
    if (!write_fields(memory, owner, fields,
            sizeof(fields) / sizeof(fields[0]))) return 0;
    game_memory_write_u32(memory, owner + 0x168u, resource);
    return !memory->fault;
}

int game_presentation_node_initialize(GameMemory *memory,
        GameAddress frame, GameAddress owner) {
    static const InitialField first[] = {
        {0x44u, 0u}, {0x14u, 0u}, {0x48u, 0u},
        {0x50u, 0xffffffffu}, {0x54u, 0xffffffffu}, {0x4cu, 0u}
    };
    static const InitialField rest[] = {
        {0x68u, 0u}, {0x6cu, 0u}, {0x70u, 0u}, {0x74u, 0u},
        {0x7cu, 0xffffffffu}, {0x80u, 0xffffffffu},
        {0x84u, 0xffffffffu}, {0x8cu, 0u}, {0x60u, 0u}, {0x5cu, 0u}
    };
    if (!memory) return 0;
    game_memory_write_u32(memory, frame - 4u, 0u);
    if (memory->fault || !write_fields(memory, owner, first,
            sizeof(first) / sizeof(first[0]))) return 0;
    game_memory_write_u32(memory, frame - 4u, 1u);
    if (memory->fault) return 0;
    return write_fields(memory, owner, rest,
        sizeof(rest) / sizeof(rest[0]));
}

int game_presentation_node_clear_tail(GameMemory *memory,
        GameAddress owner) {
    static const InitialField fields[] = {
        {0x58u, 0u}, {0x90u, 0u}, {0x78u, 0u}
    };
    return write_fields(memory, owner, fields,
        sizeof(fields) / sizeof(fields[0]));
}

int game_named_surface_initialize(GameMemory *memory,
        GameAddress owner) {
    static const InitialField fields[] = {
        {0u, 0x00571c40u}, {0x50u, 0u}, {0xd4u, 0u}
    };
    return write_fields(memory, owner, fields,
        sizeof(fields) / sizeof(fields[0]));
}

int game_named_surface_configure(GameMemory *memory,
        GameAddress owner, uint32_t width, uint32_t height) {
    if (!memory) return 0;
    game_memory_write_u32(memory, owner + 0x48u, 0u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, owner + 0xe0u, width);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, owner + 0xe4u, height);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, owner + 0x44u, 0u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, owner + 0xd8u, 0u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, owner + 0xdcu, 0u);
    return !memory->fault;
}
