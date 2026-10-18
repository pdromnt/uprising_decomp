#include "runtime_utilities.h"

void runtime_store_vector3(GameMemory *memory, GameAddress destination,
    uint32_t x, uint32_t y, uint32_t z) {
    game_memory_write_u32(memory, destination, x);
    if (!memory->fault) game_memory_write_u32(memory, destination + 4u, y);
    if (!memory->fault) game_memory_write_u32(memory, destination + 8u, z);
}

GameAddress runtime_follow_payload(GameMemory *memory, GameAddress owner) {
    GameAddress resource;
    if (owner == 0u) return 0u;
    resource = game_memory_read_u32(memory, owner + 0x58u);
    if (memory->fault || resource == 0u) return 0u;
    return game_memory_read_u32(memory, resource + 0x16cu);
}

void runtime_publish_event_value(GameMemory *memory, uint32_t value) {
    game_memory_write_u32(memory, 0x005a1cd0u, value);
}

uint32_t runtime_consume_u32(GameMemory *memory, GameAddress cursor_slot) {
    GameAddress cursor = game_memory_read_u32(memory, cursor_slot);
    uint32_t value;
    if (memory->fault) return 0u;
    value = game_memory_read_u32(memory, cursor);
    if (!memory->fault) game_memory_write_u32(memory, cursor_slot, cursor + 4u);
    return value;
}

int runtime_three_slots_empty(GameMemory *memory, GameAddress slots) {
    unsigned index;
    for (index = 0u; index < 3u && !memory->fault; ++index)
        if (game_memory_read_u32(memory, slots + index * 4u) != 0u) return 0;
    return !memory->fault;
}

void runtime_copy_words(GameMemory *memory, GameAddress destination,
    GameAddress source, uint32_t count) {
    uint32_t index;
    for (index = 0u; index < count && !memory->fault; ++index) {
        uint32_t word = game_memory_read_u32(memory, source + index * 4u);
        if (!memory->fault)
            game_memory_write_u32(memory, destination + index * 4u, word);
    }
}

uint32_t runtime_add_offsets(uint32_t left, uint32_t right, int *wrapped) {
    uint32_t sum = left + right;
    if (wrapped) *wrapped = left > sum || right > sum;
    return sum;
}

void runtime_shift_u96_left(GameMemory *memory, GameAddress value) {
    uint32_t low = game_memory_read_u32(memory, value);
    uint32_t middle = game_memory_read_u32(memory, value + 4u);
    uint32_t high = game_memory_read_u32(memory, value + 8u);
    if (memory->fault) return;
    game_memory_write_u32(memory, value, low << 1u);
    if (!memory->fault)
        game_memory_write_u32(memory, value + 4u, (middle << 1u) | (low >> 31u));
    if (!memory->fault)
        game_memory_write_u32(memory, value + 8u, (high << 1u) | (middle >> 31u));
}

void runtime_shift_u96_right(GameMemory *memory, GameAddress value) {
    uint32_t low = game_memory_read_u32(memory, value);
    uint32_t middle = game_memory_read_u32(memory, value + 4u);
    uint32_t high = game_memory_read_u32(memory, value + 8u);
    if (memory->fault) return;
    game_memory_write_u32(memory, value + 8u, high >> 1u);
    if (!memory->fault)
        game_memory_write_u32(memory, value + 4u, (middle >> 1u) | (high << 31u));
    if (!memory->fault)
        game_memory_write_u32(memory, value, (low >> 1u) | (middle << 31u));
}

RuntimeShiftU96Result runtime_shift_u96(GameMemory *memory,
        GameAddress value, int32_t count) {
    RuntimeShiftU96Result result;
    uint32_t carry = 0u;
    GameAddress cursor = value;
    GameAddress source;
    int index;
    result.word_shift = count / 32;
    result.bit_shift = count % 32;
    result.mask = ~(UINT32_MAX << ((uint32_t)result.bit_shift & 31u));
    result.inverse_shift = (uint8_t)(32 - result.bit_shift);
    result.final_selected = 0u;
    result.final_eax = (uint32_t)result.word_shift * 4u;
    for (index = 0; index < 3 && !memory->fault; ++index) {
        uint32_t input = game_memory_read_u32(memory, cursor);
        uint32_t selected = input & result.mask;
        uint32_t shifted = input >> ((uint32_t)result.bit_shift & 31u);
        game_memory_write_u32(memory, cursor, shifted | carry);
        carry = selected << ((uint32_t)result.inverse_shift & 31u);
        result.final_selected = selected;
        cursor += 4u;
    }
    source = value + 8u - (uint32_t)result.word_shift * 4u;
    for (index = 2; index >= 0 && !memory->fault; --index) {
        if (result.word_shift <= index) {
            result.final_eax = game_memory_read_u32(memory, source);
            game_memory_write_u32(memory, value + (uint32_t)index * 4u,
                result.final_eax);
        } else {
            game_memory_write_u32(memory, value + (uint32_t)index * 4u, 0u);
        }
        source -= 4u;
    }
    return result;
}

