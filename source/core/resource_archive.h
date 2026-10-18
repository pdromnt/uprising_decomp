#ifndef UPRISING_RESOURCE_ARCHIVE_H
#define UPRISING_RESOURCE_ARCHIVE_H
#include "resource_tree.h"
/* Host services only: callbacks must not re-enter the guest CPU. An archive is
 * single-threaded, non-reentrant, and must outlive any operation using it. */
typedef struct ResourceStream {
    void *context;
    size_t (*read)(void *context, void *output, size_t bytes);
    int (*seek)(void *context, uint64_t absolute_offset);
    int (*tell)(void *context, uint64_t *absolute_offset);
    void (*close)(void *context);
} ResourceStream;
typedef struct ResourceBufferAllocator {
    void *context;
    void *(*allocate)(void *context, size_t bytes);
    void (*release)(void *context, void *pointer);
} ResourceBufferAllocator;
typedef struct ResourceArchive {
    ResourceStream stream;
    ResourceBufferAllocator allocator;
    int active, owns_stream;
} ResourceArchive;
typedef enum ResourceLoadStatus {
    RESOURCE_LOAD_OK, RESOURCE_LOAD_INVALID_ARGUMENT, RESOURCE_LOAD_ALLOCATION_FAILED,
    RESOURCE_LOAD_TELL_FAILED, RESOURCE_LOAD_SEEK_FAILED, RESOURCE_LOAD_SHORT_READ,
    RESOURCE_LOAD_INVALID_PAYLOAD, RESOURCE_LOAD_RESTORE_FAILED
} ResourceLoadStatus;
typedef struct ResourceLoadResult {
    ResourceLoadStatus status;
    int restore_failed; /* also reported when an earlier primary error wins */
} ResourceLoadResult;
/* Storage is caller-owned. Initialize only fresh/destroyed storage; ownership
 * transfers only on success. NULL allocator selects malloc/free. Destroy closes
 * only an owned stream, exactly once; a borrowed stream stays with its caller. */
int resource_archive_initialize(ResourceArchive *archive, ResourceStream stream,
    int owns_stream, const ResourceBufferAllocator *allocator);
void resource_archive_destroy(ResourceArchive *archive);
/* Output is a borrowed nonaliasing span, at least info.unpacked_size bytes.
 * No output byte is changed unless the complete payload was read and validated.
 * Scratch is released on every path. If preserve_position is nonzero, tell is
 * required and restoration is attempted after any seek attempt, even on error.
 * Successful decode followed by restore failure leaves valid output and reports
 * RESTORE_FAILED. Error policies are explicit, not malformed retail emulation. */
ResourceLoadResult resource_archive_load(ResourceArchive *archive,
    ResourceEntryInfo info, uint8_t *output, size_t capacity, int preserve_position);
#endif
