#include "vehicle_presentation.h"
#include "game_state_ops.h"
#include "game_initializers.h"
#include "state_helper_plans.h"
#include "game_intrusive_list.h"
#include "game_matrix.h"
#include "gameplay_geometry.h"
#include "gameplay_state.h"
#include "spatial_attachment.h"
#include <math.h>
#include <string.h>
static float real_at(GameMemory *memory,GameAddress address){uint32_t bits=game_memory_read_u32(memory,address);float value;memcpy(&value,&bits,4u);return value;}
static void put_real(GameMemory *memory,GameAddress address,float value){uint32_t bits;memcpy(&bits,&value,4u);game_memory_write_u32(memory,address,bits);}
static int32_t signed_count(uint32_t bits){int32_t value;memcpy(&value,&bits,4u);return value;}
static void descriptor_copy(GameMemory *memory,GameAddress object,GameAddress source,uint32_t destination){
    unsigned axis;for(axis=0u;axis<3u;axis++)game_memory_write_u32(memory,object+destination+axis*4u,game_memory_read_u32(memory,source+axis*4u));
}
static void descriptor_height(GameMemory *memory,GameAddress source,GameAddress destination){
    put_real(memory,destination,(float)((long double)real_at(memory,source)*real_at(memory,0x5778dcu)+real_at(memory,destination)));
}
int vehicle_presentation_class29_finish(GameMemory *m,GameAddress o,GameAddress source){
    unsigned i,axis;float x,y,z;
    game_memory_write_u32(m,o,0x573230);game_memory_write_u8(m,o+12,0x29);
    game_memory_write_u32(m,o+16,game_memory_read_u32(m,o+16)|8);
    if(source){for(i=0x140;i<0x170;i+=4)game_memory_write_u32(m,o+i,game_memory_read_u32(m,source+i));}
    else{
        x=real_at(m,o+0x2c);y=-real_at(m,o+0x30);z=real_at(m,o+0x34);
        for(i=0;i<4;i++){float corner[3]={i==0 || i==3?x:-x,y,i<2?z:-z};
            for(axis=0;axis<3;axis++)put_real(m,o+0x140+i*12+axis*4,corner[axis]);}
    }
    return !m->fault;
}
int vehicle_presentation_finish(GameMemory *memory,GameAddress object,const VehiclePresentationDefinition *definition,uint8_t type){
    static const uint32_t anchors[3]={0x1c8u,0x1bcu,0x1d4u};
    unsigned index,axis;GameAddress model,table,descriptor;uint16_t lower_word,upper_word;int32_t lower,upper;
    float x=real_at(memory,object+0x2cu),y=real_at(memory,object+0x30u),z=real_at(memory,object+0x34u);
    if(!vehicle_presentation_family(type))return 0;
    game_memory_write_u16(memory,object+0x224u,0u);game_memory_write_u32(memory,object,0x571288u);
    model=game_memory_read_u32(memory,object+0x54u);table=game_memory_read_u32(memory,model+0x4cu);
    game_memory_write_u32(memory,object+0x1b4u,table);game_memory_write_u32(memory,object+0x1b0u,game_memory_read_u32(memory,table+0x4cu));
    game_memory_write_u16(memory,object+0x1e2u,(uint16_t)definition->groups[0]);
    game_memory_write_u16(memory,object+0x1e0u,(uint16_t)(definition->groups[0]+definition->groups[1]));
    for(index=0u;index<3u;index++)for(axis=0u;axis<3u;axis++)put_real(memory,object+anchors[index]+axis*4u,definition->anchors[index][axis]);
    for(index=0u;index<4u;index++){
        put_real(memory,object+0x228u+index*12u,index==0u || index==3u?x:-x);
        put_real(memory,object+0x22cu+index*12u,-y);put_real(memory,object+0x230u+index*12u,index<2u?z:-z);
    }
    game_memory_write_u32(memory,object+0x21cu,0u);game_memory_write_u32(memory,object+0x220u,0u);
    game_memory_write_u32(memory,object+0x258u,0x6058a0u);game_memory_write_u32(memory,object+0x25cu,0x6058d0u);
    game_memory_write_u8(memory,object+12u,0x21u);game_memory_write_u32(memory,object+16u,game_memory_read_u32(memory,object+16u)|0x3248u);
    if(type==0x21u)return !memory->fault;
    game_initializer_type22_identity(memory,object,0x571088u,0x22u,0x5ab0c0u,0x5ab0c8u);
    if(type==0x22u)return !memory->fault;
    if(type==0x24u || type==0x25u || type==0x28u){
        const VehiclePresentationFamily *family=vehicle_presentation_family(type);
        uint32_t flags=game_memory_read_u32(memory,object+16u);
        game_initializer_type22_identity(memory,object,type==0x24u?0x5718b8u:type==0x25u?0x5714b8u:0x5716b8u,type,family->part_counts,family->parts);
        game_memory_write_u32(memory,object+16u,type==0x24u?(flags&~0x2000u):type==0x28u?((flags&~0x1000u)|0x8000u):flags);
        return !memory->fault;
    }
    game_state_initialize_descriptor(memory,object+0x264u);
    game_memory_write_u32(memory,object,0x5776d8u);game_memory_write_u32(memory,object+0x264u,0x5776b8u);
    game_memory_write_u32(memory,object+16u,game_memory_read_u32(memory,object+16u)|0x8100u);
    game_memory_write_u32(memory,object+0x294u,0u);game_memory_write_u32(memory,object+0x298u,0u);
    game_memory_write_u32(memory,object+0x33cu,1u);game_memory_write_u32(memory,object+0x2c8u,0u);game_memory_write_u32(memory,object+0x2b8u,0u);
    game_memory_write_u8(memory,object+12u,0x23u);game_memory_write_u32(memory,object+0x258u,0x639af0u);game_memory_write_u32(memory,object+0x25cu,0x639ae8u);
    table=game_memory_read_u32(memory,object+0x3cu);descriptor_copy(memory,object,table+12u,0x2bcu);descriptor_height(memory,table+24u,object+0x2c0u);
    lower_word=game_memory_read_u16(memory,object+0x1e2u);upper_word=game_memory_read_u16(memory,object+0x1e0u);
    lower=(int32_t)lower_word-((lower_word&0x8000u)?65536:0);upper=(int32_t)upper_word-((upper_word&0x8000u)?65536:0);
    descriptor=table+(uint32_t)lower*28u+12u;descriptor_copy(memory,object,descriptor,0x2ccu);
    /* Original adds this second initial height to the lower sample again. */
    descriptor_height(memory,descriptor+12u,object+0x2c0u);
    for(index=0u;index<(uint32_t)(lower>0?lower:0) && !memory->fault;index++){
        descriptor=table+index*28u+12u;
        if(!(real_at(memory,descriptor+8u)>=real_at(memory,object+0x2c4u))){
            descriptor_copy(memory,object,descriptor,0x2bcu);descriptor_height(memory,descriptor+12u,object+0x2c0u);
        }
    }
    for(index=0u;upper>lower && index<(uint32_t)(upper-lower) && !memory->fault;index++){
        descriptor=table+((uint32_t)lower+index)*28u+12u;
        if(real_at(memory,descriptor)>real_at(memory,object+0x2ccu)){
            descriptor_copy(memory,object,descriptor,0x2ccu);descriptor_height(memory,descriptor+12u,object+0x2d0u);
        }
    }
    return !memory->fault;
}
int vehicle_presentation_construct(GameMemory *memory,GameAddress object,const VehiclePresentationDefinition *definition,uint8_t type){
    PresentationInstanceDefinition base=definition->base;base.count=signed_count(definition->groups[0]+definition->groups[1]+definition->groups[2]);
    return presentation_instance_create(memory,object,&base,1) && vehicle_presentation_finish(memory,object,definition,type);
}
int vehicle_presentation_copy_finish(GameMemory *memory,GameAddress object,GameAddress source,uint8_t type){
    unsigned offset;GameAddress model,nested;uint32_t flags;
    if(!vehicle_presentation_family(type))return 0;
    flags=game_memory_read_u32(memory,source+16u);game_memory_write_u32(memory,object,0x571288u);
    game_memory_write_u16(memory,object+0x224u,0u);game_memory_write_u32(memory,object+16u,flags);
    game_memory_write_u8(memory,object+12u,game_memory_read_u8(memory,source+12u));
    model=game_memory_read_u32(memory,object+0x54u);nested=game_memory_read_u32(memory,model+0x4cu);
    game_memory_write_u32(memory,object+0x1b4u,nested);game_memory_write_u32(memory,object+0x1b0u,game_memory_read_u32(memory,nested+0x4cu));
    game_memory_write_u16(memory,object+0x1e2u,game_memory_read_u16(memory,source+0x1e2u));
    game_memory_write_u16(memory,object+0x1e0u,game_memory_read_u16(memory,source+0x1e0u));
    for(offset=0x1bcu;offset<0x1e0u;offset+=4u)game_memory_write_u32(memory,object+offset,game_memory_read_u32(memory,source+offset));
    for(offset=0x228u;offset<0x258u;offset+=4u)game_memory_write_u32(memory,object+offset,game_memory_read_u32(memory,source+offset));
    game_memory_write_u32(memory,object+0x258u,0x6058a0u);game_memory_write_u32(memory,object+0x25cu,0x6058d0u);
    game_memory_write_u32(memory,object+0x21cu,0u);game_memory_write_u32(memory,object+0x220u,0u);
    game_memory_write_u8(memory,object+12u,0x21u);game_memory_write_u32(memory,object+16u,flags|0x3248u);
    if(type==0x21u)return !memory->fault;
    game_initializer_type22_identity(memory,object,0x571088u,0x22u,0x5ab0c0u,0x5ab0c8u);
    if(type==0x22u)return !memory->fault;
    if(type==0x24u || type==0x25u || type==0x28u){
        const VehiclePresentationFamily *family=vehicle_presentation_family(type);
        uint32_t flags=game_memory_read_u32(memory,object+16u);
        game_initializer_type22_identity(memory,object,type==0x24u?0x5718b8u:type==0x25u?0x5714b8u:0x5716b8u,type,family->part_counts,family->parts);
        game_memory_write_u32(memory,object+16u,type==0x24u?(flags&~0x2000u):type==0x28u?((flags&~0x1000u)|0x8000u):flags);
        return !memory->fault;
    }
    game_state_initialize_descriptor(memory,object+0x264u);state_helper_finish_type23_owner(memory,object);
    /* Type23 copies initialize the embedded service, but do not copy or
       recompute the fresh constructor's descriptor extrema. */
    return !memory->fault;
}
int vehicle_presentation_copy(GameMemory *memory,GameAddress object,GameAddress source,uint8_t type,const PresentationConstructionServices *services){
    return presentation_instance_create_copy(memory,object,source,1,services) && vehicle_presentation_copy_finish(memory,object,source,type);
}
const VehiclePresentationFamily *vehicle_presentation_family(uint8_t type){
    static const VehiclePresentationFamily families[]={
        {0x21,11,0x260,0x5adb98,0x5846d8,0x5addb0,0x5adc38,0x6058a0,0x6058d0},
        {0x22,2,0x264,0x5adb90,0x5848c8,0x5adda0,0x5adde0,0x5ab0c0,0x5ab0c8},
        {0x23,2,0x340,0x5adc68,0x5848c0,0x5ada18,0x5adb68,0x639af0,0x639ae8},
        {0x24,3,0x264,0x5adb80,0x5848d0,0x5adc00,0x5adb58,0x5ab0b0,0x5ab0e0},
        {0x25,1,0x264,0x5adb70,0x5848dc,0x5adad8,0x5adc70,0x5ab0d0,0x5ab0d8},
        {0x28,1,0x264,0x5add10,0x5848e0,0x5adcf0,0x5adce8,0x5ab0a8,0x5ab0d4}
    };unsigned i;for(i=0;i<sizeof(families)/sizeof(families[0]);i++)if(families[i].type==type)return &families[i];return NULL;
}
GameAddress vehicle_presentation_template(GameMemory *memory,uint16_t slot,const VehiclePresentationFactoryServices *services){
    GameAddress publication,object;VehiclePresentationDefinition definition={0};int loaded;
    const VehiclePresentationFamily *family=services?vehicle_presentation_family(services->type?services->type:0x23):NULL;
    if(!family || slot>=family->slots || !services->describe || !services->allocate || !services->release)return 0u;
    publication=family->templates+(uint32_t)slot*4u;object=game_memory_read_u32(memory,publication);
    if(object || memory->fault)return object;
    if(game_memory_read_u32(memory,0x624f8cu)==10u || memory->fault)return 0u;
    loaded=services->describe(services->context,slot,&definition);if(loaded<=0){if(loaded<0)memory->fault=1;return 0u;}
    object=services->allocate(services->context,family->bytes);
    if(!object){if(services->retirement.release_model)(void)services->retirement.release_model(services->retirement.context,definition.base.model);memory->fault=1;return 0u;}
    if(!vehicle_presentation_construct(memory,object,&definition,family->type)){
        if(presentation_instance_find(object))(void)presentation_instance_release(memory,object,&services->retirement,1,1);
        else if(services->retirement.release_model)(void)services->retirement.release_model(services->retirement.context,definition.base.model);
        (void)services->release(services->context,object);memory->fault=1;return 0u;
    }
    game_memory_write_u32(memory,publication,object);return memory->fault?0u:object;
}
int vehicle_presentation_fill(GameMemory *memory,uint16_t slot,int32_t count,const VehiclePresentationFactoryServices *services){
    GameAddress source=vehicle_presentation_template(memory,slot,services);int32_t index;
    const VehiclePresentationFamily *family=services?vehicle_presentation_family(services->type?services->type:0x23):NULL;
    if(!source || memory->fault)return 0;
    if(count>65536){memory->fault=1;return 0;}
    for(index=0;index<count;index++){
        GameAddress object=services->allocate(services->context,family->bytes);
        if(!object){memory->fault=1;return 0;}
        if(!vehicle_presentation_copy(memory,object,source,family->type,&services->construction)){
            if(presentation_instance_find(object))(void)presentation_instance_release(memory,object,&services->retirement,1,1);
            (void)services->release(services->context,object);memory->fault=1;return 0;
        }
        (void)game_list_insert_after(memory,object,source,4u,8u);if(memory->fault)return 0;
    }
    return 1;
}
GameAddress vehicle_presentation_take(GameMemory *memory,uint16_t slot,const VehiclePresentationFactoryServices *services){
    GameAddress source=vehicle_presentation_template(memory,slot,services),object;
    if(!source || memory->fault)return 0u;
    if(!game_memory_read_u32(memory,source+4u) && !vehicle_presentation_fill(memory,slot,1,services))return 0u;
    object=game_memory_read_u32(memory,source+4u);if(!object || memory->fault)return 0u;
    (void)game_list_detach_node(memory,object,8u,4u);return memory->fault?0u:object;
}
int vehicle_presentation_clear_events(GameMemory *memory,GameAddress descriptor,int destroying,const VehicleRetirementServices *services){
    GameAddress event=game_memory_read_u32(memory,descriptor+0x28u);unsigned visits=0u;
    if(destroying){game_memory_write_u32(memory,descriptor,0x577a38u);if(!event)return !memory->fault;}
    while(event && !memory->fault){
        if(++visits>65536u)return 0;
        game_memory_write_u32(memory,descriptor+0x28u,game_memory_read_u32(memory,event+4u));
        if(memory->fault || !services || !services->return_event || !services->return_event(services->context,event))return 0;
        event=game_memory_read_u32(memory,descriptor+0x28u);
    }
    game_memory_write_u32(memory,descriptor+0x2cu,0u);return !memory->fault;
}
uint32_t vehicle_presentation_release_relation(GameMemory *memory,GameAddress worker,GameAddress member){
    unsigned index;uint32_t count;
    for(index=0u;index<3u;index++)if(game_memory_read_u32(memory,worker+0x50u+index*4u)==member)break;
    /* The original falls through to its fourth slot when no match exists. */
    game_memory_write_u32(memory,worker+0x50u+index*4u,0u);
    count=game_memory_read_u32(memory,worker+0x4cu)+1u;game_memory_write_u32(memory,worker+0x4cu,count);return count;
}
static int vehicle_auxiliary_release(GameMemory *memory,GameAddress object,const VehicleRetirementServices *services){
    unsigned index;
    for(index=0u;index<2u;index++){
        GameAddress auxiliary=game_memory_read_u32(memory,object+0x21cu+index*4u);
        if(memory->fault)return 0;
        if(!auxiliary)continue;
        game_memory_write_u32(memory,auxiliary,0x572dc0u);
        if(memory->fault || !services || !services->release_storage || !services->release_storage(services->context,auxiliary))return 0;
        /* Successful releases cannot be retried against freed storage. */
        game_memory_write_u32(memory,object+0x21cu+index*4u,0u);
    }
    return !memory->fault;
}
int vehicle_presentation_auxiliary_reset(GameMemory *memory,GameAddress object,const VehicleRetirementServices *services){
    return services && vehicle_auxiliary_release(memory,object,services) && services->base_reset && services->base_reset(services->context,object);
}
int vehicle_presentation_base_reset(GameMemory *memory,GameAddress object,const VehicleRetirementServices *services){
    GameAddress token=game_memory_read_u32(memory,object+0x260u),catalog,group;unsigned visits=0u;int matched=0;
    if(token){
        catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+20u)&63u)*4u);
        group=game_memory_read_u32(memory,catalog+0x294u);
        while(group && !matched && !memory->fault){
            uint16_t word=game_memory_read_u16(memory,group+0x24u);int32_t count=(int32_t)word-((word&0x8000u)?65536:0),index;
            GameAddress entries=game_memory_read_u32(memory,group+0x18u);
            if(++visits>65536u)return 0;
            for(index=0;index<count && !memory->fault;index++){
                GameAddress worker=game_memory_read_u32(memory,entries+(uint32_t)index*4u);
                if(worker && !game_memory_read_u16(memory,worker+20u) &&
                   game_memory_read_u32(memory,game_memory_read_u32(memory,worker+0x48u)+20u)==token){
                    (void)vehicle_presentation_release_relation(memory,worker,object);matched=1;break;
                }
            }
            if(!matched)group=game_memory_read_u32(memory,group+0x64u);
        }
    }
    game_memory_write_u32(memory,object+0x260u,0u);
    return !memory->fault && vehicle_presentation_auxiliary_reset(memory,object,services);
}
int vehicle_presentation_reset(GameMemory *memory,GameAddress object,const VehicleRetirementServices *services){
    static const uint32_t fields[2]={0x2b8u,0x2c8u};unsigned index;GameAddress controller,catalog;
    if(!services)return 0;
    for(index=0u;index<2u;index++){
        GameAddress effect=game_memory_read_u32(memory,object+fields[index]);
        if(effect){game_memory_write_u32(memory,effect+0xb4u,3u);game_memory_write_u32(memory,object+fields[index],0u);}
    }
    if(memory->fault || !vehicle_presentation_clear_events(memory,object+0x264u,0,services))return 0;
    if(game_memory_read_u8(memory,object+0xd8u)&1u){
        catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+20u)&63u)*4u);
        if(memory->fault || !services->refresh_selection || !services->refresh_selection(services->context,catalog))return 0;
    }
    if(signed_count(game_memory_read_u32(memory,object+0x58u))<=0 &&
       (!services->notify_inactive || !services->notify_inactive(services->context,object)))return 0;
    controller=game_memory_read_u32(memory,object+0x294u);
    if(controller){
        if(memory->fault || !services->detach_controller || !services->detach_controller(services->context,controller,object))return 0;
        game_memory_write_u32(memory,object+0x294u,0u);
    }
    return !memory->fault && vehicle_presentation_base_reset(memory,object,services);
}
int vehicle_presentation_base_destroy(GameMemory *memory,GameAddress object,const VehicleRetirementServices *services){
    if(!services)return 0;
    game_memory_write_u32(memory,object,0x571288u);
    return vehicle_auxiliary_release(memory,object,services) && services->base_release && services->base_release(services->context,object);
}
int vehicle_presentation_destroy(GameMemory *memory,GameAddress object,int deleting,const VehicleRetirementServices *services){
    game_memory_write_u32(memory,object,0x5776d8u);game_memory_write_u32(memory,object+0x264u,0x5776b8u);
    if(!vehicle_presentation_clear_events(memory,object+0x264u,0,services) ||
       !vehicle_presentation_clear_events(memory,object+0x264u,1,services) ||
       !vehicle_presentation_base_destroy(memory,object,services))return 0;
    return !deleting || (services->release_storage && services->release_storage(services->context,object));
}
static float vehicle_sine(GameMemory *memory,uint16_t angle){return real_at(memory,0x6298a8u+((uint32_t)angle&2047u)*4u);}
static void vehicle_store_matrix(GameMemory *memory,GameAddress target,const float matrix[12]){
    unsigned index;for(index=0u;index<12u;index++)put_real(memory,target+index*4u,matrix[index]);
}
static void vehicle_load_matrix(GameMemory *memory,GameAddress source,float matrix[12]){
    unsigned index;for(index=0u;index<12u;index++)matrix[index]=real_at(memory,source+index*4u);
}
/*4e65a0/4e68f0: inclination of the composed yaw axis, using the original
 * lookup table and signed float-bit quadrant test. */
