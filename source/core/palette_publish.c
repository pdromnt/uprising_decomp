#include "palette_publish.h"

int game_palette_publish_rgb(GameMemory *memory, GameAddress source,
    GameAddress destination, GamePalettePublication *publication) {
    unsigned entry, component;
    uint8_t value = 0u;
    if (!memory || !publication) return 0;
    for (entry = 0u; entry < 256u; ++entry) {
        for (component = 0u; component < 3u; ++component) {
            value = game_memory_read_u8(memory,
                source + entry * 3u + component);
            if (memory->fault) return 0;
            game_memory_write_u8(memory,
                destination + entry * 4u + component, value);
            if (memory->fault) return 0;
        }
    }
    publication->source_end = source + 256u * 3u;
    publication->destination_end = destination + 256u * 4u;
    publication->final_component = value;
    return 1;
}

int game_palette_initialize_332(GameMemory *memory, GameAddress destination) {
    unsigned index;
    if (!memory) return 0;
    for (index = 0u; index < 256u; ++index) {
        GameAddress entry = destination + index * 4u;
        game_memory_write_u8(memory, entry,
            (uint8_t)(((index >> 5u) * 255u) / 7u));
        game_memory_write_u8(memory, entry + 1u,
            (uint8_t)((((index >> 2u) & 7u) * 255u) / 7u));
        game_memory_write_u8(memory, entry + 2u,
            (uint8_t)(((index & 3u) * 255u) / 3u));
        game_memory_write_u8(memory, entry + 3u, 0u);
        if (memory->fault) return 0;
    }
    return 1;
}
