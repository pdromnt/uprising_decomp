#ifndef UPRISING_RUNTIME_UTILITIES_H
#define UPRISING_RUNTIME_UTILITIES_H
#include "game_memory.h"

typedef struct RuntimeResourceDescriptor {
    GameAddress owner;
    uint32_t value;
    GameAddress format;
} RuntimeResourceDescriptor;

int runtime_set_mode_enabled(GameMemory *memory, uint16_t mode);
uint32_t runtime_register_pointer(GameMemory *memory, GameAddress value);
void runtime_reset_registration_array(GameMemory *memory, int backwards);
void runtime_update_viewport_bounds(GameMemory *memory, int quarter_lower);
void runtime_reset_presentation_pair(GameMemory *memory);
RuntimeResourceDescriptor runtime_resource_descriptor(GameMemory *memory,
        unsigned index);
GameAddress runtime_display_descriptor(GameMemory *memory);
void runtime_publish_display_descriptor(GameMemory *memory,
        GameAddress descriptor);
void runtime_store_display_descriptor(GameMemory *memory,
        GameAddress descriptor, uint16_t width, uint16_t height,
        GameAddress handle);
#endif