int runtime_divide_u64(uint32_t dividend_low, uint32_t dividend_high,
        uint32_t divisor_low, uint32_t divisor_high,
        RuntimeDivideU64Result *result) {
    uint64_t dividend = ((uint64_t)dividend_high << 32u) | dividend_low;
    uint64_t divisor = ((uint64_t)divisor_high << 32u) | divisor_low;
    uint64_t quotient;
    uint64_t remainder;
    if (divisor == 0u) return 0;
    quotient = dividend / divisor;
    remainder = dividend % divisor;
    result->quotient_low = (uint32_t)quotient;
    result->quotient_high = (uint32_t)(quotient >> 32u);
    result->remainder_low = (uint32_t)remainder;
    result->remainder_high = (uint32_t)(remainder >> 32u);
    return 1;
}

static int mode_is_enabled(uint16_t mode) {
    int16_t signed_mode = (int16_t)mode;
    return signed_mode <= 0x0100 ||
        (signed_mode >= 0x0300 && signed_mode <= 0x0500) ||
        signed_mode >= 0x0700;
}

int runtime_set_mode_enabled(GameMemory *memory, uint16_t mode) {
    int enabled = mode_is_enabled(mode);
    game_memory_write_u32(memory, 0x0059d438u, enabled ? 1u : 0u);
    return enabled;
}

uint32_t runtime_register_pointer(GameMemory *memory, GameAddress value) {
    uint32_t index = game_memory_read_u32(memory, 0x00639828u) + 1u;
    if (!memory->fault) game_memory_write_u32(memory, 0x00639828u, index);
    if (!memory->fault)
        game_memory_write_u32(memory, 0x00631b24u + index * 4u, value);
    return index;
}

void runtime_reset_registration_array(GameMemory *memory, int backwards) {
    uint32_t was_initialized = game_memory_read_u32(memory, 0x0059d938u);
    uint32_t index;
    if (memory->fault) return;
    game_memory_write_u32(memory, 0x0059d934u, 1u);
    if (was_initialized != 0u) {
        for (index = 0u; index < 8000u && !memory->fault; ++index)
            game_memory_write_u32(memory, backwards ?
                0x00631b28u - index * 4u : 0x00631b28u + index * 4u, 0u);
    }
    if (!memory->fault) game_memory_write_u32(memory, 0x00639828u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x0059d938u, 0u);
}

void runtime_install_callback_table(GameMemory *memory) {
    static const uint32_t callbacks[] = {
        0x00565b80u, 0x00565c00u, 0x00565b10u, 0x00565be0u,
        0x00565fa0u, 0x00565fa0u
    };
    static const GameAddress slots[] = {
        0x005a1d6cu, 0x005a1d70u, 0x005a1d74u, 0x005a1d78u,
        0x005a1d68u, 0x005a1d7cu
    };
    unsigned index;
    for (index = 0u; index < 6u && !memory->fault; ++index)
        game_memory_write_u32(memory, slots[index], callbacks[index]);
}

void runtime_decode_bit_masks(GameMemory *memory, GameAddress record,
    uint16_t masks[2]) {
    int first = 8 - (int)game_memory_read_u8(memory, record + 0x11u);
    int delta = (int)game_memory_read_u8(memory, record + 0x12u) - 3;
    int second;
    if (memory->fault) return;
    if (delta < 0) second = first + delta;
    else { first -= delta; second = first + delta; }
    masks[0] = (uint16_t)(1u << ((unsigned)first & 31u));
    masks[1] = (uint16_t)(1u << ((unsigned)second & 31u));
}

GameAddress runtime_resolve_handle(GameMemory *memory, uint32_t index) {
    uint32_t limit = game_memory_read_u32(memory, 0x0063b6a0u);
    if (!memory->fault && index < limit) {
        GameAddress page = game_memory_read_u32(memory,
            0x0063b5a0u + (index & 0xffffffe7u) / 8u);
        GameAddress entry = page + (index & 0x1fu) * 8u;
        if (!memory->fault && (game_memory_read_u8(memory, entry + 4u) & 1u))
            return game_memory_read_u32(memory, entry);
    }
    if (!memory->fault) game_memory_write_u32(memory, 0x005a1c5cu, 9u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005a1c60u, 0u);
    return 0xffffffffu;
}

int runtime_byte_class_matches(GameMemory *memory, uint8_t code,
    uint32_t class_mask, uint32_t requested_mask) {
    uint8_t direct = game_memory_read_u8(memory, 0x005a30a1u + code);
    if (memory->fault) return 0;
    if ((requested_mask & direct) != 0u) return 1;
    if (class_mask != 0u) {
        uint16_t classes = game_memory_read_u16(memory,
            0x005a1ed2u + (uint32_t)code * 2u);
        if (!memory->fault && (classes & class_mask) != 0u) return 1;
    }
    return 0;
}

uint32_t runtime_pack_file_mode(uint16_t mode) {
    uint32_t packed = 0u;
    if (mode & 0x0001u) packed |= 0x00000010u;
    if (mode & 0x0004u) packed |= 0x00000008u;
    if (mode & 0x0008u) packed |= 0x00000004u;
    if (mode & 0x0010u) packed |= 0x00000002u;
    if (mode & 0x0020u) packed |= 0x00000001u;
    if (mode & 0x0002u) packed |= 0x00080000u;
    if ((mode & 0x0c00u) == 0x0400u) packed |= 0x00000100u;
    else if ((mode & 0x0c00u) == 0x0800u) packed |= 0x00000200u;
    else if ((mode & 0x0c00u) == 0x0c00u) packed |= 0x00000300u;
    if ((mode & 0x0300u) == 0u) packed |= 0x00020000u;
    else if ((mode & 0x0300u) == 0x0200u) packed |= 0x00010000u;
    if (mode & 0x1000u) packed |= 0x00040000u;
    return packed;
}

