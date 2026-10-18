#include "resource_tree.h"
#include <stdlib.h>
#include <string.h>

static void *host_allocate(void *context, ResourceAllocation kind, size_t bytes) {
    (void)context; (void)kind; return malloc(bytes);
}
static void host_release(void *context, ResourceAllocation kind, void *allocation) {
    (void)context; (void)kind; free(allocation);
}
static void *allocate(ResourceTree *tree, ResourceAllocation kind, size_t bytes) {
    return tree->allocator.allocate(tree->allocator.context, kind, bytes);
}
static void release(ResourceTree *tree, ResourceAllocation kind, void *allocation) {
    if (allocation) tree->allocator.release(tree->allocator.context, kind, allocation);
}
void resource_tree_initialize(ResourceTree *tree, const ResourceAllocator *allocator) {
    memset(tree, 0, sizeof(*tree));
    if (allocator) tree->allocator = *allocator;
    else {
        tree->allocator.allocate = host_allocate;
        tree->allocator.release = host_release;
    }
}
static ResourceTreeStatus make_directory(ResourceTree *tree, const char *segment,
    size_t length, ResourceDirectory **output) {
    ResourceDirectory *directory = allocate(tree, RESOURCE_DIRECTORY, sizeof(*directory));
    if (!directory) return RESOURCE_TREE_DIRECTORY_ALLOCATION_FAILED;
    memset(directory, 0, sizeof(*directory));
    directory->segment = allocate(tree, RESOURCE_DIRECTORY_NAME, length + 1);
    if (!directory->segment) {
        tree->unfinished_directory = directory;
        tree->fatal_status = RESOURCE_TREE_FATAL_DIRECTORY_NAME;
        tree->failure_copy_size = length;
        return tree->fatal_status;
    }
    memcpy(directory->segment, segment, length);
    directory->segment[length] = 0;
    *output = directory;
    return RESOURCE_TREE_OK;
}
ResourceTreeStatus resource_tree_insert(ResourceTree *tree, ResourcePathBytes path,
    ResourceEntryInfo info, const ResourceEntry **output) {
    ResourceScanResult end;
    const char *cursor;
    const char *separator;
    ResourceDirectory **siblings = &tree->roots;
    ResourceDirectory *directory = NULL;
    ResourceEntry *entry;
    size_t length;
    *output = NULL;
    if (tree->fatal_status) return tree->fatal_status;
    end = resource_path_find_byte(path, 0);
    if (end.status != RESOURCE_SCAN_FOUND) return RESOURCE_TREE_INVALID_PATH;
    cursor = (const char *)path.data;
    separator = strchr(cursor, '\\');
    if (!separator) return RESOURCE_TREE_INVALID_PATH;
    do {
        ResourceTreeStatus status;
        length = (size_t)(separator - cursor) + 1;
        while (*siblings && strncmp((*siblings)->segment, cursor, length) != 0)
            siblings = &(*siblings)->next;
        if (!*siblings) {
            status = make_directory(tree, cursor, length, siblings);
            if (status != RESOURCE_TREE_OK) {
                tree->failure_path_offset = (size_t)(cursor - (const char *)path.data);
                return status;
            }
        }
        directory = *siblings;
        cursor += length;
        siblings = &directory->children;
        separator = strchr(cursor, '\\');
    } while (separator);
    entry = allocate(tree, RESOURCE_ENTRY, sizeof(*entry));
    if (!entry) {
        tree->fatal_status = RESOURCE_TREE_FATAL_ENTRY;
        return tree->fatal_status;
    }
    entry->info = info; entry->next = NULL;
    length = strlen(cursor) + 1;
    entry->name = allocate(tree, RESOURCE_ENTRY_NAME, length);
    if (!entry->name) {
        tree->unfinished_entry = entry;
        tree->fatal_status = RESOURCE_TREE_FATAL_ENTRY_NAME;
        tree->failure_copy_size = length;
        return tree->fatal_status;
    }
    memcpy(entry->name, cursor, length);
    entry->next = directory->entries;
    directory->entries = entry;
    *output = entry;
    return RESOURCE_TREE_OK;
}
ResourceTreeStatus resource_tree_query_begin(ResourceTree *tree, ResourcePathBytes path,
    ResourceQuery *query) {
    ResourceScanResult end;
    query->data = NULL; query->size = 0;
    if (tree->fatal_status) return tree->fatal_status;
    end = resource_path_find_byte(path, 0);
    if (end.status != RESOURCE_SCAN_FOUND) return RESOURCE_TREE_INVALID_PATH;
    query->data = allocate(tree, RESOURCE_LOOKUP_BUFFER, end.byte_offset + 1);
    if (!query->data) {
        tree->fatal_status = RESOURCE_TREE_FATAL_LOOKUP_BUFFER;
        tree->failure_copy_size = end.byte_offset + 1;
        return tree->fatal_status;
    }
    query->size = end.byte_offset + 1;
    memcpy(query->data, path.data, query->size);
    return RESOURCE_TREE_OK;
}
void resource_tree_query_end(ResourceTree *tree, ResourceQuery *query) {
    release(tree, RESOURCE_LOOKUP_BUFFER, query->data);
    query->data = NULL; query->size = 0;
}
ResourceTreeStatus resource_tree_query_find(ResourceTree *tree, const ResourceQuery *query,
    const ResourceEntry **output) {
    ResourceDirectory *directory = tree->roots;
    ResourceEntry *entry = NULL;
    ResourcePathBytes bytes = {query->data, query->size};
    const char *cursor, *separator;
    *output = NULL;
    if (tree->fatal_status) return tree->fatal_status;
    if (resource_path_find_byte(bytes, 0).status != RESOURCE_SCAN_FOUND)
        return RESOURCE_TREE_INVALID_PATH;
    cursor = (const char *)query->data;
    separator = strchr(cursor, '\\');
    if (!separator) directory = NULL;
    while (directory && separator) {
        size_t length = (size_t)(separator - cursor) + 1;
        while (directory && strncmp(directory->segment, cursor, length) != 0)
            directory = directory->next;
        if (!directory) break;
        cursor += length;
        separator = strchr(cursor, '\\');
        if (separator) directory = directory->children;
    }
    if (directory) {
        entry = directory->entries;
        while (entry && strcmp(entry->name, cursor) != 0) entry = entry->next;
    }
    *output = entry;
    return entry ? RESOURCE_TREE_OK : RESOURCE_TREE_NOT_FOUND;
}
static ResourceTreeStatus lookup(ResourceTree *tree, ResourcePathBytes path,
    const ResourceEntry **output, int fold_ascii) {
    ResourceQuery query;
    ResourceTreeStatus status = resource_tree_query_begin(tree, path, &query);
    size_t index;
    *output = NULL;
    if (status != RESOURCE_TREE_OK) return status;
    if (fold_ascii) {
        for (index = 0; index < query.size; ++index)
            if (query.data[index] >= 'A' && query.data[index] <= 'Z') query.data[index] += 'a' - 'A';
    }
    status = resource_tree_query_find(tree, &query, output);
    resource_tree_query_end(tree, &query);
    return status;
}
ResourceTreeStatus resource_tree_lookup_c_locale(ResourceTree *tree,
    ResourcePathBytes path, const ResourceEntry **output) {
    return lookup(tree, path, output, 1);
}
ResourceTreeStatus resource_tree_lookup_folded(ResourceTree *tree,
    ResourcePathBytes path, const ResourceEntry **output) {
    return lookup(tree, path, output, 0);
}
static int directory_entry_info(const ResourceDirectory *directory,
    const ResourceEntry *candidate, ResourceEntryInfo *output) {
    for (; directory; directory = directory->next) {
        const ResourceEntry *entry;
        for (entry = directory->entries; entry; entry = entry->next) {
            if (entry == candidate) { *output = entry->info; return 1; }
        }
        if (directory_entry_info(directory->children, candidate, output)) return 1;
    }
    return 0;
}
int resource_tree_entry_info(const ResourceTree *tree, const ResourceEntry *candidate,
    ResourceEntryInfo *output) {
    if (!tree || !candidate || !output || tree->fatal_status != RESOURCE_TREE_OK) return 0;
    return directory_entry_info(tree->roots, candidate, output);
}
static void destroy_directories(ResourceTree *tree, ResourceDirectory *directory) {
    while (directory) {
        ResourceDirectory *next = directory->next;
        ResourceEntry *entry = directory->entries;
        release(tree, RESOURCE_DIRECTORY_NAME, directory->segment);
        while (entry) {
            ResourceEntry *next_entry = entry->next;
            release(tree, RESOURCE_ENTRY_NAME, entry->name);
            release(tree, RESOURCE_ENTRY, entry);
            entry = next_entry;
        }
        destroy_directories(tree, directory->children);
        release(tree, RESOURCE_DIRECTORY, directory);
        directory = next;
    }
}
void resource_tree_destroy(ResourceTree *tree) {
    destroy_directories(tree, tree->roots);
    tree->roots = NULL;
    release(tree, RESOURCE_DIRECTORY, tree->unfinished_directory);
    release(tree, RESOURCE_ENTRY, tree->unfinished_entry);
    tree->unfinished_directory = NULL;
    tree->unfinished_entry = NULL;
    tree->fatal_status = RESOURCE_TREE_OK;
    tree->failure_copy_size = 0;
    tree->failure_path_offset = 0;
}