static uint16_t vehicle_inclination(GameMemory *m,const float parent[12],const float yaw[12]){
    float matrix[12],x,z,scale;long double ratio;uint32_t index,bits;uint16_t angle,table;
    game_matrix_compose_values(matrix,parent,yaw);x=matrix[6];z=matrix[5];scale=real_at(m,0x574cc8);
    if(fabsl(x)<fabsl(z)){
        ratio=(long double)x*scale/z+scale;memcpy(&bits,&z,4);angle=0;
    }else{
        ratio=(long double)z*scale/x+scale;memcpy(&bits,&x,4);angle=(int32_t)bits>0?0x200:0x600;
    }
    index=!isfinite(ratio) || ratio< -0x1p63L || ratio>=0x1p63L?0x80000000u:(uint32_t)(uint64_t)(int64_t)ratio;
    table=game_memory_read_u16(m,0x62e8b0+index*2u);
    if(fabsl(x)<fabsl(z))return (int32_t)bits>0?(uint16_t)(table&2047):(uint16_t)(table+0x400);
    return (uint16_t)(angle-table);
}
static int vehicle_world_frame(GameMemory *memory,GameAddress object,uint8_t type){
    uint32_t flags=game_memory_read_u32(memory,object+16u);uint16_t heading,pitch,roll;
    float sa,sb,sc,ca,cb,cc,matrix[12],anchor[3],pivot[3];unsigned axis;GameAddress commander;
    if(type==0x21 || !(flags&0x80000u)){
        heading=game_memory_read_u16(memory,object+0x24u);pitch=game_memory_read_u16(memory,object+0x26u);roll=game_memory_read_u16(memory,object+0x28u);
        sa=vehicle_sine(memory,heading);sb=vehicle_sine(memory,pitch);sc=vehicle_sine(memory,roll);
        ca=vehicle_sine(memory,(uint16_t)(heading+512u));cb=vehicle_sine(memory,(uint16_t)(pitch+512u));cc=vehicle_sine(memory,(uint16_t)(roll+512u));
        {
            /* Original world frame saves these four products as float before
               reuse. Keep the extended sums and this rounding boundary. */
            float sc_sa=(float)((long double)sc*sa),cc_ca=(float)((long double)cc*ca);
            float sa_cc=(float)((long double)sa*cc),ca_sc=(float)((long double)ca*sc);
            matrix[0]=(float)((long double)sc_sa*sb+cc_ca);matrix[1]=(float)((long double)sa_cc*sb-ca_sc);
            matrix[2]=(float)((long double)sa*cb);matrix[4]=(float)((long double)sc*cb);
            matrix[5]=(float)((long double)cc*cb);matrix[6]=-sb;
            matrix[8]=(float)((long double)ca_sc*sb-sa_cc);matrix[9]=(float)((long double)cc_ca*sb+sc_sa);
            matrix[10]=(float)((long double)cb*ca);
            if(type==0x21){
                /*4e65a0 saves only sc*sa; the remaining products stay extended. */
                matrix[0]=(float)(((long double)sc*sa)*sb+(long double)cc*ca);
                matrix[1]=(float)(((long double)sb*sa)*cc-(long double)sc*ca);
                matrix[8]=(float)(((long double)sc*sb)*ca-(long double)sa*cc);
                matrix[9]=(float)(((long double)sb*cc)*ca+sc_sa);
            }
        }
        for(axis=0u;axis<3u;axis++)matrix[axis*4u+3u]=real_at(memory,object+0x18u+axis*4u);
        if(type==0x22){memset(matrix,0,sizeof(matrix));matrix[0]=ca;matrix[2]=sa;matrix[5]=1;matrix[8]=-sa;matrix[10]=ca;
            for(axis=0;axis<3;axis++)matrix[axis*4+3]=real_at(memory,object+0x18+axis*4);
            game_memory_write_u32(memory,object+16,flags|0x80000);}
        vehicle_store_matrix(memory,object+0x80u,matrix);
        commander=type==0x23?game_memory_read_u32(memory,object+0x294u):0;
        if(commander && game_memory_read_u32(memory,commander+16u)==1u)game_memory_write_u32(memory,object+16u,flags|0x80000u);
    }
    heading=game_memory_read_u16(memory,object+0x14cu);sa=vehicle_sine(memory,heading);ca=vehicle_sine(memory,(uint16_t)(heading+512u));
    memset(matrix,0,sizeof(matrix));matrix[0]=ca;matrix[2]=sa;matrix[5]=1.0f;matrix[8]=-sa;matrix[10]=ca;
    for(axis=0u;axis<3u;axis++)matrix[axis*4u+3u]=real_at(memory,object+0x1c8u+axis*4u);
    vehicle_store_matrix(memory,object+0x180u,matrix);
    pitch=game_memory_read_u16(memory,object+0x14au);
    if(type==0x21){float parent[12];vehicle_load_matrix(memory,object+0x80,parent);pitch=(uint16_t)(pitch+vehicle_inclination(memory,parent,matrix));}
    sa=vehicle_sine(memory,pitch);ca=vehicle_sine(memory,(uint16_t)(pitch+512u));
    for(axis=0u;axis<3u;axis++){anchor[axis]=real_at(memory,object+0x1bcu+axis*4u);pivot[axis]=real_at(memory,object+0x1d4u+axis*4u);}
    memset(matrix,0,sizeof(matrix));matrix[0]=1.0f;matrix[3]=anchor[0];matrix[5]=ca;matrix[6]=-sa;matrix[9]=sa;matrix[10]=ca;
    matrix[7]=(float)(((long double)pivot[2]*sa-(long double)pivot[1]*ca)+anchor[1]+pivot[1]);
    matrix[11]=(float)((-(long double)pivot[1]*sa-(long double)pivot[2]*ca)+anchor[2]+pivot[2]);
    vehicle_store_matrix(memory,object+0x150u,matrix);return !memory->fault;
}
static void vehicle_transform_sample(GameMemory *memory,GameAddress output,const float matrix[12],GameAddress source,const unsigned orders[3][3]){
    float point[3],result[3];unsigned axis,row;
    for(axis=0u;axis<3u;axis++)point[axis]=real_at(memory,source+axis*4u);
    for(row=0u;row<3u;row++){
        const unsigned *order=orders[row];
        long double value=(long double)matrix[row*4u+order[0]]*point[order[0]]+(long double)matrix[row*4u+order[1]]*point[order[1]];
        result[row]=(float)(value+(long double)matrix[row*4u+order[2]]*point[order[2]]+matrix[row*4u+3u]);
    }
    for(axis=0u;axis<3u;axis++)put_real(memory,output+axis*4u,result[axis]);
}
static int32_t vehicle_short(GameMemory *memory,GameAddress address){uint32_t word=game_memory_read_u16(memory,address);return (int32_t)word-((word&0x8000u)?65536:0);}
static int vehicle_world_vertices(GameMemory *memory,GameAddress object,uint8_t type){
    static const unsigned orders[3][3][3]={{{1u,2u,0u},{1u,2u,0u},{1u,2u,0u}},
        {{2u,1u,0u},{2u,1u,0u},{2u,1u,0u}},{{0u,1u,2u},{0u,1u,2u},{0u,1u,2u}}};
    static const unsigned upper_orders[3][3]={{1u,2u,0u},{1u,2u,0u},{0u,1u,2u}};
    float world[12],yaw[12],pitch[12],middle[12],tip[12];unsigned group;int32_t begin,end,index;GameAddress records,output,source;
    vehicle_load_matrix(memory,object+0x80u,world);vehicle_load_matrix(memory,object+0x180u,yaw);vehicle_load_matrix(memory,object+0x150u,pitch);
    game_matrix_compose_values(middle,world,yaw);game_matrix_compose_values(tip,middle,pitch);
    records=game_memory_read_u32(memory,object+0x3cu);output=game_memory_read_u32(memory,object+0x40u);
    for(group=0u;group<3u;group++){
        const float *matrix=group==0u?tip:group==1u?middle:world;
        if(type==0x22 && group==2 && (game_memory_read_u32(memory,object+16)&0x800000))break;
        begin=group==0u?vehicle_short(memory,object+0x1e0u):group==1u?vehicle_short(memory,object+0x1e2u):0;
        end=vehicle_short(memory,object+(group==0u?0x44u:group==1u?0x1e0u:0x1e2u));
        for(index=begin;index<end && !memory->fault;index++)vehicle_transform_sample(memory,output+(uint32_t)index*12u,matrix,records+(uint32_t)index*28u+12u,orders[group]);
    }
    if(type==0x22){uint32_t flags=game_memory_read_u32(memory,object+16);
        if(flags&0x80000)game_memory_write_u32(memory,object+16,flags|0x800000);}
    if(type!=0x23)return !memory->fault;
    source=game_memory_read_u32(memory,object+0x50u);
    vehicle_transform_sample(memory,object+0x2bcu,world,source+0x2bcu,orders[2]);
    vehicle_transform_sample(memory,object+0x2ccu,middle,source+0x2ccu,upper_orders);return !memory->fault;
}
static int vehicle_equal_or_unordered(float left,float right){return left==right || isnan(left) || isnan(right);}
static int vehicle_render_frame(GameMemory *memory,GameAddress object,uint8_t type){
    uint32_t flags=game_memory_read_u32(memory,object+16u);GameAddress commander,target;uint16_t angles[3],angle;
    float matrix[12],position[3],anchor[3],pivot[3],s,c,recoil;unsigned axis;
    if(type==0x21 || !(flags&0x40000u)){
        for(axis=0u;axis<3u;axis++){angles[axis]=game_memory_read_u16(memory,object+0x78u-axis*2u);position[axis]=real_at(memory,object+0x64u+axis*4u);}
        gameplay_geometry_orientation_values(memory,angles,position,matrix);
        {
            float sa=vehicle_sine(memory,angles[0]),sb=vehicle_sine(memory,angles[1]),sc=vehicle_sine(memory,angles[2]);
            float ca=vehicle_sine(memory,(uint16_t)(angles[0]+512u)),cc=vehicle_sine(memory,(uint16_t)(angles[2]+512u));
            float sc_sb=(float)((long double)sc*sb);
            matrix[0]=(float)((long double)cc*ca+(long double)sc_sb*sa);
            if(type==0x21){
                float cc_ca=(float)((long double)cc*ca),sc_ca=(float)((long double)sc*ca);
                matrix[0]=(float)(cc_ca+((long double)sc*sb)*sa);
                matrix[1]=(float)(((long double)sa*sb)*cc-sc_ca);
                matrix[8]=(float)((long double)sc_ca*sb-(long double)sa*cc);
                matrix[9]=(float)((long double)cc_ca*sb+(long double)sc*sa);
            }
        }
        if(type==0x22){s=vehicle_sine(memory,angles[0]);c=vehicle_sine(memory,(uint16_t)(angles[0]+512));
            memset(matrix,0,sizeof(matrix));matrix[0]=c;matrix[2]=s;matrix[5]=1;matrix[8]=-s;matrix[10]=c;
            for(axis=0;axis<3;axis++)matrix[axis*4+3]=position[axis];
            game_memory_write_u32(memory,object+16,flags|0x40000);}
        target=game_memory_read_u32(memory,object+0x54u)+4u;vehicle_store_matrix(memory,target,matrix);
        commander=type==0x23?game_memory_read_u32(memory,object+0x294u):0;
        if(commander && game_memory_read_u32(memory,commander+16u)==1u &&
           vehicle_equal_or_unordered(real_at(memory,object+0x18u),position[0]) &&
           vehicle_equal_or_unordered(real_at(memory,object+0x1cu),position[1]) &&
           vehicle_equal_or_unordered(real_at(memory,object+0x20u),position[2]) &&
           game_memory_read_u16(memory,object+0x24u)==angles[0])game_memory_write_u32(memory,object+16u,flags|0x40000u);
    }
    angle=game_memory_read_u16(memory,object+0x1bau);s=vehicle_sine(memory,angle);c=vehicle_sine(memory,(uint16_t)(angle+512u));
    memset(matrix,0,sizeof(matrix));matrix[0]=c;matrix[2]=s;matrix[5]=1.0f;matrix[8]=-s;matrix[10]=c;
    for(axis=0u;axis<3u;axis++)matrix[axis*4u+3u]=real_at(memory,object+0x1c8u+axis*4u);
    vehicle_store_matrix(memory,game_memory_read_u32(memory,object+0x1b4u)+4u,matrix);
    angle=game_memory_read_u16(memory,object+0x1b8u);
    if(type==0x21){float parent[12];vehicle_load_matrix(memory,game_memory_read_u32(memory,object+0x54)+4,parent);angle=(uint16_t)(angle+vehicle_inclination(memory,parent,matrix));}
    s=vehicle_sine(memory,angle);c=vehicle_sine(memory,(uint16_t)(angle+512u));
    for(axis=0u;axis<3u;axis++){anchor[axis]=real_at(memory,object+0x1bcu+axis*4u);pivot[axis]=real_at(memory,object+0x1d4u+axis*4u);}
    recoil=real_at(memory,object+0x1e8u);
    memset(matrix,0,sizeof(matrix));matrix[0]=1.0f;matrix[3]=anchor[0];matrix[5]=c;matrix[6]=-s;matrix[9]=s;matrix[10]=c;
    matrix[7]=(float)((((long double)pivot[2]+recoil)*s-(long double)pivot[1]*c)+anchor[1]+pivot[1]);
    matrix[11]=(float)((-(long double)pivot[1]*s-((long double)pivot[2]+recoil)*c)+anchor[2]+pivot[2]);
    vehicle_store_matrix(memory,game_memory_read_u32(memory,object+0x1b0u)+4u,matrix);
    /* The original final timer loop only fills discarded local angles; all
       four jump-table entries return to that loop, with no publication. */
    return !memory->fault;
}
static int vehicle_render_vertices(GameMemory *memory,GameAddress object,uint8_t type){
    static const unsigned moving_orders[3][3]={{1u,2u,0u},{1u,2u,0u},{1u,2u,0u}};
    static const unsigned base_orders[3][3]={{2u,1u,0u},{1u,2u,0u},{1u,2u,0u}};
    float rendered[12],yaw[12],pitch[12],middle[12],tip[12],world[12];GameAddress records,output;unsigned group;int32_t begin,end,index;uint32_t flags;
    vehicle_load_matrix(memory,game_memory_read_u32(memory,object+0x54u)+4u,rendered);
    vehicle_load_matrix(memory,game_memory_read_u32(memory,object+0x1b4u)+4u,yaw);
    vehicle_load_matrix(memory,game_memory_read_u32(memory,object+0x1b0u)+4u,pitch);
    game_matrix_compose_values(middle,rendered,yaw);game_matrix_compose_values(tip,middle,pitch);
    vehicle_load_matrix(memory,object+0x80u,world);
    records=game_memory_read_u32(memory,object+0x3cu);output=game_memory_read_u32(memory,object+0x70u);
    for(group=0u;group<3u;group++){
        const float *matrix=group==0u?tip:group==1u?middle:world;const unsigned (*orders)[3]=group==2u?base_orders:moving_orders;
        if(type!=0x21 && group==2u && (game_memory_read_u32(memory,object+16u)&0x400000u))break;
        begin=group==0u?vehicle_short(memory,object+0x1e0u):group==1u?vehicle_short(memory,object+0x1e2u):0;
        end=vehicle_short(memory,object+(group==0u?0x44u:group==1u?0x1e0u:0x1e2u));
        for(index=begin;index<end && !memory->fault;index++)vehicle_transform_sample(memory,output+(uint32_t)index*12u,matrix,records+(uint32_t)index*28u+12u,orders);
    }
    flags=game_memory_read_u32(memory,object+16u);if(type!=0x21 && (flags&0x40000u))game_memory_write_u32(memory,object+16u,flags|0x400000u);
    return !memory->fault;
}
int vehicle_presentation_world_frame(GameMemory *m,GameAddress o){return vehicle_world_frame(m,o,0x23);}
int vehicle_presentation_world_vertices(GameMemory *m,GameAddress o){return vehicle_world_vertices(m,o,0x23);}
int vehicle_presentation_render_frame(GameMemory *m,GameAddress o){return vehicle_render_frame(m,o,0x23);}
int vehicle_presentation_render_vertices(GameMemory *m,GameAddress o){return vehicle_render_vertices(m,o,0x23);}
int vehicle_presentation_group_geometry(GameMemory *m,GameAddress o,unsigned operation){
    uint8_t type=game_memory_read_u8(m,o+12);
    if(!vehicle_presentation_family(type) || m->fault)return 0;
    if(type==0x24 || type==0x25 || type==0x28)type=0x22;
    return operation==0?vehicle_world_frame(m,o,type):operation==1?vehicle_render_frame(m,o,type):
        operation==2?vehicle_world_vertices(m,o,type):operation==3?vehicle_render_vertices(m,o,type):0;
}
static int32_t input_signed_word(uint32_t bits){int32_t value;memcpy(&value,&bits,4);return value;}
static int16_t input_signed_short(uint16_t bits){int16_t value;memcpy(&value,&bits,2);return value;}
static uint32_t input_truncate(long double value){return (uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);}
/*4e7280: the shared grouped controller catches up to simulation state. The
 * last articulation pitch uses a signed linear delta; the other axes wrap. */
