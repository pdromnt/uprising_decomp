#include "gameplay_state.h"
#include "game_intrusive_list.h"
#include "scene_projection_storage.h"
#include "gameplay_geometry.h"
#include "gameplay_spatial.h"
#include "entity_queries.h"
#include "effect_resource_pool.h"
#include "level_player_slots.h"
#include "level_effects.h"
#include "gameplay_leaves.h"
#include <string.h>

/* 4e1c20/4e1f00: untagged presentation/global words in their verified file
 * order. Keep existing owners and exact partial-read behavior. */
int gameplay_state_stream_presentation(GameMemory *memory,int reading,
    uint32_t *last_elements,const TaggedRecordStreamServices *services){
    static const GameAddress fields[]={
        0x605890u,0x604164u,0x604160u,0x60412cu,0x6041b4u,0x604184u,
        0x60417cu,0x60415cu,0x6041a8u,0x604198u,0x6041b0u,0x604154u,
        0x604150u,0x6041c0u,0x6041a0u,0x60418cu,0x6041a4u,0x604130u,
        0x604194u,0x604178u,0x5ac690u,0x5ac694u,0x5ac698u,0x5ac69cu,
        0x604138u,0x6041c4u,0x604140u,0x604134u,0x604128u,0x604190u,
        0x60413cu,0x604144u,0x60419cu,0x604170u,0x604124u,0x604180u,
        0x6041c8u,0x6041b8u,0x604148u,0x629730u
    };
    unsigned index;
    if(!memory || !last_elements || !services || (reading?!services->read:!services->write))return 0;
    for(index=0u;index<sizeof(fields)/sizeof(fields[0]);index++){
        uint8_t bytes[4];uint32_t value=game_memory_read_u32(memory,fields[index]);unsigned byte;
        for(byte=0u;byte<4u;byte++)bytes[byte]=(uint8_t)(value>>(byte*8u));
        if(memory->fault)return 0;
        if(reading){
            if(!services->read(services->context,bytes,4u,1u,last_elements))return 0;
            game_memory_write_u32(memory,fields[index],(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8u|
                (uint32_t)bytes[2]<<16u|(uint32_t)bytes[3]<<24u);
        }else{
            if(!services->write(services->context,bytes,4u,1u))return 0;
            *last_elements=1u;
        }
        if(memory->fault)return 0;
    }
    return 1;
}

static int state_archive_record(GameMemory *memory,int reading,uint32_t *marker,
    const TaggedRecordStreamServices *services,uint32_t tag,GameAddress address,
    unsigned width,unsigned count,int discard){
    uint8_t bytes[8192];unsigned index,byte;uint32_t header,elements;
    if((width!=1u && width!=2u && width!=4u) || count>sizeof(bytes)/width)return 0;
    for(index=0u;index<count;index++){
        uint32_t value=discard?0u:width==4u?game_memory_read_u32(memory,address+index*width):
            width==2u?game_memory_read_u16(memory,address+index*width):game_memory_read_u8(memory,address+index);
        for(byte=0u;byte<width;byte++)bytes[index*width+byte]=(uint8_t)(value>>(byte*8u));
    }
    if(memory->fault)return 0;
    if(!reading)return tagged_record_stream_write_elements(marker,tag,bytes,width,count,services);
    if(!tagged_record_stream_read_elements(marker,bytes,width,count,&header,&elements,services))return 0;
    game_memory_write_u32(memory,0x59d4b8u,header);
    if(!discard)for(index=0u;index<count;index++){
        const uint8_t *value=bytes+index*width;
        if(width==4u)game_memory_write_u32(memory,address+index*width,(uint32_t)value[0]|(uint32_t)value[1]<<8u|
            (uint32_t)value[2]<<16u|(uint32_t)value[3]<<24u);
        else if(width==2u)game_memory_write_u16(memory,address+index*width,(uint16_t)((unsigned)value[0]|(unsigned)value[1]<<8u));
        else game_memory_write_u8(memory,address+index,value[0]);
    }
    return !memory->fault;
}
static int state_archive_word(GameMemory *memory,int reading,uint32_t *marker,
    const TaggedRecordStreamServices *services,uint32_t *value){
    uint8_t bytes[4];uint32_t header,elements;unsigned byte;
    for(byte=0u;byte<4u;byte++)bytes[byte]=(uint8_t)(*value>>(byte*8u));
    if(!reading)return tagged_record_stream_write_elements(marker,0xfedcba92u,bytes,4u,1u,services);
    if(!tagged_record_stream_read_elements(marker,bytes,4u,1u,&header,&elements,services))return 0;
    game_memory_write_u32(memory,0x59d4b8u,header);
    *value=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8u|(uint32_t)bytes[2]<<16u|(uint32_t)bytes[3]<<24u;return !memory->fault;
}
static int state_root_member_saved(GameMemory *memory,GameAddress member,unsigned family){
    uint8_t kind=game_memory_read_u8(memory,member+0xcu);
    uint32_t flags=game_memory_read_u32(memory,member+0x10u);
    if(family==2u)return kind==0xbu || kind==0x2au;
    if(flags&0x20000u)return 0;
    return !family || (flags&0x80u) || kind==9u || kind==0xb0u || kind==0xa0u || kind==0xa1u;
}
static int state_archive_kind(GameMemory *memory,int reading,uint32_t *marker,
    const TaggedRecordStreamServices *records,uint8_t *kind){
    uint32_t header,elements;
    if(!reading)return tagged_record_stream_write_elements(marker,0xfedcba91u,kind,1u,1u,records);
    if(!tagged_record_stream_read_elements(marker,kind,1u,1u,&header,&elements,records))return 0;
    game_memory_write_u32(memory,0x59d4b8u,header);return !memory->fault;
}
/* Shared477900/477af0: save filtering and restoration into the embedded
 * sentinels preserve the established owners, order and file protocol. */
int gameplay_state_stream_root(GameMemory *memory,int reading,uint32_t *marker,
    const GameplayRootArchiveServices *services,GameAddress owner){
    static const unsigned heads[3]={0xc8u,0x12cu,0x64u};
    static const unsigned anchors[3]={0x80u,0xe4u,0x1cu};
    unsigned family,i,visits;uint32_t count,variant,identity;GameAddress node,anchor;
    const TaggedRecordStreamServices *records=services->records;
    if(!state_archive_record(memory,reading,marker,records,0xfedcba92u,owner+0x18u,4u,1u,0))return 0;
    for(family=0u;family<3u;family++){
        count=0u;visits=0u;
        if(!reading)for(node=game_memory_read_u32(memory,owner+heads[family]);node;node=game_memory_read_u32(memory,node+0x48u)){
            if(++visits>65536u || memory->fault)return 0;
            if(state_root_member_saved(memory,node,family))count++;
        }
        if(!state_archive_word(memory,reading,marker,records,&count))return 0;
        if(reading){
            if(count>=0x80000000u)continue;
            if(count>65536u)return 0;
            anchor=owner+anchors[family];
            for(i=0u;i<count;i++){
                uint8_t kind=0u;variant=0u;node=0u;
                if(!state_archive_kind(memory,1,marker,records,&kind))return 0;
                if(family==1u && (kind==0x77u || kind==0xb0u)){
                    if(!state_archive_word(memory,1,marker,records,&variant))return 0;
                }else if(!services->variant || !services->variant(services->context,owner,kind,&variant))return 0;
                if(!services->create || !services->create(services->context,kind,(uint16_t)variant,&node) || !node)return 0;
                if(!services->member || !services->member(services->context,node,1u))return 0;
                (void)game_list_insert_after(memory,node,anchor,0x48u,0x4cu);anchor=node;
                if(memory->fault)return 0;
            }
        }else{
            visits=0u;
            for(node=game_memory_read_u32(memory,owner+heads[family]);node;node=game_memory_read_u32(memory,node+0x48u)){
                uint8_t kind=game_memory_read_u8(memory,node+0xcu);
                if(++visits>65536u || memory->fault)return 0;
                if(!state_root_member_saved(memory,node,family))continue;
                if(!state_archive_kind(memory,0,marker,records,&kind))return 0;
                if(family==1u && (kind==0x77u || kind==0xb0u)){
                    variant=game_memory_read_u32(memory,node+(kind==0x77u?0xa0u:0x64u));
                    if(!state_archive_word(memory,0,marker,records,&variant))return 0;
                }
                if(!services->member || !services->member(services->context,node,0u))return 0;
            }
        }
    }
    if(!state_archive_record(memory,reading,marker,records,0xfedcba92u,owner+0x148u,4u,1u,0) ||
        !state_archive_record(memory,reading,marker,records,0xfedcba90u,owner+0x150u,4u,1u,0) ||
        !state_archive_record(memory,reading,marker,records,0xfedcba98u,owner+0x154u,4u,18u,0))return 0;
    identity=game_memory_read_u16(memory,owner+8u);
    for(i=0u;i<6u;i++)if(!state_archive_record(memory,reading,marker,records,0xfedcba92u,
        0x5f0160u+(identity*6u+i)*4u,4u,1u,0))return 0;
    return !memory->fault;
}
int gameplay_state_fix_root(GameMemory *memory,const GameplayRootArchiveServices *services,GameAddress owner){
    static const unsigned heads[3]={0xc8u,0x64u,0x12cu};unsigned family,visits;
    for(family=0u;family<3u;family++){
        GameAddress node=game_memory_read_u32(memory,owner+heads[family]);visits=0u;
        while(node){
            if(++visits>65536u || memory->fault || !services->member || !services->member(services->context,node,2u))return 0;
            node=game_memory_read_u32(memory,node+0x48u);
        }
    }
    return !memory->fault;
}
int gameplay_state_stream_group(GameMemory *memory,int reading,uint32_t *marker,
    const GameplayRootArchiveServices *services,GameAddress owner){
    uint8_t identities[6]={0u};uint32_t enabled=0u,header,elements;unsigned i;
    GameAddress catalog=game_memory_read_u32(memory,0x589740u),array;int count;
    if(!reading){
        array=game_memory_read_u32(memory,owner+0x18u);enabled=game_memory_read_u32(memory,array)!=0u;
    }
    if(!state_archive_word(memory,reading,marker,services->records,&enabled))return 0;
    if(reading && (!services->bind_group || !services->bind_group(services->context,owner,catalog,enabled)))return 0;
    count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
    if(count>4){memory->fault=1;return 0;}
    array=game_memory_read_u32(memory,owner+0x18u);
    if(reading){
        if(!tagged_record_stream_read_elements(marker,identities,2u,3u,&header,&elements,services->records))return 0;
        game_memory_write_u32(memory,0x59d4b8u,header);
        for(i=0u;count>1 && i<(unsigned)(count-1);i++){
            uint16_t identity=(uint16_t)((unsigned)identities[i*2u]|(unsigned)identities[i*2u+1u]<<8u);
            if(identity && (!services->bind_group_member || !services->bind_group_member(services->context,owner,identity,i+1u)))return 0;
            count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
            if(count>4){memory->fault=1;return 0;}
        }
    }else{
        for(i=1u;count>1 && i<(unsigned)count;i++){
            GameAddress member=game_memory_read_u32(memory,array+i*4u);
            unsigned identity=member?game_memory_read_u16(memory,member+0x14u):0u;
            identities[(i-1u)*2u]=(uint8_t)identity;identities[(i-1u)*2u+1u]=(uint8_t)(identity>>8u);
        }
        if(!tagged_record_stream_write_elements(marker,0xfedcba9fu,identities,2u,3u,services->records))return 0;
    }
    for(i=0u;count>0 && i<(unsigned)count;i++){
        GameAddress member=game_memory_read_u32(memory,game_memory_read_u32(memory,owner+0x18u)+i*4u);
        if(member && (!services->member || !services->member(services->context,member,reading?4u:3u)))return 0;
        count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
        if(count>4){memory->fault=1;return 0;}
    }
    return !memory->fault;
}
int gameplay_state_fix_group(GameMemory *memory,const GameplayRootArchiveServices *services,GameAddress owner){
    int count=(int16_t)game_memory_read_u16(memory,owner+0x24u);unsigned i;
    for(i=0u;count>0 && i<(unsigned)count;i++){
        GameAddress member=game_memory_read_u32(memory,game_memory_read_u32(memory,owner+0x18u)+i*4u);
        if(member && (!services->member || !services->member(services->context,member,5u)))return 0;
        count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
    }
    return !memory->fault;
}
static int state_player_root_groups(GameMemory *memory,int reading,uint32_t *marker,
    const GameplayRootArchiveServices *services,GameAddress owner){
    uint8_t bytes[512]={0u};uint32_t count=0u,header,elements,i;
    GameAddress node;
    if(!state_archive_record(memory,reading,marker,services->records,0xfedcba92u,owner+0x22cu,4u,1u,0))return 0;
    if(!reading)for(node=game_memory_read_u32(memory,owner+0x294u);node;node=game_memory_read_u32(memory,node+0x64u)){
        uint32_t identity=game_memory_read_u32(memory,node);unsigned b;
        if(count>=128u || memory->fault)return 0;
        for(b=0u;b<4u;b++)bytes[count*4u+b]=(uint8_t)(identity>>(b*8u));
        count++;
    }
    if(!state_archive_word(memory,reading,marker,services->records,&count) || count>128u)return 0;
    if(count){
        if(reading){
            if(!tagged_record_stream_read_elements(marker,bytes,4u,count,&header,&elements,services->records))return 0;
            game_memory_write_u32(memory,0x59d4b8u,header);
        }else if(!tagged_record_stream_write_elements(marker,0xfedcba9au,bytes,4u,count,services->records))return 0;
    }
    for(i=count;i>0u;i--){
        const uint8_t *value=bytes+(i-1u)*4u;
        uint32_t identity=(uint32_t)value[0]|(uint32_t)value[1]<<8u|(uint32_t)value[2]<<16u|(uint32_t)value[3]<<24u;
        int32_t groups=(int16_t)game_memory_read_u16(memory,0x639a74u);
        if(groups<=0 || identity>=(uint32_t)groups)return 0;
        GameAddress group=game_memory_read_u32(memory,game_memory_read_u32(memory,0x639a70u)+identity*4u);
        if(!group || !gameplay_state_stream_group(memory,reading,marker,services,group))return 0;
    }
    return !memory->fault;
}
int gameplay_state_save_player_root_base(GameMemory *memory,uint32_t *marker,
    const GameplayRootArchiveServices *services,GameAddress owner){
    return gameplay_state_stream_player_root_base(memory,0,marker,services,owner);
}
int gameplay_state_stream_player_root_base(GameMemory *memory,int reading,uint32_t *marker,
    const GameplayRootArchiveServices *services,GameAddress owner){
    return state_player_root_groups(memory,reading,marker,services,owner) &&
        gameplay_state_stream_root(memory,reading,marker,services,owner);
}
int gameplay_state_stream_player_root(GameMemory *memory,int reading,uint32_t *marker,
    const GameplayRootArchiveServices *services,GameAddress owner){
    static const unsigned first[5]={0x588u,0x4b4u,0x4b8u,0x4bcu,0x4c0u};
    static const unsigned last[6]={0x5a8u,0x5f4u,0x5f8u,0x5fcu,0x600u,0x5a4u};unsigned i;
    GameAddress primary=game_memory_read_u32(memory,owner+0x3bcu);
    if(!state_archive_record(memory,reading,marker,services->records,0xfedcbaa0u,owner+0x39cu,1u,32u,0) ||
        !primary || !services->member || !services->member(services->context,primary,reading?1u:0u))return 0;
    for(i=0u;i<5u;i++)if(!state_archive_record(memory,reading,marker,services->records,0xfedcba92u,owner+first[i],4u,1u,0))return 0;
    for(i=0u;i<18u;i++)if(!state_archive_record(memory,reading,marker,services->records,0xfedcba92u,owner+0x5acu+i*4u,4u,1u,0))return 0;
    for(i=0u;i<6u;i++)if(!state_archive_record(memory,reading,marker,services->records,0xfedcba92u,owner+last[i],4u,1u,0))return 0;
    if(!state_archive_record(memory,reading,marker,services->records,0xfedcba9au,owner+0x4c8u,4u,14u,0) ||
        !state_player_root_groups(memory,reading,marker,services,owner))return 0;
    return gameplay_state_stream_root(memory,reading,marker,services,owner);
}
int gameplay_state_fix_player_root(GameMemory *memory,const GameplayRootArchiveServices *services,GameAddress owner){
    GameAddress primary=game_memory_read_u32(memory,owner+0x3bcu);
    return primary && services->member && services->member(services->context,primary,2u) && gameplay_state_fix_root(memory,services,owner);
}
/* Descriptor-backed group members share their base record protocol. Family1
 * owns three identity references; family2 owns only the primary payload. */
int gameplay_state_stream_group_member(GameMemory *memory,int reading,unsigned family,uint32_t *marker,
    const GameplayRootArchiveServices *services,GameAddress owner){
    static const unsigned offsets[13]={0xcu,0x10u,0x18u,0x1cu,0x40u,0x44u,0x34u,0x20u,0x24u,0x28u,0x2cu,0x2eu,0x30u};
    static const uint8_t tags[13]={0x92u,0x92u,0x92u,0x92u,0x90u,0x93u,0x92u,0x93u,0x93u,0x93u,0x97u,0x97u,0x97u};
    unsigned i;
    if(family>2u)return 0;
    if(family){
        GameAddress primary=game_memory_read_u32(memory,owner+0x48u);
        if(!primary || !services->member || !services->member(services->context,primary,reading?1u:0u))return 0;
    }
    if(family==1u){
        uint8_t bytes[4]={0u};
        if(!state_archive_record(memory,reading,marker,services->records,0xfedcba92u,owner+0x4cu,4u,1u,0))return 0;
        for(i=0u;i<3u;i++){
            uint32_t identity=0u,header,elements;unsigned b;
            if(!reading){
                GameAddress member=game_memory_read_u32(memory,owner+0x50u+i*4u);
                identity=member?game_memory_read_u32(memory,member+0x14u):0u;
                for(b=0u;b<4u;b++)bytes[b]=(uint8_t)(identity>>(b*8u));
                if(!tagged_record_stream_write_elements(marker,0xfedcba90u,bytes,4u,1u,services->records))return 0;
            }else{
                if(!tagged_record_stream_read_elements(marker,bytes,4u,1u,&header,&elements,services->records))return 0;
                game_memory_write_u32(memory,0x59d4b8u,header);
                identity=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8u|(uint32_t)bytes[2]<<16u|(uint32_t)bytes[3]<<24u;
                game_memory_write_u32(memory,owner+0x50u+i*4u,identity);
            }
        }
    }
    for(i=0u;i<13u;i++)if(!state_archive_record(memory,reading,marker,services->records,0xfedcba00u+tags[i],
        owner+offsets[i],i<10u?4u:2u,1u,0))return 0;
    return !memory->fault;
}
int gameplay_state_fix_group_member(GameMemory *memory,unsigned family,const GameplayRootArchiveServices *services,GameAddress owner){
    unsigned i;uint32_t identity;GameAddress resolved=0u;
    if(family>2u)return 0;
    if(family==1u){
        for(i=0u;i<3u;i++){
            identity=game_memory_read_u32(memory,owner+0x50u+i*4u);
            if(identity){
                if(!services->resolve || !services->resolve(services->context,identity,&resolved))return 0;
                game_memory_write_u32(memory,owner+0x50u+i*4u,resolved);
            }
        }
        if(!services->member || !services->member(services->context,game_memory_read_u32(memory,owner+0x48u),2u))return 0;
    }
    identity=game_memory_read_u32(memory,owner+0x40u);
    if(identity && (!services->resolve || !services->resolve(services->context,identity,&resolved)))return 0;
    if(!identity)resolved=0u;
    game_memory_write_u32(memory,owner+0x3cu,resolved);
    if(identity && !resolved)game_memory_write_u32(memory,owner+0x40u,0u);
    return !memory->fault;
}
int gameplay_state_stream_factions(GameMemory *memory,int reading,const GameplayRootArchiveServices *services){
    uint32_t i=0u,visits;int success=1;
    if(reading)game_memory_write_u32(memory,0x589740u,game_memory_read_u32(memory,0x5f0210u));
    while(!memory->fault && (int32_t)i<(int32_t)game_memory_read_u32(memory,0x5e147cu)){
        GameAddress table=game_memory_read_u32(memory,0x5e1480u),faction,node;
        if(i>=65536u){success=0;break;}
        faction=game_memory_read_u32(memory,table+i*4u);node=game_memory_read_u32(memory,faction+0x20u);visits=0u;
        while(node){
            if(++visits>65536u || memory->fault || !services->member || !services->member(services->context,node,reading?1u:0u)){success=0;break;}
            node=game_memory_read_u32(memory,node+0xccu);
        }
        if(!success)break;
        i++;
    }
    if(reading)game_memory_write_u32(memory,0x589740u,0u);
    return success && !memory->fault;
}
int gameplay_state_stream_outer(GameMemory *memory,int reading,uint32_t *marker,
    const GameplayContainerArchiveServices *services,GameAddress outer){
    GameAddress node;uint32_t count=0u,index;unsigned offset,visits=0u;
    const TaggedRecordStreamServices *records=services->records;
    if(reading && !recyclable_outer_reset_payload(memory,outer,services->storage))return 0;
    if(!reading)for(node=game_memory_read_u32(memory,outer+0xcu);node;node=game_memory_read_u32(memory,node+4u)){
        if(++count>65536u || memory->fault)return 0;
    }
    for(offset=0x14u;offset<=0x1cu;offset+=4u)
        if(!state_archive_record(memory,reading,marker,records,0xfedcba93u,outer+offset,4u,1u,0))return 0;
    if(!state_archive_record(memory,reading,marker,records,0xfedcba92u,outer+0x20u,4u,1u,0) ||
        !state_archive_word(memory,reading,marker,records,&count))return 0;
    if(reading && count>65536u)return 0;
    node=game_memory_read_u32(memory,outer+0xcu);
    for(index=0u;node;index++){
        if(++visits>65536u || memory->fault)return 0;
        if(!state_archive_record(memory,reading,marker,records,0xfedcba91u,node+0xdu,1u,1u,0) ||
            !state_archive_record(memory,reading,marker,records,0xfedcba91u,node+0xcu,1u,1u,0) ||
            !state_archive_record(memory,reading,marker,records,0xfedcba99u,node+0x18u,1u,64u,0))return 0;
        if(reading){
            game_memory_write_u32(memory,outer+0x10u,node);
            if(index+1u>=count)break;
            {
                GameAddress next=recyclable_inner_acquire(memory,0u,0u,0u,services->storage);
                if(!next)return 0;
                game_memory_write_u32(memory,node+4u,next);game_memory_write_u32(memory,next+8u,node);node=next;
            }
        }else node=game_memory_read_u32(memory,node+4u);
    }
    return !memory->fault;
}
static int archive_reset_container(GameMemory *memory,GameAddress owner,
    const GameplayContainerArchiveServices *services){
    uint32_t reset=game_memory_read_u32(memory,game_memory_read_u32(memory,owner)+4u);
    if(reset==0x453fe0u || reset==0x454870u || reset==0x4fd4f0u){
        if(!recyclable_container_reset(memory,owner,0u,0u,0u,0,reset==0x454870u,services->storage))return 0;
        return reset!=0x4fd4f0u || game_memory_fill_u32(memory,owner+0x20u,0x1000u,0u,0);
    }
    return services->reset_foreign && services->reset_foreign(services->context,owner);
}
int gameplay_state_stream_container(GameMemory *memory,int reading,uint32_t *marker,
    const GameplayContainerArchiveServices *services,GameAddress owner){
    GameAddress node,table=game_memory_read_u32(memory,owner);uint32_t count=0u,index,append;unsigned offset;
    const TaggedRecordStreamServices *records=services->records;
    if(reading){
        if(!archive_reset_container(memory,owner,services))return 0;
    }else for(node=game_memory_read_u32(memory,owner+4u);node;node=game_memory_read_u32(memory,node+4u)){
        if(++count>65536u || memory->fault)return 0;
    }
    for(offset=0xcu;offset<=0xdu;offset++)
        if(!state_archive_record(memory,reading,marker,records,0xfedcba91u,owner+offset,1u,1u,0))return 0;
    if(!state_archive_record(memory,reading,marker,records,0xfedcba92u,owner+0x10u,4u,1u,0))return 0;
    for(offset=0x14u;offset<=0x18u;offset+=4u)
        if(!state_archive_record(memory,reading,marker,records,0xfedcba93u,owner+offset,4u,1u,0))return 0;
    if(!gameplay_state_stream_outer(memory,reading,marker,services,game_memory_read_u32(memory,owner+0x1cu)) ||
        !state_archive_word(memory,reading,marker,records,&count))return 0;
    if(count>65536u)return 0;
    if(!reading)for(node=game_memory_read_u32(memory,owner+4u);node;node=game_memory_read_u32(memory,node+4u)){
        if(!gameplay_state_stream_outer(memory,0,marker,services,node))return 0;
    }else{
        append=game_memory_read_u32(memory,table);
        for(index=0u;index<count;index++){
            node=recyclable_outer_acquire(memory,0u,services->storage);
            if(!node || !gameplay_state_stream_outer(memory,1,marker,services,node))return 0;
            if(append==0x4542d0u || append==0x454a50u || append==0x4fd4a0u){
                if(!recyclable_container_insert(memory,owner,node,append==0x4fd4a0u?2:append==0x454a50u,services->storage))return 0;
            }else if(!services->append_foreign || !services->append_foreign(services->context,owner,node))return 0;
        }
    }
    return !memory->fault;
}

/* The two grid consumers borrow their established records and byte maps.
 * Only temporary file bytes live on the stack; no writable state mirror. */
int gameplay_state_stream_grid(GameMemory *memory,int reading,int alternate,uint32_t *marker,
    const GameplayContainerArchiveServices *services,GameAddress owner){
    unsigned offset,index;uint32_t base=alternate?0x2000u:0x1800u;
    const TaggedRecordStreamServices *records=services->records;
    if(reading){
        game_memory_write_u32(memory,owner+base,1u);
        if(alternate){
            for(offset=4u;offset<=0x24u;offset+=4u)game_memory_write_u32(memory,owner+base+offset,0u);
            if(!game_memory_fill_u32(memory,0x596f20u,16u,0u,0) ||
                !game_memory_fill_u32(memory,owner,0x800u,0u,0))return 0;
        }else{
            uint32_t last=game_memory_read_u32(memory,owner+0x12034u);
            game_memory_write_u32(memory,owner+0x1804u,0u);
            game_memory_write_u32(memory,owner+0x1808u,0x7f7fffffu);
            game_memory_write_u32(memory,owner+0x180cu,0x7fffffffu);
            game_memory_write_u32(memory,owner+0x11830u,0x40800000u);
            game_memory_write_u32(memory,owner+0x12038u,15u);
            if(last<0x80000000u){
                if(last>=100u)return 0;
                for(index=last+1u;index>0u;index--){
                    GameAddress node=game_memory_read_u32(memory,owner+0x1203cu+(index-1u)*4u);
                    if(node && !recyclable_outer_retire(memory,node,services->storage))return 0;
                }
            }
            if(!game_memory_fill_u32(memory,owner+0x1203cu,100u,0u,0) ||
                !game_memory_fill_u32(memory,owner,0x200u,0u,0))return 0;
            game_memory_write_u32(memory,owner+0x12034u,UINT32_MAX);
        }
        if(!archive_reset_container(memory,owner+base+(alternate?0x28u:0x10u),services))return 0;
    }
    for(offset=0u;offset<(alternate?0x1cu:0x10u);offset+=4u){
        uint32_t tag=!alternate && offset==8u?0xfedcba93u:0xfedcba92u;
        if(!state_archive_record(memory,reading,marker,records,tag,owner+base+offset,4u,1u,0))return 0;
    }
    if(!state_archive_record(memory,reading,marker,records,0xfedcba93u,
        owner+(alternate?0x2024u:0x11830u),4u,1u,0))return 0;
    if(!game_memory_read_u32(memory,owner+base+4u))return !memory->fault;
    if(alternate){
        if(!state_archive_record(memory,reading,marker,records,0xfedcba99u,owner,1u,0x2000u,0) ||
            !gameplay_state_stream_container(memory,reading,marker,services,owner+0x2028u))return 0;
        for(offset=0x6048u;offset<=0x6054u;offset+=4u)
            if(!state_archive_record(memory,reading,marker,records,0xfedcba92u,owner+offset,4u,1u,0))return 0;
    }else{
        uint32_t last;
        if(!gameplay_state_stream_container(memory,reading,marker,services,owner+0x1810u) ||
            !state_archive_record(memory,reading,marker,records,0xfedcba99u,owner,1u,0x800u,0) ||
            !state_archive_record(memory,reading,marker,records,0xfedcba99u,owner+0x800u,1u,0x1000u,0) ||
            !state_archive_record(memory,reading,marker,records,0xfedcba92u,owner+0x12034u,4u,1u,0) ||
            !state_archive_record(memory,reading,marker,records,0xfedcba92u,owner+0x12038u,4u,1u,0))return 0;
        last=game_memory_read_u32(memory,owner+0x12034u);
        if(last<0x80000000u){
            if(last>=100u)return 0;
            for(index=0u;index<=last;index++){
                GameAddress node=game_memory_read_u32(memory,owner+0x1203cu+index*4u);
                if(reading){
                    node=recyclable_outer_acquire(memory,0u,services->storage);
                    if(!node)return 0;
                    game_memory_write_u32(memory,owner+0x1203cu+index*4u,node);
                }
                if(!gameplay_state_stream_outer(memory,reading,marker,services,node))return 0;
            }
        }
    }
    return !memory->fault;
}

/* 45a9e0/45ab00: persist the existing indexed named records and linked entity
 * bindings. File counts/names are consumed on load; the live level determines
 * destination order exactly as in the original. No second entity owner. */
int gameplay_state_stream_entities(GameMemory *memory,int reading,uint32_t *marker,
    const TaggedRecordStreamServices *services,GameplayArchiveResolve resolve,
    void *context,GameplayEntityArchiveResult *result){
    uint32_t elements,index,visits=0u;unsigned group;GameAddress node;
    static const GameAddress count_fields[2]={0x605884u,0x605888u};
    static const GameAddress list_fields[2]={0x605870u,0x60586cu};
    uint8_t count_bytes[4];uint32_t count,header;
    if(!result || !marker)return 0;
    result->resolved_link=0;result->last_resolution=0u;
    if(!gameplay_state_stream_presentation(memory,reading,&elements,services))return 0;
    for(group=0u;group<2u;group++){
        if(!state_archive_record(memory,reading,marker,services,0xfedcba92u,count_fields[group],4u,1u,reading))return 0;
        for(index=0u;;index++){
            count=game_memory_read_u32(memory,count_fields[group]);
            if(memory->fault)return 0;
            if(!count || count>=0x80000000u || index>=count)break;
            if(index>=65536u){memory->fault=1;return 0;}
            node=game_memory_read_u32(memory,game_memory_read_u32(memory,list_fields[group])+index*4u);
            if(!state_archive_record(memory,reading,marker,services,0xfedcbaa0u,node,1u,32u,reading) ||
                !state_archive_record(memory,reading,marker,services,group?0xfedcba9bu:0xfedcba92u,node+0x20u,4u,group?3u:1u,0))return 0;
        }
    }
    node=game_memory_read_u32(memory,0x605868u);count=0u;
    if(!reading)while(node){
        if(++count>65536u){memory->fault=1;return 0;}
        node=game_memory_read_u32(memory,node+0x28u);if(memory->fault)return 0;
    }
    for(index=0u;index<4u;index++)count_bytes[index]=(uint8_t)(count>>(index*8u));
    if(reading){
        if(!tagged_record_stream_read_elements(marker,count_bytes,4u,1u,&header,&elements,services))return 0;
        game_memory_write_u32(memory,0x59d4b8u,header);
    }else if(!tagged_record_stream_write_elements(marker,0xfedcba92u,count_bytes,4u,1u,services))return 0;
    node=game_memory_read_u32(memory,0x605868u);
    while(node){
        uint32_t identity;GameAddress match=0u;
        if(++visits>65536u){memory->fault=1;return 0;}
        if(!state_archive_record(memory,reading,marker,services,0xfedcbaa0u,node,1u,32u,reading) ||
            !state_archive_record(memory,reading,marker,services,0xfedcba90u,node+0x24u,4u,1u,0) ||
            !state_archive_record(memory,reading,marker,services,0xfedcba91u,node+0x2cu,1u,1u,0))return 0;
        if(reading){
            identity=game_memory_read_u32(memory,node+0x24u);
            if(identity && (!resolve || !resolve(context,identity,&match)))return 0;
            game_memory_write_u32(memory,node+0x20u,match);result->resolved_link=1;result->last_resolution=match;
        }
        node=game_memory_read_u32(memory,node+0x28u);if(memory->fault)return 0;
    }
    return !memory->fault;
}

/* 45a6d0: restore the four established settings records into their existing
 * owners, then clear the local player's transient eighteen-word block. */
int gameplay_state_load_settings(GameMemory *memory,uint32_t *marker,
    const TaggedRecordStreamServices *services){
    static const uint32_t counts[4]={10u,17u,13u,32u};
    static const GameAddress roots[3]={0x5f0220u,0x5f0224u,0x5f0208u};
    static const uint32_t offsets[3]={0x29cu,0x1fcu,0x29cu};
    unsigned record,index;
    for(record=0u;record<4u;record++){
        GameAddress base=record?game_memory_read_u32(memory,roots[record-1u])+offsets[record-1u]:0x5ac678u;
        uint8_t bytes[128];uint32_t header,payload;
        for(index=0u;index<counts[record];index++){
            uint32_t value=game_memory_read_u32(memory,base+index*4u);unsigned byte;
            for(byte=0u;byte<4u;byte++)bytes[index*4u+byte]=(uint8_t)(value>>(byte*8u));
        }
        if(memory->fault || !tagged_record_stream_read_elements(marker,bytes,4u,counts[record],&header,&payload,services))return 0;
        game_memory_write_u32(memory,0x59d4b8u,header);
        for(index=0u;index<counts[record];index++){
            const uint8_t *value=bytes+index*4u;
            game_memory_write_u32(memory,base+index*4u,(uint32_t)value[0]|(uint32_t)value[1]<<8u|
                (uint32_t)value[2]<<16u|(uint32_t)value[3]<<24u);
        }
        if(memory->fault)return 0;
    }
    return game_memory_fill_u32(memory,game_memory_read_u32(memory,0x5f0208u)+0x5acu,18u,0u,0) && !memory->fault;
}

int gameplay_feedback_request(GameMemory *memory,const uint32_t values[3]){
    static const GameAddress fields[3]={0x608220u,0x608214u,0x608210u};unsigned i;
    if(!memory || !values)return 0;
    for(i=0;i<3u && !memory->fault;i++)game_memory_write_u32(memory,fields[i],values[i]);
    game_memory_write_u32(memory,0x608194u,0u);game_memory_write_u32(memory,0x6081c0u,1u);
    return !memory->fault;
}
#include "render_packet_view.h"
#include "display_transition.h"
#include <string.h>
#include <math.h>

static int32_t ui_signed(uint32_t bits){int32_t value;memcpy(&value,&bits,4u);return value;}
static int panel_limit(GameMemory *memory,GameAddress panel,uint32_t index){
    int32_t count=ui_signed(game_memory_read_u32(memory,panel));
    if(count>256){memory->fault=1;return 0;}
    return !memory->fault && (int32_t)index<count;
}
int gameplay_widget_kind(GameMemory *memory,GameAddress widget,uint32_t *kind){
    GameAddress table=game_memory_read_u32(memory,widget);
    uint32_t method=game_memory_read_u32(memory,table+4u);
    if(memory->fault)return -1;
    switch(method){
    case 0x416a30u:*kind=0u;break;
    case 0x416c30u:*kind=1u;break;
    case 0x416e00u:*kind=2u;break;
    default:return 0;
    }
    return 1;
}
int gameplay_panel_settle(GameMemory *memory,GameAddress panel,GameplayWidgetKindQuery query,void *context){
    uint32_t index;
    for(index=0u;panel_limit(memory,panel,index);index++){
        GameAddress slot=panel+4u+index*4u,widget=game_memory_read_u32(memory,slot);
        uint32_t kind=0u,delta;int status=gameplay_widget_kind(memory,widget,&kind);
        if(status<0)return 0;
        if(!status && (!query || !query(context,widget,&kind))){memory->fault=1;return 0;}
        if(kind!=2u)continue;
        /* A foreign query may replace the panel member. Reload its slot. */
        widget=game_memory_read_u32(memory,slot);delta=game_memory_read_u32(memory,widget+0x128u);
        game_memory_write_u32(memory,widget+0xa8u,game_memory_read_u32(memory,widget+0xa8u)+delta);
        game_memory_write_u32(memory,widget+0xb0u,game_memory_read_u32(memory,widget+0xb0u)+delta);
        game_memory_write_u32(memory,widget+0x128u,0u);
        delta=game_memory_read_u32(memory,widget+0x12cu);
        game_memory_write_u32(memory,widget+0xb4u,game_memory_read_u32(memory,widget+0xb4u)+delta);
        game_memory_write_u32(memory,widget+0xacu,game_memory_read_u32(memory,widget+0xacu)+delta);
        game_memory_write_u32(memory,widget+0x12cu,0u);
    }
    return !memory->fault;
}
int gameplay_selector_ranges(GameMemory *memory,int alternate){
    uint32_t index=0u;
    while(!memory->fault && (int32_t)index<ui_signed(game_memory_read_u32(memory,0x602618u))){
        uint32_t base=game_memory_read_u32(memory,0x6025f0u),limit=base+(alternate?5u:4u);
        GameAddress widget;
        int enabled=ui_signed(index)>=ui_signed(base) && ui_signed(index)<ui_signed(limit);
        if(index>=65536u){memory->fault=1;break;}
        widget=game_memory_read_u32(memory,(alternate?0x602500u:0x602570u)+index*4u);
        game_memory_write_u32(memory,widget+8u,(uint32_t)enabled);++index;
    }
    return !memory->fault;
}

static void selector_bounds(GameMemory *memory,GameAddress slot,uint32_t left,
    uint32_t bottom,uint32_t right,uint32_t top){
    GameAddress widget=game_memory_read_u32(memory,slot);
    game_memory_write_u32(memory,widget+0xa8u,left);
    game_memory_write_u32(memory,widget+0xacu,bottom);
    game_memory_write_u32(memory,widget+0xb0u,right);
    game_memory_write_u32(memory,widget+0xb4u,top);
}
static void selector_enable(GameMemory *memory,GameAddress slot,uint32_t enabled){
    GameAddress widget=game_memory_read_u32(memory,slot);
    game_memory_write_u32(memory,widget+8u,enabled);
    game_memory_write_u32(memory,widget+12u,enabled);
}
static float selector_float(uint32_t bits){float value;memcpy(&value,&bits,4u);return value;}
static uint32_t selector_scaled(uint32_t extent,uint32_t position){
    long double value=(long double)ui_signed(extent)*(long double)selector_float(position);
    int64_t integer=!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value;
    uint32_t bits=(uint32_t)(uint64_t)integer;
    return (bits>>17u)|((bits&0x80000000u)?0xffff8000u:0u);
}
/* 4a03b0: refresh either selector layout and the existing minimap records.
 * Widget/panel/map owners remain unchanged; no parallel UI representation. */
int gameplay_selector_refresh(GameMemory *memory,int alternate,GameplayRadarReveal reveal,void *context){
    static const struct {GameAddress address;uint32_t normal,alternate;} fields[]={
        {0x6025e0u,204u,403u},{0x6025e4u,125u,317u},{0x6025e8u,308u,628u},{0x6025ecu,189u,459u},
        {0x602528u,203u,119u},{0x602534u,7u,405u},{0x6027b8u,15u,26u},{0x6027bcu,15u,33u},
        {0x60252cu,35u,438u},{0x6026a8u,254u,496u},{0x6026b4u,68u,122u},{0x6026b0u,312u,625u},
        {0x6026acu,95u,189u},{0x6025f8u,273u,576u},{0x602604u,104u,212u},{0x602600u,298u,628u},
        {0x6025fcu,108u,223u},{0x6025c8u,205u,418u},{0x6025d4u,114u,79u},{0x6025ccu,120u,90u},
        {0x6024f0u,200u,386u},{0x6024f4u,54u,98u},{0x6024f8u,249u,478u},{0x6027c0u,184u,360u},
        {0x6027c4u,184u,367u},{0x602688u,290u,467u},{0x602694u,97u,212u},{0x602690u,311u,519u},
        {0x6025a8u,206u,110u},{0x6025acu,41u,455u},{0x602530u,306u,271u},{0x6025d0u,306u,593u},
        {0x6024fcu,101u,190u},{0x60268cu,101u,223u}
    };
    static const struct {GameAddress slot;uint32_t normal[4],alternate[4];} bounds[]={
        {0x6026a0u,{197u,27u,203u,15u},{101u,437u,117u,405u}},
        {0x6027e0u,{307u,27u,313u,15u},{272u,437u,288u,405u}},
        {0x602624u,{253u,64u,271u,54u},{439u,66u,477u,55u}},
        {0x6025b0u,{273u,64u,287u,54u},{488u,66u,523u,55u}},
        {0x6025d8u,{289u,64u,314u,54u},{534u,66u,573u,55u}}
    };
    uint32_t index,spacing=alternate?30u:25u;GameAddress panel;
    for(index=0u;index<sizeof(fields)/sizeof(fields[0]);index++)
        game_memory_write_u32(memory,fields[index].address,alternate?fields[index].alternate:fields[index].normal);
    panel=game_memory_read_u32(memory,alternate?0x60261cu:0x602620u);
    game_memory_write_u32(memory,0x602678u,panel);
    gameplay_state_assign_collection(memory,panel,game_memory_read_u32(memory,0x6025a8u),game_memory_read_u32(memory,0x6025acu));
    gameplay_state_assign_collection(memory,game_memory_read_u32(memory,0x60259cu),
        game_memory_read_u32(memory,0x6025a8u),game_memory_read_u32(memory,0x6025acu));
    for(index=0u;!memory->fault && ui_signed(index)<ui_signed(game_memory_read_u32(memory,0x602618u));index++){
        uint32_t left=game_memory_read_u32(memory,0x602528u)+spacing*index+1u;
        uint32_t top=game_memory_read_u32(memory,0x602534u)+1u;
        if(index>=65536u){memory->fault=1;break;}
        selector_bounds(memory,(alternate?0x602500u:0x602570u)+4u*index,left,top+(alternate?31u:26u),left+spacing-1u,top);
    }
    gameplay_state_offset_collection(memory,panel,0u-spacing*game_memory_read_u32(memory,0x6025f0u),0u);
    for(index=0u;index<2u;index++){
        const uint32_t *b=alternate?bounds[index].alternate:bounds[index].normal;
        selector_bounds(memory,bounds[index].slot,b[0],b[1],b[2],b[3]);
    }
    for(index=0u;index<4u;index++){
        uint32_t top=game_memory_read_u32(memory,0x6026b4u)+index*6u+1u;
        selector_bounds(memory,0x6027d0u+index*4u,game_memory_read_u32(memory,0x6026a8u)+1u,
            top+6u,game_memory_read_u32(memory,0x6026b0u)-1u,top);
    }
    selector_bounds(memory,0x6027c8u,game_memory_read_u32(memory,0x6025e0u),game_memory_read_u32(memory,0x6025ecu),
        game_memory_read_u32(memory,0x6025e8u),game_memory_read_u32(memory,0x6025e4u));
    for(index=2u;index<sizeof(bounds)/sizeof(bounds[0]);index++){
        const uint32_t *b=alternate?bounds[index].alternate:bounds[index].normal;
        selector_bounds(memory,bounds[index].slot,b[0],b[1],b[2],b[3]);
    }
    if(alternate)selector_bounds(memory,0x6025dcu,378u,395u,395u,365u);
    selector_enable(memory,0x6025dcu,alternate?1u:0u);
    selector_bounds(memory,alternate?0x602634u:0x602614u,alternate?381u:138u,alternate?68u:14u,alternate?403u:144u,alternate?51u:8u);
    selector_enable(memory,alternate?0x602634u:0x602614u,1u);
    {
        GameAddress source=game_memory_read_u32(memory,alternate?0x602614u:0x602634u);
        GameAddress destination=game_memory_read_u32(memory,alternate?0x602634u:0x602614u);
        game_memory_write_u32(memory,destination+4u,game_memory_read_u32(memory,source+4u));
    }
    selector_enable(memory,alternate?0x602614u:0x602634u,0u);
    gameplay_selector_ranges(memory,alternate);
    for(index=0u;!memory->fault && (int32_t)index<(int16_t)game_memory_read_u16(memory,0x639a74u);index++){
        GameAddress collection=game_memory_read_u32(memory,game_memory_read_u32(memory,0x639a70u)+4u*index);
        GameAddress position=game_memory_read_u32(memory,collection+0x1cu),record;
        uint32_t z=game_memory_read_u32(memory,position+8u),x=game_memory_read_u32(memory,position),right,bottom,y;
        if(x>0x80000000u || ui_signed(x)>0x48000000 || z>0x80000000u || ui_signed(z)>0x48000000)continue;
        bottom=game_memory_read_u32(memory,0x6027c4u);
        y=bottom-selector_scaled(bottom-game_memory_read_u32(memory,0x6027bcu),z)-1u;
        right=game_memory_read_u32(memory,0x6027c0u);
        record=game_memory_read_u32(memory,0x602698u)+index*12u;
        game_memory_write_u32(memory,record,right-selector_scaled(right-game_memory_read_u32(memory,0x6027b8u),x));
        game_memory_write_u32(memory,game_memory_read_u32(memory,0x602698u)+index*12u+4u,y);
        record=game_memory_read_u32(memory,0x602698u)+index*12u+8u;
        if(game_memory_read_u32(memory,record))continue;
        if(!game_memory_read_u32(memory,0x58de70u)){game_memory_write_u32(memory,record,1u);continue;}
        collection=game_memory_read_u32(memory,game_memory_read_u32(memory,0x639a70u)+4u*index);
        if(game_memory_read_u32(memory,collection+0x54u)==game_memory_read_u32(memory,0x5f0208u)){
            if(memory->fault || !reveal || !reveal(context,collection+0x48u,15000.0f)){memory->fault=1;return 0;}
        }
    }
    game_memory_write_u32(memory,0x6025a4u,game_memory_read_u32(memory,alternate?0x602560u:0x602564u));
    return !memory->fault;
}

static float queued_packet_depth(GameMemory *memory,GameAddress packet){
    GameAddress vertices=game_memory_read_u32(memory,packet+4u);
    uint32_t bits=game_memory_read_u32(memory,game_memory_read_u32(memory,vertices)+8u);
    float depth;memcpy(&depth,&bits,4u);return depth;
}
static void queued_packet_link(GameMemory *memory,GameAddress packet,GameAddress next){
    if(!memory->fault && !render_packet_set_next(packet,next))game_memory_write_u32(memory,packet+12u,next);
}
/* Sorted effect queue (4d94e0). Existing packet owners retain storage and links.
 * Equal depths preserve the original head and insert before the next equal;
 * unordered depths walk past candidates, matching the original x87 C0 test. */
int gameplay_state_queue_packet(GameMemory *memory,GameAddress packet){
    float depth=queued_packet_depth(memory,packet);
    GameAddress head=game_memory_read_u32(memory,0x604f08u),previous,next;
    if(memory->fault)return 0;
    if(!head){
        game_memory_write_u32(memory,0x604f08u,packet);queued_packet_link(memory,packet,0u);
        return !memory->fault;
    }
    if(queued_packet_depth(memory,head)>depth){
        queued_packet_link(memory,packet,head);
        if(!memory->fault)game_memory_write_u32(memory,0x604f08u,packet);
        return !memory->fault;
    }
    previous=head;next=game_memory_read_u32(memory,previous+12u);
    while(next && !memory->fault){
        if(queued_packet_depth(memory,next)>=depth)break;
        previous=next;next=game_memory_read_u32(memory,next+12u);
    }
    queued_packet_link(memory,packet,next);queued_packet_link(memory,previous,packet);
    return !memory->fault;
}

/*5533a0: the input attachment borrows its vehicle's owned model holders. The
 *cached matrices/angles restore when the vehicle counter exceeds160 and no
 *input is pending.
 *Queue flags, palette binding and voice publication use the original records. */
int gameplay_controller_activate_attachment(GameMemory *memory,GameAddress child,
    const GamePresentationFeedbackServices *feedback){
    GameAddress local,model,source,catalog;uint32_t elapsed,bits;float x,z;
    int accepted;unsigned i;int16_t sounds[8]={0,0,-1,-1,-1,-1,-1,-1};
    if(!memory || !feedback || child<0x264u || memory->fault)return 0;
    local=game_memory_read_u32(memory,0x5f0208u);
    if((game_memory_read_u32(memory,child-0x250u)&63u)==game_memory_read_u16(memory,local+8u)){
        bits=game_memory_read_u32(memory,child-0x24cu);memcpy(&x,&bits,4u);
        bits=game_memory_read_u32(memory,child-0x244u);memcpy(&z,&bits,4u);
        sounds[0]=(int16_t)game_presentation_location_token(memory,x,1);
        sounds[1]=(int16_t)game_presentation_location_token(memory,z,0);
        catalog=game_memory_read_u32(memory,0x628a90u);
        if(memory->fault || !feedback->clear_queue || !feedback->clear_queue(feedback->context,catalog) ||
           !feedback->submit_queue || !feedback->submit_queue(feedback->context,catalog,sounds,&accepted))return 0;
        if(accepted){
            local=game_memory_read_u32(memory,0x5f0208u);
            game_memory_write_u16(memory,local+0x598u,(uint16_t)sounds[0]);
            game_memory_write_u32(memory,local+0x58cu,game_memory_read_u32(memory,0x5ac6acu));
        }
    }
    /*558c20 clears directional flags, not the owned event list at+0c. */
    game_memory_write_u32(memory,child+4u,0u);game_memory_write_u32(memory,child+8u,0u);
    game_memory_write_u32(memory,child-0x70u,0u);
    game_memory_write_u16(memory,child+14u,0u);game_memory_write_u16(memory,child+16u,0u);
    game_memory_write_u8(memory,child-0x18cu,game_memory_read_u8(memory,child-0x18cu)|9u);
    elapsed=game_memory_read_u32(memory,child-0x138u);
    game_memory_write_u32(memory,child-0x74u,0u);game_memory_write_u32(memory,child-0x78u,1u);
    if(elapsed>160u && elapsed<0x80000000u){
        if(game_memory_read_u32(memory,child+0xd8u)){
            game_memory_write_u32(memory,child+4u,game_memory_read_u32(memory,child+4u)|0x20000u);
        }else{
            game_memory_write_u16(memory,child-0xaau,game_memory_read_u16(memory,child+0xd6u));
            game_memory_write_u16(memory,child-0xacu,game_memory_read_u16(memory,child+0xd4u));
            source=game_memory_read_u32(memory,child-0xb4u);
            for(i=0u;i<12u;i++)game_memory_write_u32(memory,source+4u+i*4u,game_memory_read_u32(memory,child+0xa4u+i*4u));
            source=game_memory_read_u32(memory,child-0xb0u);
            for(i=0u;i<12u;i++)game_memory_write_u32(memory,source+4u+i*4u,game_memory_read_u32(memory,child+0x74u+i*4u));
        }
    }
    catalog=game_memory_read_u32(memory,0x58973cu);model=game_memory_read_u32(memory,child-0x48u);
    game_memory_write_u32(memory,model+20u,game_memory_read_u32(memory,catalog+0x4a0u));
    model=game_memory_read_u32(memory,child-0x48u);game_memory_write_u32(memory,model+0xa8u,0u);
    return !memory->fault;
}
int gameplay_controller_reset_attachment(GameMemory *memory,GameAddress child,
    const GameplayControllerServices *services){
    GameAddress descriptor,model,source;unsigned i;
    if(!services || !services->stop_sound || !services->stop_sound(services->context,129))return 0;
    game_memory_write_u32(memory,child-0x70u,0u);
    game_memory_write_u16(memory,child+14u,0u);
    game_memory_write_u16(memory,child+16u,0u);
    game_memory_write_u8(memory,child-0x18cu,(game_memory_read_u8(memory,child-0x18cu)&0xfeu)|8u);
    descriptor=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,child-0x250u)&63u)*4u);
    game_memory_write_u32(memory,child-0x74u,0u);
    game_memory_write_u32(memory,child-0x78u,1u);
    model=game_memory_read_u32(memory,child-0x48u);
    game_memory_write_u32(memory,model+20u,game_memory_read_u32(memory,descriptor+0x1dcu));
    model=game_memory_read_u32(memory,child-0x48u);
    game_memory_write_u32(memory,model+0xa8u,1u);
    game_memory_write_u32(memory,child+4u,0u);
    game_memory_write_u32(memory,child+8u,0u);
    game_memory_write_u32(memory,child+0xd8u,0u);
    game_memory_write_u16(memory,child+0xd4u,game_memory_read_u16(memory,child-0xacu));
    game_memory_write_u16(memory,child+0xd6u,game_memory_read_u16(memory,child-0xaau));
    source=game_memory_read_u32(memory,child-0xb4u);
    for(i=0;i<12u;i++)game_memory_write_u32(memory,child+0xa4u+i*4u,game_memory_read_u32(memory,source+4u+i*4u));
    source=game_memory_read_u32(memory,child-0xb0u);
    for(i=0;i<12u;i++)game_memory_write_u32(memory,child+0x74u+i*4u,game_memory_read_u32(memory,source+4u+i*4u));
    return !memory->fault;
}
int gameplay_controller_refresh_selection(GameMemory *memory,GameAddress controller,
    const GameplayControllerServices *services){
    GameAddress scene,previous,child,method,camera;
    if(!services)return 0;
    scene=game_memory_read_u32(memory,controller+0x3bcu);
    previous=game_memory_read_u32(memory,controller+0x448u);
    if(scene!=previous){
        child=previous+0x264u;
        method=game_memory_read_u32(memory,game_memory_read_u32(memory,child)+16u);
        if(memory->fault)return 0;
        if(method==0x5535b0u){if(!gameplay_controller_reset_attachment(memory,child,services))return 0;}
        else if(!services->detach_external || !services->detach_external(services->context,child,method))return 0;
        scene=game_memory_read_u32(memory,controller+0x3bcu);
        game_memory_write_u32(memory,controller+0x448u,scene);
        game_memory_write_u32(memory,controller+0x450u,scene);
        if(controller==game_memory_read_u32(memory,0x5f0208u)){
            camera=game_memory_read_u32(memory,0x58f8a8u);
            if(camera && !game_camera_bind_scene(memory,camera,scene))return 0;
        }
    }
    game_memory_write_u32(memory,controller+0x5a0u,0u);
    if(memory->fault)return 0;
    if(controller!=game_memory_read_u32(memory,0x5f0208u))return !memory->fault;
    if(game_memory_read_u32(memory,0x604760u)==7u &&
        (!services->reset_application || !services->reset_application(services->context)))return 0;
    if(!services->play_sound || !services->play_sound(services->context,9,0.88f,1u))return 0;
    if(!game_camera_restore(memory,NULL))return 0;
    if(!services->change_mode || !services->change_mode(services->context,1u))return 0;
    if(!services->set_control_mode || !services->set_control_mode(services->context,0u))return 0;
    if(!services->play_sound(services->context,14,1.0f,0u))return 0;
    return services->show_selection_message && services->show_selection_message(services->context) && !memory->fault;
}

