#include "application_name_registry.h"

#include <stdlib.h>
#include <string.h>

#define NAME_RECORD_BASE (APPLICATION_NAME_VIEW_BASE + 0x30u)
enum { NAME_WIRE_SIZE = 0x30, NAME_LIMIT =
    (APPLICATION_NAME_VIEW_END - NAME_RECORD_BASE) / NAME_WIRE_SIZE };

typedef struct ApplicationName {
    char name[256];
    uint32_t object;
    uint32_t binding;
    uint32_t next;
    uint8_t flags;
} ApplicationName;

typedef struct ApplicationNameRegistry {
    ApplicationName **records;
    uint32_t count, capacity, head;
    ApplicationName descriptor;
    uint8_t descriptor_active;
    ApplicationNameRegistryStats stats;
} ApplicationNameRegistry;

static ApplicationNameRegistry registry;

static uint32_t handle_at(uint32_t index) {
    return NAME_RECORD_BASE + index * NAME_WIRE_SIZE;
}

static ApplicationName *record_at(uint32_t handle) {
    uint32_t offset, index;
    if (handle < NAME_RECORD_BASE ||
            handle >= APPLICATION_NAME_VIEW_END) return NULL;
    offset = handle - NAME_RECORD_BASE;
    if (offset % NAME_WIRE_SIZE) return NULL;
    index = offset / NAME_WIRE_SIZE;
    return index < registry.count ? registry.records[index] : NULL;
}

static unsigned char fold(unsigned char value) {
    return value >= 'A' && value <= 'Z' ?
        (unsigned char)(value + ('a' - 'A')) : value;
}

static int compare(const char *left, const char *right) {
    size_t i;
    for (i = 0; i < 256u; ++i) {
        unsigned char a = fold((unsigned char)left[i]);
        unsigned char b = fold((unsigned char)right[i]);
        if (a != b) return (int)a - (int)b;
        if (!a) return 0;
    }
    return 0;
}

void application_name_registry_reset(void) {
    uint32_t i;
    for (i = 0; i < registry.count; ++i) free(registry.records[i]);
    free(registry.records);
    memset(&registry, 0, sizeof(registry));
}

int application_name_registry_begin(const char name[32]) {
    if (!name || registry.descriptor_active || registry.count ||
            registry.head) return 0;
    memset(&registry.descriptor, 0, sizeof(registry.descriptor));
    memcpy(registry.descriptor.name, name, 32u);
    registry.descriptor_active = 1u;
    return 1;
}

uint32_t application_name_registry_head(void) { return registry.head; }
uint32_t application_name_registry_mark(void) { return registry.count; }
int application_name_registry_is_marker(const char *name) {
    return registry.descriptor_active && name &&
        compare(name, "NULL") == 0;
}

int application_name_registry_binding(uint32_t handle,uint32_t *object,uint32_t *generation) {
    ApplicationName *record=record_at(handle);
    if(!record || !object || !generation)return 0;
    *object=record->object;*generation=record->binding;return 1;
}

uint32_t application_name_registry_find(const char *name) {
    uint32_t cursor = registry.head;
    if (!name) return 0;
    ++registry.stats.lookups;
    while (cursor) {
        ApplicationName *record = record_at(cursor);
        int order;
        if (!record) return 0;
        order = compare(name, record->name);
        if (order == 0) return cursor;
        if (order < 0) return 0;
        cursor = record->next;
    }
    return 0;
}

int application_name_registry_register(const char *name, uint32_t object,
        uint32_t binding, uint32_t *output) {
    uint32_t previous = 0, cursor = registry.head, handle;
    ApplicationName *record;
    if (!registry.descriptor_active || !name || !output ||
            registry.count >= NAME_LIMIT) return 0;
    {
        size_t length = 0;
        while (length < 256u && name[length]) ++length;
        if (length == 256u) return 0;
    }
    /* 4e1a00 compares the immutable NULL literal at596260. The descriptor
       constructor uses strncpy, so bytes beyond dummy's terminator are zero. */
    if (application_name_registry_is_marker(name)) {
        *output = 0;
        return 1;
    }
    while (cursor) {
        ApplicationName *current = record_at(cursor);
        int order;
        if (!current) return 0;
        order = compare(name, current->name);
        if (order == 0) {
            ++registry.stats.duplicates;
            *output = cursor;
            return 1;
        }
        if (order < 0) break;
        previous = cursor;
        cursor = current->next;
    }
    if (registry.count == registry.capacity) {
        uint32_t capacity = registry.capacity ? registry.capacity * 2u : 64u;
        ApplicationName **records;
        if (capacity > NAME_LIMIT) capacity = NAME_LIMIT;
        records = (ApplicationName **)realloc(registry.records,
            (size_t)capacity * sizeof(*records));
        if (!records) return 0;
        registry.records = records;
        registry.capacity = capacity;
    }
    record = (ApplicationName *)calloc(1u, sizeof(*record));
    if (!record) return 0;
    strcpy(record->name, name);
    record->object = object;
    record->binding = binding;
    record->next = cursor;
    handle = handle_at(registry.count);
    registry.records[registry.count++] = record;
    if (previous) record_at(previous)->next = handle;
    else registry.head = handle;
    ++registry.stats.registrations;
    ++registry.stats.live;
    *output = handle;
    return 1;
}

