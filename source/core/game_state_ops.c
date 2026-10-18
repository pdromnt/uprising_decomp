#include "game_state_ops.h"

#include <string.h>

static int32_t clamp_i32(int32_t value, int32_t minimum, int32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

void game_state_initialize_word_table(GameMemory *memory, GameAddress object) {
    unsigned index;
    game_memory_write_u32(memory, object, 0u);
    for (index = 0u; index < 256u && !memory->fault; ++index)
        game_memory_write_u32(memory, object + 4u + index * 4u, 0u);
}

uint32_t game_state_accumulate_field(GameMemory *memory, GameAddress object,
    uint32_t source_offset, uint32_t destination_offset, uint32_t amount) {
    uint32_t result = game_memory_read_u32(memory, object + source_offset) + amount;
    if (!memory->fault)
        game_memory_write_u32(memory, object + destination_offset, result);
    return result;
}

void game_state_reset_slot_tables(GameMemory *memory) {
    GameAddress address;
    for (address = 0x005e6dd4u; address < 0x005e6e28u; address += 4u) {
        game_memory_write_u32(memory, address, 0xfffffffeu);
        if (!memory->fault)
            game_memory_write_u32(memory, address + 4u - 0x58u, 0xfffffffeu);
        if (memory->fault) return;
    }
    game_memory_write_u32(memory, 0x005e6e28u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005e6e2cu, 0u);
}

void game_state_copy_48(GameMemory *memory, GameAddress destination,
    GameAddress source) {
    unsigned index;
    for (index = 0u; index < 12u && !memory->fault; ++index) {
        uint32_t value = game_memory_read_u32(memory, source + index * 4u);
        if (!memory->fault)
            game_memory_write_u32(memory, destination + index * 4u, value);
    }
}

void game_state_reset_runtime_vectors(GameMemory *memory) {
    static const GameAddress source[] = {0x00597010u, 0x00597014u, 0x00597018u, 0x0059701cu};
    static const GameAddress destination[] = {0x00624f30u, 0x00624f44u, 0x00624f00u, 0x00624f40u};
    static const GameAddress cleared[] = {0x00624f18u, 0x00624efcu, 0x00624f2cu};
    unsigned index;
    for (index = 0u; index < 4u && !memory->fault; ++index) {
        uint32_t value = game_memory_read_u32(memory, source[index]);
        if (!memory->fault) game_memory_write_u32(memory, destination[index], value);
    }
    for (index = 0u; index < 3u && !memory->fault; ++index)
        game_memory_write_u32(memory, cleared[index], 0u);
}

GameRectangleBounds game_state_clamp_rectangle(GameRectangleBounds input,
    int32_t minimum_x, int32_t maximum_x, int32_t minimum_y, int32_t maximum_y) {
    GameRectangleBounds result;
    result.left = clamp_i32(input.left, minimum_x, maximum_x);
    result.top = clamp_i32(input.top, minimum_y, maximum_y);
    result.right = clamp_i32(input.right, minimum_x, maximum_x);
    result.bottom = clamp_i32(input.bottom, minimum_y, maximum_y);
    return result;
}

int game_state_assign_related(GameMemory *memory, GameAddress owner,
    GameAddress candidate, uint32_t destination_offset) {
    if (candidate != 0u) {
        uint32_t candidate_group = game_memory_read_u32(memory, candidate + 0x14u);
        uint32_t owner_group;
        if (memory->fault || candidate_group == 0u) return 0;
        owner_group = game_memory_read_u32(memory, owner + 0x14u);
        if (memory->fault || ((owner_group ^ candidate_group) & 0x3fu) != 0u) return 0;
    }
    game_memory_write_u32(memory, owner + destination_offset, candidate);
    return !memory->fault;
}

void game_state_initialize_descriptor(GameMemory *memory, GameAddress object) {
    uint32_t offset;
    game_memory_write_u32(memory, object, 0x00577a38u);
    for (offset = 0x14u; offset <= 0x2cu && !memory->fault; offset += 4u)
        game_memory_write_u32(memory, object + offset, 0u);
}

uint32_t game_state_resolve_event_category(GameMemory *memory, uint32_t identifier) {
    unsigned index;
    uint32_t category = 22u;
    game_memory_write_u32(memory, 0x005a1c60u, identifier);
    for (index = 0u; index < 45u && !memory->fault; ++index) {
        if (game_memory_read_u32(memory, 0x005a2398u + index * 8u) == identifier) {
            category = game_memory_read_u32(memory,
                0x005a239cu + index * 8u);
            goto resolved;
        }
    }
    if (identifier >= 0x13u && identifier <= 0x24u) category = 13u;
    else if (identifier >= 0xbcu && identifier <= 0xcau) category = 8u;
resolved:
    if (!memory->fault)
        game_memory_write_u32(memory, 0x005a1c5cu, category);
    return category;
}

int game_state_bitset_suffix_clear(GameMemory *memory, GameAddress bitset,
    int32_t bit_index) {
    int32_t word_index = bit_index / 32;
    int32_t remainder = bit_index % 32;
    uint32_t shift = (uint32_t)(31 - remainder) & 31u;
    uint32_t mask = ~((uint32_t)-1 << shift);
    int32_t index;
    if ((game_memory_read_u32(memory, bitset + (uint32_t)word_index * 4u) & mask) != 0u) return 0;
    for (index = word_index + 1; index < 3 && !memory->fault; ++index)
        if (game_memory_read_u32(memory, bitset + (uint32_t)index * 4u) != 0u) return 0;
    return !memory->fault;
}

int game_state_timed_child_ready(GameMemory *memory, GameAddress object) {
    int32_t active = (int32_t)game_memory_read_u32(memory, object + 0x58u);
    GameAddress child;
    uint32_t duration;
    uint32_t elapsed;
    uint32_t limit;
    if (memory->fault || active <= 0) return 0;
    child = game_memory_read_u32(memory, object + 0x64u);
    duration = game_memory_read_u32(memory, child + 0x24u);
    if (memory->fault || duration == 0u) return 0;
    elapsed = game_memory_read_u32(memory, object + 0x68u);
    limit = game_memory_read_u32(memory, 0x005ac6acu);
    return !memory->fault && (int32_t)(elapsed + duration) <= (int32_t)limit;
}

GameAssetSelection game_state_asset_selection(uint8_t type) {
    GameAssetSelection result = {0u, 0u, 1};
    switch (type) {
        case 2u: result.field_offset = 0x214u; result.table_base = 0x005e6e30u; break;
        case 9u: result.field_offset = 0x1fcu; result.table_base = 0x005e8380u; break;
        case 12u: result.field_offset = 0x208u; result.table_base = 0x005e4d20u; break;
        default: result.valid = 0; break;
    }
    return result;
}

GameAssetSelection game_state_secondary_asset_selection(uint8_t type, uint8_t subtype) {
    GameAssetSelection result = {0u, 0u, 1};
    switch (type) {
        case 2u: result.field_offset = 0x2b4u; result.table_base = 0x005ecba8u; break;
        case 9u:
            result.field_offset = subtype == 0x20u ? 0x2dcu : 0x29cu;
            result.table_base = subtype == 0x20u ? 0x005ee608u : 0x005e67e0u;
            break;
        case 12u: result.field_offset = 0x2a8u; result.table_base = 0x005e1cb8u; break;
        default: result.valid = 0; break;
    }
    return result;
}

uint32_t game_state_asset_record_index(uint32_t field, uint32_t caller_index) {
    return caller_index + (field * 7u) * 4u;
}

uint32_t game_state_asset_owner_field(GameMemory *memory, GameAddress owner,
    GameAssetSelection selection) {
    return game_memory_read_u32(memory, owner + selection.field_offset);
}

uint32_t game_state_asset_record(GameMemory *memory,
    GameAssetSelection selection, uint32_t field, uint32_t caller_index) {
    uint32_t record_index = game_state_asset_record_index(field, caller_index);
    return game_memory_read_u32(memory,
        selection.table_base + record_index * 4u);
}

uint32_t game_state_read_global(GameMemory *memory, GameAddress address) {
    return game_memory_read_u32(memory, address);
}

void game_state_write_global(GameMemory *memory, GameAddress address,
    uint32_t value) {
    game_memory_write_u32(memory, address, value);
}

void game_state_copy_global_pair(GameMemory *memory, GameAddress first_source,
    GameAddress second_source, GameAddress first_output,
    GameAddress second_output) {
    uint32_t first = game_memory_read_u32(memory, first_source);
    uint32_t second = game_memory_read_u32(memory, second_source);
    if (!memory->fault) game_memory_write_u32(memory, first_output, first);
    if (!memory->fault) game_memory_write_u32(memory, second_output, second);
}

static int occupancy_cell_clear(GameMemory *memory, GameAddress object,
        uint32_t x, uint32_t y) {
    uint8_t bits = game_memory_read_u8(memory,
        object + y * 32u + (x >> 3u));
    return !memory->fault &&
        (bits & (uint8_t)(1u << (x & 7u))) == 0u;
}

int game_state_occupancy_query(GameMemory *memory, GameAddress object,
    uint32_t original_x, uint32_t original_y, uint32_t x_offset,
    uint32_t y_offset) {
    uint32_t x = original_x + x_offset;
    uint32_t y = original_y + y_offset;
    int32_t minimum_x = (int32_t)game_memory_read_u32(memory, object + 0x200cu);
    int32_t minimum_y = (int32_t)game_memory_read_u32(memory, object + 0x2010u);
    int32_t maximum_y = (int32_t)game_memory_read_u32(memory, object + 0x2014u);
    int32_t maximum_x = (int32_t)game_memory_read_u32(memory, object + 0x2018u);
    if (memory->fault || (int32_t)x < minimum_x || (int32_t)x >= maximum_x ||
        (int32_t)y < minimum_y || (int32_t)y >= maximum_y)
        return 0;
    if (!occupancy_cell_clear(memory, object, x, y)) return 0;
    if (x_offset == 0u || y_offset == 0u) return 1;
    if (!occupancy_cell_clear(memory, object, x, original_y)) return 0;
    return occupancy_cell_clear(memory, object, original_x, y);
}

static uint32_t occupancy_window_minimum(uint32_t first, uint32_t second) {
    int32_t minimum = (int32_t)((first + second + 1u) >> 1u) - 32;
    if (minimum < 0) return 0u;
    if (minimum >= 192) return 192u;
    return (uint32_t)minimum;
}

GameOccupancyWindow game_state_set_occupancy_window(GameMemory *memory,
    GameAddress object, uint32_t first_x, uint32_t first_y,
    uint32_t second_x, uint32_t second_y) {
    GameOccupancyWindow result;
    result.minimum_x = occupancy_window_minimum(first_x, second_x);
    result.minimum_y = occupancy_window_minimum(first_y, second_y);
    result.maximum_y = result.minimum_y + 64u;
    result.maximum_x = result.minimum_x + 64u;
    game_memory_write_u32(memory, object + 0x200cu, result.minimum_x);
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x2010u, result.minimum_y);
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x2014u, result.maximum_y);
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x2018u, result.maximum_x);
    return result;
}