uint32_t gameplay_state_store_counter(GameMemory *memory, GameAddress self,
    uint32_t value) {
    game_memory_write_u32(memory, self + 0x22cu, value);
    return value;
}

uint32_t gameplay_state_copy_global(GameMemory *memory, GameAddress self) {
    uint32_t value = game_memory_read_u32(memory, 0x005ac6acu);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x114u, value);
    return value;
}

uint32_t gameplay_state_increment_indexed(GameMemory *memory, GameAddress self,
    int16_t index) {
    GameAddress address = self + 0x200u + (uint32_t)(int32_t)index * 4u;
    uint32_t value = game_memory_read_u32(memory, address) + 1u;
    if (!memory->fault) game_memory_write_u32(memory, address, value);
    return value;
}

int gameplay_state_counter_at_least(GameMemory *memory, GameAddress self,
    uint32_t value) {
    return (int32_t)game_memory_read_u32(memory, self + 0x22cu) >= (int32_t)value;
}

uint32_t gameplay_state_subtract_counter(GameMemory *memory, GameAddress self,
    uint32_t amount) {
    uint32_t value = game_memory_read_u32(memory, self + 0x22cu) - amount;
    if (!memory->fault) game_memory_write_u32(memory, self + 0x22cu, value);
    return value;
}

