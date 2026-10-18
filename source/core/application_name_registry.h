#ifndef UPRISING_APPLICATION_NAME_REGISTRY_H
#define UPRISING_APPLICATION_NAME_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

/* External x86 readers see serialized records in this reserved address range.
   The authoritative name, binding, links and flags live in native records. */
#define APPLICATION_NAME_VIEW_BASE 0xfe000000u
#define APPLICATION_NAME_VIEW_END  0xfe400000u
#define APPLICATION_NAME_DESCRIPTOR APPLICATION_NAME_VIEW_BASE

typedef struct ApplicationNameRegistryStats {
    uint32_t registrations;
    uint32_t duplicates;
    uint32_t released;
    uint32_t level_released;
    uint32_t live;
    uint32_t lookups;
} ApplicationNameRegistryStats;

void application_name_registry_reset(void);
int application_name_registry_begin(const char name[32]);
int application_name_registry_is_marker(const char *name);
int application_name_registry_register(const char *name, uint32_t object,
    uint32_t binding, uint32_t *record);
uint32_t application_name_registry_find(const char *name);
int application_name_registry_binding(uint32_t record,uint32_t *object,uint32_t *generation);
uint32_t application_name_registry_head(void);
uint32_t application_name_registry_mark(void);
int application_name_registry_release_since(uint32_t mark);
void application_name_registry_shutdown(void);
int application_name_registry_read(uint32_t address, void *output, size_t size);
int application_name_registry_write(uint32_t address, const void *input,
    size_t size);
void application_name_registry_stats(ApplicationNameRegistryStats *output);

#endif
