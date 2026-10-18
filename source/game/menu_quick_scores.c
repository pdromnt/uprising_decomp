#include "menu_widgets.h"
#include "menu_runtime_tables.h"
#include <math.h>
#include <stdio.h>
typedef struct ScoreTerm {GameAddress word,weight;} ScoreTerm;
static long double factor(GameApplication *a,GameAddress p){return game_menu_runtime_float(&a->memory,p);}
static long double integer(GameApplication *a,GameAddress p,int increment) {
    uint32_t word=game_memory_read_u32(&a->memory,p);
    return (long double)(int32_t)(word+(increment?1u:0u));
}
static long double sum(GameApplication *a,const ScoreTerm *terms,unsigned count,GameAddress offset) {
    long double total=integer(a,terms[0].word+offset,1)*factor(a,terms[0].weight);unsigned i;
    for(i=1;i<count;++i)total+=integer(a,terms[i].word+offset,1)*factor(a,terms[i].weight);
    return total;
}
static float capped(GameApplication *a,long double value) {
    return value>factor(a,0x57447c)?100.0f:(float)value;
}
/*004a7350: original faction parameter weighting, explicit float stores and
 * saturation, followed by ordered component addition. No floating CPU state. */
long double game_menu_quick_strength(GameApplication *a) {
    static const ScoreTerm terms[]={
        {0x602840,0x574470},{0x602858,0x574474},{0x602814,0x574474},
        {0x602830,0x574474},{0x602838,0x574474},{0x60283c,0x574474},
        {0x602854,0x574474},{0x602820,0x574474},{0x602844,0x574474},
        {0x602834,0x574474},{0x602804,0x574478},{0x6027f4,0x574478},
        {0x6027f8,0x574478},{0x6027f0,0x574478}};
    GameAddress root=game_memory_read_u32(&a->memory,0x5f0208),offset;
    float parameters,resources,capacity,population;long double v,result,weight;
    if(!root){a->memory.fault=1;return 0;}
    offset=(uint32_t)game_memory_read_u16(&a->memory,root+8)*240u;
    parameters=(float)(sum(a,terms,sizeof(terms)/sizeof(terms[0]),offset)*factor(a,0x57447c));
    resources=capped(a,integer(a,0x602874+offset,0)*factor(a,0x574480));
    capacity=capped(a,integer(a,0x5acc8c,0)*factor(a,0x574484));
    v=integer(a,0x60287c+offset,0)+integer(a,0x602880+offset,0);
    v+=integer(a,0x602884+offset,0);v+=integer(a,0x602878+offset,0);
    population=capped(a,v*factor(a,0x574488)*factor(a,0x57447c));
    weight=factor(a,0x57448c);result=(long double)population*weight+(long double)resources*weight;
    result+=(long double)capacity*weight;result+=(long double)parameters*weight;return result;
}
/*004a7590: unit/vehicle settings and scenario difficulty. Wrapped increments
 * retain default-69 and integer-limit behavior; three float stores are explicit. */
long double game_menu_quick_difficulty(GameApplication *a) {
    static const ScoreTerm first[]={
        {0x602fa0,0x574490},{0x602fb4,0x574490},{0x602fa4,0x574490},
        {0x602fa8,0x574494},{0x602fd0,0x574498},{0x602fc8,0x574498},
        {0x602fcc,0x57449c},{0x602fc4,0x5744a0}};
    static const ScoreTerm second[]={
        {0x602fd4,0x5744a4},{0x603008,0x5744a4},{0x602fe8,0x5744a4},
        {0x602fd8,0x5744a4},{0x602fdc,0x5744a4},{0x603004,0x5744a8},
        {0x602ff8,0x5744a8},{0x60300c,0x5744a8},{0x602ffc,0x5744ac},{0x603000,0x5744ac}};
    float units=(float)(sum(a,first,sizeof(first)/sizeof(first[0]),0)*integer(a,0x602f7c,0));
    float vehicles=(float)(sum(a,second,sizeof(second)/sizeof(second[0]),0)*integer(a,0x602f78,0));
    float auxiliary=(float)(integer(a,0x602f88,0)*factor(a,0x57447c));
    int32_t scenario=(int32_t)game_memory_read_u32(&a->memory,0x5accd0);
    long double weight=factor(a,0x57448c),total;
    if(scenario==-1)return factor(a,0x5744b0);
    total=(long double)units*weight+integer(a,0x6027e8,0)*factor(a,0x5744b4);
    total+=(long double)auxiliary*weight;total+=(long double)vehicles*weight;
    return total*factor(a,0x5744b8)+((long double)scenario-factor(a,0x5744bc))*factor(a,0x5744b8)*factor(a,0x57447c);
}
static int32_t display_integer(long double value) {
    return !isfinite(value) || value<-9223372036854775808.0L || value>=9223372036854775808.0L?
        0:(int32_t)(uint32_t)(uint64_t)(int64_t)truncl(value);
}
/*0048fd70: existing owned labels consume the same two score algorithms. */
int game_menu_quick_feedback(GameApplication *a) {
    GameAddress label=game_memory_read_u32(&a->memory,0x58abec);char text[128];int32_t value;const char *format;
    if(label) {
        value=display_integer(game_menu_quick_strength(a)*factor(a,0x574114));
        snprintf(text,sizeof(text),"%d",value);if(!game_menu_text_image_set_bytes(a,label,text,1))return 0;
    }
    label=game_memory_read_u32(&a->memory,0x58abf0);
    if(label) {
        value=display_integer(game_menu_quick_difficulty(a)*factor(a,0x574114));
        if(value<0)snprintf(text,sizeof(text),"TRAINING");
        else {format=value<200?"%d (VERY EASY)":value<400?"%d (EASY)":value<600?"%d (MEDIUM)":value<800?"%d (HARD)":"%d (EXPERT)";
            snprintf(text,sizeof(text),format,value);}
        if(!game_menu_text_image_set_bytes(a,label,text,1))return 0;
    }
    return !a->memory.fault;
}