void gameplay_state_add_counters(GameMemory *memory, GameAddress self,
    uint32_t amount, uint32_t *primary, uint32_t *secondary) {
    *primary = game_memory_read_u32(memory, self + 0x22cu) + amount;
    *secondary = game_memory_read_u32(memory, self + 0x150u) + amount;
    if (!memory->fault) game_memory_write_u32(memory, self + 0x22cu, *primary);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x150u, *secondary);
}

int gameplay_state_entry_matches(GameMemory *memory, GameAddress self) {
    GameAddress entry = game_memory_read_u32(memory, self + 0x3c4u);
    if (memory->fault || entry == 0u) return 0;
    return game_memory_read_u32(memory, entry + 0x14u) ==
        game_memory_read_u32(memory, self + 0x3c8u);
}

void gameplay_state_publish_value(GameMemory *memory, GameAddress self) {
    uint32_t value = game_memory_read_u32(memory, self + 8u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00605890u, value);
}

int gameplay_state_flag_set(GameMemory *memory, GameAddress self, uint8_t mask) {
    return (game_memory_read_u8(memory, self + 0xd8u) & mask) != 0u;
}

uint32_t gameplay_state_class_slot(GameMemory *memory, GameAddress self,
    uint32_t slot_offset, int honor_disabled) {
    uint32_t index;
    if (honor_disabled && game_memory_read_u32(memory, 0x00594d3cu) != 0u)
        return 0u;
    index = game_memory_read_u32(memory, self + 0x64u);
    return game_memory_read_u32(memory, 0x005e4fc0u + index * 0xb0u + slot_offset);
}

void gameplay_state_clear_words(GameMemory *memory, GameAddress self,
    uint32_t offset, uint32_t count) {
    uint32_t index;
    for (index = 0u; index < count && !memory->fault; ++index)
        game_memory_write_u32(memory, self + offset + index * 4u, 0u);
}

uint16_t gameplay_state_update_angle(GameMemory *memory, GameAddress self,
    int *updated) {
    uint8_t state = game_memory_read_u8(memory, self + 0xd8u);
    uint16_t angle = 0u;
    *updated = (state & 1u) == 0u;
    if (*updated) {
        angle = (uint16_t)((game_memory_read_u16(memory, self + 0x148u) -
            game_memory_read_u16(memory, self + 0x24u)) & 0x07ffu);
        if (!memory->fault) game_memory_write_u16(memory, self + 0x14cu, angle);
    }
    return angle;
}

void gameplay_state_reset_actor(GameMemory *memory, GameAddress self) {
    static const uint16_t word_offsets[] = {0x148u, 0x14au, 0x14cu, 0x1b8u,
        0x1bau, 0x212u, 0x210u};
    static const uint16_t dword_offsets[] = {0x1e4u, 0x1e8u, 0x1f4u, 0x1f0u,
        0x1f8u, 0x1fcu, 0x13cu};
    unsigned index;
    for (index = 0u; index < sizeof(word_offsets) / sizeof(word_offsets[0]) && !memory->fault; ++index)
        game_memory_write_u16(memory, self + word_offsets[index], 0u);
    for (index = 0u; index < sizeof(dword_offsets) / sizeof(dword_offsets[0]) && !memory->fault; ++index)
        game_memory_write_u32(memory, self + dword_offsets[index], 0u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x1ecu, 1u);
}

uint32_t gameplay_state_increment_command(GameMemory *memory, GameAddress self,
    uint8_t command, int *matched) {
    uint32_t offset = command == 0x21u ? 0x510u : command == 0x27u ? 0x514u :
        command == 0x40u ? 0x50cu : 0u;
    uint32_t value = command;
    *matched = offset != 0u;
    if (offset != 0u) {
        value = game_memory_read_u32(memory, self + offset) + 1u;
        if (!memory->fault) game_memory_write_u32(memory, self + offset, value);
    }
    return value;
}

void gameplay_state_apply_palette(GameMemory *memory, GameAddress self,
    uint16_t palette_index) {
    uint8_t component[4];
    unsigned index;
    game_memory_write_u16(memory, self + 0x0au, palette_index);
    for (index = 0u; index < 4u && !memory->fault; ++index)
        component[index] = game_memory_read_u8(memory, 0x00589748u + palette_index * 4u + index);
    for (index = 0u; index < 8u && !memory->fault; ++index)
        game_memory_write_u8(memory, self + 0x0cu + index, component[index & 3u]);
}

void gameplay_state_reset_large_object(GameMemory *memory, GameAddress self) {
    static const uint16_t minus_one_offsets[] = {0x2fcu, 0x2dcu, 0x304u, 0x308u,
        0x300u, 0x30cu, 0x2f4u, 0x2f8u, 0x2f0u, 0x310u, 0x314u, 0x318u, 0x2ecu};
    static const uint16_t zero_offsets[] = {0x4c0u, 0x4bcu, 0x4b8u, 0x4b4u};
    unsigned index;
    for (index = 0u; index < sizeof(zero_offsets) / sizeof(zero_offsets[0]) && !memory->fault; ++index)
        game_memory_write_u32(memory, self + zero_offsets[index], 0u);
    gameplay_state_clear_words(memory, self, 0x29cu, 32u);
    for (index = 0u; index < sizeof(minus_one_offsets) / sizeof(minus_one_offsets[0]) && !memory->fault; ++index)
        game_memory_write_u32(memory, self + minus_one_offsets[index], 0xffffffffu);
}

static GameplayCollectionUpdateResult collection_result(GameMemory *memory,
        GameAddress collection, uint32_t processed) {
    GameplayCollectionUpdateResult result;
    result.processed = processed;
    result.final_count = game_memory_read_u32(memory, collection);
    return result;
}

GameplayCollectionUpdateResult gameplay_state_offset_collection(
    GameMemory *memory, GameAddress collection, uint32_t first_delta,
    uint32_t second_delta) {
    uint32_t processed = 0u;
    uint32_t count = game_memory_read_u32(memory, collection);
    while (!memory->fault && (int32_t)processed < (int32_t)count) {
        GameAddress entity = game_memory_read_u32(memory,
            collection + 4u + processed * 4u);
        uint32_t value;
        value = game_memory_read_u32(memory, entity + 0xa8u);
        if (!memory->fault)
            game_memory_write_u32(memory, entity + 0xa8u,
                value + first_delta);
        value = game_memory_read_u32(memory, entity + 0xb0u);
        if (!memory->fault)
            game_memory_write_u32(memory, entity + 0xb0u,
                value + first_delta);
        value = game_memory_read_u32(memory, entity + 0xb4u);
        if (!memory->fault)
            game_memory_write_u32(memory, entity + 0xb4u,
                value + second_delta);
        value = game_memory_read_u32(memory, entity + 0xacu);
        if (!memory->fault)
            game_memory_write_u32(memory, entity + 0xacu,
                value + second_delta);
        processed++;
        count = game_memory_read_u32(memory, collection);
    }
    return collection_result(memory, collection, processed);
}

GameplayCollectionUpdateResult gameplay_state_assign_collection(
    GameMemory *memory, GameAddress collection, uint32_t first_value,
    uint32_t second_value) {
    uint32_t processed = 0u;
    uint32_t count = game_memory_read_u32(memory, collection);
    while (!memory->fault && (int32_t)processed < (int32_t)count) {
        GameAddress entity = game_memory_read_u32(memory,
            collection + 4u + processed * 4u);
        if (!memory->fault)
            game_memory_write_u32(memory, entity + 0xa0u, first_value);
        if (!memory->fault)
            game_memory_write_u32(memory, entity + 0xa4u, second_value);
        processed++;
        count = game_memory_read_u32(memory, collection);
    }
    return collection_result(memory, collection, processed);
}

uint32_t gameplay_state_advance_ping_pong(GameMemory *memory,
        GameAddress object) {
    GameAddress rows = game_memory_read_u32(memory, object + 0x38u);
    GameAddress limits = game_memory_read_u32(memory, object + 0x3cu);
    GameAddress values = game_memory_read_u32(memory, object + 0x40u);
    GameAddress deltas = game_memory_read_u32(memory, object + 0x44u);
    GameAddress entries = game_memory_read_u32(memory, object + 0x18u);
    uint32_t count = game_memory_read_u32(memory, object + 0x1cu);
    uint32_t processed = 0u;
    if (memory->fault || rows == 0u || (int32_t)(count - 1u) < 0)
        return 0u;
    while (processed < count && !memory->fault) {
        uint32_t index = count - processed - 1u;
        int16_t value = (int16_t)game_memory_read_u16(memory,
            values + index * 2u);
        int8_t delta = (int8_t)game_memory_read_u8(memory, deltas + index);
        int16_t limit = (int16_t)game_memory_read_u16(memory,
            limits + index * 2u);
        value = (int16_t)(value + delta);
        if (value < 0) {
            value = 0;
            delta = 1;
        } else if ((int32_t)limit - 1 < (int32_t)value) {
            value = (int16_t)(limit - 1);
            delta = -1;
        }
        game_memory_write_u16(memory, values + index * 2u,
            (uint16_t)value);
        game_memory_write_u8(memory, deltas + index, (uint8_t)delta);
        if (!memory->fault) {
            GameAddress row = game_memory_read_u32(memory,
                rows + index * 4u);
            GameAddress output = game_memory_read_u32(memory,
                entries + index * 40u + 0x14u);
            uint32_t selected = game_memory_read_u32(memory,
                row + (uint32_t)(int32_t)value * 4u);
            if (!memory->fault)
                game_memory_write_u32(memory, output + 4u, selected);
        }
        processed++;
    }
    return processed;
}

static void copy_rotating_text(GameMemory *memory, GameAddress source,
        GameAddress destination, int backwards) {
    uint32_t remaining = 0xffffffffu;
    uint32_t length;
    uint32_t count;
    int32_t byte_step = backwards ? -1 : 1;
    int32_t word_step = backwards ? -4 : 4;
    for (;;) {
        uint8_t value = game_memory_read_u8(memory, source);
        source = (GameAddress)(source + byte_step);
        remaining--;
        if (memory->fault || value == 0u) break;
    }
    if (memory->fault) return;
    length = ~remaining;
    source -= length;
    count = length >> 2u;
    while (count-- != 0u && !memory->fault) {
        uint32_t value = game_memory_read_u32(memory, source);
        if (!memory->fault)
            game_memory_write_u32(memory, destination, value);
        source = (GameAddress)(source + word_step);
        destination = (GameAddress)(destination + word_step);
    }
    count = length & 3u;
    while (count-- != 0u && !memory->fault) {
        uint8_t value = game_memory_read_u8(memory, source);
        if (!memory->fault)
            game_memory_write_u8(memory, destination, value);
        source = (GameAddress)(source + byte_step);
        destination = (GameAddress)(destination + byte_step);
    }
}

GameplayRotatingTextResult gameplay_state_update_rotating_text(
        GameMemory *memory, GameAddress object, GameAddress destination,
        int backwards) {
    GameplayRotatingTextResult result;
    uint32_t current[3];
    uint32_t previous[3];
    unsigned group;
    result.destination = destination;
    result.changed = 0;
    for (group = 0u; group < 3u; ++group) {
        current[group] = game_memory_read_u32(memory,
            object + 0x500u + group * 4u);
        previous[group] = game_memory_read_u32(memory,
            object + 0x50cu + group * 4u);
    }
    result.current_primary = current[0];
    result.previous_tertiary = previous[2];
    result.current_index = current[2];
    if (memory->fault) return result;
    for (group = 0u; group < 3u; ++group) {
        if (previous[group] != current[group]) {
            GameAddress source = object + group * 0x180u +
                current[group] * 0x80u;
            result.changed = 1;
            result.current_index = current[group];
            copy_rotating_text(memory, source, destination, backwards);
            if (!memory->fault)
                game_memory_write_u32(memory,
                    object + 0x500u + group * 4u,
                    current[group] == 2u ? 0u : current[group] + 1u);
            return result;
        }
    }
    game_memory_write_u8(memory, destination, 0u);
    return result;
}

GameplaySlotReplacementResult gameplay_state_replace_entity_slot(
        GameMemory *memory, GameAddress manager, GameAddress target) {
    GameplaySlotReplacementResult result;
    int32_t count = (int32_t)(int16_t)game_memory_read_u16(memory,
        0x00639a74u);
    int32_t index;
    unsigned row;
    unsigned column;
    result.kind = GAMEPLAY_SLOT_NOT_FOUND;
    result.replacement = game_memory_read_u32(memory, target + 0x10cu);
    result.primary_table = game_memory_read_u32(memory, manager + 0x2c8u);
    result.empty_map = game_memory_read_u32(memory, manager + 0x30cu);
    result.index = 0u;
    if (memory->fault) return result;
    for (index = count * 8 - 1; index >= 0 && !memory->fault; --index) {
        GameAddress entry = result.primary_table + (uint32_t)index * 4u;
        if (game_memory_read_u32(memory, entry) == target) {
            game_memory_write_u32(memory, entry, result.replacement);
            result.index = (uint32_t)index;
            if (result.replacement == 0u) {
                game_memory_write_u32(memory,
                    result.empty_map + (uint32_t)index * 4u, 0u);
                result.kind = GAMEPLAY_SLOT_PRIMARY_CLEARED;
            } else {
                result.kind = GAMEPLAY_SLOT_PRIMARY;
            }
            return result;
        }
    }
    for (row = 0u; row < 4u && !memory->fault; ++row) {
        for (column = 0u; column < 4u && !memory->fault; ++column) {
            uint32_t linear = row * 4u + column;
            GameAddress entry = manager + 0x2ccu + linear * 4u;
            if (game_memory_read_u32(memory, entry) == target) {
                game_memory_write_u32(memory, entry, result.replacement);
                result.kind = GAMEPLAY_SLOT_FALLBACK;
                result.index = linear;
                return result;
            }
        }
    }
    return result;
}

static void detach_link_pair(GameMemory *memory, GameAddress self,
        uint32_t previous_offset, uint32_t next_offset, int clear_previous) {
    GameAddress previous = game_memory_read_u32(memory,
        self + previous_offset);
    GameAddress next = game_memory_read_u32(memory, self + next_offset);
    if (next != 0u)
        game_memory_write_u32(memory, next + previous_offset, previous);
    if (previous != 0u)
        game_memory_write_u32(memory, previous + next_offset, next);
    game_memory_write_u32(memory, self + next_offset, 0u);
    if (clear_previous)
        game_memory_write_u32(memory, self + previous_offset, 0u);
}

GameplayEntityResetPlan gameplay_state_prepare_entity_reset(
        GameMemory *memory, GameAddress self) {
    GameplayEntityResetPlan plan;
    uint32_t flags = game_memory_read_u32(memory, self + 0x10u);
    uint32_t index = game_memory_read_u32(memory, self + 0x14u) & 0x3fu;
    plan.selector = game_memory_read_u8(memory, self + 0x0cu);
    plan.owner = game_memory_read_u32(memory, 0x005f0210u + index * 4u);
    plan.target = game_memory_read_u32(memory,
        game_memory_read_u32(memory, plan.owner) + 0x68u);
    game_memory_write_u32(memory, self + 0x58u, 0u);
    game_memory_write_u32(memory, self + 0x10u, flags & 0xff11ffffu);
    detach_link_pair(memory, self, 4u, 8u, 1);
    detach_link_pair(memory, self, 0x48u, 0x4cu, 0);
    return plan;
}

GameplayEntityRelinkResult gameplay_state_finish_entity_reset(
        GameMemory *memory, GameAddress self) {
    GameplayEntityRelinkResult result;
    result.head = game_memory_read_u32(memory, self + 0x50u);
    result.old_first = game_memory_read_u32(memory, result.head + 4u);
    game_memory_write_u16(memory, self + 0x24u, 0u);
    game_memory_write_u32(memory, self + 0x20u, 0u);
    game_memory_write_u16(memory, self + 0x26u, 0u);
    game_memory_write_u32(memory, self + 0x1cu, 0u);
    game_memory_write_u16(memory, self + 0x28u, 0u);
    game_memory_write_u32(memory, self + 0x18u, 0u);
    game_memory_write_u32(memory, self + 0x14u, 0u);
    game_memory_write_u32(memory, self + 8u, result.head);
    game_memory_write_u32(memory, self + 4u, result.old_first);
    if (result.old_first != 0u)
        game_memory_write_u32(memory, result.old_first + 8u, self);
    game_memory_write_u32(memory, result.head + 4u, self);
    return result;
}

static int32_t signed_scale_ninth(uint32_t value, uint32_t factor,
        uint32_t *remainder) {
    int32_t product = (int32_t)(value * factor);
    int32_t quotient = product / 9;
    if (remainder) *remainder = (uint32_t)(product % 9);
    return quotient;
}

GameplayWeightedSlotResult gameplay_state_apply_weighted_slot(
        GameMemory *memory, GameAddress object, uint32_t slot,
        uint32_t candidate, uint32_t first_weight, uint32_t second_weight,
        uint32_t increment) {
    GameplayWeightedSlotResult result;
    GameAddress slot_address = object + 0x178u + slot * 4u;
    int32_t source_weight;
    uint32_t factor;
    result.slot = slot;
    result.final_remainder = 0u;
    result.increment = increment;
    if (candidate == 0xffffffffu) {
        game_memory_write_u32(memory, object + 0x184u, 0u);
        game_memory_write_u32(memory, object + 0x188u, 0u);
        game_memory_write_u32(memory, object + 0x18cu, 0u);
        game_memory_write_u32(memory, slot_address, 0u);
        return result;
    }
    source_weight = (int32_t)game_memory_read_u32(memory, object + 0x190u);
    factor = 10u - (uint32_t)(source_weight < 10 ? source_weight : 9);
    game_memory_write_u32(memory, object + 0x184u,
        game_memory_read_u32(memory, object + 0x184u) +
        (uint32_t)signed_scale_ninth(candidate, factor, 0));
    game_memory_write_u32(memory, object + 0x188u,
        game_memory_read_u32(memory, object + 0x188u) +
        (uint32_t)signed_scale_ninth(first_weight, factor, 0));
    game_memory_write_u32(memory, object + 0x18cu,
        game_memory_read_u32(memory, object + 0x18cu) +
        (uint32_t)signed_scale_ninth(second_weight, factor,
            &result.final_remainder));
    game_memory_write_u32(memory, slot_address,
        game_memory_read_u32(memory, slot_address) + increment);
    return result;
}

static void clear_entity_bucket(GameMemory *memory, GameAddress destination,
        int32_t count, int backwards) {
    int32_t index;
    GameAddress address = destination;
    for (index = 0; index < count; ++index) {
        game_memory_write_u32(memory, address, 0u);
        address += backwards ? (GameAddress)-4 : 4u;
    }
}

GameplayEntityBucketRefreshResult gameplay_state_refresh_entity_buckets(
        GameMemory *memory, GameAddress object, int forced, int backwards) {
    GameplayEntityBucketRefreshResult result;
    GameAddress destinations[3];
    GameAddress entity_table;
    uint32_t matching_class;
    int32_t index;
    result.tick = game_memory_read_u32(memory, 0x005ac6acu);
    result.timer = game_memory_read_u32(memory, object + 0x88u);
    result.count = (int32_t)(int16_t)game_memory_read_u16(memory, 0x00639a74u);
    result.refreshed = forced || (int32_t)result.timer <= (int32_t)result.tick;
    if (!result.refreshed) return result;

    game_memory_write_u32(memory, object + 0x88u, result.tick + 0x140u);
    destinations[0] = game_memory_read_u32(memory, object + 0x68u);
    destinations[1] = game_memory_read_u32(memory, object + 0x6cu);
    destinations[2] = game_memory_read_u32(memory, object + 0x70u);
    clear_entity_bucket(memory, destinations[0], result.count, backwards);
    clear_entity_bucket(memory, destinations[1], result.count, backwards);
    clear_entity_bucket(memory, destinations[2], result.count, backwards);
    game_memory_write_u32(memory, object + 0x78u, 0u);
    game_memory_write_u32(memory, object + 0x7cu, 0u);
    game_memory_write_u32(memory, object + 0x80u, 0u);
    if (result.count <= 0) return result;

    entity_table = game_memory_read_u32(memory, 0x00639a70u);
    matching_class = game_memory_read_u32(memory, 0x0058973cu);
    for (index = 0; index < result.count; ++index) {
        GameAddress entity = game_memory_read_u32(memory,
            entity_table + (uint32_t)index * 4u);
        uint32_t classification = game_memory_read_u32(memory, entity + 0x54u);
        unsigned bucket = classification == 0u ? 0u :
            (classification == matching_class ? 1u : 2u);
        GameAddress counter = object + (bucket == 0u ? 0x7cu :
            (bucket == 1u ? 0x78u : 0x80u));
        game_memory_write_u32(memory, destinations[bucket] +
            (uint32_t)index * 4u, entity);
        game_memory_write_u32(memory, counter,
            game_memory_read_u32(memory, counter) + 1u);
    }
    return result;
}

GameplayCalendarExpansionResult gameplay_state_expand_calendar(
        GameMemory *memory, uint32_t packed) {
    GameplayCalendarExpansionResult result;
    uint32_t four_years = packed / 0x07861f80u;
    uint32_t remainder = packed - four_years * 0x07861f80u;
    uint32_t table = 0x005a33c0u;
    uint32_t month = 1u;
    result.year = four_years * 4u + 0x46u;
    if (remainder >= 0x01e13380u) {
        ++result.year;
        remainder -= 0x01e13380u;
        if (remainder >= 0x01e13380u) {
            ++result.year;
            remainder -= 0x01e13380u;
            if (remainder >= 0x01e28500u) {
                ++result.year;
                remainder -= 0x01e28500u;
            } else {
                table = 0x005a3388u;
            }
        }
    }
    result.day_of_year = remainder / 0x00015180u;
    remainder -= result.day_of_year * 0x00015180u;
    while ((int32_t)game_memory_read_u32(memory, table + month * 4u) <
            (int32_t)result.day_of_year)
        ++month;
    result.month = month - 1u;
    result.day_of_month = result.day_of_year -
        game_memory_read_u32(memory, table + result.month * 4u);
    result.weekday = ((packed / 0x00015180u) + 4u) % 7u;
    result.hour = remainder / 0x00000e10u;
    result.seconds_within_hour = remainder - result.hour * 0x00000e10u;
    result.minute = result.seconds_within_hour / 60u;
    result.second = result.seconds_within_hour - result.minute * 60u;
    game_memory_write_u32(memory, 0x005a270cu, result.year);
    game_memory_write_u32(memory, 0x005a2714u, result.day_of_year);
    game_memory_write_u32(memory, 0x005a2708u, result.month);
    game_memory_write_u32(memory, 0x005a2704u, result.day_of_month);
    game_memory_write_u32(memory, 0x005a2710u, result.weekday);
    game_memory_write_u32(memory, 0x005a2700u, result.hour);
    game_memory_write_u32(memory, 0x005a26fcu, result.minute);
    game_memory_write_u32(memory, 0x005a26f8u, result.second);
    game_memory_write_u32(memory, 0x005a2718u, 0u);
    return result;
}

GameplayPackedCalendarResult gameplay_state_pack_calendar(
        GameMemory *memory, const uint32_t arguments[11]) {
    GameplayPackedCalendarResult result;
    uint32_t output_kind = arguments[0];
    uint32_t calculation_kind = arguments[1];
    uint32_t year = arguments[2];
    uint32_t table_index = arguments[3];
    uint32_t calendar_index;
    uint32_t base;
    uint32_t leap_selector = year & 3u;
    result.local_value = 0u;
    result.wrote_local = 0;
    if (calculation_kind == 1u) {
        uint32_t table = leap_selector == 0u ? 0x005a3384u : 0x005a33bcu;
        int32_t leap_days = ((int32_t)(year - 1u)) >> 2;
        uint32_t days = year * 365u + (uint32_t)leap_days;
        int32_t weekday_remainder = (int32_t)(days +
            game_memory_read_u32(memory, table + table_index * 4u) + 1u -
            25563u) % 7;
        uint32_t weeks;
        if ((int32_t)arguments[5] > weekday_remainder) {
            result.local_value = arguments[4] - 1u;
            result.wrote_local = 1;
            weeks = result.local_value * 7u;
        } else {
            weeks = arguments[4] * 7u;
        }
        calendar_index = game_memory_read_u32(memory,
            table + table_index * 4u) + 1u + weeks -
            (uint32_t)weekday_remainder + arguments[5];
        if (arguments[4] == 5u) {
            uint32_t limit_table = leap_selector == 0u ?
                0x005a3388u : 0x005a33c0u;
            if ((int32_t)calendar_index > (int32_t)game_memory_read_u32(
                    memory, limit_table + table_index * 4u))
                calendar_index -= 7u;
        }
    } else {
        uint32_t table = leap_selector == 0u ? 0x005a3384u : 0x005a33bcu;
        calendar_index = game_memory_read_u32(memory,
            table + table_index * 4u) + arguments[6];
    }
    base = ((arguments[7] * 60u + arguments[8]) * 60u + arguments[9]);
    if (output_kind == 1u) {
        result.packed_scale_125 = base * 125u;
        result.raw_packed = base * 1000u + arguments[10];
        result.packed = result.raw_packed;
        game_memory_write_u32(memory, 0x005a26dcu, calendar_index);
        game_memory_write_u32(memory, 0x005a26d8u, year);
        game_memory_write_u32(memory, 0x005a26e0u, result.packed);
    } else {
        base += game_memory_read_u32(memory, 0x005a2640u);
        result.packed_scale_125 = base * 125u;
        result.raw_packed = base * 1000u + arguments[10];
        result.packed = result.raw_packed;
        if ((int32_t)result.packed < 0)
            result.packed += 0x05265c00u - 1u;
        else if ((int32_t)result.packed >= (int32_t)0x05265c00u)
            result.packed -= 0x05265c00u - 1u;
        game_memory_write_u32(memory, 0x005a26ecu, calendar_index);
        game_memory_write_u32(memory, 0x005a26f0u, result.packed);
        game_memory_write_u32(memory, 0x005a26e8u, year);
    }
    result.calendar_index = calendar_index;
    return result;
}