int application_name_registry_release_since(uint32_t mark) {
    uint32_t previous = 0, cursor = registry.head, visited = 0;
    if (mark > registry.count) return 0;
    while (cursor) {
        uint32_t index = (cursor - NAME_RECORD_BASE) / NAME_WIRE_SIZE;
        ApplicationName *record = record_at(cursor);
        uint32_t next;
        if (!record || ++visited > registry.count) return 0;
        next = record->next;
        if (index >= mark) {
            if (previous) record_at(previous)->next = next;
            else registry.head = next;
            registry.records[index] = NULL;
            free(record);
            ++registry.stats.released;
            ++registry.stats.level_released;
            --registry.stats.live;
        } else previous = cursor;
        cursor = next;
    }
    return 1;
}

void application_name_registry_shutdown(void) {
    uint32_t i;
    for (i = 0; i < registry.count; ++i) {
        if (registry.records[i]) {
            free(registry.records[i]);
            ++registry.stats.released;
            --registry.stats.live;
        }
    }
    free(registry.records);
    registry.records = NULL;
    registry.count = registry.capacity = registry.head = 0;
    memset(&registry.descriptor, 0, sizeof(registry.descriptor));
    registry.descriptor_active = 0;
}

static void write_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static ApplicationName *view_record(uint32_t address, size_t size,
        uint32_t *offset) {
    uint32_t relative, index;
    if (address < APPLICATION_NAME_VIEW_BASE ||
            address >= APPLICATION_NAME_VIEW_END) return NULL;
    relative = address - APPLICATION_NAME_VIEW_BASE;
    if (relative < NAME_WIRE_SIZE) {
        *offset = relative;
        return registry.descriptor_active &&
            size <= NAME_WIRE_SIZE - *offset ? &registry.descriptor : NULL;
    }
    relative -= NAME_WIRE_SIZE;
    index = relative / NAME_WIRE_SIZE;
    *offset = relative % NAME_WIRE_SIZE;
    if (index >= registry.count || size > NAME_WIRE_SIZE - *offset)
        return NULL;
    return registry.records[index];
}

static void serialize(const ApplicationName *record,
        uint8_t bytes[NAME_WIRE_SIZE]) {
    memset(bytes, 0, NAME_WIRE_SIZE);
    memcpy(bytes, record->name, 32u);
    write_u32(bytes + 0x20u, record->object);
    write_u32(bytes + 0x24u, record->binding);
    write_u32(bytes + 0x28u, record->next);
    bytes[0x2cu] = record->flags;
}

int application_name_registry_read(uint32_t address, void *output,
        size_t size) {
    uint32_t offset;
    ApplicationName *record = view_record(address, size, &offset);
    uint8_t bytes[NAME_WIRE_SIZE];
    if (!record || !output) return 0;
    serialize(record, bytes);
    if (record == &registry.descriptor)
        write_u32(bytes + 0x28u, registry.head);
    memcpy(output, bytes + offset, size);
    return 1;
}

int application_name_registry_write(uint32_t address, const void *input,
        size_t size) {
    uint32_t offset;
    ApplicationName *record = view_record(address, size, &offset);
    uint8_t bytes[NAME_WIRE_SIZE];
    if (!record || !input) return 0;
    serialize(record, bytes);
    if (record == &registry.descriptor)
        write_u32(bytes + 0x28u, registry.head);
    memcpy(bytes + offset, input, size);
    memcpy(record->name, bytes, 32u);
    record->name[32] = '\0';
    record->object = read_u32(bytes + 0x20u);
    record->binding = read_u32(bytes + 0x24u);
    record->next = read_u32(bytes + 0x28u);
    record->flags = bytes[0x2cu];
    if (record == &registry.descriptor) registry.head = record->next;
    return 1;
}

void application_name_registry_stats(ApplicationNameRegistryStats *output) {
    if (output) *output = registry.stats;
}
