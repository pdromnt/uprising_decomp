#ifndef UPRISING_RESOURCE_BLOB_H
#define UPRISING_RESOURCE_BLOB_H
#include "resource_archive.h"
typedef struct ResourceBlob {
    uint8_t *data;
    size_t size;
    ResourceBufferAllocator allocator;
} ResourceBlob;
/* Pass empty zero-initialized output. Copies allocator ownership; the allocator
 * context must survive release. Failure leaves output empty. */
ResourceLoadResult resource_blob_load(ResourceArchive *archive, ResourceEntryInfo info,
    int preserve_position, ResourceBlob *output);
void resource_blob_release(ResourceBlob *blob);
/* Loose asset file, bounded to 64 MiB. Owns its bytes until blob release;
 * failure leaves the zero-initialized output empty. */
int resource_blob_open_file(const char *path,ResourceBlob *output);
/* Bootstrap access before the shared catalog exists. Opens/closes its own file,
 * scans the index for the FIRST matching usable C-locale name, then loads it.
 * Returns 1 on success, 0 missing, -1 invalid/read/allocation failure. */
int resource_blob_open_named(const char *archive_path, const char *resource_name,
    ResourceBlob *output, uint32_t *records_scanned);
#endif