uint32_t game_state_grid_mask(GameMemory *memory, GameAddress object,
    int32_t x, int32_t y) {
    if (x < 0 || x >= 256 || y < 0 || y >= 256) return 1u;
    return game_memory_read_u8(memory, object + 0x6058u +
        (uint32_t)y * 32u + (uint32_t)x / 8u) &
        (1u << ((uint32_t)x & 7u));
}

void game_state_write_fields(GameMemory *memory, GameAddress object,
    const uint32_t *offsets, const uint32_t *values, unsigned count) {
    unsigned index;
    for (index = 0u; index < count && !memory->fault; ++index)
        game_memory_write_u32(memory, object + offsets[index], values[index]);
}

uint32_t game_state_scaled_indexed_value(uint32_t index, uint32_t value,
    uint32_t scaled_mask) {
    return index < 32u && (scaled_mask & (1u << index)) ? value * 10u : value;
}

void game_state_reset_pair_array(GameMemory *memory, GameAddress object,
    unsigned count, uint32_t second_offset) {
    unsigned index;
    for (index = 0u; index < count && !memory->fault; ++index) {
        game_memory_write_u32(memory, object + index * 4u, 0xfffffffeu);
        if (!memory->fault)
            game_memory_write_u32(memory, object + second_offset +
                index * 4u, 0xfffffffeu);
    }
    if (!memory->fault) game_memory_write_u32(memory, object + count * 8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + count * 8u + 4u, 0u);
}

