#include "resource_blob.h"
#include "resource_file.h"
#include "resource_index.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static void file_blob_free(void *context,void *data){(void)context;free(data);}
int resource_blob_open_file(const char *path,ResourceBlob *output){
    ResourceBlob blob={0};FILE *file;long length;int result=0;
    if(!path || !output || output->data)return 0;
    file=fopen(path,"rb");if(!file)return 0;
    if(fseek(file,0,SEEK_END) || (length=ftell(file))<0 ||
        length>64L*1024L*1024L || fseek(file,0,SEEK_SET))goto done;
    blob.allocator.release=file_blob_free;blob.size=(size_t)length;
    blob.data=malloc(blob.size?blob.size:1u);
    if(!blob.data || fread(blob.data,1u,blob.size,file)!=blob.size)goto done;
    *output=blob;memset(&blob,0,sizeof(blob));result=1;
done:
    resource_blob_release(&blob);fclose(file);return result;
}
ResourceLoadResult resource_blob_load(ResourceArchive *archive, ResourceEntryInfo info,
    int preserve_position, ResourceBlob *output) {
    ResourceLoadResult result={RESOURCE_LOAD_INVALID_ARGUMENT,0};ResourceBlob blob={0};
    if(!archive || !archive->active || !output || output->data)return result;
    blob.allocator=archive->allocator;blob.size=info.unpacked_size;
    if(blob.size) {
        blob.data=blob.allocator.allocate(blob.allocator.context,blob.size);
        if(!blob.data){result.status=RESOURCE_LOAD_ALLOCATION_FAILED;return result;}
    }
    result=resource_archive_load(archive,info,blob.data,blob.size,preserve_position);
    if(result.status!=RESOURCE_LOAD_OK){resource_blob_release(&blob);return result;}
    *output=blob;return result;
}
void resource_blob_release(ResourceBlob *blob) {
    if(!blob)return;
    if(blob->data)blob->allocator.release(blob->allocator.context,blob->data);
    memset(blob,0,sizeof(*blob));
}
static int equal_name(const uint8_t *folded,const char *name) {
    while(*folded && *name) {
        uint8_t byte=(uint8_t)*name++;
        if(byte>='A' && byte<='Z')byte+='a'-'A';
        if(*folded++!=byte)return 0;
    }
    return !*folded && !*name;
}
int resource_blob_open_named(const char *archive_path,const char *resource_name,
    ResourceBlob *output,uint32_t *records_scanned) {
    ResourceFile *file;ResourceArchive *archive;uint32_t count,index;int result=-1;
    if(records_scanned)*records_scanned=0;
    if(!archive_path || !resource_name || !output || output->data)return -1;
    file=resource_file_open(archive_path);if(!file)return -1;
    archive=resource_file_archive(file);
    if(resource_index_read_count(archive,&count)!=RESOURCE_INDEX_OK)goto done;
    result=0;
    for(index=0;index<count;index++) {
        ResourceIndexRecord record={0};int match;
        if(resource_index_read_record(archive,&record)!=RESOURCE_INDEX_OK){result=-1;break;}
        if(records_scanned)++*records_scanned;
        resource_index_record_fold_c_locale(&record);
        match=resource_index_record_usable(&record) && equal_name(record.name,resource_name);
        if(match)result=resource_blob_load(archive,record.info,0,output).status==RESOURCE_LOAD_OK?1:-1;
        resource_index_record_release(&record);
        if(match)break;
    }
done:
    resource_file_close(file);return result;
}
