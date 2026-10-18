#include "attachment_effect.h"
#include "mesh_buffer.h"
#include "gameplay_state.h"
#include "gameplay_leaves.h"
#include "game_matrix.h"
#include "gameplay_geometry.h"
#include "entity_queries.h"
#include "terrain_asset.h"
#include "projectile.h"
#include "gameplay_spatial.h"
#include "game_intrusive_list.h"
#include "effect_resource_pool.h"
#include <math.h>
#include <string.h>

static float read_float(GameMemory *memory,GameAddress address){
    uint32_t bits=game_memory_read_u32(memory,address);float value;
    memcpy(&value,&bits,4u);return value;
}
static void write_float(GameMemory *memory,GameAddress address,float value){
    uint32_t bits;memcpy(&bits,&value,4u);game_memory_write_u32(memory,address,bits);
}
static uint32_t truncate_word(long double value){
    /* FISTP int64 indefinite has a zero low dword. */
    if(!isfinite(value) || value<-9223372036854775808.0L || value>=9223372036854775808.0L)return 0u;
    return (uint32_t)(int64_t)value;
}
static uint32_t emission_random(GameMemory *memory,GameAddress multiplier){
    GameplaySubmissionSample sample=gameplay_state_prepare_submission_sample(memory,0u);
    long double value=(long double)read_float(memory,sample.scalar)*read_float(memory,multiplier);
    gameplay_state_commit_submission_sample(memory,&sample);return truncate_word(value);
}
int attachment_effect_initialize(GameMemory *memory,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);unsigned i;
    if(!pose || !presentation_instance_enable_attachment(memory,object) ||
        !presentation_instance_reserve_attachment_buffers(object))return 0;
    game_memory_write_u32(memory,object,0x577980u);
    game_memory_write_u8(memory,object+12u,10u);pose->flags|=0x802u;
    /* Shared 536440/536520 extended presentation base initializes motion. */
    for(i=0;i<3u;i++){
        game_memory_write_u32(memory,object+0xb4u+i*4u,0u);
        game_memory_write_u16(memory,object+0xccu+i*2u,0u);
    }
    game_memory_write_u16(memory,object+0xd4u,0u);
    game_memory_write_u16(memory,object+0xe4u,0u);
    game_memory_write_u32(memory,object+0xdcu,0u);
    game_memory_write_u32(memory,object+0xecu,0u);
    game_memory_write_u32(memory,object+0x100u,0u);
    game_memory_write_u32(memory,object+0xe8u,0x3f800000u);
    for(i=0;i<4u;i++)game_memory_write_u32(memory,object+0xf0u+i*4u,0u);
    return !memory->fault;
}
int attachment_effect_configure(GameMemory *memory,GameAddress object,
        GameAddress descriptor,GameAddress bones,uint16_t count){
    PresentationPose *pose=presentation_instance_pose(object);unsigned i;
    if(!pose || count>20u || !mesh_buffer_bind(memory,pose->model,descriptor))return 0;
    pose->vertex_count=count;pose->hull=bones;
    pose->radius=(float)((long double)read_float(memory,descriptor+0x20u)*
        (long double)read_float(memory,0x57793cu));
    for(i=0;i<3u;i++)pose->dimensions[i]=read_float(memory,descriptor+0x24u+i*4u);
    return !memory->fault;
}
int attachment_mesh_clear(GameMemory *memory,GameAddress model){
    GameAddress descriptor=game_memory_read_u32(memory,model+0x50u);
    game_memory_write_u32(memory,model+0x60u,0u);
    game_memory_write_u32(memory,descriptor+0x1cu,0u);
    game_memory_write_u32(memory,descriptor+0x14u,0u);
    game_memory_write_u32(memory,descriptor,0u);
    game_memory_write_u32(memory,descriptor+0x34u,0u);
    return !memory->fault;
}
int attachment_effect_present(GameMemory *memory,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);uint32_t scale_bits;float scale;unsigned i;
    float ca,cb,cc,sa,sb,sc,t0,t1,t2,t3;
    if(!pose)return 0;
    if(pose->flags&0x80000u)return 1;
    ca=read_float(memory,0x6298a8u+((pose->angles[0]+512u)&2047u)*4u);
    cb=read_float(memory,0x6298a8u+((pose->angles[1]+512u)&2047u)*4u);
    cc=read_float(memory,0x6298a8u+((pose->angles[2]+512u)&2047u)*4u);
    sa=read_float(memory,0x6298a8u+(pose->angles[0]&2047u)*4u);
    sb=read_float(memory,0x6298a8u+(pose->angles[1]&2047u)*4u);
    sc=read_float(memory,0x6298a8u+(pose->angles[2]&2047u)*4u);
    /* These four float stores are observable original numeric boundaries. */
    t0=(float)((long double)sc*sa);t1=(float)((long double)cc*ca);
    t2=(float)((long double)sa*cc);t3=(float)((long double)ca*sc);
    pose->world_matrix[0]=(float)((long double)t0*sb+t1);
    pose->world_matrix[1]=(float)((long double)t2*sb-t3);
    pose->world_matrix[2]=(float)((long double)sa*cb);
    pose->world_matrix[4]=(float)((long double)sc*cb);
    pose->world_matrix[5]=(float)((long double)cc*cb);
    pose->world_matrix[6]=-sb;
    pose->world_matrix[8]=(float)((long double)t3*sb-t2);
    pose->world_matrix[9]=(float)((long double)sb*t1+t0);
    pose->world_matrix[10]=(float)((long double)cb*ca);
    for(i=0;i<3u;i++)pose->world_matrix[i*4u+3u]=pose->position[i];
    scale_bits=game_memory_read_u32(memory,object+0xe8u);memcpy(&scale,&scale_bits,4u);
    if(scale_bits!=0x3f800000u)for(i=0;i<12u;i++)if((i&3u)!=3u)
        pose->world_matrix[i]=(float)((long double)pose->world_matrix[i]*(long double)scale);
    /* Both live-motion bits clear means the matrix may be cached. */
    if(!(game_memory_read_u16(memory,object+0xe4u)&0x101u))pose->flags|=0x80000u;
    return !memory->fault;
}
int attachment_effect_reset(GameMemory *memory,GameAddress object,const AttachmentResetServices *services){
    PresentationPose *pose=presentation_instance_pose(object);GameAddress child;unsigned i;
    if(!pose || !services)return 0;
    if(game_memory_read_u16(memory,object+0xe4u)&8u){
        uint32_t voice=game_memory_read_u32(memory,object+0x104u);
        if(!services->stop_voice || !services->stop_voice(services->context,voice))return 0;
    }
    game_memory_write_u16(memory,object+0xd4u,0u);
    for(i=0;i<3u;i++)game_memory_write_u32(memory,object+0xd8u+i*4u,0u);
    if(!attachment_mesh_clear(memory,pose->model))return 0;
    pose->vertex_count=0u;pose->hull=0u;pose->radius=1.0f;
    memset(pose->dimensions,0,sizeof(pose->dimensions));
    game_memory_write_u16(memory,object+0xe4u,0u);
    game_memory_write_u32(memory,object+0xe8u,0x3f800000u);
    for(i=0;i<5u;i++){
        GameAddress slot=object+(i?0xf0u+(i-1u)*4u:0xecu);
        child=game_memory_read_u32(memory,slot);
        if(child && (!services->remove_child || !services->remove_child(services->context,child)))return 0;
        game_memory_write_u32(memory,slot,0u);
    }
    game_memory_write_u32(memory,object+0x100u,0u);
    return !memory->fault && presentation_instance_reset(memory,object,1u,services->membership)==1;
}
int attachment_effect_controller(GameMemory *memory,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);uint32_t bits;float scale;unsigned i;
    float ca,cb,cc,sa,sb,sc,scca,matrix[12];
    if(!pose)return 0;
    if(pose->flags&0x40000u)return 1;
    ca=read_float(memory,0x6298a8u+((pose->local_angles[2]+512u)&2047u)*4u);
    cb=read_float(memory,0x6298a8u+((pose->local_angles[1]+512u)&2047u)*4u);
    cc=read_float(memory,0x6298a8u+((pose->local_angles[0]+512u)&2047u)*4u);
    sa=read_float(memory,0x6298a8u+(pose->local_angles[2]&2047u)*4u);
    sb=read_float(memory,0x6298a8u+(pose->local_angles[1]&2047u)*4u);
    sc=read_float(memory,0x6298a8u+(pose->local_angles[0]&2047u)*4u);
    scca=(float)((long double)sc*ca);
    matrix[0]=(float)(((long double)sc*sb)*sa+(long double)ca*cc);
    matrix[1]=(float)(((long double)sb*sa)*cc-scca);
    matrix[2]=(float)((long double)cb*sa);
    matrix[4]=(float)((long double)sc*cb);matrix[5]=(float)((long double)cb*cc);matrix[6]=-sb;
    matrix[8]=(float)((long double)scca*sb-(long double)sa*cc);
    matrix[9]=(float)(((long double)sb*ca)*cc+(long double)sa*sc);
    matrix[10]=(float)((long double)cb*ca);
    for(i=0;i<3u;i++)matrix[i*4u+3u]=pose->local_position[i];
    bits=game_memory_read_u32(memory,object+0xe8u);memcpy(&scale,&bits,4u);
    for(i=0;i<12u;i++){
        float value=matrix[i];uint32_t value_bits;
        if(bits!=0x3f800000u && (i&3u)!=3u)value=(float)((long double)value*scale);
        memcpy(&value_bits,&value,4u);game_memory_write_u32(memory,pose->model+4u+i*4u,value_bits);
    }
    if(!(game_memory_read_u16(memory,object+0xe4u)&0x101u)){
        for(i=0;i<3u;i++)if(pose->position[i]<pose->local_position[i] || pose->position[i]>pose->local_position[i] ||
            pose->angles[i]!=pose->local_angles[2u-i])return !memory->fault;
        pose->flags|=0x40000u;
    }
    return !memory->fault;
}
int attachment_effect_finish_placement(GameMemory *memory,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);unsigned i;
    if(!pose)return 0;
    for(i=0;i<3u;i++){
        pose->local_position[i]=pose->position[i];pose->local_angles[2u-i]=pose->angles[i];
    }
    pose->local_phase=0u;
    if(!attachment_effect_controller(memory,object) || !presentation_pose_transform_vertices(memory,pose,1))return 0;
    game_memory_write_u32(memory,object+0xb0u,0u);return !memory->fault;
}
static void initialize_motion(GameMemory *memory,GameAddress object,const AttachmentEmission *request,const uint16_t motion[3]){
    unsigned i;for(i=0;i<3u;i++){
        write_float(memory,object+0xb4u+i*4u,request->velocity[i]);
        game_memory_write_u16(memory,object+0xccu+i*2u,motion[i]);
    }
}
int attachment_effect_scale(GameMemory *memory,GameAddress object,float radius){
    PresentationPose *pose=presentation_instance_pose(object);GameAddress descriptor;
    uint32_t bits;float scale;unsigned i;
    if(!pose)return 0;
    memcpy(&bits,&radius,4u);
    descriptor=game_memory_read_u32(memory,pose->model+0x50u);
    if(!(bits&0x7fffffffu)){
        scale=1.0f;pose->radius=(float)((long double)read_float(memory,descriptor+0x20u)*read_float(memory,0x57793cu));
        for(i=0;i<3u;i++)pose->dimensions[i]=read_float(memory,descriptor+0x24u+i*4u);
    }else{
        pose->radius=radius;scale=(float)(((long double)radius/read_float(memory,descriptor+0x20u))*read_float(memory,0x57793cu));
        for(i=0;i<3u;i++)pose->dimensions[i]=(float)((long double)read_float(memory,descriptor+0x24u+i*4u)*scale);
    }
    write_float(memory,object+0xe8u,scale);return !memory->fault;
}
uint32_t attachment_effect_detail(GameMemory *memory,GameAddress object){
    return game_memory_read_u16(memory,object+0xe4u)&0x4000u?10u:40u;
}
int attachment_effect_accelerate(GameMemory *memory,GameAddress object){
    float velocity[3],step,speed;long double magnitude;unsigned i;
    if(!presentation_instance_pose(object))return 0;
    for(i=0;i<3u;i++)velocity[i]=read_float(memory,object+0xb4u+i*4u);
    if(game_memory_read_u16(memory,object+0xe4u)&8u){
        step=read_float(memory,0x577978u);write_float(memory,object+0xb8u,(float)((long double)velocity[1]-step));
        magnitude=sqrtl((long double)velocity[2]*velocity[2]+(long double)velocity[0]*velocity[0]);speed=(float)magnitude;
        /* Original C3 skips equal and unordered magnitudes. */
        if(!isnan(magnitude) && magnitude!=read_float(memory,0x577940u)){
            write_float(memory,object+0xb4u,(float)(((long double)velocity[0]*step)/speed+velocity[0]));
            write_float(memory,object+0xbcu,(float)(((long double)velocity[2]*step)/speed+velocity[2]));
        }
        if(!(game_memory_read_u32(memory,0x5ac6acu)&1u)){
            uint16_t angle=game_memory_read_u16(memory,object+0xceu);
            angle=(uint16_t)(angle+((int16_t)angle>0?1u:0xffffu));game_memory_write_u16(memory,object+0xceu,angle);
        }
    }else write_float(memory,object+0xb8u,(float)((long double)velocity[1]-read_float(memory,0x57795cu)));
    return !memory->fault;
}
int attachment_effect_move(GameMemory *memory,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);unsigned i;
    static const uint32_t angular_offsets[3]={0xccu,0xd0u,0xceu};
    if(!pose)return 0;
    for(i=0;i<3u;i++){
        uint16_t angle=(uint16_t)((pose->angles[i]+game_memory_read_u16(memory,object+angular_offsets[i]))&2047u);
        pose->position[i]=(float)((long double)read_float(memory,object+0xb4u+i*4u)+pose->position[i]);
        pose->angles[i]=i && angle>1024u?(uint16_t)(angle-2048u):angle;
    }
    return attachment_effect_present(memory,object) && presentation_pose_transform_vertices(memory,pose,0);
}
int attachment_effect_interpolate(GameMemory *memory,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);unsigned i;uint32_t delta;
    static const uint32_t angular_offsets[3]={0xccu,0xd0u,0xceu};
    if(!pose)return 0;
    for(i=0;i<3u;i++){pose->local_position[i]=pose->position[i];pose->local_angles[2u-i]=pose->angles[i];}
    if(game_memory_read_u32(memory,0x5eff04u) && (game_memory_read_u16(memory,object+0xe4u)&1u)){
        delta=game_memory_read_u32(memory,0x5acbe8u)-game_memory_read_u32(memory,0x5acc80u);
        for(i=0;i<3u;i++){
            pose->local_position[i]=(float)((long double)read_float(memory,object+0xb4u+i*4u)*(int32_t)delta+pose->local_position[i]);
            pose->local_angles[2u-i]=(uint16_t)(pose->angles[i]+game_memory_read_u16(memory,object+angular_offsets[i])*(uint32_t)(uint16_t)delta);
        }
    }
    return attachment_effect_controller(memory,object) && presentation_pose_transform_vertices(memory,pose,1);
}
static int32_t attachment_signed(uint32_t value){
    return value<=INT32_MAX?(int32_t)value:(int32_t)((int64_t)value-4294967296LL);
}
static long double attachment_random(GameMemory *memory){
    GameplaySubmissionSample sample=gameplay_state_prepare_submission_sample(memory,0u);
    long double value=read_float(memory,sample.scalar);gameplay_state_commit_submission_sample(memory,&sample);return value;
}
static int emit_anchors(GameMemory *memory,GameAddress source,int32_t count,
        GameAddress anchors,const float inherited[3],uint16_t extra,int legacy,const AttachmentAnchorServices *services){
    PresentationPose borrowed,*pose=presentation_instance_pose(source);GameAddress manager,child=0u;
    int32_t index;unsigned axis;
    if(!services || !services->acquire || !services->emit)return 0;
    if(!pose && legacy){
        /*Small scenery records keep their existing raw owner. This bounded,
         *read-only death view ends here; it never creates a competing object. */
        if(!presentation_pose_import_prefix(memory,source,&borrowed,0x64u))return 0;
        pose=&borrowed;
    }
    if(!pose)return 0;
    manager=game_memory_read_u32(memory,0x5f0210u+(pose->identity&63u)*4u);
    if(count<=0)return !memory->fault;
    if((uint64_t)anchors+(uint64_t)(uint32_t)count*32u>UINT32_MAX)return 0;
    for(index=0;index<count;index++){
        GameAddress record=anchors+(uint32_t)index*32u,model,descriptor;
        int16_t selector=(int16_t)game_memory_read_u16(memory,record);uint32_t slot,tick;
        float local[12],world[12],delta[3],nx,ny,speed;uint16_t angles[3];
        AttachmentEmission request={0};long double magnitude,factor;
        if(selector<0)return 0;
        slot=game_memory_read_u32(memory,game_memory_read_u32(memory,0x639afcu)+(uint32_t)selector*4u);
        if(slot>UINT16_MAX || memory->fault)return 0;
        child=services->acquire(services->context,(uint16_t)slot);if(!child)return 0;
        model=game_memory_read_u32(memory,game_memory_read_u32(memory,0x639b04u)+(uint32_t)selector*4u);
        descriptor=game_memory_read_u32(memory,model+0x50u);
        if(!attachment_effect_configure(memory,child,descriptor,0u,0u) ||
           !attachment_effect_scale(memory,child,read_float(memory,record+0x18u)))goto failed;
        for(axis=0;axis<3u;axis++)angles[axis]=game_memory_read_u16(memory,record+0x10u+axis*2u);
        game_matrix_attachment_orientation(memory,local,angles);
        for(axis=0;axis<3u;axis++)local[axis*4u+3u]=read_float(memory,record+4u+axis*4u);
        if(legacy){float matrix[12];for(axis=0;axis<12;axis++)matrix[axis]=read_float(memory,pose->model+4u+axis*4u);
            game_matrix_compose_values(world,matrix,local);
        }else game_matrix_compose_values(world,pose->world_matrix,local);
        if(!game_matrix_extract_angles(memory,world,request.angles))goto failed;
        for(axis=0;axis<3u;axis++)request.position[axis]=world[axis*4u+3u];
        request.flags=(uint16_t)(extra|6u);
        if(game_memory_read_u32(memory,record+0x1cu)){
            request.flags|=1u;
            for(axis=0;axis<3u;axis++)delta[axis]=(float)((long double)request.position[axis]-pose->position[axis]);
            if(delta[0]==0.0f && delta[1]==0.0f && delta[2]==0.0f){nx=0.0f;ny=1.0f;}
            else{
                float squared=(float)(((long double)delta[1]*delta[1]+(long double)delta[2]*delta[2])+
                    (long double)delta[0]*delta[0]);
                magnitude=sqrtl(squared);speed=(float)magnitude;
                if(!isfinite(speed) || speed==0.0f)goto failed;
                if(speed==1.0f){nx=delta[0];ny=delta[1];}
                else{factor=(long double)read_float(memory,legacy?0x57445cu:0x576594u)/speed;
                    nx=(float)((long double)delta[0]*factor);ny=(float)((long double)delta[1]*factor);}
            }
            speed=(float)attachment_signed(emission_random(memory,legacy?0x574460u:0x576598u)+20u);
            factor=sqrtl((float)((long double)read_float(memory,legacy?0x574464u:0x57659cu)/presentation_instance_pose(child)->radius));
            request.velocity[0]=(float)(((long double)speed*nx+inherited[0])*factor);
            request.velocity[1]=(float)(((long double)speed*ny+inherited[1])*factor);
            request.velocity[2]=request.velocity[0];
        }
        tick=game_memory_read_u32(memory,0x5ac6acu);
        if((tick+(uint32_t)index)&3u)request.flags|=0x10u;
        if(legacy){
            /*4a6830 consumes separate duration/mode samples.
             *Keep its wrap points;5356b0 uses a different one-sample recipe. */
            uint32_t sample=game_memory_read_u32(memory,0x6308c0u),next;
            if(sample==1024u)sample=0;
            if(sample>1023u)goto failed;
            next=sample+1u;if(next==1024u)next=0;
            request.duration=truncate_word((long double)read_float(memory,0x6308c8u+sample*4u)*read_float(memory,0x574468u))+60u;
            request.mode=(uint16_t)truncate_word((long double)read_float(memory,0x6308c8u+next*4u)*read_float(memory,0x57446cu));
            game_memory_write_u32(memory,0x6308c0u,next+1u);
        }else{
            request.duration=emission_random(memory,0x5765a0u)+60u;
            request.mode=(uint16_t)((game_memory_read_u32(memory,0x5ac6acu)+(uint32_t)index)&1u);
        }
        request.source=pose->identity;
        if(memory->fault || !services->emit(services->context,manager,child,&request))goto failed;
    }
    return !memory->fault;
failed:
    if(services->discard)services->discard(services->context,child);
    return 0;
}
int attachment_effect_anchors(GameMemory *m,GameAddress o,int32_t count,GameAddress records,const float velocity[3],const AttachmentAnchorServices *s){
    return emit_anchors(m,o,count,records,velocity,0,0,s);
}
int attachment_effect_legacy_anchors(GameMemory *m,GameAddress o,int32_t count,GameAddress records,const float velocity[3],uint16_t flags,const AttachmentAnchorServices *s){
    return emit_anchors(m,o,count,records,velocity,flags,1,s);
}
int attachment_effect_retain_anchors(GameMemory *m,GameAddress o,int32_t count,GameAddress records,uint16_t sound,const float velocity[3]){
    unsigned i;
    if(m->fault || !presentation_instance_pose(o) || !velocity)return 0;
    game_memory_write_u32(m,o+0x108u,(uint32_t)count);game_memory_write_u32(m,o+0x10cu,records);
    game_memory_write_u16(m,o+0x11cu,sound);
    for(i=0;i<3;i++)write_float(m,o+0x110u+i*4u,velocity[i]);
    return !m->fault;
}
/*5561f0: move handles, never copy ownership. Source count is independent
 *of the sparse four slots, and its original unsigned decrement is retained. */
