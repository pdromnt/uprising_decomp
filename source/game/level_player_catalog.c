#include "level_player_resources.h"
#include "gameplay_catalog.h"

typedef struct AiFamily {uint16_t choice;GameAddress records;} AiFamily;
/*477e00/478180/478350: the AI has its own banks and choice fields. Original
 *jump-table destinations are behavior IDs; shipping selects these C recipes.
 *No instruction or guest calling convention is dispatched. */
static AiFamily ai_family(uint8_t type){
    AiFamily result={0,0x5e6d80};
    switch(type){
    case 0x21:result=(AiFamily){0x200,0x5ebf28};break;
    case 0x22:result=(AiFamily){0x220,0x5ecdd8};break;
    case 0x24:result=(AiFamily){0x22c,0x5e7b40};break;
    case 0x25:result=(AiFamily){0x224,0x5e2d98};break;
    case 0x26:result=(AiFamily){0x210,0x5e2a28};break;
    case 0x27:result=(AiFamily){0x204,0x5e8660};break;
    case 0x28:result=(AiFamily){0x228,0x5ece88};break;
    case 0x29:result=(AiFamily){0x218,0x5ed098};break;
    case 0x40:result=(AiFamily){0x1fc,0x5e1948};break;
    case 0x60:result=(AiFamily){0x1fc,0x5eeac8};break;
    case 0x62:result=(AiFamily){0x204,0x5e8df0};break;
    case 0x63:result=(AiFamily){0x200,0x5e61b0};break;
    case 0x64:result=(AiFamily){0x210,0x5eeee8};break;
    }return result;
}
static uint32_t method(GameApplication *a,GameAddress player,unsigned slot){
    return game_memory_read_u32(&a->memory,game_memory_read_u32(&a->memory,player)+slot);
}
/*479990/479ec0/47a190 use the imperial root's choices, not AI choices.
 *All banks retain their verified176-byte records;80..87 ignore alternate. */
static AiFamily imperial_family(uint8_t type){
    AiFamily result={0,0x5e6d80};
    switch(type){
    case 0x21:result=(AiFamily){0x2a0,0x5ed358};break;
    case 0x22:result=(AiFamily){0x2c0,0x5e7880};break;
    case 0x23:result=(AiFamily){0x2d0,0x5e7060};break;
    case 0x24:result=(AiFamily){0x2cc,0x5e46b0};break;
    case 0x25:result=(AiFamily){0x2c4,0x5e5b60};break;
    case 0x26:result=(AiFamily){0x2b0,0x5e6a10};break;
    case 0x27:result=(AiFamily){0x2a4,0x5e96c8};break;
    case 0x28:result=(AiFamily){0x2c8,0x5e85b0};break;
    case 0x29:result=(AiFamily){0x2b8,0x5e3528};break;
    case 0x2a:result=(AiFamily){0x2bc,0x5eee38};break;
    case 0x40:result=(AiFamily){0x29c,0x5edc68};break;
    case 0x80:result=(AiFamily){0x29c,0x5e37e8};break;
    case 0x81:result=(AiFamily){0x2a0,0x5ee758};break;
    case 0x82:result=(AiFamily){0x2a4,0x5e5e40};break;
    case 0x83:result=(AiFamily){0x2b0,0x5efb08};break;
    case 0x84:result=(AiFamily){0x29c,0x5eb618};break;
    case 0x85:result=(AiFamily){0x2a0,0x5e26b8};break;
    case 0x86:result=(AiFamily){0x2a4,0x5e8010};break;
    case 0x87:result=(AiFamily){0x2b0,0x5e9c48};break;
    case 0x88:result=(AiFamily){0x2d8,0x5ec578};break;
    case 0x89:result=(AiFamily){0x2d4,0x5e65d0};break;
    }return result;
}
int game_level_player_catalog_record(GameApplication *a,GameAddress player,uint8_t type,GameAddress *record){
    uint32_t selected=method(a,player,0x4c);
    if(a->memory.fault)return 0;
    if(selected==0x47eaf0)*record=gameplay_catalog_record(&a->memory,player,type);
    else if(selected==0x478180){
        AiFamily family=ai_family(type);uint32_t choice=family.choice?game_memory_read_u32(&a->memory,player+family.choice):0;
        *record=family.records+choice*176u;
    }else if(selected==0x479ec0){
        AiFamily family=imperial_family(type);uint32_t choice=family.choice?game_memory_read_u32(&a->memory,player+family.choice):0;
        *record=family.records+choice*176u;
    }else return 0;
    return !a->memory.fault;
}
int game_level_player_catalog_value(GameApplication *a,GameAddress player,uint8_t type,unsigned item,uint32_t alternate,uint32_t *value){
    uint32_t selected=method(a,player,0x44);
    if(a->memory.fault)return 0;
    if(selected==0x47e280)*value=gameplay_catalog_value(&a->memory,player,type,item,alternate);
    else if(selected==0x477e00){
        AiFamily family=ai_family(type);uint32_t choice=family.choice?game_memory_read_u32(&a->memory,player+family.choice):0;
        /*60/62/63/64 have a single bank, unlike the other AI/default families. */
        if(type==0x60 || type==0x62 || type==0x63 || type==0x64)alternate=0;
        *value=game_memory_read_u32(&a->memory,family.records+choice*176u+item*4u+(alternate?84u:0));
    }else if(selected==0x479990){
        AiFamily family=imperial_family(type);uint32_t choice=family.choice?game_memory_read_u32(&a->memory,player+family.choice):0;
        if(type>=0x80 && type<=0x87)alternate=0;
        *value=game_memory_read_u32(&a->memory,family.records+choice*176u+item*4u+(alternate?84u:0));
    }else return 0;
    return !a->memory.fault;
}
int game_level_player_catalog_choice(GameApplication *a,GameAddress player,uint8_t type,uint32_t *value){
    uint32_t selected=method(a,player,0x50);
    if(a->memory.fault)return 0;
    if(selected==0x47ede0)return gameplay_catalog_choice(&a->memory,player,type,value)==1;
    if(selected==0x478350){AiFamily family=ai_family(type);*value=family.choice?game_memory_read_u32(&a->memory,player+family.choice):0;return !a->memory.fault;}
    if(selected==0x47a190){AiFamily family=imperial_family(type);*value=family.choice?game_memory_read_u32(&a->memory,player+family.choice):0;return !a->memory.fault;}
    return 0;
}
/*479590: signed unit selector, unavailable choices and actual imperial bank.
 *The same native consumer is used by citadels and strategy production. */
