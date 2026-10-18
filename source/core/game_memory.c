#include "game_memory.h"
#include "recyclable_records.h"
#include "game_state_ops.h"
#include "game_intrusive_list.h"
#include "gameplay_geometry.h"
#include "game_initializers.h"
#include "gameplay_state.h"
#include "scene_projection_storage.h"
#include "vehicle_targeting.h"

#include <string.h>

void game_memory_initialize(GameMemory *memory, uint8_t *data,
    GameAddress base, size_t size) {
    memset(memory, 0, sizeof(*memory));
    memory->data = data;
    memory->base = base;
    memory->size = size;
}

int game_memory_contains(const GameMemory *memory, GameAddress address,
    size_t size) {
    size_t offset;
    if (address < memory->base || size > memory->size) return 0;
    offset = (size_t)(address - memory->base);
    return offset <= memory->size - size;
}

static int game_memory_fail(GameMemory *memory, GameAddress address,
    size_t size, int write) {
    memory->fault = 1;
    memory->fault_address = address;
    memory->fault_size = size;
    memory->fault_write = write;
    return 0;
}

int game_memory_read(GameMemory *memory, GameAddress address,
    void *destination, size_t size) {
    if (memory->fault) return 0;
    if(address>=RECYCLABLE_RECORD_BASE && address<RECYCLABLE_RECORD_END){
        if(recyclable_record_read(address,destination,size))return 1;
        memset(destination,0,size);return game_memory_fail(memory,address,size,0);
    }
    if (memory->read_fallback && ((address >= memory->owned_begin && address < memory->owned_end) ||
        (memory->owned_pages && memory->owned_pages[address >> 12u])) && memory->read_fallback(
            memory->fallback_context,address,destination,size)) return 1;
    if (game_memory_contains(memory, address, size)) {
        memcpy(destination, memory->data + (address - memory->base), size);
        return 1;
    }
    if (memory->read_fallback && memory->read_fallback(
            memory->fallback_context, address, destination, size))
        return 1;
    memset(destination, 0, size);
    return game_memory_fail(memory, address, size, 0);
}

int game_memory_write(GameMemory *memory, GameAddress address,
    const void *source, size_t size) {
    if (memory->fault) return 0;
    if(address>=RECYCLABLE_RECORD_BASE && address<RECYCLABLE_RECORD_END)
        return recyclable_record_write(address,source,size)?1:game_memory_fail(memory,address,size,1);
    if (memory->write_fallback && ((address >= memory->owned_begin && address < memory->owned_end) ||
        (memory->owned_pages && memory->owned_pages[address >> 12u])) && memory->write_fallback(
            memory->fallback_context,address,source,size)) return 1;
    if (!memory->trace_writes && game_memory_contains(memory, address, size)) {
        memcpy(memory->data + (address - memory->base), source, size);
        return 1;
    }
    if (memory->write_fallback && memory->write_fallback(
            memory->fallback_context, address, source, size))
        return 1;
    return game_memory_fail(memory, address, size, 1);
}

uint8_t game_memory_read_u8(GameMemory *memory, GameAddress address) {
    uint8_t value = 0u;
    game_memory_read(memory, address, &value, sizeof(value));
    return value;
}

uint16_t game_memory_read_u16(GameMemory *memory, GameAddress address) {
    uint16_t value = 0u;
    game_memory_read(memory, address, &value, sizeof(value));
    return value;
}

uint32_t game_memory_read_u32(GameMemory *memory, GameAddress address) {
    uint32_t value = 0u;
    game_memory_read(memory, address, &value, sizeof(value));
    return value;
}

void game_memory_write_u8(GameMemory *memory, GameAddress address,
    uint8_t value) {
    game_memory_write(memory, address, &value, sizeof(value));
}

void game_memory_write_u16(GameMemory *memory, GameAddress address,
    uint16_t value) {
    game_memory_write(memory, address, &value, sizeof(value));
}

void game_memory_write_u32(GameMemory *memory, GameAddress address,
    uint32_t value) {
    game_memory_write(memory, address, &value, sizeof(value));
}

int game_memory_fill_u32(GameMemory *memory, GameAddress address,
        uint32_t count, uint32_t value, int reverse) {
    uint32_t index;
    if (!memory) return 0;
    for (index = 0u; index < count; ++index) {
        game_memory_write_u32(memory,
            address + index * (reverse ? 0xfffffffcu : 4u), value);
        if (memory->fault) return 0;
    }
    return 1;
}

int game_memory_fill_u8(GameMemory *memory, GameAddress address,
        uint32_t count, uint8_t value, int reverse) {
    uint32_t index;
    if (!memory) return 0;
    for (index = 0u; index < count; ++index) {
        game_memory_write_u8(memory,
            address + (reverse ? 0u - index : index), value);
        if (memory->fault) return 0;
    }
    return 1;
}

uint32_t game_memory_copy_elements(GameMemory *memory,
        GameAddress destination, GameAddress source, uint32_t count,
        unsigned element_bytes, int reverse) {
    uint32_t index;
    uint32_t step = reverse ? 0u - element_bytes : element_bytes;
    uint8_t item[4];
    if (!memory || (element_bytes != 1u && element_bytes != 2u &&
            element_bytes != 4u)) return 0u;
    for (index = 0u; index < count; ++index) {
        if (!game_memory_read(memory, source, item, element_bytes) ||
                !game_memory_write(memory, destination, item, element_bytes))
            break;
        source += step;
        destination += step;
    }
    return index;
}

uint32_t game_memory_fill_elements(GameMemory *memory,
        GameAddress destination, uint32_t count, uint32_t value,
        unsigned element_bytes, int reverse) {
    uint32_t index;
    uint32_t step = reverse ? 0u - element_bytes : element_bytes;
    if (!memory || (element_bytes != 1u && element_bytes != 2u &&
            element_bytes != 4u)) return 0u;
    for (index = 0u; index < count; ++index) {
        if (element_bytes == 1u)
            game_memory_write_u8(memory, destination, (uint8_t)value);
        else if (element_bytes == 2u)
            game_memory_write_u16(memory, destination, (uint16_t)value);
        else
            game_memory_write_u32(memory, destination, value);
        if (memory->fault) break;
        destination += step;
    }
    return index;
}

GameByteScan game_memory_scan_byte(GameMemory *memory, GameAddress address,
        uint32_t count, uint8_t needle, int reverse) {
    GameByteScan result = {0u, 0u};
    uint32_t step = reverse ? 0xffffffffu : 1u;
    if (!memory) return result;
    while (result.scanned < count) {
        uint8_t value = game_memory_read_u8(memory, address);
        if (memory->fault) break;
        result.last = value;
        result.scanned++;
        if (value == needle) break;
        address += step;
    }
    return result;
}

/* Recyclable record ownership and algorithms; kept with memory access so
 * existing native consumers share this owner without extra link layers. */
