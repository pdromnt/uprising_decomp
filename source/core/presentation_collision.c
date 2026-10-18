#include "presentation_collision.h"
#include "presentation_instance.h"
#include "scene_projection_storage.h"
#include "gameplay_geometry.h"
#include "game_matrix.h"
#include <math.h>
#include <string.h>

typedef struct CollisionBody {
    float center[3],radius;
    int count;
    GameAddress hull,vertices;
    const float *positions;
    const ModelHullSphere *spheres;
} CollisionBody;
static float read_float(GameMemory *memory,GameAddress address){
    uint32_t bits=game_memory_read_u32(memory,address);float value;
    memcpy(&value,&bits,sizeof(value));return value;
}
/*419e60: player pickups use inclusive sphere overlap and publish the pickup's
 *own point. Other categories retain the shared world-hull collision program. */
int presentation_spawn_collision(GameMemory *m,GameAddress object,GameAddress other,float contact[3],int *hit){
    float p[3];long double d[3],radius,distance;unsigned i;
    if(game_memory_read_u8(m,other+12)!=0x20)return presentation_collision(m,object,other,0,NULL,contact,hit);
    for(i=0;i<3;i++){p[i]=read_float(m,object+24+i*4);d[i]=(long double)p[i]-read_float(m,other+24+i*4);}
    radius=(long double)read_float(m,object+0x38)+read_float(m,other+0x38);
    distance=(d[2]*d[2]+d[0]*d[0])+d[1]*d[1];
    if(m->fault)return 0;
    *hit=radius*radius>=distance;
    if(*hit)memcpy(contact,p,sizeof(p));
    return 1;
}
static int body_read(GameMemory *memory,GameAddress object,int controller,CollisionBody *body){
    const PresentationInstance *instance=presentation_instance_find(object);
    unsigned i;
    if(instance){
        const PresentationPose *pose=&instance->pose;
        memcpy(body->center,controller?pose->local_position:pose->position,sizeof(body->center));
        body->radius=pose->radius;
        body->count=(int16_t)pose->vertex_count;body->hull=pose->hull;
        body->vertices=controller?pose->controller:pose->vertices;
    }else{
        for(i=0u;i<3u;++i)body->center[i]=read_float(memory,object+(controller?0x64u:0x18u)+i*4u);
        body->radius=read_float(memory,object+0x38u);
        body->count=(int16_t)game_memory_read_u16(memory,object+0x44u);
        body->hull=game_memory_read_u32(memory,object+0x3cu);
        body->vertices=game_memory_read_u32(memory,object+(controller?0x70u:0x40u));
    }
    body->positions=body->count>0?scene_projection_storage_positions(body->vertices,(size_t)body->count):NULL;
    body->spheres=body->count>0?scene_projection_storage_hull(body->hull,(size_t)body->count):NULL;
    return !memory->fault;
}
static int sphere_read(GameMemory *memory,const CollisionBody *body,int index,float position[3],float *radius){
    unsigned i;
    if(!body->count){memcpy(position,body->center,12u);*radius=body->radius;return 1;}
    if(body->positions)memcpy(position,body->positions+index*3,12u);
    else for(i=0u;i<3u;++i)position[i]=read_float(memory,body->vertices+(uint32_t)index*12u+i*4u);
    *radius=body->spheres?body->spheres[index].radius:read_float(memory,body->hull+(uint32_t)index*28u+24u);
    return !memory->fault;
}
static long double distance_squared(const float a[3],const float b[3],int order){
    volatile long double x=(long double)a[0]-b[0],y=(long double)a[1]-b[1],z=(long double)a[2]-b[2];
    volatile long double sum=order==2?(x*x+z*z):order==1?(y*y+x*x):(y*y+z*z);
    return sum+(order==2?y*y:order==1?z*z:x*x);
}
static void sphere_contact(const float p[3],float radius,const float q[3],float other,float output[3]){
    volatile long double sum=(long double)radius+other;
    volatile long double ratio=(long double)radius/sum;
    unsigned i;
    for(i=0u;i<3u;++i){
        volatile float delta=(float)((long double)q[i]-p[i]);
        volatile float scaled=(float)((long double)delta*ratio);
        output[i]=(float)((long double)p[i]+scaled);
    }
}
static long double record_dot(const float normal[3],const float point[3]){
    return ((long double)normal[0]*point[0]+(long double)normal[1]*point[1])+(long double)normal[2]*point[2];
}
static int record_plane_test(GameMemory *memory,GameAddress record,const float point[3],float radius,
        float normal[3],float *distance,int *inside){
    unsigned axis,index,count=game_memory_read_u16(memory,record);long double separation;
    GameAddress normals=game_memory_read_u32(memory,record+0x24u),scalars=game_memory_read_u32(memory,record+0x2cu);
    *inside=0;
    for(axis=0u;axis<3u;axis++)normal[axis]=read_float(memory,record+0x18u+axis*4u);
    separation=record_dot(normal,point)-(long double)read_float(memory,record+0x28u);
    *distance=(float)separation;
    if(separation>(long double)radius)return !memory->fault;
    if(count<0x8000u)for(index=0u;index<count;index++){
        float side[3];long double offset;
        for(axis=0u;axis<3u;axis++)side[axis]=read_float(memory,normals+index*12u+axis*4u);
        offset=record_dot(side,point)-(long double)read_float(memory,scalars+index*4u);
        if(!(offset>=(long double)read_float(memory,0x573f8cu)))return !memory->fault;
    }
    *inside=1;return !memory->fault;
}
static void record_contact(const float point[3],const float normal[3],float distance,float padding,float contact[3]){
    unsigned axis;
    for(axis=0u;axis<3u;axis++){
        float shift=(float)(-(long double)distance*(long double)normal[axis]);
        float projection=(float)((long double)point[axis]+shift);
        float offset=(float)((long double)normal[axis]*padding);
        contact[axis]=(float)((long double)projection+offset);
    }
}
static int record_collision(GameMemory *memory,GameAddress first,GameAddress second,int controller,float contact[3],int *hit){
    CollisionBody a,b;GameAddress records;unsigned count,index;int sphere,inside,changed=0;float normal[3],distance,point[3],radius;long double combined;
    *hit=0;
    if(!body_read(memory,first,0,&a) || !body_read(memory,second,controller,&b))return 0;
    combined=(long double)a.radius+b.radius;
    if(!(combined*combined>=distance_squared(a.center,b.center,controller?1:2)))return 1;
    /* Retail needs at least one overlapping hull sphere; negative counts skip
     * queries. The subsequent plane query retains its own branch ordering. */
    if(b.count){
        for(sphere=0;sphere<b.count;sphere++){
            if(!sphere_read(memory,&b,sphere,point,&radius))return 0;
            combined=(long double)a.radius+radius;
            if(combined*combined>=distance_squared(point,a.center,0))break;
        }
        if(sphere>=b.count)return 1;
    }
    count=game_memory_read_u16(memory,first+0x64u);records=game_memory_read_u32(memory,first+0x68u);
    if(count>=0x8000u)return !memory->fault;
    for(index=0u;index<count;index++){
        GameAddress record=records+index*48u;
        if(!record_plane_test(memory,record,b.center,b.radius,normal,&distance,&inside))return 0;
        if(!inside)continue;
        record_contact(b.center,normal,distance,read_float(memory,0x573f94u),contact);
        changed=1;
        if(!b.count){*hit=1;return memory->fault?0:2;}
        for(sphere=0;sphere<b.count;sphere++){
            if(!sphere_read(memory,&b,sphere,point,&radius) || !record_plane_test(memory,record,point,radius,normal,&distance,&inside))return 0;
            if(!inside)continue;
            record_contact(point,normal,distance,40.0f,contact);
            *hit=1;return memory->fault?0:2;
        }
    }
    return memory->fault?0:changed?2:1;
}
int presentation_record_collision(GameMemory *memory,GameAddress first,GameAddress second,float contact[3],int *hit){
    return record_collision(memory,first,second,0,contact,hit);
}
/*482f20 uses the other's controller position/hull vertices; its extra
 *position argument is unused. Plane/contact ordering matches482bb0. */
