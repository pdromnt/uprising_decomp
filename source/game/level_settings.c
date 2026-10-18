#include "level_settings.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
typedef struct ScalarSetting {const char *name;GameAddress field;int floating;} ScalarSetting;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static const ScalarSetting settings[]={
 {"FOG_COLOR:",0x5accdc,0},{"USE_RABBLE:",0x5ac67c,0},{"BOMBER_RATE:",0x5ac69c,0},
 {"AAV_RATE:",0x5ac698,0},{"TANK_RATE:",0x5ac694,0},{"SOLDIER_RATE:",0x5ac690,0},
 {"MAP_MAKERS_RULE:",0x6027e8,0},{"RABBLE_BUILDING_DIST:",0x5e1218,0},
 {"BOMBER_BUILDING_DIST:",0x629730,0},{"SLDR_BLDG_DEFENSE_SQUADS:",0x5e1210,0},
 {"SLDR_BLDG_DEFENSE_GUYS_PER_SQUAD:",0x5e1224,0},{"SLDR_BLDG_ATTACK_GUYS_PER_SQUAD:",0x5e1214,0},
 {"SLDR_BLDG_TOTAL_GUYS:",0x5e122c,0},{"TANK_BLDG_DEFENSE_SQUADS:",0x5e1230,0},
 {"TANK_BLDG_DEFENSE_GUYS_PER_SQUAD:",0x5e120c,0},{"TANK_BLDG_ATTACK_GUYS_PER_SQUAD:",0x5e1220,0},
 {"TANK_BLDG_TOTAL_GUYS:",0x5e1234,0},{"AAV_BLDG_DEFENSE_SQUADS:",0x5e1228,0},
 {"AAV_BLDG_DEFENSE_GUYS_PER_SQUAD:",0x5e121c,0},{"AAV_BLDG_ATTACK_GUYS_PER_SQUAD:",0x5e1238,0},
 {"AAV_BLDG_TOTAL_GUYS:",0x5e123c,0},{"DITHER_ON:",0x5acdb4,0},{"LIGHTING_ON:",0x626718,0},
 {"RANDDC_ON:",0x5acd64,0},{"INIT_ENERGY:",0x5acc8c,0},{"INIT_LIVES:",0x5ac9c0,0},
 {"FREE_SOLDIERS:",0x5ac900,0},{"FREE_TANKS:",0x5acbf0,0},{"FREE_BOMBERS:",0x5ac904,0},
 {"FREE_AAVS:",0x5acc84,0},{"LOAD_TRAINING_SOUNDS:",0x59bd98,0},{"USE_RANDOM_POWERUPS:",0x5ac68c,0},
 {"SKY_SPEED:",0x626d50,1},{"PLR_CONVOY_TL:",0x5ac9d8,0},{"FOG_O_WAR:",0x58de70,0},
 /*0054e800 shared AI settings: immutable directive data, ordinary C parsing. */
 {"IMPERIUM_EXPAND_RATE:",0x604138,0},{"IMPERIUM_ATTACK_RATE:",0x6041c4,0},
 {"IMPERIUM_DEFENSIVE_CALLIN_DELAY_TURRET:",0x604144,0},
 {"IMPERIUM_DEFENSIVE_CALLIN_DELAY_SOLDIER:",0x604128,0},
 {"IMPERIUM_DEFENSIVE_CALLIN_DELAY_TANK:",0x60413c,0},
 {"IMPERIUM_DEFENSIVE_CALLIN_DELAY_AAV:",0x604190,0},
 {"IMPERIUM_DEFENSIVE_SOLDIERS:",0x604170,0},{"IMPERIUM_DEFENSIVE_AAVS:",0x604124,0},
 {"IMPERIUM_DEFENSIVE_TANKS:",0x604180,0},{"MAKE_INITIAL_SQUAD:",0x604120,0},
 {"2ND_IMPERIAL_ATTACK:",0x604164,0},{"ATTACK_PLAYER_RATE:",0x604160,0},
 {"ATTACK_PLAYER_TANK_FORCE:",0x6041b4,1},{"ATTACK_PLAYER_SOLDIER_FORCE:",0x60417c,1},
 {"ATTACK_PLAYER_AAV_FORCE:",0x604184,1},{"INIT_IMPERIUM_MONEY:",0x6041ac,0},
 {"USE_CLAN_AI:",0x5ac678,0},{"IMPERIUM_KSAT_ATTACK_RATE:",0x6041a4,0},
 {"IMPERIUM_KSAT_SWEEP_PLATFORM:",0x604130,0},{"IMPERIUM_KSAT_REPEAT:",0x604194,0},
 {"IMPERIUM_DEFENSIVE_AAV_CAP:",0x6041c8,0},{"IMPERIUM_DEFENSIVE_TANK_CAP:",0x6041b8,0},
 {"IMPERIUM_DEFENSIVE_SOLDIER_CAP:",0x604148,0},{"IMPERIUM_ATTACK_WAVES:",0x604140,0},
 {"IMPERIUM_TOWERLESS_ATTACK_WAVES:",0x604134,0},{"IMPERIUM_MINE_PERCENT_CHANCE:",0x604250,0},
 {"IMPERIUM_EFFECTIVENESS_WEIGHT:",0x60415c,0},{"IMPERIUM_UPGRADE_USED_WEIGHT:",0x6041a8,0},
 {"IMPERIUM_PLATFORM_HEALTH_WEIGHT:",0x604198,0},{"IMPERIUM_PLATFORM_PROXIMITY_WEIGHT:",0x6041b0,0},
 {"IMPERIUM_PLATFORM_LODE_WEIGHT:",0x604154,0},{"IMPERIUM_OWNER_PROXIMITY_WEIGHT:",0x604150,0},
 {"IMPERIUM_ATTACK_HISTORY_WEIGHT:",0x6041c0,0},{"IMPERIUM_PSYCHO_INDEX:",0x6041a0,0},
 {"IMPERIUM_PSYCHO_WAVES:",0x60418c,0},{"IMPERIUM_REPLACE_TOWER_DELAY:",0x60419c,0},
 {"IMPERIUM_KSAT_ATTACK_TYPE:",0x604178,0},{"ATTACK_CONVOY_RATE:",0x60412c,0},
 {"IMPERIUM_REINIT_SQUAD_COUNT:",0x60414c,0}
};
static const char *after(const char *line,const char *key){size_t n=strlen(key);return strncmp(line,key,n)?0:line+n;}
static int scalar(GameApplication *a,const char *line) {
    unsigned i;for(i=0;i<sizeof(settings)/sizeof(settings[0]);++i) {
        const char *value=after(line,settings[i].name);if(!value)continue;
        if(settings[i].floating){float number;uint32_t bits;if(sscanf(value,"%f",&number)==1){memcpy(&bits,&number,4);put(a,settings[i].field,bits);}}
        else {int number;if(sscanf(value,"%d",&number)==1)put(a,settings[i].field,(uint32_t)number);}
        return 1;
    }
    return 0;
}
static int name_word(GameApplication *a,GameAddress source,const char *name) {
    unsigned i;for(i=0;i<256;++i){uint8_t c=game_memory_read_u8(&a->memory,source+i);if(c!=(uint8_t)name[i])return 0;if(!c)return !a->memory.fault;}return 0;
}
static int environment_name(GameApplication *a,GameAddress source,const char *name) {
    unsigned i;for(i=0;i<256;++i) {
        uint8_t first=game_memory_read_u8(&a->memory,source+i),second=(uint8_t)name[i];
        if(first>='A' && first<='Z')first+=32;
        if(second>='A' && second<='Z')second+=32;
        if(first!=second)return 0;
        if(!first)return !a->memory.fault;
    }
    return 0;
}
static uint32_t ticks(uint32_t n) {uint32_t word=n*1000u;return (word>>5)|((word&0x80000000u)?0xf8000000u:0);}
static int pair(GameApplication *a,const char *value,GameAddress first,GameAddress second,int convert) {
    int lo,hi;if(sscanf(value,"%d %d",&lo,&hi)!=2 || hi<lo)return -1;
    put(a,first,convert?ticks((uint32_t)lo):(uint32_t)lo);put(a,second,convert?ticks((uint32_t)hi):(uint32_t)hi);return 1;
}
static int string_field(GameApplication *a,const char *value,GameAddress field,size_t capacity,const char *prefix) {
    char name[512],output[512];int size;
    if(sscanf(value,"%511s",name)!=1)return 1;
    size=snprintf(output,sizeof(output),"%s%s",prefix,name);
    if(size<0 || (size_t)size>=capacity || (size_t)size>=sizeof(output))return -1;
    return game_memory_write(&a->memory,field,output,(size_t)size+1)?1:-1;
}
/*0054f0d0: one line operates directly on existing configuration owners.
 * Return1 handled,0 unknown,-1 malformed/invalid. No CPU callback ABI. */
