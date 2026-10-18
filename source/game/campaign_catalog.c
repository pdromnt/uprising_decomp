#include "application_entry.h"
#include "model_assets.h"
#include "portable_crt.h"
#include "scene_projection_storage.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*5580c0 and its section readers. The published tables own bounded legacy
 * records; file cursors and texture pixels retain full-width native owners. */
enum { CATALOG_LIMIT=50 };
static const char catalog_definition[] =
#include "campaign_catalog_data.inc"
;
static void catalog_definition_release(void *context,void *bytes){(void)context;free(bytes);}
typedef struct CampaignText { ResourceBlob blob;size_t position; } CampaignText;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static GameAddress allocate(GameApplication *a,size_t size,const char *name) {
    GameAddress p=scene_projection_storage_allocate(size,name);
    if(p && !game_memory_fill_u8(&a->memory,p,size,0,0)){scene_projection_storage_free(p);return 0;}return p;
}
static GameAddress copy_text(GameApplication *a,const char *text) {
    size_t size=strlen(text)+1;GameAddress p=allocate(a,size,"campaign catalog string");
    if(p && !game_memory_write(&a->memory,p,text,size)){scene_projection_storage_free(p);return 0;}return p;
}
static int character(CampaignText *s) {
    int c;if(s->position>=s->blob.size)return EOF;c=s->blob.data[s->position++];
    if(c=='\r' && s->position<s->blob.size && s->blob.data[s->position]=='\n'){++s->position;return '\n';}return c;
}
static int line(CampaignText *s,char *out,size_t capacity) {
    size_t n=0;int c=EOF;while(n+1<capacity && (c=character(s))!=EOF){out[n++]=(char)c;if(c=='\n')break;}
    out[n]=0;return n!=0;
}
/*The original section readers remove the last character even at a short EOF. */
static void strip_last(char *s){size_t n=strlen(s);if(n)s[n-1]=0;}
static void shuffle(GameApplication *a,GameAddress values,uint32_t count) {
    uint32_t i,first,second,temp,divisor;if(count<=1)return;divisor=count-1;
    for(i=0;i<count*2;++i){first=portable_crt_rand_step(&a->memory)%divisor;second=portable_crt_rand_step(&a->memory)%divisor;
        if(first==second)second=(first+1)%divisor;temp=get(a,values+first*4);put(a,values+first*4,get(a,values+second*4));put(a,values+second*4,temp);}
}
static int table(GameApplication *a,GameAddress slot) {
    GameAddress p;if(get(a,slot))return 0;p=allocate(a,CATALOG_LIMIT*4,"campaign catalog table");put(a,slot,p);return p!=0;
}
static int names(GameApplication *a,CampaignText *s,GameAddress slot,const char *end,uint32_t minimum,const char *format) {
    char text[128]={0};uint32_t count=0,i;GameAddress p;
    if(!table(a,slot))return 0;
    put(a,slot+8,0);
    if(slot!=0x5a04fc && !table(a,slot+4))return 0;
    while(line(s,text,sizeof(text))){if(strstr(text,end))break;strip_last(text);if(count==CATALOG_LIMIT)return 0;
        p=copy_text(a,text);if(!p)return 0;put(a,get(a,slot)+count*4,p);put(a,slot+8,++count);}
    if(!strstr(text,end))return 0;
    while(count<minimum){snprintf(text,sizeof(text),format,count);p=copy_text(a,text);if(!p)return 0;
        put(a,get(a,slot)+count*4,p);put(a,slot+8,++count);}
    if(slot!=0x5a04fc){for(i=0;i<count;++i)put(a,get(a,slot+4)+i*4,i);shuffle(a,get(a,slot+4),count);}return count!=0;
}
/*5507e0's MAP_TEXT branch: ignore blank/comment lines, skip the LFL header,
 * preserve the remainder after its fixed ten-byte keyword prefix. */
