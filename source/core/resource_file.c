#include "resource_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
struct ResourceFile { FILE *stream; ResourceArchive archive; };
static size_t host_read(void *context,void *output,size_t bytes) { return fread(output,1,bytes,context); }
static int host_seek(void *context,uint64_t offset) { return offset<=LONG_MAX && fseek(context,(long)offset,SEEK_SET)==0; }
static int host_tell(void *context,uint64_t *offset) { long value=ftell(context); if(value<0)return 0; *offset=(uint64_t)value; return 1; }
static void host_close(void *context) { fclose(context); }
ResourceFile *resource_file_open_mode(const char *path,const char *mode) {
    ResourceFile *file;
    ResourceStream stream;
    FILE *opened;
    if(!path || !mode)return NULL;
    opened=fopen(path,mode); if(!opened)return NULL;
    file=malloc(sizeof(*file)); if(!file){fclose(opened);return NULL;}
    file->stream=opened;
    stream.context=opened;stream.read=host_read;stream.seek=host_seek;stream.tell=host_tell;stream.close=host_close;
    if(!resource_archive_initialize(&file->archive,stream,1,NULL)){fclose(opened);free(file);return NULL;}
    return file;
}
ResourceFile *resource_file_open(const char *path) { return resource_file_open_mode(path,"rb"); }
void resource_file_close(ResourceFile *file) { if(file){resource_archive_destroy(&file->archive);free(file);} }
size_t resource_file_read(ResourceFile *file,void *output,size_t bytes) { return fread(output,1,bytes,file->stream); }
size_t resource_file_write(ResourceFile *file,const void *input,size_t bytes) { return fwrite(input,1,bytes,file->stream); }
int resource_file_flush(ResourceFile *file) { return fflush(file->stream)==0; }
int resource_file_seek(ResourceFile *file,int32_t offset,int origin) {
    if(origin!=SEEK_SET && origin!=SEEK_CUR && origin!=SEEK_END)return 0;
    return fseek(file->stream,(long)offset,origin)==0;
}
int resource_file_tell(ResourceFile *file,uint32_t *position) {
    long value=ftell(file->stream); if(value<0 || (uint64_t)value>UINT32_MAX)return 0;
    *position=(uint32_t)value;return 1;
}
size_t resource_file_line(ResourceFile *file,char *output,size_t capacity) {
    size_t used=0;
    if(!capacity)return 0;
    while(used<capacity-1) {
        int value=fgetc(file->stream);
        if(value==EOF){if(!used)return 0;break;}
        output[used++]=(char)value;
        if(value=='\n')break;
    }
    output[used]=0;return used+1;
}
ResourceArchive *resource_file_archive(ResourceFile *file) { return &file->archive; }
