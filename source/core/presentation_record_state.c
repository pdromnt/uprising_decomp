#include "presentation_record_state.h"

static int write_field(GameMemory *memory, GameAddress owner,
    uint32_t offset, uint32_t value) {
    game_memory_write_u32(memory, owner + offset, value);
    return !memory->fault;
}

static uint32_t read_field(GameMemory *memory, GameAddress address,
    uint32_t offset) {
    return game_memory_read_u32(memory, address + offset);
}

int game_presentation_publish_record(GameMemory *memory, GameAddress owner,
    GamePresentationRecordResult *result) {
    uint32_t nested;
    if (!memory || !result) return 0;
    result->record = read_field(memory, owner, 0xdcu);
    if (memory->fault || !write_field(memory, owner, 0xf0u, 0u) ||
        !write_field(memory, owner, 0xc8u, 0u) ||
        !write_field(memory, owner, 0xe0u, 5u)) return 0;
    result->type = read_field(memory, result->record, 0x0cu);
    if (memory->fault) return 0;
    result->primary = result->type;
    result->secondary = result->record;
    result->flags = GAME_PRESENTATION_RECORD_COMPARE;
    result->add_left = result->add_right = 0u;

    switch (result->type) {
        case 0u:
            result->primary = read_field(memory, result->record, 0x18u);
            if (memory->fault || !write_field(memory, owner, 0xe4u,
                    result->primary)) return 0;
            nested = read_field(memory, result->record, 0x1cu);
            if (memory->fault || !write_field(memory, owner, 0xe8u,
                    nested)) return 0;
            result->secondary = read_field(memory, result->record, 0x20u);
            if (memory->fault || !write_field(memory, owner, 0xecu,
                    result->secondary)) return 0;
            result->flags = GAME_PRESENTATION_RECORD_ADD;
            result->add_left = result->record;
            result->add_right = 0x18u;
            break;
        case 1u:
        case 8u:
            nested = read_field(memory, result->record, 0x10u);
            if (memory->fault || !write_field(memory, owner, 0xf0u,
                    nested)) return 0;
            result->secondary = read_field(memory, result->record, 0x14u);
            if (memory->fault || !write_field(memory, owner, 0xf4u,
                    result->secondary)) return 0;
            result->secondary = read_field(memory, nested, 0x18u);
            if (memory->fault || !write_field(memory, owner, 0xe4u,
                    result->secondary)) return 0;
            result->primary = read_field(memory, nested, 0x1cu);
            if (memory->fault || !write_field(memory, owner, 0xe8u,
                    result->primary)) return 0;
            result->secondary = read_field(memory, nested, 0x20u);
            if (memory->fault || !write_field(memory, owner, 0xecu,
                    result->secondary)) return 0;
            result->flags = GAME_PRESENTATION_RECORD_ADD;
            result->add_left = nested;
            result->add_right = 0x18u;
            break;
        case 3u: case 4u: case 5u: case 6u: case 7u:
            result->primary = read_field(memory, result->record, 0x10u);
            if (memory->fault || !write_field(memory, owner, 0xf0u,
                    result->primary)) return 0;
            result->secondary = read_field(memory, result->record, 0x14u);
            if (memory->fault || !write_field(memory, owner, 0xf4u,
                    result->secondary)) return 0;
            break;
        case 9u:
            result->add_left = read_field(memory, result->record, 0x24u);
            if (memory->fault) return 0;
            result->add_right = game_memory_read_u32(memory, 0x005ac6acu);
            if (memory->fault) return 0;
            result->primary = result->add_left + result->add_right;
            result->flags = GAME_PRESENTATION_RECORD_ADD;
            if (!write_field(memory, owner, 0xd0u, result->primary)) return 0;
            break;
        case 11u:
            result->primary = 0u;
            result->flags = GAME_PRESENTATION_RECORD_ZERO;
            if (!write_field(memory, owner, 0x130u, 0u) ||
                !write_field(memory, owner, 0x134u, 0u)) return 0;
            break;
        case 12u:
            result->primary = read_field(memory, result->record, 0x10u);
            if (memory->fault || !write_field(memory, owner, 0x138u,
                    result->primary)) return 0;
            result->primary = 0u;
            result->flags = GAME_PRESENTATION_RECORD_ZERO;
            if (!write_field(memory, owner, 0x134u, 0u) ||
                !write_field(memory, owner, 0x130u, 0u)) return 0;
            break;
        case 13u:
            result->primary = read_field(memory, result->record, 0x10u);
            if (memory->fault || !write_field(memory, owner, 0xc8u,
                    result->primary)) return 0;
            result->secondary = read_field(memory, result->record, 0x14u);
            if (memory->fault || !write_field(memory, owner, 0xccu,
                    result->secondary) ||
                !write_field(memory, owner, 0x104u, 0u)) return 0;
            break;
        case 14u:
            if (!write_field(memory, owner, 0x104u, 0u)) return 0;
            break;
        default:
            break;
    }
    return 1;
}