int game_level_player_catalog_unit_price(GameApplication *a,GameAddress player,int16_t unit,uint32_t *value){
    static const uint8_t types[]={0x23,0x80,0x81,0x82,0x83,0x89,0x84,0x85,0x86,0x87,0x88};
    AiFamily family;
    if(method(a,player,0x28)!=0x479590)return gameplay_catalog_unit_price(&a->memory,player,unit,value)==1;
    if(unit<0 || unit>10){*value=UINT32_MAX;return !a->memory.fault;}
    family=imperial_family(types[unit]);
    if(unit && (game_memory_read_u32(&a->memory,player+family.choice)&0x80000000u)){*value=0;return !a->memory.fault;}
    return game_level_player_catalog_value(a,player,types[unit],8,0,value);
}
/*478470 ignores the type for these three mount records.47e7f0 has the actual
 *player-specific item/type rules already reconstructed in gameplay_catalog. */
int game_level_player_catalog_mount(GameApplication *a,GameAddress player,uint8_t item,uint8_t type,uint32_t variant,uint32_t *value){
    uint32_t selected=method(a,player,0x48);GameAddress records;unsigned field;
    if(a->memory.fault)return 0;
    if(selected==0x47e7f0){*value=gameplay_catalog_mount_value(&a->memory,player,item,type,variant);return !a->memory.fault;}
    if(selected==0x47a3e0){
        switch(item){case 2:field=0x2b4;records=0x5ecba8;break;
        case 9:field=type==0x20?0x2dc:0x29c;records=type==0x20?0x5ee608:0x5e67e0;break;
        case 12:field=0x2a8;records=0x5e1cb8;break;default:*value=0;return 1;}
        *value=game_memory_read_u32(&a->memory,records+game_memory_read_u32(&a->memory,player+field)*112u+variant*4u);
        return !a->memory.fault;
    }
    if(selected!=0x478470)return 0;
    switch(item){case 2:field=0x214;records=0x5e6e30;break;case 9:field=0x1fc;records=0x5e8380;break;
    case 12:field=0x208;records=0x5e4d20;break;default:*value=0;return 1;}
    *value=game_memory_read_u32(&a->memory,records+game_memory_read_u32(&a->memory,player+field)*112u+variant*4u);
    return !a->memory.fault;
}