void game_state_apply_default_pair_array(GameMemory *memory,
    GameAddress object, GameAddress source, unsigned count,
    uint32_t second_offset, GameAddress default_source) {
    unsigned index;
    for (index = 0u; index < count && !memory->fault; ++index) {
        GameAddress entry = object + index * 4u;
        uint32_t first = game_memory_read_u32(memory, entry);
        if (first == 0xfffffffeu) {
            first = game_memory_read_u32(memory,
                (source ? source : default_source) + index * 4u);
            if (!memory->fault) game_memory_write_u32(memory, entry, first);
        }
        if (!memory->fault && game_memory_read_u32(memory,
                entry + second_offset) == 0xfffffffeu)
            game_memory_write_u32(memory, entry + second_offset, first);
    }
}

uint32_t game_state_next_owner_sequence(GameMemory *memory,
    GameAddress object) {
    uint32_t sequence = game_memory_read_u32(memory, object + 0x18u);
    uint16_t owner = game_memory_read_u16(memory, object + 8u);
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x18u, sequence + 1u);
    return (uint32_t)owner | (sequence << 6u);
}

int game_state_append_bounded(GameMemory *memory, GameAddress object,
    uint32_t count_offset, uint32_t values_offset, unsigned stride,
    unsigned capacity, uint32_t value, uint32_t *old_count) {
    uint32_t count = game_memory_read_u32(memory, object + count_offset);
    if (old_count) *old_count = count;
    if (memory->fault || count >= capacity) return 0;
    game_memory_write_u32(memory, object + values_offset + count * stride, value);
    if (!memory->fault)
        game_memory_write_u32(memory, object + count_offset, count + 1u);
    return !memory->fault;
}