#include "recyclable_records.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct RecordSlot {uint8_t bytes[0x58];unsigned size;uint32_t next;} RecordSlot;
static RecordSlot *records;
static uint32_t used,capacity,free_slot;
static RecordSlot *record_at(GameAddress address,size_t bytes){
    uint32_t index,offset;
    if(address<RECYCLABLE_RECORD_BASE || address>=RECYCLABLE_RECORD_END)return NULL;
    index=(address-RECYCLABLE_RECORD_BASE)/128u;offset=(address-RECYCLABLE_RECORD_BASE)%128u;
    if(index>=used || !records[index].size || offset>records[index].size || bytes>records[index].size-offset)return NULL;
    return &records[index];
}
GameAddress recyclable_record_allocate(size_t size){
    uint32_t index;
    if(size!=0x24u && size!=0x58u)return 0u;
    if(free_slot){index=free_slot-1u;free_slot=records[index].next;}
    else{
        if(used==capacity){
            uint32_t next=capacity?capacity*2u:256u;RecordSlot *grown;
            if(next>131072u)return 0u;
            grown=realloc(records,(size_t)next*sizeof(*records));if(!grown)return 0u;
            memset(grown+capacity,0,(size_t)(next-capacity)*sizeof(*records));records=grown;capacity=next;
        }
        index=used++;
    }
    memset(&records[index],0,sizeof(records[index]));records[index].size=(unsigned)size;
    return RECYCLABLE_RECORD_BASE+index*128u;
}
int recyclable_record_release(GameAddress address){
    RecordSlot *slot=record_at(address,0u);uint32_t index;
    if(!slot || (address-RECYCLABLE_RECORD_BASE)%128u)return 0;
    index=(address-RECYCLABLE_RECORD_BASE)/128u;slot->size=0u;slot->next=free_slot;free_slot=index+1u;return 1;
}
int recyclable_record_read(GameAddress address,void *output,size_t bytes){
    RecordSlot *slot=record_at(address,bytes);if(!slot || (!output && bytes))return 0;
    if(bytes)memcpy(output,slot->bytes+(address-RECYCLABLE_RECORD_BASE)%128u,bytes);
    return 1;
}
int recyclable_record_write(GameAddress address,const void *input,size_t bytes){
    RecordSlot *slot=record_at(address,bytes);if(!slot || (!input && bytes))return 0;
    if(bytes)memcpy(slot->bytes+(address-RECYCLABLE_RECORD_BASE)%128u,input,bytes);
    return 1;
}
void recyclable_record_shutdown(void){free(records);records=NULL;used=capacity=free_slot=0u;}
static GameAddress allocate_record(const RecyclableRecordServices *services,size_t size){
    return services && services->allocate?services->allocate(services->context,size):recyclable_record_allocate(size);
}
static int release_record(const RecyclableRecordServices *services,GameAddress address){
    return services && services->release?services->release(services->context,address):recyclable_record_release(address);
}
static GameAddress pop_record(GameMemory *memory,GameAddress global){
    GameAddress record=game_memory_read_u32(memory,global),next=game_memory_read_u32(memory,record+4u);
    game_memory_write_u32(memory,global,next);if(next)game_memory_write_u32(memory,next+8u,0u);
    game_memory_write_u32(memory,record+4u,0u);return record;
}
static int initialize_inner_record(GameMemory *memory,GameAddress record){
    game_memory_write_u32(memory,record,0x573560u);game_memory_write_u32(memory,record+4u,0u);game_memory_write_u32(memory,record+8u,0u);
    game_memory_write_u8(memory,record+0xcu,0u);game_memory_write_u8(memory,record+0xdu,0u);
    game_memory_write_u32(memory,record+0x10u,record+0x18u);game_memory_write_u32(memory,record+0x14u,record+0x38u);
    return !memory->fault;
}
int recyclable_records_preallocate(GameMemory *memory,uint32_t outer_count,uint32_t inner_count,const RecyclableRecordServices *services){
    unsigned group;uint32_t index,counts[2]={inner_count,outer_count};
    for(group=0u;group<2u;group++){
        GameAddress head=0u,global=group?0x585378u:0x585374u;uint32_t count=counts[group];
        if(!count || count>=0x80000000u)continue;
        if(count>65536u)return 0;
        /* Original reserves one head plus count following records. */
        for(index=0u;index<=count;index++){
            GameAddress record=allocate_record(services,group?0x24u:0x58u);
            if(!record || !(group?recyclable_outer_initialize(memory,record,services):initialize_inner_record(memory,record)))return 0;
            game_memory_write_u32(memory,record+4u,head);if(head)game_memory_write_u32(memory,head+8u,record);
            head=record;game_memory_write_u32(memory,global,head);if(memory->fault)return 0;
        }
    }
    return !memory->fault;
}
GameAddress recyclable_inner_acquire(GameMemory *memory,GameAddress source,uint8_t first,uint8_t second,
    const RecyclableRecordServices *services){
    GameAddress record;unsigned index;
    if(source && game_memory_read_u8(memory,source+0xdu)<32u)record=source;
    else if(game_memory_read_u32(memory,0x585374u)){
        record=pop_record(memory,0x585374u);game_memory_write_u8(memory,record+0xcu,0u);game_memory_write_u8(memory,record+0xdu,0u);
    }else{
        game_memory_write_u32(memory,0x585380u,game_memory_read_u32(memory,0x585380u)+1u);
        record=allocate_record(services,0x58u);if(!record)return 0u;
        if(!initialize_inner_record(memory,record))return 0u;
    }
    index=game_memory_read_u8(memory,record+0xdu);
    game_memory_write_u8(memory,game_memory_read_u32(memory,record+0x10u)+index,first);
    game_memory_write_u8(memory,game_memory_read_u32(memory,record+0x14u)+index,second);
    game_memory_write_u8(memory,record+0xdu,(uint8_t)(index+1u));return memory->fault?0u:record;
}
static int retire_inner(GameMemory *memory,GameAddress node,const RecyclableRecordServices *services){
    if(!game_memory_read_u32(memory,0x585370u)){
        game_memory_write_u32(memory,0x585384u,game_memory_read_u32(memory,0x585384u)+1u);
        game_memory_write_u32(memory,node,0x572230u);return !memory->fault && release_record(services,node);
    }else{
        GameAddress head=game_memory_read_u32(memory,0x585374u);
        if(head)game_memory_write_u32(memory,head+8u,node);
        game_memory_write_u32(memory,node+4u,head);game_memory_write_u32(memory,node+8u,0u);game_memory_write_u32(memory,0x585374u,node);
        return !memory->fault;
    }
}
int recyclable_outer_clear(GameMemory *memory,GameAddress outer,const RecyclableRecordServices *services){
    GameAddress node;unsigned visits=0u;
    game_memory_write_u32(memory,outer+0x14u,0u);
    while((node=game_memory_read_u32(memory,outer+0xcu))!=0u){
        if(memory->fault || ++visits>65536u)return 0;
        game_memory_write_u32(memory,outer+0xcu,game_memory_read_u32(memory,node+4u));
        if(!retire_inner(memory,node,services))return 0;
    }
    game_memory_write_u32(memory,outer+0xcu,0u);game_memory_write_u32(memory,outer+0x10u,0u);return !memory->fault;
}
int recyclable_outer_initialize(GameMemory *memory,GameAddress outer,const RecyclableRecordServices *services){
    GameAddress inner=recyclable_inner_acquire(memory,0u,0u,0u,services);unsigned offset;
    if(!inner)return 0;
    game_memory_write_u32(memory,outer,0x573564u);game_memory_write_u32(memory,outer+4u,0u);game_memory_write_u32(memory,outer+8u,0u);
    for(offset=0x14u;offset<=0x20u;offset+=4u)game_memory_write_u32(memory,outer+offset,0u);
    game_memory_write_u32(memory,outer+0xcu,inner);game_memory_write_u32(memory,outer+0x10u,inner);
    game_memory_write_u8(memory,inner+0xcu,0u);game_memory_write_u8(memory,inner+0xdu,0u);return !memory->fault;
}
static int record_copy_chain(GameMemory *memory,GameAddress destination,GameAddress input,const RecyclableRecordServices *services){
    GameAddress target=game_memory_read_u32(memory,destination+0xcu);unsigned visits=0u;
    while(target && input){
        if(++visits>65536u)return 0;
        if(game_memory_copy_elements(memory,target+0x18u,input+0x18u,16u,4u,0)!=16u)return 0;
        game_memory_write_u8(memory,target+0xdu,game_memory_read_u8(memory,input+0xdu));
        target=game_memory_read_u32(memory,target+4u);input=game_memory_read_u32(memory,input+4u);if(memory->fault)return 0;
    }
    if(target){
        GameAddress previous=game_memory_read_u32(memory,target+8u);
        game_memory_write_u32(memory,destination+0x10u,previous);game_memory_write_u32(memory,previous+4u,0u);
        while(target){GameAddress next=game_memory_read_u32(memory,target+4u);if(++visits>65536u || !retire_inner(memory,target,services))return 0;target=next;}
    }
    while(input){
        GameAddress next=recyclable_inner_acquire(memory,0u,0u,0u,services),tail=game_memory_read_u32(memory,destination+0x10u);
        if(!next || ++visits>65536u)return 0;
        game_memory_write_u32(memory,tail+4u,next);game_memory_write_u32(memory,next+8u,tail);game_memory_write_u32(memory,destination+0x10u,next);
        if(game_memory_copy_elements(memory,next+0x18u,input+0x18u,16u,4u,0)!=16u)return 0;
        game_memory_write_u8(memory,next+0xdu,game_memory_read_u8(memory,input+0xdu));input=game_memory_read_u32(memory,input+4u);
        if(memory->fault)return 0;
    }
    return !memory->fault;
}
int recyclable_outer_copy(GameMemory *memory,GameAddress destination,GameAddress source,const RecyclableRecordServices *services){
    unsigned offset;
    if(!record_copy_chain(memory,destination,game_memory_read_u32(memory,source+0xcu),services))return 0;
    for(offset=0x14u;offset<=0x20u;offset+=4u)game_memory_write_u32(memory,destination+offset,game_memory_read_u32(memory,source+offset));
    return !memory->fault;
}
GameAddress recyclable_outer_acquire(GameMemory *memory,GameAddress source,const RecyclableRecordServices *services){
    GameAddress record;
    if(game_memory_read_u32(memory,0x585378u))record=pop_record(memory,0x585378u);
    else{
        game_memory_write_u32(memory,0x58537cu,game_memory_read_u32(memory,0x58537cu)+1u);
        record=allocate_record(services,0x24u);if(!record)return 0u;
        if(!recyclable_outer_initialize(memory,record,services)){(void)release_record(services,record);return 0u;}
    }
    if(source && !recyclable_outer_copy(memory,record,source,services))return 0u;
    return memory->fault?0u:record;
}
int recyclable_record_destroy(GameMemory *memory,GameAddress record,unsigned flags,const RecyclableRecordServices *services){
    uint32_t table=game_memory_read_u32(memory,record);
    if(table==0x573564u && !recyclable_outer_clear(memory,record,services))return 0;
    if(table!=0x573564u && table!=0x573560u && table!=0x572230u)return 0;
    game_memory_write_u32(memory,record,0x572230u);
    return !memory->fault && (!(flags&1u) || release_record(services,record));
}
int recyclable_outer_reset_payload(GameMemory *memory,GameAddress outer,const RecyclableRecordServices *services){
    GameAddress first=game_memory_read_u32(memory,outer+0xcu),node;unsigned offset,visits=0u;
    for(offset=0x14u;offset<=0x20u;offset+=4u)game_memory_write_u32(memory,outer+offset,0u);
    while((node=game_memory_read_u32(memory,first+4u))!=0u){
        if(++visits>65536u || memory->fault)return 0;
        game_memory_write_u32(memory,first+4u,game_memory_read_u32(memory,node+4u));
        if(!retire_inner(memory,node,services))return 0;
    }
    game_memory_write_u32(memory,outer+0x10u,first);
    game_memory_write_u8(memory,first+0xcu,0u);game_memory_write_u8(memory,first+0xdu,0u);return !memory->fault;
}
int recyclable_outer_retire(GameMemory *memory,GameAddress node,const RecyclableRecordServices *services){
    if(!game_memory_read_u32(memory,0x585370u)){
        game_memory_write_u32(memory,0x585388u,game_memory_read_u32(memory,0x585388u)+1u);
        return !node || recyclable_record_destroy(memory,node,1u,services);
    }else{
        GameAddress head=game_memory_read_u32(memory,0x585378u);
        if(head)game_memory_write_u32(memory,head+8u,node);
        game_memory_write_u32(memory,node+4u,head);game_memory_write_u32(memory,node+8u,0u);game_memory_write_u32(memory,0x585378u,node);
        return recyclable_outer_reset_payload(memory,node,services);
    }
}
int recyclable_container_reset(GameMemory *memory,GameAddress owner,uint8_t kind,uint8_t subtype,
    uint32_t value,int alternate,int clear_table,const RecyclableRecordServices *services){
    GameAddress node,retained;unsigned visits=0u;
    game_memory_write_u8(memory,owner+0xcu,kind);game_memory_write_u8(memory,owner+0xdu,subtype);
    game_memory_write_u32(memory,owner+0x10u,0u);game_memory_write_u32(memory,owner+0x14u,alternate?0xbf800000u:value);
    game_memory_write_u32(memory,owner+0x18u,alternate?value:0xbf800000u);
    while((node=game_memory_read_u32(memory,owner+4u))!=0u){
        if(++visits>65536u || memory->fault)return 0;
        game_memory_write_u32(memory,owner+4u,game_memory_read_u32(memory,node+4u));
        if(!recyclable_outer_retire(memory,node,services))return 0;
    }
    retained=game_memory_read_u32(memory,owner+0x1cu);
    if(!retained){retained=recyclable_outer_acquire(memory,0u,services);if(!retained)return 0;game_memory_write_u32(memory,owner+0x1cu,retained);}
    if(!recyclable_outer_reset_payload(memory,retained,services))return 0;
    game_memory_write_u32(memory,owner+4u,0u);game_memory_write_u32(memory,owner+8u,0u);
    return (!clear_table || game_memory_fill_u32(memory,owner+0x20u,0x4000u,0u,0)) && !memory->fault;
}
static float record_float(GameMemory *memory,GameAddress address){
    uint32_t bits=game_memory_read_u32(memory,address);float value;memcpy(&value,&bits,4u);return value;
}
static uint32_t record_float_bits(float value){uint32_t bits;memcpy(&bits,&value,4u);return bits;}
static int32_t record_signed(uint32_t value){return (int32_t)(value<0x80000000u?(int64_t)value:(int64_t)value-4294967296LL);}
GameAddress recyclable_outer_append(GameMemory *memory,GameAddress outer,uint8_t x,uint8_t y,
    uint32_t cost,uint32_t integer_cost,const RecyclableRecordServices *services){
    GameAddress tail=game_memory_read_u32(memory,outer+0x10u);
    GameAddress point=recyclable_inner_acquire(memory,tail,x,y,services);float delta;
    if(!point)return 0u;
    if(point!=tail){
        game_memory_write_u32(memory,tail+4u,point);game_memory_write_u32(memory,point+8u,tail);
        game_memory_write_u32(memory,outer+0x10u,point);
    }
    memcpy(&delta,&cost,4u);
    game_memory_write_u32(memory,outer+0x14u,record_float_bits((float)((long double)record_float(memory,outer+0x14u)+delta)));
    game_memory_write_u32(memory,outer+0x20u,game_memory_read_u32(memory,outer+0x20u)+integer_cost);
    return memory->fault?0u:point;
}
int recyclable_container_keep(GameMemory *memory,GameAddress owner,GameAddress node,
    uint32_t candidate_cost,uint32_t candidate_integer,const RecyclableRecordServices *services){
    GameAddress next,previous;float cost;long double remaining;
    if(!node)return 0;
    memcpy(&cost,&candidate_cost,4u);
    remaining=((long double)record_signed(game_memory_read_u32(memory,node+0x20u))-
        record_signed(candidate_integer)-cost)+record_float(memory,node+0x14u);
    if(!(remaining>record_float(memory,0x573528u)))return 1;
    next=game_memory_read_u32(memory,node+4u);previous=game_memory_read_u32(memory,node+8u);
    if(game_memory_read_u32(memory,owner+4u)==node)game_memory_write_u32(memory,owner+4u,next);
    else game_memory_write_u32(memory,previous+4u,next);
    if(game_memory_read_u32(memory,owner+8u)==node)game_memory_write_u32(memory,owner+8u,previous);
    else game_memory_write_u32(memory,next+8u,previous);
    if(!recyclable_outer_retire(memory,node,services))memory->fault=1;
    return 0;
}
/* The existing 64-cell search owns the packed map and manager. Expansion uses
 * the same records as construction/archive/destruction; there is no CPU port
 * between queue removal, dominance pruning, path cloning and reinsertion. */
