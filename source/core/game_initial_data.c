#include "game_initial_data.h"
#include <stdlib.h>
#include <string.h>

typedef struct InitialWord {
    GameAddress address;
    uint32_t value;
} InitialWord;
typedef struct InitialString {
    GameAddress address;
    const char *text;
} InitialString;

/* Recovered declarations, not a PE image. Zero-initialized state is supplied
 * by the owner below; strings and numeric bit patterns are explicit. Selector
 * values are consumed by the existing ordinary-C dispatchers. No executable
 * sections, PE headers, imports, resources or relocation data are retained. */
static const InitialWord initial_values[] = {
#include "initial_numeric_records.inc"
};
static const InitialWord initial_selectors[] = {
#include "initial_method_ids.inc"
};
static const InitialString initial_strings[] = {
#include "initial_strings.inc"
};

enum { INITIAL_BASE = 0x400000u, INITIAL_SIZE = 0x400000u };

static int initialize_words(unsigned char *data, const InitialWord *words,
        size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        uint32_t offset = words[i].address - INITIAL_BASE;
        uint32_t value = words[i].value;
        if (words[i].address < INITIAL_BASE || offset > INITIAL_SIZE - 4u)
            return 0;
        data[offset] = (unsigned char)value;
        data[offset + 1u] = (unsigned char)(value >> 8u);
        data[offset + 2u] = (unsigned char)(value >> 16u);
        data[offset + 3u] = (unsigned char)(value >> 24u);
    }
    return 1;
}

int game_initial_data_create(GameMemory *memory) {
    unsigned char *data;
    size_t i;
    if (!memory) return 0;
    data = calloc(1, INITIAL_SIZE);
    if (!data) return 0;
    if (!initialize_words(data, initial_values,
            sizeof(initial_values) / sizeof(initial_values[0])) ||
        !initialize_words(data, initial_selectors,
            sizeof(initial_selectors) / sizeof(initial_selectors[0]))) goto fail;
    for (i = 0; i < sizeof(initial_strings) / sizeof(initial_strings[0]); ++i) {
        uint32_t offset = initial_strings[i].address - INITIAL_BASE;
        size_t length = strlen(initial_strings[i].text) + 1u;
        if (initial_strings[i].address < INITIAL_BASE ||
            offset > INITIAL_SIZE || length > INITIAL_SIZE - offset) goto fail;
        memcpy(data + offset, initial_strings[i].text, length);
    }
    game_memory_initialize(memory, data, INITIAL_BASE, INITIAL_SIZE);
    return 1;
fail:
    free(data);
    return 0;
}