int game_level_settings_line(GameApplication *a,const char *line) {
    static const struct {const char *name;unsigned offset;int imperial;} tech[]={
     {"RBL_SOLDIER_TL:",0x1fc,0},{"RBL_TANK_TL:",0x200,0},{"RBL_AAV_TL:",0x204,0},
     {"RBL_BOMBER_TL:",0x210,0},{"RBL_TRRT_TL:",0x220,0},{"RBL_LTRRT_TL:",0x224,0},
     {"RBL_CONVOY_TL:",0x218,0},{"RBL_HTRRT_TL:",0x228,0},{"RBL_SAM_TL:",0x22c,0},
     {"IMP_SOLDIER_TL:",0x29c,1},{"IMP_TANK_TL:",0x2a0,1},{"IMP_AAV_TL:",0x2a4,1},
     {"IMP_BOMBER_TL:",0x2b0,1},{"IMP_TRRT_TL:",0x2c0,1},{"IMP_LTRRT_TL:",0x2c4,1},
     {"IMP_HTRRT_TL:",0x2c8,1},{"IMP_SAM_TL:",0x2cc,1},{"IMP_CTRRT_TL:",0x2d0,1},
     {"IMP_KSAT_TL:",0x2d4,1},{"IMP_MINE_TL:",0x2dc,1},{"IMP_CONVOY_TL:",0x2b8,1}};
    static const GameAddress powerup_names[]={0x59de8c,0x59de98,0x59dea4,0x59deb0,0x59dec0,0x59ded8,0x59dee4,0x59def0,0x59def8};
    static const GameAddress weapon_names[]={0x59df04,0x59df14,0x59df28,0x59df40,0x59df54,0x59df64,0x59df74,0x59df90,0x59dfa0,0x59dfb4,0x59dfc4,0x59dfdc,0x59dfec,0x59dffc,0x59e010};
    const char *value;char name[512];unsigned i;int number,lo,hi;
    if(!line)return -1;
    if(scalar(a,line))return a->memory.fault?-1:1;
    for(i=0;i<sizeof(tech)/sizeof(tech[0]);++i)if((value=after(line,tech[i].name))) {
        GameAddress owner=get(a,tech[i].imperial?0x5f0220:0x5f0224);
        if(!owner || sscanf(value,"%d",&number)!=1)return -1;
        put(a,owner+tech[i].offset,(uint32_t)number);return a->memory.fault?-1:1;
    }
    if((value=after(line,"SLK_FILE:")))return string_field(a,value,0x5acc90,64,"");
    if((value=after(line,"SCRIPT_FILE:"))){int result=string_field(a,value,0x5acc40,64,"");put(a,0x5ac688,1);return result;}
    if((value=after(line,"MAP_TEXT:")))return string_field(a,value,0x639a78,128,"");
    if((value=after(line,"SKY1:")))return string_field(a,value,0x5ac8c0,64,"");
    if((value=after(line,"SKY2:")))return string_field(a,value,0x5acbf8,64,"");
    if((value=after(line,"POLY_SKY:")))return string_field(a,value,0x626dd0,128,"");
    if((value=after(line,"PALETTE:")))return string_field(a,value,0x5ac930,68,"grids\\palettes\\");
    if((value=after(line,"ENVIRONMENT_TYPE:"))) {
        if(sscanf(value,"%511s",name)!=1)return -1;
        put(a,0x5ac908,UINT32_MAX);for(i=0;i<5;++i)if(environment_name(a,0x583610+i*256,name)){put(a,0x5ac908,i);return 1;}return -1;
    }
    if((value=after(line,"POWERUP_RADIUS_RANGE:")))return pair(a,value,0x5ac974,0x5ac914,0);
    if((value=after(line,"POWERUP_FREQ_RANGE:")))return pair(a,value,0x5ac6a8,0x5accd8,1);
    if((value=after(line,"POWERUP_LIFESPAN:"))){if(sscanf(value,"%d",&number)==1)put(a,0x5acc3c,ticks((uint32_t)number));return 1;}
    if((value=after(line,"POWERUP_SETTING:"))) {
        if(sscanf(value,"%511s %d %d",name,&lo,&hi)!=3 || lo<0 || hi<0)return -1;
        for(i=0;i<9;++i)if(name_word(a,powerup_names[i],name)){put(a,0x5ac978+i*8,(uint32_t)lo);put(a,0x5ac97c+i*8,(uint32_t)hi);return 1;}
        return -1;
    }
    if((value=after(line,"POWERUP_TANK_AMMO:"))) {
        if(sscanf(value,"%511s %d",name,&number)!=2)return -1;
        for(i=0;i<15;++i)if(name_word(a,weapon_names[i],name)){put(a,0x596a00+i*4,(uint32_t)number);return 1;}
        return -1;
    }
    /*These original0054e800 branches discard their local value, or return1
     * immediately. Preserve the actual behavior, not invented AI settings. */
    if(after(line,"IMPERIUM_DEFENSIVE_CALLIN_DELAY:") || after(line,"IMPERIUM_ATTACK_ACCURACY:") ||
       after(line,"IMPERIUM_DEFENSIVENESS:") || after(line,"IMPERIUM_BOOSTER_TYPE:") || after(line,"IMPERIUM_PERSONALITY_TYPE:"))return 1;
    return 0;
}
/*The original line reader ignores blank/comment lines without trimming the
 * returned directive. EOF closes this owned read-only stream and reports failure
 * instead of looping indefinitely on malformed/missing END_LFL_FILE. */