GameplaySubmissionSample gameplay_state_prepare_submission_sample(
        GameMemory *memory, unsigned mode) {
    static const GameAddress tables[7] = {
        0x00639ad0u,0x00639ae0u,0x00639abcu,0x00639ad8u,
        0x00639adcu,0x00639ae4u,0x00639ab8u
    };
    GameplaySubmissionSample sample;
    sample.original_cursor = game_memory_read_u32(memory, 0x006308c0u);
    sample.cursor = sample.original_cursor == 0x400u ?
        0u : sample.original_cursor;
    sample.next_cursor = sample.cursor + 1u;
    sample.scalar = 0x006308c8u + sample.cursor * 4u;
    sample.table = mode < 7u ? tables[mode] : 0u;
    sample.scaled = mode == 0u;
    return sample;
}

void gameplay_state_commit_submission_sample(GameMemory *memory,
        const GameplaySubmissionSample *sample) {
    game_memory_write_u32(memory, 0x006308c0u, sample->next_cursor);
}

uint32_t gameplay_state_submission_table_value(GameMemory *memory,
        unsigned mode, uint32_t index) {
    static const GameAddress tables[7] = {
        0x00639ad0u,0x00639ae0u,0x00639abcu,0x00639ad8u,
        0x00639adcu,0x00639ae4u,0x00639ab8u
    };
    if (mode >= 7u) return 0u;
    return game_memory_read_u32(memory, tables[mode] + index * 4u);
}

GameplayVirtualCallPlan gameplay_state_virtual_call_plan(GameMemory *memory,
        GameAddress object, uint32_t slot) {
    GameplayVirtualCallPlan plan;
    plan.vtable = game_memory_read_u32(memory, object);
    plan.target = game_memory_read_u32(memory, plan.vtable + slot);
    return plan;
}

int gameplay_state_low_flag_set(uint32_t value) {
    return (value & 1u) != 0u;
}

int gameplay_state_refresh_due(GameMemory *memory, GameAddress object,
        uint32_t interval) {
    uint32_t tick = game_memory_read_u32(memory, 0x005ac6acu);
    uint32_t previous = game_memory_read_u32(memory, object + 0xb4u);
    return tick - previous == interval;
}

GameAddress gameplay_state_link_next(GameMemory *memory, GameAddress node,
        uint32_t offset) {
    return game_memory_read_u32(memory, node + offset);
}

uint32_t gameplay_state_merge_entity_flags(GameMemory *memory,
        GameAddress entity, uint32_t flags) {
    uint32_t merged = game_memory_read_u32(memory, entity) | flags;
    if (!memory->fault) game_memory_write_u32(memory, entity, merged);
    return merged;
}

void gameplay_state_publish_entity_grid(GameMemory *memory,
        GameAddress output, int16_t x, int16_t y, GameAddress entity) {
    game_memory_write_u32(memory, output + 4u, (uint32_t)(int32_t)x);
    if (!memory->fault)
        game_memory_write_u32(memory, output + 8u, (uint32_t)(int32_t)y);
    if (!memory->fault) game_memory_write_u32(memory, output + 0x14u, entity);
}

GameplayReleasePlan gameplay_state_release_plan(GameMemory *memory,
        GameAddress object) {
    GameplayReleasePlan plan;
    plan.active = game_memory_read_u32(memory, object + 0xd4u);
    plan.handle = 0u;
    plan.target = 0u;
    if (!memory->fault && plan.active != 0u) {
        plan.handle = game_memory_read_u32(memory, object + 0x50u);
        plan.target = game_memory_read_u32(memory, 0x0063ca2cu);
    }
    return plan;
}

int gameplay_state_cached_link_matches(GameMemory *memory,
        GameAddress cached, uint32_t generation) {
    return cached != 0u &&
        game_memory_read_u32(memory, cached + 0x14u) == generation;
}

GameplayComponentLookupPlan gameplay_state_component_lookup_plan(
        GameMemory *memory, GameAddress object) {
    GameplayComponentLookupPlan plan;
    plan.raw_group = game_memory_read_u32(memory, object + 0x14u);
    plan.group = plan.raw_group & 0x3fu;
    plan.type = game_memory_read_u8(memory, object + 0x0cu);
    plan.provider = game_memory_read_u32(memory,
        0x005f0210u + plan.group * 4u);
    plan.lookup = gameplay_state_virtual_call_plan(memory, plan.provider,
        0x4cu);
    return plan;
}

GameplayVirtualCallPlan gameplay_state_component_check_plan(
        GameMemory *memory, GameAddress object) {
    return gameplay_state_virtual_call_plan(memory, object, 0x60u);
}

uint16_t gameplay_state_component_word(GameMemory *memory,
        GameAddress component, uint32_t live_offset, uint32_t fallback_offset,
        int fallback) {
    return game_memory_read_u16(memory, component +
        (fallback ? fallback_offset : live_offset));
}

uint32_t gameplay_state_component_value(GameMemory *memory,
        GameAddress component, uint32_t live_offset,
        uint32_t fallback_offset, int fallback) {
    return game_memory_read_u32(memory, component +
        (fallback ? fallback_offset : live_offset));
}

GameplayVirtualCallPlan gameplay_state_status_forward_plan(
        GameMemory *memory, GameAddress object, uint32_t required_state,
        uint32_t member_offset, uint32_t slot) {
    GameplayVirtualCallPlan plan = {0u, 0u};
    if (game_memory_read_u32(memory, object + 0x10u) == required_state) {
        GameAddress member = game_memory_read_u32(memory,
            object + member_offset);
        plan = gameplay_state_virtual_call_plan(memory, member, slot);
    }
    return plan;
}

GameplayViewportLayoutPlan gameplay_state_viewport_layout_plan(
        GameMemory *memory, uint32_t selector, int sibling) {
    GameplayViewportLayoutPlan plan;
    GameAddress anchor_address;
    plan.selector = selector;
    plan.width = sibling ? 0u :
        game_memory_read_u32(memory, 0x00627928u);
    plan.enabled = game_memory_read_u32(memory,
        sibling ? 0x00589ac0u : 0x0058404cu);
    plan.lower = sibling ? (selector == 0u ? 0x115u : 0x242u) :
        plan.width - (selector == 0u ? 0x2bu : 0x3eu);
    plan.upper = sibling ? (selector == 0u ? 0x13eu : 0x27eu) :
        plan.width - 2u;
    plan.span = selector == 0u ? 0x92u : 0x14bu;
    plan.update_span = plan.enabled == 0u;
    plan.early_zero_layout = selector == 0u && plan.enabled == 0u;
    anchor_address = sibling ?
        (selector == 0u ? 0x005f0238u : 0x005f0234u) :
        (selector == 0u ? 0x005aced0u : 0x005aced8u);
    plan.end = plan.update_span ?
        game_memory_read_u32(memory, anchor_address) + plan.span : 0u;
    return plan;
}

void gameplay_state_apply_viewport_layout(GameMemory *memory,
        const GameplayViewportLayoutPlan *plan, int sibling) {
    GameAddress base = sibling ? 0x005f0240u : 0x005acec0u;
    game_memory_write_u32(memory, base, plan->lower);
    if (!memory->fault) game_memory_write_u32(memory, base + 8u,
        plan->upper);
    if (plan->update_span && !memory->fault) {
        game_memory_write_u32(memory, base + 4u, plan->span);
        if (!memory->fault) game_memory_write_u32(memory, base + 0x0cu,
            plan->end);
    }
}

static uint32_t clamp_signed_viewport_coordinate(uint32_t value,
        uint32_t origin, uint32_t extent, uint32_t *compared) {
    uint32_t relative = value - origin;
    uint32_t maximum = extent - 1u;
    if ((int32_t)relative <= 0) relative = 0u;
    *compared = relative;
    return (int32_t)relative >= (int32_t)maximum ? maximum : relative;
}

GameplayViewportClampResult gameplay_state_clamp_viewport_values(
        GameMemory *memory,uint32_t x,uint32_t y) {
    GameplayViewportClampResult result;
    uint32_t compared_x;
    result.x = clamp_signed_viewport_coordinate(
        x,
        game_memory_read_u32(memory, 0x00597f70u),
        game_memory_read_u32(memory, 0x00597f68u), &compared_x);
    result.maximum_y = game_memory_read_u32(memory, 0x00597f6cu) - 1u;
    result.y = clamp_signed_viewport_coordinate(
        y,
        game_memory_read_u32(memory, 0x00597f74u),
        game_memory_read_u32(memory, 0x00597f6cu), &result.compared_y);
    return result;
}
GameplayViewportClampResult gameplay_state_clamp_viewport_point(
        GameMemory *memory, GameAddress point) {
    GameplayViewportClampResult result=gameplay_state_clamp_viewport_values(memory,
        game_memory_read_u32(memory,point),game_memory_read_u32(memory,point+4u));
    game_memory_write_u32(memory, point, result.x);
    if (!memory->fault) game_memory_write_u32(memory, point + 4u, result.y);
    return result;
}

int gameplay_state_timed_update_due(uint32_t timer) {
    return (int32_t)timer > 0;
}

GameAddress gameplay_state_saved_exception_handler(GameMemory *memory,
        GameAddress frame) {
    return game_memory_read_u32(memory, frame - 0x0cu);
}

GameplayModeTransitionPlan gameplay_state_apply_mode(GameMemory *memory,
        GameAddress object, uint32_t mode) {
    GameplayModeTransitionPlan plan = {0u, 0u, 0u};
    game_memory_write_u32(memory, object + 0xb0u, mode);
    if (memory->fault || mode != 0x65u) return plan;
    plan.secondary = game_memory_read_u32(memory, object + 0x13cu);
    if (memory->fault) return plan;
    plan.interface_handle = game_memory_read_u32(memory, object + 0xb4u);
    if (!memory->fault && plan.secondary != 0u &&
            plan.interface_handle != 0u) {
        GameAddress vtable = game_memory_read_u32(memory,
            plan.interface_handle);
        if (!memory->fault)
            plan.virtual_target = game_memory_read_u32(memory,
                vtable + 0x74u);
    }
    return plan;
}

void gameplay_state_begin_interface_teardown(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object, 0x00571c58u);
}

GameplayInterfaceTeardownPlan gameplay_state_interface_teardown_plan(
        GameMemory *memory, GameAddress object) {
    GameplayInterfaceTeardownPlan plan = {0u, 0u, 0u};
    plan.interface_handle = game_memory_read_u32(memory, object + 0xb4u);
    if (memory->fault || plan.interface_handle == 0u) return plan;
    plan.secondary = game_memory_read_u32(memory, object + 0x140u);
    if (!memory->fault && plan.secondary == 0u) {
        GameAddress vtable = game_memory_read_u32(memory,
            plan.interface_handle);
        if (!memory->fault)
            plan.release_target = game_memory_read_u32(memory, vtable + 8u);
    }
    return plan;
}

void gameplay_state_clear_optional_interface(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object + 0xb4u, 0u);
}

int gameplay_state_object_message_allowed(uint32_t message) {
    return message != 0x0bu;
}

GameplayObjectMessageSpec gameplay_state_object_message_spec(unsigned event) {
    static const uint32_t guards[11] = {
        0u, 0x29cu, 0x2a0u, 0x2a4u, 0x2b0u, 0x2d4u,
        0x29cu, 0x2a0u, 0x2a4u, 0x2b0u, 0x2d8u
    };
    static const uint32_t messages[11] = {
        0x23u, 0x80u, 0x81u, 0x82u, 0x83u, 0x89u,
        0x84u, 0x85u, 0x86u, 0x87u, 0x88u
    };
    static const GameAddress returns[11] = {
        0x004795b4u, 0x004795d4u, 0x004795f4u, 0x00479614u,
        0x00479634u, 0x00479654u, 0x00479674u, 0x00479694u,
        0x004796b4u, 0x004796d4u, 0x004796f4u
    };
    GameplayObjectMessageSpec spec = {0u, 0u, 0u, 0};
    if (event < 11u) {
        spec.guard_offset = guards[event];
        spec.message = messages[event];
        spec.return_address = returns[event];
        spec.guarded = event != 0u;
    }
    return spec;
}

uint32_t gameplay_state_object_message_guard(GameMemory *memory,
        GameAddress object, uint32_t guard_offset) {
    return game_memory_read_u32(memory, object + guard_offset);
}

void gameplay_state_store_presentation_mode(GameMemory *memory,
        GameAddress object, uint32_t mode) {
    game_memory_write_u32(memory, object + 0x44u, mode);
}

void gameplay_state_prepare_presentation_mode(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u16(memory, object + 0x2e8u, 0u);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x2ecu, 0u);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x2eau,
        0x100u);
}

GameplayPresentationModePlan gameplay_state_presentation_mode_plan(
        GameMemory *memory, GameAddress object) {
    GameplayPresentationModePlan plan = {0};
    plan.source = game_memory_read_u32(memory, object + 0xb4u);
    if (!memory->fault)
        plan.nested = game_memory_read_u32(memory, plan.source + 4u);
    if (!memory->fault)
        plan.nested_state = game_memory_read_u32(memory, plan.nested + 0x14u);
    if (!memory->fault && plan.nested_state != 0u)
        plan.source_mode = game_memory_read_u32(memory, plan.source + 0x10u);
    if (!memory->fault)
        plan.shared_state = game_memory_read_u32(memory, 0x005ac6acu);
    if (!memory->fault)
        plan.selector = game_memory_read_u32(memory, object + 0x14u);
    if (!memory->fault)
        plan.table_object = game_memory_read_u32(memory,
            0x005f0210u + (plan.selector & 0x3fu) * 4u);
    return plan;
}

void gameplay_state_store_presentation_runtime(GameMemory *memory,
        GameAddress object, uint32_t mode, uint32_t shared_state) {
    game_memory_write_u32(memory, object + 0x2f4u, mode);
    if (!memory->fault)
        game_memory_write_u32(memory, object + 0x2f8u, shared_state);
}

void gameplay_state_finish_presentation_runtime(GameMemory *memory,
        GameAddress object, GameAddress handle) {
    game_memory_write_u32(memory, object + 0x2f0u, handle);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x31cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x30cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x2fcu, 0u);
}

GameAddress gameplay_state_typed_presentation_table(GameMemory *memory,
        uint32_t type) {
    return game_memory_read_u32(memory,
        0x005f0210u + (type & 0x3fu) * 4u);
}

GameAddress gameplay_state_owner_presentation_runtime(GameMemory *memory,
        GameAddress owner) {
    return game_memory_read_u32(memory, owner + 0xdcu);
}

void gameplay_state_store_owner_presentation_runtime(GameMemory *memory,
        GameAddress owner, GameAddress runtime) {
    game_memory_write_u32(memory, owner + 0xdcu, runtime);
}

GameplayTypedPresentationPlan gameplay_state_attach_typed_presentation(
        GameMemory *memory, GameAddress owner, GameAddress record) {
    GameplayTypedPresentationPlan plan = {0};
    plan.runtime = game_memory_read_u32(memory, owner + 0xdcu);
    if (!memory->fault)
        game_memory_write_u32(memory, plan.runtime + 0x10u, record);
    if (!memory->fault)
        plan.record_state = game_memory_read_u32(memory, record + 0x14u);
    if (!memory->fault)
        game_memory_write_u32(memory, plan.runtime + 0x14u,
            plan.record_state);
    if (!memory->fault)
        plan.owner_type = game_memory_read_u32(memory, owner + 0x14u);
    if (!memory->fault && (plan.owner_type & 0x3fu) < 4u) {
        plan.first = game_memory_read_u32(memory, owner + 0x18u);
        plan.second = game_memory_read_u32(memory, owner + 0x1cu);
        plan.third = game_memory_read_u32(memory, owner + 0x20u);
    }
    return plan;
}

void gameplay_state_copy_typed_presentation_transform(GameMemory *memory,
        const GameplayTypedPresentationPlan *plan) {
    game_memory_write_u32(memory, plan->runtime + 0x18u, plan->first);
    if (!memory->fault)
        game_memory_write_u32(memory, plan->runtime + 0x1cu, plan->second);
    if (!memory->fault)
        game_memory_write_u32(memory, plan->runtime + 0x20u, plan->third);
}

GameAddress gameplay_state_indexed_delivery_head(GameMemory *memory,
        GameAddress owner) {
    GameAddress list = game_memory_read_u32(memory, owner + 4u);
    return memory->fault ? 0u : game_memory_read_u32(memory, list);
}

GameplayIndexedDeliveryNode gameplay_state_indexed_delivery_outer(
        GameMemory *memory, GameAddress node) {
    GameplayIndexedDeliveryNode result = {0};
    result.next = game_memory_read_u32(memory, node + 4u);
    if (!memory->fault) result.payload = game_memory_read_u32(memory, node + 8u);
    if (!memory->fault)
        result.nested = game_memory_read_u32(memory, result.payload + 4u);
    if (!memory->fault)
        result.key = game_memory_read_u32(memory, result.nested + 4u);
    return result;
}

GameAddress gameplay_state_indexed_delivery_inner_list(GameMemory *memory,
        GameAddress payload) {
    return game_memory_read_u32(memory, payload);
}

GameplayIndexedDeliveryNode gameplay_state_indexed_delivery_inner(
        GameMemory *memory, GameAddress node) {
    GameplayIndexedDeliveryNode result = {0};
    result.next = game_memory_read_u32(memory, node + 4u);
    if (!memory->fault) result.payload = game_memory_read_u32(memory, node + 8u);
    if (!memory->fault) result.key = game_memory_read_u32(memory, result.payload);
    if (!memory->fault)
        result.callback = game_memory_read_u32(memory, result.payload + 4u);
    return result;
}

GameAddress gameplay_state_owned_payload_list(GameMemory *memory,
        GameAddress owner) {
    return game_memory_read_u32(memory, owner + 0x0cu);
}

GameplayIndexedDeliveryNode gameplay_state_owned_payload_node(
        GameMemory *memory, GameAddress node) {
    GameplayIndexedDeliveryNode result = {0};
    result.next = game_memory_read_u32(memory, node + 4u);
    if (!memory->fault) result.payload = game_memory_read_u32(memory, node + 8u);
    if (!memory->fault) result.key = game_memory_read_u32(memory,
        result.payload + 8u);
    return result;
}

void gameplay_state_prepare_gameplay_record(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u8(memory, object + 0xa4u, 0u);
}

void gameplay_state_finish_gameplay_record(GameMemory *memory,
        GameAddress object, uint32_t owner) {
    game_memory_write_u32(memory, object + 0xa0u, owner);
}

GameplayPairReplacementPlan gameplay_state_pair_replacement_plan(
        GameMemory *memory, GameAddress owner, GameAddress argument) {
    GameplayPairReplacementPlan plan = {0u, 0u, 0u, 0u, 0u, 0u};
    plan.first_peer = game_memory_read_u32(memory, owner + 0x108u);
    if (memory->fault) return plan;
    if (plan.first_peer == argument) {
        plan.branch = 1u;
        plan.forwarded = game_memory_read_u32(memory, argument + 0x108u);
    } else {
        plan.second_peer = game_memory_read_u32(memory, owner + 0x10cu);
        if (!memory->fault && plan.second_peer == argument) {
            plan.branch = 2u;
            plan.forwarded = game_memory_read_u32(memory,
                argument + 0x10cu);
        }
    }
    if (!memory->fault && plan.branch != 0u) {
        plan.vtable = game_memory_read_u32(memory, owner);
        if (!memory->fault) plan.target = game_memory_read_u32(memory,
            plan.vtable + (plan.branch == 1u ? 0x104u : 0x108u));
    }
    return plan;
}

GameplayInterfaceSlotPlan gameplay_state_interface_slot_plan(
        GameMemory *memory, uint32_t index) {
    GameplayInterfaceSlotPlan plan;
    plan.slot = 0x005acd38u + index * 4u;
    plan.current = game_memory_read_u32(memory, plan.slot);
    plan.first_table = 0x005acd10u + index * 4u;
    plan.second_table = 0x005acce8u + index * 4u;
    return plan;
}

int gameplay_state_peer_method(GameMemory *memory,GameAddress owner,GameAddress argument,
        GameAddress method,const GameplayPeerServices *services){
    if(memory->fault)return 0;
    if(method==0x458580u){
        unsigned lane;for(lane=0;lane<5;lane++){
            GameAddress head=game_memory_read_u32(memory,owner+0x13cu+lane*4u),target;
            if(!head)continue;
            target=game_memory_read_u32(memory,game_memory_read_u32(memory,head)+0x10cu);
            if(memory->fault || !gameplay_state_peer_method(memory,head,owner,target,services))return 0;
        }
        return !memory->fault;
    }
    /*Actual empty controller overrides; no successful unknown-method path. */
    if(method==0x459760u || method==0x4bde70u || method==0x4bdfe0u || method==0x4bdff0u)return 1;
    if(method==0x459720u || method==0x459ea0u){
        unsigned lane;
        if(method==0x459720u && !argument)return 1;
        for(lane=0;lane<5;lane++){
            GameAddress slot=owner+0x13cu+lane*4u,head=game_memory_read_u32(memory,slot);
            if(method==0x459720u){
                if(head && game_memory_read_u32(memory,head+0x10cu)==argument){game_memory_write_u32(memory,slot,argument);break;}
            }else if(head==argument){game_memory_write_u32(memory,owner+0x164u+lane*4u,0);break;}
        }
        return !memory->fault;
    }
    if(method==0x4bde80u || method==0x4bdf30u || method==0x4be000u){
        int count=(int16_t)game_memory_read_u16(memory,0x639a74u),i;
        GameAddress sites=game_memory_read_u32(memory,owner+0x2c8u);
        if(memory->fault || count<0)return 0;
        if(method==0x4bde80u && !argument)return 1;
        for(i=count*8-1;i>=0;i--){
            GameAddress slot=sites+(unsigned)i*4u,head=game_memory_read_u32(memory,slot);
            int match=method==0x4bde80u?head && game_memory_read_u32(memory,head+0x10cu)==argument:
                method==0x4bdf30u?head && head==argument:head==argument;
            if(match){
                if(method!=0x4be000u)game_memory_write_u32(memory,slot,
                    method==0x4bde80u?argument:game_memory_read_u32(memory,argument+0x10cu));
                if(method==0x4be000u || (method==0x4bdf30u && !game_memory_read_u32(memory,slot)))
                    game_memory_write_u32(memory,game_memory_read_u32(memory,owner+0x30cu)+(unsigned)i*4u,0);
                return !memory->fault;
            }
        }
        if(method!=0x4be000u)for(i=0;i<16;i++){
            GameAddress slot=owner+0x2ccu+(unsigned)i*4u,head=game_memory_read_u32(memory,slot);
            if(head && (method==0x4bde80u?game_memory_read_u32(memory,head+0x10cu)==argument:head==argument)){
                game_memory_write_u32(memory,slot,method==0x4bde80u?argument:game_memory_read_u32(memory,argument+0x10cu));break;
            }
        }
        return !memory->fault;
    }
    if(method==0x459870u){
        unsigned lane;
        for(lane=0;lane<5;lane++){
            GameAddress slot=owner+0x13cu+lane*4u,member=game_memory_read_u32(memory,slot);
            if(member && member==argument){
                GameAddress next=game_memory_read_u32(memory,argument+0x10cu);
                game_memory_write_u32(memory,slot,next);
                if(!next)game_memory_write_u32(memory,owner+0x164u+lane*4u,0);
                break;
            }
        }
        return !memory->fault;
    }
    if(method==0x539c80u || method==0x539cb0u){
        uint32_t other=argument?game_memory_read_u32(memory,argument+20u):0u;
        if(!argument || (other && !((game_memory_read_u32(memory,owner+20u)^other)&63u)))
            game_memory_write_u32(memory,owner+(method==0x539c80u?0x108u:0x10cu),argument);
        return !memory->fault;
    }
    if(method==0x539ce0u){
        GameplayPairReplacementPlan plan=gameplay_state_pair_replacement_plan(memory,owner,argument);
        if(memory->fault)return 0;
        return !plan.branch || gameplay_state_peer_method(memory,owner,plan.forwarded,plan.target,services);
    }
    if(method==0x539c50u){
        static const uint32_t offsets[]={0x10cu,0x108u};unsigned i;
        for(i=0;i<2u;i++){
            GameAddress peer=game_memory_read_u32(memory,owner+offsets[i]),target;
            if(memory->fault)return 0;
            if(!peer)continue;
            target=game_memory_read_u32(memory,game_memory_read_u32(memory,peer)+0x10cu);
            if(memory->fault || !gameplay_state_peer_method(memory,peer,owner,target,services))return 0;
        }
        return 1;
    }
    return services && services->call && services->call(services->context,owner,method,argument) && !memory->fault;
}

uint32_t gameplay_state_callback_registration_state(GameMemory *memory,
        GameAddress owner) {
    return game_memory_read_u32(memory, owner + 8u);
}

void gameplay_state_begin_callback_registration(GameMemory *memory,
        GameAddress owner) {
    game_memory_write_u32(memory, owner + 8u, 1u);
}

GameAddress gameplay_state_rectangle_service(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0063c8b8u);
}

GameplayRecordDelegationPlan gameplay_state_record_delegation_plan(
        GameMemory *memory, GameAddress object) {
    GameplayRecordDelegationPlan plan = {0u, 0u, 0u, 0u, 0u, 0u};
    plan.global_state = game_memory_read_u32(memory, 0x00594d3cu);
    if (memory->fault) return plan;
    if (plan.global_state != 0u) {
        plan.slot = game_memory_read_u32(memory, object + 0x14u);
        plan.group = plan.slot & 0x3fu;
        if (!memory->fault && plan.slot != 0u) {
            plan.owner = game_memory_read_u32(memory, 0x0058973cu);
            if (!memory->fault) plan.table_value = game_memory_read_u32(memory,
                0x005f0210u + plan.group * 4u);
        }
    }
    if (!memory->fault)
        plan.inhibit = game_memory_read_u8(memory, object + 0xa4u);
    return plan;
}

GameAddress gameplay_state_presentation_sink(GameMemory *memory,
        GameAddress object, unsigned index) {
    static const uint32_t offsets[4] = {0x58u, 0x5cu, 0x60u, 0x8cu};
    return index < 4u ?
        game_memory_read_u32(memory, object + offsets[index]) : 0u;
}

GameplayPresentationBufferResult gameplay_state_publish_presentation_buffer(
        GameMemory *memory, GameAddress object, GameAddress buffer) {
    GameplayPresentationBufferResult result;
    result.index = game_memory_read_u32(memory, object + 0x18u);
    result.next_index = result.index + 1u;
    result.type = game_memory_read_u16(memory, object + 8u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x2e0u,
        buffer);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x18u,
        result.next_index);
    return result;
}

GameplayConfigurationSnapshot gameplay_state_publish_configuration_snapshot(
        GameMemory *memory, GameAddress object) {
    GameplayConfigurationSnapshot result = {0};
    result.first = game_memory_read_u32(memory, object + 0x18u);
    result.second = game_memory_read_u32(memory, object + 0x1cu);
    result.third = game_memory_read_u32(memory, object + 0x20u);
    result.packed_first = game_memory_read_u16(memory, object + 0x24u);
    result.packed_second = game_memory_read_u16(memory, object + 0x26u);
    result.packed_third = game_memory_read_u16(memory, object + 0x28u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x64u,
        result.first);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x68u,
        result.second);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x6cu,
        result.third);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x7cu, 0u);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x78u,
        result.packed_first);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x76u,
        result.packed_second);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x74u,
        result.packed_third);
    if (!memory->fault) result.vtable = game_memory_read_u32(memory, object);
    if (!memory->fault) result.first_target = game_memory_read_u32(memory,
        result.vtable + 0x90u);
    if (!memory->fault) result.second_target = game_memory_read_u32(memory,
        result.vtable + 0x94u);
    return result;
}