uint32_t recyclable_route_continue(GameMemory *memory,GameAddress grid,GameAddress output,
    const RecyclableRecordServices *services){
    GameAddress manager=grid+0x2028u,last=0u;unsigned steps=0u;int found=0,limited=0;
    while(!memory->fault && !found && !limited && game_memory_read_u32(memory,manager+4u)){
        GameAddress tail;unsigned count,x,y,i;
        steps++;last=game_list_remove_head(memory,manager,4u,4u,8u).first;
        limited=!(record_float(memory,grid+0x2024u)>record_float(memory,last+0x14u));
        if(!limited){
            tail=game_memory_read_u32(memory,last+0x10u);count=game_memory_read_u8(memory,tail+0xdu);
            x=game_memory_read_u8(memory,game_memory_read_u32(memory,tail+0x10u)+count-1u);
            y=game_memory_read_u8(memory,game_memory_read_u32(memory,tail+0x14u)+count-1u);
            found=record_signed(game_memory_read_u32(memory,last+0x18u))<0x3f800000;
            if(found)break;
            {
                GameAddress cell=grid+y*32u+x/8u;
                game_memory_write_u8(memory,cell,(uint8_t)(game_memory_read_u8(memory,cell)|(1u<<(x&7u))));
            }
            for(i=0u;i<8u && !memory->fault;i++){
                uint32_t dx=game_memory_read_u32(memory,0x596f20u+i*8u),dy=game_memory_read_u32(memory,0x596f24u+i*8u);
                unsigned nx=(uint8_t)(x+dx),ny=(uint8_t)(y+dy),index;GameAddress candidate,existing;
                float total;
                if(!game_state_occupancy_query(memory,grid,x,y,dx,dy))continue;
                total=(float)((long double)record_signed(game_memory_read_u32(memory,last+0x20u))+
                    record_float(memory,last+0x14u)+record_float(memory,i<4u?0x575204u:0x575208u));
                index=((unsigned)(uint8_t)(nx-game_memory_read_u8(memory,grid+0x6048u))<<6u)+
                    (uint8_t)(ny-game_memory_read_u8(memory,grid+0x604cu));
                if(index>=0x1000u){memory->fault=1;break;}
                existing=game_memory_read_u32(memory,manager+0x20u+index*4u);
                if(recyclable_container_keep(memory,manager,existing,record_float_bits(total),0u,services))continue;
                if(memory->fault)break;
                steps++;candidate=recyclable_outer_acquire(memory,last,services);
                if(!candidate){memory->fault=1;break;}
                if(!recyclable_outer_append(memory,candidate,(uint8_t)nx,(uint8_t)ny,i<4u?0x3f800000u:0x3fb504d5u,0u,services) ||
                    !recyclable_container_insert(memory,manager,candidate,2,services)){
                    (void)recyclable_outer_retire(memory,candidate,services);memory->fault=1;break;
                }
            }
        }
        if(!recyclable_outer_retire(memory,last,services))memory->fault=1;
        last=0u;if(steps>=180u)break;
    }
    if(memory->fault)return 0u;
    if(limited){
        game_memory_write_u32(memory,grid+0x2004u,0u);
        if(!recyclable_outer_reset_payload(memory,output,services))memory->fault=1;
        game_memory_write_u32(memory,output+0x18u,0x42820000u);return 0u;
    }
    if(!found && game_memory_read_u32(memory,manager+4u))return game_memory_read_u32(memory,grid+0x2004u);
    game_memory_write_u32(memory,grid+0x2004u,0u);
    if(!recyclable_outer_copy(memory,output,last?last:game_memory_read_u32(memory,manager+0x1cu),services))memory->fault=1;
    if(last && !recyclable_outer_retire(memory,last,services))memory->fault=1;
    return 0u;
}
uint32_t recyclable_route_begin(GameMemory *memory,GameAddress grid,uint8_t sx,uint8_t sy,uint8_t tx,uint8_t ty,
    GameAddress output,uint32_t limit,const RecyclableRecordServices *services){
    int dx=(int)sx-tx,dy=(int)sy-ty;GameOccupancyWindow window;GameAddress first,manager=grid+0x2028u;
    uint32_t generation,y,begin,width;
    if(dx<=-64 || dx>=64 || dy<=-64 || dy>=64){
        if(!recyclable_outer_reset_payload(memory,output,services))memory->fault=1;
        game_memory_write_u32(memory,output+0x18u,0x42820000u);game_memory_write_u32(memory,grid+0x2004u,0u);return 0u;
    }
    window=game_state_set_occupancy_window(memory,grid,sx,sy,tx,ty);
    begin=(window.minimum_x&~7u)/8u;width=(((window.maximum_x+7u)&~7u)-(window.minimum_x&~7u))/8u;
    for(y=window.minimum_y;y<window.maximum_y && !memory->fault;y++)
        if(game_memory_copy_elements(memory,grid+y*32u+begin,grid+0x6058u+y*32u+begin,width,1u,0)!=width)return 0u;
    generation=game_memory_read_u32(memory,grid+0x2000u)+1u;
    game_memory_write_u32(memory,grid+0x2000u,generation);game_memory_write_u32(memory,grid+0x2004u,generation);
    if(!recyclable_container_reset(memory,manager,tx,ty,0u,0,0,services) ||
        !game_memory_fill_u32(memory,manager+0x20u,0x1000u,0u,0)){memory->fault=1;return 0u;}
    game_memory_write_u32(memory,grid+0x6048u,window.minimum_x);game_memory_write_u32(memory,grid+0x604cu,window.minimum_y);
    game_memory_write_u32(memory,grid+0x6050u,window.maximum_y);game_memory_write_u32(memory,grid+0x6054u,window.maximum_x);
    first=recyclable_outer_acquire(memory,0u,services);if(!first){memory->fault=1;return 0u;}
    game_memory_write_u32(memory,grid+0x2024u,limit>0x80000000u?0x7f7fffffu:limit);
    if(!recyclable_outer_append(memory,first,sx,sy,0u,0u,services) ||
        !recyclable_container_insert(memory,manager,first,2,services)){memory->fault=1;return 0u;}
    if(sx==tx && sy==ty){
        if(!recyclable_outer_copy(memory,output,first,services))memory->fault=1;
        game_memory_write_u32(memory,grid+0x2004u,0u);return 0u;
    }
    return recyclable_route_continue(memory,grid,output,services);
}
/*4fe8e0/4fe990: restore the bounded occupancy window and flood it in the
 *original LIFO/right-left-down-up order. Global byte stacks are legacy data,
 *not execution state. A target neighbor is accepted before its occupied bit. */
