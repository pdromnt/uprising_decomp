#include "gameplay_catalog.h"
#include "entity_queries.h"
#include <string.h>
#include <stdio.h>

GameAddress gameplay_collection_message(uint8_t unit){
    static const GameAddress messages[]={0x58f810u,0x58f748u,0x58f778u,0x58f7a0u,
        0x58f7c8u,0x58f800u,0x58f75cu,0x58f788u,0x58f7b0u,0x58f7d8u,0x58f7f0u};
    return messages[unit<=10u?unit:0u];
}
GameAddress gameplay_unit_name(int16_t unit){
    if(unit<0 || unit>10)return 0x5897c4u;
    return unit>=5 && unit<=9?0x589770u+(uint32_t)(unit-5)*12u:0x589764u;
}
int gameplay_collection_feedback(GameMemory *m,const GameplayFeedbackServices *s,int16_t kind,uint8_t unit){
    char text[80],name[80];GameAddress message=0u;uint32_t price=0u;int status,length;unsigned i;
    if(kind<0 || kind>4)return !m->fault;
    if(!s || !s->text)return 0;
    if(kind && (!s->sound || !s->sound(s->context,122u)))return 0;
    if(kind==0)message=gameplay_collection_message(unit);
    else if(kind==1)message=0x58f828u;
    else if(kind==3)message=0x58f858u;
    if(message)return s->text(s->context,message,NULL);
    if(kind==2){
        GameAddress root=game_memory_read_u32(m,0x5f0208u);
        status=gameplay_catalog_unit_price(m,root,(int16_t)unit,&price);
        if(status<0 || m->fault)return 0;
        if(!status && (!s->units.price || !s->units.price(s->units.context,root,(int16_t)unit,&price)))return 0;
        /* Interpret the original signed %d argument without unsigned-to-signed overflow. */
        length=snprintf(text,sizeof(text),"upgrade costs $%ld\n",
            (long)(price<0x80000000u?(int64_t)price:(int64_t)price-INT64_C(4294967296)));
    }else{
        GameAddress address=gameplay_unit_name((int16_t)unit);
        if(unit>10u && (!s->diagnostic || !s->diagnostic(s->context,0x5897acu)))return 0;
        for(i=0u;i<sizeof(name);i++){
            name[i]=(char)game_memory_read_u8(m,address+i);
            if(m->fault)return 0;
            if(!name[i])break;
        }
        if(i==sizeof(name))return 0;
        length=snprintf(text,sizeof(text),"Upgrade requires: %s",name);
    }
    if(length<0 || (size_t)length>=sizeof(text))return 0;
    return s->text(s->context,0u,text);
}

int gameplay_catalog_can_spend(GameMemory *memory,GameAddress catalog,uint32_t cost){
    uint32_t bits=game_memory_read_u32(memory,catalog+0x22cu);int32_t allowance,requested;
    memcpy(&allowance,&bits,4u);memcpy(&requested,&cost,4u);
    return !memory->fault && allowance>=requested;
}
int gameplay_catalog_spend(GameMemory *memory,GameAddress catalog,uint32_t cost){
    uint32_t allowance=game_memory_read_u32(memory,catalog+0x22cu);
    if(memory->fault)return 0;
    game_memory_write_u32(memory,catalog+0x22cu,allowance-cost);
    return !memory->fault;
}