uint16_t runtime_unpack_file_mode(uint32_t packed) {
    uint16_t mode = 0u;
    if (packed & 0x00000010u) mode |= 0x0001u;
    if (packed & 0x00000008u) mode |= 0x0004u;
    if (packed & 0x00000004u) mode |= 0x0008u;
    if (packed & 0x00000002u) mode |= 0x0010u;
    if (packed & 0x00000001u) mode |= 0x0020u;
    if (packed & 0x00080000u) mode |= 0x0002u;
    if ((packed & 0x00000300u) == 0x00000100u) mode |= 0x0400u;
    else if ((packed & 0x00000300u) == 0x00000200u) mode |= 0x0800u;
    else if ((packed & 0x00000300u) == 0x00000300u) mode |= 0x0c00u;
    if ((packed & 0x00030000u) == 0u) mode |= 0x0300u;
    else if ((packed & 0x00030000u) == 0x00010000u) mode |= 0x0200u;
    if (packed & 0x00040000u) mode |= 0x1000u;
    return mode;
}

static int32_t arithmetic_shift_right(uint32_t value, unsigned amount) {
    return ((int32_t)value) >> amount;
}

void runtime_update_viewport_bounds(GameMemory *memory, int quarter_lower) {
    int32_t width = (int32_t)game_memory_read_u32(memory, 0x005ac6a0u);
    int32_t height = (int32_t)game_memory_read_u32(memory, 0x005ac6a4u);
    int32_t inset;
    int32_t horizontal;
    int32_t vertical;
    if (memory->fault) return;
    inset = arithmetic_shift_right((uint32_t)height, 3u);
    horizontal = arithmetic_shift_right((uint32_t)width, 1u);
    vertical = arithmetic_shift_right((uint32_t)height,
        quarter_lower ? 2u : 1u);
    game_memory_write_u32(memory, 0x005acce0u,
        (uint32_t)(horizontal + inset));
    if (!memory->fault) game_memory_write_u32(memory, 0x005ac920u,
        (uint32_t)(horizontal - inset));
    if (!memory->fault) game_memory_write_u32(memory, 0x005acce4u,
        (uint32_t)(arithmetic_shift_right((uint32_t)height, 1u) + inset));
    if (!memory->fault) game_memory_write_u32(memory, 0x005ac924u,
        (uint32_t)(vertical - inset));
}

GameAddress runtime_probe_stack(GameMemory *memory, GameAddress stack,
    uint32_t amount) {
    GameAddress cursor = stack + 4u;
    while (amount >= 0x1000u && !memory->fault) {
        cursor -= 0x1000u;
        amount -= 0x1000u;
        (void)game_memory_read_u32(memory, cursor);
    }
    cursor -= amount;
    if (!memory->fault) (void)game_memory_read_u32(memory, cursor);
    return cursor;
}

GameAddress runtime_optional_gameplay_service(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0059d4bcu);
}

int runtime_character_should_submit(uint32_t character) {
    return character != 0xffffffffu;
}

uint32_t runtime_parser_success_predicate(uint32_t result) {
    return result < 1u ? 0xffffffffu : 0u;
}

uint32_t runtime_locale_class_bit2(GameMemory *memory, int32_t character) {
    GameAddress table = game_memory_read_u32(memory, 0x005a1ec8u);
    if (memory->fault) return 0u;
    return game_memory_read_u16(memory,
        table + (uint32_t)character * 2u) & 4u;
}

int runtime_service_needs_initialization(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a26f4u) == 0u;
}

uint32_t runtime_service_commit_initialization(GameMemory *memory) {
    uint32_t count = game_memory_read_u32(memory, 0x005a26f4u) + 1u;
    if (!memory->fault)
        game_memory_write_u32(memory, 0x005a26f4u, count);
    return count;
}

void runtime_fullscreen_rectangle(GameMemory *memory, uint32_t rectangle[4]) {
    rectangle[0] = 0u;
    rectangle[1] = 0u;
    rectangle[2] = game_memory_read_u32(memory, 0x005ac6a0u);
    rectangle[3] = game_memory_read_u32(memory, 0x005ac6a4u);
}

uint32_t runtime_forward_table(unsigned variant) {
    static const uint32_t tables[2] = {0x005a3350u, 0x005a3368u};
    return variant < 2u ? tables[variant] : 0u;
}

uint32_t runtime_element_width(void) { return 8u; }

uint32_t runtime_conversion_width(void) { return 0x40u; }

void runtime_platform_set_input_active(GameMemory *memory, int active) {
    game_memory_write_u32(memory, 0x00597f8cu, active ? 1u : 0u);
}

GameAddress runtime_platform_input_callback(GameMemory *memory,
        unsigned stage) {
    static const GameAddress slots[3] = {
        0x0063c814u, 0x0063c874u, 0x0063c88cu
    };
    return stage < 3u ? game_memory_read_u32(memory, slots[stage]) : 0u;
}

void runtime_reset_subsystem_token(GameMemory *memory) {
    game_memory_write_u32(memory, 0x005efefcu, 0u);
}

GameAddress runtime_active_session_owner(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0058abe8u);
}

void runtime_reset_input_queues(GameMemory *memory) {
    game_memory_write_u32(memory, 0x005a387cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005a3878u, 0u);
}

