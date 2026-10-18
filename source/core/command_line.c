#include "command_line.h"

static int command_emit(GameMemory *memory, GameAddress *output,
        GameAddress count_address, uint8_t value) {
    uint32_t count = game_memory_read_u32(memory, count_address);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, count_address, count + 1u);
    if (memory->fault) return 0;
    if (*output) {
        game_memory_write_u8(memory, *output, value);
        if (memory->fault) return 0;
        ++*output;
    }
    return 1;
}

static int command_lead(GameMemory *memory, GameAddress table,
        uint8_t value) {
    uint8_t flags = game_memory_read_u8(memory, table + value);
    return !memory->fault && (flags & 4u) != 0u;
}

static int command_argument(GameMemory *memory, GameAddress *command,
        GameAddress *output, GameAddress chars_address,
        GameAddress lead_byte_table) {
    int quoted = 0;
    for (;;) {
        uint32_t slashes = 0u;
        uint8_t ch = game_memory_read_u8(memory, *command);
        int copy = 1;
        if (memory->fault) return 0;
        while (ch == '\\') {
            ++*command;
            ++slashes;
            ch = game_memory_read_u8(memory, *command);
            if (memory->fault) return 0;
        }
        if (ch == '"') {
            if ((slashes & 1u) == 0u) {
                uint8_t next = 0u;
                if (quoted) {
                    next = game_memory_read_u8(memory, *command + 1u);
                    if (memory->fault) return 0;
                }
                if (quoted && next == '"') ++*command;
                else {
                    copy = 0;
                    quoted = !quoted;
                }
            }
            slashes >>= 1u;
        }
        while (slashes--) {
            if (!command_emit(memory, output, chars_address, '\\')) return 0;
        }
        ch = game_memory_read_u8(memory, *command);
        if (memory->fault) return 0;
        if (ch == 0u || (!quoted && (ch == ' ' || ch == '\t'))) break;
        if (copy) {
            if (command_lead(memory, lead_byte_table, ch)) {
                if (memory->fault || !command_emit(memory, output,
                        chars_address, ch)) return 0;
                ++*command;
                ch = game_memory_read_u8(memory, *command);
                if (memory->fault) return 0;
            }
            if (memory->fault || !command_emit(memory, output,
                    chars_address, ch)) return 0;
        }
        ++*command;
    }
    return command_emit(memory, output, chars_address, 0u);
}

int game_command_line_parse(GameMemory *memory, GameAddress argument_slot,
        GameAddress command, GameAddress argv, GameAddress output,
        GameAddress argc_address, GameAddress chars_address,
        GameAddress lead_byte_table, GameCommandLineResult *result) {
    uint8_t ch;
    if (!memory || !result) return 0;
    game_memory_write_u32(memory, chars_address, 0u);
    game_memory_write_u32(memory, argc_address, 1u);
    if (memory->fault) return 0;
    if (argv) {
        game_memory_write_u32(memory, argv, output);
        argv += 4u;
        game_memory_write_u32(memory, argument_slot, argv);
        if (memory->fault) return 0;
    }
    ch = game_memory_read_u8(memory, command);
    if (memory->fault) return 0;
    if (ch != '"') {
        for (;;) {
            uint8_t first = game_memory_read_u8(memory, command);
            if (memory->fault || !command_emit(memory, &output,
                    chars_address, first)) return 0;
            ++command;
            if (command_lead(memory, lead_byte_table, first)) {
                uint8_t second = game_memory_read_u8(memory, command);
                if (memory->fault || !command_emit(memory, &output,
                        chars_address, second)) return 0;
                ++command;
            }
            if (memory->fault) return 0;
            if (first == ' ' || first == '\t' || first == 0u) {
                if (first == 0u) --command;
                else if (output) game_memory_write_u8(memory, output - 1u, 0u);
                if (memory->fault) return 0;
                break;
            }
        }
    } else {
        ++command;
        for (;;) {
            ch = game_memory_read_u8(memory, command);
            if (memory->fault) return 0;
            if (ch == 0u || ch == '"') break;
            if (command_lead(memory, lead_byte_table, ch)) {
                if (memory->fault || !command_emit(memory, &output,
                        chars_address, ch)) return 0;
                ++command;
                ch = game_memory_read_u8(memory, command);
                if (memory->fault) return 0;
            }
            if (memory->fault || !command_emit(memory, &output,
                    chars_address, ch)) return 0;
            ++command;
        }
        if (!command_emit(memory, &output, chars_address, 0u)) return 0;
        ch = game_memory_read_u8(memory, command);
        if (memory->fault) return 0;
        if (ch == '"') ++command;
    }

    for (;;) {
        uint32_t argc;
        do {
            ch = game_memory_read_u8(memory, command);
            if (memory->fault) return 0;
            if (ch == ' ' || ch == '\t') ++command;
        } while (ch == ' ' || ch == '\t');
        ch = game_memory_read_u8(memory, command);
        if (memory->fault) return 0;
        if (ch == 0u) break;
        if (argv) {
            game_memory_write_u32(memory, argv, output);
            argv += 4u;
            game_memory_write_u32(memory, argument_slot, argv);
            if (memory->fault) return 0;
        }
        argc = game_memory_read_u32(memory, argc_address);
        if (memory->fault) return 0;
        game_memory_write_u32(memory, argc_address, argc + 1u);
        if (memory->fault || !command_argument(memory, &command,
                &output, chars_address, lead_byte_table)) return 0;
    }
    if (argv) game_memory_write_u32(memory, argv, 0u);
    if (memory->fault) return 0;
    result->output_cursor = output;
    result->command_cursor = command;
    return 1;
}