uint32_t game_state_bounce_counter(GameMemory *memory, GameAddress total,
    GameAddress direction, uint32_t maximum) {
    uint32_t step = game_memory_read_u32(memory, direction);
    uint32_t value = game_memory_read_u32(memory, total) + step;
    if (!memory->fault) game_memory_write_u32(memory, total, value);
    if (!memory->fault && value == maximum)
        game_memory_write_u32(memory, direction, 0xffffffffu);
    else if (!memory->fault && value == 0u)
        game_memory_write_u32(memory, direction, 1u);
    return value;
}

uint32_t game_state_initialize_texture_cache(GameMemory *memory,
    int fullscreen) {
    if (fullscreen) {
        game_memory_write_u32(memory, 0x005ad388u, 0u);
        game_memory_write_u32(memory, 0x005ad38cu, 0u);
        game_memory_write_u8(memory, 0x005ad39bu, 5u);
        game_memory_write_u32(memory, 0x005ad39cu, 0x005acf48u);
        game_memory_write_u32(memory, 0x005ad394u, 0x400u);
        game_memory_write_u8(memory, 0x005ad398u, 3u);
        game_memory_write_u8(memory, 0x005ad399u, 3u);
        game_memory_write_u8(memory, 0x005ad39au, 3u);
        return 3u;
    }
    game_memory_write_u8(memory, 0x005ad7c2u, 3u);
    game_memory_write_u8(memory, 0x005ad7c3u, 5u);
    game_memory_write_u32(memory, 0x005ad7bcu, 0x4000u);
    game_memory_write_u32(memory, 0x005ad7b0u, 0u);
    game_memory_write_u32(memory, 0x005ad7b4u, 0u);
    game_memory_write_u8(memory, 0x005ad7c0u, 1u);
    game_memory_write_u8(memory, 0x005ad7c1u, 1u);
    game_memory_write_u32(memory, 0x005ad7acu, 0x80000000u);
    game_memory_write_u32(memory, 0x005ad7a8u, 0x80000000u);
    return 0x80000000u;
}

void game_state_add_pair(GameMemory *memory, GameAddress first_address,
    GameAddress second_address, uint32_t first, uint32_t second) {
    uint32_t old_first = game_memory_read_u32(memory, first_address);
    uint32_t old_second = game_memory_read_u32(memory, second_address);
    if (!memory->fault) game_memory_write_u32(memory, first_address, old_first + first);
    if (!memory->fault) game_memory_write_u32(memory, second_address, old_second + second);
}

void game_state_write_byte(GameMemory *memory, GameAddress address, uint8_t value) {
    game_memory_write_u8(memory, address, value);
}

uint32_t game_state_read_indexed(GameMemory *memory, GameAddress table,
    uint32_t index) {
    return game_memory_read_u32(memory, table + index * 4u);
}

void game_state_add_word_pair(GameMemory *memory, GameAddress first_address,
    GameAddress second_address, uint16_t first, uint16_t second) {
    uint16_t old_first = game_memory_read_u16(memory, first_address);
    uint16_t old_second = game_memory_read_u16(memory, second_address);
    if (!memory->fault)
        game_memory_write_u16(memory, first_address, (uint16_t)(old_first + first));
    if (!memory->fault)
        game_memory_write_u16(memory, second_address, (uint16_t)(old_second + second));
}

float game_state_approximate_distance(float first_x, float first_y,
    float second_x, float second_y, float half) {
    float dx = (float)((long double)first_x - (long double)second_x);
    float dy = (float)((long double)first_y - (long double)second_y);
    if (dx < 0.0f) dx = -dx;
    if (dy < 0.0f) dy = -dy;
    if (dy < dx)
        return (float)((long double)dy * (long double)half +
            (long double)dx);
    return (float)((long double)dx * (long double)half +
        (long double)dy);
}

static void write_float(GameMemory *memory, GameAddress address, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    game_memory_write_u32(memory, address, bits);
}