void gameplay_state_reset_request_words(GameMemory *memory,
        GameAddress object) {
    unsigned index;
    for (index = 0u; index < 4u && !memory->fault; ++index)
        game_memory_write_u32(memory, object + 0x6cu + index * 4u, 0u);
}

uint32_t gameplay_state_request_entry_count(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object + 0x80u);
}

void gameplay_state_mark_request_entry(GameMemory *memory,
        GameAddress object, uint32_t index) {
    GameAddress entries = game_memory_read_u32(memory, object + 0x84u);
    if (!memory->fault)
        game_memory_write_u32(memory, entries + index * 4u, 1u);
}

void gameplay_state_finish_request_reset(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object + 0x88u, 0u);
}

GameplayCallbackReplacementPlan gameplay_state_begin_callback_replacement(
        GameMemory *memory, GameAddress object) {
    GameplayCallbackReplacementPlan plan;
    game_memory_write_u32(memory, object, 0u);
    plan.active = memory->fault ? 0u :
        game_memory_read_u32(memory, object + 0x10u);
    plan.callback = memory->fault ? 0u :
        game_memory_read_u32(memory, object + 0x14u);
    plan.fallback = memory->fault ? 0u :
        game_memory_read_u32(memory, object + 0x18u);
    plan.context = memory->fault ? 0u :
        game_memory_read_u32(memory, object + 0x1cu);
    return plan;
}

uint32_t gameplay_state_callback_status(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object);
}

void gameplay_state_mark_callback_fallback(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object, 1u);
}

void gameplay_state_finish_callback_replacement(GameMemory *memory,
        GameAddress object, GameAddress fallback, GameAddress context) {
    game_memory_write_u32(memory, object + 0x18u, fallback);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1cu, context);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x10u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x14u, 0u);
}

GameAddress gameplay_state_begin_presentation_refresh(GameMemory *memory,
        GameAddress object) {
    GameAddress child = game_memory_read_u32(memory, object + 0xc8u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x1f8u, 0u);
    return child;
}

GameAddress gameplay_state_presentation_collection(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object + 0x294u);
}

GameAddress gameplay_state_presentation_buffer(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object + 0x2e0u);
}

GameAddress gameplay_state_subordinate(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object + 0xdcu);
}

uint32_t gameplay_state_subordinate_capacity(GameMemory *memory,
        GameAddress subordinate) {
    return game_memory_read_u32(memory, subordinate + 0x28u);
}

void gameplay_state_store_subordinate(GameMemory *memory,
        GameAddress object, GameAddress subordinate) {
    game_memory_write_u32(memory, object + 0xdcu, subordinate);
}

uint32_t gameplay_state_interpolation_count(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object + 0x2b4u);
}

void gameplay_state_publish_launch_result(GameMemory *memory,
        GameAddress object, uint32_t result) {
    game_memory_write_u32(memory, object + 0xa8u, result);
}

uint32_t gameplay_state_launch_mode(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00624f8cu);
}

uint32_t gameplay_state_clock(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005ac6acu);
}

void gameplay_state_store_launch_expiration(GameMemory *memory,
        GameAddress object, uint32_t expiration) {
    game_memory_write_u32(memory, object + 0xacu, expiration);
}

void gameplay_state_begin_source_scalar(GameMemory *memory,
        GameAddress object, uint8_t mode) {
    game_memory_write_u8(memory, object + 0x6cu, mode);
}

uint32_t gameplay_state_publish_source_scalar(GameMemory *memory,
        GameAddress object, uint32_t value) {
    uint32_t clock = game_memory_read_u32(memory, 0x005ac6acu);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x64u, value);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x68u, clock);
    return clock;
}

void gameplay_state_prepare_optional_source(GameMemory *memory,
        GameAddress object, uint32_t value) {
    game_memory_write_u32(memory, object + 0xb4u, value);
    if (!memory->fault) game_memory_write_u8(memory, object + 0xb8u, 0u);
}

void gameplay_state_publish_object_payload(GameMemory *memory,
        GameAddress owner, GameAddress payload) {
    game_memory_write_u32(memory, owner + 0xdcu, payload);
}

void gameplay_state_clear_object_payload(GameMemory *memory,
        GameAddress owner) {
    game_memory_write_u32(memory, owner + 0xdcu, 0u);
    if (!memory->fault) game_memory_write_u8(memory, owner + 0xd8u, 0u);
}

GameAddress gameplay_state_object_peer(GameMemory *memory,
        GameAddress owner, uint32_t offset) {
    return game_memory_read_u32(memory, owner + offset);
}

uint32_t gameplay_state_object_pool_count(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00606698u);
}

GameAddress gameplay_state_object_pool_slot(GameMemory *memory,
        uint32_t index) {
    return game_memory_read_u32(memory, 0x00605900u + index * 4u);
}

void gameplay_state_clear_object_pool_slot(GameMemory *memory,
        uint32_t index) {
    game_memory_write_u32(memory, 0x00605900u + index * 4u, 0u);
}

GameAddress gameplay_state_global_object_head(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00631908u);
}

GameAddress gameplay_state_pop_global_object(GameMemory *memory,
        GameAddress object) {
    GameAddress next = game_memory_read_u32(memory, object + 0x38u);
    if (!memory->fault) game_memory_write_u32(memory, 0x00631908u, next);
    if (!memory->fault && object != 0u)
        game_memory_write_u32(memory, object, 0x00576bc8u);
    return next;
}

void gameplay_state_store_collection_mode(GameMemory *memory,
        GameAddress object, uint32_t mode) {
    game_memory_write_u32(memory, object + 0xb0u, mode);
}

uint32_t gameplay_state_collection_count(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object + 0x2fcu);
}

GameAddress gameplay_state_collection_handle(GameMemory *memory,
        GameAddress object, uint32_t index) {
    return game_memory_read_u32(memory, object + 0x16cu + index * 4u);
}

void gameplay_state_clear_collection_handle(GameMemory *memory,
        GameAddress object, uint32_t index) {
    game_memory_write_u32(memory, object + 0x16cu + index * 4u, 0u);
}

GameAddress gameplay_state_begin_owned_field_destructor(GameMemory *memory,
        GameAddress object, GameAddress vtable, uint32_t field_offset) {
    game_memory_write_u32(memory, object, vtable);
    return memory->fault ? 0u :
        game_memory_read_u32(memory, object + field_offset);
}

void gameplay_state_clear_owned_field(GameMemory *memory,
        GameAddress object, uint32_t field_offset) {
    game_memory_write_u32(memory, object + field_offset, 0u);
}

int16_t gameplay_state_owned_interface_count(GameMemory *memory,
        GameAddress object) {
    return (int16_t)game_memory_read_u16(memory, object + 0x24u);
}

GameAddress gameplay_state_owned_interface(GameMemory *memory,
        GameAddress object, uint32_t index) {
    GameAddress array = game_memory_read_u32(memory, object + 0x18u);
    return memory->fault ? 0u :
        game_memory_read_u32(memory, array + index * 4u);
}

GameAddress gameplay_state_object_selection(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object + 0xd0u);
}

GameAddress gameplay_state_shared_presentation(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00628a90u);
}

void gameplay_state_publish_source_descriptor(GameMemory *memory,
        GameAddress output, uint32_t first, uint32_t second, uint32_t third,
        uint32_t aligned_span) {
    game_memory_write_u32(memory, output + 8u, second);
    if (!memory->fault) game_memory_write_u32(memory, output + 4u, first);
    if (!memory->fault) game_memory_write_u32(memory, output + 0x14u, third);
    if (!memory->fault)
        game_memory_write_u32(memory, output + 0x0cu, aligned_span);
}

GameAddress gameplay_state_object_label(GameAddress object) {
    return object + 0x5cu;
}

void gameplay_state_store_object_label_key(GameMemory *memory,
        GameAddress object, uint32_t key) {
    game_memory_write_u32(memory, object + 0x9cu, key);
}

uint8_t gameplay_state_prepare_object_action(GameMemory *memory,
        GameAddress object) {
    uint8_t type = game_memory_read_u8(memory, object + 0x0cu);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x260u, 0u);
    return type;
}

GameAddress gameplay_state_object_action_target(GameMemory *memory,
        GameAddress object) {
    GameAddress vtable = game_memory_read_u32(memory, object);
    return memory->fault ? 0u : game_memory_read_u32(memory, vtable + 0x68u);
}

GameplayActiveObjectPlan gameplay_state_active_object_plan(GameMemory *memory) {
    GameplayActiveObjectPlan plan;
    plan.active = game_memory_read_u32(memory, 0x0058f890u);
    plan.first_previous = game_memory_read_u32(memory, 0x0058f89cu);
    plan.second_previous = game_memory_read_u32(memory, 0x0058f898u);
    return plan;
}

void gameplay_state_archive_active_object(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, 0x0058f894u, object);
}

void gameplay_state_publish_active_object(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, 0x0058f890u, object);
}

uint16_t gameplay_state_active_object_mode(GameMemory *memory,
        GameAddress *object) {
    *object = game_memory_read_u32(memory, 0x0058f890u);
    return memory->fault ? 0u : game_memory_read_u16(memory, *object + 4u);
}

GameAddress gameplay_state_clear_active_object_request(GameMemory *memory,
        GameAddress object, uint16_t mode) {
    GameAddress target = game_memory_read_u32(memory, object + 0x28u);
    if (!memory->fault)
        game_memory_write_u32(memory, target + (mode == 0u ? 0x2c8u :
            0x1e8u), 0u);
    return target;
}

GameplayLinkedStatePlan gameplay_state_linked_state_plan(GameMemory *memory,
        GameAddress object) {
    GameplayLinkedStatePlan plan;
    plan.previous = game_memory_read_u32(memory, object + 0x3cu);
    plan.preflight_mode = game_memory_read_u32(memory, object + 0x28u);
    plan.final_mode = game_memory_read_u32(memory, object + 0x24u);
    return plan;
}

void gameplay_state_activate_linked_state(GameMemory *memory,
        GameAddress object, GameAddress next) {
    game_memory_write_u32(memory, object + 0x40u, 1u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x14u, next);
}

void gameplay_state_mark_linked_state_refresh(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object + 0x44u, 1u);
}

void gameplay_state_prepare_linked_state_clear(GameMemory *memory,
        GameAddress object, uint32_t previous) {
    game_memory_write_u32(memory, object + 0x10u, previous);
}

void gameplay_state_finish_linked_state_clear(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object + 0x3cu, 0u);
}

GameplayWorldQuerySnapshot gameplay_state_prepare_world_query(
        GameMemory *memory, GameAddress object) {
    GameplayWorldQuerySnapshot snapshot = {{0u, 0u, 0u, 0u}, 0u, 0u};
    static const GameAddress source[4] = {
        0x00597724u, 0x0059772cu, 0x00597734u, 0x0059773cu
    };
    static const GameAddress destination[4] = {
        0x00597720u, 0x00597728u, 0x00597730u, 0x00597738u
    };
    unsigned index;
    (void)object;
    for (index = 0u; index < 4u && !memory->fault; ++index)
        snapshot.world[index] = game_memory_read_u32(memory, source[index]);
    for (index = 0u; index < 4u && !memory->fault; ++index)
        game_memory_write_u32(memory, destination[index],
            snapshot.world[index]);
    return snapshot;
}

void gameplay_state_world_query_object(GameMemory *memory,
        GameAddress object, uint32_t *first, uint32_t *second) {
    *second = game_memory_read_u32(memory, object + 8u);
    *first = game_memory_read_u32(memory, object);
}

GameplayObjectStatusSnapshot gameplay_state_object_status_snapshot(
        GameMemory *memory, GameAddress object) {
    GameplayObjectStatusSnapshot snapshot = {0};
    GameAddress vtable;
    snapshot.global_active = game_memory_read_u32(memory, 0x00594d3cu);
    snapshot.handle = game_memory_read_u32(memory, object + 0x14u);
    snapshot.global_owner = game_memory_read_u32(memory, 0x0058973cu);
    if (!memory->fault)
        snapshot.handle_owner = game_memory_read_u32(memory,
            0x005f0210u + (snapshot.handle & 0x3fu) * 4u);
    if (!memory->fault)
        snapshot.inhibited = game_memory_read_u8(memory, object + 0xb8u);
    if (!memory->fault)
        snapshot.threshold = game_memory_read_u32(memory, object + 0x58u);
    vtable = game_memory_read_u32(memory, object);
    if (!memory->fault)
        snapshot.virtual_target = game_memory_read_u32(memory,
            vtable + 0x44u);
    return snapshot;
}

uint32_t gameplay_state_status_global_active(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x00594d3cu);
}

uint32_t gameplay_state_status_handle(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object + 0x14u);
}

uint32_t gameplay_state_status_global_owner(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x0058973cu);
}

uint32_t gameplay_state_status_handle_owner(GameMemory *memory,
        uint32_t handle) {
    return game_memory_read_u32(memory,
        0x005f0210u + (handle & 0x3fu) * 4u);
}

uint8_t gameplay_state_status_inhibited(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u8(memory, object + 0xb8u);
}

uint32_t gameplay_state_status_threshold(GameMemory *memory,
        GameAddress object) {
    return game_memory_read_u32(memory, object + 0x58u);
}

GameAddress gameplay_state_status_virtual_target(GameMemory *memory,
        GameAddress object) {
    GameAddress vtable = game_memory_read_u32(memory, object);
    return memory->fault ? 0u :
        game_memory_read_u32(memory, vtable + 0x44u);
}

GameplayVirtualCallPlan gameplay_state_status_virtual_call(
        GameMemory *memory, GameAddress object) {
    GameplayVirtualCallPlan plan;
    plan.vtable = game_memory_read_u32(memory, object);
    plan.target = memory->fault ? 0u :
        game_memory_read_u32(memory, plan.vtable + 0x44u);
    return plan;
}

uint32_t gameplay_state_increment_optional_counter(GameMemory *memory,
        GameAddress counter) {
    uint32_t value = 0u;
    if (counter == 0u) return value;
    value = game_memory_read_u32(memory, counter);
    if (!memory->fault) game_memory_write_u32(memory, counter, value + 1u);
    return value;
}

uint8_t gameplay_state_text_byte(GameMemory *memory, GameAddress text,
        uint32_t index) {
    return game_memory_read_u8(memory, text + index);
}

GameplayStateRangeSnapshot gameplay_state_range_snapshot(GameMemory *memory) {
    GameplayStateRangeSnapshot snapshot;
    snapshot.count = (int32_t)game_memory_read_u32(memory, 0x00602618u);
    snapshot.base = (int32_t)game_memory_read_u32(memory, 0x006025f0u);
    return snapshot;
}

GameAddress gameplay_state_range_object(GameMemory *memory,
        int alternate, uint32_t index) {
    GameAddress table = alternate ? 0x00602500u : 0x00602570u;
    return game_memory_read_u32(memory, table + index * 4u);
}

uint32_t gameplay_state_runtime_views_initialized(GameMemory *memory) {
    return game_memory_read_u32(memory, 0x005eff04u);
}

void gameplay_state_copy_runtime_view(GameMemory *memory, uint32_t index) {
    GameAddress source = game_memory_read_u32(memory,
        0x005f0210u + index * 4u);
    GameAddress destination = 0x006027f0u + index * 0xf0u;
    static const GameAddress scalar_sources[6] = {
        0x005acc8cu, 0x005ac9c0u, 0x005ac900u,
        0x005acbf0u, 0x005acc84u, 0x005ac904u
    };
    unsigned word;
    source += 0x29cu;
    for (word = 0u; word < 0x20u && !memory->fault; ++word)
        game_memory_write_u32(memory, destination + word * 4u,
            game_memory_read_u32(memory, source + word * 4u));
    for (word = 0u; word < 6u && !memory->fault; ++word)
        game_memory_write_u32(memory, destination + 0x80u + word * 4u,
            game_memory_read_u32(memory, scalar_sources[word]));
}

GameplayRuntimeViewCopyResult gameplay_state_copy_runtime_views(
        GameMemory *memory) {
    GameplayRuntimeViewCopyResult result = {0u, 0x00602bb0u,
        0x005f0220u};
    unsigned index;
    for (index = 0u; index < 4u && !memory->fault; ++index)
        gameplay_state_copy_runtime_view(memory, index);
    if (!memory->fault)
        result.final_eax = game_memory_read_u32(memory, 0x005acbf0u);
    return result;
}

void gameplay_state_copy_runtime_view_seed(GameMemory *memory) {
    unsigned word;
    for (word = 0u; word < 10u && !memory->fault; ++word)
        game_memory_write_u32(memory, 0x00602f78u + word * 4u,
            game_memory_read_u32(memory, 0x005ac678u + word * 4u));
}

GameplayRuntimeViewSourceResult gameplay_state_copy_runtime_view_sources(
        GameMemory *memory) {
    GameplayRuntimeViewSourceResult result = {0u, 0u};
    unsigned word;
    GameAddress source = game_memory_read_u32(memory, 0x005f0220u);
    for (word = 0u; word < 17u && !memory->fault; ++word)
        game_memory_write_u32(memory, 0x00602fd4u + word * 4u,
            game_memory_read_u32(memory, source + 0x29cu + word * 4u));
    source = game_memory_read_u32(memory, 0x005f0224u);
    for (word = 0u; word < 13u && !memory->fault; ++word) {
        result.final_edx = game_memory_read_u32(memory,
            source + 0x1fcu + word * 4u);
        game_memory_write_u32(memory, 0x00602fa0u + word * 4u,
            result.final_edx);
    }
    result.final_ecx = source + 0x230u;
    return result;
}

GameplayScalarTableSpec gameplay_state_scalar_table_spec(unsigned index) {
    static const GameplayScalarTableSpec specs[2] = {
        {0x005e1348u, 0x40u, 0x42c80000u},
        {0x005e13c8u, 0x40u, 0x42200000u}
    };
    return specs[index < 2u ? index : 0u];
}

GameAddress gameplay_state_value_submit_target(GameMemory *memory,
        GameAddress object) {
    GameAddress vtable = game_memory_read_u32(memory, object);
    return memory->fault ? 0u :
        game_memory_read_u32(memory, vtable + 0x74u);
}

GameplayVirtualCallPlan gameplay_state_value_submit_call(GameMemory *memory,
        GameAddress object) {
    GameplayVirtualCallPlan plan;
    plan.vtable = game_memory_read_u32(memory, object);
    plan.target = memory->fault ? 0u :
        game_memory_read_u32(memory, plan.vtable + 0x74u);
    return plan;
}

void gameplay_state_publish_value_pair(GameMemory *memory,
        GameAddress output, uint32_t value) {
    game_memory_write_u32(memory, output, value);
    if (!memory->fault) game_memory_write_u32(memory, output + 4u, value);
}

GameplayStatusDispatchPlan gameplay_state_status_dispatch_plan(
        GameMemory *memory) {
    GameplayStatusDispatchPlan plan = {0u, 0u, 0u};
    plan.state = game_memory_read_u32(memory, 0x0059d4c4u);
    if (!memory->fault)
        plan.kind = game_memory_read_u32(memory, plan.state + 0x3cu);
    if (!memory->fault && (plan.kind == 11u || plan.kind == 22u))
        plan.active = game_memory_read_u32(memory, plan.state + 0x48u);
    return plan;
}

GameplayObjectCallPlan gameplay_state_object_tail_call(GameMemory *memory,
        GameAddress owner) {
    GameplayObjectCallPlan plan = {0u, 0u, 0u};
    plan.object = game_memory_read_u32(memory, owner + 0x54u);
    if (!memory->fault) plan.vtable = game_memory_read_u32(memory,
        plan.object);
    if (!memory->fault) plan.target = game_memory_read_u32(memory,
        plan.vtable + 8u);
    return plan;
}

void gameplay_state_initialize_descriptor_binding(GameMemory *memory,
        GameAddress object, uint32_t state, int extended) {
    game_memory_write_u32(memory, object + 0x10u, state);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x3cu, 0u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x40u, 0u);
    if (extended) {
        if (!memory->fault) game_memory_write_u32(memory, object + 0x50u, 0u);
        if (!memory->fault) game_memory_write_u32(memory, object + 0x54u, 0u);
        if (!memory->fault) game_memory_write_u32(memory, object + 0x58u, 0u);
        if (!memory->fault) game_memory_write_u32(memory, object + 0x4cu, 3u);
    }
}

void gameplay_state_attach_descriptor_owner(GameMemory *memory,
        GameAddress attached, GameAddress object) {
    game_memory_write_u32(memory, attached + 0xd4u, object);
}

uint32_t gameplay_state_finish_descriptor_binding(GameMemory *memory,
        GameAddress object, GameAddress attached, uint32_t *binding) {
    game_memory_write_u32(memory, object + 0x3cu, attached);
    *binding = game_memory_read_u32(memory, attached + 0x14u);
    if (!memory->fault) game_memory_write_u32(memory, object + 0x40u,
        *binding);
    return memory->fault ? 0u : (uint32_t)(int32_t)(int16_t)
        game_memory_read_u16(memory, object + 0x14u);
}

void gameplay_state_initialize_presentation_packet(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object, 0x005758b0u);
    game_memory_write_u32(memory, object + 0x44u, 0u);
    game_memory_write_u32(memory, object + 0x14u, 0u);
    game_memory_write_u32(memory, object + 0x48u, 0u);
    game_memory_write_u32(memory, object + 0x50u, UINT32_MAX);
    game_memory_write_u32(memory, object + 0x54u, UINT32_MAX);
    game_memory_write_u32(memory, object + 0x4cu, 0u);
    game_memory_write_u32(memory, object, 0x00575908u);
    game_memory_write_u32(memory, object + 0x68u, 0u);
    game_memory_write_u32(memory, object + 0x6cu, 0u);
    game_memory_write_u32(memory, object + 0x70u, 0u);
    game_memory_write_u32(memory, object + 0x74u, 0u);
    game_memory_write_u32(memory, object + 0x7cu, UINT32_MAX);
    game_memory_write_u32(memory, object + 0x80u, UINT32_MAX);
    game_memory_write_u32(memory, object + 0x84u, UINT32_MAX);
    game_memory_write_u32(memory, object + 0x8cu, 0u);
    game_memory_write_u32(memory, object + 0x60u, 0u);
    game_memory_write_u32(memory, object + 0x5cu, 0u);
    game_memory_write_u32(memory, object + 0x58u, 0u);
    game_memory_write_u32(memory, object + 0x90u, 0u);
    game_memory_write_u32(memory, object + 0x78u, 0u);
}

void gameplay_state_initialize_container_header(GameMemory *memory,
        GameAddress container, uint8_t kind, uint8_t subtype,
        uint32_t value, int alternate) {
    game_memory_write_u8(memory, container + 0x0cu, kind);
    game_memory_write_u32(memory, container + 0x10u, 0u);
    game_memory_write_u8(memory, container + 0x0du, subtype);
    game_memory_write_u32(memory, container + 0x14u,
        alternate ? 0xbf800000u : value);
    game_memory_write_u32(memory, container + 0x18u,
        alternate ? value : 0xbf800000u);
}

void gameplay_state_initialize_surface_container(GameMemory *memory,
        GameAddress container) {
    game_memory_write_u32(memory, container, 0u);
    game_memory_write_u32(memory, container + 0x14u, 0u);
    game_memory_write_u32(memory, container + 0x1cu, 0u);
}

void gameplay_state_initialize_surface_rows(GameMemory *memory,
        GameAddress container, int32_t height) {
    GameAddress records = game_memory_read_u32(memory, container + 0x18u);
    GameAddress first = game_memory_read_u32(memory, container + 0x10u);
    GameAddress second = game_memory_read_u32(memory, container + 0x0cu);
    int32_t row;
    for (row = 0; row < height && !memory->fault; ++row) {
        GameAddress record = records + (uint32_t)row * 40u;
        game_memory_write_u32(memory, record, 3u);
        game_memory_write_u32(memory, record + 4u,
            first + (uint32_t)row * 12u);
        game_memory_write_u32(memory, record + 16u,
            second + (uint32_t)row * 24u);
    }
}

GameAddress gameplay_state_seed_primary_interface(GameMemory *memory) {
    GameAddress owner = game_memory_read_u32(memory, 0x005f0210u);
    game_memory_write_u32(memory, 0x005eff04u, 0u);
    game_memory_write_u8(memory, 0x005efe80u, 0u);
    game_memory_write_u32(memory, 0x005f0208u, owner);
    game_memory_write_u32(memory, 0x005f0038u, UINT32_MAX);
    return owner;
}

GameAddress gameplay_state_finish_primary_interface(GameMemory *memory) {
    GameAddress owner;
    game_memory_write_u32(memory, 0x005f0150u, 1u);
    game_memory_write_u32(memory, 0x00624f8cu, 7u);
    owner = game_memory_read_u32(memory, 0x005f0208u);
    game_memory_write_u32(memory, 0x005eff28u, owner);
    return owner;
}

GameAddress gameplay_state_resource_slot(GameMemory *memory,
        uint32_t index) {
    return game_memory_read_u32(memory, 0x0059dbc0u + index * 4u);
}

void gameplay_state_clear_resource_slot(GameMemory *memory,
        uint32_t index) {
    game_memory_write_u32(memory, 0x0059dbc0u + index * 4u, 0u);
}

void gameplay_state_initialize_entity_runtime(GameMemory *memory,
        GameAddress object) {
    static const uint16_t zero_offsets[] = {0xf0u, 0xc8u, 0x108u,
        0x10cu, 0x138u, 0x12cu, 0xbcu, 0xc4u, 0xd0u, 0x130u,
        0x134u};
    unsigned index;
    game_memory_write_u32(memory, object + 0xe0u, 5u);
    for (index = 0u; index < sizeof(zero_offsets) / sizeof(zero_offsets[0]);
            ++index)
        if (!memory->fault)
            game_memory_write_u32(memory, object + zero_offsets[index], 0u);
}

void gameplay_state_store_entity_integer_property(GameMemory *memory,
        GameAddress object, uint32_t index, uint32_t value) {
    game_memory_write_u32(memory, object + 0x110u + index * 4u, value);
}

GameplayNumericTableSpec gameplay_state_numeric_table_spec(unsigned index) {
    static const GameplayNumericTableSpec specs[2] = {
        {0x005ab504u, 0x005ac500u, 0x00571f1cu, 1023u},
        {0x00622c58u, 0x0062365cu, 0x0057521cu, 642u}
    };
    return specs[index < 2u ? index : 0u];
}

void gameplay_state_fill_u32(GameMemory *memory, GameAddress destination,
        uint32_t count, uint32_t value, int reverse) {
    uint32_t index;
    for (index = 0u; index < count && !memory->fault; ++index)
        game_memory_write_u32(memory, reverse ? destination - index * 4u :
            destination + index * 4u, value);
}

void gameplay_state_initialize_scaled_runtime_header(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u8(memory, object + 0x0cu, 0x0fu);
    game_memory_write_u32(memory, object, 0x00575138u);
}

void gameplay_state_clear_scaled_runtime_fields(GameMemory *memory,
        GameAddress object, GameAddress embedded) {
    game_memory_write_u32(memory, embedded + 0x0cu, 0u);
    game_memory_write_u32(memory, embedded + 0x10u, 0u);
    game_memory_write_u32(memory, embedded + 0x28u, 0u);
    game_memory_write_u32(memory, embedded + 0x2cu, 0u);
    game_memory_write_u32(memory, object + 0xe4u, 0u);
    game_memory_write_u32(memory, object + 0xe8u, 0u);
    game_memory_write_u32(memory, object + 0xecu, 0u);
    game_memory_write_u32(memory, object + 0xf0u, 0u);
}