int gameplay_catalog_credit(GameMemory *memory,GameAddress catalog,uint32_t amount){
    GameAddress primary=game_memory_read_u32(memory,catalog+0x3bcu);
    uint32_t bits=primary?game_memory_read_u32(memory,primary+0x58u):0u;int32_t health;
    memcpy(&health,&bits,4u);
    if(primary && health>0){
        game_memory_write_u32(memory,catalog+0x22cu,game_memory_read_u32(memory,catalog+0x22cu)+amount);
        game_memory_write_u32(memory,catalog+0x150u,game_memory_read_u32(memory,catalog+0x150u)+amount);
    }
    return !memory->fault;
}
static GameAddress catalog_resolve_group(GameMemory *memory,GameAddress catalog,uint32_t identity,
    GameplayCatalogMemberLookup lookup,void *context,unsigned primary_offset){
    static const uint32_t heads[]={0x64u,0xc8u,0x12cu};
    GameAddress primary,collection,node,match;uint32_t visits=0u;unsigned list;
    if(primary_offset){
        primary=game_memory_read_u32(memory,catalog+primary_offset);
        if(game_memory_read_u32(memory,primary+0x14u)==identity && !memory->fault)return primary;
    }
    for(collection=game_memory_read_u32(memory,catalog+0x294u);collection && !memory->fault;
        collection=game_memory_read_u32(memory,collection+0x64u)){
        int32_t index=0;
        if(++visits>65536u){memory->fault=1;return 0u;}
        while(!memory->fault && index<(int16_t)game_memory_read_u16(memory,collection+0x24u)){
            GameAddress array=game_memory_read_u32(memory,collection+0x18u);
            node=game_memory_read_u32(memory,array+(uint32_t)index*4u);
            if(memory->fault)return 0u;
            if(node){
                match=0u;
                if(!lookup || !lookup(context,node,identity,&match)){memory->fault=1;return 0u;}
                if(match)return match;
            }
            ++index;
        }
    }
    for(list=0u;list<3u && !memory->fault;list++)
        for(node=game_memory_read_u32(memory,catalog+heads[list]);node && !memory->fault;
            node=game_memory_read_u32(memory,node+0x48u)){
            if(++visits>65536u){memory->fault=1;return 0u;}
            if(game_memory_read_u32(memory,node+0x14u)==identity && !memory->fault)return node;
        }
    return 0u;
}

/* 481d60: identity low bits select one of six established catalog owners.
 * Known root/member methods stay native; foreign classes are explicit ports. */
int gameplay_catalog_resolve_identity(GameMemory *memory,uint32_t identity,
    GameplayCatalogMemberLookup foreign_root,GameplayCatalogMemberLookup foreign_member,
    void *context,GameAddress *result){
    uint32_t category=identity&63u,method;GameAddress owner,table;int status;
    if(!memory || !result)return 0;
    *result=0u;if(category>=6u)return 1;
    owner=game_memory_read_u32(memory,0x5f0210u+category*4u);
    if(!game_memory_read_u32(memory,owner+0x14u))return !memory->fault;
    table=game_memory_read_u32(memory,owner);method=game_memory_read_u32(memory,table+4u);
    if(memory->fault)return 0;
    status=gameplay_catalog_lookup(memory,owner,method,identity,foreign_member,context,result);
    if(status)return status>0;
    return foreign_root && foreign_root(context,owner,identity,result) && !memory->fault;
}
typedef struct CatalogLookup {
    GameMemory *memory;
    GameplayCatalogMemberLookup external;
    void *context;
    unsigned depth;
} CatalogLookup;
static int catalog_lookup_method(CatalogLookup *,GameAddress,uint32_t,uint32_t,GameAddress *);
static int catalog_lookup_member(void *opaque,GameAddress member,uint32_t identity,GameAddress *result){
    CatalogLookup *lookup=opaque;
    GameAddress table=game_memory_read_u32(lookup->memory,member);
    uint32_t method=game_memory_read_u32(lookup->memory,table+0x14u);
    int status;
    if(lookup->memory->fault)return 0;
    status=catalog_lookup_method(lookup,member,method,identity,result);
    if(status)return status>0;
    return lookup->external && lookup->external(lookup->context,member,identity,result);
}
static int catalog_lookup_method(CatalogLookup *lookup,GameAddress owner,uint32_t method,
    uint32_t identity,GameAddress *result){
    GameMemory *memory=lookup->memory;
    if(memory->fault)return -1;
    switch(method){
    case 0x53e450u:case 0x53f150u:case 0x53fc20u:{
        GameAddress payload=game_memory_read_u32(memory,owner+0x48u);
        *result=game_memory_read_u32(memory,payload+0x14u)==identity?payload:0u;
        break;
    }
    case 0x476540u:*result=native_entity_resolve_lists(memory,owner,identity);break;
    case 0x478660u:*result=native_entity_resolve(memory,owner,identity).match;break;
    case 0x4788d0u:case 0x47aee0u:case 0x47a6c0u:
        if(lookup->depth>=32u){memory->fault=1;return -1;}
        ++lookup->depth;
        *result=catalog_resolve_group(memory,owner,identity,catalog_lookup_member,lookup,
            method==0x47aee0u?0x3bcu:method==0x47a6c0u?0x2e0u:0u);
        --lookup->depth;break;
    default:return 0;
    }
    return memory->fault?-1:1;
}
int gameplay_catalog_lookup(GameMemory *memory,GameAddress owner,uint32_t method,
    uint32_t identity,GameplayCatalogMemberLookup external,void *context,GameAddress *result){
    CatalogLookup lookup={memory,external,context,0u};
    return catalog_lookup_method(&lookup,owner,method,identity,result);
}