int recyclable_route_reachable(GameMemory *m,GameAddress grid,uint8_t sx,uint8_t sy,uint8_t tx,uint8_t ty,int *reachable){
    GameOccupancyWindow w=game_state_set_occupancy_window(m,grid,sx,sy,tx,ty);
    uint32_t begin=(w.minimum_x&~7u)/8u,width=(((w.maximum_x+7u)&~7u)-(w.minimum_x&~7u))/8u;
    unsigned count=1,visits=0,y;static const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
    *reachable=0;
    for(y=w.minimum_y;y<w.maximum_y && !m->fault;y++)
        if(game_memory_copy_elements(m,grid+y*32u+begin,grid+0x6058u+y*32u+begin,width,1,0)!=width)return 0;
    game_memory_write_u32(m,grid+0x201c,tx);game_memory_write_u32(m,grid+0x2020,ty);
    game_memory_write_u8(m,0x6084c8,sx);game_memory_write_u8(m,0x6094c8,sy);
    while(count && !m->fault){unsigned x,z,i;GameAddress p;uint8_t value,mask;
        if(++visits>16385){m->fault=1;return 0;}
        --count;x=game_memory_read_u8(m,0x6084c8+count);z=game_memory_read_u8(m,0x6094c8+count);
        p=grid+z*32u+x/8u;mask=(uint8_t)(1u<<(x&7));value=game_memory_read_u8(m,p);
        if(value&mask)continue;
        game_memory_write_u8(m,p,value|mask);
        for(i=0;i<4;i++){int nx=(int)x+dx[i],nz=(int)z+dy[i];
            if(nx<(int)w.minimum_x || nx>=(int)w.maximum_x || nz<(int)w.minimum_y || nz>=(int)w.maximum_y)continue;
            if(nx==tx && nz==ty){*reachable=1;return !m->fault;}
            if(game_memory_read_u8(m,grid+(unsigned)nz*32u+(unsigned)nx/8u)&(1u<<((unsigned)nx&7)))continue;
            if(count>=4096){m->fault=1;return 0;}
            game_memory_write_u8(m,0x6084c8+count,(uint8_t)nx);game_memory_write_u8(m,0x6094c8+count,(uint8_t)nz);++count;
        }
    }
    return !m->fault;
}
int recyclable_grid_accept(GameMemory *memory,GameAddress grid,GameAddress node,const RecyclableRecordServices *services){
    uint32_t index=game_memory_read_u32(memory,grid+0x12034u)+1u;
    if(record_signed(index)<100){
        GameAddress tail=game_memory_read_u32(memory,node+0x10u);unsigned count=game_memory_read_u8(memory,tail+0xdu);
        unsigned x=game_memory_read_u8(memory,game_memory_read_u32(memory,tail+0x10u)+count-1u);
        unsigned y=game_memory_read_u8(memory,game_memory_read_u32(memory,tail+0x14u)+count-1u);
        if(index>=100u || x>=128u || y>=128u){memory->fault=1;return 0;}
        game_memory_write_u32(memory,grid+0x12034u,index);game_memory_write_u32(memory,grid+0x1203cu+index*4u,node);
        game_memory_write_u32(memory,grid+0x1830u+((x<<7u)+y)*4u,0u);return 0;
    }
    if(!recyclable_outer_retire(memory,node,services))memory->fault=1;
    return 1;
}
static void record_grid_mark(GameMemory *memory,GameAddress grid,unsigned x,unsigned y){
    GameAddress cell=grid+y*16u+x/8u;
    game_memory_write_u8(memory,cell,(uint8_t)(game_memory_read_u8(memory,cell)|(1u<<(x&7u))));
}
uint32_t recyclable_grid_route_continue(GameMemory *memory,GameAddress grid,GameAddress output,
    const RecyclableRecordServices *services){
    GameAddress manager=grid+0x1810u;unsigned steps=0u;int found=0,limited=0;
    while(!memory->fault && !found && !limited && game_memory_read_u32(memory,manager+4u)){
        GameAddress node=game_list_remove_head(memory,manager,4u,4u,8u).first,tail;
        unsigned count,x,y,i;int accepted=0;
        steps++;limited=!(record_float(memory,grid+0x1808u)>record_float(memory,node+0x14u));
        if(limited || record_signed(game_memory_read_u32(memory,grid+0x180cu))<=record_signed(game_memory_read_u32(memory,node+0x20u))){
            if(!recyclable_outer_retire(memory,node,services))memory->fault=1;
            if(steps>=180u)break;
            continue;
        }
        tail=game_memory_read_u32(memory,node+0x10u);count=game_memory_read_u8(memory,tail+0xdu);
        x=game_memory_read_u8(memory,game_memory_read_u32(memory,tail+0x10u)+count-1u);
        y=game_memory_read_u8(memory,game_memory_read_u32(memory,tail+0x14u)+count-1u);
        if(x>=128u || y>=128u){memory->fault=1;break;}
        count=game_memory_read_u32(memory,grid+0x12034u);
        if(record_signed(count)>=0 && record_signed(game_memory_read_u32(memory,grid+0x12038u))<record_signed(count)){
            GameAddress first=game_memory_read_u32(memory,grid+0x1203cu);
            long double gap=(long double)record_signed(game_memory_read_u32(memory,node+0x20u))-
                record_signed(game_memory_read_u32(memory,first+0x20u))-record_float(memory,first+0x14u)+record_float(memory,node+0x14u);
            /* Unordered comparisons follow the original less/unordered branch. */
            if(gap>=record_float(memory,grid+0x11830u)){
                if(!recyclable_outer_retire(memory,node,services))memory->fault=1;
                /* The original writes its completion flag while the cdecl
                 * argument is still on the stack, then closes this cell. */
                found=1;record_grid_mark(memory,grid,x,y);break;
            }
        }
        accepted=gameplay_geometry_sprite_overlap(memory,grid,node).value!=0u;
        if(accepted){
            found=recyclable_grid_accept(memory,grid,node,services);
            if(found)record_grid_mark(memory,grid,x,y);
            /* Accepted records stay owned by the result array. Its next begin
             * releases them, and archive reload uses the same owner chain. */
        }else{
            record_grid_mark(memory,grid,x,y);
            for(i=0u;i<8u && !memory->fault;i++){
                uint32_t dx=game_memory_read_u32(memory,0x585390u+i*8u),dy=game_memory_read_u32(memory,0x585394u+i*8u);
                unsigned nx=(uint8_t)(x+dx),ny=(uint8_t)(y+dy),index;GameAddress existing,candidate;
                uint32_t penalty,step_bits=game_memory_read_u32(memory,i<4u?0x573530u:0x573534u);float total;
                if(!game_state_query_grid_path(memory,grid,x,y,dx,dy).value)continue;
                index=(nx<<7u)+ny;
                penalty=game_memory_read_u8(memory,grid+0x800u+(ny&~1u)*32u+nx/2u);
                total=(float)((long double)record_signed(game_memory_read_u32(memory,node+0x20u))+
                    record_float(memory,node+0x14u)+record_float(memory,i<4u?0x573530u:0x573534u));
                existing=game_memory_read_u32(memory,grid+0x1830u+index*4u);
                if(recyclable_container_keep(memory,manager,existing,record_float_bits(total),penalty,services))continue;
                if(memory->fault)break;
                steps++;candidate=recyclable_outer_acquire(memory,node,services);
                if(!candidate){memory->fault=1;break;}
                if(!recyclable_outer_append(memory,candidate,(uint8_t)nx,(uint8_t)ny,step_bits,0u,services) ||
                    !recyclable_container_insert(memory,manager,candidate,1,services)){
                    (void)recyclable_outer_retire(memory,candidate,services);memory->fault=1;break;
                }
            }
            if(!recyclable_outer_retire(memory,node,services))memory->fault=1;
        }
        if(steps>=180u)break;
    }
    if(memory->fault)return 0u;
    if(limited){
        game_memory_write_u32(memory,grid+0x1804u,0u);
        if(!recyclable_outer_reset_payload(memory,output,services))memory->fault=1;
        game_memory_write_u32(memory,output+0x18u,record_float_bits((float)((long double)record_float(memory,grid+0x1808u)+record_float(memory,0x573530u))));return 0u;
    }
    if(!found && game_memory_read_u32(memory,manager+4u))return game_memory_read_u32(memory,grid+0x1804u);
    game_memory_write_u32(memory,grid+0x1804u,0u);
    {
        uint32_t count=game_memory_read_u32(memory,grid+0x12034u);GameAddress source;
        if(record_signed(count)<0)source=game_memory_read_u32(memory,manager+0x1cu);
        else{
            uint32_t cursor=game_memory_read_u32(memory,0x6308c0u),index;long double value;
            if(cursor==1024u)cursor=0u;
            if(cursor>=1024u){memory->fault=1;return 0u;}
            game_memory_write_u32(memory,0x6308c0u,cursor+1u);
            value=(long double)record_signed(count)*record_float(memory,0x6308c8u+cursor*4u);
            if(!(value>=0.0L && value<100.0L)){memory->fault=1;return 0u;}
            index=(uint32_t)value;source=game_memory_read_u32(memory,grid+0x1203cu+index*4u);
        }
        if(!recyclable_outer_copy(memory,output,source,services))memory->fault=1;
    }
    return 0u;
}
uint32_t recyclable_grid_route_begin(GameMemory *memory,GameAddress grid,uint8_t sx,uint8_t sy,uint8_t tx,uint8_t ty,
    GameAddress output,uint32_t limit,const RecyclableRecordServices *services){
    GameAddress manager=grid+0x1810u,first;int dx=(int)sx-tx,dy=(int)sy-ty;uint32_t generation,last;
    game_memory_fill_u32(memory,grid+0x800u,0x400u,0u,0);game_memory_write_u32(memory,grid+0x12038u,0u);
    game_memory_write_u32(memory,grid+0x1808u,limit>0x80000000u?0x7f7fffffu:limit);
    game_memory_write_u32(memory,grid+0x180cu,0x7fffffffu);
    if((long double)(dx*dx+dy*dy)>(long double)record_float(memory,grid+0x1808u)*record_float(memory,grid+0x1808u)){
        if(!recyclable_outer_reset_payload(memory,output,services))memory->fault=1;
        game_memory_write_u32(memory,output+0x18u,record_float_bits((float)((long double)record_float(memory,grid+0x1808u)+record_float(memory,0x573530u))));
        game_memory_write_u32(memory,grid+0x1804u,0u);return 0u;
    }
    if(game_memory_copy_elements(memory,grid,grid+0x11834u,0x200u,4u,0)!=0x200u)return 0u;
    generation=game_memory_read_u32(memory,grid+0x1800u)+1u;
    game_memory_write_u32(memory,grid+0x1800u,generation);game_memory_write_u32(memory,grid+0x1804u,generation);
    if(!recyclable_container_reset(memory,manager,tx,ty,0u,0,1,services)){memory->fault=1;return 0u;}
    while(record_signed(last=game_memory_read_u32(memory,grid+0x12034u))>=0){
        if(last>=100u || !recyclable_outer_retire(memory,game_memory_read_u32(memory,grid+0x1203cu+last*4u),services)){
            memory->fault=1;return 0u;
        }
        game_memory_write_u32(memory,grid+0x12034u,last-1u);
    }
    game_memory_fill_u32(memory,grid+0x1203cu,100u,0u,0);
    first=recyclable_outer_acquire(memory,0u,services);if(!first){memory->fault=1;return 0u;}
    if(!recyclable_outer_append(memory,first,sx,sy,0u,0u,services) ||
        !recyclable_container_insert(memory,manager,first,1,services)){memory->fault=1;return 0u;}
    if(sx==tx && sy==ty){
        if(!recyclable_outer_copy(memory,output,first,services))memory->fault=1;
        game_memory_write_u32(memory,grid+0x1804u,0u);return 0u;
    }
    return recyclable_grid_route_continue(memory,grid,output,services);
}
/*454ea0: seed a weighted flood from one cell, preserving the same frontier
 *and accepted-record owners as the target-to-target grid search. */
uint32_t recyclable_grid_flood_begin(GameMemory *m,GameAddress grid,uint8_t x,uint8_t y,
    uint32_t radius,GameAddress output,const RecyclableRecordServices *s){
    GameAddress manager=grid+0x1810u,first;uint32_t generation,last;
    if(!game_memory_fill_u32(m,grid+0x800u,0x400u,0u,0) ||
       game_memory_copy_elements(m,grid,grid+0x11834u,0x200u,4u,0)!=0x200u)return 0;
    generation=game_memory_read_u32(m,grid+0x1800u)+1u;
    game_memory_write_u32(m,grid+0x1800u,generation);game_memory_write_u32(m,grid+0x1804u,generation);
    if(!recyclable_container_reset(m,manager,x,y,radius,1,1,s)){m->fault=1;return 0;}
    while(record_signed(last=game_memory_read_u32(m,grid+0x12034u))>=0){
        if(last>=100u || !recyclable_outer_retire(m,game_memory_read_u32(m,grid+0x1203cu+last*4u),s)){m->fault=1;return 0;}
        game_memory_write_u32(m,grid+0x12034u,last-1u);
    }
    if(!game_memory_fill_u32(m,grid+0x1203cu,100u,0u,0))return 0;
    game_memory_write_u32(m,grid+0x12034u,UINT32_MAX);
    first=recyclable_outer_acquire(m,0,s);if(!first){m->fault=1;return 0;}
    game_memory_write_u32(m,grid+0x1808u,0x7f7fffffu);game_memory_write_u32(m,grid+0x180cu,0x7fffffffu);
    game_memory_write_u32(m,grid+0x12038u,15u);
    if(!recyclable_outer_append(m,first,x,y,0,0,s) || !recyclable_container_insert(m,manager,first,1,s)){m->fault=1;return 0;}
    if(record_signed(radius)<0x3f800000){
        if(!recyclable_outer_copy(m,output,first,s))m->fault=1;
        game_memory_write_u32(m,grid+0x1804u,0);return 0;
    }
    return recyclable_grid_route_continue(m,grid,output,s);
}
static int32_t record_penalty_cell(float value){
    long double v=value;uint32_t word=(uint32_t)(uint64_t)(!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v);
    return record_signed((word>>11)|((word&0x80000000u)?0xffe00000u:0));
}
static void record_penalty_square(GameMemory *m,GameAddress grid,float x,float z){
    int32_t cx=record_penalty_cell(x),cz=record_penalty_cell(z),row,column;
    for(row=cz-3;row<=cz+3;row++)if(row>=0 && row<64)
        for(column=cx-3;column<=cx+3;column++)if(column>=0 && column<64)
            game_memory_write_u8(m,grid+0x800+(uint32_t)row*64+(uint32_t)column,255);
}
/*454cf0: hostile site penalties exclude the requested site and use the
 *original directional alliance matrix. Living other human players add the
 *same bounded seven-by-seven penalty squares, independent of alliances. */