GameplayRecordGroupSpec gameplay_state_record_group_spec(unsigned index,
        int alternate_first_group) {
    static const GameplayRecordGroupSpec specs[4] = {
        {3u, 4u, 0x005ad880u, {0x00584230u, 0x00584240u,
            0x00584250u, 0x00584260u}, 1u, 0x0043d8c7u, 0x0043d902u},
        {3u, 4u, 0x005ad890u, {0x005842b0u, 0x005842c0u,
            0x005842d0u, 0x005842e0u}, 1u, 0x0043d925u, 0x0043d95fu},
        {3u, 4u, 0x005ad8a0u, {0x00584270u, 0x00584280u,
            0x00584290u, 0x005842a0u}, 1u, 0x0043d97cu, 0x0043d9b5u},
        {6u, 4u, 0x005ad8b0u, {0x005842f0u, 0x00584308u,
            0x00584320u, 0x00584338u}, 1u, 0x0043da05u, 0x0043da3bu}
    };
    if (index == 0u && alternate_first_group) return specs[2];
    return specs[index < 4u ? index : 0u];
}

GameplayBufferedOwnerPlan gameplay_state_buffered_owner_plan(unsigned variant) {
    static const GameplayBufferedOwnerPlan plans[2] = {
        {0x00536520u, 0x00554b7bu, 0x00554ae9u},
        {0x00536440u, 0x00554c4fu, 0x00554bbdu}
    };
    return plans[variant < 2u ? variant : 0u];
}

GameplayTimedEffectAttachPlan gameplay_state_timed_effect_attach_plan(
        unsigned variant) {
    static const GameplayTimedEffectAttachPlan plans[2] = {
        {0x80u, 0x0053eb95u, 0x0053eba0u, 0x0053ec19u, 0x0053ec2du, 0},
        {0x98u, 0x0053f645u, 0x0053f650u, 0x0053f6d3u, 0x0053f6e7u, 1}
    };
    return plans[variant < 2u ? variant : 0u];
}

GameplaySelectionTablePlan gameplay_state_selection_table_plan(
        unsigned variant) {
    static const GameplaySelectionTablePlan plans[2] = {
        {0x005ace7cu, 0x005acefcu, 0x005ace80u, 0x005acec0u,
            0x005acec4u, 0x005acec8u, 0x00435a6bu, 0x00435b03u},
        {0x005f0230u, 0x005f022cu, 0x005f0250u, 0x005f0240u,
            0x005f0244u, 0x005f0248u, 0x004847cbu, 0x00484863u}
    };
    return plans[variant < 2u ? variant : 0u];
}

GameplayTailCallPlan gameplay_state_tail_call_plan(unsigned variant) {
    static const GameplayTailCallPlan plans[2] = {
        {0x004a6ea0u, 0x004a7105u, 0x004a6d90u},
        {0x0056a06cu, 0x0056a035u, 0x0056a05bu}
    };
    return plans[variant < 2u ? variant : 0u];
}

uint32_t gameplay_state_smacker_channel_mask(unsigned index) {
    static const uint32_t masks[7] = {
        0x2000u, 0x4000u, 0x8000u, 0x10000u,
        0x20000u, 0x40000u, 0x80000u
    };
    return index < 7u ? masks[index] : 0u;
}

int gameplay_state_apply_smacker_channels(GameMemory *memory,
    GameAddress owner, uint32_t volume, const GameplaySmackerChannelOps *ops) {
    unsigned channel;
    if (!memory || !ops || !ops->apply) return 0;
    for (channel = 0u; channel < 7u; ++channel) {
        uint32_t handle = game_memory_read_u32(memory, owner + 0x50u);
        if (memory->fault || !ops->apply(ops->context, channel, handle,
                gameplay_state_smacker_channel_mask(channel), volume,
                0x7ff6u)) return 0;
    }
    return !memory->fault;
}

GameplayAllocationSpec gameplay_state_world_buffer_spec(unsigned index) {
    static const GameplayAllocationSpec specs[4] = {
        {0x20d0u, 0x00597010u, 0x0050363au},
        {0x3480u, 0x00597014u, 0x00503653u},
        {0x0d20u, 0x00597018u, 0x0050366cu},
        {0x1a40u, 0x0059701cu, 0x00503685u}
    };
    return specs[index < 4u ? index : 0u];
}

GameplayAllocationSpec gameplay_state_runtime_pool_spec(unsigned index) {
    static const GameplayAllocationSpec specs[7] = {
        {0x6d60u, 0x0059443cu, 0x004cc3bbu},
        {0xbb80u, 0x00594440u, 0x004cc3d5u},
        {0x2ee0u, 0x00594444u, 0x004cc3efu},
        {0x5dc0u, 0x00594448u, 0x004cc409u},
        {0x5dc0u, 0x0059444cu, 0x004cc423u},
        {0x2ee0u, 0x00594450u, 0x004cc43du},
        {0x0190u, 0x00594454u, 0x004cc457u}
    };
    return specs[index < 7u ? index : 0u];
}

GameplayAllocationSpec gameplay_state_object_array_spec(unsigned index) {
    static const GameplayAllocationSpec specs[5] = {
        {0u, 0x68u, 0x00441775u}, {0u, 0x6cu, 0x0044178bu},
        {0u, 0x70u, 0x004417a1u}, {0u, 0x74u, 0x004417b7u},
        {0u, 0x84u, 0x004417cdu}
    };
    return specs[index < 5u ? index : 0u];
}

void gameplay_state_initialize_world_down_vector(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00624f20u, 0u);
    game_memory_write_u32(memory, 0x00624f24u, 0xbf800000u);
    game_memory_write_u32(memory, 0x00624f28u, 0u);
}

void gameplay_state_reset_runtime_pool_tables(GameMemory *memory) {
    unsigned index;
    for (index = 0u; index < 0x40u && !memory->fault; ++index)
        game_memory_write_u32(memory, 0x00604278u + index * 4u, 0u);
    for (index = 0u; index < 0x100u && !memory->fault; ++index)
        game_memory_write_u8(memory, 0x00604380u + index, (uint8_t)index);
}

uint32_t gameplay_state_prepare_runtime_mode(GameMemory *memory) {
    uint32_t mode = game_memory_read_u32(memory, 0x00597f80u);
    game_memory_write_u32(memory, 0x00604064u, UINT32_MAX);
    if (mode == 0u) {
        game_memory_write_u32(memory, 0x006037e8u, 0x13u);
        game_memory_write_u32(memory, 0x006034d0u, 0x38u);
        return 0x38u;
    }
    game_memory_write_u32(memory, 0x006037e8u, 0x1eu);
    game_memory_write_u32(memory, 0x006034d0u, 0x80u);
    return 0x80u;
}

GameAddress gameplay_state_indexed_slot_target(GameMemory *memory,
        GameAddress object, int *dispatch) {
    uint32_t index = game_memory_read_u32(memory, object + 0x1c4u);
    uint32_t enabled = game_memory_read_u32(memory, object + 0x1a4u);
    if (dispatch) *dispatch = 0;
    if (memory->fault || index >= 32u || (enabled & (1u << index)) == 0u ||
            index == 0u || index > 12u)
        return 0x004b4577u;
    if (dispatch) *dispatch = 1;
    return game_memory_read_u32(memory, 0x004b45e0u + (index - 1u) * 4u);
}

GameplayDisplayTransitionPlan gameplay_state_display_transition_plan(
        GameMemory *memory) {
    GameplayDisplayTransitionPlan plan = {0u, 0x004ad6d5u, 1};
    uint32_t phase = game_memory_read_u32(memory, 0x00597f88u);
    if (memory->fault || (int32_t)phase <= 1) { plan.apply = 0; return plan; }
    if (game_memory_read_u32(memory, 0x0058f89cu) !=
            game_memory_read_u32(memory, 0x0058f890u) ||
            game_memory_read_u32(memory, 0x0059d440u) != 0x3f800000u)
        return plan;
    if (game_memory_read_u32(memory, 0x006038a0u) != 0u) {
        plan.scalar_bits = 0x41800000u;
        plan.call_return = game_memory_read_u32(memory, 0x00597f80u) == 0u ?
            0x004ad6b9u : 0x004ad6c6u;
    } else if (game_memory_read_u32(memory, 0x00597f80u) == 0u) {
        plan.scalar_bits = 0x424c0000u; plan.call_return = 0x004ad691u;
    } else {
        plan.scalar_bits = 0x42c60000u; plan.call_return = 0x004ad6a3u;
    }
    return plan;
}

void gameplay_state_begin_presentation_bootstrap(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00597f90u, 1u);
    game_memory_write_u32(memory, 0x00627928u, 0x280u);
    game_memory_write_u32(memory, 0x0062792cu, 0x1e0u);
    game_memory_write_u32(memory, 0x00597f80u, 1u);
}

void gameplay_state_publish_presentation_dimensions(GameMemory *memory) {
    uint32_t width = game_memory_read_u32(memory, 0x00597f68u);
    uint32_t height = game_memory_read_u32(memory, 0x00597f6cu);
    game_memory_write_u32(memory, 0x005ac6a0u, width);
    game_memory_write_u32(memory, 0x005ac6a4u, height);
    game_memory_write_u32(memory, 0x005ac9d0u,
        (uint32_t)((int32_t)width >> 1u));
    game_memory_write_u32(memory, 0x005ac9d4u,
        (uint32_t)((int32_t)height >> 1u));
}

void gameplay_state_finish_presentation_bootstrap(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00597f90u, 0u);
}

GameplayIndexedSlotMethod gameplay_state_indexed_slot_method(
        unsigned variant, uint8_t state) {
    GameplayIndexedSlotMethod method = {0u, 0u, 0};
    if (variant == 0u && state == 0u) {
        method.vtable_offset = 0x1e8u;
        method.return_address = 0x004b41f0u;
        method.invoke = 1;
    } else if (variant == 1u && state <= 2u) {
        static const uint32_t offsets[3] = {0x1f4u, 0x1c0u, 0x1c4u};
        static const GameAddress returns[3] = {
            0x004b429fu, 0x004b42aeu, 0x004b42bdu
        };
        method.vtable_offset = offsets[state];
        method.return_address = returns[state];
        method.invoke = 1;
    } else if (variant == 2u && state <= 2u) {
        static const uint32_t offsets[3] = {0x1bcu, 0x1c0u, 0x1c4u};
        static const GameAddress returns[3] = {
            0x004b4416u, 0x004b4425u, 0x004b4434u
        };
        method.vtable_offset = offsets[state];
        method.return_address = returns[state];
        method.invoke = 1;
    } else if (variant == 3u && state <= 1u) {
        static const uint32_t offsets[2] = {0x1c8u, 0x1d4u};
        static const GameAddress returns[2] = {0x004b4309u, 0x004b4318u};
        method.vtable_offset = offsets[state];
        method.return_address = returns[state];
        method.invoke = 1;
    }
    return method;
}

GameplayOptionalLookupSpec gameplay_state_optional_lookup_spec(unsigned index) {
    static const GameplayOptionalLookupSpec specs[2] = {
        {0x0057802cu, 0x0063c75cu, 0x00565aebu},
        {0x00578010u, 0x0063c7b8u, 0x00565afbu}
    };
    return specs[index < 2u ? index : 0u];
}

void gameplay_state_initialize_controller_fields(GameMemory *memory,
        GameAddress object) {
    static const uint16_t zero_offsets[] = {
        0x01dcu, 0x01fcu, 0x03bcu, 0x045cu, 0x0458u, 0x0468u,
        0x03c4u, 0x03c8u, 0x04c4u, 0x0470u, 0x0478u, 0x047cu,
        0x0480u, 0x0474u, 0x0484u, 0x0488u, 0x0498u, 0x048cu,
        0x0490u, 0x0494u, 0x049cu, 0x04a0u, 0x04a4u, 0x04a8u,
        0x04acu, 0x04b0u, 0x022cu, 0x04c0u, 0x04bcu, 0x04b8u,
        0x04b4u, 0x058cu, 0x0590u, 0x059cu, 0x0464u, 0x0460u,
        0x0448u, 0x0450u
    };
    unsigned index;
    game_memory_write_u32(memory, object, 0x00573d20u);
    for (index = 0u; index < 2u; ++index)
        game_memory_write_u32(memory, object + zero_offsets[index], 0u);
    game_memory_write_u32(memory, object, 0x00573e78u);
    for (index = 2u; index < sizeof(zero_offsets) / sizeof(zero_offsets[0]);
            ++index)
        if (!memory->fault)
            game_memory_write_u32(memory, object + zero_offsets[index], 0u);
    game_memory_write_u8(memory, object + 0x39cu, 0u);
    game_memory_write_u8(memory, object + 0x31cu, 0u);
    game_memory_write_u32(memory, object + 0x46cu, 1u);
    game_memory_write_u32(memory, object + 0x3c0u, UINT32_MAX);
    game_memory_write_u16(memory, object + 0x598u, 0u);
    game_memory_write_u32(memory, object + 0x594u, UINT32_MAX);
    game_memory_write_u32(memory, object + 0x44cu, UINT32_MAX);
}

void gameplay_state_initialize_descriptor_defaults(GameMemory *memory) {
    static const uint16_t words[][2] = {
        {0xa0,0},{0xa4,0x15e},{0xaa,0x3c},{0xac,0x12},{0xf2,0x96},
        {0xf4,0x98},{0xa2,0x64},{0xa6,0},{0xa8,0x1e},{0xfc,0x177},
        {0x88,3},{0xf0,0},{0xf6,0},{0xf8,0x32},{0xfa,0x64},{0x8a,4},
        {0x8c,4},{0x8e,0},{0x90,1},{0x92,1},{0x94,0x1e},{0xc0,0},
        {0xc2,6},{0xc4,6},{0xc6,0},{0xd8,2},{0xda,0x0e},{0xdc,0x11},
        {0xc8,1},{0xca,1},{0xe4,0x23},{0xcc,0x1e},{0xde,0},{0xe0,0},
        {0x52,0x10},{0x54,0x18},{0x5a,5},{0xe2,0},{0x50,0},{0x56,0},
        {0x58,0},{0x5c,0x46},{0x38,0},{0x3a,0x32},{0x3c,0x46},
        {0x44,0x32},{0x40,0x28},{0x42,0x28}
    };
    static const struct { uint16_t offset; uint32_t value; } dwords[] = {
        {0xb0,0x40666666u},{0xb4,0x40800000u},{0x100,0x41400000u},
        {0x104,0x41000000u},{0x98,0},{0x9c,0},{0xd0,0},{0xd4,0},
        {0xe8,0x3f800000u},{0xec,0x3f800000u},{0x60,0x40400000u},
        {0x64,0x40400000u},{0x4c,0x3f800000u}
    };
    unsigned index;
    for (index = 0u; index < sizeof(words) / sizeof(words[0]); ++index)
        game_memory_write_u16(memory, 0x00629700u + words[index][0],
            words[index][1]);
    for (index = 0u; index < sizeof(dwords) / sizeof(dwords[0]); ++index)
        game_memory_write_u32(memory, 0x00629700u + dwords[index].offset,
            dwords[index].value);
}

static GameAddress group_allocate(const GameplayGroupServices *services,size_t size){
    return services && services->allocate?services->allocate(services->context,size):
        scene_projection_storage_allocate(size,"gameplay-group-array");
}
static int group_release(const GameplayGroupServices *services,GameAddress address){
    if(!address)return 1;
    if(address>=SCENE_PROJECTION_BASE && address<RECYCLABLE_RECORD_BASE)return scene_projection_storage_free(address);
    return services && services->release && services->release(services->context,address);
}
static void group_store_float(GameMemory *memory,GameAddress address,long double value){
    float stored=(float)value;uint32_t bits;memcpy(&bits,&stored,4u);game_memory_write_u32(memory,address,bits);
}
static void group_merge_bounds(GameMemory *memory,GameAddress owner,GameAddress incoming,int rectangle){
    unsigned i;
    for(i=0u;i<4u;i++){
        GameAddress target=owner+0x38u+i*4u;
        uint32_t current_bits=game_memory_read_u32(memory,target),next_bits=game_memory_read_u32(memory,incoming+i*4u);
        float current=selector_float(current_bits),next=selector_float(next_bits);
        int replace=i>=2u?!(current>=next):rectangle && i==0u?!(next>current):current>next;
        if(replace)game_memory_write_u32(memory,target,next_bits);
    }
}
/* 549750 owns the three allocated arrays and takes the position/metadata/
 * rectangle buffers supplied by its caller. All storage retains bounded IDs. */
int gameplay_group_construct(GameMemory *memory,const GameplayGroupServices *services,GameAddress owner,
    const GameplayGroupConstruction *input){
    GameAddress arrays[3]={0u};uint16_t count=(uint16_t)(input->secondary_count+1u);unsigned i,j;
    static const unsigned fields[3]={0x18u,0x2cu,0x28u};
    if(count>0x7fffu){memory->fault=1;return 0;}
    for(i=0u;i<4u;i++){
        game_memory_write_u32(memory,owner+4u+i*4u,0xbf800000u);
        game_memory_write_u32(memory,owner+0x38u+i*4u,0xbf800000u);
    }
    game_memory_write_u32(memory,owner,input->identity);
    game_memory_write_u32(memory,owner+0x14u,input->price==UINT32_MAX?5000u:input->price);
    game_memory_copy_elements(memory,owner+4u,input->bounds,4u,4u,0);
    game_memory_write_u32(memory,owner+0x1cu,input->positions);
    game_memory_write_u32(memory,owner+0x20u,input->metadata);
    game_memory_write_u16(memory,owner+0x24u,count);game_memory_write_u16(memory,owner+0x26u,0u);
    for(i=0u;i<3u;i++){
        arrays[i]=count?group_allocate(services,(size_t)count*(i==1u?8u:4u)):0u;
        if(count && !arrays[i])goto allocation_failed;
    }
    for(i=0u;i<3u;i++)game_memory_write_u32(memory,owner+fields[i],arrays[i]);
    for(i=0u;i<count;i++){
        long double dx=(long double)selector_float(game_memory_read_u32(memory,input->positions+i*12u))-
            (long double)selector_float(game_memory_read_u32(memory,input->positions));
        long double dz=(long double)selector_float(game_memory_read_u32(memory,input->positions+i*12u+8u))-
            (long double)selector_float(game_memory_read_u32(memory,input->positions+8u));
        float square=(float)(dx*dx+dz*dz);
        game_memory_write_u32(memory,arrays[0]+i*4u,0u);
        for(j=0u;j<4u;j++)game_memory_write_u16(memory,arrays[1]+i*8u+j*2u,0xffffu);
        group_store_float(memory,arrays[2]+i*4u,gameplay_geometry_square_root(square));
    }
    game_memory_write_u16(memory,owner+0x34u,input->rectangle_count);
    game_memory_write_u32(memory,owner+0x30u,input->rectangles);
    /* The original caller supplies the first rectangle even for count zero. */
    game_memory_copy_elements(memory,owner+0x38u,input->rectangles,4u,4u,0);
    if(input->rectangle_count==1u){
        if(!group_release(services,input->rectangles))return 0;
        game_memory_write_u32(memory,owner+0x30u,0u);game_memory_write_u16(memory,owner+0x34u,0u);
    }else if(input->rectangle_count<0x8000u){
        for(i=0u;i<input->rectangle_count;i++)group_merge_bounds(memory,owner,input->rectangles+i*16u,1);
    }
    group_merge_bounds(memory,owner,owner+4u,0);
    group_store_float(memory,owner+0x48u,((long double)selector_float(game_memory_read_u32(memory,owner+4u))+
        (long double)selector_float(game_memory_read_u32(memory,owner+0xcu)))*(long double)selector_float(game_memory_read_u32(memory,0x5773e0u)));
    group_store_float(memory,owner+0x50u,((long double)selector_float(game_memory_read_u32(memory,owner+8u))+
        (long double)selector_float(game_memory_read_u32(memory,owner+0x10u)))*(long double)selector_float(game_memory_read_u32(memory,0x5773e0u)));
    group_store_float(memory,owner+0x4cu,gameplay_geometry_terrain_height(memory,
        selector_float(game_memory_read_u32(memory,owner+0x48u)),selector_float(game_memory_read_u32(memory,owner+0x50u))));
    for(i=0x54u;i<=0x68u;i+=4u)game_memory_write_u32(memory,owner+i,0u);
    return !memory->fault;
allocation_failed:
    for(i=0u;i<3u;i++)if(arrays[i])(void)group_release(services,arrays[i]);
    for(i=0u;i<3u;i++)game_memory_write_u32(memory,owner+fields[i],0u);
    /* Input buffers transfer to this owner even when its array allocation fails. */
    (void)group_release(services,input->positions);
    (void)group_release(services,input->metadata);
    (void)group_release(services,input->rectangles);
    game_memory_write_u32(memory,owner+0x1cu,0u);
    game_memory_write_u32(memory,owner+0x20u,0u);
    game_memory_write_u32(memory,owner+0x30u,0u);
    memory->fault=1;return 0;
}
int gameplay_payload_set_disabled(GameMemory *memory,GameAddress object,int articulated,uint32_t enabled,void *context,GameplayPayloadPropertyQuery query){
    GameAddress field=object+(articulated?0xb8u:0xa4u),table;uint8_t flags;uint32_t value;
    flags=game_memory_read_u8(memory,field);
    game_memory_write_u8(memory,field,(uint8_t)(enabled?flags|1u:flags&0xfeu));
    table=game_memory_read_u32(memory,object);
    if(memory->fault || !query || !query(context,object,game_memory_read_u32(memory,table+0x48u),&value))return 0;
    game_memory_write_u32(memory,object+0x5cu,value);
    /* The original caches the vtable across both getters. */
    if(memory->fault || !query(context,object,game_memory_read_u32(memory,table+0x4cu),&value))return 0;
    game_memory_write_u32(memory,object+0x60u,value);return !memory->fault;
}
int gameplay_group_remove_member(GameMemory *memory,const GameplayGroupDetachServices *services,GameAddress owner,int32_t slot){
    GameAddress members,member,catalog,counter,metadata,back,next;uint16_t remaining;int32_t identity;unsigned i;
    if(slot==0){
        int32_t count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
        for(i=1u;(int32_t)i<count;i++){
            member=game_memory_read_u32(memory,game_memory_read_u32(memory,owner+0x18u)+i*4u);
            if(memory->fault || (member && (!services || !services->disable || !services->disable(services->context,member,1))))return 0;
            count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
        }
    }
    members=game_memory_read_u32(memory,owner+0x18u);
    member=game_memory_read_u32(memory,members+(uint32_t)slot*4u);
    identity=(int16_t)game_memory_read_u16(memory,member+0x14u);
    catalog=game_memory_read_u32(memory,owner+0x54u);
    counter=catalog+0x200u+(uint32_t)identity*4u;
    game_memory_write_u32(memory,counter,game_memory_read_u32(memory,counter)-1u);
    /* Descriptor storage belongs to the existing recyclable pool, not the
     * payload factory. Return it without freeing the live payload. */
    member=game_memory_read_u32(memory,game_memory_read_u32(memory,owner+0x18u)+(uint32_t)slot*4u);
    game_memory_write_u32(memory,member+0x38u,game_memory_read_u32(memory,0x631908u));
    game_memory_write_u32(memory,0x631908u,member);
    game_memory_write_u32(memory,game_memory_read_u32(memory,owner+0x18u)+(uint32_t)slot*4u,0u);
    metadata=game_memory_read_u32(memory,owner+0x2cu)+(uint32_t)slot*8u;
    for(i=4u;i-- >0u;)game_memory_write_u16(memory,metadata+i*2u,0xffffu);
    remaining=(uint16_t)(game_memory_read_u16(memory,owner+0x26u)-1u);
    game_memory_write_u16(memory,owner+0x26u,remaining);
    if(!remaining){
        back=game_memory_read_u32(memory,owner+0x68u);
        if(back)game_memory_write_u32(memory,back+0x64u,game_memory_read_u32(memory,owner+0x64u));
        next=game_memory_read_u32(memory,owner+0x64u);
        if(next)game_memory_write_u32(memory,next+0x68u,game_memory_read_u32(memory,owner+0x68u));
        game_memory_write_u32(memory,owner+0x68u,0u);game_memory_write_u32(memory,owner+0x64u,0u);
        game_memory_write_u32(memory,owner+0x54u,0u);game_memory_write_u32(memory,owner+0x58u,0u);
    }
    return !memory->fault;
}
int gameplay_descriptor_detach(GameMemory *memory,const GameplayGroupDetachServices *services,GameAddress record,int auxiliaries){
    GameAddress payload;unsigned i;
    if(auxiliaries){
        for(i=0u;i<3u;i++){
            GameAddress field=record+0x50u+i*4u,effect=game_memory_read_u32(memory,field);
            if(effect){
                if(!services || !services->stop || !services->stop(services->context,effect))return 0;
                effect=game_memory_read_u32(memory,field);
                if(!services->retire || !services->retire(services->context,effect))return 0;
                game_memory_write_u32(memory,field,0u);
            }
        }
        game_memory_write_u32(memory,record+0x4cu,3u);
    }
    payload=game_memory_read_u32(memory,record+0x3cu);
    if(payload){
        game_memory_write_u32(memory,payload+0xb4u,0u);
        payload=game_memory_read_u32(memory,record+0x3cu);
        game_memory_write_u32(memory,payload+0xd4u,0u);
    }
    return !memory->fault && gameplay_group_remove_member(memory,services,game_memory_read_u32(memory,record+4u),(int16_t)game_memory_read_u16(memory,record+8u));
}
int gameplay_group_reset(GameMemory *memory,const GameplayGroupServices *services,GameAddress owner){
    unsigned i=0u;int count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
    while(count>0 && i<(unsigned)count){
        GameAddress member=game_memory_read_u32(memory,game_memory_read_u32(memory,owner+0x18u)+i*4u);
        if(memory->fault || (member && (!services || !services->member_reset || !services->member_reset(services->context,member))))return 0;
        i++;count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
    }
    return !memory->fault;
}
int gameplay_group_destroy(GameMemory *memory,const GameplayGroupServices *services,GameAddress owner){
    static const unsigned pairs[2][2]={{0x5cu,0x60u},{0x64u,0x68u}};
    static const unsigned buffers[5]={0x30u,0x1cu,0x20u,0x2cu,0x28u};unsigned i,j;
    for(i=0u;i<2u;i++){
        GameAddress back=game_memory_read_u32(memory,owner+pairs[i][1]),next=game_memory_read_u32(memory,owner+pairs[i][0]);
        if(back){
            game_memory_write_u32(memory,back+pairs[i][0],next);
            if(next)game_memory_write_u32(memory,next+pairs[i][1],back);
            game_memory_write_u32(memory,owner+pairs[i][1],0u);
            if(!i)game_memory_write_u32(memory,owner+pairs[i][0],0u);
        }
    }
    for(i=0u;i<5u;i++){
        GameAddress buffer=game_memory_read_u32(memory,owner+buffers[i]);
        if(buffer && !group_release(services,buffer))return 0;
        game_memory_write_u32(memory,owner+buffers[i],0u);
    }
    if(game_memory_read_u32(memory,owner+0x18u)){
        int count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
        for(j=0u;count>0 && j<(unsigned)count;j++){
            GameAddress array=game_memory_read_u32(memory,owner+0x18u),member=game_memory_read_u32(memory,array+j*4u);
            if(member && (!services || !services->member_destroy || !services->member_destroy(services->context,member)))return 0;
            member=game_memory_read_u32(memory,game_memory_read_u32(memory,owner+0x18u)+j*4u);
            if(member){
                game_memory_write_u32(memory,member,0x576bc8u);
                if(!group_release(services,member))return 0;
                game_memory_write_u32(memory,game_memory_read_u32(memory,owner+0x18u)+j*4u,0u);
            }
            count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
        }
    }
    if(!group_release(services,game_memory_read_u32(memory,owner+0x18u)))return 0;
    game_memory_write_u32(memory,owner+0x18u,0u);
    return !memory->fault;
}

