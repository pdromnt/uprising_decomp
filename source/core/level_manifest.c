#include "level_manifest.h"
#include "resource_file.h"
#include <ctype.h>
#include <stdio.h>

int level_manifest_read(const char *path,LevelManifest *output){
    ResourceFile *file;LevelManifest result={0};char line[512];
    unsigned directive=0;
    if(!path || !output)return 0;
    file=resource_file_open(path);
    if(!file)return 0;
    while(directive<3u && resource_file_line(file,line,sizeof(line))){
        const unsigned char *text=(const unsigned char *)line;
        while(isspace(*text))++text;
        if(!*text || *text=='#')continue;
        if((directive==0u && sscanf((const char *)text,"SLICK %f",
                &result.slick)!=1) ||
           (directive==1u && sscanf((const char *)text,"terrain %d",
                &result.terrain_offset)!=1) ||
           (directive==2u && sscanf((const char *)text,"slots %d",
                &result.slots_offset)!=1))break;
        ++directive;
    }
    resource_file_close(file);
    if(directive!=3u)return 0;
    *output=result;return 1;
}
