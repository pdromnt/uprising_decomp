#include "presentation_commands.h"
#include <string.h>

PresentationCommandDisposition presentation_command_plan(const RuntimeNode *current,
    uint32_t priority,uint32_t kind){
    if(!current || !current->live || current->in_pool)return PRESENTATION_COMMAND_IGNORE;
    if(kind==12u)priority=current->sequence;
    if((int32_t)current->sequence>(int32_t)priority || current->kind==11u)
        return PRESENTATION_COMMAND_IGNORE;
    return current->kind==12u || current->kind==13u?
        PRESENTATION_COMMAND_REWRITE:PRESENTATION_COMMAND_PREPEND;
}

int presentation_command_build(GameMemory *memory,RuntimeNode *current,
    const PresentationCommand *command,RuntimeNode **result){
    PresentationCommandDisposition disposition;
    RuntimeNode *node;
    if(!memory || memory->fault || !current || !command || !result)return 0;
    *result=current;
    disposition=presentation_command_plan(current,command->priority,command->kind);
    if(disposition==PRESENTATION_COMMAND_IGNORE)return 1;
    if(disposition==PRESENTATION_COMMAND_PREPEND){
        if(!command->has_position){memory->fault=1;return 0;}
        node=runtime_node_pool_find(runtime_node_pool_acquire(memory));
        if(!node || memory->fault)return 0;
        node->next=current;current->previous=node;
        node->sequence=command->kind==12u?current->sequence:command->priority;
    }else node=current;
    node->kind=command->kind;node->target=command->target;
    if(command->has_target)node->target_identity=command->target_identity;
    node->parameter=command->parameter;
    node->flags=(node->flags&~3u)|(command->flagged?1u:0u)|
        (command->continue_chain?2u:0u);
    if(command->has_position)memcpy(node->position,command->position,sizeof(node->position));
    *result=node;return 1;
}

void presentation_command_seed_position(RuntimeNode *node,uint32_t identity,const float position[3]){
    uint32_t x,z;
    if(!node || !position || (identity&63u)>=4u || node->kind!=3u)return;
    memcpy(&x,node->position,4u);memcpy(&z,node->position+2u,4u);
    if((x&0x7fffffffu)==0u && (z&0x7fffffffu)==0u)
        memcpy(node->position,position,12u);
}

int presentation_command_replace(GameMemory *memory,PresentationLifecycle *state,const PresentationCommand *command){
    RuntimeNode *node;
    if(!memory || memory->fault || !state || !command)return 0;
    if(state->runtime.packet_chain && runtime_node_pool_return_chain(memory,state->runtime.packet_chain)!=1)return 0;
    state->runtime.packet_chain=0u;
    node=runtime_node_pool_find(runtime_node_pool_acquire(memory));
    if(!node || memory->fault)return 0;
    state->runtime.packet_chain=node->projection;
    if(!runtime_node_pool_prepare(memory,node->projection))return 0;
    node->target=command->target;node->target_identity=command->target_identity;
    if(command->has_position)memcpy(node->position,command->position,12u);
    return 1;
}