static int record_attack_penalties(GameMemory *m,GameAddress grid,uint32_t owner,uint32_t ignored_site){
    int count=(int16_t)game_memory_read_u16(m,0x639a74);unsigned i;
    if(!game_memory_fill_u32(m,grid+0x800,0x400,0,0))return 0;
    for(i=0;(int)i<count;i++){
        GameAddress site=game_memory_read_u32(m,game_memory_read_u32(m,0x639a70)+i*4);
        GameAddress faction=game_memory_read_u32(m,site+84),point;uint32_t identity;
        if(!faction || i==ignored_site)continue;
        identity=game_memory_read_u16(m,faction+8)&63u;
        if(game_memory_read_u32(m,0x5f0160+identity*24+(owner&63u)*4))continue;
        point=game_memory_read_u32(m,site+28);record_penalty_square(m,grid,record_float(m,point),record_float(m,point+8));
    }
    for(i=0;i<4;i++){
        GameAddress faction=game_memory_read_u32(m,0x5f0210+i*4),player;
        if(!faction || game_memory_read_u16(m,faction+8)==owner || !game_memory_read_u32(m,faction+20))continue;
        player=game_memory_read_u32(m,faction+0x3bc);
        if(player && record_signed(game_memory_read_u32(m,player+88))>0)
            record_penalty_square(m,grid,record_float(m,player+24),record_float(m,player+32));
    }
    return !m->fault;
}
/*455850: weighted hostile-aware search with its own radius and step limits.
 *The existing grid owns accepted paths; output borrows none of their nodes. */
uint32_t recyclable_grid_attack_begin(GameMemory *m,GameAddress grid,uint8_t sx,uint8_t sy,uint8_t tx,uint8_t ty,
    uint32_t radius,GameAddress output,uint32_t owner,uint32_t ignored_site,uint32_t limit,uint32_t steps,const RecyclableRecordServices *s){
    GameAddress manager=grid+0x1810,first;uint32_t generation,last;
    if(!record_attack_penalties(m,grid,owner,ignored_site) || game_memory_copy_elements(m,grid,grid+0x11834,0x200,4,0)!=0x200)return 0;
    generation=game_memory_read_u32(m,grid+0x1800)+1;
    game_memory_write_u32(m,grid+0x1800,generation);game_memory_write_u32(m,grid+0x1804,generation);
    if(!recyclable_container_reset(m,manager,tx,ty,radius,0,1,s)){m->fault=1;return 0;}
    while(record_signed(last=game_memory_read_u32(m,grid+0x12034))>=0){
        if(last>=100 || !recyclable_outer_retire(m,game_memory_read_u32(m,grid+0x1203c+last*4),s)){m->fault=1;return 0;}
        game_memory_write_u32(m,grid+0x12034,last-1);
    }
    if(!game_memory_fill_u32(m,grid+0x1203c,100,0,0))return 0;
    game_memory_write_u32(m,grid+0x12034,UINT32_MAX);
    first=recyclable_outer_acquire(m,0,s);if(!first){m->fault=1;return 0;}
    game_memory_write_u32(m,grid+0x1808,limit>0x80000000u?0x7f7fffffu:limit);
    game_memory_write_u32(m,grid+0x180c,record_signed(steps)<0?0x7fffffffu:steps);game_memory_write_u32(m,grid+0x12038,15);
    if(!recyclable_outer_append(m,first,sx,sy,0,0,s) || !recyclable_container_insert(m,manager,first,1,s)){m->fault=1;return 0;}
    if(record_signed(radius)<0x3f800000){
        if(!recyclable_outer_copy(m,output,first,s))m->fault=1;
        game_memory_write_u32(m,grid+0x1804,0);return 0;
    }
    return recyclable_grid_route_continue(m,grid,output,s);
}
static int record_grid_random_index(GameMemory *m,int32_t last,int32_t *index){
    uint32_t cursor=game_memory_read_u32(m,0x6308c0u);long double value;int64_t n;
    if(cursor==1024u)cursor=0;
    if(cursor>=1024u){m->fault=1;return 0;}
    game_memory_write_u32(m,0x6308c0u,cursor+1u);value=(long double)last*record_float(m,0x6308c8u+cursor*4u);
    n=!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value;
    *index=record_signed((uint32_t)(uint64_t)n);return !m->fault;
}
/*456400 returns a borrowed result, never transferring ownership. */
GameAddress recyclable_grid_random_route(GameMemory *m,GameAddress grid){
    int32_t last=record_signed(game_memory_read_u32(m,grid+0x12034u)),index;
    if(last<0)return 0;
    if(last>=100 || !record_grid_random_index(m,last,&index))return 0;
    if(index<0 || index>last){m->fault=1;return 0;}
    return game_memory_read_u32(m,grid+0x1203cu+(uint32_t)index*4u);
}
static int record_route_endpoint(GameMemory *m,GameAddress route,unsigned *x,unsigned *y){
    GameAddress tail=game_memory_read_u32(m,route+0x10u);unsigned count=game_memory_read_u8(m,tail+0xdu);
    if(!count || count>32u)return 0;
    *x=game_memory_read_u8(m,game_memory_read_u32(m,tail+0x10u)+count-1u);
    *y=game_memory_read_u8(m,game_memory_read_u32(m,tail+0x14u)+count-1u);return !m->fault;
}
static int record_route_angle(GameMemory *m,int dx,int dy){
    float ratio=(float)((long double)dy/dx);uint16_t bits;int16_t angle;
    /*457fd0 takes the unordered comparison's first branch; FTOL's
     *indefinite64 narrows to index0. Preserve that for coincident endpoints. */
    bits=isnan(ratio)?(game_memory_read_u16(m,0x62e8b0u)&2047u):
        vehicle_targeting_vector_angle(m,ratio,1.0f,record_float(m,0x573538u));
    memcpy(&angle,&bits,2);return angle+(dx<0?1024:0);
}
/*4564b0: cycle from a random result and select a sufficiently different
 *endpoint angle. The difference is deliberately not normalized modulo2048. */
int recyclable_grid_pick_route(GameMemory *m,GameAddress grid,GameAddress route,int32_t separation,int32_t *selected){
    GameAddress first,candidate;unsigned x,y,sx,sy,cx,cy;int32_t last,index;int angle,remaining;
    if(!m || !selected || m->fault)return 0;
    *selected=-1;
    last=record_signed(game_memory_read_u32(m,grid+0x12034u));if(last>=100){m->fault=1;return 0;}
    first=game_memory_read_u32(m,route+0xcu);if(first)game_memory_write_u8(m,first+0xcu,0);
    if(!record_route_endpoint(m,route,&x,&y)){
        /*An empty temporary route made retail read before its coordinate
         *array. Keep its RNG draw, then allow the caller's real fallback. */
        (void)record_grid_random_index(m,last,&index);return 0;
    }
    sx=game_memory_read_u8(m,game_memory_read_u32(m,first+0x10u));
    sy=game_memory_read_u8(m,game_memory_read_u32(m,first+0x14u));
    angle=record_route_angle(m,(int)x-(int)sx,(int)y-(int)sy);
    if(!record_grid_random_index(m,last,&index))return 0;
    for(remaining=last;remaining>0;remaining--){
        int dx,dy,difference;if(++index>last)index=0;
        if(index<0){m->fault=1;return 0;}candidate=game_memory_read_u32(m,grid+0x1203cu+(uint32_t)index*4u);
        if(!record_route_endpoint(m,candidate,&cx,&cy)){m->fault=1;return 0;}
        dx=(int)cx-(int)sx;dy=(int)cy-(int)sy;if(!dx && !dy)continue;
        difference=record_route_angle(m,dx,dy)-angle;if(difference<0)difference=-difference;
        if(difference>separation){*selected=index;return !m->fault;}
    }
    return 0;
}
static GameAddress record_point_next(GameMemory *memory,GameAddress point){
    unsigned cursor=game_memory_read_u8(memory,point+0xcu)+1u;
    game_memory_write_u8(memory,point+0xcu,(uint8_t)cursor);
    if(cursor==32u){
        point=game_memory_read_u32(memory,point+4u);
        if(point)game_memory_write_u8(memory,point+0xcu,0u);
        return point;
    }
    return cursor<game_memory_read_u8(memory,point+0xdu)?point:0u;
}
static unsigned record_point_coordinate(GameMemory *memory,GameAddress point,unsigned axis,unsigned cursor){
    return game_memory_read_u8(memory,game_memory_read_u32(memory,point+0x10u+axis*4u)+cursor);
}
static int record_chain_append(GameMemory *memory,GameAddress *tail,unsigned x,unsigned z,const RecyclableRecordServices *services){
    GameAddress next=recyclable_inner_acquire(memory,*tail,(uint8_t)x,(uint8_t)z,services);
    if(!next)return 0;
    if(next!=*tail){game_memory_write_u32(memory,*tail+4u,next);game_memory_write_u32(memory,next+8u,*tail);*tail=next;}
    return !memory->fault;
}
/* Preserve the original cursor walk and radius-to-direction transition. The
 * temporary chain has ordinary local ownership, with no addressable stack view. */
int recyclable_route_simplify(GameMemory *memory,GameAddress route,uint32_t radius_bits,const RecyclableRecordServices *services){
    GameAddress input=game_memory_read_u32(memory,route+0xcu),point,first,tail,last;
    unsigned x,z,last_x,last_z,origin_x,origin_z,cursor,visits=0u,offset;
    int dx,dz,previous_dx,previous_dz,directional=(radius_bits&0x7fffffffu)==0u,success=0;
    float radius,limit;
    memcpy(&radius,&radius_bits,4u);limit=(float)((long double)radius*radius);
    if(!input){memory->fault=1;return 0;}
    game_memory_write_u8(memory,input+0xcu,0u);
    origin_x=last_x=record_point_coordinate(memory,input,0u,0u);
    origin_z=last_z=record_point_coordinate(memory,input,1u,0u);
    first=tail=recyclable_inner_acquire(memory,0u,0u,0u,services);
    if(!first)return 0;
    game_memory_write_u8(memory,first+0xcu,0u);game_memory_write_u8(memory,first+0xdu,0u);
    if(!record_chain_append(memory,&tail,last_x,last_z,services))goto cleanup;
    point=record_point_next(memory,input);
    previous_dx=previous_dz=0;
    if(point){
        cursor=game_memory_read_u8(memory,point+0xcu);
        previous_dx=(int)record_point_coordinate(memory,point,0u,cursor)-(int)last_x;
        previous_dz=(int)record_point_coordinate(memory,point,1u,cursor)-(int)last_z;
    }
    while(point && !memory->fault){
        if(++visits>65536u){memory->fault=1;goto cleanup;}
        cursor=game_memory_read_u8(memory,point+0xcu);
        x=record_point_coordinate(memory,point,0u,cursor);z=record_point_coordinate(memory,point,1u,cursor);
        dx=(int)x-(int)last_x;dz=(int)z-(int)last_z;
        if(!directional){
            int ox=(int)x-(int)origin_x,oz=(int)z-(int)origin_z;
            if((long double)(ox*ox+oz*oz)>=limit){
                if(!record_chain_append(memory,&tail,last_x,last_z,services))goto cleanup;
                directional=1;
            }
        }else if((dx!=previous_dx || dz!=previous_dz) && !record_chain_append(memory,&tail,last_x,last_z,services))goto cleanup;
        last_x=x;last_z=z;previous_dx=dx;previous_dz=dz;point=record_point_next(memory,point);
    }
    last=game_memory_read_u32(memory,route+0x10u);cursor=game_memory_read_u8(memory,last+0xdu);
    if(!cursor){memory->fault=1;goto cleanup;}
    if(!record_chain_append(memory,&tail,record_point_coordinate(memory,last,0u,cursor-1u),record_point_coordinate(memory,last,1u,cursor-1u),services))goto cleanup;
    success=record_copy_chain(memory,route,first,services);
    if(success)for(offset=0x14u;offset<=0x20u;offset+=4u)game_memory_write_u32(memory,route+offset,0u);
cleanup:
    while(first){GameAddress next=game_memory_read_u32(memory,first+4u);if(!retire_inner(memory,first,services))success=0;first=next;}
    return success && !memory->fault;
}
/* Retail truncation returns the low word of int64, followed by arithmetic
 * shift. Guard conversion bounds so NaN/overflow has the original low word0. */