void runtime_reset_presentation_pair(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00604258u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x0060425cu, 0u);
}

RuntimeShutdownRegistration runtime_shutdown_registration(unsigned kind) {
    static const RuntimeShutdownRegistration registrations[5] = {
        {0x005cee40u, 0x0048f320u, 0x004525b0u},
        {0x005cef90u, 0x0048f320u, 0x004525e0u},
        {0x005ceee8u, 0x0048f320u, 0x00452610u},
        {0x005f0800u, 0x00499800u, 0x00499760u},
        {0x00631910u, 0x0048ef80u, 0x00540d80u}
    };
    RuntimeShutdownRegistration empty = {0u, 0u, 0u};
    return kind < 5u ? registrations[kind] : empty;
}

GameAddress runtime_exception_registration_target(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0063c664u);
}

void runtime_publish_exception_handle(GameMemory *memory,
        GameAddress handle) {
    game_memory_write_u32(memory, 0x0063a524u, handle);
}

void runtime_reset_environment_state(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00597088u, 0u);
}

GameAddress runtime_platform_region_target(GameMemory *memory,
        unsigned operation) {
    return game_memory_read_u32(memory,
        operation == 0u ? 0x0063c69cu : 0x0063c6a0u);
}

void runtime_publish_platform_region(GameMemory *memory, GameAddress handle) {
    game_memory_write_u32(memory, 0x0063a584u, handle);
}

GameAddress runtime_platform_region(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0063a584u);
}

void runtime_publish_environment_mode(GameMemory *memory, uint32_t mode) {
    game_memory_write_u32(memory, 0x005a1cb4u, mode);
}

GameAddress runtime_pretermination_callback(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a1cb8u);
}

void runtime_termination_range(unsigned index, GameAddress *begin,
        GameAddress *end) {
    static const GameAddress ranges[2][2] = {
        {0x005831bcu, 0x005831c8u},
        {0x00583000u, 0x005831b8u}
    };
    if (!begin || !end) return;
    if (index < 2u) {
        *begin = ranges[index][0];
        *end = ranges[index][1];
    } else {
        *begin = 0u;
        *end = 0u;
    }
}

uint32_t runtime_default_locale_selector(void) { return 0xfffffffdu; }

void runtime_default_control_masks(uint32_t *requested, uint32_t *mask) {
    if (requested) *requested = 0x00010000u;
    if (mask) *mask = 0x00030000u;
}

uint32_t runtime_supported_control_mask(void) { return 0xfff7ffffu; }

GameAddress runtime_platform_close_target(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0063c700u);
}

void runtime_publish_platform_error(GameMemory *memory, uint32_t error) {
    game_memory_write_u32(memory, 0x005a1c5cu, error);
}

GameAddress runtime_platform_probe_target(GameMemory *memory,
        unsigned stage) {
    return game_memory_read_u32(memory,
        stage == 0u ? 0x0063c6e8u : 0x0063c754u);
}

GameAddress runtime_initializer_target(GameMemory *memory,
        GameAddress cursor) {
    return game_memory_read_u32(memory, cursor);
}

uint32_t runtime_control_preserved(uint32_t current, uint32_t mask) {
    return current & ~mask;
}

uint32_t runtime_control_selected(uint32_t requested, uint32_t mask) {
    return requested & mask;
}

void runtime_multibyte_classification_state(GameMemory *memory,
        uint32_t *locale, GameAddress *table) {
    if (locale) *locale = game_memory_read_u32(memory, 0x005a31a4u);
    if (!memory->fault && table)
        *table = game_memory_read_u32(memory, 0x005a31a8u);
}

uint32_t runtime_multibyte_locale(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a31a4u);
}

uint16_t runtime_decode_folded_multibyte(GameMemory *memory,
        GameAddress *cursor) {
    uint16_t value;
    uint8_t first;
    uint8_t flags;
    if (!cursor) return 0u;
    first = game_memory_read_u8(memory, *cursor);
    if (memory->fault) return 0u;
    (*cursor)++;
    flags = game_memory_read_u8(memory, 0x005a30a1u + first);
    if (memory->fault) return 0u;
    if ((flags & 4u) != 0u) {
        uint8_t second = game_memory_read_u8(memory, *cursor);
        uint16_t lower;
        uint16_t upper;
        if (memory->fault || second == 0u) return 0u;
        (*cursor)++;
        value = (uint16_t)(((uint16_t)first << 8u) | second);
        lower = game_memory_read_u16(memory, 0x005a31b0u);
        upper = game_memory_read_u16(memory, 0x005a31b2u);
        if (memory->fault) return 0u;
        if (value >= lower && value <= upper) {
            value = (uint16_t)(value +
                game_memory_read_u16(memory, 0x005a31b4u));
        } else {
            lower = game_memory_read_u16(memory, 0x005a31b6u);
            upper = game_memory_read_u16(memory, 0x005a31b8u);
            if (memory->fault) return 0u;
            if (value >= lower && value <= upper)
                value = (uint16_t)(value +
                    game_memory_read_u16(memory, 0x005a31bau));
        }
        return memory->fault ? 0u : value;
    }
    return (uint16_t)runtime_ascii_lower(first);
}

uint32_t runtime_ascii_lower(uint32_t value) {
    return value >= (uint32_t)'A' && value <= (uint32_t)'Z' ?
        value + 0x20u : value;
}