int presentation_record_controller_collision(GameMemory *memory,GameAddress first,GameAddress second,float contact[3],int *hit){
    return record_collision(memory,first,second,1,contact,hit);
}
int presentation_collision(GameMemory *memory,GameAddress first,GameAddress second,int controller,
        const float previous[3],float contact[3],int *hit){
    CollisionBody a,b;int i,j,na,nb;float p[3],q[3],rp,rq;long double radius;
    *hit=0;
    if(!body_read(memory,first,controller==1,&a) || !body_read(memory,second,controller!=0,&b))return 0;
    radius=(long double)a.radius+b.radius;
    if(!(distance_squared(a.center,b.center,controller==1?1:2)<=radius*radius))return 1;
    na=a.count?a.count:1;nb=b.count?b.count:1;
    for(i=0;i<na;++i){
        if(!sphere_read(memory,&a,i,p,&rp))return 0;
        for(j=0;j<nb;++j){
            if(!sphere_read(memory,&b,j,q,&rq))return 0;
            radius=(long double)rp+rq;
            if(!(distance_squared(p,q,0)<=radius*radius))continue;
            /* Retail reverses the contact origin when only the first body
             * has hull spheres. Preserve that rounding and collision order. */
            if(a.count && !b.count)sphere_contact(q,rq,p,rp,contact);
            else sphere_contact(p,rp,q,rq,contact);
            if(controller){
                float delta[3],zero[3]={0};unsigned k;
                for(k=0u;k<3u;++k)delta[k]=(float)((long double)b.center[k]-a.center[k]);
                *hit=!(distance_squared(delta,zero,1)>=distance_squared(previous,a.center,0));
            }else *hit=1;
            return memory->fault?0:2;
        }
    }
    return !memory->fault;
}