int attachment_effect_transfer_children(GameMemory *m,GameAddress o,GameAddress slots,GameAddress count_address){
    GameAddress children[4];uint32_t count;unsigned i,n=0;
    if(m->fault || !presentation_instance_pose(o))return 0;
    count=game_memory_read_u32(m,count_address);
    for(i=0;i<4;i++){children[i]=game_memory_read_u32(m,slots+i*4u);if(children[i])n++;}
    if(m->fault)return 0;
    for(i=0;i<4;i++){
        game_memory_write_u32(m,o+0xf0u+i*4u,children[i]);
        if(children[i])game_memory_write_u32(m,slots+i*4u,0);
    }
    game_memory_write_u32(m,count_address,count-n);game_memory_write_u32(m,o+0x100u,n);
    return !m->fault;
}
float attachment_effect_pitch(GameMemory *memory,float altitude,long double ground){
    long double value=(((long double)altitude-ground)/read_float(memory,0x59d8a8u))*
        read_float(memory,0x577944u)+read_float(memory,0x577938u);
    float pitch=(float)value,cap=read_float(memory,0x577948u);
    if(isnan(value) || value<read_float(memory,0x577938u))return 1.0f;
    return cap>pitch?pitch:cap;
}
static int attachment_vertex(GameMemory *memory,const PresentationPose *pose,uint32_t index,float position[3]){
    GameAddress descriptor=game_memory_read_u32(memory,pose->model+0x50u);
    GameAddress points=game_memory_read_u32(memory,descriptor);float local[3];unsigned i;
    uint32_t count=game_memory_read_u32(memory,descriptor+0x14u);
    if(memory->fault || index>=count || (uint64_t)points+(uint64_t)index*12u+12u>UINT32_MAX)return 0;
    for(i=0;i<3u;i++)local[i]=read_float(memory,points+index*12u+i*4u);
    game_matrix_transform_point_values(position,pose->world_matrix,local);return !memory->fault;
}
int attachment_effect_expansion(GameMemory *memory,GameAddress object,const AttachmentUpdateServices *s){
    PresentationPose *pose=presentation_instance_pose(object);GameAddress descriptor,child;float position[3];
    if(!pose || !s || !s->spawn_expansion)return 0;
    if(!game_terrain_cell_presence(memory,object,0u))return !memory->fault;
    descriptor=game_memory_read_u32(memory,pose->model+0x50u);
    memcpy(position,pose->position,12u);
    position[1]=(float)(((long double)read_float(memory,descriptor+0x34u)-
        (long double)pose->radius*read_float(memory,0x577974u))+position[1]);
    if(memory->fault || !s->spawn_expansion(s->context,position,
        (float)((long double)pose->radius*read_float(memory,0x577978u)),
        (float)((long double)pose->radius*read_float(memory,0x57793cu)),&child))return 0;
    game_memory_write_u32(memory,object+0xecu,child);return !memory->fault;
}
int attachment_effect_contact(GameMemory *memory,GameAddress object,GameAddress other,const AttachmentContactServices *s){
    PresentationPose *pose=presentation_instance_pose(object);uint16_t flags;int active;int32_t amount;
    const AttachmentUpdateServices *update=s?s->update:NULL;
    if(!pose || !update || !update->post || !update->reset)return 0;
    if(attachment_signed(pose->properties[0])<=0)return 1;
    flags=game_memory_read_u16(memory,object+0xe4u);
    if((flags&0x4001u) && !(game_memory_read_u32(memory,other+0x10u)&0x400u))
        return !memory->fault && update->post(update->context,other) && update->reset(update->context);
    if(!native_entity_type_activity(memory,other,&active))return 0;
    if(active && (!update->post(update->context,other) || !update->reset(update->context)))return 0;
    flags=game_memory_read_u16(memory,object+0xe4u);
    if(flags&0x201u || attachment_signed(pose->properties[0])<=0)return !memory->fault;
    if(!active && !(game_memory_read_u32(memory,other+0x10u)&4u)){
        uint8_t type=game_memory_read_u8(memory,other+12u);
        if(type!=0x41u && type!=11u && type!=10u)return !memory->fault;
    }
    /* Class10's shared damage response has no player/structure notification tail. */
    if(pose->flags&0x100u || !s->damage || !s->damage(s->context,other,&amount))return 0;
    pose->properties[0]-=(uint32_t)amount;
    if(attachment_signed(pose->properties[0])<=0)
        return update->post(update->context,other) && update->reset(update->context);
    return !memory->fault;
}
int attachment_effect_advance(GameMemory *memory,GameAddress object,const AttachmentAdvanceServices *s){
    PresentationPose *pose=presentation_instance_pose(object);float previous[3],normal[3],velocity[3],point[3];
    GameAddress record,other;unsigned axis,visits=0u;int hit;EffectResourcePool *pool;
    const AttachmentUpdateServices *update=s && s->contact?s->contact->update:NULL;
    if(!pose || !s || !update)return 0;
    memcpy(previous,pose->position,12u);
    if(!attachment_effect_accelerate(memory,object) || !attachment_effect_move(memory,object))return 0;
    if(!gameplay_geometry_world_bounds_values(pose->radius,pose->position))return update->reset && update->reset(update->context);
    game_list_detach_node(memory,object,4u,8u);
    if(memory->fault || !s->gather || !s->gather(s->context,previous))return 0;
    record=game_memory_read_u32(memory,object+0xb0u);
    while(record && !memory->fault){
        if(++visits>65536u)return 0;
        other=game_memory_read_u32(memory,record+12u);
        for(axis=0;axis<3u;axis++){velocity[axis]=read_float(memory,object+0xb4u+axis*4u);point[axis]=read_float(memory,record+16u+axis*4u);}
        if(!s->peer_contact || !s->peer_contact(s->context,other,velocity,record+16u,point) ||
            !attachment_effect_contact(memory,object,other,s->contact))return 0;
        /* Reset owns outstanding contacts when a response kills the object. */
        if(!pose->properties[0])return !memory->fault;
        record=game_memory_read_u32(memory,record+4u);
    }
    record=game_memory_read_u32(memory,object+0xb0u);
    if(record){pool=effect_resource_pool_find(0x631910u);
        if(!pool || effect_resource_pool_return_chain(memory,pool,record)!=1)return 0;
        game_memory_write_u32(memory,object+0xb0u,0u);
    }
    if(!presentation_terrain_contact(memory,pose,normal,&hit))return 0;
    if(hit){
        if(!attachment_effect_terrain_response(memory,object,s->terrain))return 0;
        if(!pose->properties[0])return !memory->fault;
    }
    gameplay_spatial_insert(memory,0x5f0800u,object,(int32_t)pose->position[0]>>12,(int32_t)pose->position[2]>>12,(pose->flags&0x400u)!=0u);
    return !memory->fault;
}
int attachment_effect_terrain_response(GameMemory *memory,GameAddress object,const AttachmentTerrainServices *s){
    PresentationPose *pose=presentation_instance_pose(object);uint16_t flags;uint8_t terrain_flags;uint32_t xb,zb,cell,normal_bits;
    GameAddress terrain;int16_t mode;float ground,normal[3];long double threshold;unsigned axis;
    if(!pose || !s)return 0;
    flags=game_memory_read_u16(memory,object+0xe4u);
    if((flags&8u) && game_memory_read_u32(memory,game_memory_read_u32(memory,0x628a90u)+0x468u)){
        if(!s->stop_voice || !s->stop_voice(s->context,game_memory_read_u32(memory,object+0x104u)))return 0;
    }
    /* Retail uses an unsigned bit comparison: exclude positive Y and -0. */
    if(game_memory_read_u32(memory,object+0xb8u)<=0x80000000u)return !memory->fault;
    xb=truncate_word(pose->position[0]);zb=truncate_word(pose->position[2]);
    cell=((zb&0xfffffe00u)*4u)+(uint32_t)(attachment_signed(xb&0xfffffe3fu)>>6);
    terrain=game_memory_read_u32(memory,0x597d90u);
    if((uint64_t)terrain+cell+6u>UINT32_MAX)return 0;
    terrain_flags=memory->terrain && memory->terrain->cells && cell<65536u*8u?
        memory->terrain->cells[cell/8u].flags:game_memory_read_u8(memory,terrain+cell+5u);
    mode=(int16_t)game_memory_read_u16(memory,object+0xd4u);
    if(!(terrain_flags&0x50u) && mode>0){
        game_memory_write_u16(memory,object+0xd4u,(uint16_t)(mode-1));
        write_float(memory,object+0xb8u,(float)((long double)read_float(memory,object+0xb8u)*read_float(memory,0x577964u)));
        write_float(memory,object+0xb4u,(float)((long double)attachment_signed(emission_random(memory,0x577960u)-5u)+read_float(memory,object+0xb4u)));
        write_float(memory,object+0xbcu,(float)((long double)attachment_signed(emission_random(memory,0x577960u)-5u)+read_float(memory,object+0xbcu)));
        return !memory->fault;
    }
    ground=(float)gameplay_geometry_terrain_height(memory,pose->position[0],pose->position[2]);
    threshold=(long double)pose->radius*read_float(memory,0x577968u)+ground;
    if(isnan(threshold) || isnan(pose->position[1]) || threshold<pose->position[1])return !memory->fault;
    flags=game_memory_read_u16(memory,object+0xe4u);
    if(!(flags&0x40u)){
        game_memory_write_u16(memory,object+0xe4u,(uint16_t)(flags|0x40u));
        if((terrain_flags&0x50u) && (!s->impact || !s->impact(s->context,terrain_flags)))return 0;
    }
    threshold=gameplay_geometry_detail_height(memory,pose->position[0],pose->position[2])+
        (long double)pose->radius*read_float(memory,0x577968u)+ground;
    if(isnan(threshold) || isnan(pose->position[1]) || threshold<pose->position[1])return !memory->fault;
    flags=(uint16_t)(game_memory_read_u16(memory,object+0xe4u)&~1u);
    game_memory_write_u16(memory,object+0xe4u,flags);
    for(axis=0;axis<3u;axis++){
        game_memory_write_u32(memory,object+0xb4u+axis*4u,0u);
        game_memory_write_u16(memory,object+0xccu+axis*2u,0u);
    }
    if(flags&4u)game_memory_write_u32(memory,object+0xd8u,game_memory_read_u32(memory,0x5ac6acu));
    if(!(terrain_flags&0x10u) && !(flags&0x20u) && !attachment_effect_expansion(memory,object,s->update))return 0;
    if(!gameplay_geometry_terrain_normal_values(memory,normal,pose->position[0],pose->position[2]))return 0;
    memcpy(&normal_bits,normal+1,4u);
    if(attachment_signed(normal_bits)<0x3f59999a)game_memory_write_u32(memory,object+0xdcu,1u);
    return !memory->fault;
}
int attachment_effect_post(GameMemory *memory,GameAddress object,GameAddress collision,const AttachmentUpdateServices *s){
    PresentationPose *pose=presentation_instance_pose(object);uint16_t flags;GameAddress anchors;float velocity[3];unsigned i;
    (void)collision;
    if(!pose || !s)return 0;
    flags=game_memory_read_u16(memory,object+0xe4u);
    if(flags&2u){
        if(!s->burst || !s->burst(s->context,6u,1,flags&0x800u?-1:1))return 0;
        flags=game_memory_read_u16(memory,object+0xe4u);
        for(i=0;i<3u;i++)velocity[i]=flags&1u?read_float(memory,object+0xb4u+i*4u):(i==1u?30.0f:0.0f);
        if(!s->mesh_fragments || !s->mesh_fragments(s->context,
            game_memory_read_u32(memory,pose->model+0x50u),pose->world_matrix,velocity))return 0;
    }
    anchors=game_memory_read_u32(memory,object+0x10cu);
    if(anchors){
        flags=game_memory_read_u16(memory,object+0xe4u);
        for(i=0;i<3u;i++){
            velocity[i]=read_float(memory,object+0x110u+i*4u);
            if(flags&1u)velocity[i]=(float)((long double)velocity[i]+read_float(memory,object+0xb4u+i*4u));
        }
        if(!s->anchors || !s->anchors(s->context,attachment_signed(game_memory_read_u32(memory,object+0x108u)),anchors,velocity) ||
            !s->sound || !s->sound(s->context,pose->position,(int16_t)game_memory_read_u16(memory,object+0x11cu)))return 0;
    }
    flags=game_memory_read_u16(memory,object+0xe4u);
    if((flags&0x81u)==0x80u)game_terrain_impact(memory,pose->position[0],pose->position[2],0x400);
    return !memory->fault;
}
int attachment_effect_update(GameMemory *memory,GameAddress object,const AttachmentUpdateServices *s){
    PresentationPose *pose=presentation_instance_pose(object);uint16_t flags;uint32_t visible,tick,index,count;
    GameAddress descriptor,child;float position[3],size;int32_t radius,quarter;
    if(!pose || !s)return 0;
    visible=game_terrain_cell_presence(memory,object,0u);flags=game_memory_read_u16(memory,object+0xe4u);
    if(memory->fault)return 0;
    if(!(flags&1u) && !visible)return s->reset && s->reset(s->context);
    tick=game_memory_read_u32(memory,0x5ac6acu);
    if(flags&0x100u){
        if(attachment_signed(game_memory_read_u32(memory,object+0xd8u)+game_memory_read_u32(memory,object+0x120u))<=attachment_signed(tick)){
            flags=(uint16_t)((flags|1u)&~0x500u);game_memory_write_u16(memory,object+0xe4u,flags);
            game_memory_write_u32(memory,object+0xd8u,tick);
            if(!s->sound || !s->sound(s->context,pose->position,130) || !s->burst || !s->burst(s->context,0u,0,1))return 0;
        }
    }else if(!(flags&0x2000u) && (!(flags&1u) || !(flags&4u)) &&
            attachment_signed(game_memory_read_u32(memory,object+0xd8u)+game_memory_read_u32(memory,object+0xdcu))<=attachment_signed(tick)){
        return s->post && s->post(s->context,0u) && s->reset && s->reset(s->context);
    }
    if((game_memory_read_u16(memory,object+0xe4u)&0x400u) && visible && emission_random(memory,0x577960u)==0u){
        descriptor=game_memory_read_u32(memory,pose->model+0x50u);
        count=game_memory_read_u32(memory,descriptor+0x14u);
        index=truncate_word((long double)attachment_signed(count)*attachment_random(memory));
        if(!attachment_vertex(memory,pose,index,position) || !s->sound ||
            !s->sound(s->context,pose->position,index&1u?(index&2u?1:0):(index&2u?4:1)))return 0;
        radius=attachment_signed(truncate_word(pose->radius));
        quarter=radius>=0?radius/4:(int32_t)(((int64_t)radius-3)/4);
        size=(float)attachment_signed(truncate_word((long double)attachment_signed((uint32_t)radius-(uint32_t)quarter)*
            attachment_random(memory))+(uint32_t)quarter);
        if(!s->spawn_animation || !s->spawn_animation(s->context,position,
            game_memory_read_u8(memory,object+12u),size))return 0;
    }
    flags=game_memory_read_u16(memory,object+0xe4u);child=game_memory_read_u32(memory,object+0xecu);
    if(!(flags&0x121u) && !child && visible){
        if(!attachment_effect_expansion(memory,object,s))return 0;
    }
    if(!(game_memory_read_u16(memory,object+0xe4u)&0x10u) && visible){
        descriptor=game_memory_read_u32(memory,pose->model+0x50u);count=game_memory_read_u32(memory,descriptor+0x14u);
        if(!count || count>INT32_MAX)return 0;
        index=(pose->identity>>6u)%count;
        if(!attachment_vertex(memory,pose,index,position) || !s->surface || !s->surface(s->context,position))return 0;
        if((pose->identity>>6u)&1u){index=(index+3u)%count;
            if(!attachment_vertex(memory,pose,index,position) || !s->surface(s->context,position))return 0;}
    }
    if((game_memory_read_u16(memory,object+0xe4u)&8u) &&
            game_memory_read_u32(memory,game_memory_read_u32(memory,0x628a90u)+0x468u) && !(game_memory_read_u32(memory,0x5ac6acu)&3u)){
        /* The sound service queries whether this voice is still playing before
         * requesting height; preserve that order inside the adapter. */
        if(!s->wind || !s->wind(s->context,pose->position,game_memory_read_u32(memory,object+0x104u)))return 0;
    }
    if(game_memory_read_u16(memory,object+0xe4u)&1u)return s->advance && s->advance(s->context);
    return !memory->fault;
}
int attachment_effect_emit(GameMemory *memory,GameAddress object,const AttachmentEmission *request,
        const AttachmentEmissionServices *services){
    PresentationPose *pose=presentation_instance_pose(object);uint16_t motion[3]={0};unsigned i;
    uint32_t seed,tick;long double height;float a,b,pitch;
    if(!pose || !request || !services || !services->place)return 0;
    tick=game_memory_read_u32(memory,0x5ac6acu);
    game_memory_write_u16(memory,object+0xd4u,request->mode);
    game_memory_write_u32(memory,object+0x108u,0u);game_memory_write_u32(memory,object+0x10cu,0u);
    game_memory_write_u16(memory,object+0x11cu,0xffffu);
    game_memory_write_u32(memory,object+0xd8u,tick);game_memory_write_u32(memory,object+0xdcu,request->duration);
    game_memory_write_u16(memory,object+0xe4u,request->flags);game_memory_write_u32(memory,object+0xe0u,request->source);
    game_memory_write_u32(memory,object+0x120u,request->deadline);
    if(request->flags&8u){
        motion[1]=emission_random(memory,0x577944u)<1u?(uint16_t)-2:2u;
        initialize_motion(memory,object,request,motion);
        if(memory->fault || !services->place(services->context,request,motion) || !services->height ||
            !services->height(services->context,request->position[0],request->position[2],&height))return 0;
        pitch=attachment_effect_pitch(memory,request->position[1],height);
        {uint32_t voice;
            if(!services->sound || !services->sound(services->context,pose->position,5,pitch,1u,1,&voice))return 0;
            game_memory_write_u32(memory,object+0x104u,voice);
        }
        {GameAddress player=game_memory_read_u32(memory,0x5f0208u);
            if((pose->identity&63u)==game_memory_read_u16(memory,player+8u) &&
                (int32_t)(game_memory_read_u32(memory,0x5ac6acu)-game_memory_read_u32(memory,player+0x58cu))>0x120){
                game_memory_write_u32(memory,player+0x58cu,game_memory_read_u32(memory,0x5ac6acu));
                player=game_memory_read_u32(memory,0x5f0208u);game_memory_write_u16(memory,player+0x598u,78u);
                if(!services->announcement || !services->announcement(services->context,
                    (int16_t)game_memory_read_u16(memory,game_memory_read_u32(memory,0x5f0208u)+0x598u)))return 0;
            }
        }
    }else{
        long double radius=(long double)pose->radius*pose->radius;
        seed=emission_random(memory,0x57794cu);
        a=(float)((long double)read_float(memory,0x577950u)/radius);
        b=(float)((long double)read_float(memory,0x577954u)/radius);
        for(i=0;i<3u;i++,seed>>=2u){
            uint32_t word;
            if(seed&1u){
                if(seed&2u)word=0u;
                else{word=truncate_word((long double)a*read_float(memory,0x577958u));if(i==2u)word=0u-word;}
            }else{
                word=truncate_word((long double)b*read_float(memory,0x577958u));if(!(seed&2u))word=0u-word;
            }
            motion[i]=(uint16_t)word;
        }
        initialize_motion(memory,object,request,motion);
        if(memory->fault || !services->place(services->context,request,motion))return 0;
    }
    if(game_memory_read_u8(memory,object+0xe5u)&0x10u){
        uint16_t flags=game_memory_read_u16(memory,object+0xe4u);
        if(emission_random(memory,0x57795cu))flags|=0x20u;else flags&=(uint16_t)~0x20u;
        game_memory_write_u16(memory,object+0xe4u,flags);
    }
    return !memory->fault;
}