int game_level_settings_read(GameApplication *a,const char *path) {
    ResourceFile *file=resource_file_open(path);char line[512];unsigned records=0;int header=0,result=0;
    if(!file){game_application_log(a,"native level settings open failed: %s\n",path);return 0;}
    while(records++<65536 && resource_file_line(file,line,sizeof(line))) {
        const unsigned char *text=(const unsigned char *)line;
        while(isspace(*text))++text;
        if(!*text || *text=='#')continue;
        if(!header){header=1;continue;}
        if(!strncmp(line,"END_LFL_FILE",12)){result=1;break;}
        {int status=game_level_settings_line(a,line);char diagnostic[800];
         if(status<=0){snprintf(diagnostic,sizeof(diagnostic),"%s: %s:%u: %s\n",
             status<0?"native level settings rejected":"Bad lfl line",path,records,line);
             game_application_log(a,"%s",diagnostic);}
         if(status<0)break;}
    }
    resource_file_close(file);return result && !a->memory.fault;
}
/*0054e580/004b3960: reset only the original published settings. Manager
 * allocations belong to their startup owners and survive selecting a new level. */
int game_level_settings_defaults(GameApplication *a) {
    static const GameAddress strings[]={0x639a78,0x5ac930,0x626dd0,0x5acbf8,0x5ac8c0,0x5acc40,0x5acc90};
    static const struct {GameAddress field;uint32_t value;} words[]={
     {0x5ac688,0},{0x5ac908,0},{0x5acdb4,1},{0x626718,1},{0x5acd64,0},{0x5ac67c,1},
     {0x5ac68c,1},{0x5ac684,0},{0x5ac690,580},{0x626d50,0x3f800000},{0x5ac694,580},
     {0x59bd98,0},{0x5accdc,UINT32_MAX},{0x5ac698,800},{0x5ac69c,1160},
     {0x5ac678,0},{0x6041c4,640},{0x604144,640},{0x60413c,640},{0x604124,0},
     {0x604170,2},{0x604180,1},{0x604120,1},{0x604188,0},{0x604164,10},{0x604160,10},
     {0x604138,1920},{0x604130,0},{0x604194,1},{0x604128,480},{0x604178,0},
     {0x6041c8,2},{0x604190,1440},{0x6041b8,2},{0x604134,1},{0x604140,0},
     {0x6041a4,13440},{0x60415c,4},{0x604198,4},{0x6041b0,2},{0x604154,1},
     {0x604150,1},{0x6041c0,1},{0x6041a0,1},{0x60418c,2},{0x6041ac,9000},
     {0x604148,6},{0x6041a8,3},{0x6041b4,0x40200000},{0x604184,0x3f000000},
     {0x60412c,0},{0x60417c,0x40c00000},{0x60419c,960},{0x60414c,6},{0x604250,66},
     {0x5e1218,21900},{0x629730,131072},{0x5e1210,2},{0x5e1230,1},{0x6027e8,50},
     {0x5e120c,1},{0x5e1220,1},{0x5e1234,5},{0x5e1228,1},{0x5e121c,1},
     {0x5e1238,1},{0x5e123c,5},{0x58de70,0},{0x5e1224,3},{0x5ac9c0,5},
     {0x5acc84,0},{0x5ac904,0},{0x5acbf0,0},{0x5ac900,0},{0x5ac9d8,0},
     {0x5e1214,3},{0x5e122c,12},{0x5acc8c,2000},{0x5ac984,100},{0x5ac994,0},
     {0x5ac98c,1000},{0x5ac974,6000},{0x5ac914,12000},
     {0x5ac6a8,3125},{0x5accd8,6250},{0x5acc3c,3750}};
    static const uint32_t ammo[]={100,100,1,200,100,100,20,100,25,200,20,15,50,3,10};unsigned i;
    for(i=0;i<sizeof(strings)/sizeof(strings[0]);++i)game_memory_write_u8(&a->memory,strings[i],0);
    game_memory_fill_u32(&a->memory,0x5ac978,18,1,0);
    for(i=0;i<sizeof(words)/sizeof(words[0]);++i)put(a,words[i].field,words[i].value);
    for(i=0;i<15;++i)put(a,0x596a00+i*4,ammo[i]);
    return !a->memory.fault;
}