/* Shared descriptor payload construction used by group activation. The
 * catalog counter and record state execute here; distinct factory/update
 * behavior remains visible in the services until its own family is recovered. */
int gameplay_group_primary_construct(GameMemory *memory,const GameplayGroupServices *services,
    GameAddress record,GameAddress descriptor){
    GameAddress catalog=game_memory_read_u32(memory,descriptor+0x54u),primary=0u;uint32_t variant=0u;
    game_memory_write_u32(memory,record,0x576bc8u);game_memory_write_u16(memory,record+0x14u,0u);
    game_memory_write_u32(memory,record+4u,descriptor);
    game_memory_write_u32(memory,catalog+0x200u,game_memory_read_u32(memory,catalog+0x200u)+1u);
    game_memory_write_u32(memory,record,0x576c68u);game_memory_write_u16(memory,record+8u,0u);
    if(memory->fault || !services || !services->catalog_value ||
        !services->catalog_value(services->context,catalog,0x23u,3u,&variant) || !services->create ||
        !services->create(services->context,0x23u,(uint16_t)variant,&primary) || !primary)return 0;
    game_memory_write_u32(memory,record+0x48u,primary);return !memory->fault;
}
int gameplay_group_activate(GameMemory *memory,const GameplayGroupServices *services,GameAddress owner,
    GameAddress catalog,uint32_t enabled,uint16_t *status){
    uint32_t price=0u,budget=0u,mode;GameAddress prior,array,record=0u;unsigned i;int count;
    if(!status || !services || !services->catalog_value ||
        !services->catalog_value(services->context,catalog,0x23u,8u,&price))return 0;
    mode=game_memory_read_u32(memory,0x624f8cu);prior=game_memory_read_u32(memory,owner+0x54u);
    if(mode==10u){
        if(prior && prior!=catalog){*status=2u;return !memory->fault;}
        if(prior==catalog && game_memory_read_u32(memory,game_memory_read_u32(memory,owner+0x18u))){*status=1u;return !memory->fault;}
        if(!services->budget || !services->budget(services->context,catalog,price,0,&budget))return 0;
        if(!budget){*status=0u;return !memory->fault;}
    }
    if(!prior){
        GameAddress back=game_memory_read_u32(memory,owner+0x68u),next=game_memory_read_u32(memory,owner+0x64u),anchor=catalog+0x230u;
        if(back)game_memory_write_u32(memory,back+0x64u,next);
        if(next)game_memory_write_u32(memory,next+0x68u,back);
        game_memory_write_u32(memory,owner+0x68u,0u);
        game_memory_write_u32(memory,owner+0x54u,catalog);
        game_memory_write_u32(memory,owner+0x58u,game_memory_read_u32(memory,0x5ac6acu));
        game_memory_write_u32(memory,owner+0x68u,anchor);
        game_memory_write_u32(memory,owner+0x64u,game_memory_read_u32(memory,anchor+0x64u));
        next=game_memory_read_u32(memory,anchor+0x64u);
        if(next)game_memory_write_u32(memory,next+0x68u,owner);
        game_memory_write_u32(memory,anchor+0x64u,owner);
    }
    if(!enabled){*status=3u;return !memory->fault;}
    if(mode==10u && (!services->budget || !services->budget(services->context,
        game_memory_read_u32(memory,owner+0x54u),price,1,&budget)))return 0;
    /* Non-interactive restore can replace an existing primary. Retire it before
     * publishing another owned record instead of losing the previous owner. */
    array=game_memory_read_u32(memory,owner+0x18u);
    record=game_memory_read_u32(memory,array);
    if(memory->fault)return 0;
    if(record){
        if(!services->member_destroy || !services->member_destroy(services->context,record))return 0;
        record=game_memory_read_u32(memory,array);
        if(record && !group_release(services,record))return 0;
        game_memory_write_u32(memory,array,0u);
    }
    record=group_allocate(services,0x5cu);
    if(!record || !gameplay_group_primary_construct(memory,services,record,owner)){
        if(record)(void)group_release(services,record);
        return 0;
    }
    array=game_memory_read_u32(memory,owner+0x18u);game_memory_write_u32(memory,array,record);
    if(memory->fault || !services->member_activate || !services->member_activate(services->context,record,
        game_memory_read_u32(memory,0x5ac6acu),mode!=10u))return 0;
    count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
    for(i=1u;count>1 && i<(unsigned)count;i++){
        GameAddress member=game_memory_read_u32(memory,game_memory_read_u32(memory,owner+0x18u)+i*4u);
        if(member && (!services->member_disable || !services->member_disable(services->context,member)))return 0;
        count=(int16_t)game_memory_read_u16(memory,owner+0x24u);
    }
    game_memory_write_u16(memory,owner+0x26u,(uint16_t)(game_memory_read_u16(memory,owner+0x26u)+1u));
    *status=3u;return !memory->fault;
}

int gameplay_group_member_construct(GameMemory *memory,GameAddress record,GameAddress descriptor,
    uint16_t slot,GameAddress payload,uint16_t identity,unsigned family){
    GameAddress catalog=game_memory_read_u32(memory,descriptor+0x54u),counter;
    if(family>2u)return 0;
    if(family==1u)identity=5u;
    counter=catalog+0x200u+(uint32_t)((int32_t)(int16_t)identity*4);
    game_memory_write_u32(memory,record,0x576bc8u);
    game_memory_write_u32(memory,record+4u,descriptor);game_memory_write_u16(memory,record+0x14u,identity);
    game_memory_write_u32(memory,counter,game_memory_read_u32(memory,counter)+1u);
    game_memory_write_u16(memory,record+8u,slot);
    game_memory_write_u32(memory,record,family==0u?0x576d38u:family==1u?0x576da0u:0x576e08u);
    game_memory_write_u32(memory,record+0x48u,payload);return !memory->fault;
}

static float group_float(GameMemory *memory,GameAddress address){return selector_float(game_memory_read_u32(memory,address));}

static int descriptor_duration(GameMemory *memory,const GameplayDescriptorServices *services,
    GameAddress payload,unsigned family,int32_t *duration){
    if(family==0u){
        uint32_t token,value;GameAddress catalog;
        catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,payload+0x14u)&63u)*4u);
        if(memory->fault || !services->token || !services->token(services->context,payload,&token) ||
           !services->catalog_value || !services->catalog_value(services->context,catalog,0x23u,0u,token,&value))return 0;
        *duration=ui_signed(value);return 1;
    }
    return services->duration && services->duration(services->context,payload,duration);
}

int gameplay_descriptor_bind(GameMemory *memory,const GameplayDescriptorServices *services,
    GameAddress record,unsigned family,uint32_t tick,uint32_t restored){
    static const float height[5]={400.0f,225.0f,500.0f,500.0f,1000.0f};
    GameAddress group,catalog,positions,metadata,payload,attached=0u;uint32_t variant,sequence,binding;
    int32_t slot,identity,duration;float position[3];uint16_t heading;unsigned i;
    if(!services || family>2u)return 0;
    game_memory_write_u32(memory,record+0xcu,tick);
    gameplay_state_initialize_descriptor_binding(memory,record,restored,family==0u);
    group=game_memory_read_u32(memory,record+4u);catalog=game_memory_read_u32(memory,group+0x54u);
    slot=(int32_t)(int16_t)game_memory_read_u16(memory,record+8u);
    if(slot<0 || slot>=32768){memory->fault=1;return 0;}
    positions=game_memory_read_u32(memory,group+0x1cu)+(uint32_t)slot*12u;
    metadata=game_memory_read_u32(memory,group+0x20u);
    for(i=0u;i<3u;i++)position[i]=group_float(memory,positions+i*4u);
    payload=game_memory_read_u32(memory,record+0x48u);
    if(!restored){
        if(memory->fault || !services->catalog_value ||
           !services->catalog_value(services->context,catalog,0x2au,3u,0u,&variant) ||
           !services->create || !services->create(services->context,0x2au,(uint16_t)variant,&attached) || !attached)return 0;
        position[1]=(float)((long double)position[1]+group_float(memory,0x576c4cu));
        gameplay_state_attach_descriptor_owner(memory,attached,record);
        heading=game_memory_read_u16(memory,metadata+(uint32_t)slot*2u);
        if(memory->fault || !services->next_sequence || !services->next_sequence(services->context,catalog,&sequence) ||
           !services->attach || !services->attach(services->context,attached,sequence,position,heading) ||
           !services->publish || !services->publish(services->context,catalog,attached))return 0;
        identity=ui_signed(gameplay_state_finish_descriptor_binding(memory,record,attached,&binding));
        if((uint32_t)identity<=10u)group_store_float(memory,record+0x44u,height[(uint32_t)identity<5u?(unsigned)identity:0u]);
        position[1]=(float)((long double)position[1]-group_float(memory,record+0x44u));
    }else{
        /* Primary and third-family restore deliberately use heading slot zero.
         * The second family keeps the indexed heading and unraised position. */
        heading=game_memory_read_u16(memory,metadata+(family==1u?(uint32_t)slot*2u:0u));
        if(family!=1u)position[1]=(float)((long double)group_float(memory,payload+0x30u)+position[1]);
    }
    payload=game_memory_read_u32(memory,record+0x48u);
    if(memory->fault || !services->next_sequence || !services->next_sequence(services->context,catalog,&sequence) ||
       !services->bind_payload || !services->bind_payload(services->context,payload,family,sequence,position,heading,record,restored!=0u))return 0;
    if(!restored){
        GameAddress state=game_memory_read_u32(memory,payload+0x54u);
        game_memory_write_u8(memory,state+0x3cu,(uint8_t)(game_memory_read_u8(memory,state+0x3cu)|0x20u));
        game_memory_write_u32(memory,record+0x34u,0xffffffu);
        game_memory_write_u32(memory,record+0x10u,0u);
    }
    game_memory_write_u32(memory,record+0x18u,0u);
    payload=game_memory_read_u32(memory,record+0x48u);
    if(memory->fault || !descriptor_duration(memory,services,payload,family,&duration))return 0;
    if(duration/300>1){
        payload=game_memory_read_u32(memory,record+0x48u);
        if(!descriptor_duration(memory,services,payload,family,&duration))return 0;
        game_memory_write_u32(memory,record+0x1cu,(uint32_t)(duration/300));
    }else game_memory_write_u32(memory,record+0x1cu,1u);
    return !memory->fault;
}

static int descriptor_transition(GameMemory *memory,const GameplayDescriptorServices *services,
    GameAddress catalog,GameAddress payload){
    GameAddress effect;uint32_t sequence;
    if(!services->create || !services->create(services->context,0x45u,0u,&effect) || !effect ||
       !services->next_sequence || !services->next_sequence(services->context,catalog,&sequence) ||
       !services->transition || !services->transition(services->context,effect,sequence,payload) ||
       !services->publish || !services->publish(services->context,catalog,effect))return 0;
    return !memory->fault;
}

int gameplay_descriptor_destroy(GameMemory *memory,const GameplayDescriptorServices *services,
    GameAddress record,unsigned family,uint32_t mode){
    GameAddress payload,attached,group,catalog;unsigned i;
    if(!services || family>2u)return 0;
    payload=game_memory_read_u32(memory,record+0x48u);
    attached=game_memory_read_u32(memory,record+0x3cu);
    if(family==1u && payload)game_memory_write_u32(memory,payload+0xa0u,0u);
    if(attached){game_memory_write_u32(memory,attached+0xb4u,0u);game_memory_write_u32(memory,attached+0xd4u,0u);}
    if(memory->fault)return 0;
    /* Clearing the reference after successful retirement also makes an ordinary
     * repeated shutdown safe. This does not claim exhaustive fault equivalence. */
    if(!payload)return 1;
    if(mode==0u || mode==2u){
        if(mode==2u && (!services->stop || !services->stop(services->context,payload)))return 0;
        payload=game_memory_read_u32(memory,record+0x48u);
        if(!services->retire || !services->retire(services->context,payload,family,0))return 0;
        if(family==0u && mode==2u){
            for(i=0u;i<3u;i++){
                GameAddress child=game_memory_read_u32(memory,record+0x50u+i*4u);
                if(!child)continue;
                if(!services->stop || !services->stop(services->context,child))return 0;
                child=game_memory_read_u32(memory,record+0x50u+i*4u);
                if(!services->retire(services->context,child,family,1))return 0;
                game_memory_write_u32(memory,record+0x50u+i*4u,0u);
            }
            game_memory_write_u32(memory,record+0x4cu,3u);
        }
    }else if(mode==1u){
        group=game_memory_read_u32(memory,record+4u);catalog=game_memory_read_u32(memory,group+0x54u);
        if(family==0u){
            if((game_memory_read_u8(memory,payload+0xd8u)&1u) &&
               (!services->notify || !services->notify(services->context,catalog)))return 0;
            for(i=0u;i<3u;i++){
                GameAddress child=game_memory_read_u32(memory,record+0x50u+i*4u);int32_t duration,value,refund;float fraction;long double amount;
                if(!child)continue;
                if(!services->duration || !services->duration(services->context,child,&duration))return 0;
                fraction=(float)((long double)ui_signed(game_memory_read_u32(memory,child+0x58u))/(long double)duration);
                child=game_memory_read_u32(memory,record+0x50u+i*4u);
                if(!services->unit_value || !services->unit_value(services->context,catalog,game_memory_read_u8(memory,child+0xcu),&value))return 0;
                amount=(long double)value*fraction;
                /* Retail FTOL truncates to int64, then passes its low word. */
                refund=ui_signed((uint32_t)(uint64_t)(!isfinite(amount) || amount< -0x1p63L || amount>=0x1p63L?INT64_MIN:(int64_t)amount));
                if(!services->credit || !services->credit(services->context,catalog,refund) ||
                   !descriptor_transition(memory,services,catalog,game_memory_read_u32(memory,record+0x50u+i*4u)))return 0;
            }
            game_memory_write_u32(memory,record+0x4cu,3u);
        }else if(family==2u)game_memory_write_u32(memory,payload+0xb4u,0u);
        if(!descriptor_transition(memory,services,catalog,game_memory_read_u32(memory,record+0x48u)))return 0;
        if(family==0u)game_memory_write_u32(memory,game_memory_read_u32(memory,record+0x48u)+0x294u,0u);
    }
    game_memory_write_u32(memory,record+0x48u,0u);
    return !memory->fault;
}
/* Original signed-zero/bit-sign branches and lookup rounding are intentional. */
int gameplay_descriptor_motion(GameMemory *memory,GameAddress record,float height,const float position[3]){
    GameAddress group=game_memory_read_u32(memory,record+4u),source,attached;int32_t slot,duration;float square,divisor;long double root;
    slot=(int32_t)(int16_t)game_memory_read_u16(memory,record+8u);
    if(slot<0){memory->fault=1;return 0;}
    source=game_memory_read_u32(memory,group+0x1cu)+(uint32_t)slot*12u;
    square=(float)(((long double)position[1]-((long double)group_float(memory,source+4u)+height))*
        group_float(memory,0x576c40u)*group_float(memory,0x576c44u));
    root=gameplay_geometry_square_root(square);
    duration=ui_signed((uint32_t)(uint64_t)(!isfinite(root) || root< -0x1p63L || root>=0x1p63L?INT64_MIN:(int64_t)root));
    game_memory_write_u32(memory,record+0x34u,(uint32_t)duration);
    game_memory_write_u32(memory,record+0xcu,game_memory_read_u32(memory,0x5ac6acu));
    if(memory->fault)return 0;
    /* All three original angular quotients are immediately overwritten with
     * zero. Keep the resulting motion, without instruction-shaped temporaries. */
    game_memory_write_u16(memory,record+0x2cu,0u);game_memory_write_u16(memory,record+0x2eu,0u);game_memory_write_u16(memory,record+0x30u,0u);
    if(!duration){
        /*A restored carrier can already be below its landing plane. Retail
         *divides its zero duration before discarding the angular quotients.
         *Keep duration0 so the next update settles; never publish NaN motion. */
        game_memory_write_u32(memory,record+0x20u,0u);game_memory_write_u32(memory,record+0x24u,0u);
        game_memory_write_u32(memory,record+0x28u,0u);return !memory->fault;
    }
    divisor=(float)duration;
    group_store_float(memory,record+0x20u,((long double)group_float(memory,source)-position[0])/divisor);
    attached=game_memory_read_u32(memory,record+0x3cu);
    group_store_float(memory,record+0x24u,attached?(long double)group_float(memory,attached+0xb4u)*group_float(memory,0x576c48u):0.0L);
    group_store_float(memory,record+0x28u,((long double)group_float(memory,source+8u)-position[2])/divisor);
    return !memory->fault;
}

static int descriptor_release_contacts(GameMemory *memory,const GameplayDescriptorUpdateServices *services,GameAddress head){
    EffectResourcePool *pool=effect_resource_pool_find(0x631910u);int result;
    if(!head)return 1;
    if(pool){result=effect_resource_pool_return_chain(memory,pool,head);if(result)return result>0;}
    return services->release_contacts && services->release_contacts(services->context,head);
}
int gameplay_descriptor_contacts(GameMemory *memory,const GameplayDescriptorUpdateServices *services,GameAddress payload,GameAddress *result){
    GameAddress buckets[9],head=0u;unsigned i;uint32_t visits=0u;
    EffectResourcePool *pool=effect_resource_pool_find(0x631910u);
    if(!services || !result || !gameplay_spatial_query_values(0x5f0800u,group_float(memory,payload+0x18u),group_float(memory,payload+0x20u),buckets))return 0;
    *result=0u;
    for(i=0u;i<9u;i++){
        GameAddress candidate=game_memory_read_u32(memory,buckets[i]+4u);
        for(;candidate && !memory->fault;candidate=game_memory_read_u32(memory,candidate+4u)){
            uint32_t flags=game_memory_read_u32(memory,candidate+0x10u);uint8_t type=game_memory_read_u8(memory,candidate+0xcu);int active,hit;float position[3];GameAddress node;unsigned j;
            if(++visits>65536u){memory->fault=1;goto failed;}
            if(candidate==payload || (!(flags&0x40u) && type!=0xau) || type==0xa2u)continue;
            if(!native_entity_type_activity(memory,candidate,&active) &&
               (!services->activity || !services->activity(services->context,candidate,&active)))goto failed;
            if(active)continue;
            if(!services->intersect || !services->intersect(services->context,payload,candidate,position,&hit))goto failed;
            if(!hit)continue;
            node=pool?effect_resource_pool_acquire_kind(memory,pool,2u):services->allocate_contact?services->allocate_contact(services->context):0u;
            if(!node)goto failed;
            game_memory_write_u32(memory,node+0xcu,candidate);
            for(j=0u;j<3u;j++)group_store_float(memory,node+0x10u+j*4u,position[j]);
            game_memory_write_u32(memory,node+4u,head);head=node;
        }
    }
    *result=head;return !memory->fault;
failed:
    (void)descriptor_release_contacts(memory,services,head);return 0;
}

static int descriptor_insert(GameMemory *memory,GameAddress payload){
    float x=group_float(memory,payload+0x18u),z=group_float(memory,payload+0x20u);GameAddress bucket;uint32_t flags;uint8_t type;int alternate;
    if(memory->fault || !gameplay_spatial_query_group(0x5f0800u,x,z,&bucket))return 0;
    flags=game_memory_read_u32(memory,payload+0x10u);type=game_memory_read_u8(memory,payload+0xcu);
    alternate=(flags&0x400u)!=0u && type!=9u && type!=12u && type!=0xa0u;
    (void)gameplay_spatial_insert(memory,0x5f0800u,payload,(int32_t)x/4096,(int32_t)z/4096,alternate);
    return !memory->fault;
}
static void descriptor_copy_position(GameMemory *memory,GameAddress destination,GameAddress source){
    unsigned i;for(i=0u;i<3u;i++)game_memory_write_u32(memory,destination+i*4u,game_memory_read_u32(memory,source+i*4u));
}
int gameplay_descriptor_update(GameMemory *memory,const GameplayDescriptorUpdateServices *services,GameAddress record,unsigned family){
    const GameplayDescriptorServices *lifecycle;GameAddress payload,group,source,metadata,attached,head,node;uint32_t state,now,elapsed,binding,visits=0u;int32_t slot;unsigned i;
    if(!services || !services->lifecycle || family>2u)return 0;
    lifecycle=services->lifecycle;
    state=game_memory_read_u32(memory,record+0x10u);payload=game_memory_read_u32(memory,record+0x48u);
    if(state>1u)return !memory->fault;
    if(state==1u){
        uint32_t pending=game_memory_read_u32(memory,record+0x18u),step=game_memory_read_u32(memory,record+0x1cu);int32_t duration;
        if(pending){
            uint32_t amount=ui_signed(step)<ui_signed(pending)?step:pending,health;
            if(!lifecycle->duration || !lifecycle->duration(lifecycle->context,payload,&duration))return 0;
            health=game_memory_read_u32(memory,payload+0x58u);
            if(duration>=ui_signed(health+amount))game_memory_write_u32(memory,payload+0x58u,health+amount);
            else{
                payload=game_memory_read_u32(memory,record+0x48u);
                if(!lifecycle->duration(lifecycle->context,payload,&duration))return 0;
                game_memory_write_u32(memory,payload+0x58u,(uint32_t)duration);
            }
            payload=game_memory_read_u32(memory,record+0x48u);
            if(!lifecycle->duration(lifecycle->context,payload,&duration))return 0;
            pending=(uint32_t)duration==game_memory_read_u32(memory,payload+0x58u)?0u:game_memory_read_u32(memory,record+0x18u)-game_memory_read_u32(memory,record+0x1cu);
            game_memory_write_u32(memory,record+0x18u,ui_signed(pending)>0?pending:0u);
        }
        return !memory->fault && services->refresh && services->refresh(services->context,game_memory_read_u32(memory,record+0x48u),0x34u,0u);
    }
    (void)game_list_detach_node(memory,payload,4u,8u);
    now=game_memory_read_u32(memory,0x5ac6acu);elapsed=now-game_memory_read_u32(memory,record+0xcu);
    group=game_memory_read_u32(memory,record+4u);slot=(int32_t)(int16_t)game_memory_read_u16(memory,record+8u);
    if(slot<0){memory->fault=1;return 0;}
    source=game_memory_read_u32(memory,group+0x1cu)+(uint32_t)slot*12u;metadata=game_memory_read_u32(memory,group+0x20u);
    if(ui_signed(elapsed)>=ui_signed(game_memory_read_u32(memory,record+0x34u))){
        uint16_t heading;
        if(family!=1u)game_memory_write_u32(memory,record+0x10u,1u);
        if(!services->sound || !services->sound(services->context,payload+0x18u))return 0;
        payload=game_memory_read_u32(memory,record+0x48u);
        if(family && (!services->phase || !services->phase(services->context,payload,3u)))return 0;
        payload=game_memory_read_u32(memory,record+0x48u);descriptor_copy_position(memory,payload+0x18u,source);
        if(family!=1u)group_store_float(memory,payload+0x1cu,(long double)group_float(memory,payload+0x30u)+group_float(memory,payload+0x1cu));
        heading=game_memory_read_u16(memory,metadata+(family==0u?0u:(uint32_t)slot*2u));
        if(family==1u && (!services->relocate || !services->relocate(services->context,payload,payload+0x18u,heading)))return 0;
        payload=game_memory_read_u32(memory,record+0x48u);
        game_memory_write_u16(memory,payload+0x24u,heading);game_memory_write_u16(memory,payload+0x26u,0u);game_memory_write_u16(memory,payload+0x28u,0u);
        attached=game_memory_read_u32(memory,payload+0x54u);game_memory_write_u8(memory,attached+0x3cu,(uint8_t)(game_memory_read_u8(memory,attached+0x3cu)&0xdfu));
        if(family==0u){
            game_memory_write_u32(memory,payload+0x29cu,0u);descriptor_copy_position(memory,payload+0x2a0u,payload+0x18u);
            group_store_float(memory,payload+0x2a4u,(long double)group_float(memory,payload+0x30u)+group_float(memory,payload+0x2a4u));
        }else{
            uint32_t method=game_memory_read_u32(memory,game_memory_read_u32(memory,record)+0x54u);
            if(family==1u)game_memory_write_u32(memory,record+0x10u,1u);
            if(method==0x53f320u)game_memory_write_u32(memory,payload+0xacu,now);
            else if(method==0x53f420u)game_memory_write_u32(memory,payload+0xc4u,0u);
            else if(!services->settled || !services->settled(services->context,record))return 0;
        }
    }else{
        attached=game_memory_read_u32(memory,record+0x3cu);binding=game_memory_read_u32(memory,record+0x40u);
        if(attached && game_memory_read_u32(memory,attached+0x14u)==binding){
            for(i=0u;i<3u;i++)game_memory_write_u16(memory,payload+0x24u+i*2u,game_memory_read_u16(memory,attached+0x24u+i*2u));
            descriptor_copy_position(memory,payload+0x18u,attached+0x18u);
            game_memory_write_u32(memory,payload+0x10u,game_memory_read_u32(memory,payload+0x10u)&0xfff3ffffu);
            group_store_float(memory,payload+0x1cu,(long double)group_float(memory,payload+0x1cu)-group_float(memory,record+0x44u));
            if((long double)group_float(memory,source+4u)+group_float(memory,0x576c54u)>=(long double)group_float(memory,payload+0x1cu)){
                float position[3];for(i=0u;i<3u;i++)position[i]=group_float(memory,payload+0x18u+i*4u);
                if(!gameplay_descriptor_motion(memory,record,group_float(memory,payload+0x30u),position))return 0;
                game_memory_write_u32(memory,attached+0xd4u,0u);game_memory_write_u32(memory,record+0x3cu,0u);game_memory_write_u32(memory,record+0x40u,0u);
            }
        }else{
            for(i=0u;i<3u;i++)group_store_float(memory,payload+0x18u+i*4u,(long double)group_float(memory,payload+0x18u+i*4u)+group_float(memory,record+0x20u+i*4u));
            game_memory_write_u32(memory,payload+0x10u,game_memory_read_u32(memory,payload+0x10u)&0xfff3ffffu);
            group_store_float(memory,record+0x24u,(long double)group_float(memory,record+0x24u)-group_float(memory,0x576c58u));
            for(i=0u;i<3u;i++)game_memory_write_u16(memory,payload+0x24u+i*2u,(uint16_t)((game_memory_read_u16(memory,payload+0x24u+i*2u)+game_memory_read_u16(memory,record+0x2cu+i*2u))&2047u));
        }
    }
    payload=game_memory_read_u32(memory,record+0x48u);
    if(memory->fault || !services->refresh || !services->refresh(services->context,payload,8u,0u) ||
       !services->refresh(services->context,game_memory_read_u32(memory,record+0x48u),12u,0u))return 0;
    payload=game_memory_read_u32(memory,record+0x48u);
    if(family!=1u && !services->refresh(services->context,payload,0x78u,payload+0x18u))return 0;
    if(family==1u){if(!gameplay_descriptor_contacts(memory,services,payload,&head))return 0;}
    else head=game_memory_read_u32(memory,payload+0xb0u);
    for(node=head;node && !memory->fault;node=game_memory_read_u32(memory,node+4u)){
        GameAddress other=game_memory_read_u32(memory,node+0xcu);uint32_t parameters[3]={0u,0u,0u},response;float position[3];
        if(++visits>65536u){memory->fault=1;return 0;}
        for(i=0u;i<3u;i++)position[i]=group_float(memory,node+0x10u+i*4u);
        if(game_memory_read_u32(memory,other+0x10u)&2u)for(i=0u;i<3u;i++)parameters[i]=game_memory_read_u32(memory,other+0xb4u+i*4u);
        if(!services->contact || !services->contact(services->context,payload,other,parameters,position,&response))return 0;
        parameters[0]=parameters[1]=parameters[2]=0u;
        if(!services->contact(services->context,other,game_memory_read_u32(memory,record+0x48u),parameters,position,&response))return 0;
        if(!game_memory_read_u32(memory,game_memory_read_u32(memory,record+0x48u)+0x58u)){
            /* A temporary secondary contact list must not lose its pool owner
             * when a response ends the update early. Stored primary lists stay
             * attached to their payload for its own retirement. */
            return family!=1u || descriptor_release_contacts(memory,services,head);
        }
    }
    if(head && !descriptor_release_contacts(memory,services,head))return 0;
    payload=game_memory_read_u32(memory,record+0x48u);
    if(family!=1u && head)game_memory_write_u32(memory,payload+0xb0u,0u);
    return !memory->fault && descriptor_insert(memory,payload);
}

