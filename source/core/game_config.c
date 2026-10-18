#include "game_config.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct ConfigEntry {char *section,*key,*value;} ConfigEntry;
struct GameConfig {ConfigEntry *entries;size_t count,capacity;int dirty;};
static char *copy(const char *s){size_t n=strlen(s)+1;char *p=malloc(n);if(p)memcpy(p,s,n);return p;}
static int name_valid(const char *s){size_t i;if(!s || !*s || strlen(s)>63)return 0;for(i=0;s[i];i++)if(!isalnum((unsigned char)s[i]) && s[i]!='_' && s[i]!='.' && s[i]!='-')return 0;return 1;}
GameConfig *game_config_create(void){return calloc(1,sizeof(GameConfig));}
void game_config_free(GameConfig *c){size_t i;if(!c)return;for(i=0;i<c->count;i++){free(c->entries[i].section);free(c->entries[i].key);free(c->entries[i].value);}free(c->entries);free(c);}
const char *game_config_get(const GameConfig *c,const char *section,const char *key){size_t i;if(!c)return NULL;for(i=0;i<c->count;i++)if(!strcmp(c->entries[i].section,section) && !strcmp(c->entries[i].key,key))return c->entries[i].value;return NULL;}
int game_config_set(GameConfig *c,const char *section,const char *key,const char *value){
    size_t i;char *text;if(!c || !name_valid(section) || !name_valid(key) || !value || strlen(value)>4096)return 0;
    for(i=0;i<c->count;i++)if(!strcmp(c->entries[i].section,section) && !strcmp(c->entries[i].key,key)){
        if(!strcmp(c->entries[i].value,value))return 1;text=copy(value);if(!text)return 0;free(c->entries[i].value);c->entries[i].value=text;c->dirty=1;return 1;
    }
    if(c->count>=512)return 0;
    if(c->count==c->capacity){size_t capacity=c->capacity?c->capacity*2:32;ConfigEntry *p=realloc(c->entries,capacity*sizeof(*p));if(!p)return 0;c->entries=p;c->capacity=capacity;}
    {ConfigEntry entry={copy(section),copy(key),copy(value)};if(!entry.section || !entry.key || !entry.value){free(entry.section);free(entry.key);free(entry.value);return 0;}c->entries[c->count++]=entry;}
    c->dirty=1;return 1;
}
uint32_t game_config_number(const GameConfig *c,const char *section,const char *key,uint32_t fallback){
    const char *s=game_config_get(c,section,key);char *end;unsigned long value;if(!s || !*s || *s=='-')return fallback;
    errno=0;value=strtoul(s,&end,0);return errno || *end || value>UINT32_MAX?fallback:(uint32_t)value;
}
int game_config_set_number(GameConfig *c,const char *section,const char *key,uint32_t value){char text[16];snprintf(text,sizeof(text),"%lu",(unsigned long)value);return game_config_set(c,section,key,text);}
static char *trim(char *s){char *end;while(isspace((unsigned char)*s))s++;end=s+strlen(s);while(end>s && isspace((unsigned char)end[-1]))*--end=0;return s;}
static int hex(unsigned char c){if(c>='0' && c<='9')return c-'0';if(c>='a' && c<='f')return c-'a'+10;if(c>='A' && c<='F')return c-'A'+10;return -1;}
static int decode(char *s){char *out=s;size_t n=0;while(*s){unsigned char c=(unsigned char)*s++;if(c=='\\'){
    if(*s=='\\'){c='\\';s++;}else if(*s=='n'){c='\n';s++;}else if(*s=='r'){c='\r';s++;}else if(*s=='t'){c='\t';s++;}
    else if(*s=='x' && s[1] && s[2] && hex((unsigned char)s[1])>=0 && hex((unsigned char)s[2])>=0){c=(unsigned char)(hex((unsigned char)s[1])*16+hex((unsigned char)s[2]));s+=3;if(!c)return 0;}
    }if(++n>4096)return 0;*out++=(char)c;}*out=0;return 1;}
int game_config_read(GameConfig *c,FILE *file){
    char line[16448],section[64]="configuration";size_t total=0;unsigned lines=0;
    if(!c || !file)return 0;
    while(fgets(line,sizeof(line),file)){char *s,*equal,*end;size_t length=strlen(line);
        if(++lines>2048 || (total+=length)>1048576 || (length==sizeof(line)-1 && line[length-1]!='\n'))return 0;
        s=trim(line);if(!*s || *s==';' || *s=='#')continue;
        if(*s=='['){end=strchr(s,']');if(!end || *trim(end+1))return 0;*end=0;if(!name_valid(s+1))return 0;strcpy(section,s+1);continue;}
        equal=strchr(s,'=');if(!equal)return 0;*equal=0;s=trim(s);equal=trim(equal+1);
        if(!decode(equal) || !game_config_set(c,section,s,equal))return 0;
    }
    if(ferror(file))return 0;c->dirty=0;return 1;
}
static int compare(const void *a,const void *b){const ConfigEntry *x=*(const ConfigEntry *const *)a,*y=*(const ConfigEntry *const *)b;int n=strcmp(x->section,y->section);return n?n:strcmp(x->key,y->key);}
int game_config_write(const GameConfig *c,FILE *file){
    const ConfigEntry *sorted[512];const char *section="";size_t i,j;
    if(!c || !file)return 0;for(i=0;i<c->count;i++)sorted[i]=&c->entries[i];qsort(sorted,c->count,sizeof(*sorted),compare);
    if(fputs("; Uprising configuration. Written atomically; game assets are read-only.\n",file)<0)return 0;
    for(i=0;i<c->count;i++){const ConfigEntry *entry=sorted[i];if(strcmp(section,entry->section)){if(fprintf(file,"\n[%s]\n",entry->section)<0)return 0;section=entry->section;}
        if(fprintf(file,"%s=",entry->key)<0)return 0;
        for(j=0;entry->value[j];j++){unsigned char v=(unsigned char)entry->value[j];if(v=='\\'){if(fputs("\\\\",file)<0)return 0;}else if(v<32 || v>=127 || (v==' ' && (!j || !entry->value[j+1]))){if(fprintf(file,"\\x%02x",v)<0)return 0;}else if(fputc(v,file)==EOF)return 0;}
        if(fputc('\n',file)==EOF)return 0;
    }
    return fflush(file)==0;
}
int game_config_dirty(const GameConfig *c){return c && c->dirty;}
void game_config_saved(GameConfig *c){if(c)c->dirty=0;}
