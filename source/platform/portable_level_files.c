#include "portable_sdl.h"
#include "host_file.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/stat.h>
#include <errno.h>
#endif
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
/* SDL owns the directory snapshot; game filtering and strcmp ordering remain.
 * Windows hidden/system flags have no SDL equivalent, so retain that one
 * attribute check on Windows instead of admitting previously excluded saves. */
int game_menu_level_files(GameApplication *a,GameLevelNames *output,const char *pattern,const char *suffix,const char *const *filters,unsigned filter_count){
    char search[4128],path[4128],directory[4128],swap[128];char **files;
    const char *slash,*mask;SDL_PathInfo info;size_t prefix;int count,index;unsigned i,j;int ok=1;
    if(!output || !pattern || !suffix || (filter_count && !filters))return 0;
    if(!game_application_file_path(a,pattern,0,search,sizeof(search)))return 0;
    for(i=0;search[i];++i)if(search[i]=='\\')search[i]='/';
    slash=strrchr(search,'/');prefix=slash?(size_t)(slash-search)+1:0;mask=search+prefix;
    if(prefix){memcpy(directory,search,prefix);directory[prefix]=0;}else strcpy(directory,".");
    if(!host_file_path(directory,directory,sizeof(directory)))return 0;
    memset(output,0,sizeof(*output));
#ifdef _WIN32
    {DWORD attributes=GetFileAttributesA(directory);
     if(attributes==INVALID_FILE_ATTRIBUTES){DWORD error=GetLastError();return error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND;}}
#else
    {struct stat status;if(stat(directory,&status))return errno==ENOENT || errno==ENOTDIR;}
#endif
    if(!SDL_GetPathInfo(directory,&info))return 0;
    if(info.type==SDL_PATHTYPE_NONE)return 1;
    if(info.type!=SDL_PATHTYPE_DIRECTORY)return 0;
    files=SDL_GlobDirectory(directory,mask,SDL_GLOB_CASEINSENSITIVE,&count);if(!files)return 0;
    for(index=0;index<count;++index){
        const char *name=files[index],*extension=strchr(name,'.');size_t length=strlen(name);
        if(!extension || SDL_strcasecmp(extension,suffix) || length>=128)continue;
        if(snprintf(path,sizeof(path),"%s/%s",directory,name)>=(int)sizeof(path)){ok=0;break;}
        if(!SDL_GetPathInfo(path,&info)){ok=0;break;}if(info.type!=SDL_PATHTYPE_FILE)continue;
#ifdef _WIN32
        {DWORD attributes=GetFileAttributesA(path);if(attributes==INVALID_FILE_ATTRIBUTES){ok=0;break;}
         if(attributes&(FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN))continue;}
#endif
        if(!matches(path,filters,filter_count))continue;
        if(output->count==100){ok=0;break;}
        memcpy(output->names[output->count],name,length+1);
        output->names[output->count++][extension-name]=0;
    }
    SDL_free(files);if(!ok)return 0;
    for(i=0;i<output->count;++i)for(j=i+1;j<output->count;++j)
        if(strcmp(output->names[i],output->names[j])>0){memcpy(swap,output->names[i],128);memcpy(output->names[i],output->names[j],128);memcpy(output->names[j],swap,128);}
    return 1;
}