/* The legacy four-slot ring owns its effect handles and uses the shared random
 * table. Slot flags and the position array remain on the same record owner. */
int gameplay_effect_ring_update(GameMemory *memory,const GameplayEffectRingServices *services,GameAddress object){
    int visible;uint32_t tick,count,live;unsigned slot;
    if(!services || !services->visible || !services->visible(services->context,object,&visible))return 0;
    tick=game_memory_read_u32(memory,0x5ac6acu);count=game_memory_read_u32(memory,object+0x80u);
    live=game_memory_read_u32(memory,object+0x88u);
    if(count>65536u || live>4u){memory->fault=1;return 0;}
    if(visible && !(tick&15u)){
        int32_t duration,scaled,desired;uint32_t desired_bits;GameplaySubmissionSample sample;long double random;
        if(!services->duration || !services->duration(services->context,object,&duration))return 0;
        scaled=ui_signed(game_memory_read_u32(memory,object+0x58u)<<3u);
        if(!duration || (scaled==INT32_MIN && duration==-1)){memory->fault=1;return 0;}
        desired_bits=4u-(uint32_t)(scaled/duration);desired=ui_signed(desired_bits);
        if(desired_bits&0x80000000u)desired=0;
        if(desired>ui_signed(count))desired=ui_signed(count);
        if(desired<0 || desired>4){memory->fault=1;return 0;}
        if((uint32_t)desired!=live){
            unsigned visits=0u;sample=gameplay_state_prepare_submission_sample(memory,0u);
            random=(long double)group_float(memory,sample.scalar)*group_float(memory,0x573f80u);
            gameplay_state_commit_submission_sample(memory,&sample);
            if(!isfinite(random) || random<0.0L || random>=4.0L){memory->fault=1;return 0;}
            slot=(unsigned)random;
            while(live!=(uint32_t)desired && !memory->fault){
                GameAddress field=object+0x6cu+slot*4u,effect=game_memory_read_u32(memory,field);
                GameAddress positions=game_memory_read_u32(memory,object+0x7cu),flags=game_memory_read_u32(memory,object+0x84u);uint32_t index;
                if(++visits>16u){memory->fault=1;return 0;}
                if(live>(uint32_t)desired && effect){
                    for(index=0u;index<count;index++){
                        GameAddress position=positions+index*12u;
                        if(group_float(memory,effect+0x18u)==group_float(memory,position) &&
                           group_float(memory,effect+0x20u)==group_float(memory,position+8u)){
                            game_memory_write_u32(memory,flags+index*4u,1u);break;
                        }
                    }
                    if(!services->retire_effect || !services->retire_effect(services->context,effect,0))return 0;
                    game_memory_write_u32(memory,field,0u);live--;
                    game_memory_write_u32(memory,object+0x88u,live);
                }else if(live<(uint32_t)desired && !effect){
                    float position[3];unsigned axis;
                    for(index=0u;index<count;index++)if(game_memory_read_u32(memory,flags+index*4u))break;
                    /* Retail reads past the array if its availability invariant
                     * fails. Reject the broken record instead of inventing a pose. */
                    if(index==count){memory->fault=1;return 0;}
                    game_memory_write_u32(memory,flags+index*4u,0u);
                    for(axis=0u;axis<3u;axis++)position[axis]=group_float(memory,positions+index*12u+axis*4u);
                    if(!services->create_effect || !services->create_effect(services->context,object,position,&effect) || !effect){
                        game_memory_write_u32(memory,flags+index*4u,1u);return 0;
                    }
                    game_memory_write_u32(memory,field,effect);live++;
                    game_memory_write_u32(memory,object+0x88u,live);
                }
                slot=(slot+1u)&3u;
            }
        }
    }
    if(visible)for(slot=4u;slot-- >0u;){
        GameAddress effect=game_memory_read_u32(memory,object+0x6cu+slot*4u);
        /* Original alternates odd and even slots from the low tick bit. */
        if(effect && (slot&1u)==(game_memory_read_u32(memory,0x5ac6acu)&1u)){
            float position[3];unsigned axis;for(axis=0u;axis<3u;axis++)position[axis]=group_float(memory,effect+0x18u+axis*4u);
            if(!services->emit || !services->emit(services->context,object,position))return 0;
        }
    }
    return !memory->fault;
}
int gameplay_effect_ring_reset(GameMemory *memory,const GameplayEffectRingServices *services,GameAddress object,int detach_controller){
    unsigned slot;uint32_t count,index;GameAddress flags,controller;GameplayEntityResetPlan plan;
    if(!services)return 0;
    if(detach_controller>0 && ui_signed(game_memory_read_u32(memory,object+0x58u))<=0 &&
       (!services->phase || !services->phase(services->context,object,1u)))return 0;
    for(slot=0u;slot<4u;slot++){
        GameAddress field=object+0x6cu+slot*4u,effect=game_memory_read_u32(memory,field);
        if(effect){
            if(!services->retire_effect || !services->retire_effect(services->context,effect,1))return 0;
            game_memory_write_u32(memory,field,0u);
        }
    }
    count=game_memory_read_u32(memory,object+0x80u);flags=game_memory_read_u32(memory,object+0x84u);
    if(count>65536u){memory->fault=1;return 0;}
    for(index=0u;index<count;index++)game_memory_write_u32(memory,flags+index*4u,1u);
    plan=gameplay_state_prepare_entity_reset(memory,object);
    (void)level_player_slots_forget_scene_member(object);
    level_effects_retire(object);
    if(memory->fault || !services->retire_catalog || !services->retire_catalog(services->context,plan.owner,plan.selector))return 0;
    (void)gameplay_state_finish_entity_reset(memory,object);
    controller=game_memory_read_u32(memory,object+0xa0u);
    if(detach_controller>0 && controller &&
       (!services->detach_controller || !services->detach_controller(services->context,controller,object)))return 0;
    if(detach_controller>=0)game_memory_write_u32(memory,object+0xa0u,0u);
    return !memory->fault;
}
int gameplay_descriptor_payload_update(GameMemory *memory,const GameplayEffectRingServices *services,GameAddress object){
    uint32_t token,born,interval,tick;
    if(!gameplay_effect_ring_update(memory,services,object))return 0;
    born=game_memory_read_u32(memory,object+0xacu);interval=game_memory_read_u32(memory,object+0xa8u);tick=game_memory_read_u32(memory,0x5ac6acu);
    if(born+interval==tick && game_memory_read_u32(memory,object+0x58u)){
        if(!services->token || !services->token(services->context,object,&token) || !services->notify ||
           !services->notify(services->context,game_memory_read_u32(memory,0x58973cu),token,0))return 0;
        game_memory_write_u8(memory,object+0xa4u,(uint8_t)(game_memory_read_u8(memory,object+0xa4u)|2u));
    }
    return !memory->fault;
}
int gameplay_descriptor_payload_retire(GameMemory *memory,const GameplayEffectRingServices *services,GameAddress object,int detach_controller){
    if(game_memory_read_u8(memory,object+0xa4u)&2u){
        uint32_t token;GameAddress catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+0x14u)&63u)*4u);
        if(!services->token || !services->token(services->context,object,&token) || !services->notify ||
           !services->notify(services->context,catalog,token,1))return 0;
        game_memory_write_u8(memory,object+0xa4u,(uint8_t)(game_memory_read_u8(memory,object+0xa4u)&0xfdu));
    }
    return gameplay_effect_ring_reset(memory,services,object,detach_controller);
}

static int16_t group_direction(GameMemory *memory,float x,float z){
    float scale=group_float(memory,0x5773ecu);long double index;uint16_t angle;uint32_t bits;
    if(fabsl(x)<fabsl(z)){
        index=(long double)x*scale/z+scale;memcpy(&bits,&z,4u);
        if(!isfinite(index) || index<0.0L || index>=2049.0L){memory->fault=1;return 0;}
        angle=game_memory_read_u16(memory,0x62e8b0u+(uint32_t)index*2u);
        angle=(int32_t)bits>0?(uint16_t)(angle&2047u):(uint16_t)(angle+1024u);
    }else{
        index=(long double)z*scale/x+scale;memcpy(&bits,&x,4u);
        if(!isfinite(index) || index<0.0L || index>=2049.0L){memory->fault=1;return 0;}
        angle=(uint16_t)(((int32_t)bits>0?512u:1536u)-game_memory_read_u16(memory,0x62e8b0u+(uint32_t)index*2u));
    }
    return (int16_t)angle;
}
int gameplay_group_place_member(GameMemory *memory,const GameplayGroupServices *services,
    GameAddress owner,GameAddress payload,uint32_t slot){
    GameAddress positions=game_memory_read_u32(memory,owner+0x1cu),angles,template_object=0u,anchor,next;
    uint32_t variant=0u;float target[3],base[3],half=group_float(memory,0x5773e4u),radius,vertical,raw_height;
    int16_t low=32767,high=-32768;unsigned i,corner;long double dx,dz;
    int16_t yaw,pitch;float extent[3],sine,cosine;
    if(slot>0x7fffu){memory->fault=1;return 0;}
    for(i=0u;i<3u;i++){
        target[i]=group_float(memory,positions+slot*12u+i*4u);
        base[i]=group_float(memory,positions+i*4u);
    }
    raw_height=target[1];
    target[1]=(float)((long double)target[1]+(long double)group_float(memory,payload+0x30u)*half);
    if(memory->fault || !services || !services->catalog_value || !services->create ||
        !services->catalog_value(services->context,game_memory_read_u32(memory,owner+0x54u),0x23u,3u,&variant) ||
        !services->create(services->context,0x23u,(uint16_t)variant,&template_object) || !template_object)return 0;
    /* The template remains owned by its factory list, as in the original. */
    radius=(float)((long double)group_float(memory,template_object+0x30u)*half);
    anchor=game_memory_read_u32(memory,template_object+0x50u);next=game_memory_read_u32(memory,anchor+4u);
    game_memory_write_u32(memory,template_object+8u,anchor);game_memory_write_u32(memory,template_object+4u,next);
    if(next)game_memory_write_u32(memory,next+8u,template_object);
    game_memory_write_u32(memory,anchor+4u,template_object);
    base[1]=(float)((long double)base[1]+radius);
    dx=(long double)base[0]-target[0];dz=(long double)base[2]-target[2];
    radius=(float)gameplay_geometry_square_root((float)(dx*dx+dz*dz));
    /* The two angle slots use raw and raised target heights respectively;
     * both use the stored horizontal radius, as the original scratch reuse does. */
    vertical=(float)((long double)base[1]-raw_height);
    pitch=group_direction(memory,vertical,radius);
    yaw=group_direction(memory,(float)((long double)base[1]-target[1]),radius);
    if(pitch>1024)pitch=(int16_t)(pitch-2048);
    if(yaw>1024)yaw=(int16_t)(yaw-2048);
    angles=game_memory_read_u32(memory,owner+0x2cu)+slot*8u;
    game_memory_write_u16(memory,angles,(uint16_t)pitch);game_memory_write_u16(memory,angles+2u,(uint16_t)yaw);
    for(i=0u;i<3u;i++)extent[i]=group_float(memory,payload+0x2cu+i*4u);
    {uint16_t heading=game_memory_read_u16(memory,game_memory_read_u32(memory,owner+0x20u)+slot*2u);
        sine=group_float(memory,0x6298a8u+(heading&2047u)*4u);
        cosine=group_float(memory,0x6298a8u+((heading+512u)&2047u)*4u);}
    for(corner=0u;corner<4u;corner++){
        float x=corner<2u?-extent[0]:extent[0],z=corner==0u || corner==3u?extent[2]:-extent[2];
        float rotated_x=(float)((long double)x*cosine-(long double)z*sine);
        float rotated_z=(float)((long double)x*sine+(long double)z*cosine);
        float world_x=(float)((long double)target[0]+rotated_x),world_z=(float)((long double)target[2]+rotated_z);
        int16_t angle=group_direction(memory,(float)((long double)world_x-base[0]),(float)((long double)world_z-base[2]));
        if(angle<low)low=angle;
        if(angle>high)high=angle;
    }
    if((int32_t)high-low>1024){int16_t swap=low;low=high;high=swap;}
    game_memory_write_u16(memory,angles+4u,(uint16_t)low);game_memory_write_u16(memory,angles+6u,(uint16_t)high);
    return !memory->fault;
}
int gameplay_group_bind_member(GameMemory *memory,const GameplayGroupServices *services,
    const GameplayUnitServices *units,GameAddress owner,int16_t identity,uint32_t slot,uint16_t *status){
    static const uint8_t kinds[10]={0x80u,0x81u,0x82u,0x83u,0x89u,0x84u,0x85u,0x86u,0x87u,0x88u};
    uint32_t mode=game_memory_read_u32(memory,0x624f8cu),price=0u,budget=0u,variant=0u;
    GameAddress catalog=game_memory_read_u32(memory,owner+0x54u),payload=0u,record,array;
    unsigned family=identity==5?1u:identity==10?2u:0u;int handled;
    if(!services || !status)return 0;
    if(mode==10u){
        if(!gameplay_collection_eligibility(memory,units,owner,identity,&slot,status))return 0;
        if(*status)return 1;
        handled=gameplay_catalog_unit_price(memory,catalog,identity,&price);
        if(handled<0 || (!handled && (!units || !units->price || !units->price(units->context,catalog,identity,&price))) ||
            !services->budget || !services->budget(services->context,catalog,price,1,&budget))return 0;
    }
    if(identity<1 || identity>10){*status=0xffffu;return !memory->fault;}
    if(slot>0x7fffu || (int32_t)slot>=(int16_t)game_memory_read_u16(memory,owner+0x24u)){memory->fault=1;return 0;}
    array=game_memory_read_u32(memory,owner+0x18u);record=game_memory_read_u32(memory,array+slot*4u);
    if(record){
        if(!services->member_destroy || !services->member_destroy(services->context,record))return 0;
        record=game_memory_read_u32(memory,array+slot*4u);
        if(record && !group_release(services,record))return 0;
        game_memory_write_u32(memory,array+slot*4u,0u);
    }
    record=group_allocate(services,0x4cu);if(!record)return 0;
    if(!services->catalog_value || !services->create ||
        !services->catalog_value(services->context,catalog,kinds[identity-1],3u,&variant) ||
        !services->create(services->context,kinds[identity-1],(uint16_t)variant,&payload) || !payload){
        (void)group_release(services,record);return 0;
    }
    if(!gameplay_group_member_construct(memory,record,owner,(uint16_t)slot,payload,(uint16_t)identity,family)){
        game_memory_write_u32(memory,array+slot*4u,record);return 0;
    }
    game_memory_write_u32(memory,array+slot*4u,record);
    if(!gameplay_group_place_member(memory,services,owner,payload,slot) || !services->member_activate ||
        !services->member_activate(services->context,record,game_memory_read_u32(memory,0x5ac6acu),mode!=10u))return 0;
    game_memory_write_u16(memory,owner+0x26u,(uint16_t)(game_memory_read_u16(memory,owner+0x26u)+1u));
    *status=0u;return !memory->fault;
}

int gameplay_group_note_attack(GameMemory *memory,GameAddress controller,GameAddress member,GameAddress other){
    uint32_t member_index=game_memory_read_u32(memory,member),local,attacker,flags,type,choice,offset=0u;
    GameAddress members=game_memory_read_u32(memory,controller+0x26cu),state;
    if(game_memory_read_u32(memory,members+member_index*4u)!=member)return !memory->fault;
    local=game_memory_read_u32(memory,controller+0x14u)&63u;
    attacker=game_memory_read_u32(memory,other+0x14u)&63u;
    if(game_memory_read_u32(memory,0x5f0160u+attacker*24u+local*4u) || attacker>=4u)return !memory->fault;
    state=game_memory_read_u32(memory,member+0x18u);
    if(game_memory_read_u32(memory,state)){
        GameAddress timestamps=game_memory_read_u32(memory,controller+0x31cu);
        game_memory_write_u32(memory,timestamps+member_index*4u,game_memory_read_u32(memory,0x5ac6acu));
        game_memory_write_u32(memory,controller+0x338u,game_memory_read_u32(memory,0x5ac6acu));
    }
    game_memory_write_u32(memory,controller+0x2b4u,attacker);
    type=game_memory_read_u8(memory,other+12u);
    if(type==10u)return !memory->fault;
    choice=type==0x41u?game_memory_read_u8(memory,other+0xccu):0u;
    if(type==0x41u && choice==10u)return !memory->fault;
    flags=game_memory_read_u32(memory,other+0x10u);
    if(flags&4u)type=game_memory_read_u8(memory,game_memory_read_u32(memory,other+0xd4u)+12u);
    else if((flags&0x40u) && type!=11u){
        if(type==0x41u)type=choice;
        else{
            /* This original branch reads an uninitialized stack local. Keep
             * the valid attacker/timestamp state; no invented category count. */
            return !memory->fault;
        }
    }
    /*4bc7bc/4bc7e0 classify the resolved attacker, including projectile owners.
     *These are behavior selectors in .text, not live game storage. */
    switch(type){
    case 0x0b:offset=0x194u;break;case 0x20:offset=0x144u;break;
    case 0x21:offset=0x154u;break;case 0x26:offset=0x174u;break;
    case 0x27:offset=0x164u;break;case 0x40:offset=0x184u;break;
    default:return !memory->fault;
    }
    game_memory_write_u32(memory,controller+offset+attacker*4u,
        game_memory_read_u32(memory,controller+offset+attacker*4u)+1u);
    return !memory->fault;
}
static GameAddress contact_binding(GameMemory *memory,GameAddress object){
    uint8_t type=game_memory_read_u8(memory,object+12u);
    return game_memory_read_u32(memory,object+((type==0x88u || type==0x89u)?0xb4u:0xa0u));
}
static int contact_reference(GameMemory *memory,GameAddress object,uint32_t *index){
    uint32_t type=game_memory_read_u8(memory,object+12u),offset;
    GameAddress binding;
    /*42dcf0/42dd00: the same binding families used by the real contacts. */
    switch(type){
    case 0x23:offset=0x294u;break;
    case 0x80:case 0x81:case 0x82:case 0x83:
    case 0x84:case 0x85:case 0x86:case 0x87:offset=0xa0u;break;
    case 0x88:case 0x89:offset=0xb4u;break;
    /* The default original branch uses an uninitialized stack local. */
    default:return 0;
    }
    binding=game_memory_read_u32(memory,object+offset);
    *index=game_memory_read_u32(memory,game_memory_read_u32(memory,binding+4u));
    return !memory->fault;
}
static int contact_local_feedback(GameMemory *memory,GameAddress object,uint32_t index,
    const GamePresentationFeedbackServices *services){
    GameAddress records=game_memory_read_u32(memory,0x639a70u),record,owner,scene,point,bank;
    uint32_t elapsed,tick;long double distance;int accepted;int16_t sounds[8]={0x57,0x5d,0,0,0x5f,-1,-1,-1};
    record=game_memory_read_u32(memory,records+index*4u);owner=game_memory_read_u32(memory,0x5f0208u);
    if(game_memory_read_u32(memory,record+0x54u)!=owner)return !memory->fault;
    tick=game_memory_read_u32(memory,0x5ac6acu);elapsed=tick-game_memory_read_u32(memory,owner+0x590u);
    if(!(ui_signed(elapsed)>160 && game_memory_read_u32(memory,owner+0x594u)!=index) && ui_signed(elapsed)<=320)return !memory->fault;
    owner=game_memory_read_u32(memory,0x5f0208u);scene=game_memory_read_u32(memory,owner+0x448u);
    distance=game_leaf_horizontal_distance_squared(memory,object+0x18u,scene+12u);
    if(!(distance>(long double)group_float(memory,0x5721a4u)))return !memory->fault;
    point=game_memory_read_u32(memory,record+0x1cu);
    sounds[3]=(int16_t)game_presentation_location_token(memory,group_float(memory,point+8u),0);
    sounds[2]=(int16_t)game_presentation_location_token(memory,group_float(memory,point),1);
    bank=game_memory_read_u32(memory,0x628a90u);
    if(memory->fault || !services->clear_queue || !services->submit_queue || !services->clear_queue(services->context,bank))return 0;
    bank=game_memory_read_u32(memory,0x628a90u);
    if(memory->fault || !services->submit_queue(services->context,bank,sounds,&accepted))return 0;
    if(accepted){
        owner=game_memory_read_u32(memory,0x5f0208u);game_memory_write_u16(memory,owner+0x598u,0x57u);
        tick=game_memory_read_u32(memory,0x5ac6acu);owner=game_memory_read_u32(memory,0x5f0208u);game_memory_write_u32(memory,owner+0x590u,tick);
        owner=game_memory_read_u32(memory,0x5f0208u);game_memory_write_u32(memory,owner+0x58cu,game_memory_read_u32(memory,owner+0x590u));
        owner=game_memory_read_u32(memory,0x5f0208u);game_memory_write_u32(memory,owner+0x594u,index);
    }
    return !memory->fault;
}
/*475fb0/4760d0 and533390: native type classifications replace the original
 *code-section jump tables. Counters remain in the existing catalog owner. */
int gameplay_statistics_note(GameMemory *memory,GameAddress statistics,uint8_t type,int loss,uint32_t *value){
    uint32_t count,offset;
    switch(type){
    case 0x20:offset=0x24;break;case 0x21:offset=0x10;break;
    case 0x22:case 0x24:case 0x25:case 0x28:offset=0x20;break;
    case 0x23:offset=0x1c;break;case 0x26:offset=0x18;break;
    case 0x27:offset=0x14;break;case 0x29:offset=0x2c;break;
    case 0x40:offset=0x0c;break;
    case 0x60:case 0x62:case 0x63:case 0x64:case 0x77:
    case 0x80:case 0x81:case 0x82:case 0x83:case 0x84:
    case 0x85:case 0x86:case 0x87:case 0x88:case 0x89:offset=0x28;break;
    default:return !memory->fault;
    }
    if(loss)offset+=0x24;
    if(memory->fault)return 0;
    count=game_memory_read_u32(memory,statistics+offset)+1u;
    game_memory_write_u32(memory,statistics+offset,count);if(value)*value=count;
    return !memory->fault;
}
uint32_t gameplay_statistics_reward(GameMemory *memory,uint8_t type){
    (void)memory;
    switch(type){
    case 0x20:case 0x23:return 40;
    case 0x26:return 15;case 0x27:return 20;
    case 0x29:case 0x60:case 0x80:case 0x84:return 50;
    case 0x63:case 0x81:case 0x85:return 80;
    case 0x62:case 0x82:case 0x86:case 0x88:case 0x89:return 90;
    case 0x64:case 0x83:case 0x87:return 100;
    default:return 10;
    }
}
int gameplay_entity_note_kill(GameMemory *memory,GameAddress object,GameAddress other,void *context,GameplayKillCredit credit){
    uint32_t owner,attacker,reward;GameAddress catalog;uint8_t type;
    if(!other)return !memory->fault;
    attacker=game_memory_read_u32(memory,other+0x14u)&63u;owner=game_memory_read_u32(memory,object+0x14u)&63u;
    type=game_memory_read_u8(memory,object+0xcu);catalog=game_memory_read_u32(memory,0x5f0210u+owner*4u);
    if(!gameplay_statistics_note(memory,catalog+0x148u,type,1,NULL))return 0;
    if(owner==attacker)return !memory->fault;
    catalog=game_memory_read_u32(memory,0x5f0210u+attacker*4u);
    reward=gameplay_statistics_reward(memory,game_memory_read_u8(memory,object+0xcu));
    if(memory->fault || !credit || !credit(context,catalog,reward))return 0;
    catalog=game_memory_read_u32(memory,0x5f0210u+attacker*4u);
    return gameplay_statistics_note(memory,catalog+0x148u,game_memory_read_u8(memory,object+0xcu),0,NULL);
}
int gameplay_entity_contact(GameMemory *memory,GameAddress object,GameAddress other,
    const GameplayEntityContactServices *services){
    uint32_t health=game_memory_read_u32(memory,object+0x58u),flags,type,index;
    GameAddress binding,record,root,owner;int accepted;int32_t amount;
    if(ui_signed(health)<=0)return !memory->fault;
    if(!native_entity_type_activity(memory,other,&accepted))return 0;
    flags=game_memory_read_u32(memory,other+0x10u);type=game_memory_read_u8(memory,other+12u);
    if(!accepted && !(flags&4u) && type!=0x41u && type!=11u && type!=10u)return !memory->fault;
    flags=game_memory_read_u32(memory,object+0x10u);
    if(flags&0x100u){
        binding=contact_binding(memory,object);
        if(binding){
            record=game_memory_read_u32(memory,binding+4u);root=game_memory_read_u32(memory,record+0x18u);
            root=game_memory_read_u32(memory,root);
            if(root)game_memory_write_u32(memory,game_memory_read_u32(memory,root+0x48u)+0x12cu,0u);
        }
    }
    if(memory->fault || !services || !services->damage || !services->damage(services->context,object,other,&amount))return 0;
    health=game_memory_read_u32(memory,object+0x58u)-(uint32_t)amount;
    game_memory_write_u32(memory,object+0x58u,health);
    if(ui_signed(health)<=0){
        GameAddress table=game_memory_read_u32(memory,object),method;
        if(memory->fault || !services->killed || !services->reset || !services->killed(services->context,object,other))return 0;
        method=game_memory_read_u32(memory,table+0x20u);
        return !memory->fault && services->reset(services->context,object,method);
    }
    flags=game_memory_read_u32(memory,object+0x10u);
    if(!(flags&0x100u) || (flags&0x20000u))return !memory->fault;
    owner=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+0x14u)&63u)*4u);
    if(game_memory_read_u16(memory,owner+10u)==4u){
        type=game_memory_read_u8(memory,object+12u);
        if(type!=0x88u && type!=0x89u && !(flags&0x80u))return !memory->fault;
        binding=contact_binding(memory,object);record=game_memory_read_u32(memory,binding+4u);
        return !memory->fault && gameplay_group_note_attack(memory,game_memory_read_u32(memory,owner+0x2e0u),record,other);
    }
    if(!contact_reference(memory,object,&index))return !memory->fault;
    return contact_local_feedback(memory,object,index,&services->feedback);
}