int gameplay_catalog_unit_price(GameMemory *memory,GameAddress owner,int16_t unit,uint32_t *price){
    static const uint8_t types[]={0x23u,0x80u,0x81u,0x82u,0x83u,0x89u,0x84u,0x85u,0x86u,0x87u,0x88u};
    static const uint16_t choices[]={0u,0x29cu,0x2a0u,0x2a4u,0x2b0u,0x2d4u,0x29cu,0x2a0u,0x2a4u,0x2b0u,0x2d8u};
    GameAddress table=game_memory_read_u32(memory,owner);
    uint32_t method=game_memory_read_u32(memory,table+0x28u);
    if(memory->fault)return -1;
    if(method==0x481dd0u){*price=0u;return 1;}
    if(method!=0x47e0e0u)return 0;
    if(unit<0 || unit>10){*price=0xffffffffu;return 1;}
    if(choices[unit] && (game_memory_read_u32(memory,owner+choices[unit])&0x80000000u)){
        *price=0u;return memory->fault?-1:1;
    }
    if(game_memory_read_u32(memory,table+0x44u)!=0x47e280u)return memory->fault?-1:0;
    *price=gameplay_catalog_value(memory,owner,types[unit],8u,0u);
    return memory->fault?-1:1;
}
int gameplay_catalog_choice(GameMemory *memory,GameAddress owner,uint8_t kind,uint32_t *value){
    unsigned field=0;GameAddress methods=game_memory_read_u32(memory,owner);
    uint32_t method=game_memory_read_u32(memory,methods+0x50u);
    if(memory->fault || !value)return -1;
    if(method!=0x47ede0u)return 0;
    switch(kind){
    case 0x20:field=0x2e0;break;case 0x21:case 0x81:case 0x85:field=0x2a0;break;
    case 0x22:field=0x2c0;break;case 0x23:field=0x2d0;break;case 0x24:field=0x2cc;break;
    case 0x25:field=0x2c4;break;case 0x26:case 0x83:case 0x87:field=0x2b0;break;
    case 0x27:case 0x82:case 0x86:field=0x2a4;break;case 0x28:field=0x2c8;break;
    case 0x29:field=0x2b8;break;case 0x2a:field=0x2bc;break;
    case 0x40:case 0x80:case 0x84:field=0x29c;break;
    case 0x88:field=0x2d8;break;case 0x89:field=0x2d4;break;
    default:*value=0;return 1;
    }
    *value=game_memory_read_u32(memory,owner+field);return memory->fault?-1:1;
}
int gameplay_catalog_unit_enabled(GameMemory *memory,GameAddress owner,int16_t unit){
    static const uint16_t fields[]={0u,0u,0u,0u,0u,0x228u,0x204u,0x208u,0x20cu,0x210u,0u};
    uint32_t value;
    if(unit<0 || unit>10)return 0;
    if(!fields[unit])return !memory->fault;
    value=game_memory_read_u32(memory,owner+fields[unit]);
    return !memory->fault && value>=2u && value<0x80000000u;
}
int gameplay_catalog_unit_affordable(GameMemory *memory,GameAddress owner,uint32_t cost,uint32_t *allowed){
    GameAddress table=game_memory_read_u32(memory,owner);
    uint32_t method=game_memory_read_u32(memory,table+0x2cu);
    if(memory->fault)return -1;
    if(method==0x477730u)*allowed=1u;
    else if(method==0x47f210u || method==0x479470u)*allowed=(uint32_t)gameplay_catalog_can_spend(memory,owner,cost);
    else return 0;
    return memory->fault?-1:1;
}
int gameplay_collection_eligibility(GameMemory *memory,const GameplayUnitServices *services,
    GameAddress collection,int16_t unit,uint32_t *free_slot,uint16_t *reason){
    GameAddress array=game_memory_read_u32(memory,collection+0x18u),owner;
    uint32_t slot=1u,price=0u,allowed=0u;int32_t count;int status;
    if(!game_memory_read_u32(memory,array)){*reason=3u;return !memory->fault;}
    owner=game_memory_read_u32(memory,collection+0x54u);
    status=gameplay_catalog_unit_price(memory,owner,unit,&price);
    if(status<0)return 0;
    if(!status && (!services || !services->price || !services->price(services->context,owner,unit,&price))){
        memory->fault=1;return 0;
    }
    count=(int16_t)game_memory_read_u16(memory,collection+0x24u);
    while((int32_t)slot<count && game_memory_read_u32(memory,array+slot*4u))++slot;
    if(memory->fault)return 0;
    if((int32_t)slot==count){*reason=1u;return 1;}
    if(!gameplay_catalog_unit_enabled(memory,owner,unit)){*reason=4u;return !memory->fault;}
    status=gameplay_catalog_unit_affordable(memory,owner,price,&allowed);
    if(status<0)return 0;
    if(!status && (!services || !services->can_afford || !services->can_afford(services->context,owner,price,&allowed))){
        memory->fault=1;return 0;
    }
    if(!allowed){*reason=2u;return 1;}
    *free_slot=slot;*reason=0u;return !memory->fault;
}
static float catalog_float(GameMemory *memory,GameAddress address){
    uint32_t bits=game_memory_read_u32(memory,address);float value;memcpy(&value,&bits,4u);return value;
}
static long double collection_distance(GameMemory *memory,GameAddress collection,GameAddress scene){
    volatile long double x=(long double)catalog_float(memory,collection+0x48u)-catalog_float(memory,scene+0x18u);
    volatile long double z=(long double)catalog_float(memory,collection+0x50u)-catalog_float(memory,scene+0x20u);
    volatile long double xx=x*x,zz=z*z;
    volatile long double distance=xx+zz;return distance;
}
GameAddress gameplay_player_choose_collection(GameMemory *memory,const GameplayUnitServices *services,uint8_t unit){
    GameAddress root=game_memory_read_u32(memory,0x5f0208u),selected=0u;
    uint32_t override=game_memory_read_u32(memory,root+0x44cu),index;
    int32_t count;float closest=0.0f;
    if(override!=0xffffffffu){
        GameAddress array=game_memory_read_u32(memory,0x639a70u);
        return game_memory_read_u32(memory,array+override*4u);
    }
    count=(int16_t)game_memory_read_u16(memory,0x639a74u);
    for(index=0u;(int32_t)index<count && !memory->fault;index++){
        GameAddress array=game_memory_read_u32(memory,0x639a70u);
        GameAddress collection=game_memory_read_u32(memory,array+index*4u),scene;
        uint32_t slot=0u;uint16_t reason;long double distance;
        if(game_memory_read_u32(memory,collection+0x54u)!=root)continue;
        if(unit && (!gameplay_collection_eligibility(memory,services,collection,unit,&slot,&reason) || reason))continue;
        scene=game_memory_read_u32(memory,root+0x448u);
        distance=collection_distance(memory,collection,scene);
        /* Original compares an extended candidate against the rounded cached
         * float. Equal distances retain the first; unordered compares select. */
        if(!selected || !(distance>=(long double)closest)){selected=collection;closest=(float)distance;}
    }
    return memory->fault?0u:selected;
}
typedef struct CatalogFamily { uint16_t choice; GameAddress records; } CatalogFamily;
static CatalogFamily family(uint8_t type){
    static const CatalogFamily vehicle[] = {
        {0x2e0u,0x5e40f8u},{0x2a0u,0x5e8a80u},{0x2c0u,0x5e7440u},
        {0x2d0u,0x5edfd8u},{0x2ccu,0x5e9a38u},{0x2c4u,0x5e7a90u},
        {0x2b0u,0x5e1488u},{0x2a4u,0x5eaaf8u},{0x2c8u,0x5ec788u},
        {0x2b8u,0x5ed6c8u},{0x2bcu,0x5eb418u}};
    static const CatalogFamily structure[] = {
        {0x29cu,0x5ea418u},{0x2a0u,0x5ebad8u},{0x2a4u,0x5e3b58u},
        {0x2b0u,0x5e21f8u},{0x29cu,0x5e31b8u},{0x2a0u,0x5e4308u},
        {0x2a4u,0x5ec838u},{0x2b0u,0x5ea788u},{0x2d8u,0x5ee348u},
        {0x2d4u,0x5e2fa8u}};
    CatalogFamily fallback={0u,0x5e6d80u},tank={0x29cu,0x5ef648u};
    if(type>=0x20u && type<=0x2au)return vehicle[type-0x20u];
    if(type==0x40u)return tank;
    if(type>=0x80u && type<=0x89u)return structure[type-0x80u];
    return fallback;
}
GameAddress gameplay_catalog_record(GameMemory *memory,GameAddress catalog,uint8_t type){
    CatalogFamily selected=family(type);
    uint32_t choice=selected.choice?game_memory_read_u32(memory,catalog+selected.choice):0u;
    return selected.records+choice*176u;
}
uint32_t gameplay_catalog_value(GameMemory *memory,GameAddress catalog,uint8_t type,
    uint32_t item,uint32_t alternate){
    GameAddress record=gameplay_catalog_record(memory,catalog,type);
    /* Structure families 80..87 have one property bank. Their query ignores
     * the alternate argument; the remaining families select either bank. */
    if(type>=0x80u && type<=0x87u)alternate=0u;
    return memory->fault?0u:game_memory_read_u32(memory,record+item*4u+(alternate?84u:0u));
}
/* Controller property consumers select an ordinary native catalog query. */
int gameplay_source_property(GameMemory *memory,GameAddress object,unsigned index,void *context,GameplayCatalogPropertyQuery query,uint32_t *value){
    uint32_t identity,health,limit,bits;int32_t signed_limit,signed_twice;
    if(!value || !query || (index!=1u && index!=2u))return 0;
    *value=0u;identity=game_memory_read_u32(memory,object+0x14u);
    if(index==2u && game_memory_read_u32(memory,0x594d3cu) && identity &&
       game_memory_read_u32(memory,0x5f0210u+(identity&63u)*4u)!=game_memory_read_u32(memory,0x58973cu))return !memory->fault;
    if(game_memory_read_u8(memory,object+0xb8u)&1u)return !memory->fault;
    if(index==2u && identity){
        health=game_memory_read_u32(memory,object+0x58u);
        if(health){
            bits=health*2u;memcpy(&signed_twice,&bits,4u);
            if(memory->fault || !query(context,object,0u,&limit))return 0;
            memcpy(&signed_limit,&limit,4u);
            if(signed_limit>=signed_twice){*value=11u;return !memory->fault;}
        }
    }
    return !memory->fault && query(context,object,index,value);
}
int gameplay_catalog_object_property(GameMemory *memory,GameAddress object,unsigned index,uint32_t *value){
    GameAddress catalog,table,record;uint8_t kind;
    if(index>2u || !value)return 0;
    if(game_memory_read_u8(memory,object+12u)==0x77u){
        uint32_t subtype=game_memory_read_u32(memory,object+0xa0u),identity=game_memory_read_u32(memory,object+20u);
        uint32_t maximum,health,twice;int32_t signed_maximum,signed_twice;
        if(subtype>=4u)return 0;
        *value=0u;
        /*43cde0 permits an unplaced template through the visibility gate. */
        if(index==2u && game_memory_read_u32(memory,0x594d3cu) && identity &&
           game_memory_read_u32(memory,0x5f0210u+(identity&63u)*4u)!=game_memory_read_u32(memory,0x58973cu))return memory->fault?-1:1;
        maximum=game_memory_read_u32(memory,0x5e7d50u+subtype*176u);
        health=game_memory_read_u32(memory,object+88u);twice=health*2u;
        memcpy(&signed_maximum,&maximum,4);memcpy(&signed_twice,&twice,4);
        if(index==2u && identity && health && signed_maximum>=signed_twice)*value=11u;
        else *value=game_memory_read_u32(memory,0x5e7d50u+subtype*176u+index*4u);
        return memory->fault?-1:1;
    }
    if(game_memory_read_u8(memory,object+12u)==0xb0u){
        uint32_t descriptor=game_memory_read_u32(memory,object+0x64u);
        if(descriptor>=15u)return 0;
        *value=index==2u && game_memory_read_u32(memory,0x594d3cu)?0u:
            game_memory_read_u32(memory,0x5e4fc0u+descriptor*0xb0u+index*4u);
        return memory->fault?-1:1;
    }
    catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+0x14u)&63u)*4u);
    if(index==2u && game_memory_read_u32(memory,0x594d3cu) && catalog!=game_memory_read_u32(memory,0x58973cu)){
        *value=0u;return memory->fault?-1:1;
    }
    kind=game_memory_read_u8(memory,object+0xcu);
    table=game_memory_read_u32(memory,catalog);
    if(game_memory_read_u32(memory,table+0x4cu)!=0x47eaf0u)return memory->fault?-1:0;
    record=gameplay_catalog_record(memory,catalog,kind);
    *value=record?game_memory_read_u32(memory,record+index*4u):(index==0u?25u:0u);
    return memory->fault?-1:1;
}
uint32_t gameplay_catalog_component_property(GameMemory *memory,GameAddress catalog,
    uint8_t type,unsigned index,int alternate,int local_only,GameAddress local_catalog){
    GameAddress record;
    if(index>2u || (index==2u && local_only && catalog!=local_catalog))return 0u;
    record=gameplay_catalog_record(memory,catalog,type);
    return memory->fault?0u:game_memory_read_u32(memory,record+index*4u+(alternate?84u:0u));
}
uint32_t gameplay_catalog_mount_value(GameMemory *memory,GameAddress catalog,
    uint8_t item,uint8_t type,uint32_t variant){
    CatalogFamily selected={0u,0u};uint32_t choice;
    switch(item){
    case 2u:
        if(type==0x20u){selected.choice=0x2e4u;selected.records=0x5e4bd0u;}
        else if(type==0x26u){selected.choice=0x2b4u;selected.records=0x5eda38u;}
        else {selected.choice=0x2d0u;selected.records=0x5e9fb8u;} break;
    case 3u: selected.choice=0x2f0u;selected.records=0x5eb4c8u;break;
    case 6u: selected.choice=0x2e8u;selected.records=0x5eb988u;break;
    case 7u: selected.choice=0x2f4u;selected.records=0x5e5a10u;break;
    case 8u: selected.choice=0x2f8u;selected.records=0x5e17f8u;break;
    case 9u:
        if(type==0x11u){selected.choice=0x308u;selected.records=0x5eae68u;}
        else if(type==0x20u){selected.choice=0x2dcu;selected.records=0x5ee608u;}
        else {selected.choice=0x29cu;selected.records=0x5e7650u;} break;
    case 12u:
        if(type==0x27u){selected.choice=0x2a8u;selected.records=0x5e1cb8u;}
        else {selected.choice=0x300u;selected.records=0x5ef9b8u;} break;
    case 13u:selected.choice=0x2fcu;selected.records=0x5e48c0u;break;
    case 15u:selected.choice=0x304u;selected.records=0x5ef4f8u;break;
    case 16u:selected.choice=0x2ecu;selected.records=0x5e4a10u;break;
    case 17u:selected.choice=0x308u;selected.records=0x5ebe48u;break;
    case 19u:selected.choice=0x30cu;selected.records=0x5e2568u;break;
    case 20u:selected.choice=0x310u;selected.records=0x5ef488u;break;
    case 21u:selected.choice=0x314u;selected.records=0x5e2188u;break;
    case 22u:selected.choice=0x318u;selected.records=0x5e2188u;break;
    default:return 0u;
    }
    choice=game_memory_read_u32(memory,catalog+selected.choice);
    return memory->fault?0u:game_memory_read_u32(memory,selected.records+(choice*28u+variant)*4u);
}
