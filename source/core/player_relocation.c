#include "player_relocation.h"
#include "gameplay_spatial.h"
#include "presentation_instance.h"
#include <string.h>

static float read_float(GameMemory *memory,GameAddress address){
    uint32_t bits=game_memory_read_u32(memory,address);float value;
    memcpy(&value,&bits,sizeof(value));return value;
}
static int destination_inside(float value){
    int32_t bits;memcpy(&bits,&value,sizeof(bits));
    return bits>0 && bits<0x48000000;
}
static int spawn_probe(float position[3],float clearance,
        const PlayerSpawnServices *services,int *clear){
    float detail;long double terrain;volatile long double height;
    if(!services->height(services->context,position[0],position[2],&detail,&terrain))return 0;
    height=terrain+detail;height+=clearance;position[1]=(float)height;
    return services->clear(services->context,position,clear);
}
int player_spawn_choose(const float requested[3],float clearance,float ring_scale,
        const PlayerSpawnServices *services,float position[3]){
    uint32_t ring=1u,sectors=12u;int clear;
    memcpy(position,requested,3u*sizeof(float));
    if(!spawn_probe(position,clearance,services,&clear))return 0;
    if(clear)return 1;
    for(;;){
        float radius=(float)((long double)(int32_t)ring*ring_scale);
        uint16_t phase=0u,step;
        /* Retail's signed division and 16-bit phase produce thirteen samples
         * on the first ring (step170), rather than twelve uniform samples. */
        if(!sectors || sectors>2048u)return 0;
        step=(uint16_t)(2048u/sectors);
        do{
            volatile long double x=(long double)services->sine(services->context,phase&2047u)*radius;
            volatile long double z=(long double)services->sine(services->context,(phase+512u)&2047u)*radius;
            position[0]=(float)(x+requested[0]);position[2]=(float)(z+requested[2]);
            if(!spawn_probe(position,clearance,services,&clear))return 0;
            if(clear)return 1;
            phase=(uint16_t)(phase+step);
        }while((int16_t)phase<2048);
        ring++;sectors+=12u;
    }
}
int player_relocation_around(const float destination[2],float radius,
        int32_t angle,const PlayerRelocationServices *services,float position[2]){
    unsigned attempt;
    for(attempt=0u;attempt<3u;++attempt){
        unsigned phase=(uint16_t)angle&2047u;
        volatile long double x=(long double)services->sine(services->context,phase)*radius;
        volatile long double z=(long double)services->sine(services->context,(phase+512u)&2047u)*radius;
        position[0]=(float)(x+destination[0]);position[1]=(float)(z+destination[1]);
        if(destination_inside(position[0]) && destination_inside(position[1]))return 1;
        angle=(int32_t)(((uint32_t)angle+512u)&2047u);
    }
    return 0;
}
int player_relocation_choose(PresentationPose *pose,float radius_step,
        const PlayerRelocationServices *services,float position[3]){
    float destination[2],height_offset,radius=500.0f;long double height;int clear;
    if(!services->height(services->context,pose->position[0],pose->position[2],&height))return 0;
    height_offset=(float)((long double)pose->position[1]-height);
    if(!services->destination(services->context,destination))return 0;
    if(!destination_inside(destination[0]) || !destination_inside(destination[1])){
        memcpy(position,pose->position,3u*sizeof(float));return 1;
    }
    position[0]=destination[0];position[2]=destination[1];
    if(!services->height(services->context,position[0],position[2],&height))return 0;
    position[1]=(float)(height+height_offset);
    if(!services->clear(services->context,position,&clear))return 0;
    if(clear)return 1;
    for(;;){
        unsigned attempt;
        for(attempt=0u;attempt<20u;++attempt){
            int32_t angle;float horizontal[2];
            if(!services->destination(services->context,destination) ||
               !services->angle(services->context,&angle))return 0;
            /* Retail uses the helper's outputs even after its three trials
             * fail the world bounds check. The clearance query decides. */
            (void)player_relocation_around(destination,radius,angle,services,horizontal);
            position[0]=horizontal[0];position[2]=horizontal[1];
            if(!services->height(services->context,position[0],position[2],&height))return 0;
            position[1]=(float)(height+height_offset);
            if(!services->clear(services->context,position,&clear))return 0;
            if(clear)return 1;
        }
        radius=(float)((long double)radius+radius_step);
    }
}
int player_relocation_position_clear(GameMemory *memory,GameAddress manager,
        float radius,const float limits[3],const float position[3],int *clear){
    GameAddress heads[9],buckets[9];GameplaySpatialGrid *grid;unsigned bucket;
    volatile long double lower=(long double)radius*limits[0],upper;
    float rounded=(float)lower;
    *clear=0;
    if(lower>position[0] || !(position[2]>=rounded))return 1;
    upper=(long double)radius*limits[1];upper+=limits[2];rounded=(float)upper;
    if(!(position[0]<=upper) || position[2]>rounded)return 1;
    grid=gameplay_spatial_grid(manager);
    if(grid){if(!gameplay_spatial_query_heads(grid,position[0],position[2],heads))return 0;}
    else{
        if(!gameplay_spatial_query_values(manager,position[0],position[2],buckets))return 0;
        for(bucket=0u;bucket<9u;++bucket)heads[bucket]=game_memory_read_u32(memory,buckets[bucket]+4u);
    }
    for(bucket=0u;bucket<9u;++bucket){
        GameAddress entity=heads[bucket];
        while(entity){
            PresentationPose *pose=presentation_instance_pose(entity);float center[3];unsigned axis;
            volatile long double y,z,x,sum_radius,distance,squared_radius;
            if(pose)memcpy(center,pose->position,sizeof(center));
            else for(axis=0u;axis<3u;++axis)center[axis]=read_float(memory,entity+0x18u+axis*4u);
            y=(long double)position[1]-center[1];z=(long double)position[2]-center[2];x=(long double)position[0]-center[0];
            sum_radius=(long double)read_float(memory,entity+0x38u)+radius;
            y*=y;z*=z;x*=x;distance=y+z;distance+=x;squared_radius=sum_radius*sum_radius;
            if(memory->fault)return 0;
            if(squared_radius>=distance)return 1;
            entity=game_memory_read_u32(memory,entity+4u);
            if(memory->fault)return 0;
        }
    }
    if(memory->fault)return 0;
    *clear=1;return 1;
}
int entity_arrival_finish(EntityArrivalAction *action,const float position[3],
        const EntityArrivalServices *services){
    uint32_t identity;int32_t health;
    if(action->mode>2u)return 1;
    if(!services->identity(services->context,action->target,&identity))return 0;
    if(identity==action->identity){
        if(action->mode==0u){
            if(!services->clear_flag(services->context,action->target,0x200000u))return 0;
        }else if(action->mode==1u){
            if(!services->remove(services->context,action->target) ||
               !services->mark_removed(services->context,action->identity))return 0;
        }else{
            if(!services->health(services->context,action->target,&health))return 0;
            if(health && !services->move(services->context,action->target,position))return 0;
        }
    }
    action->target=0u;action->identity=0u;return 1;
}