static uint32_t record_world_cell(long double value){
    uint32_t bits=0u;
    if(value>=-9223372036854775808.0L && value<9223372036854775808.0L)bits=(uint32_t)(uint64_t)(int64_t)value;
    return (bits>>10u)|((bits&0x80000000u)?0xffc00000u:0u);
}
static int record_cell_mismatch(uint32_t sample,uint32_t coordinate){
    uint32_t difference=sample-coordinate;
    uint32_t magnitude=(difference&0x80000000u)?0u-difference:difference;
    return record_signed(magnitude)>1;
}
int recyclable_queued_transition(GameMemory *memory,GameAddress owner,const RecyclableRecordServices *services){
    uint32_t index=game_memory_read_u32(memory,owner+0x1c4u),state,count,result;
    GameAddress source,route,selected;
    RecyclablePosition position;
    if(index>=14u){memory->fault=1;return 0;}
    state=game_memory_read_u8(memory,owner+0x1b6u+index);
    if(state>1u)return !memory->fault;
    if(!state){
        (void)gameplay_state_refresh_entity_buckets(memory,owner+0x1c8u,1,0);
        if(game_memory_copy_elements(memory,owner+0xd2cu,0x603018u,3u,4u,0)!=3u)return 0;
        source=game_memory_read_u32(memory,owner+0x244u)?game_memory_read_u32(memory,owner+0x230u):
            game_memory_read_u32(memory,owner+0x248u)?game_memory_read_u32(memory,owner+0x238u):0u;
        if(!source){
            game_memory_write_u32(memory,owner+0xda0u+index*4u,game_memory_read_u32(memory,0x5ac6acu));
            game_memory_write_u8(memory,owner+0x1a8u+index,0u);game_memory_write_u8(memory,owner+0x1b6u+index,0u);
            game_memory_write_u32(memory,owner+0x1a4u,game_memory_read_u32(memory,owner+0x1a4u)&~(1u<<index));return !memory->fault;
        }
        count=game_memory_read_u16(memory,0x639a74u);
        if(count>32767u){memory->fault=1;return 0;}
        if(game_memory_copy_elements(memory,game_memory_read_u32(memory,owner+0x23cu),source,count,4u,0)!=count)return 0;
        game_memory_write_u32(memory,owner+0x254u,1u);game_memory_write_u8(memory,owner+0x1b6u+index,1u);
    }
    route=game_memory_read_u32(memory,owner+0x430u+index*4u);
    position=recyclable_position_read(memory,owner+0xd2cu);
    result=recyclable_visibility_update(memory,owner+0x1c8u,owner+0x340u+index*4u,position,route,services);
    if(memory->fault)return 0;
    if(result)return 1;
    game_memory_write_u32(memory,owner+0x254u,0u);
    selected=game_memory_read_u32(memory,owner+0x340u+index*4u);
    if(selected){if(!recyclable_route_simplify(memory,route,0u,services))return 0;}
    else{
        selected=game_memory_read_u32(memory,game_memory_read_u32(memory,owner+0x23cu));
        if(!selected){memory->fault=1;return 0;}
        game_memory_write_u32(memory,owner+0x340u+index*4u,selected);
        if(!recyclable_outer_reset_payload(memory,route,services))return 0;
        if(!recyclable_outer_append(memory,route,
            (uint8_t)record_world_cell(((long double)record_float(memory,selected+4u)+record_float(memory,selected+0xcu))*record_float(memory,0x57464cu)),
            (uint8_t)record_world_cell(((long double)record_float(memory,selected+8u)+record_float(memory,selected+0x10u))*record_float(memory,0x57464cu)),0u,0u,services))return 0;
        if(!recyclable_outer_append(memory,route,(uint8_t)record_world_cell(position.x),(uint8_t)record_world_cell(position.z),0u,0u,services))return 0;
    }
    game_memory_write_u8(memory,owner+0x1b6u+index,0u);
    game_memory_write_u8(memory,owner+0x1a8u+index,(uint8_t)(game_memory_read_u8(memory,owner+0x1a8u+index)+1u));
    return !memory->fault;
}
RecyclablePosition recyclable_position_read(GameMemory *memory,GameAddress address){
    RecyclablePosition position={record_float(memory,address),record_float(memory,address+8u)};return position;
}
static RecyclablePosition record_candidate_center(GameMemory *memory,GameAddress candidate){
    RecyclablePosition position;
    position.x=(float)(((long double)record_float(memory,candidate+4u)+record_float(memory,candidate+0xcu))*record_float(memory,0x5730d8u));
    position.z=(float)(((long double)record_float(memory,candidate+8u)+record_float(memory,candidate+0x10u))*record_float(memory,0x5730d8u));
    return position;
}
uint32_t recyclable_spatial_cache_update(GameMemory *memory,GameAddress owner,GameAddress request,
    RecyclablePosition position,GameAddress output,uint32_t limit,const RecyclableRecordServices *services){
    const GameAddress grid=0x5cf038u;GameAddress tail;unsigned count;uint32_t result,x,y;
    if(game_memory_read_u32(memory,owner+0x40u)!=request){
        if(game_memory_read_u32(memory,grid+0x1804u))return 3u;
        game_memory_write_u32(memory,owner+0x40u,request);
        if(!recyclable_outer_reset_payload(memory,output,services)){memory->fault=1;return 0u;}
        x=record_world_cell(((long double)record_float(memory,request+4u)+record_float(memory,request+0xcu))*record_float(memory,0x5730d8u));
        y=record_world_cell(((long double)record_float(memory,request+8u)+record_float(memory,request+0x10u))*record_float(memory,0x5730d8u));
        game_memory_write_u32(memory,owner+0x1cu,x);game_memory_write_u32(memory,owner+0x20u,y);
        result=recyclable_grid_route_begin(memory,grid,(uint8_t)x,(uint8_t)y,
            (uint8_t)record_world_cell(position.x),
            (uint8_t)record_world_cell(position.z),output,limit,services);
        game_memory_write_u32(memory,owner+0x3cu,0u);game_memory_write_u32(memory,owner+0x18u,result);
    }else{
        uint32_t age=game_memory_read_u32(memory,owner+0x3cu)+1u;
        game_memory_write_u32(memory,owner+0x3cu,age);
        if(record_signed(age)>=100){
            if(game_memory_read_u32(memory,owner+0x18u)==game_memory_read_u32(memory,grid+0x1804u))
                game_memory_write_u32(memory,grid+0x1804u,0u);
            if(!recyclable_outer_reset_payload(memory,output,services))memory->fault=1;
            return 0u;
        }
        result=recyclable_grid_route_continue(memory,grid,output,services);
        game_memory_write_u32(memory,owner+0x18u,result);
    }
    if(memory->fault)return 0u;
    if(game_memory_read_u32(memory,owner+0x18u))return 2u;
    tail=game_memory_read_u32(memory,output+0x10u);count=game_memory_read_u8(memory,tail+0xdu);
    x=game_memory_read_u8(memory,game_memory_read_u32(memory,tail+0x10u)+count-1u);
    if(record_cell_mismatch(x,record_world_cell(position.x)))return 0u;
    y=game_memory_read_u8(memory,game_memory_read_u32(memory,tail+0x14u)+count-1u);
    return record_cell_mismatch(y,record_world_cell(position.z))?0u:1u;
}
/* Candidate arrays and route payload stay in their existing owners. The
 * comparison order, intermediate float stores and signed class bias are retail
 * behavior, including the distance-cancelling bias used for initial ordering. */