void game_state_configure_record(GameMemory *memory, GameAddress object,
    const GameStateRecordConfig *config) {
    game_memory_write_u8(memory, object + 0x0cu, config->type);
    game_memory_write_u32(memory, object + 0x10u, config->second);
    game_memory_write_u32(memory, object + 0x14u, config->third);
    write_float(memory, object + 0x1cu, (float)config->fourth);
    game_memory_write_u32(memory, object + 0x20u, config->fifth);
    game_memory_write_u32(memory, object + 0x24u, config->sixth);
    game_memory_write_u32(memory, object + 0x18u, config->seventh);
    game_memory_write_u32(memory, object + 0x28u, config->eighth);
    game_memory_write_u16(memory, object + 0x2cu, config->ninth);
    game_memory_write_u32(memory, object + 0x30u, config->tenth);
    game_memory_write_u32(memory, object + 0x34u, config->eleventh);
    game_memory_write_u16(memory, object + 0x38u, config->twelfth);
    write_float(memory, object + 0x3cu, (float)config->thirteenth);
    game_memory_write_u32(memory, object + 0x40u, config->fourteenth);
}

int game_state_append_typed_record(GameMemory *memory, GameAddress object,
    uint8_t type, uint32_t value, uint32_t *old_count) {
    uint32_t count = game_memory_read_u32(memory, object + 0x1c8u);
    if (old_count) *old_count = count;
    if (memory->fault || count + 1u >= 10u) return 0;
    game_memory_write_u8(memory, object + 0x100u + count * 20u, type);
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x104u + count * 20u, value);
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x1c8u, count + 1u);
    return !memory->fault;
}

GameStateStringSearchResult game_state_find_string(GameMemory *memory, GameAddress table,
    int32_t count, GameAddress query) {
    GameStateStringSearchResult result = {0xffffffffu, 0};
    int32_t index;
    for (index = 0; index < count && !memory->fault; ++index) {
        GameAddress left = game_memory_read_u32(memory,
            table + (uint32_t)index * 4u);
        GameAddress right = query;
        for (;;) {
            uint8_t left_byte = game_memory_read_u8(memory, left++);
            uint8_t right_byte = game_memory_read_u8(memory, right++);
            if (memory->fault) return result;
            if (left_byte != right_byte) {
                result.comparison = left_byte < right_byte ? -1 : 1;
                break;
            }
            if (left_byte == 0u) {
                result.index = (uint32_t)index;
                result.comparison = 0;
                return result;
            }
        }
    }
    return result;
}

GameStateCallbackBinding game_state_bind_callbacks(GameMemory *memory,
    GameAddress object, uint32_t first, uint32_t second,
    GameAddress enabled_first, GameAddress enabled_second) {
    GameStateCallbackBinding result;
    result.enabled = game_memory_read_u32(memory, object + 0xccu);
    result.disabled = game_memory_read_u32(memory, object + 0xd0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0xfcu, first);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x100u, second);
    if (!memory->fault)
        game_memory_write_u32(memory, result.enabled + 0x98u, enabled_first);
    if (!memory->fault)
        game_memory_write_u32(memory, result.enabled + 0x9cu, enabled_second);
    if (!memory->fault)
        game_memory_write_u32(memory, result.disabled + 0x98u, 0u);
    if (!memory->fault)
        game_memory_write_u32(memory, result.disabled + 0x9cu, 0u);
    return result;
}

void game_state_initialize_transform(GameMemory *memory, GameAddress object,
    GameAddress vtable) {
    static const uint32_t values[14] = {
        0x3f800000u, 0u, 0u, 0u, 0u,
        0x3f800000u, 0u, 0u, 0u, 0u,
        0x3f800000u, 0u, 0u, 0u
    };
    unsigned index;
    game_memory_write_u32(memory, object, vtable);
    for (index = 0u; index < 14u && !memory->fault; ++index)
        game_memory_write_u32(memory, object + 4u + index * 4u,
            values[index]);
    if (!memory->fault) game_memory_write_u8(memory, object + 0x3cu, 0u);
}

GameSelectorAdvanceResult game_state_advance_selector(GameMemory *memory,
    GameAddress object) {
    GameSelectorAdvanceResult result;
    uint32_t selector = game_memory_read_u32(memory, object + 0x1c4u);
    result.mask = game_memory_read_u32(memory, object + 0x1a4u);
    selector++;
    result.final_eax = selector;
    if (selector == 14u) selector = 0u;
    while (!memory->fault && result.mask != 0u &&
            (result.mask & (1u << selector)) == 0u) {
        selector++;
        result.final_eax = selector;
        if (selector == 14u) selector = 0u;
    }
    result.selector = selector;
    if (result.mask != 0u) result.final_eax = 1u << selector;
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x1c4u, selector);
    return result;
}

