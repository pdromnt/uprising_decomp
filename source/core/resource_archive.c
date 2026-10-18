#include "resource_archive.h"
#include "resource_payload.h"
#include <stdlib.h>
#include <string.h>
static void *default_allocate(void *context, size_t bytes) { (void)context; return malloc(bytes); }
static void default_release(void *context, void *pointer) { (void)context; free(pointer); }
int resource_archive_initialize(ResourceArchive *archive, ResourceStream stream,
    int owns_stream, const ResourceBufferAllocator *allocator) {
    ResourceBufferAllocator selected = {NULL, default_allocate, default_release};
    if (!archive || !stream.read || !stream.seek || (owns_stream && !stream.close)) return 0;
    if (allocator) { if (!allocator->allocate || !allocator->release) return 0; selected = *allocator; }
    archive->stream = stream; archive->allocator = selected;
    archive->active = 1; archive->owns_stream = owns_stream != 0;
    return 1;
}
void resource_archive_destroy(ResourceArchive *archive) {
    if (!archive || !archive->active) return;
    if (archive->owns_stream) archive->stream.close(archive->stream.context);
    memset(archive, 0, sizeof(*archive));
}
ResourceLoadResult resource_archive_load(ResourceArchive *archive,
    ResourceEntryInfo info, uint8_t *output, size_t capacity, int preserve_position) {
    ResourceLoadResult result = {RESOURCE_LOAD_OK, 0};
    uint8_t header[12], *scratch = NULL;
    size_t bytes = info.packed_size ? info.packed_size : info.unpacked_size;
    uint64_t saved = 0;
    if (!archive || !archive->active || capacity < info.unpacked_size ||
        (!output && info.unpacked_size) || (preserve_position && !archive->stream.tell)) {
        result.status = RESOURCE_LOAD_INVALID_ARGUMENT; return result;
    }
    if (preserve_position && !archive->stream.tell(archive->stream.context, &saved)) {
        result.status = RESOURCE_LOAD_TELL_FAILED; return result;
    }
    if (bytes) {
        scratch = archive->allocator.allocate(archive->allocator.context, bytes);
        if (!scratch) { result.status = RESOURCE_LOAD_ALLOCATION_FAILED; return result; }
    }
    if (!archive->stream.seek(archive->stream.context, info.archive_header_offset))
        result.status = RESOURCE_LOAD_SEEK_FAILED;
    else if (archive->stream.read(archive->stream.context, header, sizeof(header)) != sizeof(header))
        result.status = RESOURCE_LOAD_SHORT_READ;
    else if (bytes && archive->stream.read(archive->stream.context, scratch, bytes) != bytes)
        result.status = RESOURCE_LOAD_SHORT_READ;
    else if (info.packed_size) {
        if (resource_payload_decode_rle(scratch, bytes, output, info.unpacked_size) != RESOURCE_PAYLOAD_OK)
            result.status = RESOURCE_LOAD_INVALID_PAYLOAD;
    } else if (bytes) memcpy(output, scratch, bytes);
    if (scratch) archive->allocator.release(archive->allocator.context, scratch);
    if (preserve_position && !archive->stream.seek(archive->stream.context, saved)) {
        result.restore_failed = 1;
        if (result.status == RESOURCE_LOAD_OK) result.status = RESOURCE_LOAD_RESTORE_FAILED;
    }
    return result;
}