int recyclable_candidate_distances(GameMemory *memory,RecyclablePosition position,GameAddress candidates,GameAddress distances){
    int count=(int16_t)game_memory_read_u16(memory,0x639a74u),i;
    for(i=0;i<count && !memory->fault;i++){
        GameAddress candidate=game_memory_read_u32(memory,candidates+(uint32_t)i*4u);
        float value;
        if(!candidate){game_memory_write_u32(memory,distances+(uint32_t)i*4u,0x7f7fffffu);continue;}
        value=game_state_approximate_distance(position.x,position.z,
            (float)(((long double)record_float(memory,candidate+0xcu)+record_float(memory,candidate+4u))*record_float(memory,0x5730d8u)),
            (float)(((long double)record_float(memory,candidate+8u)+record_float(memory,candidate+0x10u))*record_float(memory,0x5730d8u)),
            record_float(memory,0x571f18u));
        {
            int bias=(1-(int16_t)game_memory_read_u16(memory,candidate+0x24u))*3;
            long double weighted=(long double)value-((long double)bias+value);
            value=weighted>=record_float(memory,0x5730d4u)?(float)weighted:0.0f;
        }
        game_memory_write_u32(memory,distances+(uint32_t)i*4u,record_float_bits(value));
    }
    return !memory->fault;
}
static void record_candidates_sort(GameMemory *memory,GameAddress candidates,GameAddress distances,int count){
    int i,j;
    /* Preserve the x87 less/unordered branch; equal distances never swap. */
    for(i=0;i<count && !memory->fault;i++)for(j=i+1;j<count;j++){
        if(!(record_float(memory,distances+(uint32_t)j*4u)>=record_float(memory,distances+(uint32_t)i*4u))){
            uint32_t a=game_memory_read_u32(memory,candidates+(uint32_t)i*4u);
            uint32_t b=game_memory_read_u32(memory,distances+(uint32_t)i*4u);
            game_memory_write_u32(memory,candidates+(uint32_t)i*4u,game_memory_read_u32(memory,candidates+(uint32_t)j*4u));
            game_memory_write_u32(memory,distances+(uint32_t)i*4u,game_memory_read_u32(memory,distances+(uint32_t)j*4u));
            game_memory_write_u32(memory,candidates+(uint32_t)j*4u,a);
            game_memory_write_u32(memory,distances+(uint32_t)j*4u,b);
        }
    }
}
uint32_t recyclable_visibility_update(GameMemory *memory,GameAddress owner,GameAddress selected,
    RecyclablePosition position,GameAddress output,const RecyclableRecordServices *services){
    GameAddress candidates,distances,candidate;int count,index;uint32_t result;
    count=(int16_t)game_memory_read_u16(memory,0x639a74u);
    candidates=game_memory_read_u32(memory,owner+0x74u);
    distances=game_memory_read_u32(memory,owner+0x84u);
    if(game_memory_read_u32(memory,owner+4u)){
        if(!recyclable_candidate_distances(memory,position,candidates,distances))return 0u;
        record_candidates_sort(memory,candidates,distances,count);
        if(!recyclable_outer_reset_payload(memory,output,services) ||
            !recyclable_outer_reset_payload(memory,owner+0x44u,services)){memory->fault=1;return 0u;}
        game_memory_write_u32(memory,selected,0u);
        game_memory_write_u32(memory,owner+0x24u,0x7f7fffffu);
        game_memory_write_u32(memory,owner+0x28u,0x7f7fffffu);
        for(index=0;index<count && !game_memory_read_u32(memory,candidates+(uint32_t)index*4u);index++){}
        game_memory_write_u32(memory,owner+0xcu,index<count?(uint32_t)index:UINT32_MAX);
        game_memory_write_u32(memory,owner+0x14u,0u);game_memory_write_u32(memory,owner+4u,0u);
        game_memory_write_u32(memory,owner+0x40u,0u);game_memory_write_u32(memory,owner+0x18u,0u);
    }
    index=record_signed(game_memory_read_u32(memory,owner+0xcu));
    /* An empty candidate set has no route to advance. Avoid the original
     * negative array access; normal nonempty routes preserve the exact ordering. */
    if(index<0 || index>=count){game_memory_write_u32(memory,owner+4u,1u);return 0u;}
    candidate=game_memory_read_u32(memory,candidates+(uint32_t)index*4u);
    result=recyclable_spatial_cache_update(memory,owner,candidate,position,owner+0x44u,
        game_memory_read_u32(memory,owner+0x28u),services);
    if(memory->fault)return 0u;
    if(!game_memory_read_u32(memory,owner+0x14u)){
        if(result==3u)return 1u;
        game_memory_write_u32(memory,owner+0x14u,1u);
    }
    if(result==2u || result==3u)return 1u;
    if(result>3u)return 0u;
    if(result==1u){
        int bias=(1-(int16_t)game_memory_read_u16(memory,candidate+0x24u))*3;
        long double weighted=(long double)bias+record_signed(game_memory_read_u32(memory,owner+0x64u));
        float score;
        weighted+=record_float(memory,owner+0x58u);
        score=weighted>=record_float(memory,0x5730d4u)?(float)weighted:0.0f;
        if(game_memory_read_u32(memory,owner+0x2cu)==game_memory_read_u32(memory,candidate))
            score=(float)((long double)record_signed(game_memory_read_u32(memory,owner+0x30u)<<2u)*score);
        if(!game_memory_read_u32(memory,selected) || record_float(memory,owner+0x28u)>score){
            if(!recyclable_outer_copy(memory,output,owner+0x44u,services)){memory->fault=1;return 0u;}
            game_memory_write_u32(memory,owner+0x28u,record_float_bits(score));
            game_memory_write_u32(memory,owner+0x24u,record_float_bits((float)((long double)
                record_signed(game_memory_read_u32(memory,owner+0x64u))+record_float(memory,owner+0x58u))));
            game_memory_write_u32(memory,selected,candidate);
        }
    }
    for(index++;index<count && !game_memory_read_u32(memory,candidates+(uint32_t)index*4u);index++){}
    game_memory_write_u32(memory,owner+0xcu,index<count?(uint32_t)index:UINT32_MAX);
    if(index>=count){game_memory_write_u32(memory,owner+4u,1u);return 0u;}
    game_memory_write_u32(memory,owner+0x14u,0u);return 1u;
}
static int record_root_next(GameMemory *memory,GameAddress roots,int index,int count){
    for(;index<count && !memory->fault;index++){
        GameAddress root=game_memory_read_u32(memory,roots+(uint32_t)index*4u);
        if(root && game_memory_read_u32(memory,game_memory_read_u32(memory,root+0x18u)))return index;
    }
    return -1;
}
static const unsigned record_projection_arrays[5]={0x68u,0x6cu,0x70u,0x74u,0x84u};
static int record_projection_array_release(GameAddress array,const RecyclableRecordServices *services){
    if(!array)return 1;
    if(array>=SCENE_PROJECTION_BASE && array<RECYCLABLE_RECORD_BASE)return scene_projection_storage_free(array);
    return services && services->release && services->release(services->context,array);
}
int recyclable_projection_construct(GameMemory *memory,GameAddress owner,const RecyclableRecordServices *services){
    (void)game_memory_read_u32(memory,owner+0x8cu);
    if(memory->fault)return 0;
    if(!recyclable_outer_initialize(memory,owner+0x44u,services))return 0;
    game_initializer_gameplay_presentation_controller(memory,owner);
    game_initializer_clear_gameplay_presentation_link(memory,owner);
    return !memory->fault;
}
int recyclable_projection_allocate_arrays(GameMemory *memory,GameAddress owner,const RecyclableRecordServices *services){
    int count=(int16_t)game_memory_read_u16(memory,0x639a74u);unsigned i,j;GameAddress arrays[5]={0u};
    if(count<0){memory->fault=1;return 0;}
    /* The constructor owns these slots. Reject replacement of live arrays
     * instead of leaking their allocations into a competing state copy. */
    for(i=0u;i<5u;i++)if(game_memory_read_u32(memory,owner+record_projection_arrays[i])){memory->fault=1;return 0;}
    if(memory->fault)return 0;
    for(i=0u;i<5u;i++){
        GameAddress array=services && services->allocate?services->allocate(services->context,(size_t)count*4u):
            count?scene_projection_storage_allocate((size_t)count*4u,"gameplay-candidate-array"):0u;
        arrays[i]=array;
        if(count && !array)goto allocation_failed;
        /* Owned pages are calloc'd; injected oracle allocations retain the
         * original exact zeroing of the logical span. */
        if(array && services && services->allocate && !game_memory_fill_u32(memory,array,(uint32_t)count,0u,0))goto allocation_failed;
    }
    for(i=0u;i<5u;i++)game_memory_write_u32(memory,owner+record_projection_arrays[i],arrays[i]);
    if(memory->fault)goto allocation_failed;
    return !memory->fault;
allocation_failed:
    for(j=0u;j<5u;j++)if(arrays[j])(void)record_projection_array_release(arrays[j],services);
    memory->fault=1;return 0;
}
int recyclable_projection_destroy(GameMemory *memory,GameAddress owner,const RecyclableRecordServices *services){
    unsigned i;game_memory_write_u32(memory,owner,0x5730e0u);
    for(i=0u;i<5u;i++)if(!record_projection_array_release(game_memory_read_u32(memory,owner+record_projection_arrays[i]),services))return 0;
    return recyclable_record_destroy(memory,owner+0x44u,0u,services);
}
/* Advance the enclosing root/candidate search. Centers are ordinary C locals;
 * none are serialized into a guest stack or held in a second state owner. */
