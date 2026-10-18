#ifndef UPRISING_RESOURCE_INDEX_H
#define UPRISING_RESOURCE_INDEX_H
#include "resource_archive.h"
typedef enum ResourceIndexStatus {
    RESOURCE_INDEX_OK, RESOURCE_INDEX_INVALID_ARGUMENT, RESOURCE_INDEX_SHORT_READ,
    RESOURCE_INDEX_INVALID_COUNT, RESOURCE_INDEX_LENGTH_OVERFLOW,
    RESOURCE_INDEX_ALLOCATION_FAILED
} ResourceIndexStatus;
typedef struct ResourceIndexRecord {
    uint8_t *name; /* owns length+1 bytes; counted bytes plus appended NUL */
    size_t length;
    ResourceEntryInfo info;
    ResourceBufferAllocator allocator;
} ResourceIndexRecord;
/* Read explicit little-endian scalars; never serialize a native struct. Count
 * is bounded to the retail positive signed range. Failures consume bytes read
 * but do not publish a partial output. Host-only archive callbacks are required. */
ResourceIndexStatus resource_index_read_count(ResourceArchive *archive,uint32_t *count);
/* Output must be initialized to zero and empty. Name bytes are not normalized;
 * embedded NUL and high bytes are retained. The allocator context must outlive
 * the record. The caller must release records, including unusable ones. */
ResourceIndexStatus resource_index_read_record(ResourceArchive *archive,ResourceIndexRecord *record);
void resource_index_record_fold_c_locale(ResourceIndexRecord *record);
int resource_index_record_usable(const ResourceIndexRecord *record);
void resource_index_record_release(ResourceIndexRecord *record);
#endif
