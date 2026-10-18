#include "presentation_lifecycle.h"
#include <stddef.h>
#include <string.h>
#include <math.h>
typedef struct LifecycleField { uint16_t offset,member;uint8_t runtime; } LifecycleField;
#define MEMBER(at,field,run) [at/4u]={at,(uint16_t)offsetof(PresentationLifecycle,field),run}
static const LifecycleField fields[0x13cu/4u]={
    MEMBER(4u,membership.previous,0),MEMBER(8u,membership.next,0),
    MEMBER(0xcu,membership.selector,0),MEMBER(0x48u,membership.spatial_previous,0),
    MEMBER(0x4cu,membership.spatial_next,0),MEMBER(0x50u,membership.source,0),
    MEMBER(0xb0u,runtime.queued_handle,0),MEMBER(0xb4u,runtime.pending[0],1),
    MEMBER(0xb8u,runtime.pending[1],1),MEMBER(0xbcu,runtime.counters[0],1),
    MEMBER(0xc4u,runtime.counters[1],1),MEMBER(0xc8u,runtime.counters[2],1),
    MEMBER(0xd0u,runtime.counters[3],1),MEMBER(0xd4u,runtime.random_phase,1),
    MEMBER(0xd8u,runtime.status,1),MEMBER(0xdcu,runtime.packet_chain,1),
    MEMBER(0xe0u,runtime.mode,1),MEMBER(0xf0u,runtime.counters[4],1),
    MEMBER(0xe4u,runtime.command_position[0],1),MEMBER(0xe8u,runtime.command_position[1],1),
    MEMBER(0xecu,runtime.command_position[2],1),MEMBER(0xf4u,runtime.command_identity,1),
    MEMBER(0xccu,runtime.secondary_identity,1),
    MEMBER(0x104u,runtime.activation,1),MEMBER(0x108u,runtime.peers[0],1),
    MEMBER(0x10cu,runtime.peers[1],1),MEMBER(0x110u,runtime.properties[0],1),
    MEMBER(0x114u,runtime.properties[1],1),MEMBER(0x118u,runtime.properties[2],1),
    MEMBER(0x11cu,runtime.properties[3],1),MEMBER(0x120u,runtime.properties[4],1),
    MEMBER(0x124u,runtime.fractions[0],1),MEMBER(0x128u,runtime.fractions[1],1),
    MEMBER(0x12cu,runtime.counters[5],1),MEMBER(0x130u,runtime.counters[6],1),
    MEMBER(0x134u,runtime.counters[7],1),MEMBER(0x138u,runtime.counters[8],1)};
#undef MEMBER
static uint8_t *field_at(PresentationLifecycle *state,uint32_t offset,uint32_t *begin){
    const LifecycleField *field;
    if(offset>=0x13cu)return NULL;
    field=&fields[offset/4u];
    if(!field->offset || (field->runtime && !state->has_runtime))return NULL;
    *begin=field->offset;return (uint8_t *)state+field->member;
}
static int transfer(PresentationLifecycle *state,uint32_t offset,void *bytes,size_t size,int writing){
    uint32_t cursor=offset,begin;size_t remaining=size;uint8_t *field,*data=bytes;
    if(!state || !bytes || offset>0x13cu || size>0x13cu-offset)return 0;
    while(remaining){
        size_t step;field=field_at(state,cursor,&begin);if(!field)return 0;
        step=4u-(cursor-begin);if(step>remaining)step=remaining;
        cursor+=(uint32_t)step;remaining-=step;
    }
    while(size){
        size_t step;field=field_at(state,offset,&begin);step=4u-(offset-begin);
        if(step>size)step=size;
        if(writing)memcpy(field+offset-begin,data,step);
        else memcpy(data,field+offset-begin,step);
        offset+=(uint32_t)step;data+=step;size-=step;
    }
    return 1;
}
int presentation_lifecycle_projection_read(PresentationLifecycle *s,uint32_t o,void *b,size_t n){return transfer(s,o,b,n,0);}
int presentation_lifecycle_projection_write(PresentationLifecycle *s,uint32_t o,const void *b,size_t n){return transfer(s,o,(void *)b,n,1);}
int presentation_lifecycle_import(GameMemory *memory,GameAddress object,PresentationLifecycle *state,int runtime){
    PresentationLifecycle imported;size_t i;
    memset(&imported,0,sizeof(imported));imported.has_runtime=runtime!=0;
    for(i=0u;i<sizeof(fields)/sizeof(fields[0]) && !memory->fault;i++){
        const LifecycleField *field=&fields[i];uint32_t value;
        if(!field->offset)continue;
        if(field->runtime && !runtime)continue;
        value=game_memory_read_u32(memory,object+field->offset);
        memcpy((uint8_t *)&imported+field->member,&value,4u);
    }
    if(memory->fault)return 0;
    *state=imported;return 1;
}
void presentation_lifecycle_initialize_runtime(PresentationLifecycle *state,uint32_t phase){
    PresentationRuntime *runtime=&state->runtime;
    memset(runtime->counters,0,sizeof(runtime->counters));
    runtime->peers[0]=runtime->peers[1]=0u;
    runtime->mode=5u;runtime->random_phase=phase;runtime->activation=0u;
    state->resetting=state->reset_phase=state->detached_peers=0u;
}
/* Float inputs multiply exactly in double precision. Truncate as the original
 * signed 64-bit conversion, then negate its low word with unsigned wrapping. */