static void write_span(GameMemory *memory, GameAddress address,
    uint32_t first, uint32_t second, uint32_t width, uint32_t fourth) {
    game_memory_write_u32(memory, address, first);
    game_memory_write_u32(memory, address + 4u, second);
    game_memory_write_u32(memory, address + 8u, first + width);
    game_memory_write_u32(memory, address + 12u, fourth);
}

void game_state_build_span_table(GameMemory *memory, int primary_mode,
    int secondary_mode) {
    GameAddress address = 0x00603fa8u;
    uint32_t first, step, second, width, fourth;
    unsigned count, index;
    if (!primary_mode) {
        first = 0x60u; step = 0x1au; second = 3u;
        width = 0x16u; fourth = 9u; count = 5u;
    } else if (!secondary_mode) {
        first = 0xb6u; step = 0x22u; second = 0x19u;
        width = 0x1eu; fourth = 0x1fu; count = 2u;
        for (index = 0u; index < count && !memory->fault; ++index) {
            write_span(memory, address, first, second, width, fourth);
            address += 0x10u; first += step;
        }
        first = 0x168u; count = 3u;
    } else {
        first = 0x59u; step = 0x26u; second = 0x1fu;
        width = 0x20u; fourth = 0x25u; count = 5u;
    }
    for (index = 0u; index < count && !memory->fault; ++index) {
        write_span(memory, address, first, second, width, fourth);
        address += 0x10u; first += step;
    }
}

void game_state_initialize_gameplay_grid(GameMemory *memory,
    GameAddress object, uint32_t mode, int wide_variant,
    const uint32_t mode_seven_bounds[4]) {
    static const uint32_t narrow_bounds[4] = {0x78u, 0x96u, 0xd0u, 0x9du};
    static const uint32_t wide_bounds[4] = {0x1e0u, 0x15cu, 0x26au, 0x164u};
    static const uint32_t mode_two_bounds[4] = {0x67u, 0x0bu, 0xd8u, 0x11u};
    const uint32_t *bounds = mode == 7u ? mode_seven_bounds :
        mode == 2u ? mode_two_bounds :
        wide_variant ? wide_bounds : narrow_bounds;
    unsigned index;
    for (index = 0u; index < 4u && !memory->fault; ++index)
        game_memory_write_u32(memory, object + 0x534u + index * 4u,
            bounds[index]);
    for (index = 0u; index < 3u && !memory->fault; ++index) {
        game_memory_write_u32(memory, object + 0x518u + index * 4u, 0u);
        game_memory_write_u32(memory, object + 0x528u + index * 4u, 0u);
        game_memory_write_u32(memory, object + 0x500u + index * 4u, 0u);
        game_memory_write_u32(memory, object + 0x50cu + index * 4u, 0u);
    }
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x524u, bounds[3]);
    for (index = 0u; index < 9u && !memory->fault; ++index)
        game_memory_write_u8(memory, object + index * 0x80u, 0u);
}

void game_state_initialize_gameplay_order(GameMemory *memory,
    GameAddress object) {
    static const uint8_t order[98] = {
        0x05,0x03,0x02,0x01,0x06,0x04,0x00,0x03,
        0x02,0x01,0x07,0x04,0x05,0x00,0x03,0x02,
        0x02,0x04,0x05,0x00,0x00,0x03,0x02,0x04,
        0x01,0x05,0x00,0x00,0x07,0x01,0x04,0x02,
        0x03,0x05,0x00,0x02,0x01,0x07,0x05,0x03,
        0x04,0x00,0x02,0x03,0x04,0x07,0x01,0x05,
        0x00,0x06,0x03,0x02,0x05,0x01,0x00,0x00,
        0x06,0x03,0x02,0x05,0x01,0x00,0x00,0x04,
        0x03,0x02,0x05,0x01,0x07,0x00,0x02,0x05,
        0x03,0x01,0x04,0x00,0x00,0x07,0x01,0x04,
        0x02,0x03,0x05,0x00,0x07,0x01,0x04,0x02,
        0x03,0x05,0x00,0x07,0x01,0x04,0x02,0x03,
        0x05,0x00
    };
    static const uint32_t copied_offsets[4] = {0x4b4u,0x4b8u,0x4bcu,0x4c0u};
    unsigned index;
    for (index = 0u; index < 4u && !memory->fault; ++index) {
        uint32_t value = game_memory_read_u32(memory,
            object + copied_offsets[index]);
        if (!memory->fault)
            game_memory_write_u32(memory, object + 0x4ccu + index * 4u,
                value);
    }
    game_memory_write_u32(memory, object + 0x4e8u, 1u);
    game_memory_write_u32(memory, object + 0x4e4u, 0u);
    game_memory_write_u32(memory, object + 0x4e0u, 0u);
    game_memory_write_u32(memory, object + 0x4dcu, 0u);
    game_memory_write_u32(memory, object + 0x4ecu, 1u);
    game_memory_write_u32(memory, object + 0x4f0u, 1u);
    game_memory_write_u32(memory, object + 0x4c8u, 0u);
    for (index = 0u; index < 98u && !memory->fault; ++index)
        game_memory_write_u8(memory, object + 0x518u + index, order[index]);
}

