#ifndef UPRISING_RESOURCE_TREE_H
#define UPRISING_RESOURCE_TREE_H
#include "resource_path.h"

typedef struct ResourceEntryInfo {
    uint32_t unpacked_size;
    uint32_t packed_size; /* zero means stored verbatim */
    uint32_t archive_header_offset; /* payload follows a 12-byte header */
} ResourceEntryInfo;

typedef struct ResourceEntry {
    char *name;
    ResourceEntryInfo info;
    struct ResourceEntry *next;
} ResourceEntry;
typedef struct ResourceDirectory {
    ResourceEntry *entries;
    char *segment; /* includes its trailing backslash */
    struct ResourceDirectory *next;
    struct ResourceDirectory *children;
} ResourceDirectory;

typedef enum ResourceAllocation {
    RESOURCE_DIRECTORY, RESOURCE_DIRECTORY_NAME, RESOURCE_ENTRY,
    RESOURCE_ENTRY_NAME, RESOURCE_LOOKUP_BUFFER
} ResourceAllocation;
/* Host allocation service only; implementations must not re-enter guest code.
 * Returned storage has malloc alignment and remains owned by this tree. */
typedef struct ResourceAllocator {
    void *context;
    void *(*allocate)(void *context, ResourceAllocation kind, size_t bytes);
    void (*release)(void *context, ResourceAllocation kind, void *allocation);
} ResourceAllocator;
typedef enum ResourceTreeStatus {
    RESOURCE_TREE_OK, RESOURCE_TREE_NOT_FOUND, RESOURCE_TREE_INVALID_PATH,
    RESOURCE_TREE_DIRECTORY_ALLOCATION_FAILED,
    RESOURCE_TREE_FATAL_DIRECTORY_NAME, RESOURCE_TREE_FATAL_ENTRY,
    RESOURCE_TREE_FATAL_ENTRY_NAME, RESOURCE_TREE_FATAL_LOOKUP_BUFFER
} ResourceTreeStatus;
typedef struct ResourceTree {
    ResourceDirectory *roots;
    ResourceAllocator allocator;
    ResourceTreeStatus fatal_status;
    size_t failure_copy_size; /* attempted name/query copy at a fatal allocation */
    size_t failure_path_offset; /* directory segment within the supplied path */
    /* Unpublished allocations remain owned after a fatal allocation failure.
     * Only destruction is allowed after fatal_status is set. */
    ResourceDirectory *unfinished_directory;
    ResourceEntry *unfinished_entry;
} ResourceTree;

/* Caller-owned query storage allocated by the tree's host allocator. Keep the
 * tree/allocator alive and call query_end even if external normalization fails. */
typedef struct ResourceQuery {
    uint8_t *data;
    size_t size;
} ResourceQuery;
ResourceTreeStatus resource_tree_query_begin(ResourceTree *tree, ResourcePathBytes path,
    ResourceQuery *query);
ResourceTreeStatus resource_tree_query_find(ResourceTree *tree, const ResourceQuery *query,
    const ResourceEntry **entry);
void resource_tree_query_end(ResourceTree *tree, ResourceQuery *query);

void resource_tree_initialize(ResourceTree *tree, const ResourceAllocator *allocator);
/* Input is borrowed, already case-folded, NUL-terminated within readable_size,
 * and contains at least one backslash. Names are copied; duplicate entries
 * prepend. Returned entry is borrowed until destruction, never serialized. */
ResourceTreeStatus resource_tree_insert(ResourceTree *tree, ResourcePathBytes path,
    ResourceEntryInfo info, const ResourceEntry **entry);
/* Default C-locale query: ASCII A-Z fold; all other bytes are unchanged.
 * Non-default locale mapping belongs at the future archive service boundary. */
ResourceTreeStatus resource_tree_lookup_c_locale(ResourceTree *tree,
    ResourcePathBytes path, const ResourceEntry **entry);
/* Query has already been normalized by the archive's locale service. */
ResourceTreeStatus resource_tree_lookup_folded(ResourceTree *tree,
    ResourcePathBytes path, const ResourceEntry **entry);
/* Copy metadata only for a published entry owned by this live tree. The
 * candidate is compared, never dereferenced before ownership is established.
 * Failure leaves output unchanged. Borrowed pointers expire at destruction. */
int resource_tree_entry_info(const ResourceTree *tree, const ResourceEntry *candidate,
    ResourceEntryInfo *output);
void resource_tree_destroy(ResourceTree *tree);
#endif
