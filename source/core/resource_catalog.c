#include "resource_catalog.h"
#include <stdlib.h>
struct ResourceCatalog {
    ResourceTree tree;
    const ResourceEntry **entries;
    size_t count, capacity;
};
ResourceCatalog *resource_catalog_create(void) {
    ResourceCatalog *catalog = calloc(1, sizeof(*catalog));
    if (catalog) resource_tree_initialize(&catalog->tree, NULL);
    return catalog;
}
void resource_catalog_destroy(ResourceCatalog *catalog) {
    if (!catalog) return;
    resource_tree_destroy(&catalog->tree);
    free(catalog->entries);
    free(catalog);
}
ResourceTreeStatus resource_catalog_insert(ResourceCatalog *catalog, ResourcePathBytes path,
    ResourceEntryInfo info, ResourceId *id) {
    const ResourceEntry *entry;
    ResourceTreeStatus status;
    *id = 0;
    if (!catalog || catalog->count == UINT32_MAX) return RESOURCE_TREE_FATAL_ENTRY;
    if (catalog->count == catalog->capacity) {
        size_t capacity = catalog->capacity ? catalog->capacity * 2 : 256;
        const ResourceEntry **entries;
        if (capacity < catalog->capacity || capacity > SIZE_MAX / sizeof(*entries))
            return RESOURCE_TREE_FATAL_ENTRY;
        entries = realloc(catalog->entries, capacity * sizeof(*entries));
        if (!entries) return RESOURCE_TREE_FATAL_ENTRY;
        catalog->entries = entries; catalog->capacity = capacity;
    }
    status = resource_tree_insert(&catalog->tree, path, info, &entry);
    if (status != RESOURCE_TREE_OK) return status;
    catalog->entries[catalog->count++] = entry;
    *id = (ResourceId)catalog->count;
    return RESOURCE_TREE_OK;
}
ResourceTreeStatus resource_catalog_find(ResourceCatalog *catalog, ResourcePathBytes path, ResourceId *id) {
    const ResourceEntry *entry;
    ResourceTreeStatus status;
    size_t index;
    *id = 0;
    if (!catalog) return RESOURCE_TREE_NOT_FOUND;
    status = resource_tree_lookup_c_locale(&catalog->tree, path, &entry);
    if (status != RESOURCE_TREE_OK) return status;
    for (index = catalog->count; index; --index) if (catalog->entries[index-1] == entry) {
        *id = (ResourceId)index; return RESOURCE_TREE_OK;
    }
    return RESOURCE_TREE_NOT_FOUND;
}
int resource_catalog_info(const ResourceCatalog *catalog, ResourceId id, ResourceEntryInfo *info) {
    if (!catalog || !id || id > catalog->count || !info) return 0;
    *info = catalog->entries[id-1]->info;
    return 1;
}
int resource_catalog_read_index(ResourceCatalog *catalog, ResourceArchive *archive, ResourceCatalogStats *stats) {
    uint32_t count, index;
    stats->records = stats->skipped = 0;
    if (!catalog || resource_index_read_count(archive, &count) != RESOURCE_INDEX_OK) return 0;
    for (index = 0; index < count; ++index) {
        ResourceIndexRecord record = {0};
        ResourceTreeStatus status;
        ResourceId id;
        ResourcePathBytes path;
        if (resource_index_read_record(archive, &record) != RESOURCE_INDEX_OK) return 0;
        ++stats->records;
        if (!resource_index_record_usable(&record)) {
            ++stats->skipped; resource_index_record_release(&record); continue;
        }
        resource_index_record_fold_c_locale(&record);
        path.data = record.name; path.readable_size = record.length + 1;
        status = resource_catalog_insert(catalog, path, record.info, &id);
        resource_index_record_release(&record);
        if (status != RESOURCE_TREE_OK && status != RESOURCE_TREE_DIRECTORY_ALLOCATION_FAILED) return 0;
    }
    return 1;
}
ResourceLoadResult resource_catalog_load(ResourceCatalog *catalog, ResourceArchive *archive,
    ResourceId id, uint8_t *output, size_t capacity, int preserve_position) {
    ResourceEntryInfo info;
    ResourceLoadResult invalid = {RESOURCE_LOAD_INVALID_ARGUMENT, 0};
    if (!resource_catalog_info(catalog, id, &info)) return invalid;
    return resource_archive_load(archive, info, output, capacity, preserve_position);
}
