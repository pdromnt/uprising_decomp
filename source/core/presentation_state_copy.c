#include "presentation_state_copy.h"
#include "game_initializers.h"
#include "game_intrusive_list.h"
#include <string.h>

GamePresentationRecordCopy game_presentation_record_copy(GameMemory *memory,
        GameAddress destination, GameAddress source) {
    GamePresentationRecordCopy result = {0u, 0u, 0u};
    uint16_t count = game_memory_read_u16(memory, source);
    if (memory->fault) return result;
    game_memory_write_u16(memory, destination, count);
    if (memory->fault) return result;
    result.eax = game_memory_read_u32(memory, source + 4u);
    if (memory->fault) return result;
    game_memory_write_u32(memory, destination + 4u, result.eax);
    if (memory->fault) return result;
    result.edx = game_memory_read_u32(memory, source + 8u);
    if (memory->fault) return result;
    game_memory_write_u32(memory, destination + 8u, result.edx);
    if (memory->fault) return result;
    result.eax = game_memory_read_u32(memory, source + 0x0cu);
    if (memory->fault) return result;
    game_memory_write_u32(memory, destination + 0x0cu, result.eax);
    if (memory->fault) return result;
    result.edx = game_memory_read_u32(memory, source + 0x10u);
    if (memory->fault) return result;
    game_memory_write_u32(memory, destination + 0x10u, result.edx);
    if (memory->fault) return result;
    result.ecx = game_memory_read_u32(memory, source + 0x14u);
    if (!memory->fault)
        game_memory_write_u32(memory, destination + 0x14u, result.ecx);
    return result;
}

