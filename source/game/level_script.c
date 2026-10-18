#include "level_script.h"
#include "level_routes.h"
#include "level_run.h"
#include "gameplay_root.h"
#include "scene_projection_storage.h"
#include "application_name_registry.h"
#include "sound_playback.h"
#include "sound_platform.h"
#include "gameplay_notices.h"
#include "menu_sound.h"
#include "gameplay_geometry.h"
#include "gameplay_input.h"
#include "level_factory.h"
#include "level_effect_runtime.h"
#include "level_player_resources.h"
#include "level_player_slots.h"
#include "game_intrusive_list.h"
#include "state_helper_plans.h"
#include "presentation_instance.h"
#include "presentation_commands.h"
#include "sample_pair_registry.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
extern int game_level_teleporter_enable(GameApplication *,GameAddress,uint32_t);
extern int game_level_teleporter_bind(GameApplication *,GameAddress,GameAddress);
extern int game_level_scene_killed(GameApplication *,GameAddress,GameAddress,GameAddress);

typedef struct ScriptTable {GameAddress rows;uint32_t count,capacity;unsigned bytes;} ScriptTable;
struct GameScriptState {
    ScriptTable scalar,vector;
    GameAddress root,commands,names,auxiliary;
    uint32_t command_count,name_count;
    int manager_ready;
};
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void store(uint8_t *p,uint32_t v){unsigned i;for(i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int manager_nodes_retire(GameApplication *);
static int space(unsigned char c){return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\v' || c=='\f';}
/*4a8090: comments after indentation and whitespace-only physical lines are
 *discarded. ResourceFile owns its stream; no CRT pointer enters a game word. */
static int line(GameScriptReader *r){
    for(;;){size_t i;
        if(!resource_file_line(r->file,r->line,sizeof(r->line))){r->failed=1;return 0;}
        ++r->line_number;r->cursor=0;
        for(i=0;space((unsigned char)r->line[i]);++i){}
        if(r->line[i] && r->line[i]!='#')return 1;
    }
}
/*48d460: section filtering and brace text are language syntax, not instruction
 *execution. Explicit cursor/arguments replace the original shared lexer cells. */
int game_script_next(GameScriptReader *r){
    size_t used=0;unsigned char c;int escaped=0;
    if(!r || !r->app || !r->file || r->failed)return 0;
    for(;;){
        if(!r->line[r->cursor]){
            int include;
            if(!line(r))return 0;
            if(!strncmp(r->line,"START_SECTION",13)){
                if(!strncmp(r->line,"START_SECTION_OPTIONAL",22))include=get(r->app,0x5ac688)!=0;
                else if(!strncmp(r->line,"START_SECTION_MP_ONLY",21))include=get(r->app,0x5eff04)!=0;
                else if(!strncmp(r->line,"START_SECTION_SP_ONLY",21))include=get(r->app,0x5eff04)==0;
                else {r->failed=1;return 0;}
                if(!include){unsigned depth=1;
                    while(depth){if(!line(r))return 0;
                        if(!strncmp(r->line,"START_SECTION",13)){if(depth>=65536){r->failed=1;return 0;}++depth;}
                        else if(!strncmp(r->line,"END_SECTION",11))--depth;
                    }
                }
                r->line[0]=0;continue;
            }
            if(!strncmp(r->line,"END_SECTION",11)){r->line[0]=0;continue;}
        }
        c=(unsigned char)r->line[r->cursor];
        if(space(c)){++r->cursor;continue;}
        break;
    }
    if(c=='(' || c==')' || c==','){r->token[0]=(char)c;r->token[1]=0;++r->cursor;return 1;}
    if(c=='{'){
        for(;;){
            c=(unsigned char)r->line[r->cursor];
            if(!c || c=='\n' || used+1>=sizeof(r->token)){r->failed=1;return 0;}
            if(c=='\\' && !escaped){escaped=1;++r->cursor;continue;}
            if(escaped){if(c=='n')c='\n';else if(c=='t')c='\t';escaped=0;}
            else if(c=='}'){r->token[used++]=(char)c;++r->cursor;break;}
            r->token[used++]=(char)c;++r->cursor;
        }
    }else do{
        if(used+1>=sizeof(r->token)){r->failed=1;return 0;}
        r->token[used++]=(char)c;++r->cursor;c=(unsigned char)r->line[r->cursor];
    }while(c && !space(c) && c!='(' && c!=')' && c!=',' && c!='{' && c!='}');
    r->token[used]=0;return !r->app->memory.fault;
}
static int table_begin(ScriptTable *t,unsigned bytes){
    t->capacity=16;t->bytes=bytes;t->rows=scene_projection_storage_allocate(64,"script variable table");
    return t->rows!=0;
}
static void table_end(ScriptTable *t){
    uint8_t *rows=scene_projection_storage_borrow_bytes(t->rows,(size_t)t->capacity*4);uint32_t i;
    if(rows)for(i=0;i<t->count;++i)(void)scene_projection_storage_free(word(rows+i*4));
    if(t->rows)(void)scene_projection_storage_free(t->rows);
    memset(t,0,sizeof(*t));
}
static int table_add(ScriptTable *t,const char *name,const uint32_t values[3]){
    GameAddress id;uint8_t *rows,*record;size_t length=strlen(name);unsigned i;
    if(!length || length>=32 || t->count>=65536)return 0;
    rows=scene_projection_storage_borrow_bytes(t->rows,(size_t)t->capacity*4);if(!rows)return 0;
    if(t->count==t->capacity){GameAddress next;uint8_t *grown;
        next=scene_projection_storage_allocate((size_t)t->capacity*8,"script variable table growth");if(!next)return 0;
        grown=scene_projection_storage_borrow_bytes(next,(size_t)t->capacity*8);
        if(!grown){(void)scene_projection_storage_free(next);return 0;}
        memcpy(grown,rows,(size_t)t->capacity*4);
        if(!scene_projection_storage_free(t->rows)){(void)scene_projection_storage_free(next);return 0;}
        t->rows=next;t->capacity*=2;rows=grown;
    }
    id=scene_projection_storage_allocate(t->bytes,"script variable");if(!id)return 0;
    record=scene_projection_storage_borrow_bytes(id,t->bytes);
    if(!record){(void)scene_projection_storage_free(id);
    return 0;}
    memcpy(record,name,length+1);for(i=0;i<(t->bytes-32)/4;++i)store(record+32+i*4,values[i]);
    store(rows+t->count*4,id);++t->count;return 1;
}
static void publish(GameApplication *a){
    struct GameScriptState *s=a->scripts;
    put(a,0x605870,s?s->scalar.rows:0);put(a,0x605884,s?s->scalar.count:0);put(a,0x605898,s?s->scalar.capacity:0);
    put(a,0x60586c,s?s->vector.rows:0);put(a,0x605888,s?s->vector.count:0);put(a,0x605874,s?s->vector.capacity:0);
    put(a,0x60587c,s?s->root:0);
}
GameAddress game_script_variable_find(GameApplication *a,const char *name,int vector){
    ScriptTable *t;uint8_t *rows;uint32_t i;
    if(!a || !a->scripts || !name)return 0;
    t=vector?&a->scripts->vector:&a->scripts->scalar;
    rows=scene_projection_storage_borrow_bytes(t->rows,(size_t)t->capacity*4);if(!rows)return 0;
    for(i=0;i<t->count;++i){GameAddress id=word(rows+i*4);uint8_t *record=scene_projection_storage_borrow_bytes(id,t->bytes);
        if(record && !strcmp((const char *)record,name))return id;
    }
    return 0;
}
int game_level_script_release(GameApplication *a){
    struct GameScriptState *s;
    if(!a)return 0;
    s=a->scripts;
    /*An unconverted snapshot/manager owner must not have its roots erased. */
    if(!s)return !a->memory.fault && !get(a,0x605870) && !get(a,0x60586c) &&
        !get(a,0x60587c) && !get(a,0x605894) && !get(a,0x60588c);
    if(s){
        if(s->commands && !scene_projection_storage_free(s->commands))return 0;
        s->commands=0;s->command_count=0;put(a,0x605894,0);put(a,0x60588c,0);
        while(s->root){GameAddress next=get(a,s->root+4);
            if(a->memory.fault || !game_script_statement_destroy(a,s->root))return 0;
            s->root=next;put(a,0x60587c,next);
        }
        if(!manager_nodes_retire(a))return 0;
        table_end(&s->scalar);table_end(&s->vector);free(s);a->scripts=NULL;
    }
    publish(a);return !a->memory.fault;
}
static int expect(GameScriptReader *r,const char *token){return game_script_next(r) && !strcmp(r->token,token);}
static int numeric(GameScriptReader *r,int integer,uint32_t *value){
    long double n;char *end;
    if(!game_script_next(r) || (!(r->token[0]>='0' && r->token[0]<='9') && r->token[0]!='-'))return 0;
    if(integer){long long v=strtoll(r->token,&end,10);*value=(uint32_t)(uint64_t)v;}
    else{float v;n=(long double)strtod(r->token,&end);v=(float)n;memcpy(value,&v,4);}
    /*Original atoi/atof accept numeric suffixes after a valid first byte. */
    return end!=r->token;
}
static int declarations(GameApplication *a,GameScriptReader *r){
    while(!strcmp(r->token,"VDECL")){char name[32];uint32_t values[3]={0};int vector;unsigned i;
        if(!game_script_next(r))return 0;
        if(!strcmp(r->token,"VINT"))vector=0;else if(!strcmp(r->token,"VLOC"))vector=1;else return 0;
        if(!game_script_next(r) || !r->token[0] || strlen(r->token)>=sizeof(name))return 0;
        strcpy(name,r->token);
        if(!vector){if(!numeric(r,1,&values[0]))return 0;}
        else{int direction;float factor,center;
            if(!game_script_next(r) || (strcmp(r->token,"LOC") && strcmp(r->token,"VEC")))return 0;
            /*The original LOC and VEC readers convert map units, then mirror
             *the first axis. Retain the explicit intermediate float stores. */
            direction=!strcmp(r->token,"VEC");
            if(!expect(r,"("))return 0;
            /*Text order is map-x,map-y,height; storage is x,height,z. */
            for(i=0;i<3;++i){unsigned component=i==1?2:i==2?1:0;
                if(!numeric(r,0,&values[component]) || (i<2 && !expect(r,",")))return 0;}
            if(!expect(r,")"))return 0;
            {uint32_t bits=get(a,0x573fac);memcpy(&factor,&bits,4);bits=get(a,0x573fb0);memcpy(&center,&bits,4);}
            for(i=0;i<3;++i){float value;memcpy(&value,&values[i],4);value=(float)((long double)value*factor);memcpy(&values[i],&value,4);}
            if(direction)values[0]^=0x80000000u;
            else{float value;memcpy(&value,&values[0],4);value=(float)((long double)center-value);memcpy(&values[0],&value,4);}
        }
        if(!table_add(vector?&a->scripts->vector:&a->scripts->scalar,name,values))return 0;
        publish(a);if(!game_script_next(r))return 0;
    }
    return 1;
}
/*484930: complete file/lexer/declaration ownership and statement-chain linking.
 *The remaining statement builders are strong native interfaces, never CPU calls. */
int game_level_script_load_path(GameApplication *a,const char *path){
    GameScriptReader reader={0};GameAddress tail=0;uint32_t count=0;int ok=0;
    if(!a || !path || a->memory.fault)return 0;
    if(a->scripts){
        struct GameScriptState *s=a->scripts;
        if(!s->manager_ready || s->root || s->commands || s->scalar.count || s->vector.count ||
           get(a,0x605870)!=s->scalar.rows || get(a,0x60586c)!=s->vector.rows)return 0;
    }else if(get(a,0x605870) || get(a,0x60586c) || get(a,0x60587c) || get(a,0x605894))return 0;
    reader.app=a;reader.file=resource_file_open(path);if(!reader.file)return 0;
    if(!a->scripts){
        a->scripts=calloc(1,sizeof(*a->scripts));if(!a->scripts)goto done;
        if(!table_begin(&a->scripts->scalar,36) || !table_begin(&a->scripts->vector,44))goto done;
    }
    publish(a);
    if(!expect(&reader,"START_SCRIPT_FILE") || !game_script_next(&reader) || !declarations(a,&reader))goto done;
    while(strcmp(reader.token,"END_SCRIPT_FILE")){GameAddress node=0;
        if(++count>65536 || !game_script_statement_create(a,&reader,&node) || !node)goto done;
        if(tail)put(a,tail+4,node);else a->scripts->root=node;
        tail=node;put(a,node+4,0);publish(a);if(a->memory.fault)goto done;
    }
    ok=!reader.failed && !a->memory.fault;
done:
    resource_file_close(reader.file);
    if(!ok) {
        fprintf(stderr,"native script load failed: %s line=%u token=%s\n",path,reader.line_number,reader.token);
        (void)game_level_script_release(a);
    }
    return ok;
}
int game_level_player_script_load(GameApplication *a){
    char name[256],path[512];unsigned i;int n;
    if(!get(a,0x5ac688))return !a->memory.fault;
    for(i=0;i<sizeof(name);++i){name[i]=(char)game_memory_read_u8(&a->memory,0x5acc40+i);if(!name[i])break;}
    if(a->memory.fault || i==sizeof(name))return 0;
    n=snprintf(path,sizeof(path),"grids/%s",name);if(n<0 || (size_t)n>=sizeof(path))return 0;
    return game_level_script_load_path(a,path);
}

/*The identifiers below select native language nodes. They are never read as
 *guest function tables. Each graph has one owner in projection storage. */
#define SCRIPT_AND 0x574b20u
#define SCRIPT_OR 0x574b30u
#define SCRIPT_NOT 0x574b38u
#define SCRIPT_IF 0x574bb0u
#define SCRIPT_FOR 0x574bc0u
#define SCRIPT_DEFAULT_VICTORY 0x574c68u
#define SCRIPT_COMPARE 0x574ba8u
#define SCRIPT_CHANGE 0x574ca0u
#define SCRIPT_ALIVE 0x574b70u
#define SCRIPT_DEFAULT_COMMAND 0x574c98u
static int alive_node_create(GameApplication *,GameScriptReader *,GameAddress *);
static int alive_node_evaluate(GameApplication *,GameAddress,int *);
static int default_command_execute(GameApplication *,int *);
static int scalar_node_create(GameApplication *,GameScriptReader *,GameAddress *,int);
static int scalar_node_execute(GameApplication *,GameAddress,int *,int);
static int scalar_node_destroy(GameApplication *,GameAddress,int);
static GameAddress script_node(uint32_t kind,size_t bytes){
    GameAddress id=scene_projection_storage_allocate(bytes,"script node");
    uint8_t *p=scene_projection_storage_borrow_bytes(id,bytes);
    if(!p){if(id)(void)scene_projection_storage_free(id);
    return 0;}
    memset(p,0,bytes);store(p,kind);return id;
}
static GameAddress scalar_record(GameApplication *a,uint32_t index){
    if(index>=get(a,0x605884)){a->memory.fault=1;return 0;}
    return get(a,get(a,0x605870)+index*4);
}
static int scalar_index(GameApplication *a,const char *name,uint32_t *index){
    GameAddress id=game_script_variable_find(a,name,0);uint32_t i;
    if(!id || !a->scripts)return 0;
    for(i=0;i<a->scripts->scalar.count;++i)
        if(get(a,a->scripts->scalar.rows+i*4)==id){*index=i;return 1;}
    return 0;
}
static int condition_parse(GameApplication *a,GameScriptReader *r,GameAddress *out,unsigned depth){
    GameAddress left=0,right=0,id=0;uint32_t kind;int unary;
    *out=0;if(depth>=256)return 0;
    if(!strcmp(r->token,"VBL_TEST"))return scalar_node_create(a,r,out,1);
    if(!strcmp(r->token,"IS_ALIVE"))return alive_node_create(a,r,out);
    if(!strcmp(r->token,"(")){
        if(!game_script_next(r) || !condition_parse(a,r,&id,depth+1))return 0;
        if(strcmp(r->token,")") || !game_script_next(r))goto fail;
        *out=id;return 1;
    }
    if(!strcmp(r->token,"AND"))kind=SCRIPT_AND;
    else if(!strcmp(r->token,"OR"))kind=SCRIPT_OR;
    else if(!strcmp(r->token,"NOT"))kind=SCRIPT_NOT;
    else return game_script_condition_leaf_create(a,r,out) && *out!=0;
    unary=kind==SCRIPT_NOT;
    if(!expect(r,"(") || !game_script_next(r) || !condition_parse(a,r,&left,depth+1))goto fail;
    if(!unary && !condition_parse(a,r,&right,depth+1))goto fail;
    if(strcmp(r->token,")") || !game_script_next(r))goto fail;
    id=script_node(kind,unary?8:12);if(!id)goto fail;
    put(a,id+4,left);if(!unary)put(a,id+8,right);
    if(a->memory.fault)goto fail;
    *out=id;return 1;
fail:
    if(left)(void)game_script_condition_destroy(a,left);
    if(right)(void)game_script_condition_destroy(a,right);
    if(id)(void)scene_projection_storage_free(id);
    return 0;
}
int game_script_condition_create(GameApplication *a,GameScriptReader *r,GameAddress *out){
    return a && r && out && condition_parse(a,r,out,0);
}
int game_script_condition_destroy(GameApplication *a,GameAddress id){
    uint32_t kind;if(!id)return 1;kind=get(a,id);
    if(kind==SCRIPT_COMPARE)return scalar_node_destroy(a,id,1);
    if(kind==SCRIPT_ALIVE)return !a->memory.fault && scene_projection_storage_free(id);
    if(kind!=SCRIPT_AND && kind!=SCRIPT_OR && kind!=SCRIPT_NOT)
        return !a->memory.fault && game_script_condition_leaf_destroy(a,id);
    if(!game_script_condition_destroy(a,get(a,id+4)))return 0;
    put(a,id+4,0);
    if(kind!=SCRIPT_NOT){if(!game_script_condition_destroy(a,get(a,id+8)))return 0;
    put(a,id+8,0);}
    return !a->memory.fault && scene_projection_storage_free(id);
}
static int condition_eval(GameApplication *a,GameAddress id,int *result,unsigned depth){
    uint32_t kind;int left,right;if(!id || depth>=256)return 0;kind=get(a,id);
    if(a->memory.fault)return 0;
    if(kind==SCRIPT_COMPARE)return scalar_node_execute(a,id,result,1);
    if(kind==SCRIPT_ALIVE)return alive_node_evaluate(a,id,result);
    if(kind!=SCRIPT_AND && kind!=SCRIPT_OR && kind!=SCRIPT_NOT)
        return game_script_condition_leaf_evaluate(a,id,result);
    if(!condition_eval(a,get(a,id+4),&left,depth+1))return 0;
    if(kind==SCRIPT_NOT){*result=!left;return 1;}
    /*Original AND/OR short circuit; unevaluated leaves may consume RNG. */
    if((kind==SCRIPT_AND && !left) || (kind==SCRIPT_OR && left)){*result=left!=0;return 1;}
    if(!condition_eval(a,get(a,id+8),&right,depth+1))return 0;
    *result=right!=0;return 1;
}
int game_script_condition_evaluate(GameApplication *a,GameAddress id,int *result){
    return a && result && condition_eval(a,id,result,0);
}
static int statement_parse(GameApplication *,GameScriptReader *,GameAddress *,unsigned);
static int rows_retire(GameApplication *a,GameAddress rows,uint32_t count){
    uint32_t i;
    if(count>65536 || a->memory.fault)return 0;
    for(i=0;i<count;++i){GameAddress child=get(a,rows+i*4);
        if(child && !game_script_statement_destroy(a,child))return 0;
        put(a,rows+i*4,0);
    }
    if(rows && !scene_projection_storage_free(rows))return 0;
    return !a->memory.fault;
}
static int array_retire(GameApplication *a,GameAddress id,unsigned pointer_offset,unsigned count_offset){
    if(!rows_retire(a,get(a,id+pointer_offset),get(a,id+count_offset)))return 0;
    put(a,id+pointer_offset,0);put(a,id+count_offset,0);return !a->memory.fault;
}
/*485380/486770/486b60: body nodes retain sibling links for layout consumers;
 *the pointer array owns the same nodes, not a competing copy of their state. */
static int body_parse(GameApplication *a,GameScriptReader *r,GameAddress *rows,uint32_t *count,unsigned depth){
    GameAddress head=0,tail=0,id=0,table=0;uint32_t n=0,i;uint8_t *p;
    *rows=0;*count=0;
    if(strcmp(r->token,"(") || !game_script_next(r))return 0;
    while(strcmp(r->token,")")){
        if(n>=65536 || !statement_parse(a,r,&id,depth+1) || !id)goto fail;
        if(tail)put(a,tail+4,id);else head=id;tail=id;id=0;++n;
        if(a->memory.fault)goto fail;
    }
    if(!n || !game_script_next(r))goto fail;
    table=scene_projection_storage_allocate((size_t)n*4,"script body");
    p=scene_projection_storage_borrow_bytes(table,(size_t)n*4);if(!p)goto fail;
    id=head;for(i=0;i<n;++i){store(p+i*4,id);id=get(a,id+4);}id=0;
    if(a->memory.fault)goto fail;
    *rows=table;*count=n;return 1;
fail:
    if(table)(void)scene_projection_storage_free(table);
    if(id)(void)game_script_statement_destroy(a,id);
    while(head){GameAddress next=get(a,head+4);if(!game_script_statement_destroy(a,head))break;head=next;}
    return 0;
}
static int statement_parse(GameApplication *a,GameScriptReader *r,GameAddress *out,unsigned depth){
    GameAddress id=0,condition=0,first=0,second=0;uint32_t nfirst=0,nsecond=0,index=0,lo=0,hi=0,value=0,kind;
    *out=0;if(depth>=256)return 0;
    if(!strcmp(r->token,"CHANGE_VBL"))return scalar_node_create(a,r,out,0);
    if(!strcmp(r->token,"IF"))kind=SCRIPT_IF;
    else if(!strcmp(r->token,"FOR"))kind=SCRIPT_FOR;
    else if(!strcmp(r->token,"SET_USE_DEFAULT_VICTORY"))kind=SCRIPT_DEFAULT_VICTORY;
    else return game_script_statement_leaf_create(a,r,out) && *out!=0;
    if(kind==SCRIPT_DEFAULT_VICTORY){
        if(!game_script_next(r) || r->token[0]<'0' || r->token[0]>'9')goto fail;
        value=(uint32_t)(uint64_t)strtoll(r->token,NULL,10);
        if(!game_script_next(r))goto fail;
        id=script_node(kind,12);if(!id)goto fail;put(a,id+8,value);
    }else{
        if(kind==SCRIPT_IF){
            if(!expect(r,"(") || !game_script_next(r) || !condition_parse(a,r,&condition,depth+1))goto fail;
            if(strcmp(r->token,")") || !expect(r,"THEN") || !game_script_next(r))goto fail;
        }else{
            if(!game_script_next(r) || !scalar_index(a,r->token,&index) || !expect(r,"EQUAL") ||
               !numeric(r,1,&lo) || !expect(r,"TO") || !numeric(r,1,&hi) ||
               !expect(r,"DO") || !game_script_next(r))goto fail;
        }
        if(!body_parse(a,r,&first,&nfirst,depth))goto fail;
        if(kind==SCRIPT_IF && !strcmp(r->token,"ELSE")){
            if(!game_script_next(r) || !body_parse(a,r,&second,&nsecond,depth))goto fail;
        }
        id=script_node(kind,28);if(!id)goto fail;
        if(kind==SCRIPT_IF){put(a,id+8,condition);put(a,id+12,first);put(a,id+16,second);put(a,id+20,nfirst);put(a,id+24,nsecond);}
        else{put(a,id+8,index);put(a,id+12,lo);put(a,id+16,hi);put(a,id+20,nfirst);put(a,id+24,first);}
    }
    if(a->memory.fault)goto fail;
    *out=id;return 1;
fail:
    if(condition)(void)game_script_condition_destroy(a,condition);
    if(first)(void)rows_retire(a,first,nfirst);
    if(second)(void)rows_retire(a,second,nsecond);
    if(id)(void)scene_projection_storage_free(id);
    return 0;
}
int game_script_statement_create(GameApplication *a,GameScriptReader *r,GameAddress *out){
    return a && r && out && statement_parse(a,r,out,0);
}
int game_script_statement_destroy(GameApplication *a,GameAddress id){
    uint32_t kind;if(!a)return 0;if(!id)return 1;kind=get(a,id);
    if(kind==SCRIPT_CHANGE)return scalar_node_destroy(a,id,0);
    if(kind==SCRIPT_IF){
        if(!game_script_condition_destroy(a,get(a,id+8)))return 0;
    put(a,id+8,0);
        if(!array_retire(a,id,12,20) || !array_retire(a,id,16,24))return 0;
    }else if(kind==SCRIPT_FOR){if(!array_retire(a,id,24,20))return 0;}
    else if(kind!=SCRIPT_DEFAULT_VICTORY && kind!=SCRIPT_DEFAULT_COMMAND)return !a->memory.fault && game_script_statement_leaf_destroy(a,id);
    return !a->memory.fault && scene_projection_storage_free(id);
}
static int statement_execute(GameApplication *,GameAddress,int *,unsigned);
static int body_execute(GameApplication *a,GameAddress rows,uint32_t count,unsigned depth){
    uint32_t i;int ignored;if(count>65536)return 0;
    for(i=0;i<count;++i){GameAddress child=get(a,rows+i*4);
        if(a->memory.fault || (child && !statement_execute(a,child,&ignored,depth+1)))return 0;
    }
    return !a->memory.fault;
}
static int statement_execute(GameApplication *a,GameAddress id,int *result,unsigned depth){
    uint32_t kind;if(depth>=256 || !id)return 0;kind=get(a,id);if(a->memory.fault)return 0;
    if(kind==SCRIPT_CHANGE)return scalar_node_execute(a,id,result,0);
    if(kind==SCRIPT_DEFAULT_COMMAND)return default_command_execute(a,result);
    if(kind==SCRIPT_DEFAULT_VICTORY)put(a,0x605890,get(a,id+8));
    else if(kind==SCRIPT_IF){int truth;
        if(!game_script_condition_evaluate(a,get(a,id+8),&truth))return 0;
        if(!body_execute(a,get(a,id+(truth?12:16)),get(a,id+(truth?20:24)),depth))return 0;
    }else if(kind==SCRIPT_FOR){GameAddress record=scalar_record(a,get(a,id+8));uint32_t value=get(a,id+12),limit=get(a,id+16);
        if(!record || a->memory.fault)return 0;
    put(a,record+32,value);
        /*The body may change the loop variable. Re-read it before incrementing;
         *signed comparisons and modulo32 increment reproduce4dd5d0. */
        while((int32_t)value<=(int32_t)limit){
            if(!body_execute(a,get(a,id+24),get(a,id+20),depth))return 0;
            value=get(a,record+32)+1u;put(a,record+32,value);if(a->memory.fault)return 0;
        }
    }else {
        if(!game_script_statement_leaf_execute(a,id,result)){
            fprintf(stderr,"native script statement failed: kind=%08x node=%08x fault=%d\n",kind,id,a->memory.fault);return 0;
        }
        return 1;
    }
    *result=1;return !a->memory.fault;
}
int game_script_statement_execute(GameApplication *a,GameAddress id,int *result){
    return a && result && statement_execute(a,id,result,0);
}
/*4e1b50: index both live chains in original order. The index borrows nodes;
 *the script tree and auxiliary manager retain their separate actual owners. */
int game_level_commands_initialize(GameApplication *a){
    struct GameScriptState *s;GameAddress roots[2],id,rows=0;uint32_t count=0,i=0,k;uint8_t *p;
    if(!a || a->memory.fault)return 0;
    roots[0]=get(a,0x60587c);roots[1]=get(a,0x605878);
    s=a->scripts;
    if(!s){
        if(roots[0] || get(a,0x605870) || get(a,0x60586c) || get(a,0x605894))return 0;
        s=calloc(1,sizeof(*s));if(!s)return 0;a->scripts=s;
    }
    if(roots[0]!=s->root)return 0;
    for(k=0;k<2;++k)for(id=roots[k];id;id=get(a,id+4))
        if(++count>65536 || a->memory.fault)return 0;
    if(count){
        rows=scene_projection_storage_allocate((size_t)count*4,"script command index");
        p=scene_projection_storage_borrow_bytes(rows,(size_t)count*4);
        if(!p){if(rows)(void)scene_projection_storage_free(rows);return 0;}
        for(k=0;k<2;++k)for(id=roots[k];id;id=get(a,id+4))store(p+(i++)*4,id);
        if(a->memory.fault){(void)scene_projection_storage_free(rows);return 0;}
    }
    if(s->commands && !scene_projection_storage_free(s->commands)){if(rows)(void)scene_projection_storage_free(rows);return 0;}
    s->commands=rows;s->command_count=count;put(a,0x605894,rows);put(a,0x60588c,count);
    return !a->memory.fault;
}
/*4e1bf0: statement return values are ignored, but native service failure is
 *distinct from the language's false result and stops the update. */
int game_level_commands_update(GameApplication *a){
    if(!a || a->memory.fault)return 0;
    if(!a->scripts)return !get(a,0x60588c) && !get(a,0x605894);
    return body_execute(a,a->scripts->commands,a->scripts->command_count,0);
}

/*48b8c0/48bdb0,4e0a40/4e0de0 and4e11c0: scalar bindings share the
 *existing variable/player/citadel/settings owners. A binding caches only an
 *object identity or a variable index, never a second writable value. */
typedef struct ScriptScalar {uint32_t type,arg;GameAddress binding;} ScriptScalar;
static const GameAddress setting_words[39]={
    0x604164,0x604160,0x60412c,0x604168,0x604158,0x604174,0x60415c,
    0x6041a8,0x604198,0x6041b0,0x604154,0x604150,0x6041c0,0x6041a0,
    0x60418c,0x6041a4,0x604130,0x604194,0x604178,0x5ac690,0x5ac694,
    0x5ac698,0x5ac69c,0x604138,0x6041c4,0x604140,0x604134,0x604144,
    0x604128,0x60413c,0x604190,0x60419c,0x604170,0x604124,0x604180,
    0x6041c8,0x6041b8,0x604148,0x629730
};
/*The audio bank's duration owner is a remaining native service boundary. */
static int32_t duration_format(void *context,GameAddress buffer,uint32_t *frequency,uint16_t *bits){
    GameApplication *a=context;
    if(!sound_platform_owns(buffer)){a->memory.fault=1;a->memory.fault_address=buffer;a->memory.fault_size=4;return -1;}
    return sound_platform_format(buffer,frequency,bits);
}
/*52f1e0 shares the actual native bank/voice owner and format provider. */
int game_script_sound_duration(GameApplication *a,uint32_t id,long double *duration){
    SoundPlaybackServices services={a,NULL,NULL,NULL,duration_format};GameAddress bank;
    if(!a || !duration || a->memory.fault)return 0;
    bank=get(a,0x628a90);if(!bank || a->memory.fault)return 0;
    *duration=sound_playback_duration(&a->memory,bank,(int16_t)(uint16_t)id,&services);
    return !a->memory.fault;
}
static float scalar_float(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
static void scalar_float_put(GameApplication *a,GameAddress p,float v){uint32_t bits;memcpy(&bits,&v,4);put(a,p,bits);}
static uint32_t scalar_truncate(long double v){
    /*560db0 truncates to64 bits, then its callers consume the low32 word.
     *Invalid conversion produces INT64_MIN, whose low word is zero. */
    v=truncl(v);if(!isfinite(v) || v<-9223372036854775808.0L || v>=9223372036854775808.0L)return 0;
    return (uint32_t)(uint64_t)(int64_t)v;
}
static int named_id(GameApplication *a,const char *name,GameAddress rows,uint32_t count,uint32_t stride,uint32_t *id){
    uint32_t i;size_t j;
    for(i=0;i<count;++i){GameAddress text=get(a,rows+i*stride);
        if(a->memory.fault)return 0;
        for(j=0;j<512;++j){unsigned char c=game_memory_read_u8(&a->memory,text+(uint32_t)j);
            if(a->memory.fault || c!=(unsigned char)name[j])break;
            if(!c){*id=i;return 1;}
        }
    }
    return 0;
}
static int token_integer(const char *text,uint32_t *value){
    if(text[0]<'0' || text[0]>'9')return 0;
    *value=(uint32_t)(uint64_t)strtoll(text,NULL,10);return 1;
}
static int scalar_parse(GameApplication *a,GameScriptReader *r,ScriptScalar *s){
    uint32_t index,value;const char *t=r->token;int player=0;uint32_t literal=0,variable=0;
    memset(s,0,sizeof(*s));
    if(!strcmp(t,"GAME_TIME")){s->type=8;return 1;}
    if(!strcmp(t,"LFL_SETTINGS"))return game_script_next(r) && named_id(a,r->token,0x590928,39,4,&s->arg);
    if(!strcmp(t,"RANDOM_INT")){s->type=11;return game_script_next(r) && token_integer(r->token,&s->arg);}
    if(!strcmp(t,"SOUND_LENGTH")){s->type=19;return game_script_next(r) && named_id(a,r->token,0x59ac58,276,16,&s->arg);}
    if(!strcmp(t,"PLAYER_MONEY")){literal=1;variable=2;player=1;}
    else if(!strcmp(t,"CITADEL_LODE")){literal=3;variable=4;}
    else if(!strcmp(t,"PLAYER_TANK_HEALTH")){literal=10;variable=9;player=1;}
    else if(!strcmp(t,"PLAYER_SPEED_SETTING")){literal=12;variable=13;player=1;}
    else if(!strcmp(t,"PLAYER_WEPN_SETTING")){literal=14;variable=15;player=1;}
    else if(!strcmp(t,"PLAYER_SHIELD_SETTING")){literal=16;variable=17;player=1;}
    else{
        if(!scalar_index(a,t,&s->arg))return 0;
        s->type=7;return 1;
    }
    if(!game_script_next(r))return 0;
    if(scalar_index(a,r->token,&index)){s->type=variable;s->arg=index;return 1;}
    if(!token_integer(r->token,&value) || (player && value>=6))return 0;
    s->type=literal;s->arg=value;return 1;
}
static int scalar_is_player(uint32_t type){return type==1 || type==2 || type==9 || type==10 || (type>=12 && type<=17);}
static int scalar_dynamic(uint32_t type){return type==2 || type==9 || type==13 || type==15 || type==17;}
static GameAddress selected_player(GameApplication *a,uint32_t selector){
    GameAddress object;
    if(selector>=6)return 0;
    selector=get(a,0x5f01f0+selector*4);if(selector>=6)return 0;
    object=get(a,0x5f0210+selector*4);
    return object && get(a,object+20)?object:0;
}
static int scalar_bind(GameApplication *a,ScriptScalar *s){
    if(!scalar_is_player(s->type))return 1;
    s->binding=scene_projection_storage_allocate(8,"script player binding");if(!s->binding)return 0;
    put(a,s->binding+4,UINT32_MAX);
    if(scalar_dynamic(s->type)){
        if(s->arg==UINT32_MAX)put(a,s->binding,get(a,0x5f0208));
        else{put(a,s->binding,0);put(a,s->binding+4,s->arg);}
    }else put(a,s->binding,s->arg==UINT32_MAX?get(a,0x5f0208):selected_player(a,s->arg));
    return !a->memory.fault;
}
static GameAddress scalar_player(GameApplication *a,const ScriptScalar *s){
    GameAddress object=get(a,s->binding),record;uint32_t index;
    if(object)return object;
    index=get(a,s->binding+4);if(index>=get(a,0x605884))return 0;
    record=scalar_record(a,index);return record?selected_player(a,get(a,record+32)):0;
}
static GameAddress scalar_setting(GameApplication *a,uint32_t index){
    static const GameAddress inputs[3]={0x6041b4,0x604184,0x60417c};
    if(index>=39)return 0;
    if(index>=3 && index<=5)put(a,setting_words[index],scalar_truncate(scalar_float(a,inputs[index-3])));
    return setting_words[index];
}
static int setting_commit(GameApplication *a,uint32_t index){
    static const GameAddress outputs[3]={0x6041b4,0x604184,0x60417c};
    int32_t value;GameAddress p;if(index>=39)return 0;p=setting_words[index];value=(int32_t)get(a,p);
    if(index>=3 && index<=5){
        long double low=scalar_float(a,0x574bd0),v=value;
        scalar_float_put(a,outputs[index-3],(float)(v<low?low:v));
    }else{
        int32_t low=0,high=INT32_MAX;
        if(index<3)high=100;else if(index>=6 && index<=12){low=-10;high=10;}
        else if(index==16 || index==17)high=1;else if(index==18)high=2;
        if(value<low)value=low;
        if(value>high)value=high;
        put(a,p,(uint32_t)value);
    }
    return !a->memory.fault;
}
static GameAddress scalar_location(GameApplication *a,const ScriptScalar *s){
    GameAddress object,record;uint32_t index=s->arg;
    if(s->type==0)return scalar_setting(a,index);
    if(s->type==7){record=scalar_record(a,index);return record?record+32:0;}
    if(s->type==1 || s->type==2){
        object=scalar_player(a,s);return object && game_memory_read_u16(&a->memory,object+8)!=5?object+0x22c:0;
    }
    if(s->type==3 || s->type==4){
        if(s->type==4){record=scalar_record(a,index);if(!record)return 0;index=get(a,record+32);}
        if((int32_t)index<0 || (int32_t)index>=(int16_t)game_memory_read_u16(&a->memory,0x639a74))return 0;
        object=get(a,get(a,0x639a70)+index*4);return object?object+20:0;
    }
    return 0;
}
static int scalar_value(GameApplication *a,const ScriptScalar *s,uint32_t *out){
    GameAddress p,object;uint32_t index;long double duration;
    *out=0;if(s->type==18){*out=s->arg;return 1;}
    if(s->type<=4 || s->type==7){p=scalar_location(a,s);if(p)*out=get(a,p);}
    else if(s->type==8)*out=(uint32_t)((int32_t)(get(a,0x5ac6ac)<<5)/1000);
    else if(s->type==11){
        index=get(a,0x6308c0);if(index==1024)index=0;if(index>=1024)return 0;
        put(a,0x6308c0,index+1);*out=scalar_truncate((long double)(int32_t)s->arg*scalar_float(a,0x6308c8+index*4));
    }else if(scalar_is_player(s->type)){
        object=scalar_player(a,s);
        if(object && game_memory_read_u16(&a->memory,object+8)<4){
            object=get(a,object+0x3bc);
            if(s->type==9 || s->type==10)*out=get(a,object+0x58);
            else{
                unsigned offset=s->type==12 || s->type==13?0x2dc:s->type==14 || s->type==15?0x2e4:0x2e0;
                *out=scalar_truncate((long double)scalar_float(a,object+offset)*scalar_float(a,0x574be4));
            }
        }
    }else if(s->type==19){
        if(get(a,0x628a90)){
            if(!game_script_sound_duration(a,s->arg,&duration))return 0;
            *out=scalar_truncate(duration)+1u;
        }
    }
    return !a->memory.fault;
}
static int scalar_node_create(GameApplication *a,GameScriptReader *r,GameAddress *out,int comparison){
    static const char *compare_names[5]={"EQUAL","GREATER_THAN","LESS_THAN","GREATER_THAN_EQUAL","LESS_THAN_EQUAL"};
    static const char *change_names[5]={"ADD","SUB","MUL","DIV","SET"};
    ScriptScalar left={0},right={0};uint32_t op=0;GameAddress id=0;unsigned base=comparison?4:8;
    *out=0;
    if(!game_script_next(r) || !scalar_parse(a,r,&left) || !game_script_next(r))goto fail;
    for(op=0;op<5;++op)if(!strcmp(r->token,(comparison?compare_names:change_names)[op]))break;
    if(op==5 || !game_script_next(r))goto fail;
    if((r->token[0]>='0' && r->token[0]<='9') || r->token[0]=='-'){
        right.type=18;right.arg=(uint32_t)(uint64_t)strtoll(r->token,NULL,10);
    }else if(!scalar_parse(a,r,&right))goto fail;
    if(!game_script_next(r) || !scalar_bind(a,&left) || !scalar_bind(a,&right))goto fail;
    id=script_node(comparison?SCRIPT_COMPARE:SCRIPT_CHANGE,comparison?32:36);if(!id)goto fail;
    put(a,id+base,left.type);put(a,id+base+4,left.arg);put(a,id+base+8,right.type);
    put(a,id+base+12,right.arg);put(a,id+base+16,op);put(a,id+base+20,left.binding);put(a,id+base+24,right.binding);
    if(a->memory.fault)goto fail;
    *out=id;return 1;
fail:
    if(left.binding)(void)scene_projection_storage_free(left.binding);
    if(right.binding)(void)scene_projection_storage_free(right.binding);
    if(id)(void)scene_projection_storage_free(id);
    return 0;
}
static int scalar_node_destroy(GameApplication *a,GameAddress id,int comparison){
    unsigned base=comparison?4:8;GameAddress first=get(a,id+base+20),second=get(a,id+base+24);
    if(a->memory.fault)return 0;
    if(first && !scene_projection_storage_free(first))return 0;
    put(a,id+base+20,0);
    if(second && !scene_projection_storage_free(second))return 0;
    put(a,id+base+24,0);return !a->memory.fault && scene_projection_storage_free(id);
}
static int scalar_node_execute(GameApplication *a,GameAddress id,int *result,int comparison){
    unsigned base=comparison?4:8;ScriptScalar left,right;uint32_t x=0,y,op;GameAddress p;
    left.type=get(a,id+base);left.arg=get(a,id+base+4);left.binding=get(a,id+base+20);
    right.type=get(a,id+base+8);right.arg=get(a,id+base+12);right.binding=get(a,id+base+24);op=get(a,id+base+16);
    if(a->memory.fault)return 0;
    if(comparison){
        if(!scalar_value(a,&left,&x) || !scalar_value(a,&right,&y))return 0;
        switch(op){
        case 0:*result=x==y;break;case 1:*result=(int32_t)x>(int32_t)y;break;
        case 2:*result=(int32_t)x<(int32_t)y;break;case 3:*result=(int32_t)x>=(int32_t)y;break;
        case 4:*result=(int32_t)x<=(int32_t)y;break;default:*result=0;break;
        }
        return 1;
    }
    p=scalar_location(a,&left);if(a->memory.fault)return 0;
    if(!p){*result=1;return 1;}
    if(!scalar_value(a,&right,&y))return 0;
    x=get(a,p);
    switch(op){case 0:x+=y;break;case 1:x-=y;break;case 2:x*=y;break;
    case 3:if(!y || (x==0x80000000u && y==UINT32_MAX)){a->memory.fault=1;return 0;}x=(uint32_t)((int32_t)x/(int32_t)y);break;
    case 4:x=y;break;default:break;}
    put(a,p,x);
    if(left.type==0){if(!setting_commit(a,left.arg))return 0;}
    else if(left.type>=1 && left.type<=4){p=scalar_location(a,&left);if(p && (int32_t)get(a,p)<=0)put(a,p,0);}
    *result=1;return !a->memory.fault;
}

GameAddress game_script_name_find(GameApplication *a,const char *name){
    if(!a || !name || !a->scripts || !a->scripts->manager_ready || !a->scripts->names)return 0;
    return application_name_registry_find(name);
}
/*4e1a00/4e1b10: case-insensitive sorted names. Defining an existing name
 *preserves its original binding; actual object creation updates that record. */
int game_script_name_define(GameApplication *a,const char *name,GameAddress actor,GameAddress *out){
    uint32_t generation;size_t length;
    if(!out)return 0;
    *out=0;if(!a || !name || !a->scripts || !a->scripts->manager_ready || a->memory.fault)return 0;
    length=strlen(name);if(!length || length>=32)return 0;
    *out=application_name_registry_find(name);if(*out)return 1;
    if(application_name_registry_is_marker(name))return 1;
    generation=actor?get(a,actor+20):0;
    return !a->memory.fault && application_name_registry_register(name,actor,generation,out);
}
static int alive_node_create(GameApplication *a,GameScriptReader *r,GameAddress *out){
    GameAddress binding,id;
    *out=0;if(!game_script_next(r) || !game_script_name_define(a,r->token,0,&binding) || !binding || !game_script_next(r))return 0;
    id=script_node(SCRIPT_ALIVE,8);if(!id)return 0;put(a,id+4,binding);
    if(a->memory.fault){(void)scene_projection_storage_free(id);return 0;}*out=id;return 1;
}
/*A retired native handle is a false identity test, not a memory fault. Probe
 *the real owner before reading; an in-image record uses the same endian view. */
static int name_generation(GameApplication *a,GameAddress actor,uint32_t *generation){
    GameMemory *m=&a->memory;uint8_t bytes[4];GameAddress p=actor+20;int owned;
    if(!actor || p<actor)return 0;
    owned=(p>=m->owned_begin && p<m->owned_end) || (m->owned_pages && m->owned_pages[p>>12]);
    if(m->read_fallback && (owned || !game_memory_contains(m,p,4)))
        return m->read_fallback(m->fallback_context,p,bytes,4)?(*generation=word(bytes),1):0;
    if(!game_memory_contains(m,p,4))return 0;
    *generation=get(a,p);return !m->fault;
}
static int alive_node_evaluate(GameApplication *a,GameAddress id,int *result){
    GameAddress binding=get(a,id+4),actor;uint32_t generation,captured;*result=0;
    if(a->memory.fault || !binding)return 0;
    if(!application_name_registry_binding(binding,&actor,&captured))return 0;
    if(actor && name_generation(a,actor,&generation))*result=generation==captured;
    return !a->memory.fault;
}
/*48c6c0/48c740 and4dc740..4dced0. A leaf borrows the same player/name
 *binding used by the script manager; only its small language record is owned.
 *The original method addresses are language IDs, not callable guest tables. */
#define SCRIPT_PLAYER_PROPS 0x574b68u
#define SCRIPT_HAS_UNITS 0x574b78u
#define SCRIPT_ONLY_SURVIVOR 0x574b80u
#define SCRIPT_LIVED_DIED 0x574b88u
#define SCRIPT_DIED_WELL 0x574b90u
#define SCRIPT_DIED_VIOLENT 0x574b98u
#define SCRIPT_GATE_UP 0x574ba0u
#define SCRIPT_CLOSE_OBJECT 0x574b40u
#define SCRIPT_CLOSE_PLAYER 0x574b48u
#define SCRIPT_POINT_OBJECT 0x574b50u
#define SCRIPT_POINT_PLAYER 0x574b58u
#define SCRIPT_CITADEL 0x574b60u
static int spatial_leaf_create(GameApplication *,GameScriptReader *,GameAddress *);
static int spatial_leaf_evaluate(GameApplication *,GameAddress,int *);
static int spatial_leaf_kind(uint32_t kind){return kind>=SCRIPT_CLOSE_OBJECT && kind<=SCRIPT_CITADEL && (kind&7u)==0;}
static int player_leaf_kind(uint32_t kind){
    return kind==SCRIPT_PLAYER_PROPS || kind==SCRIPT_HAS_UNITS || kind==SCRIPT_ONLY_SURVIVOR;
}
static int condition_leaf_kind(uint32_t kind){
    return spatial_leaf_kind(kind) || player_leaf_kind(kind) || kind==SCRIPT_LIVED_DIED || kind==SCRIPT_DIED_WELL ||
        kind==SCRIPT_DIED_VIOLENT || kind==SCRIPT_GATE_UP;
}
static GameAddress leaf_player(GameApplication *a,GameAddress id,uint32_t limit){
    GameAddress player=get(a,id+4),record;uint32_t index,selector;
    if(player)return player;
    index=get(a,id+8);if(index>=get(a,0x605884))return 0;
    record=scalar_record(a,index);if(!record)return 0;selector=get(a,record+32);
    if(selector>=limit)return 0;
    selector=get(a,0x5f01f0+selector*4);if(selector>=limit)return 0;
    player=get(a,0x5f0210+selector*4);
    return player && get(a,player+20)?player:0;
}
int game_script_condition_leaf_create(GameApplication *a,GameScriptReader *r,GameAddress *out){
    uint32_t kind,selector=UINT32_MAX,index=UINT32_MAX,property=0,i,count;GameAddress id=0,binding=0,player=0;
    if(!a || !r || !out || a->memory.fault)return 0;
    *out=0;
    if(!strcmp(r->token,"CLOSE") || !strcmp(r->token,"CLOSE_PT") || !strcmp(r->token,"CITADEL_OWNED") ||
       !strcmp(r->token,"CITADEL_OWNED_W_TOWER") || !strcmp(r->token,"CITADEL_OWNED_W_STRG_TOWER"))
        return spatial_leaf_create(a,r,out);
    if(!strcmp(r->token,"PLAYER_PROPS"))kind=SCRIPT_PLAYER_PROPS;
    else if(!strcmp(r->token,"PLAYER_HAS_UNITS"))kind=SCRIPT_HAS_UNITS;
    else if(!strcmp(r->token,"ONLY_I_SURVIVE"))kind=SCRIPT_ONLY_SURVIVOR;
    else if(!strcmp(r->token,"LIVED_AND_DIED"))kind=SCRIPT_LIVED_DIED;
    else if(!strcmp(r->token,"DIED_WELL"))kind=SCRIPT_DIED_WELL;
    else if(!strcmp(r->token,"DIED_VIOLENT"))kind=SCRIPT_DIED_VIOLENT;
    else if(!strcmp(r->token,"GATE_UP"))kind=SCRIPT_GATE_UP;
    else return 0;
    if(!game_script_next(r))return 0;
    if(player_leaf_kind(kind)){
        if(!scalar_index(a,r->token,&index)){
            if(token_integer(r->token,&selector) && selector<6){
                uint32_t mapped=get(a,0x5f01f0+selector*4),limit=kind==SCRIPT_PLAYER_PROPS?4:6;
                if(mapped<limit){player=get(a,0x5f0210+mapped*4);if(player && !get(a,player+20))player=0;}
            }else {selector=UINT32_MAX;if(kind==SCRIPT_PLAYER_PROPS)return 0;}
            /*An unrecognized selector in48c740 means the current player. */
            if(selector==UINT32_MAX && index==UINT32_MAX)player=get(a,0x5f0208);
        }
        if(!game_script_next(r))return 0;
        if(kind==SCRIPT_PLAYER_PROPS){
            if(!strcmp(r->token,"IN_OVERHEAD_MAP"))property=0;
            else if(!strcmp(r->token,"IN_CITADEL"))property=1;
            else if(!strcmp(r->token,"IN_POWER_TRI"))property=2;
            else return 0;
            if(!game_script_next(r))return 0;
        }
        id=script_node(kind,kind==SCRIPT_PLAYER_PROPS?16:12);
        if(id){put(a,id+4,player);put(a,id+8,index);if(kind==SCRIPT_PLAYER_PROPS)put(a,id+12,property);}
    }else{
        if(kind==SCRIPT_GATE_UP){
            count=get(a,0x5e147c);if(count>65536)return 0;binding=UINT32_MAX;
            for(i=0;i<count;++i){GameAddress gate=get(a,get(a,0x5e1480)+i*4);size_t j;
                if(!gate)return 0;
                for(j=0;j<sizeof(r->token);++j){unsigned char c=game_memory_read_u8(&a->memory,gate+(uint32_t)j),t=(unsigned char)r->token[j];
                    if(c>='A' && c<='Z')c+='a'-'A';
                    if(t>='A' && t<='Z')t+='a'-'A';
                    if(c!=t || a->memory.fault)break;
                    if(!c){binding=i;break;}
                }
                if(a->memory.fault)return 0;
            }
        }else if(!game_script_name_define(a,r->token,0,&binding) || !binding)return 0;
        if(!game_script_next(r))return 0;
        id=script_node(kind,8);if(id)put(a,id+4,binding);
    }
    if(!id)return 0;
    if(a->memory.fault){(void)scene_projection_storage_free(id);return 0;}
    *out=id;return 1;
}
int game_script_condition_leaf_destroy(GameApplication *a,GameAddress id){
    return a && id && !a->memory.fault && condition_leaf_kind(get(a,id)) &&
        !a->memory.fault && scene_projection_storage_free(id);
}
int game_script_condition_leaf_evaluate(GameApplication *a,GameAddress id,int *result){
    uint32_t kind,index,i,captured,generation,property;GameAddress player,other,binding,actor;int alive;
    if(!a || !id || !result || a->memory.fault)return 0;
    *result=0;kind=get(a,id);if(!condition_leaf_kind(kind) || a->memory.fault)return 0;
    if(spatial_leaf_kind(kind))return spatial_leaf_evaluate(a,id,result);
    if(player_leaf_kind(kind)){
        player=leaf_player(a,id,kind==SCRIPT_PLAYER_PROPS?4:6);if(a->memory.fault)return 0;
        if(!player)return 1;
        if(kind==SCRIPT_HAS_UNITS)return game_script_player_units_alive(a,player,result);
        if(kind==SCRIPT_ONLY_SURVIVOR){
            for(i=0;i<6;++i){other=get(a,0x5f0210+i*4);
                if(!other)return 0;
                if(other!=player && get(a,other+20)){
                    if(!game_script_player_units_alive(a,other,&alive))return 0;
                    if(alive)return 1;
                }
            }
            *result=1;
        }else{
            property=get(a,id+12);
            if(property==0)*result=(int)get(a,player+0x5a0);
            else if(property==1){other=get(a,player+0x448);if(!other)return 0;
                *result=game_memory_read_u8(&a->memory,other+12)==0x23;
            }else if(property==2 && !get(a,0x5eff04) && player==get(a,0x5f0208)){
                index=get(a,0x604760);*result=index==5 || index==6;
            }
        }
    }else if(kind==SCRIPT_GATE_UP){
        index=get(a,id+4);if(index==UINT32_MAX)return !a->memory.fault;
        if(index>=get(a,0x5e147c))return 0;
        other=get(a,get(a,0x5e1480)+index*4);if(!other)return 0;
        *result=(int)get(a,other+0x24);
    }else{
        binding=get(a,id+4);if(!application_name_registry_binding(binding,&actor,&captured))return 0;
        alive=actor && name_generation(a,actor,&generation) && generation==captured;
        if(kind==SCRIPT_LIVED_DIED)*result=captured!=0 && !alive;
        else if(actor && !alive){
            uint8_t flags;if(!application_name_registry_read(binding+0x2c,&flags,1))return 0;
            *result=kind==SCRIPT_DIED_WELL?(flags&1)!=0:(flags&1)==0;
        }
    }
    return !a->memory.fault;
}
/*4776c0/478f70/479860/47ef90: actual shared player queries. Method IDs select
 *ordinary algorithms; no virtual guest call or register state enters here. */
int game_script_player_units_alive(GameApplication *a,GameAddress player,int *alive){
    GameAddress methods,node,control;uint32_t method,visits=0;
    if(!a || !player || !alive || a->memory.fault)return 0;
    *alive=0;methods=get(a,player);if(!methods)return 0;method=get(a,methods+0x54);
    if(a->memory.fault)return 0;
    switch(method){
    case 0x47ef90:if(get(a,player+0x468)){*alive=1;return !a->memory.fault;}break;
    case 0x479860:control=get(a,player+0x2e0);if(!control)return 0;
        if(get(a,control+0x1a4)&0x100u){*alive=1;return !a->memory.fault;}break;
    case 0x478f70:case 0x4776c0:break;
    default:return 0;
    }
    if((method!=0x4776c0 && get(a,player+0x294)) || get(a,player+0xc8)){
        *alive=1;return !a->memory.fault;
    }
    for(node=get(a,player+0x12c);node;node=get(a,node+0x48)){
        if(++visits>65536 || a->memory.fault)return 0;
        if(get(a,node+0x10)&0x80u){*alive=1;return !a->memory.fault;}
    }
    visits=0;
    for(node=get(a,player+0x64);node;node=get(a,node+0x48)){
        if(++visits>65536 || a->memory.fault)return 0;
        if(game_memory_read_u8(&a->memory,node+0xc)==0x45 && !get(a,node+0x100)){
            *alive=1;return !a->memory.fault;
        }
    }
    return !a->memory.fault;
}
static int spatial_player_capture(GameApplication *a,const char *token,uint32_t limit,GameAddress *player,uint32_t *index){
    uint32_t selector,mapped;*player=0;*index=UINT32_MAX;
    if(scalar_index(a,token,index))return 1;
    if(!token_integer(token,&selector) || selector>=6)return 0;
    mapped=get(a,0x5f01f0+selector*4);
    if(mapped<limit){*player=get(a,0x5f0210+mapped*4);if(*player && !get(a,*player+20))*player=0;}
    return !a->memory.fault;
}
static int spatial_point_parse(GameApplication *a,GameScriptReader *r,uint32_t *index,float point[3]){
    uint32_t values[3],i;int direction;GameAddress record;
    *index=UINT32_MAX;
    if(strcmp(r->token,"LOC") && strcmp(r->token,"VEC")){
        record=game_script_variable_find(a,r->token,1);if(!record || !a->scripts)return 0;
        for(i=0;i<a->scripts->vector.count;++i)if(get(a,a->scripts->vector.rows+i*4)==record){*index=i;break;}
        if(*index==UINT32_MAX)return 0;
        point[0]=point[1]=point[2]=0;return game_script_next(r);
    }
    direction=!strcmp(r->token,"VEC");if(!expect(r,"("))return 0;
    for(i=0;i<3;++i){unsigned component=i==1?2:i==2?1:0;
        if(!numeric(r,0,&values[component]) || (i<2 && !expect(r,",")))return 0;
    }
    if(!expect(r,")"))return 0;
    for(i=0;i<3;++i){float v;memcpy(&v,&values[i],4);point[i]=(float)((long double)v*scalar_float(a,0x573fac));}
    point[0]=direction?-point[0]:(float)((long double)scalar_float(a,0x573fb0)-point[0]);
    return !a->memory.fault && game_script_next(r);
}
static int spatial_leaf_create(GameApplication *a,GameScriptReader *r,GameAddress *out){
    uint32_t kind,index=UINT32_MAX,vector=UINT32_MAX,mode=0,citadel=UINT32_MAX,citadel_index=UINT32_MAX;
    GameAddress player=0,first=0,second=0,id;float point[3]={0},radius;int is_point=!strcmp(r->token,"CLOSE_PT"),is_citadel=!strncmp(r->token,"CITADEL_OWNED",13),is_player;
    if(is_citadel){mode=!strcmp(r->token,"CITADEL_OWNED_W_TOWER")?1:!strcmp(r->token,"CITADEL_OWNED_W_STRG_TOWER")?2:0;
        if(!game_script_next(r) || !spatial_player_capture(a,r->token,6,&player,&index) || !game_script_next(r))return 0;
        if(!strcmp(r->token,"ALL"))citadel=UINT32_MAX-22;
        else if(!strcmp(r->token,"ANY"))citadel=UINT32_MAX-23;
        else if(!scalar_index(a,r->token,&citadel_index) && !token_integer(r->token,&citadel))return 0;
        if(!game_script_next(r))return 0;
        id=script_node(SCRIPT_CITADEL,24);if(!id)return 0;
        put(a,id+4,citadel);put(a,id+8,citadel_index);put(a,id+12,mode);put(a,id+16,player);put(a,id+20,index);
    }else{
        if(!game_script_next(r))return 0;
        is_player=!strcmp(r->token,"HTANK");
        if(is_player){if(!game_script_next(r) || !spatial_player_capture(a,r->token,4,&player,&index))return 0;}
        else if(!game_script_name_define(a,r->token,0,&first) || !first)return 0;
        if(!game_script_next(r))return 0;
        if(is_point){if(!spatial_point_parse(a,r,&vector,point))return 0;}
        else if(!game_script_name_define(a,r->token,0,&second) || !second || !game_script_next(r))return 0;
        if(r->token[0]<'0' || r->token[0]>'9')return 0;
        radius=(float)((long double)strtod(r->token,NULL)*scalar_float(a,0x573fac));
        radius=(float)((long double)radius*radius);if(!game_script_next(r))return 0;
        kind=is_point?(is_player?SCRIPT_POINT_PLAYER:SCRIPT_POINT_OBJECT):(is_player?SCRIPT_CLOSE_PLAYER:SCRIPT_CLOSE_OBJECT);
        id=script_node(kind,is_point?(is_player?32:28):(is_player?20:16));if(!id)return 0;
        put(a,id+4,is_player?player:first);put(a,id+8,is_player?index:is_point?vector:second);
        if(is_point){GameAddress base=id+(is_player?16:12);
            if(is_player)put(a,id+12,vector);
            for(index=0;index<3;++index)scalar_float_put(a,base+index*4,point[index]);
            scalar_float_put(a,base+12,radius);
        }else {if(is_player)put(a,id+12,second);scalar_float_put(a,id+(is_player?16:12),radius);}
    }
    if(a->memory.fault){(void)scene_projection_storage_free(id);return 0;}*out=id;return 1;
}
static int spatial_binding_actor(GameApplication *a,GameAddress binding,GameAddress *actor){
    uint32_t captured,generation;*actor=0;
    if(!application_name_registry_binding(binding,actor,&captured))return 0;
    if(*actor && (!name_generation(a,*actor,&generation) || generation!=captured))*actor=0;
    return !a->memory.fault;
}
static int citadel_matches(GameApplication *a,GameAddress citadel,GameAddress player,uint32_t mode){
    GameAddress hull,tower,configuration;
    if(get(a,citadel+0x54)!=player)return 0;
    if(!mode)return !a->memory.fault;
    hull=get(a,citadel+0x18);if(!hull)return 0;tower=get(a,hull);if(!tower)return 0;
    if(mode==1)return !a->memory.fault;
    configuration=get(a,tower+0x48);return configuration && get(a,configuration+0x2b4)==30 && !a->memory.fault;
}
static int spatial_leaf_evaluate(GameApplication *a,GameAddress id,int *result){
    uint32_t kind=get(a,id),index,i,mode,visits=0;GameAddress player,first=0,second=0,base,record,citadel;long double x,y,z,distance;float point[3];int is_player,is_point;
    *result=0;
    if(kind==SCRIPT_CITADEL){
        /*The selector pair sits at16/20, the same bounded player record view. */
        player=leaf_player(a,id+12,6);if(!player)return !a->memory.fault;
        index=get(a,id+4);mode=get(a,id+12);if(mode>2)return 0;
        if(index==UINT32_MAX-22){
            int32_t count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);*result=1;
            for(i=0;i<(uint32_t)(count>0?count:0);++i){citadel=get(a,get(a,0x639a70)+i*4);
                if(!citadel || !citadel_matches(a,citadel,player,mode)){*result=0;break;}}
        }else if(index==UINT32_MAX-23){
            if(game_memory_read_u16(&a->memory,player+8)==5)return !a->memory.fault;
            for(citadel=get(a,player+0x294);citadel;citadel=get(a,citadel+0x64)){
                if(++visits>65536 || a->memory.fault)return 0;
                /*The player's own intrusive list already establishes ownership. */
                if(!mode){*result=1;break;}
                record=get(a,citadel+0x18);if(record && get(a,record)){
                    if(mode==1){*result=1;break;}
                    record=get(a,get(a,record)+0x48);if(record && get(a,record+0x2b4)==30){*result=1;break;}
                }
            }
        }else{
            if(index==UINT32_MAX){i=get(a,id+8);if(i>=get(a,0x605884))return 0;record=scalar_record(a,i);index=get(a,record+32);}
            if((int32_t)index<0 || (int32_t)index>=(int16_t)game_memory_read_u16(&a->memory,0x639a74))return !a->memory.fault;
            citadel=get(a,get(a,0x639a70)+index*4);if(!citadel)return 0;*result=citadel_matches(a,citadel,player,mode);
        }
        return !a->memory.fault;
    }
    is_player=kind==SCRIPT_CLOSE_PLAYER || kind==SCRIPT_POINT_PLAYER;is_point=kind==SCRIPT_POINT_OBJECT || kind==SCRIPT_POINT_PLAYER;
    if(is_player){player=leaf_player(a,id,4);if(!player || !get(a,player+20))return !a->memory.fault;
        first=get(a,player+0x3bc);if(!first)return 0;if((int32_t)get(a,first+0x58)<=0)return !a->memory.fault;
    }else if(!spatial_binding_actor(a,get(a,id+4),&first))return 0;
    if(!first)return !a->memory.fault;
    if(is_point){
        index=get(a,id+(is_player?12:8));base=id+(is_player?16:12);
        if(index!=UINT32_MAX){if(index>=get(a,0x605888))return 0;record=get(a,get(a,0x60586c)+index*4);if(!record)return 0;
            for(i=0;i<3;++i)put(a,base+i*4,get(a,record+32+i*4));}
        for(i=0;i<3;++i)point[i]=scalar_float(a,base+i*4);
    }else{
        if(!spatial_binding_actor(a,get(a,id+(is_player?12:8)),&second))return 0;
        if(!second)return !a->memory.fault;
        for(i=0;i<3;++i)point[i]=scalar_float(a,second+24+i*4);
        base=id+(is_player?4:0);
    }
    x=(long double)scalar_float(a,first+24)-point[0];y=(long double)scalar_float(a,first+28)-point[1];z=(long double)scalar_float(a,first+32)-point[2];
    distance=is_point?(x*x+z*z)+(is_player?0:y*y):(x*x+y*y)+z*z;
    /*Original fcomp accepts equality and unordered comparisons. */
    *result=!(distance>(long double)scalar_float(a,base+12));return !a->memory.fault;
}
#define SCRIPT_REFRESH_WEAPONS 0x574c10u
#define SCRIPT_PLAY_SOUND 0x574c28u
#define SCRIPT_BILLBOARD 0x574c20u
#define SCRIPT_NAV_POINT 0x574c58u
#define SCRIPT_GOTO_MODE 0x574c80u
#define SCRIPT_CREATE_UNIT 0x574bc8u
#define SCRIPT_CREATE_PICKUP 0x574be8u
#define SCRIPT_KILLER_SAT 0x574c90u
#define SCRIPT_UPDATE_NP 0x574c60u
#define SCRIPT_DESTROY_UNIT 0x574bf0u
#define SCRIPT_REMOVE_UNIT 0x574bf8u
#define SCRIPT_MOVEMENT_PATH 0x574c30u
#define SCRIPT_COUNT_OBJECTS 0x574c70u
#define SCRIPT_CURRENT_WEAPON 0x574c78u
#define SCRIPT_END_MISSION 0x574c88u
#define SCRIPT_CHANGE_VECTOR 0x574ca8u
#define SCRIPT_LOCATION_PLAYER 0x574c00u
#define SCRIPT_LOCATION_OBJECT 0x574c08u
#define SCRIPT_SET_GATE 0x574c48u
#define SCRIPT_SET_TELEPORTER 0x574c38u
#define SCRIPT_TELEPORTER_OWNER 0x574c40u
static int statement_leaf_kind(uint32_t kind){return kind==SCRIPT_UPDATE_NP || kind==SCRIPT_DESTROY_UNIT || kind==SCRIPT_REMOVE_UNIT || kind==SCRIPT_KILLER_SAT || kind==SCRIPT_CREATE_PICKUP || kind==SCRIPT_COUNT_OBJECTS || kind==SCRIPT_CURRENT_WEAPON || kind==SCRIPT_END_MISSION || kind==SCRIPT_MOVEMENT_PATH || kind==SCRIPT_CREATE_UNIT || kind==SCRIPT_GOTO_MODE || kind==SCRIPT_NAV_POINT || kind==SCRIPT_BILLBOARD || kind==SCRIPT_SET_TELEPORTER || kind==SCRIPT_TELEPORTER_OWNER || kind==SCRIPT_SET_GATE || kind==SCRIPT_REFRESH_WEAPONS || kind==SCRIPT_PLAY_SOUND || kind==SCRIPT_CHANGE_VECTOR || kind==SCRIPT_LOCATION_PLAYER || kind==SCRIPT_LOCATION_OBJECT;}
/*4777f0/4784f0/479080: count through the actual player catalogs and lists.
 *The counters are existing authoritative fields, not another unit inventory. */
static int script_unit_count(GameApplication *a,GameAddress player,uint8_t type,uint32_t *out){
    GameAddress method=get(a,get(a,player)+0x74),head=0,node;unsigned visits=0;
    *out=0;
    if(method==0x479080){
        static const uint16_t fields[]={0x204,0x208,0x20c,0x210,0x218,0x21c,0x220,0x224,0x228,0x214};
        if(type==0x23){*out=get(a,player+0x200);return !a->memory.fault;}
        if(type>=0x80 && type<=0x89){*out=get(a,player+fields[type-0x80]);return !a->memory.fault;}
    }else if(method!=0x4777f0 && method!=0x4784f0)return 0;
    if(method==0x4784f0 && (type==0x60 || (type>=0x62 && type<=0x64)))head=player+0x12c;
    else if(type==0x21 || type==0x22 || (type>=0x24 && type<=0x29) || type==0x40)head=player+0xc8;
    else if(type>=0x77 && type<=0x7a)head=player+0x12c;
    if(!head)return !a->memory.fault;
    for(node=get(a,head);node && !a->memory.fault;node=get(a,node+0x48)){
        uint8_t actual=game_memory_read_u8(&a->memory,node+12);
        if(++visits>65536u)return 0;
        if(type>=0x77 && type<=0x7a){if(actual==0x77 && type-get(a,node+0xa0)==0x77u)++*out;}
        else if(actual==type)++*out;
    }
    return !a->memory.fault;
}
extern int game_level_scene_command_publish(GameApplication *,GameAddress,RuntimeNode *);
/*53c260's existing native path algorithm owns its pooled chain. Decode only
 *this assignment's bounded point view, then retain the original height stores;
 *there is no second retained route or frame-time synchronization. */
static int script_movement_execute(GameApplication *a,GameAddress node,int *result){
    GameAddress object=0,route=get(a,node+12),points;PresentationLifecycle *state;
    float (*view)[3];uint32_t count,mode,i,j,method;int ok;
    if(!spatial_binding_actor(a,get(a,node+8),&object))return 0;
    if(!object){*result=0;return !a->memory.fault;}
    method=get(a,get(a,object)+0x54);
    if(!route || (method!=0x53c260 && method!=0x55cc20))return 0;
    state=presentation_instance_lifecycle(object);if(!state)return 0;
    points=get(a,route);count=get(a,route+4);mode=get(a,route+8);
    if(a->memory.fault || count>65536u || (method==0x53c260 && mode>3u))return 0;
    view=malloc((count?count:1u)*sizeof(*view));if(!view)return 0;
    for(i=0;i<count;i++)for(j=0;j<3;j++)view[i][j]=scalar_float(a,points+i*12+j*4);
    ok=!a->memory.fault && (method==0x55cc20?
        presentation_command_aircraft_path(&a->memory,state,view,count):
        presentation_command_path(&a->memory,state,view,count,mode));
    for(i=0;i<count;i++)for(j=0;j<3;j++)scalar_float_put(a,points+i*12+j*4,view[i][j]);
    free(view);put(a,object+0xdc,state->runtime.packet_chain);
    return ok && !a->memory.fault && game_level_scene_command_publish(a,object,runtime_node_pool_find(state->runtime.packet_chain));
}
/*48cb80: language names select native factory kinds, independently of host
 *addresses. These are the original aliases, including the four scenery kinds. */
static int script_unit_kind(const char *name,uint8_t *kind){
    static const struct {const char *name;uint8_t kind;} kinds[]={
        {"CONVOY_TRUCK",0x29},{"AI_TANK",0x21},{"AAV",0x27},{"SOLDIER",0x40},
        {"TURRET",0x22},{"LIGHT_TURRET",0x25},{"HEAVY_TURRET",0x28},{"SAMS",0x24},{"BOMBER",0x26},
        {"SLDR_GNRTR_BLDG",0x60},{"AITANK_GNRTR_BLDG",0x63},{"AAV_GNRTR_BLDG",0x62},{"BOMBER_GNRTR_BLDG",0x64},
        {"SLDR_UPGRADE_BLDG",0x80},{"TANK_UPGRADE_BLDG",0x81},{"AAV_UPGRADE_BLDG",0x82},{"BOMBER_UPGRADE_BLDG",0x83},
        {"INFANTRY_BOOSTER",0x84},{"TANK_BOOSTER",0x85},{"AAV_BOOSTER",0x86},{"BOMBER_BOOSTER",0x87},
        {"CITADEL_TURRET",0x23},{"ENERGY_BLDG",0x88},{"KSAT_DISH",0x89},
        {"NP_BLDG_GENERATOR",0x77},{"NP_BLDG_NEEDLE",0x78},{"NP_BLDG_TECHCENTER",0x79},{"NP_BLDG_POWERPLANT",0x7a}};
    unsigned i;for(i=0;i<sizeof(kinds)/sizeof(kinds[0]);i++)if(!strcmp(name,kinds[i].name)){*kind=kinds[i].kind;return 1;}
    return 0;
}
static unsigned script_spawn_cell(float value){return (scalar_truncate(value)>>9)&255u;}
static int script_spawn_blocked(GameApplication *a,unsigned x,unsigned z){
    if(x>=256u || z>=256u)return 1;
    return (game_memory_read_u8(&a->memory,0x610520u+z*32u+x/8u)&(1u<<(x&7u)))!=0;
}
static int script_spawn_neighbors(GameApplication *a,unsigned x,unsigned z){
    return script_spawn_blocked(a,x-1u,z) && script_spawn_blocked(a,x+1u,z) &&
        script_spawn_blocked(a,x,z-1u) && script_spawn_blocked(a,x,z+1u);
}
static int script_spawn_return(GameApplication *a,GameAddress object){
    GameAddress head=get(a,object+0x50);
    if(!head || a->memory.fault)return 0;
    game_list_insert_after(&a->memory,object,head,4,8);return !a->memory.fault;
}
static int script_spawn_publish(GameApplication *a,GameAddress player,GameAddress object){
    uint32_t offset=state_helper_flagged_member_offset(get(a,object+16));
    if(a->memory.fault || (level_player_slots_owns_player(player) &&
       !level_player_slots_attach_scene_member(player,object,offset)))return 0;
    game_list_insert_after(&a->memory,object,player+offset,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(object);return 0;}return 1;
}
/*4dd790: one pending factory object, the existing collision/occupancy planes,
 *and the original bounded nearby search. Successful arrival construction
 *transfers the payload to the player; the caller publishes its arrival effect. */
static int script_spawn_execute(GameApplication *a,GameAddress node,int *result){
    GameAddress player,unit=0,arrival=0,binding,record;uint32_t index,slot,sequence,id,bits;uint8_t kind,type;
    float requested[3],position[3];char name[32];unsigned i,attempt=0,x,z;int clear=0,eligible,ground;
    *result=1;index=get(a,node+16);
    if(index!=UINT32_MAX){
        if(index>=get(a,0x605888) || !(record=get(a,get(a,0x60586c)+index*4)))return 0;
        for(i=0;i<3;i++)put(a,node+20+i*4,get(a,record+32+i*4));
    }
    for(i=0;i<3;i++)requested[i]=scalar_float(a,node+20+i*4);
    /*Original raw signed-word bounds also reject NaNs and signed zero. */
    for(i=0;i<3;i+=2){memcpy(&bits,requested+i,4);if(!bits || bits>=0x48000000u)return !a->memory.fault;}
    for(i=0;i<sizeof(name);i++){name[i]=(char)game_memory_read_u8(&a->memory,node+36+i);if(!name[i])break;}
    if(i==sizeof(name) || a->memory.fault)return 0;
    binding=application_name_registry_find(name);
    if(binding){GameAddress prior=0;if(!spatial_binding_actor(a,binding,&prior))return 0;if(prior){*result=0;return 1;}}
    player=leaf_player(a,node+4,6);if(!player)return !a->memory.fault;
    kind=game_memory_read_u8(&a->memory,node+32);
    if(kind>=0x77 && kind<0x7b){slot=kind-0x77;kind=0x77;}
    else if(!game_level_player_catalog_value(a,player,kind,3,0,&slot))return 0;
    unit=game_level_factory_take(a,kind,(uint16_t)slot);if(!unit)return 0;
    type=game_memory_read_u8(&a->memory,unit+12);ground=type==0x21 || type==0x40 || type==0x29;
    memcpy(position,requested,sizeof(position));
    if(type!=0x26 && type!=0x27){
        position[1]=(float)(gameplay_geometry_terrain_height(&a->memory,requested[0],requested[2])+scalar_float(a,unit+0x30));
        scalar_float_put(a,node+24,position[1]);requested[1]=position[1];
        x=script_spawn_cell(requested[0]);z=script_spawn_cell(requested[2]);
        eligible=!script_spawn_blocked(a,x,z);
        /*Retail uses OR initially and AND on retries. Retry neighbors are
         *deliberately sampled around the original request, not the candidate. */
        if(ground)eligible|=!script_spawn_neighbors(a,x,z);
    }else eligible=1;
    if(eligible && !gameplay_geometry_position_clear(&a->memory,unit,position,&clear))goto failed;
    while(!clear){
        uint32_t minimum_word,range_word,cursor,distance;int32_t minimum,range;float nearby[2];long double sample;
        ++attempt;
        minimum_word=scalar_truncate((long double)scalar_float(a,unit+0x38)*scalar_float(a,0x574bd8));
        memcpy(&minimum,&minimum_word,4);range_word=3000u-minimum_word;memcpy(&range,&range_word,4);
        cursor=get(a,0x6308c0);if(cursor==1024u)cursor=0;if(cursor>=1024u)goto failed;
        put(a,0x6308c0,cursor+1u);sample=(long double)range*scalar_float(a,0x6308c8+cursor*4);
        distance=minimum_word+scalar_truncate(sample);memcpy(&minimum,&distance,4);
        nearby[0]=requested[0];nearby[1]=requested[2];
        (void)gameplay_geometry_nearby_location(&a->memory,requested[0],requested[2],(float)minimum,nearby);
        position[0]=nearby[0];position[2]=nearby[1];
        if(type==0x26 || type==0x27){
            long double height=gameplay_geometry_terrain_height(&a->memory,position[0],position[2])+scalar_float(a,unit+0x30);
            position[1]=!(height>=requested[1])?requested[1]:(float)height;eligible=1;
        }else{
            position[1]=(float)(gameplay_geometry_terrain_height(&a->memory,position[0],position[2])+scalar_float(a,unit+0x30));
            x=script_spawn_cell(position[0]);z=script_spawn_cell(position[2]);eligible=!script_spawn_blocked(a,x,z);
            if(ground)eligible&=!script_spawn_neighbors(a,script_spawn_cell(requested[0]),script_spawn_cell(requested[2]));
        }
        clear=0;if(a->memory.fault || (eligible && !gameplay_geometry_position_clear(&a->memory,unit,position,&clear)))goto failed;
        /*Even an accepted tenth candidate returns to the ready list. */
        if(attempt==10u){*result=0;return script_spawn_return(a,unit);}
    }
    arrival=game_level_factory_take(a,0x45,0);if(!arrival)goto failed;
    sequence=get(a,player+0x18);put(a,player+0x18,sequence+1);
    id=(sequence<<6)|game_memory_read_u16(&a->memory,player+8);
    if(!game_level_arrival_produce_requested(a,arrival,id,position,unit,0,game_memory_read_u16(&a->memory,node+34)))goto failed;
    /*The registry borrows the payload's live generation, never owns its pages. */
    if(!application_name_registry_is_marker(name) &&
       !application_name_registry_register(name,unit,get(a,unit+20),&binding))goto failed;
    if(binding){
        put(a,binding+32,unit);put(a,binding+36,get(a,unit+20));
        game_memory_write_u8(&a->memory,binding+44,0);
    }
    if(!script_spawn_publish(a,player,arrival))goto failed;
    return !a->memory.fault;
failed:
    if(arrival)(void)game_level_factory_destroy(a,arrival);
    if(unit)(void)game_level_factory_destroy(a,unit);
    return 0;
}
static int statement_player_capture(GameApplication *a,const char *token,uint32_t limit,GameAddress *player,uint32_t *index){
    uint32_t selector;
    if(scalar_index(a,token,index)) {*player=0;return 1;}
    if(token_integer(token,&selector) && selector<6)return spatial_player_capture(a,token,limit,player,index);
    *index=UINT32_MAX;*player=get(a,0x5f0208);return !a->memory.fault;
}
/*4de8d0/4decd0/4e03f0 and their actual consumers. Child quantities, audio
 *voices and vector variables retain their existing sole owners. */
int game_script_statement_leaf_create(GameApplication *a,GameScriptReader *r,GameAddress *out){
    static const char *operations[]={"ADD","SUB","MUL","DIV","SET"};
    GameAddress id,player=0,record;uint32_t kind,index=UINT32_MAX,source=UINT32_MAX,operation=0,sound=0,i,destination=UINT32_MAX;float point[3]={0};
    if(!a || !r || !out || a->memory.fault)return 0;
    *out=0;
    if(!strcmp(r->token,"UPDATE_NP")){
        GameAddress binding=0;
        if(!game_script_next(r) || !game_script_name_define(a,r->token,0,&binding) || !binding ||
           !game_script_next(r) || !scalar_index(a,r->token,&destination) || !game_script_next(r))return 0;
        id=script_node(SCRIPT_UPDATE_NP,16);if(!id)return 0;
        put(a,id+8,destination);put(a,id+12,binding);*out=id;return !a->memory.fault;
    }
    if(!strcmp(r->token,"DESTROY_UNIT") || !strcmp(r->token,"REMOVE_UNIT")){
        char label[32];GameAddress binding=0;
        kind=!strcmp(r->token,"DESTROY_UNIT")?SCRIPT_DESTROY_UNIT:SCRIPT_REMOVE_UNIT;
        if(!game_script_next(r) || !r->token[0] || strlen(r->token)>=sizeof(label))return 0;
        strcpy(label,r->token);
        if(!game_script_name_define(a,label,0,&binding) || !binding || !game_script_next(r) ||
           (kind==SCRIPT_REMOVE_UNIT && !game_level_factory_template(a,0x45,0)))return 0;
        id=script_node(kind,40);if(!id)return 0;
        if(!game_memory_write(&a->memory,id+8,label,strlen(label)+1)){
            (void)scene_projection_storage_free(id);return 0;}
        *out=id;return 1;
    }
    if(!strcmp(r->token,"KILLER_SAT")){
        /*4dfd10: retain the captured player or dynamic scalar selector and
         *the vector index in the original bounded32-byte command layout. */
        if(!game_script_next(r) || !statement_player_capture(a,r->token,6,&player,&index) ||
           !game_script_next(r) || !spatial_point_parse(a,r,&source,point) ||
           !game_level_factory_template(a,11,0))return 0;
        id=script_node(SCRIPT_KILLER_SAT,32);if(!id)return 0;
        for(i=0;i<3;i++)scalar_float_put(a,id+8+i*4,point[i]);
        put(a,id+20,source);put(a,id+24,player);put(a,id+28,index);
        *out=id;return !a->memory.fault;
    }
    if(!strcmp(r->token,"CREATE_FIXED_PU") || !strcmp(r->token,"CREATE_FALLING_PU")){
        static const char *names[]={"PU_LIVES","PU_HEALTH","PU_MONEY","PU_WEPN_BOOST",
            "PU_SPECIAL_WEAPON_BOOST","PU_SOLDIERS","PU_TANKS","PU_AAVS","PU_BOMBERS"};
        int falling=!strcmp(r->token,"CREATE_FALLING_PU");char label[32],*end;
        uint32_t variant=UINT32_MAX;float radii[2]={0};ScriptScalar quantity={0};GameAddress binding=0;
        if(!game_script_next(r) || !r->token[0] || strlen(r->token)>=32)return 0;
        strcpy(label,r->token);
        if(!game_script_next(r) || !statement_player_capture(a,r->token,4,&player,&index) || !game_script_next(r))return 0;
        if(falling){
            for(i=0;i<2;i++){
                float v=strtof(r->token,&end);
                if(end==r->token || *end || !isfinite(v))return 0;
                radii[i]=(float)((long double)v*scalar_float(a,0x573fac));
                if(!game_script_next(r))return 0;
            }
            if(!(radii[1]>radii[0]))return 0;
        }else if(!spatial_point_parse(a,r,&source,point))return 0;
        if(strcmp(r->token,"PU_RANDOM")){
            for(variant=0;variant<9;variant++)if(!strcmp(r->token,names[variant]))break;
            if(variant==9 || !game_level_spawn_variant_load(a,variant))return 0;
        }
        if(!game_script_next(r))return 0;
        if((r->token[0]>='0' && r->token[0]<='9') || r->token[0]=='-'){
            quantity.type=18;quantity.arg=(uint32_t)(uint64_t)strtoll(r->token,&end,10);
            if(*end)return 0;
        }else if(!scalar_parse(a,r,&quantity))return 0;
        if(!game_script_next(r) || !game_script_name_define(a,label,0,&binding) || !scalar_bind(a,&quantity))goto pickup_failed;
        id=script_node(SCRIPT_CREATE_PICKUP,0x5c);if(!id)goto pickup_failed;
        put(a,id+8,player);put(a,id+12,index);put(a,id+16,!falling);put(a,id+20,source);
        for(i=0;i<3;i++)scalar_float_put(a,id+24+i*4,point[i]);
        for(i=0;i<2;i++)scalar_float_put(a,id+36+i*4,radii[i]);
        put(a,id+44,variant);put(a,id+48,quantity.type);put(a,id+52,quantity.arg);put(a,id+88,quantity.binding);
        if(!game_memory_write(&a->memory,id+56,label,strlen(label)+1) || a->memory.fault){
            (void)scene_projection_storage_free(id);goto pickup_failed;}
        *out=id;return 1;
pickup_failed:
        if(quantity.binding)(void)scene_projection_storage_free(quantity.binding);return 0;
    }
    if(!strcmp(r->token,"COUNT_OBJECTS") || !strcmp(r->token,"CURRENT_WEAPON")){
        uint8_t subject;uint32_t selector;int count=!strcmp(r->token,"COUNT_OBJECTS");
        if(!game_script_next(r))return 0;
        if(scalar_index(a,r->token,&selector) || (token_integer(r->token,&selector) && selector<(count?6u:4u))){
            if(!statement_player_capture(a,r->token,count?6u:4u,&player,&index) || !game_script_next(r))return 0;
        }else player=get(a,0x5f0208);
        if(count){if(!script_unit_kind(r->token,&subject))return 0;}
        else if(!strcmp(r->token,"RIGHT"))subject=1;
        else if(!strcmp(r->token,"LEFT"))subject=0;
        else return 0;
        if(!game_script_next(r) || !scalar_index(a,r->token,&destination) || !game_script_next(r))return 0;
        id=script_node(count?SCRIPT_COUNT_OBJECTS:SCRIPT_CURRENT_WEAPON,24);if(!id)return 0;
        put(a,id+8,destination);put(a,id+12,player);put(a,id+16,index);game_memory_write_u8(&a->memory,id+20,subject);
        *out=id;return !a->memory.fault;
    }
    if(!strcmp(r->token,"END_MISSION")){
        uint32_t outcome,seconds,product,ticks;size_t length;char text[512];
        if(!game_script_next(r) || !statement_player_capture(a,r->token,4,&player,&index) ||
           !game_script_next(r) || !token_integer(r->token,&outcome) || !game_script_next(r) ||
           !token_integer(r->token,&seconds) || !game_script_next(r))return 0;
        length=strlen(r->token);if(length<2 || r->token[0]!='{' || r->token[length-1]!='}' || length-2>=sizeof(text))return 0;
        memcpy(text,r->token+1,length-2);text[length-2]=0;if(!game_script_next(r))return 0;
        product=seconds*1000u;ticks=(product>>5)|((product&0x80000000u)?0xf8000000u:0u);
        id=script_node(SCRIPT_END_MISSION,0x218);if(!id)return 0;
        put(a,id+8,player);put(a,id+12,index);put(a,id+16,outcome);put(a,id+20,ticks);
        if(!game_memory_write(&a->memory,id+24,text,length-1)){(void)scene_projection_storage_free(id);return 0;}
        *out=id;return !a->memory.fault;
    }
    if(!strcmp(r->token,"SET_MOVEMENT_PATH")){
        GameAddress binding=0,route=0;uint32_t count;
        if(!game_script_next(r) || !game_script_name_define(a,r->token,0,&binding) || !binding || !game_script_next(r))return 0;
        count=get(a,0x60589c);if(count>65536)return 0;
        for(i=0;i<count;i++){
            char label[256];unsigned n;GameAddress candidate=get(a,get(a,0x605880)+i*4),at=get(a,candidate+12);
            if(!candidate)return 0;
            for(n=0;n<sizeof(label);n++){label[n]=(char)game_memory_read_u8(&a->memory,at+n);if(!label[n])break;}
            if(n==sizeof(label) || a->memory.fault)return 0;
            for(n=0;n<sizeof(label);n++){
                unsigned char left=(unsigned char)label[n],right=(unsigned char)r->token[n];
                if(left>='A' && left<='Z')left+='a'-'A';
                if(right>='A' && right<='Z')right+='a'-'A';
                if(left!=right)break;
                if(!left){route=candidate;break;}
            }
            if(route)break;
        }
        if(!route || !game_script_next(r))return 0;
        id=script_node(SCRIPT_MOVEMENT_PATH,16);if(!id)return 0;
        put(a,id+8,binding);put(a,id+12,route);*out=id;return !a->memory.fault;
    }
    if(!strcmp(r->token,"CREATE_UNIT")){
        char labels[3][32],*end;uint8_t unit_kind;uint32_t slot;uint16_t yaw;double degrees;GameAddress binding;
        if(!game_script_next(r) || !r->token[0] || strlen(r->token)>=32)return 0;
        strcpy(labels[0],r->token);
        if(!game_script_next(r) || !statement_player_capture(a,r->token,6,&player,&index) ||
           !game_script_next(r) || !script_unit_kind(r->token,&unit_kind) ||
           !game_script_next(r) || !spatial_point_parse(a,r,&source,point))return 0;
        if(strcmp(r->token,"YAW") || !expect(r,"(") || !game_script_next(r))return 0;
        degrees=strtod(r->token,&end);if(end==r->token || *end || !expect(r,")"))return 0;
        yaw=(uint16_t)(scalar_truncate((long double)degrees*scalar_float(a,0x573fa8))&2047u);
        for(i=1;i<3;i++){
            if(!game_script_next(r) || strlen(r->token)>=32)return 0;strcpy(labels[i],r->token);
        }
        if(!game_script_next(r))return 0;
        for(i=0;i<3;i++)if(!game_script_name_define(a,labels[i],0,&binding))return 0;
        /*Generator and fixed scenery templates must exist before gameplay
         *for the same reason as the original constructor's442a10 calls. */
        if(unit_kind>=0x77 && unit_kind<0x7b){
            if(!game_level_factory_template(a,0x77,(uint16_t)(unit_kind-0x77)))return 0;
        }else if(unit_kind==0x60 || (unit_kind>=0x62 && unit_kind<=0x64)){
            if(!game_level_player_catalog_value(a,get(a,0x5f0224),unit_kind,3,0,&slot) ||
               !game_level_factory_template(a,unit_kind,(uint16_t)slot))return 0;
        }
        id=script_node(SCRIPT_CREATE_UNIT,0x84);if(!id)return 0;
        put(a,id+8,player);put(a,id+12,index);put(a,id+16,source);
        for(i=0;i<3;i++)scalar_float_put(a,id+20+i*4,point[i]);
        game_memory_write_u8(&a->memory,id+32,unit_kind);game_memory_write_u16(&a->memory,id+34,yaw);
        for(i=0;i<3;i++)if(!game_memory_write(&a->memory,id+36+i*32,labels[i],strlen(labels[i])+1)){
            (void)scene_projection_storage_free(id);return 0;
        }
        *out=id;return !a->memory.fault;
    }
    if(!strcmp(r->token,"GOTO_OVERHEAD") || !strcmp(r->token,"GOTO_CITADEL") || !strcmp(r->token,"GOTO_PTRI")){
        uint32_t mode=!strcmp(r->token,"GOTO_OVERHEAD")?0u:!strcmp(r->token,"GOTO_CITADEL")?1u:2u;
        if(!game_script_next(r) || !statement_player_capture(a,r->token,4,&player,&index) || !game_script_next(r))return 0;
        id=script_node(SCRIPT_GOTO_MODE,20);if(!id)return 0;
        put(a,id+8,mode);put(a,id+12,player);put(a,id+16,index);*out=id;return !a->memory.fault;
    }
    if(!strcmp(r->token,"SET_NAV_POINT")){
        uint32_t enabled;
        if(!game_script_next(r) || !spatial_point_parse(a,r,&source,point))return 0;
        if(!strcmp(r->token,"ON"))enabled=1;
        else if(!strcmp(r->token,"OFF"))enabled=0;
        else return 0;
        if(!game_script_next(r))return 0;
        id=script_node(SCRIPT_NAV_POINT,28);if(!id)return 0;
        put(a,id+8,source);for(i=0;i<3;i++)scalar_float_put(a,id+12+i*4,point[i]);
        put(a,id+24,enabled);*out=id;return !a->memory.fault;
    }
    if(!strcmp(r->token,"SHOW_BILLBOARD")){
        uint32_t selector,duration=4;char text[256];size_t length;
        if(!game_script_next(r))return 0;
        if(scalar_index(a,r->token,&selector) || (token_integer(r->token,&selector) && selector<6u)){
            if(!statement_player_capture(a,r->token,6,&player,&index) || !game_script_next(r))return 0;
        }else player=get(a,0x5f0208);
        length=strlen(r->token);
        if(length<2 || r->token[0]!='{' || r->token[length-1]!='}' || length-2>=sizeof(text))return 0;
        memcpy(text,r->token+1,length-2);text[length-2]=0;
        if(!game_script_next(r))return 0;
        if((r->token[0]>='0' && r->token[0]<='9') || r->token[0]=='-'){
            duration=(uint32_t)(uint64_t)strtoll(r->token,NULL,10);
            if(!game_script_next(r))return 0;
        }
        id=script_node(SCRIPT_BILLBOARD,0x114);if(!id)return 0;
        if(!game_memory_write(&a->memory,id+8,text,length-1)){
            (void)scene_projection_storage_free(id);return 0;
        }
        put(a,id+0x108,player);put(a,id+0x10c,index);put(a,id+0x110,duration);
        *out=id;return !a->memory.fault;
    }
    /*4887e0/488900 and4def90/4df0c0: names borrow the existing generation
     *binding; literal owners are captured, scalar-variable owners stay dynamic. */
    if(!strcmp(r->token,"SET_TELEPORTER") || !strcmp(r->token,"SET_TPORT_OWNER")){
        GameAddress binding=0,captured=0;uint32_t argument=UINT32_MAX,literal=0;int owner=!strcmp(r->token,"SET_TPORT_OWNER");
        if(!game_script_next(r) || strlen(r->token)>=32 || !game_script_name_define(a,r->token,0,&binding) || !binding || !game_script_next(r))return 0;
        if(!owner){if(!token_integer(r->token,&literal))return 0;}
        else if(!strcmp(r->token,"-1"))captured=get(a,0x5f0208);
        else if(!scalar_index(a,r->token,&argument)){
            if(!token_integer(r->token,&literal) || literal>=6)return 0;
            captured=selected_player(a,literal);
            /*Retail leaves its dynamic index uninitialized for an unavailable
             *literal player. Native execution returns false instead. */
        }
        if(!game_script_next(r))return 0;
        id=script_node(owner?SCRIPT_TELEPORTER_OWNER:SCRIPT_SET_TELEPORTER,owner?20:16);if(!id)return 0;
        put(a,id+8,binding);put(a,id+12,owner?captured:literal);if(owner)put(a,id+16,argument);
        if(a->memory.fault){(void)scene_projection_storage_free(id);return 0;}*out=id;return 1;
    }
    /*488a70/4df170: retain the scalar's ordinary binding owner and the last
     *case-insensitive gate-name match, including the absent-name sentinel. */
    if(!strcmp(r->token,"SET_GATE")){
        ScriptScalar value={0};uint32_t count,j,match=UINT32_MAX;GameAddress node=0;
        if(!game_script_next(r) || strlen(r->token)>=32)return 0;
        count=get(a,0x5e147c);if(count>65536)return 0;
        for(i=0;i<count;i++){
            GameAddress gate=get(a,get(a,0x5e1480)+i*4);if(!gate)return 0;
            for(j=0;j<32;j++){
                unsigned char c=game_memory_read_u8(&a->memory,gate+j),t=(unsigned char)r->token[j];
                if(c>='A' && c<='Z')c+='a'-'A';
                if(t>='A' && t<='Z')t+='a'-'A';
                if(c!=t || a->memory.fault)break;
                if(!c){match=i;break;}
            }
            if(a->memory.fault)return 0;
        }
        if(!game_script_next(r))return 0;
        if((r->token[0]>='0' && r->token[0]<='9') || r->token[0]=='-'){
            value.type=18;value.arg=(uint32_t)(uint64_t)strtoll(r->token,NULL,10);
        }else if(!scalar_parse(a,r,&value))return 0;
        if(!game_script_next(r) || !scalar_bind(a,&value))goto gate_failed;
        node=script_node(SCRIPT_SET_GATE,24);if(!node)goto gate_failed;
        put(a,node+8,match);put(a,node+12,value.type);put(a,node+16,value.arg);put(a,node+20,value.binding);
        if(a->memory.fault)goto gate_failed;
        *out=node;return 1;
gate_failed:
        if(value.binding)(void)scene_projection_storage_free(value.binding);
        if(node)(void)scene_projection_storage_free(node);return 0;
    }
    if(!strcmp(r->token,"REFRESH_WEAPONS"))kind=SCRIPT_REFRESH_WEAPONS;
    else if(!strcmp(r->token,"PLAY_SOUND"))kind=SCRIPT_PLAY_SOUND;
    else if(!strcmp(r->token,"CHANGE_LOC_VBL"))kind=SCRIPT_CHANGE_VECTOR;
    else if(!strcmp(r->token,"GET_LOCATION")){
        GameAddress binding=0;float radius=0;int is_player;
        if(!game_script_next(r))return 0;
        is_player=!strcmp(r->token,"HTANK");
        if(is_player){if(!game_script_next(r) || !spatial_player_capture(a,r->token,4,&player,&index))return 0;}
        else if(!game_script_name_define(a,r->token,0,&binding) || !binding)return 0;
        if(!game_script_next(r))return 0;
        record=game_script_variable_find(a,r->token,1);if(!record || !a->scripts)return 0;
        for(i=0;i<a->scripts->vector.count;++i)if(get(a,a->scripts->vector.rows+i*4)==record){destination=i;break;}
        if(destination==UINT32_MAX || !game_script_next(r))return 0;
        /*487adb: the radius is optional. Preserve the next statement token
         *when absent; consecutive GET_LOCATION commands share no arguments. */
        if((r->token[0]>='0' && r->token[0]<='9') || r->token[0]=='-'){
            radius=(float)((long double)strtod(r->token,NULL)*scalar_float(a,0x573fac));
            if(!game_script_next(r))return 0;
        }
        id=script_node(is_player?SCRIPT_LOCATION_PLAYER:SCRIPT_LOCATION_OBJECT,is_player?24:20);if(!id)return 0;
        put(a,id+8,is_player?player:binding);put(a,id+12,is_player?index:destination);
        if(is_player)put(a,id+16,destination);
        scalar_float_put(a,id+(is_player?20:16),radius);
        if(a->memory.fault){(void)scene_projection_storage_free(id);return 0;}*out=id;return 1;
    }
    else return 0;
    if(!game_script_next(r))return 0;
    if(kind==SCRIPT_CHANGE_VECTOR){
        record=game_script_variable_find(a,r->token,1);if(!record || !a->scripts)return 0;
        for(i=0;i<a->scripts->vector.count;++i)if(get(a,a->scripts->vector.rows+i*4)==record){destination=i;break;}
        if(destination==UINT32_MAX || !game_script_next(r))return 0;
        for(operation=0;operation<5;++operation)if(!strcmp(r->token,operations[operation]))break;
        if(operation==5 || !game_script_next(r) || !spatial_point_parse(a,r,&source,point))return 0;
        id=script_node(kind,32);if(!id)return 0;put(a,id+8,destination);put(a,id+12,source);
        for(i=0;i<3;++i)scalar_float_put(a,id+16+i*4,point[i]);
        put(a,id+28,operation);
    }else{
        /*485e83..485ed0: an absent player selector leaves the current token
         *as the sound name. Only a literal/dynamic selector advances it. */
        if(kind==SCRIPT_PLAY_SOUND){
            uint32_t selector;int explicit_player=scalar_index(a,r->token,&selector) ||
                (token_integer(r->token,&selector) && selector<6u);
            if(explicit_player) {
                if(!statement_player_capture(a,r->token,6,&player,&index) || !game_script_next(r))return 0;
            }else player=get(a,0x5f0208);
            if(!named_id(a,r->token,0x59ac58,276,16,&sound) || !game_script_next(r))return 0;
        }else if(!statement_player_capture(a,r->token,4,&player,&index) || !game_script_next(r))return 0;
        id=script_node(kind,kind==SCRIPT_REFRESH_WEAPONS?16:20);if(!id)return 0;
        if(kind==SCRIPT_REFRESH_WEAPONS){put(a,id+8,player);put(a,id+12,index);}
        else{put(a,id+8,sound);put(a,id+12,player);put(a,id+16,index);}
    }
    if(a->memory.fault){(void)scene_projection_storage_free(id);return 0;}*out=id;return 1;
}
int game_script_statement_leaf_destroy(GameApplication *a,GameAddress id){
    if(a && id && !a->memory.fault && get(a,id)==SCRIPT_CREATE_PICKUP){
        GameAddress binding=get(a,id+88);
        if(binding && !scene_projection_storage_free(binding))return 0;
    }
    if(a && id && !a->memory.fault && get(a,id)==SCRIPT_SET_GATE){
        GameAddress binding=get(a,id+20);
        if(binding && !scene_projection_storage_free(binding))return 0;
    }
    return a && id && !a->memory.fault && statement_leaf_kind(get(a,id)) && !a->memory.fault && scene_projection_storage_free(id);
}
int game_script_statement_leaf_execute(GameApplication *a,GameAddress id,int *result){
    uint32_t kind,i,index,operation,bits;GameAddress player,body,child,configuration,record,destination;float source[3];long double x,y,value;
    if(!a || !id || !result || a->memory.fault)return 0;
    *result=1;kind=get(a,id);if(!statement_leaf_kind(kind) || a->memory.fault)return 0;
    if(kind==SCRIPT_CREATE_UNIT)return script_spawn_execute(a,id,result);
    if(kind==SCRIPT_UPDATE_NP){
        GameAddress binding=get(a,id+12);uint32_t slot;
        record=scalar_record(a,get(a,id+8));if(!record || !spatial_binding_actor(a,binding,&body))return 0;
        slot=get(a,record+32);
        if(body){GameSamplePairSelection selected;int32_t requested;float x=(float)((long double)scalar_float(a,0x574bd4)-scalar_float(a,body+24));
            memcpy(&bits,&x,4);
            memcpy(&requested,&slot,4);
            selected=game_sample_pair_register(&a->memory,requested,bits,get(a,body+32));
            put(a,record+32,(uint32_t)selected.index);
        }else if(slot!=UINT32_MAX){
            if(slot<5u){put(a,0x602538+slot*8u,0xbf800000u);put(a,0x60253c+slot*8u,0xbf800000u);}
            put(a,record+32,UINT32_MAX);
        }
        return !a->memory.fault;
    }
    if(kind==SCRIPT_DESTROY_UNIT || kind==SCRIPT_REMOVE_UNIT){
        char label[32];GameAddress binding,arrival;uint32_t sequence,identity;
        if(!game_memory_read(&a->memory,id+8,label,sizeof(label)) || !memchr(label,0,sizeof(label)))return 0;
        binding=application_name_registry_find(label);
        if(!binding || !spatial_binding_actor(a,binding,&body))return 0;
        if(!body)return !a->memory.fault;
        if(kind==SCRIPT_DESTROY_UNIT){
            if(!game_level_scene_killed(a,body,0,get(a,get(a,body)+16)))return 0;
            body=get(a,binding+32);
            return body && game_level_scene_reset(a,body,get(a,get(a,body)+32));
        }
        if(get(a,body+16)&0x20000u)return !a->memory.fault;
        identity=get(a,binding+36);if((identity&63u)>=6u)return 0;
        player=get(a,0x5f0210+(identity&63u)*4u);
        if(!player)return 0;
        arrival=game_level_factory_take(a,0x45,0);if(!arrival)return 0;
        sequence=get(a,player+24);put(a,player+24,sequence+1u);
        identity=(sequence<<6)|game_memory_read_u16(&a->memory,player+8);
        if(game_level_descriptor_transfer(a,arrival,identity,body) && script_spawn_publish(a,player,arrival))return 1;
        (void)game_level_factory_destroy(a,arrival);return 0;
    }
    if(kind==SCRIPT_KILLER_SAT){
        player=leaf_player(a,id+20,6);if(!player)return !a->memory.fault;
        index=get(a,id+20);
        if(index!=UINT32_MAX){
            if(index>=get(a,0x605888) || !(record=get(a,get(a,0x60586c)+index*4)))return 0;
            for(i=0;i<3;i++)put(a,id+8+i*4,get(a,record+32+i*4));
        }
        for(i=0;i<3;i++)source[i]=scalar_float(a,id+8+i*4);
        if(!game_level_satellite_launch(a,player,source))return 0;
        scalar_float_put(a,id+12,source[1]);return !a->memory.fault;
    }
    if(kind==SCRIPT_CREATE_PICKUP){
        ScriptScalar quantity={get(a,id+48),get(a,id+52),get(a,id+88)};
        uint32_t variant=get(a,id+44),payload=0,vector=get(a,id+20);GameAddress object=0,binding;
        int selected;char label[32];
        player=leaf_player(a,id+4,4);if(!player)return !a->memory.fault;
        if(vector!=UINT32_MAX && get(a,id+16)){
            if(vector>=get(a,0x605888) || !(record=get(a,get(a,0x60586c)+vector*4)))return 0;
            for(i=0;i<3;i++)put(a,id+24+i*4,get(a,record+32+i*4));
        }
        for(i=0;i<3;i++)source[i]=scalar_float(a,id+24+i*4);
        if(variant==UINT32_MAX){
            if(!gameplay_root_choose_pickup(&a->memory,player,0,&variant,&payload,&selected))return 0;
            if(!selected)return !a->memory.fault;
        }else if(!scalar_value(a,&quantity,&payload))return 0;
        if(!game_level_root_pickup_create(a,player,!get(a,id+16),source,
            scalar_truncate(scalar_float(a,id+36)),scalar_truncate(scalar_float(a,id+40)),variant,payload,&object))return 0;
        if(!object)return !a->memory.fault;
        if(!game_memory_read(&a->memory,id+56,label,sizeof(label)) || !memchr(label,0,sizeof(label)))return 0;
        if(application_name_registry_is_marker(label))return !a->memory.fault;
        binding=application_name_registry_find(label);if(!binding)return 0;
        put(a,binding+32,object);put(a,binding+36,get(a,object+20));game_memory_write_u8(&a->memory,binding+44,0);
        return !a->memory.fault;
    }
    if(kind==SCRIPT_MOVEMENT_PATH)return script_movement_execute(a,id,result);
    if(kind==SCRIPT_COUNT_OBJECTS || kind==SCRIPT_CURRENT_WEAPON){
        uint32_t answer=0;player=leaf_player(a,id+8,kind==SCRIPT_COUNT_OBJECTS?6u:4u);
        record=scalar_record(a,get(a,id+8));if(!record || a->memory.fault)return 0;
        if(player){
            if(kind==SCRIPT_COUNT_OBJECTS){if(!script_unit_count(a,player,game_memory_read_u8(&a->memory,id+20),&answer))return 0;}
            else {body=get(a,player+0x3bc);if(!body)return 0;answer=get(a,body+0x214+game_memory_read_u8(&a->memory,id+20)*4);}
        }
        put(a,record+32,answer);return !a->memory.fault;
    }
    if(kind==SCRIPT_END_MISSION){
        player=leaf_player(a,id+4,4);if(!player || !get(a,player+20) || !get(a,player+0x468))return !a->memory.fault;
        return game_script_notice_publish(a,player,get(a,id+16),id+24,get(a,id+20),0);
    }
    if(kind==SCRIPT_GOTO_MODE){
        uint32_t mode=get(a,id+8);GameRootEventServices services=game_input_event_services(a);
        player=leaf_player(a,id+8,4);
        if(a->memory.fault)return 0;
        if(!player || !get(a,player+0x468))return !a->memory.fault;
        body=get(a,player+0x3bc);if(!body || !get(a,body+0x58))return !a->memory.fault;
        if(mode<2u)return game_root_signal_publish(&a->memory,&services,player,mode?33:49);
        if(mode==2u && player==get(a,0x5f0208)){
            game_input_queue_clear(&a->memory,body+0x13c);
            return !a->memory.fault && game_input_queue_button(a,body+0x13c,6) && game_input_selector_begin(a);
        }
        return !a->memory.fault;
    }
    if(kind==SCRIPT_NAV_POINT){
        index=get(a,id+8);
        if(index!=UINT32_MAX){
            if(index>=get(a,0x605888) || !(record=get(a,get(a,0x60586c)+index*4)))return 0;
            for(i=0;i<3;i++)put(a,id+12+i*4,get(a,record+32+i*4));
        }
        {float first=(float)((long double)scalar_float(a,0x574bd4)-scalar_float(a,id+12));
            memcpy(&bits,&first,4);
            return game_input_navigation_set(a,bits,get(a,id+20),get(a,id+24)!=0);
        }
    }
    if(kind==SCRIPT_BILLBOARD){
        uint8_t text[256];size_t length;
        player=leaf_player(a,id+0x104,6);
        if(a->memory.fault)return 0;
        if(player!=get(a,0x5f0208))return !a->memory.fault;
        for(length=0;length<sizeof(text);length++){
            text[length]=game_memory_read_u8(&a->memory,id+8+(uint32_t)length);if(!text[length])break;
        }
        return length<sizeof(text) && game_application_timed_message_bytes(a,text,length,(int32_t)get(a,id+0x110));
    }
    if(kind==SCRIPT_SET_TELEPORTER || kind==SCRIPT_TELEPORTER_OWNER){
        player=get(a,id+12);
        if(kind==SCRIPT_TELEPORTER_OWNER && !player){
            index=get(a,id+16);record=scalar_record(a,index);
            if(record)player=selected_player(a,get(a,record+32));
            if(!player){*result=0;return !a->memory.fault;}
        }
        if(!spatial_binding_actor(a,get(a,id+8),&body))return 0;
        if(!body)return !a->memory.fault;
        return kind==SCRIPT_SET_TELEPORTER?game_level_teleporter_enable(a,body,player):game_level_teleporter_bind(a,body,player);
    }
    if(kind==SCRIPT_SET_GATE){
        ScriptScalar scalar={get(a,id+12),get(a,id+16),get(a,id+20)};uint32_t enabled;
        if(!scalar_value(a,&scalar,&enabled))return 0;
        index=get(a,id+8);if(index==UINT32_MAX)return !a->memory.fault;
        if(index>=get(a,0x5e147c))return 0;
        record=get(a,get(a,0x5e1480)+index*4);if(!record)return 0;
        if(get(a,record+0x24)!=enabled){
            body=get(a,record+0x20);
            if(!body || !game_level_gate_begin(a,body,enabled?2:3))return 0;
            put(a,record+0x24,enabled);
        }
        return !a->memory.fault;
    }
    if(kind==SCRIPT_LOCATION_PLAYER || kind==SCRIPT_LOCATION_OBJECT){
        uint32_t x_bits,z_bits;float output[2];int is_player=kind==SCRIPT_LOCATION_PLAYER;
        index=get(a,id+(is_player?16:12));if(index>=get(a,0x605888))return 0;
        destination=get(a,get(a,0x60586c)+index*4);if(!destination)return 0;
        body=0;
        if(is_player){player=leaf_player(a,id+4,4);
            if(player && get(a,player+20)){body=get(a,player+0x3bc);if(!body)return 0;
                if((int32_t)get(a,body+0x58)<=0)body=0;}
        }else if(!spatial_binding_actor(a,get(a,id+8),&body))return 0;
        if(a->memory.fault)return 0;
        if(!body){for(i=0;i<3;++i)put(a,destination+32+i*4,0);return !a->memory.fault;}
        x_bits=get(a,body+24);z_bits=get(a,body+32);
        if((int32_t)x_bits<=0 || x_bits>=0x48000000 || (int32_t)z_bits<=0 || z_bits>=0x48000000){
            for(i=0;i<3;++i)put(a,destination+32+i*4,0);
        }else{
            put(a,destination+36,get(a,body+28));
            (void)gameplay_geometry_nearby_location(&a->memory,scalar_float(a,body+24),scalar_float(a,body+32),scalar_float(a,id+(is_player?20:16)),output);
            if(a->memory.fault)return 0;
            scalar_float_put(a,destination+32,output[0]);scalar_float_put(a,destination+40,output[1]);
        }
        return !a->memory.fault;
    }
    if(kind==SCRIPT_CHANGE_VECTOR){
        index=get(a,id+12);operation=get(a,id+28);if(operation>4)return 0;
        if(index!=UINT32_MAX){if(index>=get(a,0x605888))return 0;record=get(a,get(a,0x60586c)+index*4);if(!record)return 0;
            for(i=0;i<3;++i)source[i]=scalar_float(a,record+32+i*4);
        }else for(i=0;i<3;++i)source[i]=scalar_float(a,id+16+i*4);
        index=get(a,id+8);if(index>=get(a,0x605888))return 0;destination=get(a,get(a,0x60586c)+index*4);if(!destination)return 0;
        for(i=0;i<3;++i){x=scalar_float(a,destination+32+i*4);y=source[i];
            if(operation==0)value=x+y;else if(operation==1)value=x-y;else if(operation==2)value=x*y;
            else if(operation==3){memcpy(&bits,&source[i],4);if(!(bits&0x7fffffffu))continue;value=x/y;}
            else {memcpy(&bits,&source[i],4);put(a,destination+32+i*4,bits);continue;}
            scalar_float_put(a,destination+32+i*4,(float)value);
        }
    }else{
        player=leaf_player(a,id+(kind==SCRIPT_REFRESH_WEAPONS?4:8),kind==SCRIPT_REFRESH_WEAPONS?4:6);
        if(a->memory.fault)return 0;
        if(!player)return 1;
        if(kind==SCRIPT_PLAY_SOUND && player!=get(a,0x5f0208))return !a->memory.fault;
        body=get(a,player+0x3bc);if(!body)return kind==SCRIPT_REFRESH_WEAPONS && !a->memory.fault;
        if((int32_t)get(a,body+0x58)<=0)return !a->memory.fault;
        if(kind==SCRIPT_REFRESH_WEAPONS){
            for(i=0;i<15;++i){int32_t quantity,capacity;child=get(a,body+0x1d4+i*4);if(!child)continue;
                quantity=(int32_t)get(a,child+12);if(quantity==-1)continue;
                configuration=get(a,child+20);if(!configuration)return 0;capacity=(int32_t)get(a,configuration+40);
                if(capacity>quantity)put(a,child+12,(uint32_t)capacity);
                if(a->memory.fault)return 0;
            }
        }else {SoundPlaybackRequest request={(int16_t)(uint16_t)get(a,id+8),0,1,0,0,0};
            if(!game_menu_sound_play(a,&request))return 0;
        }
    }
    return !a->memory.fault;
}
static int default_command_execute(GameApplication *a,int *result){
    GameAddress player,other,vehicle;uint32_t i;int alive;*result=0;
    if((int32_t)get(a,0x5ac6ac)<312)return !a->memory.fault;
    player=get(a,0x5f0208);
    if(get(a,0x605890)){
        if(!player)return 0;
        vehicle=get(a,player+0x3bc);if(!vehicle)return 0;
        if((int32_t)get(a,vehicle+0x58)>0){
            for(i=0;i<6;++i){other=get(a,0x5f0210+i*4);if(!other)return 0;
                if(get(a,other+20) && other!=player){
                    if(!game_script_player_units_alive(a,other,&alive))return 0;
                    if(alive)break;
                }
            }
            if(i==6){
                if(!game_script_notice_publish(a,player,1,0x5961f8,312,0))return 0;
                *result=1;return !a->memory.fault;
            }
        }
    }
    for(i=0;i<4;++i){other=get(a,0x5f0210+i*4);
        if(!other || !get(a,other+0x468))continue;
        vehicle=get(a,other+0x3bc);if(!vehicle)return 0;
        if(!get(a,vehicle+0x58) && !get(a,other+0x588))
            if(!game_script_notice_publish(a,other,0,0x596220,312,0))return 0;
    }
    return !a->memory.fault;
}
static int manager_nodes_retire(GameApplication *a){
    struct GameScriptState *s=a->scripts;
    if(!s)return 1;
    if(s->auxiliary){if(!scene_projection_storage_free(s->auxiliary))return 0;s->auxiliary=0;if(s->manager_ready)put(a,0x605878,0);}
    if(s->names){application_name_registry_shutdown();s->names=0;if(s->manager_ready)put(a,0x605868,0);}
    s->name_count=0;s->manager_ready=0;return 1;
}
/*4e16e0/45f570. Existing catalog/text pools have separate owners; reject an
 *unconverted nonempty pool rather than clear or free unknown allocations. */
int game_level_world_prepare(GameApplication *a){
    struct GameScriptState *s;char root[32]={0};unsigned i;uint32_t victory;
    if(!a || a->scripts || a->memory.fault || get(a,0x605868) || get(a,0x605878) ||
       get(a,0x60587c) || get(a,0x605870) || get(a,0x60586c) || get(a,0x605894) ||
       get(a,0x5e1480) || get(a,0x5e147c))return 0;
    if(!get(a,0x6041e0) && (get(a,0x605880) || get(a,0x60589c)))return 0;
    s=calloc(1,sizeof(*s));if(!s)return 0;a->scripts=s;s->manager_ready=1;
    for(i=0;i<32;++i){root[i]=(char)game_memory_read_u8(&a->memory,0x596258+i);if(!root[i])break;}
    if(a->memory.fault || !application_name_registry_begin(root))goto fail;
    s->names=APPLICATION_NAME_DESCRIPTOR;
    s->name_count=1;s->auxiliary=script_node(SCRIPT_DEFAULT_COMMAND,8);
    if(!s->auxiliary || !table_begin(&s->scalar,36) || !table_begin(&s->vector,44))goto fail;
    put(a,0x605868,s->names);put(a,0x605878,s->auxiliary);put(a,0x60588c,0);put(a,0x605894,0);
    if(!get(a,0x6041e0)){put(a,0x605880,0);put(a,0x60589c,0);}
    put(a,0x5e147c,0);put(a,0x5e1480,0);
    victory=!get(a,0x58abdc);
    if(!get(a,0x5ac678) && !get(a,0x5ac67c) && !get(a,0x5eff04))victory=0;
    put(a,0x605890,victory);publish(a);if(a->memory.fault)goto fail;
    return 1;
fail:
    (void)game_level_script_release(a);return 0;
}
int game_level_world_release(GameApplication *a){
    if(!a || a->memory.fault || get(a,0x5e1480) || get(a,0x5e147c))return 0;
    if(!get(a,0x6041e0) && (get(a,0x605880) || get(a,0x60589c)))return 0;
    if((get(a,0x605868) || get(a,0x605878)) && (!a->scripts || !a->scripts->manager_ready))return 0;
    if(a->scripts && a->scripts->manager_ready &&
       (get(a,0x605868)!=a->scripts->names || get(a,0x605878)!=a->scripts->auxiliary))return 0;
    return game_level_script_release(a);
}
/*4c7270's already formatted .hzs name comes from the existing menu consumer. */
int game_menu_map_file_preview(GameApplication *a,const char *name){
    size_t length;int ok=0,released=0;if(!a || !name)return 0;length=strlen(name);if(length>=256)return 0;
    put(a,0x6041e0,1);
    if(!game_level_world_release(a))goto done;
    released=1;
    if(!game_memory_write(&a->memory,0x5acc40,name,length+1) || !game_level_world_prepare(a) ||
       !game_level_player_script_load(a) || !game_level_commands_initialize(a))goto done;
    ok=1;
done:
    if(!ok && released)(void)game_level_script_release(a);
    put(a,0x6041e0,0);return ok && !a->memory.fault;
}