uint32_t recyclable_projection_update(GameMemory *memory,GameAddress owner,GameAddress selected,
    GameAddress output,const RecyclableRecordServices *services){
    GameAddress roots=game_memory_read_u32(memory,owner+0x6cu),root,candidates,distances;
    RecyclablePosition position;uint32_t result=0u;int count,index;unsigned i;
    count=(int16_t)game_memory_read_u16(memory,0x639a74u);
    candidates=game_memory_read_u32(memory,owner+0x74u);distances=game_memory_read_u32(memory,owner+0x84u);
    if(game_memory_read_u32(memory,owner+8u)){
        static const unsigned clear[]={0x34u,0x10u,0xcu,0x40u,0x18u,4u,8u};
        game_memory_write_u32(memory,selected,0u);
        for(i=0u;i<sizeof(clear)/sizeof(clear[0]);i++)game_memory_write_u32(memory,owner+clear[i],0u);
        game_memory_write_u32(memory,owner+0x24u,0x7f7fffffu);
        game_memory_write_u32(memory,owner+0x28u,0x7f7fffffu);
        game_memory_write_u32(memory,owner+0x38u,0x7f7fffffu);
        if(!recyclable_outer_reset_payload(memory,owner+0x44u,services) ||
            !recyclable_outer_reset_payload(memory,output,services)){memory->fault=1;return 0u;}
        game_memory_write_u32(memory,owner+0x14u,0u);
        if(!game_memory_read_u32(memory,owner+0x78u))return 0u;
        index=record_root_next(memory,roots,0,count);
        game_memory_write_u32(memory,owner+0x10u,(uint32_t)index);
        if(index<0)return 0u;
        root=game_memory_read_u32(memory,roots+(uint32_t)index*4u);
        position=record_candidate_center(memory,root);
        if(!recyclable_candidate_distances(memory,position,candidates,distances))return 0u;
        record_candidates_sort(memory,candidates,distances,count);
    }
    index=record_signed(game_memory_read_u32(memory,owner+0x10u));
    if(index<0 || index>=count){game_memory_write_u32(memory,owner+8u,1u);return 0u;}
    root=game_memory_read_u32(memory,roots+(uint32_t)index*4u);
    if(root){
        position=record_candidate_center(memory,root);
        result=recyclable_visibility_update(memory,owner,selected,position,output,services);
    }else{
        uint32_t generation=game_memory_read_u32(memory,owner+0x18u);
        if(generation && generation==game_memory_read_u32(memory,0x5d083cu))game_memory_write_u32(memory,0x5d083cu,0u);
        index=record_root_next(memory,roots,index+1,count);
        game_memory_write_u32(memory,owner+0x10u,(uint32_t)index);
        if(index<0){game_memory_write_u32(memory,owner+8u,1u);return 0u;}
    }
    if(memory->fault)return 0u;
    if(result)return 1u;
    if(!(record_float(memory,owner+0x24u)>=record_float(memory,owner+0x38u))){
        game_memory_write_u32(memory,owner+0x38u,game_memory_read_u32(memory,owner+0x24u));
        game_memory_write_u32(memory,owner+0x34u,game_memory_read_u32(memory,owner+0x10u));
    }
    index=record_root_next(memory,roots,index+1,count);
    game_memory_write_u32(memory,owner+0x10u,(uint32_t)index);
    if(index<0){game_memory_write_u32(memory,owner+8u,1u);return 0u;}
    root=game_memory_read_u32(memory,roots+(uint32_t)index*4u);
    position=record_candidate_center(memory,root);
    if(!recyclable_candidate_distances(memory,position,candidates,distances))return 0u;
    game_memory_write_u32(memory,owner+0xcu,0u);game_memory_write_u32(memory,owner+0x14u,0u);
    game_memory_write_u32(memory,owner+4u,0u);game_memory_write_u32(memory,owner+0x40u,0u);
    game_memory_write_u32(memory,owner+0x18u,0u);return memory->fault?0u:1u;
}
int recyclable_projection_cancel(GameMemory *memory,GameAddress owner){
    uint32_t generation=game_memory_read_u32(memory,owner+0x18u);
    if(generation){
        if(generation==game_memory_read_u32(memory,0x5d083cu))game_memory_write_u32(memory,0x5d083cu,0u);
        game_memory_write_u32(memory,owner+0x18u,0u);
    }
    game_memory_write_u32(memory,owner+0x8cu,0u);game_memory_write_u32(memory,owner+8u,1u);
    game_memory_write_u32(memory,owner+4u,1u);return !memory->fault;
}
static int record_queued_cancel(GameMemory *memory,GameAddress owner,unsigned index){
    game_memory_write_u32(memory,owner+0xda0u+index*4u,game_memory_read_u32(memory,0x5ac6acu));
    game_memory_write_u8(memory,owner+0x1a8u+index,0u);game_memory_write_u8(memory,owner+0x1b6u+index,0u);
    game_memory_write_u32(memory,owner+0x1a4u,game_memory_read_u32(memory,owner+0x1a4u)&~(1u<<index));return !memory->fault;
}
static int record_append_center(GameMemory *memory,GameAddress route,GameAddress member,const RecyclableRecordServices *services){
    return recyclable_outer_append(memory,route,
        (uint8_t)record_world_cell(((long double)record_float(memory,member+4u)+record_float(memory,member+0xcu))*record_float(memory,0x57464cu)),
        (uint8_t)record_world_cell(((long double)record_float(memory,member+8u)+record_float(memory,member+0x10u))*record_float(memory,0x57464cu)),0u,0u,services)!=0u;
}
int recyclable_queued_root_transition(GameMemory *memory,GameAddress owner,const RecyclableRecordServices *services){
    GameAddress controller=owner+0x1c8u,source,roots,candidates,root=0u,candidate,route;
    unsigned index=game_memory_read_u32(memory,owner+0x1c4u),state,i,j,root_index=0u,candidate_index=0u;
    int count;float best;uint32_t best_bits;RecyclablePosition position;
    if(index>=14u){memory->fault=1;return 0;}
    state=game_memory_read_u8(memory,owner+0x1b6u+index);
    if(state>1u)return !memory->fault;
    (void)gameplay_state_refresh_entity_buckets(memory,controller,1,0);
    if(memory->fault)return 0;
    if(!state){
        if(!game_memory_read_u32(memory,owner+0x240u))return record_queued_cancel(memory,owner,index);
        source=game_memory_read_u32(memory,owner+0x244u)?game_memory_read_u32(memory,owner+0x230u):
            game_memory_read_u32(memory,owner+0x248u)?game_memory_read_u32(memory,owner+0x238u):0u;
        if(!source)return record_queued_cancel(memory,owner,index);
        count=(int16_t)game_memory_read_u16(memory,0x639a74u);
        if(count<0){memory->fault=1;return 0;}
        if(game_memory_copy_elements(memory,game_memory_read_u32(memory,owner+0x23cu),source,(unsigned)count,4u,0)!=(unsigned)count)return 0;
        game_memory_write_u32(memory,owner+0x254u,1u);game_memory_write_u8(memory,owner+0x1b6u+index,1u);return !memory->fault;
    }
    if(record_signed(game_memory_read_u32(memory,owner+0x28cu))<1){
        if(!recyclable_projection_cancel(memory,controller))return 0;
        return record_queued_cancel(memory,owner,index);
    }
    route=game_memory_read_u32(memory,owner+0x430u+index*4u);
    if(recyclable_projection_update(memory,controller,owner+0x340u+index*4u,route,services))return !memory->fault;
    if(memory->fault)return 0;
    roots=game_memory_read_u32(memory,owner+0x234u);
    game_memory_write_u32(memory,owner+0x254u,0u);
    candidate=game_memory_read_u32(memory,owner+0x340u+index*4u);
    if(candidate){
        root_index=game_memory_read_u32(memory,owner+0x1fcu);
        game_memory_write_u32(memory,owner+0x378u+index*4u,root_index);
        count=(int16_t)game_memory_read_u16(memory,0x639a74u);
        if(root_index>=(unsigned)count){memory->fault=1;return 0;}
        root=game_memory_read_u32(memory,roots+root_index*4u);
        if(!root)return record_queued_cancel(memory,owner,index);
        position=record_candidate_center(memory,root);
        game_memory_write_u32(memory,owner+0xd2cu,record_float_bits(position.x));
        game_memory_write_u32(memory,owner+0xd34u,record_float_bits(position.z));
        if(!recyclable_route_simplify(memory,route,0u,services))return 0;
    }else{
        candidates=game_memory_read_u32(memory,owner+0x23cu);best_bits=0x7f7fffffu;memcpy(&best,&best_bits,4u);
        count=(int16_t)game_memory_read_u16(memory,0x639a74u);
        for(i=0u;i<(unsigned)(count>0?count:0);i++){
            GameAddress member=game_memory_read_u32(memory,candidates+i*4u);
            if(!member)continue;
            position=record_candidate_center(memory,member);
            for(j=0u;j<(unsigned)count;j++){
                GameAddress destination=game_memory_read_u32(memory,roots+j*4u);long double dx,dz,distance;
                if(!destination || !game_memory_read_u32(memory,game_memory_read_u32(memory,destination+0x18u)))continue;
                dx=((long double)record_float(memory,destination+4u)+record_float(memory,destination+0xcu))*record_float(memory,0x57464cu)-position.x;
                dz=((long double)record_float(memory,destination+8u)+record_float(memory,destination+0x10u))*record_float(memory,0x57464cu)-position.z;
                distance=dx*dx+dz*dz;
                if(!(distance>=best)){best=(float)distance;root_index=j;candidate_index=i;}
            }
        }
        best_bits=record_float_bits(best);
        if(record_signed(best_bits)>record_signed(0x50a6cfe4u))return record_queued_cancel(memory,owner,index);
        if(!recyclable_outer_reset_payload(memory,route,services))return 0;
        game_memory_write_u32(memory,owner+0x378u+index*4u,root_index);
        candidate=game_memory_read_u32(memory,candidates+candidate_index*4u);
        root=game_memory_read_u32(memory,roots+root_index*4u);
        if(!candidate || !root){memory->fault=1;return 0;}
        game_memory_write_u32(memory,owner+0x340u+index*4u,candidate);
        if(!record_append_center(memory,route,candidate,services))return 0;
        position=record_candidate_center(memory,root);
        game_memory_write_u32(memory,owner+0xd2cu,record_float_bits(position.x));
        game_memory_write_u32(memory,owner+0xd34u,record_float_bits(position.z));
        if(!recyclable_outer_append(memory,route,(uint8_t)record_world_cell(position.x),(uint8_t)record_world_cell(position.z),0u,0u,services))return 0;
    }
    game_memory_write_u8(memory,owner+0x1b6u+index,0u);
    game_memory_write_u8(memory,owner+0x1a8u+index,(uint8_t)(game_memory_read_u8(memory,owner+0x1a8u+index)+1u));
    return !memory->fault;
}
long double recyclable_node_distance(GameMemory *memory,GameAddress owner,GameAddress node){
    GameAddress tail=game_memory_read_u32(memory,node+0x10u);
    unsigned count=game_memory_read_u8(memory,tail+0xdu);
    int last_x=game_memory_read_u8(memory,game_memory_read_u32(memory,tail+0x10u)+count-1u);
    int last_y=game_memory_read_u8(memory,game_memory_read_u32(memory,tail+0x14u)+count-1u);
    int dx=last_x-game_memory_read_u8(memory,owner+0xcu),dy=last_y-game_memory_read_u8(memory,owner+0xdu);
    uint32_t first_bits=game_memory_read_u32(memory,owner+0x14u),second_bits=game_memory_read_u32(memory,owner+0x18u);
    long double distance=sqrtl((long double)dx*dx+(long double)(dy*dy));
    if(first_bits && first_bits<0x80000000u){
        GameAddress first=game_memory_read_u32(memory,node+0xcu);
        int ex=game_memory_read_u8(memory,game_memory_read_u32(memory,first+0x10u))-last_x;
        int ey=game_memory_read_u8(memory,game_memory_read_u32(memory,first+0x14u))-last_y;
        int square=ex*ex+ey*ey;float stored=(float)((long double)record_float(memory,owner+0x14u)-distance);
        if(square<64)stored=(float)((long double)record_float(memory,0x573524u)-sqrtl((long double)ex*ex+(long double)(ey*ey))+(long double)stored);
        return stored;
    }
    if(second_bits && second_bits<0x80000000u)return (long double)record_float(memory,owner+0x18u)-record_float(memory,node+0x14u);
    return distance;
}
int recyclable_container_insert(GameMemory *memory,GameAddress owner,GameAddress node,int indexed,const RecyclableRecordServices *services){
    GameAddress head,tail,cursor,retained;long double distance;float key,hd,td;unsigned visits=0u;
    if(indexed){
        GameAddress descriptor=game_memory_read_u32(memory,node+0x10u);unsigned count=game_memory_read_u8(memory,descriptor+0xdu);
        unsigned high=game_memory_read_u8(memory,game_memory_read_u32(memory,descriptor+0x10u)+count-1u);
        unsigned low=game_memory_read_u8(memory,game_memory_read_u32(memory,descriptor+0x14u)+count-1u);
        uint32_t index=(high<<7u)+low;
        if(indexed==2){
            index=((high-game_memory_read_u32(memory,owner+0x4020u))<<6u)+low-game_memory_read_u32(memory,owner+0x4024u);
            if(index>=0x1000u)return 0;
        }else if(index>=0x4000u)return 0;
        game_memory_write_u32(memory,owner+0x20u+index*4u,node);
    }
    distance=recyclable_node_distance(memory,owner,node);
    game_memory_write_u32(memory,node+0x18u,record_float_bits((float)distance));
    key=(float)(distance+((long double)record_signed(game_memory_read_u32(memory,node+0x20u))+(long double)record_float(memory,node+0x14u)));
    game_memory_write_u32(memory,node+0x1cu,record_float_bits(key));
    head=game_memory_read_u32(memory,owner+4u);tail=game_memory_read_u32(memory,owner+8u);cursor=head;
    if(head){
        hd=(float)(key-record_float(memory,head+0x1cu));td=(float)(record_float(memory,tail+0x1cu)-key);
        if(record_float_bits(hd)<=0x80000000u){
            if(record_float_bits(td)>0x80000000u)cursor=0u;
            else if(td>hd){
                cursor=game_memory_read_u32(memory,head+4u);
                while(cursor && !(record_float(memory,cursor+0x1cu)>=key)){if(++visits>65536u)return 0;cursor=game_memory_read_u32(memory,cursor+4u);}
            }else{
                cursor=game_memory_read_u32(memory,tail+8u);
                while(cursor && record_float(memory,cursor+0x1cu)>key){if(++visits>65536u)return 0;cursor=game_memory_read_u32(memory,cursor+8u);}
            }
        }
    }
    if(cursor){
        GameAddress previous=game_memory_read_u32(memory,cursor+8u);
        game_memory_write_u32(memory,node+8u,previous);game_memory_write_u32(memory,node+4u,cursor);
        if(head==cursor)game_memory_write_u32(memory,owner+4u,node);else game_memory_write_u32(memory,previous+4u,node);
        game_memory_write_u32(memory,cursor+8u,node);
    }else{
        game_memory_write_u32(memory,node+8u,tail);if(tail)game_memory_write_u32(memory,tail+4u,node);
        if(!head)game_memory_write_u32(memory,owner+4u,node);
        game_memory_write_u32(memory,owner+8u,node);
    }
    retained=game_memory_read_u32(memory,owner+0x1cu);
    {
        unsigned enabled=game_memory_read_u8(memory,game_memory_read_u32(memory,retained+0xcu)+0xdu);
        float a=record_float(memory,retained+0x18u),b=record_float(memory,node+0x18u);
        long double old_key=(long double)record_signed(game_memory_read_u32(memory,retained+0x20u))+record_float(memory,retained+0x14u);
        long double new_key=(long double)record_signed(game_memory_read_u32(memory,node+0x20u))+record_float(memory,node+0x14u);
        if(!enabled || a>b || (!(a<b) && !(a>b) && !(new_key>=old_key)))
            if(!recyclable_outer_copy(memory,retained,node,services))return 0;
    }
    return !memory->fault;
}