uint32_t runtime_character_locale_active(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a1eb0u);
}

uint32_t runtime_locale_table_count(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a20d4u);
}

GameAddress runtime_character_table(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a1ec8u);
}

uint8_t runtime_character_flags(GameMemory *memory, uint8_t value) {
    return game_memory_read_u8(memory, 0x005a30a1u + value);
}

RuntimeResourceDescriptor runtime_resource_descriptor(GameMemory *memory,
        unsigned index) {
    RuntimeResourceDescriptor descriptor = {0u, 0u, 0u};
    static const GameAddress owners[2] = {0x0058abf4u, 0x0058abf8u};
    static const GameAddress values[2] = {0x005ac910u, 0x005acbecu};
    static const GameAddress formats[2] = {0x0058bae8u, 0x0058baecu};
    if (index >= 2u) return descriptor;
    descriptor.owner = game_memory_read_u32(memory, owners[index]);
    if (!memory->fault && descriptor.owner != 0u)
        descriptor.value = game_memory_read_u32(memory, values[index]);
    descriptor.format = formats[index];
    return descriptor;
}

GameAddress runtime_resource_formatter(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0063c8a0u);
}

void runtime_publish_stream_buffer_counter(GameMemory *memory,
        uint32_t value) {
    game_memory_write_u32(memory, 0x005a2370u, value);
}

void runtime_configure_stream_buffer(GameMemory *memory,
        GameAddress stream, GameAddress buffer, uint32_t flags,
        uint32_t size) {
    game_memory_write_u32(memory, stream + 8u, buffer);
    if (!memory->fault) game_memory_write_u32(memory, stream + 12u, flags);
    if (!memory->fault) game_memory_write_u32(memory, stream + 24u, size);
    if (!memory->fault) game_memory_write_u32(memory, stream, buffer);
    if (!memory->fault) game_memory_write_u32(memory, stream + 4u, 0u);
}

void runtime_reset_stream_record(GameMemory *memory, GameAddress stream) {
    game_memory_write_u32(memory, stream + 4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, stream + 12u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, stream + 8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, stream, 0u);
    if (!memory->fault) game_memory_write_u32(memory, stream + 28u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, stream + 16u,
        0xffffffffu);
}

GameAddress runtime_publish_locale_state(GameMemory *memory, uint32_t flags,
        uint32_t length, uint32_t trail_state, uint32_t lead_state) {
    GameAddress state = game_memory_read_u32(memory, 0x005a2510u);
    if (!memory->fault) game_memory_write_u32(memory, state, flags);
    if (!memory->fault) game_memory_write_u32(memory, state + 4u, length);
    if (!memory->fault)
        game_memory_write_u32(memory, state + 0x14u, trail_state);
    if (!memory->fault)
        game_memory_write_u32(memory, state + 0x10u, lead_state);
    return state;
}

RuntimeWindowBounds runtime_window_bounds(GameMemory *memory,
        GameAddress object) {
    RuntimeWindowBounds bounds;
    bounds.left = game_memory_read_u32(memory, object + 0x20u);
    bounds.top = game_memory_read_u32(memory, object + 0x24u);
    bounds.right = game_memory_read_u32(memory, object + 0x28u);
    bounds.bottom = game_memory_read_u32(memory, object + 0x2cu);
    return bounds;
}

void runtime_reposition_window(GameMemory *memory, GameAddress object,
        uint32_t left, uint32_t top, uint32_t tag, uint32_t width,
        uint32_t height) {
    game_memory_write_u32(memory, object + 0x20u, left);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1cu, 1u);
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x28u, left + width);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x40u, tag);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x24u, top);
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x2cu, top + height);
}

RuntimeCallbackTable runtime_callback_table(GameMemory *memory) {
    RuntimeCallbackTable table;
    table.begin = game_memory_read_u32(memory, 0x0063b6acu);
    table.end = game_memory_read_u32(memory, 0x0063b6a8u);
    return table;
}

void runtime_rebase_callback_table(GameMemory *memory, GameAddress begin,
        uint32_t offset) {
    game_memory_write_u32(memory, 0x0063b6acu, begin);
    if (!memory->fault)
        game_memory_write_u32(memory, 0x0063b6a8u, begin + offset);
}

void runtime_append_callback(GameMemory *memory, GameAddress callback) {
    GameAddress end = game_memory_read_u32(memory, 0x0063b6a8u);
    if (!memory->fault) game_memory_write_u32(memory, end, callback);
    if (!memory->fault) game_memory_write_u32(memory, 0x0063b6a8u,
        end + 4u);
}

void runtime_set_callback_block(GameMemory *memory, GameAddress block) {
    game_memory_write_u32(memory, 0x0063b6acu, block);
}

void runtime_publish_callback_block(GameMemory *memory, GameAddress block) {
    game_memory_write_u32(memory, 0x0063b6acu, block);
    if (!memory->fault) game_memory_write_u32(memory, block, 0u);
    if (!memory->fault) game_memory_write_u32(memory, 0x0063b6a8u, block);
}