void presentation_command_publish(PresentationLifecycle *state,const RuntimeNode *node,
    const float target_position[3],uint32_t tick,GamePresentationRecordResult *result){
    PresentationRuntime *runtime=&state->runtime;
    memset(result,0,sizeof(*result));result->record=node->projection;result->type=node->kind;
    result->primary=node->kind;result->secondary=node->projection;
    runtime->counters[4]=0u;runtime->counters[2]=0u;runtime->mode=5u;
    switch(node->kind){
    case 0u:
        memcpy(runtime->command_position,node->position,12u);
        memcpy(&result->primary,node->position,4u);memcpy(&result->secondary,node->position+2u,4u);
        result->flags=GAME_PRESENTATION_RECORD_ADD;result->add_left=node->projection;result->add_right=24u;
        break;
    case 1u:case 8u:
        runtime->counters[4]=node->target;runtime->command_identity=node->target_identity;
        memcpy(runtime->command_position,target_position,12u);
        memcpy(&result->primary,target_position+1u,4u);memcpy(&result->secondary,target_position+2u,4u);
        result->flags=GAME_PRESENTATION_RECORD_ADD;result->add_left=node->target;result->add_right=24u;
        break;
    case 3u:case 4u:case 5u:case 6u:case 7u:
        runtime->counters[4]=result->primary=node->target;
        runtime->command_identity=result->secondary=node->target_identity;break;
    case 9u:
        result->add_left=node->parameter;result->add_right=tick;
        runtime->counters[3]=result->primary=node->parameter+tick;
        result->flags=GAME_PRESENTATION_RECORD_ADD;break;
    case 11u:
        runtime->counters[6]=runtime->counters[7]=0u;
        result->primary=0u;result->flags=GAME_PRESENTATION_RECORD_ZERO;break;
    case 12u:
        runtime->counters[8]=node->target;runtime->counters[6]=runtime->counters[7]=0u;
        result->primary=0u;result->flags=GAME_PRESENTATION_RECORD_ZERO;break;
    case 13u:
        runtime->counters[2]=result->primary=node->target;
        runtime->secondary_identity=result->secondary=node->target_identity;
        runtime->activation=0u;break;
    case 14u:runtime->activation=0u;break;
    default:break;
    }
}

/* Modes preserve the original route order: reverse, reverse loop,
 * out-and-back, and out-and-back loop. The loop anchor retains its original
 * height; route points acquire the shared travel height only when inserted. */
int presentation_command_path(GameMemory *memory,PresentationLifecycle *state,
    float (*points)[3],uint32_t count,unsigned mode){
    PresentationCommand command={0};RuntimeNode *anchor,*head;
    uint32_t i;
    if(!memory || memory->fault || !state || mode>3u || (count && !points) ||
        (mode && !count))return 0;
    if(!presentation_command_replace(memory,state,&command))return 0;
    anchor=head=runtime_node_pool_find(state->runtime.packet_chain);
    if(mode){
        memcpy(anchor->position,points[mode==1u?count-1u:0u],12u);
        anchor->kind=0u;anchor->flags|=1u;
        if(mode==1u)points[count-1u][1]=32768.0f;
    }
    command.has_position=1u;command.flagged=mode==1u || mode==3u;
    if(mode>=2u){
        for(i=0u;i<count;i++){
            points[i][1]=32768.0f;memcpy(command.position,points[i],12u);
            if(!presentation_command_build(memory,head,&command,&head))return 0;
            state->runtime.packet_chain=head->projection;
        }
    }
    i=mode?count-1u:count;
    while(i){
        --i;
        if(mode<2u)points[i][1]=32768.0f;
        memcpy(command.position,points[i],12u);
        if(!presentation_command_build(memory,head,&command,&head))return 0;
        state->runtime.packet_chain=head->projection;
    }
    if(mode==1u || mode==3u){anchor->next=head;head->previous=anchor;}
    return 1;
}

/*55cc20: travel through the preceding points, then bomb the final point and
 *leave toward that same point with parameter320. Route mode is unused. The
 *existing runtime pool remains the sole owner of every command. */
int presentation_command_aircraft_path(GameMemory *memory,PresentationLifecycle *state,
    float (*points)[3],uint32_t count){
    PresentationCommand command={0};RuntimeNode *head;uint32_t i;
    if(!memory || memory->fault || !state || !points || !count || count>65536u)return 0;
    if(!presentation_command_replace(memory,state,&command))return 0;
    head=runtime_node_pool_find(state->runtime.packet_chain);
    command.has_position=1;command.kind=2;command.parameter=320;
    points[count-1u][1]=32768.0f;memcpy(command.position,points[count-1u],12);
    if(!presentation_command_build(memory,head,&command,&head))return 0;
    state->runtime.packet_chain=head->projection;
    command.kind=5;command.parameter=0;
    if(!presentation_command_build(memory,head,&command,&head))return 0;
    state->runtime.packet_chain=head->projection;
    command.kind=0;i=count-1u;
    while(i){
        --i;points[i][1]=32768.0f;memcpy(command.position,points[i],12);
        if(!presentation_command_build(memory,head,&command,&head))return 0;
        state->runtime.packet_chain=head->projection;
    }
    return !memory->fault;
}
