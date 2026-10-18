#ifndef UPRISING_GAME_MEMORY_H
#define UPRISING_GAME_MEMORY_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t GameAddress;

typedef int (*GameMemoryReadFallback)(void *context, GameAddress address,
    void *destination, size_t size);
typedef int (*GameMemoryWriteFallback)(void *context, GameAddress address,
    const void *source, size_t size);
typedef uint32_t *(*GameMemoryIndexView)(void *context,GameAddress address);
typedef float *(*GameMemoryFloatView)(void *context,GameAddress address);
struct RenderArenaPacket;
typedef struct RenderArenaPacket *(*GameMemoryPacketView)(void *context,
    GameAddress address);
typedef void (*GameMemoryTypedWrite)(void *context,GameAddress address);

typedef struct GameMemory {
    uint8_t *data;
    GameAddress base;
    size_t size;
    void *fallback_context;
    GameMemoryReadFallback read_fallback;
    GameMemoryWriteFallback write_fallback;
    GameMemoryIndexView index_view;
    GameMemoryFloatView float_view;
    GameMemoryPacketView packet_view;
    GameMemoryTypedWrite note_typed_write;
    int trace_writes;
    int fault;
    GameAddress fault_address;
    size_t fault_size;
    int fault_write;
    struct TerrainAsset *terrain;
    struct SceneSurface *surface;
    GameAddress owned_begin, owned_end;
    const uint16_t *owned_pages; /* Optional 4 KiB page ownership counts. */
} GameMemory;

void game_memory_initialize(GameMemory *memory, uint8_t *data,
    GameAddress base, size_t size);
int game_memory_contains(const GameMemory *memory, GameAddress address,
    size_t size);
int game_memory_read(GameMemory *memory, GameAddress address,
    void *destination, size_t size);
int game_memory_write(GameMemory *memory, GameAddress address,
    const void *source, size_t size);
uint8_t game_memory_read_u8(GameMemory *memory, GameAddress address);
uint16_t game_memory_read_u16(GameMemory *memory, GameAddress address);
uint32_t game_memory_read_u32(GameMemory *memory, GameAddress address);
void game_memory_write_u8(GameMemory *memory, GameAddress address,
    uint8_t value);
void game_memory_write_u16(GameMemory *memory, GameAddress address,
    uint16_t value);
void game_memory_write_u32(GameMemory *memory, GameAddress address,
    uint32_t value);
int game_memory_fill_u32(GameMemory *memory, GameAddress address,
    uint32_t count, uint32_t value, int reverse);
int game_memory_fill_u8(GameMemory *memory, GameAddress address,
    uint32_t count, uint8_t value, int reverse);
uint32_t game_memory_copy_elements(GameMemory *memory,
    GameAddress destination, GameAddress source, uint32_t count,
    unsigned element_bytes, int reverse);
uint32_t game_memory_fill_elements(GameMemory *memory,
    GameAddress destination, uint32_t count, uint32_t value,
    unsigned element_bytes, int reverse);
typedef struct GameByteScan {
    uint32_t scanned;
    uint8_t last;
} GameByteScan;
GameByteScan game_memory_scan_byte(GameMemory *memory, GameAddress address,
    uint32_t count, uint8_t needle, int reverse);

#endif