RuntimeArrayCleanupPlan runtime_array_cleanup_plan(GameMemory *memory,
        GameAddress frame) {
    RuntimeArrayCleanupPlan plan = {0u, {0u, 0u, 0u, 0u}};
    plan.completed = game_memory_read_u32(memory, frame - 0x20u);
    if (!memory->fault && plan.completed == 0u) {
        plan.arguments[0] = game_memory_read_u32(memory, frame + 8u);
        plan.arguments[1] = game_memory_read_u32(memory, frame + 0x0cu);
        plan.arguments[2] = game_memory_read_u32(memory, frame - 0x1cu);
        plan.arguments[3] = game_memory_read_u32(memory, frame + 0x18u);
    }
    return plan;
}

GameAddress runtime_display_descriptor(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005980acu);
}

void runtime_publish_display_descriptor(GameMemory *memory,
        GameAddress descriptor) {
    game_memory_write_u32(memory, 0x005980acu, descriptor);
}

void runtime_store_display_descriptor(GameMemory *memory,
        GameAddress descriptor, uint16_t width, uint16_t height,
        GameAddress handle) {
    game_memory_write_u16(memory, descriptor, width);
    if (!memory->fault)
        game_memory_write_u16(memory, descriptor + 2u, height);
    if (!memory->fault)
        game_memory_write_u32(memory, descriptor + 4u, handle);
}

RuntimeEnvironmentState runtime_environment_state(GameMemory *memory) {
    RuntimeEnvironmentState state;
    state.table = game_memory_read_u32(memory, 0x005a1c84u);
    state.initialized = 0u;
    if (!memory->fault && state.table == 0u)
        state.initialized = game_memory_read_u32(memory, 0x005a1c8cu);
    return state;
}

GameAddress runtime_environment_entry(GameMemory *memory,
        GameAddress cursor) {
    return game_memory_read_u32(memory, cursor);
}

RuntimeCallArguments3 runtime_call_arguments3(GameMemory *memory,
        GameAddress stack) {
    RuntimeCallArguments3 arguments;
    arguments.first = game_memory_read_u32(memory, stack + 4u);
    arguments.second = game_memory_read_u32(memory, stack + 8u);
    arguments.third = game_memory_read_u32(memory, stack + 12u);
    return arguments;
}

RuntimeCallArguments3 runtime_scanner_arguments(GameMemory *memory,
        GameAddress stack) {
    RuntimeCallArguments3 arguments;
    arguments.first = game_memory_read_u32(memory, stack + 4u);
    arguments.second = game_memory_read_u32(memory, stack + 8u);
    arguments.third = stack + 12u;
    return arguments;
}

uint32_t runtime_descriptor_value(GameMemory *memory, GameAddress stack) {
    return game_memory_read_u32(memory, stack + 8u);
}

GameAddress runtime_sync_handle(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a15d4u);
}

void runtime_publish_sync_handle(GameMemory *memory, GameAddress handle) {
    game_memory_write_u32(memory, 0x005a15d4u, handle);
}

GameAddress runtime_sync_target(GameMemory *memory, unsigned stage) {
    static const GameAddress slots[3] = {
        0x0063c774u, 0x0063c740u, 0x0063c788u
    };
    return stage < 3u ? game_memory_read_u32(memory, slots[stage]) : 0u;
}

GameAddress runtime_default_sync_name(void) { return 0x005a15d8u; }

uint32_t runtime_stream_flags(GameMemory *memory, GameAddress stream) {
    return game_memory_read_u32(memory, stream + 0x0cu);
}

GameAddress runtime_stream_cursor(GameMemory *memory, GameAddress stream) {
    return game_memory_read_u32(memory, stream);
}

GameAddress runtime_stream_buffer(GameMemory *memory, GameAddress stream) {
    return game_memory_read_u32(memory, stream + 8u);
}

uint32_t runtime_stream_count(GameMemory *memory, GameAddress stream) {
    return game_memory_read_u32(memory, stream + 4u);
}

GameAddress runtime_stream_handle(GameMemory *memory, GameAddress stream) {
    return game_memory_read_u32(memory, stream + 0x10u);
}

void runtime_publish_stream_cursor(GameMemory *memory, GameAddress stream,
        GameAddress cursor) {
    game_memory_write_u32(memory, stream, cursor);
}

void runtime_publish_stream_byte_state(GameMemory *memory,
        GameAddress stream, uint32_t count, uint32_t flags) {
    game_memory_write_u32(memory, stream + 4u, count);
    if (!memory->fault)
        game_memory_write_u32(memory, stream + 0x0cu, flags);
}

void runtime_store_stream_flags(GameMemory *memory, GameAddress stream,
        uint32_t flags) {
    game_memory_write_u32(memory, stream + 0x0cu, flags);
}

void runtime_clear_stream_buffer(GameMemory *memory, GameAddress stream) {
    game_memory_write_u32(memory, stream + 0x1cu, 0u);
}

void runtime_publish_text_cursor(GameMemory *memory, uint32_t x,
        uint32_t y) {
    game_memory_write_u32(memory, 0x0062970cu, x);
    if (!memory->fault) game_memory_write_u32(memory, 0x00629708u, y);
}

GameAddress runtime_text_rasterizer_target(void) {
    return 0x00532bf0u;
}

GameAddress runtime_text_formatter_target(void) {
    return 0x00562ef0u;
}