GameStateOptionApplyResult game_state_apply_object_options(
        GameMemory *memory, GameAddress target, uint32_t flags,
        GameAddress object, GameAddress first_option,
        GameAddress second_option) {
    GameStateOptionApplyResult result;
    uint32_t value;
    result.masked_flags = flags & 0xfffffffbu;
    value = game_memory_read_u32(memory, target + 0xa0u) |
        result.masked_flags;
    if (((result.masked_flags >> 8u) & 8u) != 0u) value &= 0xfffffff7u;
    game_memory_write_u32(memory, target + 0xa0u, value);
    result.final_value = value;
    if (((result.masked_flags >> 8u) & 2u) != 0u) {
        result.final_value = first_option != 0u ? first_option : 0x00604380u;
        game_memory_write_u32(memory, target + 0xb8u, result.final_value);
    }
    if ((result.masked_flags & 0xc0u) != 0u) {
        result.final_value = second_option != 0u ? second_option : 0x00604278u;
        game_memory_write_u32(memory, target + 0xbcu, result.final_value);
    }
    if ((result.masked_flags & 0x20u) != 0u) {
        result.final_value = object;
        game_memory_write_u32(memory, target + 0xb4u, object);
    }
    return result;
}

GameStateGridPathResult game_state_query_grid_path(GameMemory *memory,
        GameAddress grid, uint32_t original_x, uint32_t original_y,
        uint32_t x_offset, uint32_t y_offset) {
    GameStateGridPathResult result;
    uint32_t x = original_x & 0xffu;
    uint32_t y = original_y & 0xffu;
    memset(&result, 0, sizeof(result));
    result.column = x + x_offset;
    result.row = y + y_offset;
    if ((int32_t)result.column < 0 || (int32_t)result.column >= 128 ||
            (int32_t)result.row < 0 || (int32_t)result.row >= 128) {
        result.branch = 0u;
        return result;
    }
    result.target_index = result.row * 16u + (result.column >> 3u);
    result.target_mask = 1u << (result.column & 7u);
    result.target_bits = game_memory_read_u8(memory,
        grid + result.target_index);
    if ((result.target_bits & result.target_mask) != 0u) {
        result.branch = 1u;
        return result;
    }
    if (x_offset == 0u) {
        result.value = 1u;
        result.branch = 2u;
        return result;
    }
    if (y_offset == 0u) {
        result.value = 1u;
        result.branch = 3u;
        return result;
    }
    result.first_index = y * 16u + (result.column >> 3u);
    result.first_bits = game_memory_read_u8(memory, grid + result.first_index);
    if ((result.first_bits & result.target_mask) != 0u) {
        result.branch = 4u;
        return result;
    }
    result.final_index = result.row * 16u + (x >> 3u);
    result.final_mask = 1u << (x & 7u);
    result.final_bits = game_memory_read_u8(memory, grid + result.final_index);
    result.value = (result.final_bits & result.final_mask) == 0u ? 1u : 0u;
    result.branch = 5u;
    return result;
}

static uint32_t game_state_copy_c_string(GameMemory *memory,
        GameAddress source, GameAddress destination, int backward) {
    uint32_t length = 0u;
    GameAddress cursor = source;
    GameAddress copy_source;
    GameAddress copy_destination = destination;
    uint32_t dwords;
    uint32_t bytes;
    uint8_t value;
    do {
        value = game_memory_read_u8(memory, cursor);
        if (memory->fault) return length;
        ++length;
        cursor += backward ? UINT32_MAX : 1u;
    } while (!memory->fault && value != 0u);
    copy_source = cursor - length;
    dwords = length >> 2u;
    bytes = length & 3u;
    while (dwords-- != 0u && !memory->fault) {
        uint32_t word = game_memory_read_u32(memory, copy_source);
        game_memory_write_u32(memory, copy_destination, word);
        copy_source += backward ? (uint32_t)-4 : 4u;
        copy_destination += backward ? (uint32_t)-4 : 4u;
    }
    while (bytes-- != 0u && !memory->fault) {
        value = game_memory_read_u8(memory, copy_source);
        game_memory_write_u8(memory, copy_destination, value);
        copy_source += backward ? UINT32_MAX : 1u;
        copy_destination += backward ? UINT32_MAX : 1u;
    }
    return length;
}