static int release_field(GameMemory *memory,GameAddress field,const GamePresentationStorageServices *services){
    GameAddress storage=game_memory_read_u32(memory,field);
    if(storage && (!services->release || !services->release(services->context,storage)))return 0;
    game_memory_write_u32(memory,field,0u);return !memory->fault;
}
static int allocate_field(GameMemory *memory,GameAddress field,size_t bytes,const GamePresentationStorageServices *services){
    GameAddress storage=services->allocate?services->allocate(services->context,bytes):0u;
    game_memory_write_u32(memory,field,storage);return !memory->fault && (storage || !bytes);
}
int game_presentation_construct_values(GameMemory *memory,GameAddress object,const float dimensions[3],const GamePresentationDefinition *definition,const GamePresentationStorageServices *services){
    uint32_t slot;unsigned index;
    if(!definition || !services || definition->slot_count<0 || definition->slot_count>65536 || definition->record_count>32767u)return 0;
    game_initializer_0042cfb0(memory,object);
    for(index=0u;index<3u;index++){uint32_t bits;memcpy(&bits,dimensions+index,4u);game_memory_write_u32(memory,object+0x2cu+index*4u,bits);}
    game_memory_write_u32(memory,object+0x38u,definition->radius);
    game_memory_write_u32(memory,object+0x54u,definition->model);
    game_memory_write_u32(memory,object,0x5743e0u);
    game_memory_write_u32(memory,object+0x10u,game_memory_read_u32(memory,object+0x10u)&~1u);
    game_memory_write_u32(memory,object,0x573f00u);
    game_memory_write_u32(memory,object+0x98u,definition->data_count);game_memory_write_u32(memory,object+0x94u,definition->data);
    game_memory_write_u32(memory,object+0x80u,(uint32_t)definition->slot_count);game_memory_write_u32(memory,object+0x7cu,definition->positions);
    if(!allocate_field(memory,object+0x84u,(size_t)definition->slot_count*4u,services))return 0;
    for(slot=0u;slot<(uint32_t)definition->slot_count;slot++)game_memory_write_u32(memory,game_memory_read_u32(memory,object+0x84u)+slot*4u,1u);
    game_memory_write_u16(memory,object+0x64u,definition->record_count);game_memory_write_u32(memory,object+0x68u,definition->records);
    game_memory_write_u32(memory,object+0x10u,game_memory_read_u32(memory,object+0x10u)|0x80u);game_memory_write_u32(memory,object+0x9cu,1u);
    for(index=0u;index<4u;index++)game_memory_write_u32(memory,object+0x6cu+index*4u,0u);
    game_memory_write_u32(memory,object+0x8cu,0u);game_memory_write_u32(memory,object+0x90u,0u);
    return !memory->fault;
}
int game_presentation_construct(GameMemory *memory,GameAddress object,const GamePresentationDefinition *definition,const GamePresentationStorageServices *services){
    float dimensions[3];unsigned i;if(!definition)return 0;
    for(i=0u;i<3u;i++){uint32_t bits=game_memory_read_u32(memory,definition->dimensions+i*4u);memcpy(dimensions+i,&bits,4u);}
    return !memory->fault && game_presentation_construct_values(memory,object,dimensions,definition,services);
}
int game_presentation_base_copy(GameMemory *memory,GameAddress object,GameAddress source,const GamePresentationStorageServices *services){
    uint32_t vertex_count;GameAddress model;unsigned axis;
    if(!services)return 0;
    game_initializer_0042cfb0(memory,object);
    vertex_count=game_memory_read_u16(memory,source+0x44u);if(memory->fault || vertex_count>32767u)return 0;
    game_memory_write_u8(memory,object+0xcu,game_memory_read_u8(memory,source+0xcu));
    game_memory_write_u32(memory,object+0x10u,game_memory_read_u32(memory,source+0x10u));
    for(axis=0u;axis<5u;axis++)game_memory_write_u32(memory,object+0x2cu+axis*4u,game_memory_read_u32(memory,source+0x2cu+axis*4u));
    game_memory_write_u16(memory,object+0x44u,(uint16_t)vertex_count);game_memory_write_u32(memory,object+0x50u,source);
    if(vertex_count && !allocate_field(memory,object+0x40u,(size_t)vertex_count*12u,services))return 0;
    model=game_memory_read_u32(memory,source+0x54u);
    if(model){GameAddress clone=0u;if(!services->clone_model || !services->clone_model(services->context,model,&clone) || !clone)return 0;game_memory_write_u32(memory,object+0x54u,clone);}
    return !memory->fault;
}
static int copy_presentation(GameMemory *memory,GameAddress object,GameAddress source,const GamePresentationStorageServices *services){
    uint32_t record_count,slot_count,data_count,index;GameAddress records;unsigned axis;
    if(!services)return -1;
    record_count=game_memory_read_u16(memory,source+0x64u);slot_count=game_memory_read_u32(memory,source+0x80u);
    data_count=game_memory_read_u32(memory,source+0x98u);
    if(memory->fault || record_count>32767u || slot_count>65536u || data_count>65536u)return -1;
    game_memory_write_u16(memory,object+0x64u,0u);game_memory_write_u32(memory,object+0x68u,0u);
    game_memory_write_u32(memory,object+0x94u,0u);game_memory_write_u32(memory,object+0x7cu,0u);game_memory_write_u32(memory,object+0x84u,0u);
    game_memory_write_u32(memory,object+0x9cu,0u);
    if(!game_presentation_base_copy(memory,object,source,services))return 0;
    game_memory_write_u32(memory,object,0x573f00u);game_memory_write_u32(memory,object+0x10u,(game_memory_read_u32(memory,object+0x10u)&~1u)|0x8080u);
    if(!allocate_field(memory,object+0x68u,(size_t)record_count*48u,services))return 0;
    records=game_memory_read_u32(memory,object+0x68u);
    for(index=0u;index<record_count;index++){
        game_memory_write_u32(memory,records+index*48u+0x24u,0u);game_memory_write_u32(memory,records+index*48u+0x2cu,0u);
    }
    game_memory_write_u16(memory,object+0x64u,(uint16_t)record_count);
    for(index=0u;index<record_count;index++){
        GameAddress record=records+index*48u,original=game_memory_read_u32(memory,source+0x68u)+index*48u;uint32_t count;
        (void)game_presentation_record_copy(memory,record,original);count=game_memory_read_u16(memory,record);
        if(count>32767u || !allocate_field(memory,record+0x24u,(size_t)count*12u,services) ||
           !allocate_field(memory,record+0x2cu,(size_t)count*4u,services))return 0;
    }
    game_memory_write_u32(memory,object+0x9cu,0u);
    for(axis=0u;axis<4u;axis++)game_memory_write_u32(memory,object+0x6cu+axis*4u,0u);
    game_memory_write_u32(memory,object+0x8cu,0u);game_memory_write_u32(memory,object+0x90u,0u);
    game_memory_write_u32(memory,object+0x98u,data_count);
    if(!allocate_field(memory,object+0x94u,(size_t)data_count*12u,services))return 0;
    game_memory_write_u32(memory,object+0x80u,slot_count);
    if(!allocate_field(memory,object+0x7cu,(size_t)slot_count*12u,services) || !allocate_field(memory,object+0x84u,(size_t)slot_count*4u,services))return 0;
    for(index=0u;index<slot_count;index++)game_memory_write_u32(memory,game_memory_read_u32(memory,object+0x84u)+index*4u,1u);
    return !memory->fault;
}
int game_presentation_copy(GameMemory *memory,GameAddress object,GameAddress source,const GamePresentationStorageServices *services){
    int success=copy_presentation(memory,object,source,services);
    if(success==0 && !memory->fault && services)(void)game_presentation_destroy(memory,object,services);
    return success>0;
}
int game_presentation_destroy(GameMemory *memory,GameAddress object,const GamePresentationStorageServices *services){
    uint32_t count,index,mode;GameAddress records;
    if(!services)return 0;
    game_memory_write_u32(memory,object,0x573f00u);count=game_memory_read_u16(memory,object+0x64u);
    records=game_memory_read_u32(memory,object+0x68u);mode=game_memory_read_u32(memory,object+0x9cu);
    if(count>32767u)return 0;
    for(index=0u;index<count;index++){
        GameAddress record=records+index*48u;
        if(!mode && (!release_field(memory,record+0x2cu,services) || !release_field(memory,record+0x24u,services)))return 0;
        if(mode && (!release_field(memory,record+0x10u,services) || !release_field(memory,record+0x14u,services)))return 0;
    }
    if(!release_field(memory,object+0x68u,services) || !release_field(memory,object+0x94u,services) ||
       !release_field(memory,object+0x7cu,services) || !release_field(memory,object+0x84u,services))return 0;
    return game_presentation_base_destroy(memory,object,services);
}
int game_presentation_base_destroy(GameMemory *memory,GameAddress object,const GamePresentationStorageServices *services){
    GameAddress model;if(!services)return 0;
    game_memory_write_u32(memory,object,0x5721b8u);game_memory_write_u32(memory,object+0x58u,0u);
    if(game_memory_read_u32(memory,object+8u))(void)game_list_detach_node(memory,object,4u,8u);
    if(game_memory_read_u32(memory,object+0x4cu))(void)game_list_detach_node(memory,object,0x48u,0x4cu);
    model=game_memory_read_u32(memory,object+0x54u);
    if(model && (!services->release_model || !services->release_model(services->context,model)))return 0;
    game_memory_write_u32(memory,object+0x54u,0u);
    if(!release_field(memory,object+0x40u,services))return 0;
    if(!game_memory_read_u32(memory,object+0x50u) && !release_field(memory,object+0x3cu,services))return 0;
    game_memory_write_u32(memory,object,0x572230u);return !memory->fault;
}
int game_presentation_variant(GameMemory *memory,GameAddress object,unsigned kind){
    static const uint32_t tables[8]={0x575640u,0x5756d8u,0x575770u,0x575808u,0x576968u,0x576a00u,0x576a98u,0x576b30u};
    static const uint32_t first[8]={0x627978u,0x6279f0u,0x627a18u,0x627a40u,0x6318e0u,0x6318f0u,0x6318d8u,0x6318d0u};
    static const uint32_t second[8]={0x6279c8u,0x6279a0u,0x627950u,0x627a68u,0x6318f8u,0x631900u,0x6318c8u,0x6318e8u};
    if(kind==0x77u){GameAddress source=game_memory_read_u32(memory,object+0x50u);
        /*43cc60 clones the existing4820c0 owner and its subtype. The caches
         *are shared bases; subtype consumers choose their own indexed row. */
        game_memory_write_u32(memory,object,0x572e50u);game_memory_write_u8(memory,object+12u,0x77u);
        game_memory_write_u32(memory,object+0xa0u,source?game_memory_read_u32(memory,source+0xa0u):0u);
        game_memory_write_u32(memory,object+0x8cu,0x5ad870u);game_memory_write_u32(memory,object+0x90u,0x5ad860u);
        return !memory->fault;
    }
    if(kind==0x60u || kind==0x62u || kind==0x63u || kind==0x64u){
        unsigned n=kind==0x60u?0u:kind==0x62u?1u:kind==0x63u?2u:3u;
        static const uint32_t methods[]={0x5762d8u,0x5763e8u,0x576470u,0x576360u};
        static const uint32_t primary[]={0x62972cu,0x629728u,0x629724u,0x629710u};
        static const uint32_t secondary[]={0x629720u,0x629714u,0x62971cu,0x629718u};
        /*533bb0/5344b0/5346b0/533e00 and their copied constructors.
         *The shared presentation owns arrays/model; the producer links start empty. */
        game_memory_write_u32(memory,object,methods[n]);game_memory_write_u8(memory,object+12u,(uint8_t)kind);
        game_memory_write_u32(memory,object+16u,game_memory_read_u32(memory,object+16u)|0x100000u);
        game_memory_write_u32(memory,object+0xa8u,0);game_memory_write_u32(memory,object+0xacu,0);
        game_memory_write_u32(memory,object+0x8cu,primary[n]);game_memory_write_u32(memory,object+0x90u,secondary[n]);
        return !memory->fault;
    }
    if(kind<0x80u || kind>0x87u)return 0;
    game_memory_write_u32(memory,object,0x575040u);game_memory_write_u32(memory,object+0xa0u,0u);
    game_memory_write_u32(memory,object+0x10u,game_memory_read_u32(memory,object+0x10u)|0x100u);
    game_memory_write_u32(memory,object,tables[kind-0x80u]);game_memory_write_u8(memory,object+0xcu,(uint8_t)kind);
    game_memory_write_u32(memory,object+0x8cu,first[kind-0x80u]);game_memory_write_u32(memory,object+0x90u,second[kind-0x80u]);return !memory->fault;
}
static const GamePresentationFactoryFamily presentation_families[]={
    {0x60u,1u,0x5adb74u,0x58477cu,0x62972cu,0x629720u},
    {0x62u,1u,0x5add8cu,0x584784u,0x629728u,0x629714u},
    {0x63u,1u,0x5ada30u,0x584788u,0x629724u,0x62971cu},
    {0x64u,1u,0x5add84u,0x584780u,0x629710u,0x629718u},
    {0x77u,4u,0x5adb00u,0x584790u,0x5ad870u,0x5ad860u},
    {0x80u,10u,0x5adc78u,0x5847e0u,0x627978u,0x6279c8u},
    {0x81u,10u,0x5add58u,0x584808u,0x6279f0u,0x6279a0u},
    {0x82u,10u,0x5adc10u,0x584830u,0x627a18u,0x627950u},
    {0x83u,10u,0x5ada38u,0x584858u,0x627a40u,0x627a68u},
    {0x84u,2u,0x5ada70u,0x584880u,0x6318e0u,0x6318f8u},
    {0x85u,2u,0x5adb48u,0x584888u,0x6318f0u,0x631900u},
    {0x86u,2u,0x5ada28u,0x584890u,0x6318d8u,0x6318c8u},
    {0x87u,2u,0x5adcd0u,0x584898u,0x6318d0u,0x6318e8u}
};
const GamePresentationFactoryFamily *game_presentation_factory_family(unsigned kind){
    unsigned i;for(i=0;i<sizeof(presentation_families)/sizeof(presentation_families[0]);i++)
        if(presentation_families[i].kind==kind)return presentation_families+i;
    return NULL;
}
static int discard_factory_inputs(GameMemory *memory,const GamePresentationDefinition *definition,const GamePresentationStorageServices *storage){
    unsigned index;
    if(definition->record_count>32767u)return 0;
    for(index=0u;index<definition->record_count;index++){
        GameAddress row=definition->records+index*48u;
        if(!release_field(memory,row+0x10u,storage) || !release_field(memory,row+0x14u,storage))return 0;
    }
    if(definition->records && !storage->release(storage->context,definition->records))return 0;
    if(definition->data && !storage->release(storage->context,definition->data))return 0;
    if(definition->positions && !storage->release(storage->context,definition->positions))return 0;
    return !definition->model || (storage->release_model && storage->release_model(storage->context,definition->model));
}
GameAddress game_presentation_factory_template(GameMemory *memory,const GamePresentationFactoryFamily *family,uint16_t slot,const GamePresentationFactoryServices *services){
    GameAddress object,publication;GamePresentationDefinition definition={0};float dimensions[3];int loaded;
    if(!family || slot>=family->slots || !services || !services->storage || !services->storage->allocate || !services->storage->release || !services->describe)return 0u;
    publication=family->publication+(uint32_t)slot*4u;object=game_memory_read_u32(memory,publication);
    if(memory->fault || object)return object;
    if(game_memory_read_u32(memory,0x624f8cu)==10u || memory->fault)return 0u;
    loaded=services->describe(services->context,family,slot,dimensions,&definition);
    if(loaded<=0){if(loaded<0)memory->fault=1;return 0u;}
    object=services->storage->allocate(services->storage->context,family->kind==0x77u?0xa4u:0xb0u);
    if(!object){(void)discard_factory_inputs(memory,&definition,services->storage);memory->fault=1;return 0u;}
    game_memory_write_u32(memory,object+0x84u,0u);
    if(!game_presentation_construct_values(memory,object,dimensions,&definition,services->storage)){
        if(!memory->fault){(void)release_field(memory,object+0x84u,services->storage);(void)discard_factory_inputs(memory,&definition,services->storage);}
        (void)services->storage->release(services->storage->context,object);memory->fault=1;return 0u;
    }
    if(!game_presentation_variant(memory,object,family->kind))return 0u;
    if(family->kind==0x77u)game_memory_write_u32(memory,object+0xa0u,slot);
    game_memory_write_u32(memory,publication,object);return memory->fault?0u:object;
}
int game_presentation_factory_fill(GameMemory *memory,const GamePresentationFactoryFamily *family,uint16_t slot,int32_t count,const GamePresentationFactoryServices *services){
    GameAddress source=game_presentation_factory_template(memory,family,slot,services);int32_t index;
    if(!source || memory->fault)return 0;
    if(count>65536){memory->fault=1;return 0;}
    for(index=0;index<count;index++){
        GameAddress object=services->storage->allocate(services->storage->context,family->kind==0x77u?0xa4u:0xb0u);
        if(!object){memory->fault=1;return 0;}
        if(!game_presentation_copy(memory,object,source,services->storage)){
            (void)services->storage->release(services->storage->context,object);memory->fault=1;return 0;
        }
        if(!game_presentation_variant(memory,object,family->kind))return 0;
        (void)game_list_insert_after(memory,object,source,4u,8u);
        if(memory->fault)return 0;
    }
    return 1;
}
GameAddress game_presentation_factory_take(GameMemory *memory,const GamePresentationFactoryFamily *family,uint16_t slot,const GamePresentationFactoryServices *services){
    GameAddress source=game_presentation_factory_template(memory,family,slot,services),object;
    if(!source || memory->fault)return 0u;
    if(!game_memory_read_u32(memory,source+4u) && !game_presentation_factory_fill(memory,family,slot,1,services))return 0u;
    object=game_memory_read_u32(memory,source+4u);
    if(!object || memory->fault)return 0u;
    (void)game_list_detach_node(memory,object,8u,4u);return memory->fault?0u:object;
}
int game_presentation_bind(GameMemory *memory,GameAddress object,uint32_t identity,
        const float position[3],const uint16_t angles[3],int prepare,
        const GamePresentationBindingServices *services){
    float local[3];uint32_t count,index;GameAddress flags;
    if(!services || !services->place)return 0;
    memcpy(local,position,sizeof(local));
    if(prepare && (!services->prepare || !services->prepare(services->context,object,local,angles[0])))return 0;
    for(index=0u;index<4u;index++)game_memory_write_u32(memory,object+0x6cu+index*4u,0u);
    count=game_memory_read_u32(memory,object+0x80u);flags=game_memory_read_u32(memory,object+0x84u);
    /* Negative retail counts skip the signed loop. Bound positive counts. */
    if(count>65536u && count<0x80000000u)return 0;
    if(count<0x80000000u)for(index=0u;index<count && !memory->fault;index++)game_memory_write_u32(memory,flags+index*4u,1u);
    game_memory_write_u32(memory,object+0x88u,0u);
    return !memory->fault && services->place(services->context,object,identity,local,angles);
}