RuntimeBufferedByte runtime_stream_commit_count_and_take(GameMemory *memory,
        GameAddress stream, int32_t count) {
    RuntimeBufferedByte result = {0u, 0u, 0};
    game_memory_write_u32(memory, stream + 4u, (uint32_t)count);
    if (memory->fault || count < 0) return result;
    result.cursor = game_memory_read_u32(memory, stream);
    if (!memory->fault)
        game_memory_write_u32(memory, stream, result.cursor + 1u);
    if (!memory->fault) result.value = game_memory_read_u8(memory,
        result.cursor);
    result.available = !memory->fault;
    return result;
}

uint32_t runtime_locale_mode(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a20d4u);
}

uint16_t runtime_locale_class_value(GameMemory *memory, int32_t character,
        GameAddress *table) {
    *table = game_memory_read_u32(memory, 0x005a1ec8u);
    return memory->fault ? 0u : game_memory_read_u16(memory,
        *table + (uint32_t)character * 2u);
}

uint32_t runtime_increment_counter(GameMemory *memory,
        GameAddress counter) {
    uint32_t value = game_memory_read_u32(memory, counter);
    if (!memory->fault) game_memory_write_u32(memory, counter, value + 1u);
    return value;
}

uint32_t runtime_pointer_value(GameMemory *memory, GameAddress pointer) {
    return game_memory_read_u32(memory, pointer);
}

RuntimeStreamFlushState runtime_stream_flush_state(GameMemory *memory,
        GameAddress stream) {
    RuntimeStreamFlushState state;
    state.flags = game_memory_read_u32(memory, stream + 12u);
    state.cursor = game_memory_read_u32(memory, stream);
    state.base = game_memory_read_u32(memory, stream + 8u);
    state.handle = game_memory_read_u32(memory, stream + 16u);
    return state;
}

void runtime_stream_finish_flush(GameMemory *memory, GameAddress stream,
        uint32_t flags, GameAddress base, int reset_count) {
    game_memory_write_u32(memory, stream + 12u, flags);
    if (!memory->fault) game_memory_write_u32(memory, stream, base);
    if (!memory->fault && reset_count)
        game_memory_write_u32(memory, stream + 4u, 0u);
}

void runtime_stream_reset_write_cursor(GameMemory *memory,
        GameAddress stream, GameAddress base) {
    game_memory_write_u32(memory, stream, base);
    if (!memory->fault) game_memory_write_u32(memory, stream + 4u, 0u);
}

void runtime_stream_finish_close(GameMemory *memory, GameAddress stream,
        uint32_t flags) {
    game_memory_write_u32(memory, stream, 0u);
    if (!memory->fault)
        game_memory_write_u32(memory, stream + 12u, flags & 0xfffffbf7u);
    if (!memory->fault) game_memory_write_u32(memory, stream + 8u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, stream + 4u, 0u);
}

GameAddress runtime_stream_single_output_target(void) {
    return 0x00562a60u;
}

void runtime_initialize_format_descriptor(GameMemory *memory,
        GameAddress descriptor, GameAddress cursor, uint32_t remaining,
        uint32_t flags) {
    game_memory_write_u32(memory, descriptor, cursor);
    if (!memory->fault)
        game_memory_write_u32(memory, descriptor + 4u, remaining);
    if (!memory->fault)
        game_memory_write_u32(memory, descriptor + 8u, cursor);
    if (!memory->fault)
        game_memory_write_u32(memory, descriptor + 12u, flags);
}

GameAddress runtime_terminate_format_output(GameMemory *memory,
        GameAddress cursor) {
    game_memory_write_u8(memory, cursor, 0u);
    return cursor + 1u;
}

uint32_t runtime_calendar_month_offset(GameMemory *memory, uint32_t month) {
    return game_memory_read_u32(memory, 0x005a33bcu + month * 4u);
}

uint32_t runtime_calendar_epoch_base(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a2638u);
}

uint32_t runtime_calendar_auto_daylight(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a263cu);
}

uint32_t runtime_calendar_daylight_offset(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a2640u);
}

uint32_t runtime_handle_limit(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0063b6a0u);
}

GameAddress runtime_handle_table_block(GameMemory *memory,
        uint32_t block_index) {
    return game_memory_read_u32(memory, 0x0063b5a0u + block_index * 4u);
}

uint32_t runtime_standard_handles_owned(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a1d40u);
}

uint32_t runtime_standard_handle(uint32_t handle) {
    static const uint32_t handles[3] = {
        0xfffffff6u, 0xfffffff5u, 0xfffffff4u
    };
    return handle < 3u ? handles[handle] : 0u;
}

GameAddress runtime_standard_handle_target(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0063c6b0u);
}

void runtime_invalidate_handle_record(GameMemory *memory,
        GameAddress block_slot, uint32_t record_offset) {
    GameAddress block = game_memory_read_u32(memory, block_slot);
    if (!memory->fault)
        game_memory_write_u32(memory, block + record_offset, 0xffffffffu);
}

void runtime_publish_invalid_handle_error(GameMemory *memory) {
    game_memory_write_u32(memory, 0x005a1c5cu, 9u);
    if (!memory->fault) game_memory_write_u32(memory, 0x005a1c60u, 0u);
}

void runtime_publish_handle_block(GameMemory *memory, uint32_t block_index,
        GameAddress block, uint32_t new_limit) {
    game_memory_write_u32(memory, 0x0063b6a0u, new_limit);
    if (!memory->fault)
        game_memory_write_u32(memory, 0x0063b5a0u + block_index * 4u,
            block);
}

void runtime_reserve_handle_record(GameMemory *memory, GameAddress record) {
    game_memory_write_u32(memory, record, 0xffffffffu);
}

