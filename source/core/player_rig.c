#include "player_rig.h"
#include "gameplay_geometry.h"
#include "game_matrix.h"
#include "scene_instance_owner.h"
#include "presentation_instance.h"
#include "game_state_ops.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static void *rig_segment(PlayerRig *s,uint32_t offset,uint32_t *begin,size_t *length){
#define FIELD(at, member) do { if(offset-(at)<sizeof(s->member)){*begin=(at);*length=sizeof(s->member);return &s->member;} } while(0)
    if(offset<0x160u){
        FIELD(0x148u,motion.steering);FIELD(0x14eu,motion.steering_padding);
        FIELD(0x150u,motion.forward_scale);FIELD(0x154u,motion.strafe_scale);
        FIELD(0x158u,motion.turn_scale);FIELD(0x15cu,motion.turret_scale);
        return NULL;
    }
    if(offset>=0x484u){FIELD(0x484u,motion.slope_drag);FIELD(0x488u,motion.speed_drag);return NULL;}
    if(offset>=0x2ccu && offset<0x2fcu){
        FIELD(0x2ccu,motion.lean);FIELD(0x2ceu,motion.filtered_roll);
        FIELD(0x2d0u,motion.surface_normal);FIELD(0x2e8u,motion.engine_scalar);
        FIELD(0x2f8u,motion.idle_ticks);FIELD(0x2fau,motion.steer_ticks);
        return NULL;
    }
    FIELD(0x16cu,ground_corners);FIELD(0x424u,terrain_ring);
    FIELD(0x19cu,controller_velocity);
    FIELD(0x1a8u,motion.yaw_step);FIELD(0x1aau,motion.yaw_padding);
    FIELD(0x228u,turret_model);FIELD(0x22cu,turret_part);FIELD(0x22eu,turret_padding);
    FIELD(0x230u,inline_turret);FIELD(0x260u,turret_origin);
    FIELD(0x270u,mount_model);FIELD(0x274u,mount_part);FIELD(0x276u,mount_padding);
    FIELD(0x278u,inline_mount);FIELD(0x2a8u,mount_origin);FIELD(0x2b4u,pivot);
    FIELD(0x2c0u,smoothed_heading);FIELD(0x2c2u,smoothing_padding);FIELD(0x2c8u,aim_offset);
