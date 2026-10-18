#ifndef UPRISING_RESOURCE_CATALOG_H
#define UPRISING_RESOURCE_CATALOG_H
#include "resource_index.h"
typedef struct ResourceCatalog ResourceCatalog;
/* IDs belong to this catalog, never encode host pointers, and expire at destroy. */
typedef uint32_t ResourceId;
typedef struct ResourceCatalogStats { uint32_t records, skipped; } ResourceCatalogStats;
ResourceCatalog *resource_catalog_create(void);
void resource_catalog_destroy(ResourceCatalog *catalog);
ResourceTreeStatus resource_catalog_insert(ResourceCatalog *catalog, ResourcePathBytes path,
    ResourceEntryInfo info, ResourceId *id);
ResourceTreeStatus resource_catalog_find(ResourceCatalog *catalog, ResourcePathBytes path, ResourceId *id);
int resource_catalog_info(const ResourceCatalog *catalog, ResourceId id, ResourceEntryInfo *info);
/* Append the archive index, consuming its current cursor. C-locale names are the
 * engine resource identity policy, independent of the host filesystem locale.
 * Failure retains earlier published records; destroy/recreate before retry. */
int resource_catalog_read_index(ResourceCatalog *catalog, ResourceArchive *archive, ResourceCatalogStats *stats);
ResourceLoadResult resource_catalog_load(ResourceCatalog *catalog, ResourceArchive *archive,
    ResourceId id, uint8_t *output, size_t capacity, int preserve_position);
#endif