GameStateLocalizedRefreshResult game_state_refresh_localized_name_slots(
        GameMemory *memory, uint32_t start, uint32_t count, int backward) {
    static const GameAddress saved_index[5] = {
        0x00604c5cu, 0x00604c60u, 0x00604c64u, 0x00604c68u,
        0x00604c74u
    };
    GameStateLocalizedRefreshResult result;
    uint32_t limit = start + count;
    uint32_t index;
    result.limit = limit;
    result.final_eax = 0u;
    result.final_ecx = start;
    result.final_ebx = start * 4u;
    result.skipped = 0;
    if ((int32_t)limit >= 256) limit = 256u;
    result.limit = limit;
    if ((int32_t)start >= (int32_t)limit) {
        result.skipped = 1;
        return result;
    }
    for (index = start; index != limit && !memory->fault; ++index) {
        uint32_t type = game_memory_read_u32(memory,
            0x006047f0u + index * 4u);
        result.final_eax = type - 9u;
        if ((type >= 9u && type <= 12u) || type == 15u) {
            uint32_t slot = type == 15u ? 4u : type - 9u;
            unsigned pass;
            GameAddress source = game_memory_read_u32(memory,
                0x005948d8u + index * 4u);
            result.final_eax = game_state_copy_c_string(memory, source,
                0x006044d8u + slot * 0x10u, backward);
            result.final_ecx = 0u;
            for (pass = 0u; pass < 5u && !memory->fault; ++pass) {
                uint32_t source_index;
                if (pass == slot) continue;
                source_index = game_memory_read_u32(memory,
                    saved_index[pass]);
                source = game_memory_read_u32(memory,
                    0x005948d8u + source_index * 4u);
                result.final_eax = game_state_copy_c_string(memory, source,
                    0x006044d8u + pass * 0x10u, backward);
                result.final_ecx = 0u;
            }
        }
    }
    result.final_ebx = limit * 4u;
    return result;
}

GameStateRelinkResult game_state_relink_intrusive_node(GameMemory *memory,
        GameAddress node) {
    GameStateRelinkResult result;
    uint32_t flags = game_memory_read_u32(memory, node + 0x2cu);
    GameAddress successor = game_memory_read_u32(memory, node + 4u);
    result.final_eax = successor;
    result.final_ecx = 0u;
    result.final_edx = 0u;
    result.flag_value = flags & 0xffu;
    result.seed_left = 0u;
    result.seed_right = 0u;
    result.flag_kind = GAME_STATE_RELINK_TEST_BYTE;
    result.seed_compare = 0;
    result.modified = 0;
    if (memory->fault || (flags & 1u) != 0u) return result;
    {
        GameAddress predecessor = game_memory_read_u32(memory, node + 8u);
        GameAddress head;
        uint32_t count;
        uint32_t successor_flags;
        if (predecessor != 0u)
            game_memory_write_u32(memory, predecessor + 4u, successor);
        if (successor != 0u)
            game_memory_write_u32(memory, successor + 8u, predecessor);
        game_memory_write_u32(memory, node + 8u, 0u);
        game_memory_write_u32(memory, node + 4u, 0u);
        count = game_memory_read_u32(memory, 0x00594464u);
        game_memory_write_u32(memory, 0x00594464u, count - 1u);
        result.seed_left = count;
        result.seed_right = 1u;
        head = game_memory_read_u32(memory, 0x00594460u);
        game_memory_write_u32(memory, node + 4u, head);
        if (head != 0u)
            game_memory_write_u32(memory, head + 8u, node);
        game_memory_write_u32(memory, 0x00594460u, node);
        successor_flags = game_memory_read_u32(memory, successor + 0x2cu);
        result.modified = 1;
        result.final_ecx = successor_flags;
        result.final_edx = head;
        result.flag_value = successor_flags & 0xffu;
        if ((successor_flags & 1u) != 0u) {
            GameAddress cursor = successor;
            GameAddress next = 0u;
            while (cursor != 0u && !memory->fault) {
                next = game_memory_read_u32(memory, cursor + 4u);
                result.seed_left = successor;
                result.seed_right = next;
                result.seed_compare = 1;
                if (next == successor) break;
                cursor = next;
            }
            result.final_ecx = cursor;
            result.final_edx = next;
            result.flag_value = cursor;
            result.flag_kind = GAME_STATE_RELINK_TEST_WORD;
            if (cursor != 0u)
                game_memory_write_u32(memory, successor + 8u, cursor);
        }
    }
    return result;
}