uint32_t presentation_lifecycle_phase(float sample,float scale){
    double product=(double)sample*(double)scale;
    int64_t value;
    if(!isfinite(product) || product < -0x1p63 || product >= 0x1p63)value=INT64_MIN;
    else value=(int64_t)product;
    return 0u-(uint32_t)(uint64_t)value;
}
static int publish_initial_runtime(GameMemory *memory,GameAddress object,PresentationLifecycle *state){
    static const uint16_t offsets[]={0xe0u,0xf0u,0xc8u,0x108u,0x10cu,0x138u,
        0x12cu,0xbcu,0xc4u,0xd0u,0x130u,0x134u};
    unsigned i;
    for(i=0u;i<sizeof(offsets)/sizeof(offsets[0]) && !memory->fault;i++){
        uint32_t begin,value;uint8_t *field=field_at(state,offsets[i],&begin);
        memcpy(&value,field,4u);game_memory_write_u32(memory,object+offsets[i],value);
    }
    return !memory->fault;
}
int presentation_lifecycle_initialize(GameMemory *memory,GameAddress object,
    PresentationLifecycle *state,const PresentationInitializeServices *services){
    PresentationRuntime *runtime;
    if(!memory || memory->fault || !state || !state->has_runtime || state->resetting || !services)return 0;
    runtime=&state->runtime;
    if(state->initialize_phase==0u){
        if((uint8_t)state->membership.selector!=0x20u){
            GameAddress packet;
            if(!services->allocate_packet || !services->allocate_packet(services->context,&packet))return 0;
            runtime->packet_chain=packet;
        }
        state->initialize_phase=1u;
    }
    if(state->initialize_phase==1u){
        if((uint8_t)state->membership.selector!=0x20u){
            game_memory_write_u32(memory,object+0xdcu,runtime->packet_chain);
            if(memory->fault || !services->prepare_packet ||
               !services->prepare_packet(services->context,runtime->packet_chain))return 0;
        }
        presentation_lifecycle_initialize_runtime(state,runtime->random_phase);
        state->initialize_phase=2u;
    }
    if(state->initialize_phase==2u){
        uint32_t phase;
        if(!publish_initial_runtime(memory,object,state))return 0;
        if(!services->next_phase || !services->next_phase(services->context,&phase))return 0;
        runtime->random_phase=phase;runtime->activation=0u;
        state->initialize_phase=3u;
    }
    if(state->initialize_phase==3u){
        game_memory_write_u32(memory,object+0xd4u,runtime->random_phase);
        game_memory_write_u32(memory,object+0x104u,0u);
        if(memory->fault || !services->refresh_properties ||
           !services->refresh_properties(services->context,object))return 0;
        state->initialize_phase=4u;
    }
    if(state->initialize_phase==4u){
        runtime->pending[0]=runtime->pending[1]=0u;
        game_memory_write_u32(memory,object+0xb8u,0u);
        game_memory_write_u32(memory,object+0xb4u,0u);
        if(memory->fault)return 0;
        state->initialize_phase=0u;
    }
    return 1;
}
int presentation_lifecycle_refresh_properties(GameMemory *memory,GameAddress object,
    PresentationLifecycle *state,void *context,PresentationPropertyGetter getter){
    if(!memory || memory->fault || !state || !state->has_runtime || !getter)return 0;
    if(!state->refreshing){state->property_index=0u;state->refreshing=1u;}
    while(state->property_index<7u){
        unsigned index=state->property_index;uint32_t bits;
        if(!getter(context,index,&bits))return 0;
        if(index<5u)state->runtime.properties[index]=bits;
        else memcpy(state->runtime.fractions+index-5u,&bits,4u);
        game_memory_write_u32(memory,object+0x110u+index*4u,bits);
        if(memory->fault)return 0;
        ++state->property_index;
    }
    state->refreshing=state->property_index=0u;
    return 1;
}
static int unlink_pair(GameMemory *memory,GameAddress object,GameAddress *previous,
    GameAddress *next,uint32_t previous_offset,uint32_t next_offset,int clear_previous){
    if(*next)game_memory_write_u32(memory,*next+previous_offset,*previous);
    if(*previous)game_memory_write_u32(memory,*previous+next_offset,*next);
    if(memory->fault)return 0;
    *next=0u;if(clear_previous)*previous=0u;
    game_memory_write_u32(memory,object+next_offset,*next);
    if(clear_previous)game_memory_write_u32(memory,object+previous_offset,*previous);
    return !memory->fault;
}
int presentation_lifecycle_reset(GameMemory *memory,GameAddress object,PresentationPose *pose,
    PresentationLifecycle *state,unsigned kind,const PresentationResetServices *services){
    PresentationRuntime *runtime=&state->runtime;PresentationMembership *links=&state->membership;
    if(!memory || memory->fault || !services || kind>2u || (kind==2u && !state->has_runtime))return 0;
    if(state->resetting && state->reset_kind!=kind)return 0;
    if(!state->resetting)state->detached_peers=0u;
    state->resetting=1u;state->reset_kind=(uint8_t)kind;
    if(state->reset_phase==0u){
        if(kind==2u){
            if(services->deactivate){if(!services->deactivate(services->context,object))return 0;}
            else {
                unsigned index;
                for(index=2u;index;--index){
                    unsigned peer=index-1u;uint8_t bit=(uint8_t)(1u<<peer);
                    if((state->detached_peers&bit)!=0u)continue;
                    if(runtime->peers[peer] && (!services->detach_peer ||
                       !services->detach_peer(services->context,runtime->peers[peer],object)))return 0;
                    state->detached_peers|=bit;
                }
            }
        }
        state->reset_phase=1u;
    }
    if(state->reset_phase==1u){
        if(kind==2u){
            if(runtime->packet_chain && (!services->detach_chain ||
               !services->detach_chain(services->context,runtime->packet_chain)))return 0;
            runtime->packet_chain=0u;runtime->status&=0xffffff00u;
            game_memory_write_u32(memory,object+0xdcu,0u);game_memory_write_u8(memory,object+0xd8u,0u);
        }
        state->reset_phase=2u;
    }
    if(state->reset_phase==2u){
        if(kind && runtime->queued_handle){
            if(!services->release_queue || !services->release_queue(services->context,runtime->queued_handle))return 0;
            runtime->queued_handle=0u;game_memory_write_u32(memory,object+0xb0u,0u);
        }
        state->reset_phase=3u;
    }
    if(state->reset_phase==3u){
        pose->properties[0]=0u;pose->flags&=0xff11ffffu;
        if(!unlink_pair(memory,object,&links->previous,&links->next,4u,8u,1) ||
           !unlink_pair(memory,object,&links->spatial_previous,&links->spatial_next,0x48u,0x4cu,0))return 0;
        if(!presentation_pose_export(memory,object,pose))return 0;
        state->reset_phase=4u;
    }
    if(state->reset_phase==4u){
        if(!services->notify_catalog || !services->notify_catalog(services->context,pose->identity,(uint8_t)links->selector))return 0;
        state->reset_phase=5u;
    }
    if(memory->fault)return 0;
    if(state->reset_phase==5u){
        GameAddress first=game_memory_read_u32(memory,links->source+4u);
        if(memory->fault)return 0;
        links->previous=first;links->next=links->source;
        memset(pose->position,0,sizeof(pose->position));memset(pose->angles,0,sizeof(pose->angles));pose->identity=0u;
        state->reset_phase=6u;
    }
    if(state->reset_phase==6u){
        if(!presentation_pose_export(memory,object,pose))return 0;
        game_memory_write_u32(memory,object+4u,links->previous);game_memory_write_u32(memory,object+8u,links->next);
        if(links->previous)game_memory_write_u32(memory,links->previous+8u,object);
        game_memory_write_u32(memory,links->source+4u,object);
        if(memory->fault)return 0;
        state->resetting=state->reset_phase=0u;
        state->initialize_phase=state->refreshing=state->property_index=0u;
    }
    return 1;
}
