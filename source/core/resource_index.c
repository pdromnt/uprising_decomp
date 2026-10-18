#include "resource_index.h"
#include <string.h>
static int read_word(ResourceArchive *archive,uint32_t *value) {
    uint8_t bytes[4];
    if(archive->stream.read(archive->stream.context,bytes,4)!=4)return 0;
    *value=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8|(uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;
    return 1;
}
ResourceIndexStatus resource_index_read_count(ResourceArchive *archive,uint32_t *count) {
    uint32_t value;
    if(!archive || !archive->active || !count)return RESOURCE_INDEX_INVALID_ARGUMENT;
    if(!read_word(archive,&value))return RESOURCE_INDEX_SHORT_READ;
    if(value>INT32_MAX)return RESOURCE_INDEX_INVALID_COUNT;
    *count=value;return RESOURCE_INDEX_OK;
}
ResourceIndexStatus resource_index_read_record(ResourceArchive *archive,ResourceIndexRecord *record) {
    ResourceIndexRecord result={0};uint32_t length;
    if(!archive || !archive->active || !record || record->name)return RESOURCE_INDEX_INVALID_ARGUMENT;
    if(!read_word(archive,&length))return RESOURCE_INDEX_SHORT_READ;
    result.length=length;
    if(result.length==SIZE_MAX)return RESOURCE_INDEX_LENGTH_OVERFLOW;
    result.allocator=archive->allocator;
    result.name=result.allocator.allocate(result.allocator.context,result.length+1);
    if(!result.name)return RESOURCE_INDEX_ALLOCATION_FAILED;
    if(archive->stream.read(archive->stream.context,result.name,result.length)!=result.length ||
        !read_word(archive,&result.info.unpacked_size) || !read_word(archive,&result.info.packed_size) ||
        !read_word(archive,&result.info.archive_header_offset)) {
        result.allocator.release(result.allocator.context,result.name);
        return RESOURCE_INDEX_SHORT_READ;
    }
    result.name[result.length]=0;
    *record=result;return RESOURCE_INDEX_OK;
}
int resource_index_record_usable(const ResourceIndexRecord *record) {
    return record && record->name && record->name[0] && record->info.unpacked_size && record->info.archive_header_offset;
}
void resource_index_record_release(ResourceIndexRecord *record) {
    if(!record)return;
    if(record->name)record->allocator.release(record->allocator.context,record->name);
    memset(record,0,sizeof(*record));
}

void resource_index_record_fold_c_locale(ResourceIndexRecord *record) {
    size_t index;
    if(!record || !record->name)return;
    for(index=0;index<record->length && record->name[index];index++)
        if(record->name[index]>='A' && record->name[index]<='Z')record->name[index]+='a'-'A';
}