#undef FIELD
    if(offset>=0x308u && offset<0x3e8u){
        uint32_t relative=offset-0x308u,index=relative/28u,field=relative%28u;
        PlayerPoseChannel *channel=&s->channels[index];*begin=0x308u+index*28u;
        if(field<8u){*length=8u;return channel->samples;}
        if(field<16u){*begin+=8u;*length=8u;return channel->coefficients;}
        if(field<24u){*begin+=16u;*length=8u;return channel->ticks;}
        *begin+=24u;*length=4u;return &channel->active;
    }
    return NULL;
}
static int rig_transfer(PlayerRig *s,uint32_t offset,void *bytes,size_t size,int writing){
    uint32_t check=offset;size_t left=size;uint8_t *cursor=bytes;
    if(!s || !bytes)return 0;
    while(left){uint32_t begin;size_t length,step;
        if(!rig_segment(s,check,&begin,&length))return 0;
        step=length-(check-begin);if(step>left)step=left;
        check+=(uint32_t)step;left-=step;
    }
    while(size){uint32_t begin;size_t length,step;uint8_t *field=rig_segment(s,offset,&begin,&length);
        step=length-(offset-begin);if(step>size)step=size;
        if(writing)memcpy(field+offset-begin,cursor,step);else memcpy(cursor,field+offset-begin,step);
        offset+=(uint32_t)step;cursor+=step;size-=step;
    }return 1;
}
int player_rig_projection_read(PlayerRig *s,uint32_t offset,void *out,size_t size){return rig_transfer(s,offset,out,size,0);}
int player_rig_projection_write(PlayerRig *s,uint32_t offset,const void *in,size_t size){
    GameAddress mount=s?s->mount_model:0u,turret=s?s->turret_model:0u;
    int ok=rig_transfer(s,offset,(void *)in,size,1);
    if(ok && mount!=s->mount_model)s->mount_instance=scene_instance_owner_find(s->mount_model);
    if(ok && turret!=s->turret_model)s->turret_instance=scene_instance_owner_find(s->turret_model);
    return ok;
}
int player_rig_import(GameMemory *m,GameAddress scene,PlayerRig *s){
    uint32_t offset;memset(s,0,sizeof(*s));
    for(offset=0x148u;offset<0x48cu;){uint32_t begin;size_t length,done=0;uint8_t *field=rig_segment(s,offset,&begin,&length);
        if(!field){++offset;continue;}
        while(done<length){size_t step=length-done;if(step>4u)step=4u;
            if(!game_memory_read(m,scene+offset+(uint32_t)done,field+done,step))return 0;
            done+=step;
        }offset+=(uint32_t)length;
    }
    s->mount_instance=scene_instance_owner_find(s->mount_model);
    s->turret_instance=scene_instance_owner_find(s->turret_model);
    return !m->fault;
}
static uint32_t rig_bits(float value){uint32_t bits;memcpy(&bits,&value,4u);return bits;}
static float rig_float(GameMemory *m,GameAddress address){uint32_t bits=game_memory_read_u32(m,address);float value;memcpy(&value,&bits,4u);return value;}
int player_rig_construct(GameMemory *m,GameAddress object,GameAddress source,
        const PlayerRigDefinition *definition){
    PresentationPose *pose;PlayerRig *rig,*source_rig;PlayerControls *controls;
    unsigned i,j;float sine,cosine,radius;
    game_state_initialize_descriptor(m,object+0x13cu);
    if(m->fault || !presentation_instance_enable_player(m,object))return 0;
    pose=presentation_instance_pose(object);rig=presentation_instance_player_rig(object);
    controls=presentation_instance_player_controls(object);
    if(!pose || !rig || !controls)return 0;
    game_memory_write_u32(m,object,0x00574e70u);
    game_memory_write_u32(m,object+0x210u,0u);controls->queue_methods=0x00574e50u;
    if(source){
        source_rig=presentation_instance_player_rig(source);
        pose->flags=game_memory_read_u32(m,source+16u);
        game_memory_write_u8(m,object+12u,game_memory_read_u8(m,source+12u));
        for(i=0u;i<4u;++i)for(j=0u;j<3u;++j)
            rig->ground_corners[i][j]=source_rig?source_rig->ground_corners[i][j]:
                rig_float(m,source+0x16cu+i*12u+j*4u);
        rig->mount_part=source_rig?source_rig->mount_part:game_memory_read_u16(m,source+0x274u);
        rig->turret_part=source_rig?source_rig->turret_part:game_memory_read_u16(m,source+0x22cu);
        for(i=0u;i<3u;++i){
            rig->mount_origin[i]=source_rig?source_rig->mount_origin[i]:rig_float(m,source+0x2a8u+i*4u);
            rig->turret_origin[i]=source_rig?source_rig->turret_origin[i]:rig_float(m,source+0x260u+i*4u);
            rig->pivot[i]=source_rig?source_rig->pivot[i]:rig_float(m,source+0x2b4u+i*4u);
        }
    }else{
        const PresentationInstance *instance=presentation_instance_find(object);
        long double first=rig_float(m,0x574d34u),second=rig_float(m,0x574d38u);
        float a=(float)((long double)instance->definition.dimensions[0]*first);
        float b=-instance->definition.dimensions[1];
        float c=(float)((long double)instance->definition.dimensions[2]*first);
        float d=(float)((long double)instance->definition.dimensions[0]*second);
        float e=(float)((long double)instance->definition.dimensions[2]*second);
        pose->flags|=8u;game_memory_write_u8(m,object+12u,32u);
        controls->input_flags=controls->previous_input_flags=0u;
        for(i=0u;i<4u;++i){rig->ground_corners[i][0]=(i==0u || i==3u)?a:d;
            rig->ground_corners[i][1]=b;rig->ground_corners[i][2]=i<2u?c:e;}
        rig->mount_part=definition->mount_part;rig->turret_part=definition->turret_part;
        memcpy(rig->mount_origin,definition->mount_origin,sizeof(rig->mount_origin));
        memcpy(rig->turret_origin,definition->turret_origin,sizeof(rig->turret_origin));
        memcpy(rig->pivot,definition->pivot,sizeof(rig->pivot));
    }
    rig->mount_model=game_memory_read_u32(m,pose->model+0x4cu);
    rig->turret_model=game_memory_read_u32(m,rig->mount_model+0x4cu);
    rig->mount_instance=scene_instance_owner_find(rig->mount_model);
    rig->turret_instance=scene_instance_owner_find(rig->turret_model);
    memset(controls->weapons,0,sizeof(controls->weapons));
    controls->selected[0]=controls->selected[1]=0u;
    controls->selection[0]=0u;controls->selection[1]=1u;
    controls->feedback[0]=controls->feedback[1]=0u;
    sine=rig_float(m,0x629ca8u);cosine=rig_float(m,0x62a4a8u);
    radius=rig_float(m,object+0x38u);
    rig->terrain_ring[0][0]=rig->terrain_ring[0][1]=0.0f;
    rig->terrain_ring[0][2]=radius;
    for(i=1u;i<8u;++i){float x=rig->terrain_ring[i-1u][0],z=rig->terrain_ring[i-1u][2];
        rig->terrain_ring[i][0]=(float)((long double)x*cosine+(long double)z*sine);
        rig->terrain_ring[i][1]=rig->terrain_ring[i-1u][1];
        rig->terrain_ring[i][2]=(float)((long double)x*(-sine)+(long double)z*cosine);
    }
    game_memory_write_u32(m,object+0x49cu,1u);game_memory_write_u32(m,object+0x4a0u,1u);
    return !m->fault;
}
static float rig_trig(GameMemory *m,unsigned angle,unsigned shift){return rig_float(m,0x006298a8u+((angle+shift)&2047u)*4u);}
static uint32_t rig_truncate(long double value){
    if(!isfinite(value) || value<(long double)INT64_MIN || value>=(long double)INT64_MAX)return 0x80000000u;
    return (uint32_t)(int64_t)value;
}
static void rig_channel_rebuild(PlayerPoseChannel *channel){
    int32_t delta[2];float slope[2],intercept[2];unsigned i;
    for(i=0u;i<2u;++i)delta[i]=(int32_t)(channel->ticks[i]-channel->ticks[channel->active]);
    for(i=0u;i<2u;++i){
        int32_t denominator=(int32_t)((uint32_t)delta[i]-(uint32_t)delta[1u-i]);
        slope[i]=(float)((long double)channel->samples[i]/denominator);
        intercept[i]=(float)(0.0L-(long double)slope[i]*(float)delta[1u-i]);
    }
    channel->coefficients[0]=(float)((long double)slope[0]+slope[1]);
    channel->coefficients[1]=(float)((long double)intercept[0]+intercept[1]);
}
static int rig_channel_record(GameMemory *m,PlayerPoseChannel *channel,float value,
        uint32_t tick,int reset,int wrapping){
    unsigned next,i;
    if(reset){
        channel->active=0;channel->ticks[0]=tick-1u;channel->ticks[1]=tick;
        channel->samples[0]=channel->samples[1]=value;
    }else{
        if(channel->active<0 || channel->active>1){m->fault=1;return 0;}
        next=(unsigned)channel->active^1u;
        if(wrapping){
            long double difference=(long double)value-channel->samples[next];
            float stored_difference=(float)difference;
            float threshold=rig_float(m,0x572bdcu),period=rig_float(m,0x572be0u);
            if(difference>(long double)threshold){
                for(i=0u;i<2u;++i)channel->samples[i]=(float)((long double)channel->samples[i]+period);
            }else if(rig_bits(stored_difference)>0xc4800000u){
                for(i=0u;i<2u;++i)channel->samples[i]=(float)((long double)channel->samples[i]-period);
            }
        }
        if(channel->ticks[next]==tick)channel->samples[next]=value;
        else{channel->samples[channel->active]=value;channel->ticks[channel->active]=tick;channel->active=(int32_t)next;}
    }
    rig_channel_rebuild(channel);return !m->fault;
}
int player_rig_record_pose(GameMemory *m,PresentationPose *pose,PlayerControls *controls,
        PlayerRig *rig,int reset){
    unsigned i;
    for(i=0u;i<8u;++i){float value;int wrapping=i==3u || i==6u;
        uint32_t tick=game_memory_read_u32(m,0x5ac6acu);
        if(i<3u)value=pose->position[i];
        else{
            uint16_t angle=i<6u?pose->angles[i-3u]:i==6u?controls->heading:controls->aim;
            value=wrapping?(float)(reset?(int32_t)(angle&2047u):(int32_t)(int16_t)angle):(float)(int16_t)angle;
        }
        if(m->fault || !rig_channel_record(m,&rig->channels[i],value,tick,reset,wrapping))return 0;
    }return 1;
}
int player_rig_sample_pose(GameMemory *m,PresentationPose *pose,PlayerControls *controls,
        const PlayerRig *rig){
    unsigned i;
    for(i=0u;i<8u;++i){const PlayerPoseChannel *channel=&rig->channels[i];
        uint32_t tick=game_memory_read_u32(m,0x5ac6acu),integer;uint16_t angle;
        float delta;long double value;
        if(m->fault || channel->active<0 || channel->active>1){m->fault=1;return 0;}
        delta=(float)(int32_t)(tick-channel->ticks[channel->active]);
        value=(long double)channel->coefficients[0]*delta+channel->coefficients[1];
        if(i<3u){pose->position[i]=(float)value;continue;}
        integer=rig_truncate(value);angle=(uint16_t)integer&2047u;
        if(i!=3u && i!=6u && angle>1024u)angle=(uint16_t)(angle-2048u);
        if(i<6u)pose->angles[i-3u]=angle;
        else if(i==6u)controls->heading=angle;else controls->aim=angle;
    }return 1;
}
static uint16_t rig_bearing(GameMemory *m,const float composed[12]){
    float y=composed[5],z=composed[6];long double scale=rig_float(m,0x00574d94u);
    uint32_t index;uint16_t angle;
    /* Unordered follows the first retail x87 branch; quadrant selection tests
     * signed float bits, retaining the distinction between positive/negative zero. */
    if(!(fabsf(y)<=fabsf(z))){
        index=rig_truncate(((long double)z*scale)/(long double)y+scale);
        angle=game_memory_read_u16(m,0x0062e8b0u+index*2u);
        return (int32_t)rig_bits(y)>0?(uint16_t)(angle&2047u):(uint16_t)(angle+1024u);
    }
    angle=(int32_t)rig_bits(z)>0?512u:1536u;
    index=rig_truncate(((long double)y*scale)/(long double)z+scale);
    return (uint16_t)(angle-game_memory_read_u16(m,0x0062e8b0u+index*2u));
}
static void rig_smooth(GameMemory *m,const PresentationPose *pose,PlayerRig *rig){
    float threshold=rig_float(m,0x00574d40u);int32_t current,target,step,updated;
    if(!(fabsf(rig->controller_velocity[0])>threshold) && !(fabsf(rig->controller_velocity[2])>threshold))return;
    current=(int16_t)rig->smoothed_heading;target=(int16_t)pose->local_angles[1];
    if(current==target)return;
    step=(target<0?-target:target)<39?2:1;
    if(current<target){updated=current+step;if(updated>target)updated=target;}
    else{updated=current-step;if(updated<target)updated=target;}
    rig->smoothed_heading=(uint16_t)updated;
}
static int rig_publish(GameMemory *m,GameAddress model,SceneInstance *owned,const float matrix[12]){
    unsigned i;
    if(owned){
        memcpy(owned->local,matrix,sizeof(owned->local));
        return scene_projection_storage_project_holder(owned);
    }
    for(i=0;i<12u && !m->fault;++i)game_memory_write_u32(m,model+4u+i*4u,rig_bits(matrix[i]));
    return !m->fault;
}
static void rig_rendered_body(GameMemory *m,const PresentationPose *pose,float body[12]){
    long double ca=rig_trig(m,pose->local_angles[2],512u),cb=rig_trig(m,pose->local_angles[1],512u);
    long double cc=rig_trig(m,pose->local_angles[0],512u),sa=rig_trig(m,pose->local_angles[2],0u);
    long double sc=rig_trig(m,pose->local_angles[0],0u),sb=rig_trig(m,pose->local_angles[1],0u);
    long double product=sc*sa;float saved_product=(float)product;
    /* The rendered builder rounds SC*SA; the inline builder rounds SC*SB.
     * Their stored matrices agree geometrically but these schedules differ. */
    body[0]=(float)(cc*ca+product*sb);
    body[1]=(float)((sa*sb)*cc-sc*ca);body[2]=(float)(sa*cb);
    body[4]=(float)(sc*cb);body[5]=(float)(cc*cb);body[6]=(float)-sb;
    body[8]=(float)((sc*sb)*ca-sa*cc);
    body[9]=(float)((sb*cc)*ca+(long double)saved_product);body[10]=(float)(cb*ca);
    body[3]=pose->local_position[0];body[7]=pose->local_position[1];body[11]=pose->local_position[2];
}
int player_rig_build(GameMemory *m,PresentationPose *pose,PlayerControls *controls,PlayerRig *rig,int rendered){
    float body[12],mount[12]={0},turret[12]={0},composed[12],sine,cosine;
    uint16_t angles[3],bearing,heading;unsigned i;long double px,pz,aim;
    if(!m || !pose || !controls || !rig)return 0;
    for(i=0;i<3u;++i)angles[i]=rendered?pose->local_angles[2u-i]:pose->angles[i];
    if(rendered)rig_rendered_body(m,pose,body);
    else gameplay_geometry_orientation_values(m,angles,pose->position,body);
    if(m->fault)return 0;
    if(rendered){if(!rig_publish(m,pose->model,pose->model_instance,body))return 0;}
    else memcpy(pose->world_matrix,body,sizeof(body));
    heading=rendered?controls->previous_heading:controls->heading;
    sine=rig_trig(m,heading,0u);cosine=rig_trig(m,heading,512u);
    mount[0]=mount[10]=cosine;mount[2]=sine;mount[8]=-sine;mount[5]=1.0f;
    mount[3]=rig->mount_origin[0];mount[7]=rig->mount_origin[1];mount[11]=rig->mount_origin[2];
    if(rendered){if(!rig_publish(m,rig->mount_model,rig->mount_instance,mount))return 0;}
    else memcpy(rig->inline_mount,mount,sizeof(mount));
    game_matrix_compose_values(composed,body,mount);bearing=rig_bearing(m,composed);
    if(m->fault)return 0;
    if(rendered && game_memory_read_u32(m,0x005ac6b0u))rig_smooth(m,pose,rig);
    heading=(uint16_t)(bearing+(rendered?controls->previous_aim+rig->smoothed_heading:controls->aim));
    sine=rig_trig(m,heading,0u);cosine=rig_trig(m,heading,512u);
    turret[0]=1.0f;turret[5]=turret[10]=cosine;turret[6]=-sine;turret[9]=sine;
    px=rig->pivot[1];pz=rig->pivot[2];aim=rendered?rig->aim_offset:0.0L;
    turret[3]=rig->turret_origin[0];
    turret[7]=(float)((((pz+aim)*(long double)sine-px*(long double)cosine)+rig->turret_origin[1])+px);
    turret[11]=(float)(((-px*(long double)sine-(pz+aim)*(long double)cosine)+rig->turret_origin[2])+pz);
    if(m->fault)return 0;
    if(rendered)return rig_publish(m,rig->turret_model,rig->turret_instance,turret);
    memcpy(rig->inline_turret,turret,sizeof(turret));return 1;
}