int game_campaign_level_value(GameApplication *a,const char *path,const char *key,char *result,size_t capacity) {
    CampaignText s={0};char text[512];int header=0,found=0;size_t length=key?strlen(key):0;
    if(!length || !result || !capacity)return 0;
    if(!game_application_load_blob(a,path,&s.blob))return 0;
    while(line(&s,text,sizeof(text))){size_t i=0;while(text[i] && isspace((unsigned char)text[i]))++i;
        if(!text[i] || text[i]=='#')continue;if(!header){header=1;continue;}
        if(!strncmp(text,"END_LFL_FILE",12))break;
        if(!strncmp(text,key,length)){
            size_t n=strlen(text),start=length+2,end;
            if(n<=length || text[length]!=':')continue;
            if(!strcmp(key,"MAP_TEXT")){if(n>=start && n-start<capacity){memcpy(result,text+start,n-start+1);strip_last(result);found=1;}}
            else {start=length+1;while(text[start] && isspace((unsigned char)text[start]))++start;end=start;
                while(text[end] && !isspace((unsigned char)text[end]))++end;
                if(end>start && end-start<capacity){memcpy(result,text+start,end-start);result[end-start]=0;found=1;}}
            break;}}
    resource_blob_release(&s.blob);return found;
}
static int integer(CampaignText *s,int32_t *value) {
    char number[32];size_t n=0;int c;char *end;long v;
    do{c=character(s);}while(c!=EOF && isspace((unsigned char)c));
    if(c=='+' || c=='-'){number[n++]=(char)c;c=character(s);}
    while(c>='0' && c<='9'){if(n+1>=sizeof(number))return 0;number[n++]=(char)c;c=character(s);}
    number[n]=0;v=strtol(number,&end,10);if(end==number || *end || v<INT32_MIN || v>INT32_MAX)return 0;
    while(c!=EOF && isspace((unsigned char)c))c=character(s);if(c!=EOF)--s->position;*value=(int32_t)v;return 1;
}
static int mission_metadata(GameApplication *a,GameAddress p,const char *filename) {
    CampaignText s={0};char path[300],text_name[128],text[128];GameAddress bonuses=allocate(a,40,"campaign spy table"),bonus;
    int32_t ranking=3;uint32_t count=0;int ok=0;
    if(!bonuses)return 0;put(a,p+0xa5c,bonuses);snprintf(path,sizeof(path),"grids\\%s",filename);
    if(game_campaign_level_value(a,path,"MAP_TEXT",text_name,sizeof(text_name))){snprintf(path,sizeof(path),"grids\\%s",text_name);ok=game_application_load_blob(a,path,&s.blob);}
    if(!ok){bonus=allocate(a,0x410,"campaign default spy");if(!bonus)return 0;put(a,bonuses,bonus);put(a,p+0xa60,1);
        game_memory_write(&a->memory,bonus,"Default Spy Info",17);put(a,bonus+0x408,2);put(a,bonus+0x40c,30000+portable_crt_rand_step(&a->memory)%15000);
        game_memory_write(&a->memory,p,"Default Planet Description",27);put(a,p+0xa50,3);return !a->memory.fault;}
    ok=0;
    while(line(&s,text,sizeof(text))){
        if(strstr(text,"CAMPAIGN_RANKING")){if(!integer(&s,&ranking) || ranking<0 || ranking>=7)goto done;put(a,p+0xa50,(uint32_t)ranking);}
        if(strstr(text,"SPYINFO_BEGIN")){char description[1024]={0};size_t used=0;int32_t kind;
            if(count==10 || !integer(&s,&kind))goto done;bonus=allocate(a,0x410,"campaign spy information");if(!bonus)goto done;
            put(a,bonuses+count*4,bonus);put(a,p+0xa60,++count);put(a,bonus+0x408,(uint32_t)kind);
            while(line(&s,text,sizeof(text))){size_t n;if(strstr(text,"SPYINFO_END"))break;n=strlen(text);if(used<0x37b){if(n>sizeof(description)-1-used)n=sizeof(description)-1-used;memcpy(description+used,text,n);used+=n;}}
            game_memory_write(&a->memory,bonus,description,used+1);put(a,bonus+0x40c,(uint32_t)kind*10000+portable_crt_rand_step(&a->memory)%10000);}
        if(strstr(text,"DESCRIPTION_BEGIN")){char description[0x9d0]={0};size_t used=0;int c;char *end;
            while(used<sizeof(description)-1 && (c=character(&s))!=EOF){description[used++]=(char)c;if((end=strstr(description,"DESCRIPTION_END"))!=NULL){*end=0;break;}}
            game_memory_write(&a->memory,p,description,strlen(description)+1);}
    }
    ok=1;
done:resource_blob_release(&s.blob);return ok && !a->memory.fault;
}
static int add_mission(GameApplication *a,const char *name) {
    uint32_t count=get(a,0x5a0534);GameAddress p;
    if(count==CATALOG_LIMIT || strlen(name)>=128)return 0;p=allocate(a,0xa64,"campaign mission template");if(!p)return 0;
    put(a,get(a,0x5a0530)+count*4,p);put(a,0x5a0534,count+1);game_memory_write(&a->memory,p+0x9d0,name,strlen(name)+1);return mission_metadata(a,p,name);
}
static int missions(GameApplication *a,CampaignText *s,uint32_t minimum) {
    char text[128]={0},first[128]={0};uint32_t i,category,count,actual;GameAddress p,indices;
    if(!table(a,0x5a0530))return 0;
    put(a,0x5a0534,0);
    while(line(s,text,sizeof(text))){if(strstr(text,"LEVELINFOS_END"))break;strip_last(text);if(!get(a,0x5a0534))memcpy(first,text,strlen(text)+1);if(!add_mission(a,text))return 0;}
    if(!strstr(text,"LEVELINFOS_END") || !get(a,0x5a0534))return 0;
    while(get(a,0x5a0534)<minimum)if(!add_mission(a,first))return 0;
    game_memory_fill_u8(&a->memory,0x63a080,28,0,0);
    for(i=0;i<get(a,0x5a0534);++i){p=get(a,get(a,0x5a0530)+i*4);category=get(a,p+0xa50);if(category>=7)return 0;
        count=get(a,0x63a080+category*4);if(count==50)return 0;put(a,0x639b08+(category*50+count)*4,i);put(a,0x63a080+category*4,count+1);}
    for(category=0;category<7;++category){count=get(a,0x63a080+category*4);actual=get(a,0x5a04c0+category*4);indices=0x639b08+category*200;
        if(actual>50)return 0;for(i=count;i<actual;++i)put(a,indices+i*4,count?get(a,indices):0);if(count<actual){count=actual;put(a,0x63a080+category*4,count);}shuffle(a,indices,count);}
    return !a->memory.fault;
}
static GameAddress material(GameApplication *a,const char *path) {
    MaterialValue value;GameAddress p;if(!game_model_software_material_load(a,path,&value))return 0;
    p=allocate(a,8,"campaign material");if(!p){game_model_texture_release(a,value.backend_handle);return 0;}
    game_memory_write_u16(&a->memory,p,value.width);game_memory_write_u16(&a->memory,p+2,value.height);put(a,p+4,value.backend_handle);return p;
}
static int add_texture(GameApplication *a,const char *path) {
    uint32_t count=get(a,0x5a052c);GameAddress p;if(count==50)return 0;
    /*Two independent pixel owners: animation writes the first, the second stays pristine. */
    put(a,0x5a052c,count+1);p=material(a,path);put(a,get(a,0x5a0520)+count*4,p);if(!p)return 0;
    p=material(a,path);put(a,get(a,0x5a0524)+count*4,p);return p!=0;
}
static int textures(GameApplication *a,CampaignText *s,uint32_t minimum) {
    char text[128]={0},path[300];uint32_t count,i;
    if(!table(a,0x5a0520))return 0;
    /*557922 initializes the new catalog, independently of image/data counts. */
    put(a,0x5a052c,0);
    if(!table(a,0x5a0524) || !table(a,0x5a0528))return 0;
    while(line(s,text,sizeof(text))){strip_last(text);if(strstr(text,"TEXTURES_END"))break;snprintf(path,sizeof(path),"bitmaps\\planettex\\%s.bmp",text);if(!add_texture(a,path))return 0;}
    if(!strstr(text,"TEXTURES_END"))return 0;
    while(get(a,0x5a052c)<minimum)if(!add_texture(a,"bitmaps\\planettex\\alien0.bmp"))return 0;
    count=get(a,0x5a052c);for(i=0;i<count;++i)put(a,get(a,0x5a0528)+i*4,i);shuffle(a,get(a,0x5a0528),count);return !a->memory.fault;
}
int game_campaign_catalog_release(GameApplication *a) {
    unsigned group,i,j;GameAddress textures[2],missions;uint32_t texture_count,mission_count;
    static const GameAddress slots[]={0x5a04fc,0x5a0508,0x5a0514};
    /*An unvisited section has no owned table. Its old data count must never
     *be interpreted as allocations during startup/failure cleanup. */
    for(group=0;group<3;++group){GameAddress slot=slots[group],values=get(a,slot);uint32_t count=values?get(a,slot+8):0;if(count>50)return 0;
        for(i=0;i<count;++i)scene_projection_storage_free(get(a,values+i*4));scene_projection_storage_free(values);scene_projection_storage_free(get(a,slot+4));put(a,slot,0);put(a,slot+4,0);put(a,slot+8,0);}
    textures[0]=get(a,0x5a0520);textures[1]=get(a,0x5a0524);missions=get(a,0x5a0530);
    texture_count=(textures[0] || textures[1])?get(a,0x5a052c):0;mission_count=missions?get(a,0x5a0534):0;
    if(texture_count>50 || mission_count>50)return 0;
    for(i=0;i<texture_count;++i)for(j=0;j<2;++j){GameAddress p=textures[j]?get(a,textures[j]+i*4):0;if(p){game_model_texture_release(a,get(a,p+4));scene_projection_storage_free(p);}}
    for(j=0;j<3;++j){scene_projection_storage_free(get(a,0x5a0520+j*4));put(a,0x5a0520+j*4,0);}put(a,0x5a052c,0);
    for(i=0;i<mission_count;++i){GameAddress p=get(a,missions+i*4),bonuses;uint32_t count;if(!p)continue;bonuses=get(a,p+0xa5c);count=get(a,p+0xa60);if(count>10)return 0;
        for(j=0;j<count;++j)scene_projection_storage_free(get(a,bonuses+j*4));scene_projection_storage_free(bonuses);scene_projection_storage_free(p);}
    scene_projection_storage_free(get(a,0x5a0530));put(a,0x5a0530,0);put(a,0x5a0534,0);return !a->memory.fault;
}
int game_campaign_catalog_prepare(GameApplication *a,uint32_t requested,uint32_t systems) {
    CampaignText s={0};char text[128];unsigned seen=0;uint32_t i;int ok=0;
    if(!requested || requested>50 || !systems || systems>7 || get(a,0x5a0530))return 0;
    put(a,0x63a14c,requested);put(a,0x63a148,systems);if(systems>3)shuffle(a,0x5a04e4,systems-2);
    s.blob.data=malloc(sizeof(catalog_definition));if(!s.blob.data)return 0;
    memcpy(s.blob.data,catalog_definition,sizeof(catalog_definition));
    s.blob.size=sizeof(catalog_definition)-1;s.blob.allocator.release=catalog_definition_release;
    while(line(&s,text,sizeof(text))){
        if(strstr(text,"LEVELINFOS_BEGIN")){if(seen&1 || !missions(a,&s,requested))goto done;seen|=1;}
        else if(strstr(text,"GALAXYNAMES_BEGIN")){if(seen&2 || !names(a,&s,0x5a04fc,"GALAXYNAMES_END",0,"%u"))goto done;seen|=2;}
        else if(strstr(text,"SYSTEMNAMES_BEGIN")){if(seen&4 || !names(a,&s,0x5a0508,"SYSTEMNAMES_END",systems,"System %u"))goto done;seen|=4;}
        else if(strstr(text,"PLANETNAMES_BEGIN")){if(seen&8 || !names(a,&s,0x5a0514,"PLANETNAMES_END",requested,"PLANET %u"))goto done;seen|=8;}
        else if(strstr(text,"TEXTURES_BEGIN")){if(seen&16 || !textures(a,&s,requested))goto done;seen|=16;}
    }
    if(seen!=31)goto done;for(i=0;i<10;++i)put(a,0x63a120+i*4,i+1);shuffle(a,0x63a120,10);ok=!a->memory.fault;
done:resource_blob_release(&s.blob);if(!ok)(void)game_campaign_catalog_release(a);return ok;
}