int vehicle_presentation_local_pose(GameMemory *m,GameAddress o,int interpolate){
    static const unsigned targets[5]={0x24,0x26,0x28,0x14c,0x14a};
    static const unsigned locals[5]={0x78,0x76,0x74,0x1ba,0x1b8};
    uint32_t now;float factor=0,limit;long double duration;unsigned i;int snap=!interpolate;
    if(!m || m->fault)return 0;
    if(interpolate){
        now=game_memory_read_u32(m,0x5acbe8);
        snap=signed_count(now-game_memory_read_u32(m,o+0x7c))>10;
        game_memory_write_u32(m,o+0x7c,now);
        if(!snap){
            duration=(long double)signed_count(game_memory_read_u32(m,0x5acc80)-now)+real_at(m,0x5acc38);
            limit=real_at(m,0x574cc4);snap=!(duration>limit);
            if(!snap)factor=(float)((long double)limit/(float)duration);
        }
    }
    for(i=0;i<3;i++){
        if(snap)game_memory_write_u32(m,o+0x64+i*4,game_memory_read_u32(m,o+0x18+i*4));
        else{float current=real_at(m,o+0x64+i*4);
            put_real(m,o+0x64+i*4,(float)(((long double)real_at(m,o+0x18+i*4)-current)*factor+current));}
    }
    for(i=0;i<5;i++){
        uint16_t target=game_memory_read_u16(m,o+targets[i]),current=game_memory_read_u16(m,o+locals[i]),value;
        if(snap)value=target;
        else{int32_t delta=i==4?(int32_t)input_signed_short(target)-input_signed_short(current):(int32_t)(((uint32_t)target-current)&2047u);
            if(i!=4 && delta>1024)delta-=2048;
            value=(uint16_t)(current+(uint16_t)input_truncate((long double)delta*factor));
            value&=2047;
            if((i==1 || i==2 || i==4) && value>1024)value=(uint16_t)(value-2048);
        }
        game_memory_write_u16(m,o+locals[i],value);
    }
    return !m->fault;
}
int vehicle_presentation_input_tick(GameMemory *m,GameAddress object,const VehicleInputServices *s){
    GameAddress queue=object+0x264,table,yaw_model,pitch_model;uint32_t saved[2][12],flags,previous,tick;
    uint16_t old_pitch,old_yaw,pitch,yaw,step;int16_t dp,dy;int playing,hit=0;float position[3],contact[3]={0};unsigned i,j;
    if(!m || m->fault || !s)return 0;
    if(input_signed_word(game_memory_read_u32(m,object+0x58))<=0)return !m->fault;
    if(game_memory_read_u16(m,object+0x24)!=game_memory_read_u16(m,object+0x78) ||
       game_memory_read_u16(m,object+0x28)!=game_memory_read_u16(m,object+0x74) ||
       game_memory_read_u16(m,object+0x26)!=game_memory_read_u16(m,object+0x76) ||
       !vehicle_equal_or_unordered(real_at(m,object+0x64),real_at(m,object+0x18)) ||
       !vehicle_equal_or_unordered(real_at(m,object+0x1c),real_at(m,object+0x68)) ||
       !vehicle_equal_or_unordered(real_at(m,object+0x6c),real_at(m,object+0x20))){
        game_memory_write_u16(m,object+0x78,game_memory_read_u16(m,object+0x24));
        game_memory_write_u16(m,object+0x74,game_memory_read_u16(m,object+0x28));
        game_memory_write_u16(m,object+0x76,game_memory_read_u16(m,object+0x26));
        for(i=0;i<3;i++)game_memory_write_u32(m,object+0x64+i*4,game_memory_read_u32(m,object+0x18+i*4));
    }
    old_pitch=game_memory_read_u16(m,object+0x1b8);old_yaw=game_memory_read_u16(m,object+0x1ba);
    yaw_model=game_memory_read_u32(m,object+0x1b4);pitch_model=game_memory_read_u32(m,object+0x1b0);
    for(i=0;i<12;i++){saved[0][i]=game_memory_read_u32(m,yaw_model+4+i*4);saved[1][i]=game_memory_read_u32(m,pitch_model+4+i*4);}
    flags=game_memory_read_u32(m,queue+4);
    if(flags&0x20000u){
        if(flags==0x20000u && vehicle_equal_or_unordered((float)input_signed_short(game_memory_read_u16(m,queue+14)),real_at(m,0x5778e0)) &&
           vehicle_equal_or_unordered((float)input_signed_short(game_memory_read_u16(m,queue+16)),real_at(m,0x5778e0))){
            int32_t value=input_signed_short(old_pitch);
            game_memory_write_u16(m,object+0x1ba,(uint16_t)((old_yaw+14u)&0x7ffu));
            if(value<=85)value=value+12<85?value+12:85;
            else value=value-12>85?value-12:85;
            game_memory_write_u16(m,object+0x1b8,(uint16_t)value);
        }else game_memory_write_u32(m,queue+4,flags&~0x20000u);
    }
    pitch=game_memory_read_u16(m,queue+14);yaw=game_memory_read_u16(m,queue+16);
    flags=game_memory_read_u32(m,queue+4);previous=game_memory_read_u32(m,queue+8);
    if((flags&0x40u) && !(previous&0x80u)){step=(uint16_t)input_truncate((long double)real_at(m,queue+32)*real_at(m,0x5778e4));pitch=(uint16_t)(pitch-step);}
    if((flags&0x80u) && !(previous&0x40u)){step=(uint16_t)input_truncate((long double)real_at(m,queue+32)*real_at(m,0x5778e4));pitch=(uint16_t)(pitch+step);}
    if((flags&0x200u) && !(previous&0x100u)){step=(uint16_t)input_truncate((long double)real_at(m,queue+36)*real_at(m,0x5778e8));yaw=(uint16_t)(yaw+step);}
    if((flags&0x100u) && !(previous&0x200u)){step=(uint16_t)input_truncate((long double)real_at(m,queue+36)*real_at(m,0x5778e8));yaw=(uint16_t)(yaw-step);}
    dp=input_signed_short(pitch);dy=input_signed_short(yaw);
    if(m->fault)return 0;
    if(!dp && !dy){
        if(game_memory_read_u32(m,0x5ac6ac)-game_memory_read_u32(m,0x5a0480)>3u &&
           (!s->stop || !s->stop(s->context,0x81,0)))return 0;
    }else{
        int32_t angle=input_signed_short((uint16_t)(game_memory_read_u16(m,object+0x1b8)+pitch));
        if(angle< -398)angle=-398;else if(angle>199)angle=199;
        game_memory_write_u16(m,object+0x1b8,(uint16_t)angle);
        game_memory_write_u16(m,object+0x1ba,(uint16_t)((game_memory_read_u16(m,object+0x1ba)+yaw)&0x7ffu));
        if(!s->playing || !s->playing(s->context,0x81,0,&playing))return 0;
        if(!playing){
            if(!s->play || !s->play(s->context,0x81,1.0f,dy>0?1.0f:0.85f,1))return 0;
            tick=game_memory_read_u32(m,0x5ac6ac)-game_memory_read_u32(m,0x5a0480);
            if(tick>9u && (dy>1 || dy< -1) && !s->play(s->context,0x80,1.0f,0.0f,0))return 0;
        }else{
            int32_t ap=dp<0?-(int32_t)dp:dp,ay=dy<0?-(int32_t)dy:dy;
            previous=game_memory_read_u32(m,0x5a0484);
            if((dy>0 && !previous && ap<dy) || (dy<=0 && previous && ay>ap)){
                if(!s->volume || !s->volume(s->context,0x81,0,dy>0?1.0f:0.85f) ||
                   !s->play || !s->play(s->context,0x80,1.0f,0.0f,0))return 0;
            }
        }
        game_memory_write_u32(m,0x5a0484,dy>0?1:0);game_memory_write_u32(m,0x5a0480,game_memory_read_u32(m,0x5ac6ac));
    }
    table=game_memory_read_u32(m,object);
    if(m->fault || !s->refresh || !s->refresh(s->context,object,game_memory_read_u32(m,table+0x90)) ||
       !s->refresh(s->context,object,game_memory_read_u32(m,table+0x94)))return 0;
    for(i=0;i<3;i++)position[i]=real_at(m,object+0x18+i*4);
    if(m->fault || !s->overlap || !s->overlap(s->context,object,game_memory_read_u32(m,table+0x80),position,contact,&hit))return 0;
    if(hit){
        game_memory_write_u16(m,object+0x1ba,old_yaw);game_memory_write_u16(m,object+0x1b8,old_pitch);
        for(j=0;j<2;j++){
            GameAddress model=game_memory_read_u32(m,object+(j?0x1b0:0x1b4));
            for(i=0;i<12;i++)game_memory_write_u32(m,model+4+i*4,saved[j][i]);
        }
        if(m->fault || !s->refresh(s->context,object,game_memory_read_u32(m,table+0x94)))return 0;
    }
    return !m->fault;
}
static int vehicle_reset_actor(GameMemory *memory,GameAddress object,GameAddress methods,const VehicleActivationServices *services){
    GameAddress target=game_memory_read_u32(memory,methods+0x1f4u);
    if(memory->fault)return 0;
    if(target==0x4e6340u){gameplay_state_reset_actor(memory,object);return !memory->fault;}
    return services && services->reset_foreign && services->reset_foreign(services->context,object,target);
}
int vehicle_presentation_build_auxiliary(GameMemory *memory,GameAddress object,GameAddress catalog,const VehicleActivationServices *services){
    uint32_t selector;GameAddress record,auxiliary;GameSpatialAttachmentInit init={0};unsigned axis;
    if(!services || !services->catalog_value || !services->allocate || !services->release ||
       !services->catalog_value(services->context,catalog,0x23u,3u,0u,&selector))return 0;
    if(selector>=2u){memory->fault=1;return 0;}
    auxiliary=services->allocate(services->context,0xbcu);
    if(auxiliary){
        record=game_memory_read_u32(memory,0x5adb68u+selector*4u);
        init.parent=object;init.motion=game_memory_read_u32(memory,catalog+0x1dcu);init.heading=0x12u;
        init.orient_to_motion=1u;init.mode=1u;init.resource=6u;init.slot=4u;init.kind=1u;
        for(axis=0u;axis<3u;axis++){init.position[axis]=real_at(memory,record+axis*4u);init.secondary_position[axis]=real_at(memory,record+12u+axis*4u);}
        if(memory->fault || !game_spatial_attachment_construct(memory,auxiliary,&init)){
            (void)services->release(services->context,auxiliary);return 0;
        }
    }
    {
        GameAddress previous=game_memory_read_u32(memory,object+0x21cu);
        if(previous){
            game_memory_write_u32(memory,previous,0x572dc0u);
            if(memory->fault || !services->release(services->context,previous)){
                if(auxiliary)(void)services->release(services->context,auxiliary);
                return 0;
            }
        }
    }
    /* Original assumes a fresh auxiliary slot. Repeated native placement
       reclaims its prior owned record instead of losing the allocation. */
    /* Original publishes zero when allocation fails; no partially built record. */
    game_memory_write_u32(memory,object+0x21cu,auxiliary);return !memory->fault;
}
int vehicle_presentation_mode_place(GameMemory *memory,GameAddress object,uint32_t identity,GameAddress point,const uint16_t angles[3],const VehicleActivationServices *services){
    GameAddress methods=game_memory_read_u32(memory,object),catalog,target,player;uint32_t value,tick;uint8_t type;
    game_memory_write_u32(memory,object+0x144u,UINT32_MAX);
    if(memory->fault || !services || !services->place || !vehicle_reset_actor(memory,object,methods,services))return 0;
    catalog=game_memory_read_u32(memory,0x5f0210u+(identity&63u)*4u);type=game_memory_read_u8(memory,object+12u);
    if(type==0x21u){
        if(!services->component_enabled || !services->component_enabled(services->context,catalog,0x21u,&value))return 0;
        if(value)game_memory_write_u8(memory,object+0xd8u,(uint8_t)(game_memory_read_u8(memory,object+0xd8u)|4u));
    }
    if(memory->fault || !services->place(services->context,object,identity,point,angles))return 0;
    game_memory_write_u16(memory,object+0x224u,game_memory_read_u16(memory,object+0x24u));
    target=game_memory_read_u32(memory,methods+0x1f8u);
    if(target==0x5547d0u){if(!vehicle_presentation_build_auxiliary(memory,object,catalog,services))return 0;}
    else if(!services->auxiliary_foreign || !services->auxiliary_foreign(services->context,object,catalog,target))return 0;
    player=game_memory_read_u32(memory,0x5f0208u);
    if((game_memory_read_u32(memory,object+20u)&63u)==game_memory_read_u16(memory,player+8u)){
        tick=game_memory_read_u32(memory,0x5ac6acu);
        if(signed_count(tick-game_memory_read_u32(memory,player+0x58cu))>1)game_memory_write_u32(memory,player+0x58cu,game_memory_read_u32(memory,0x5ac6acu));
    }
    type=game_memory_read_u8(memory,object+12u);
    if((type>=0x21u && type<=0x25u) || type==0x28u){
        uint16_t first=type==0x28u?0u:type==0x23u?6u:68u,second=type==0x28u?0u:type==0x23u?12u:68u;
        game_memory_write_u16(memory,object+0x214u,first);game_memory_write_u16(memory,object+0x216u,second);
        game_memory_write_u16(memory,object+0x218u,type==0x28u?0u:34u);game_memory_write_u16(memory,object+0x21au,type==0x28u?0u:12u);
    }
    return !memory->fault;
}
int vehicle_presentation_gameplay_place(GameMemory *memory,GameAddress object,uint32_t identity,GameAddress point,const uint16_t angles[3],const VehicleActivationServices *services){
    uint8_t type=gameplay_state_prepare_object_action(memory,object);
    if(memory->fault || !services)return 0;
    if(type!=0x23u && (!services->prepare || !services->prepare(services->context,object,point,angles[0])))return 0;
    return vehicle_presentation_mode_place(memory,object,identity,point,angles,services);
}
int vehicle_presentation_activate(GameMemory *memory,GameAddress object,uint32_t identity,GameAddress point,const uint16_t angles[3],GameAddress commander,const VehicleActivationServices *services){
    static const unsigned words[3]={0x274u,0x272u,0x2acu},fields[5]={0x268u,0x26cu,0x2c8u,0x2b8u,0x29cu};unsigned index;
    if(game_memory_read_u32(memory,0x624f8cu)==10u){game_memory_write_u32(memory,object+0x2b4u,0u);game_memory_write_u32(memory,object+0x2b0u,game_memory_read_u32(memory,0x5ac6acu));}
    else {game_memory_write_u32(memory,object+0x2b4u,30u);game_memory_write_u32(memory,object+0x2b0u,0u);}
    if(memory->fault || !vehicle_reset_actor(memory,object,game_memory_read_u32(memory,object),services))return 0;
    game_memory_write_u32(memory,object+0x298u,0u);
    if(!vehicle_presentation_gameplay_place(memory,object,identity,point,angles,services))return 0;
    game_memory_write_u16(memory,object+0x224u,game_memory_read_u16(memory,object+0x24u));game_memory_write_u32(memory,object+0x294u,commander);
    for(index=0u;index<3u;index++)game_memory_write_u16(memory,object+words[index],0u);
    for(index=0u;index<5u;index++)game_memory_write_u32(memory,object+fields[index],0u);
    for(index=0u;index<3u;index++)game_memory_write_u32(memory,object+0x2a0u+index*4u,game_memory_read_u32(memory,object+0x18u+index*4u));
    game_memory_write_u32(memory,object+0x33cu,1u);return !memory->fault;
}
