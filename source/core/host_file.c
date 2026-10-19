#include "host_file.h"
#include <string.h>
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
static int same_name(const char *a,const char *b){
    while(*a && *b){unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+='a'-'A';
        if(y>='A' && y<='Z')y+='a'-'A';
        if(x!=y)return 0;
    }return !*a && !*b;
}
#endif
int host_file_path(const char *path,char *output,size_t capacity){
    size_t i,length;
    if(!path || !output || !(length=strlen(path)) || length>=capacity)return 0;
    for(i=0;i<=length;++i)output[i]=path[i]=='\\'?'/':path[i];
#ifndef _WIN32
    {struct stat info;size_t start=0;
     if(!stat(output,&info))return 1;
     while(start<length){
        size_t end=start;char saved;DIR *directory;struct dirent *entry;
        while(start<length && output[start]=='/')++start;
        if(start==length)break;
        end=start;while(end<length && output[end]!='/')++end;
        saved=output[end];output[end]=0;
        if(stat(output,&info)){
            char prefix[4096],match[4096];int found=0;
            if(start>=sizeof(prefix)){output[end]=saved;return 0;}
            if(start){memcpy(prefix,output,start);prefix[start]=0;}else strcpy(prefix,".");
            directory=opendir(prefix);
            if(directory){
                while((entry=readdir(directory))!=NULL)if(same_name(output+start,entry->d_name)){
                    size_t n=strlen(entry->d_name);
                    if(n==end-start && n<sizeof(match)){memcpy(match,entry->d_name,n+1);found=1;break;}
                }
                closedir(directory);
            }
            if(found)memcpy(output+start,match,end-start);
        }
        output[end]=saved;start=end;
     }
    }
#endif
    return 1;
}
FILE *host_file_open(const char *path,const char *mode){
    char normalized[4128];FILE *file;size_t i,n;
    if(!path || !mode || (n=strlen(path))>=sizeof(normalized))return NULL;
    for(i=0;i<=n;++i)normalized[i]=path[i]=='\\'?'/':path[i];
    file=fopen(normalized,mode);
    if(file || mode[0]!='r')return file;
    return host_file_path(path,normalized,sizeof(normalized))?fopen(normalized,mode):NULL;
}