RuntimeHandleEntry runtime_handle_entry(GameMemory *memory,
        uint32_t descriptor) {
    RuntimeHandleEntry entry = {0};
    uint32_t limit = runtime_handle_limit(memory);
    uint32_t block_index;
    if (memory->fault || descriptor >= limit) return entry;
    block_index = (descriptor & 0xffffffe7u) >> 3u;
    entry.block_slot = 0x0063b5a0u + block_index;
    entry.block = game_memory_read_u32(memory, entry.block_slot);
    entry.record_offset = (descriptor & 0x1fu) * 8u;
    if (!memory->fault) entry.value = game_memory_read_u32(memory,
        entry.block + entry.record_offset);
    if (!memory->fault) entry.flags = game_memory_read_u8(memory,
        entry.block + entry.record_offset + 4u);
    entry.valid = !memory->fault;
    return entry;
}

void runtime_store_handle_entry(GameMemory *memory,
        const RuntimeHandleEntry *entry, uint32_t value) {
    game_memory_write_u32(memory, entry->block + entry->record_offset, value);
}

void runtime_clear_handle_entry_flags(GameMemory *memory,
        const RuntimeHandleEntry *entry, uint8_t mask) {
    uint8_t flags = game_memory_read_u8(memory,
        entry->block + entry->record_offset + 4u);
    if (!memory->fault) game_memory_write_u8(memory,
        entry->block + entry->record_offset + 4u,
        (uint8_t)(flags & (uint8_t)~mask));
}

GameAddress runtime_address_service_target(GameMemory *memory,
        unsigned stage) {
    static const GameAddress slots[2] = {0x0063c6f8u, 0x0063c6fcu};
    return stage < 2u ? game_memory_read_u32(memory, slots[stage]) : 0u;
}

uint32_t runtime_floating_frame_initialized(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005a3598u);
}

void runtime_mark_floating_refresh(GameMemory *memory) {
    game_memory_write_u8(memory, 0x0063a348u, 1u);
}

RuntimeThreeWordCombine runtime_three_word_combine(GameMemory *memory,
        GameAddress destination, GameAddress source) {
    RuntimeThreeWordCombine result = {{0u, 0u, 0u}, {0u, 0u, 0u}};
    unsigned index;
    for (index = 0u; index < 3u && !memory->fault; ++index) {
        result.destination[index] = game_memory_read_u32(memory,
            destination + index * 4u);
        if (!memory->fault) result.source[index] = game_memory_read_u32(memory,
            source + index * 4u);
    }
    return result;
}

void runtime_store_three_word_carry(GameMemory *memory,
        GameAddress destination, uint32_t value) {
    game_memory_write_u32(memory, destination + 8u, value);
}

uint32_t runtime_three_word_destination(GameMemory *memory,
        GameAddress destination, unsigned index) {
    return index < 3u ? game_memory_read_u32(memory,
        destination + index * 4u) : 0u;
}

RuntimeWindowStatePlan runtime_window_state_plan(GameMemory *memory,
        GameAddress object, int active) {
    RuntimeWindowStatePlan plan = {{0u,0u,0u,0u,0u},0u,0u,0};
    plan.current_state = game_memory_read_u32(memory, object + 0x1cu);
    if (active && plan.current_state != 0u)
        return plan;
    plan.arguments[0] = object + 0x30u;
    plan.arguments[4] = game_memory_read_u32(memory, object + 0x2cu);
    plan.arguments[2] = game_memory_read_u32(memory, object + 0x24u);
    plan.arguments[3] = game_memory_read_u32(memory, object + 0x28u);
    plan.arguments[1] = game_memory_read_u32(memory, object + 0x20u);
    plan.target = game_memory_read_u32(memory, 0x0063c8b8u);
    plan.invoke = !memory->fault;
    return plan;
}

void runtime_window_state_commit(GameMemory *memory, GameAddress object,
        int active) {
    game_memory_write_u32(memory, object + 0x1cu, active ? 1u : 0u);
}

int runtime_resolve_pseudo_handle(GameMemory *memory, uint32_t value,
    RuntimePseudoHandleResult *result) {
    if (!result) return 0;
    game_memory_write_u32(memory, 0x005a31bcu, 0u);
    result->value = value;
    result->tail_target = 0u;
    result->tail_dispatch = 0;
    if (value == 0xfffffffeu || value == 0xfffffffdu) {
        game_memory_write_u32(memory, 0x005a31bcu, 1u);
        result->tail_target = game_memory_read_u32(memory,
            value == 0xfffffffeu ? 0x0063c674u : 0x0063c678u);
        result->tail_dispatch = 1;
    } else if (value == 0xfffffffcu) {
        game_memory_write_u32(memory, 0x005a31bcu, 1u);
        result->value = game_memory_read_u32(memory, 0x005a1ec0u);
    }
    return !memory->fault;
}

int runtime_resolve_punctuation(GameMemory *memory, uint8_t code,
    uint32_t *selector, GameAddress *target) {
    uint32_t index;
    if (!selector || !target || code < 0x21u || code > 0x40u) return 0;
    index = (uint32_t)code - 0x21u;
    *selector = game_memory_read_u8(memory, 0x00479054u + index);
    *target = game_memory_read_u32(memory, 0x0047903cu + *selector * 4u);
    return !memory->fault;
}
