#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "menu_level_catalog.h"
#include "application_archives.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int matches(const char *path,const char *const *filters,unsigned count) {
    FILE *file;char *bytes;long size;size_t read;unsigned i;int result=0;
    if(!count)return 1;
    file=fopen(path,"rb");if(!file)return 0;
    if(fseek(file,0,SEEK_END) || (size=ftell(file))<=0 || fseek(file,0,SEEK_SET)) {fclose(file);return 0;}
    bytes=malloc((size_t)size+1);if(!bytes){fclose(file);return 0;}
    read=fread(bytes,1,(size_t)size,file);fclose(file);bytes[read]=0;
    for(i=0;i<count;++i)if(strstr(bytes,filters[i])){result=1;break;}
    free(bytes);return result;
}
/*004701f0/004704c0: preserve filesystem enumeration, excluded attributes,
 * actual content filtering, extension removal and strcmp ascending order.
 * Native directory handles remain full-width and close on every exit. */
int game_menu_level_files(GameApplication *a,GameLevelNames *output,const char *pattern,const char *suffix,const char *const *filters,unsigned filter_count) {
    WIN32_FIND_DATAA entry;HANDLE directory;unsigned i,j;char path[4128],search[4128],swap[128];size_t prefix;const char *slash;
    if(!output || !pattern || !suffix || (filter_count && !filters))return 0;
    if(!game_application_file_path(a,pattern,0,search,sizeof(search)))return 0;
    slash=strrchr(search,'\\');{const char *forward=strrchr(search,'/');if(forward && (!slash || forward>slash))slash=forward;}
    prefix=slash?(size_t)(slash-search)+1:0;
    memset(output,0,sizeof(*output));directory=FindFirstFileA(search,&entry);
    if(directory==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND;
    do {
        size_t length=strlen(entry.cFileName);char *extension=strstr(entry.cFileName,".");
        if(entry.dwFileAttributes&(FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_DIRECTORY))continue;
        if(!extension || _stricmp(extension,suffix) || length>=128 || prefix+length>=sizeof(path))continue;
        memcpy(path,search,prefix);memcpy(path+prefix,entry.cFileName,length+1);
        if(!matches(path,filters,filter_count))continue;
        if(output->count==100){FindClose(directory);return 0;}
        memcpy(output->names[output->count],entry.cFileName,length+1);
        output->names[output->count++][extension-entry.cFileName]=0;
    }while(FindNextFileA(directory,&entry));
    {DWORD error=GetLastError();FindClose(directory);if(error!=ERROR_NO_MORE_FILES)return 0;}
    for(i=0;i<output->count;++i)for(j=i+1;j<output->count;++j)
        if(strcmp(output->names[i],output->names[j])>0){memcpy(swap,output->names[i],128);memcpy(output->names[i],output->names[j],128);memcpy(output->names[j],swap,128);}
    return 1;
}
