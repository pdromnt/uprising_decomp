#include "display_transition.h"
#include "portable_crt.h"
#include "gameplay_geometry.h"
#include "gameplay_state.h"
#include "runtime_utilities.h"
#include "game_state_ops.h"
#include <string.h>
#include <math.h>

static float camera_real(GameMemory *memory,GameAddress address){
    uint32_t bits=game_memory_read_u32(memory,address);float value;
    memcpy(&value,&bits,sizeof(value));return value;
}
static void camera_write_real(GameMemory *memory,GameAddress address,float value){
    uint32_t bits;memcpy(&bits,&value,sizeof(bits));game_memory_write_u32(memory,address,bits);
}
static void camera_coefficients(GameMemory *memory,GameAddress destination,
    const float p0[3],const float p1[3],const float p2[3],const float p3[3]){
    unsigned axis;
    for(axis=0;axis<3;axis++){
        long double start=(long double)p0[axis]*camera_real(memory,0x574398u);
        camera_write_real(memory,destination+axis*4u,(float)(((long double)p3[axis]-p2[axis])+p1[axis]-p0[axis]));
        camera_write_real(memory,destination+12u+axis*4u,(float)(((long double)p2[axis]-(long double)p1[axis]*camera_real(memory,0x57439cu))+start));
        camera_write_real(memory,destination+24u+axis*4u,(float)((long double)p1[axis]-start));
        camera_write_real(memory,destination+36u+axis*4u,p0[axis]);
    }
}
int game_camera_transition_begin(GameMemory *memory,GameAddress camera,
    const float start[3],const float target[3],uint16_t heading,float clearance){
    float points[9][3]={{0}};unsigned i,axis;uint16_t angle=(uint16_t)((heading+1024u)&2047u);
    for(axis=0;axis<3;axis++){
        camera_write_real(memory,camera+0x28u+axis*4u,start[axis]);
        camera_write_real(memory,camera+0x34u+axis*4u,target[axis]);
    }
    for(i=0;i<6;i++){
        unsigned point=i+i/2u+1u;
        long double radius=(long double)portable_crt_rand_step(memory)*camera_real(memory,0x574388u)+camera_real(memory,0x57438cu);
        float height_offset;
        points[point][0]=(float)((long double)camera_real(memory,0x6298a8u+(angle&2047u)*4u)*radius+target[0]);
        points[point][2]=(float)(radius*camera_real(memory,0x6298a8u+((angle+512u)&2047u)*4u)+target[2]);
        height_offset=(float)((long double)portable_crt_rand_step(memory)*camera_real(memory,0x574390u)-camera_real(memory,0x574394u));
        angle=(uint16_t)(angle+341u);
        points[point][1]=(float)((gameplay_geometry_terrain_height(memory,points[point][0],points[point][2])+height_offset)+clearance);
    }
    for(axis=0;axis<3;axis++){
        points[0][axis]=(float)(((long double)points[8][axis]+points[1][axis])*camera_real(memory,0x574358u));
        points[3][axis]=(float)(((long double)points[2][axis]+points[4][axis])*camera_real(memory,0x574358u));
        points[6][axis]=(float)(((long double)points[5][axis]+points[7][axis])*camera_real(memory,0x574358u));
    }
    for(i=0;i<9;i+=3){
        for(axis=0;axis<3;axis++){
            points[i+1][axis]=(float)((long double)points[i+1][axis]*camera_real(memory,0x574398u));
            points[i+2][axis]=(float)((long double)points[i+2][axis]*camera_real(memory,0x574398u));
        }
        camera_coefficients(memory,camera+0x40u+i*16u,points[i],points[i+1],points[i+2],points[(i+3)%9]);
    }
    for(i=0;i<9;i++)for(axis=0;axis<3;axis++)camera_write_real(memory,camera+0xd0u+i*12u+axis*4u,points[i][axis]);
    camera_coefficients(memory,camera+0x13cu,start,points[1],points[2],points[3]);
    game_memory_write_u32(memory,camera+0x16cu,1u);
    game_memory_write_u32(memory,camera+0x170u,game_memory_read_u32(memory,0x5acbe8u));
    return !memory->fault;
}
int game_camera_transition_position(GameMemory *memory,GameAddress camera){
    uint32_t bits=game_memory_read_u32(memory,0x5acbe8u)-game_memory_read_u32(memory,camera+0x170u);
    int32_t elapsed;int segment;float parameter;GameAddress coefficients;unsigned axis;
    memcpy(&elapsed,&bits,sizeof(elapsed));elapsed%=210;segment=elapsed/70;
    parameter=(float)((long double)(elapsed%70)*camera_real(memory,0x5743a0u));
    if(segment)game_memory_write_u32(memory,camera+0x16cu,0u);
    coefficients=game_memory_read_u32(memory,camera+0x16cu)?camera+0x13cu:camera+0x40u+(uint32_t)segment*48u;
    for(axis=0;axis<3;axis++){
        float value=(float)((long double)camera_real(memory,coefficients+axis*4u)*parameter);
        value=(float)((long double)camera_real(memory,coefficients+12u+axis*4u)+value);
        value=(float)((long double)value*parameter);
        value=(float)((long double)value+camera_real(memory,coefficients+24u+axis*4u));
        value=(float)((long double)value*parameter);
        value=(float)((long double)value+camera_real(memory,coefficients+36u+axis*4u));
        camera_write_real(memory,camera+12u+axis*4u,value);
    }
    return !memory->fault;
}
static uint32_t camera_integer(long double value){
    value=truncl(value);
    if(!isfinite(value) || value<-2147483648.0L || value>=2147483648.0L)return 0x80000000u;
    return (uint32_t)(int32_t)value;
}
int game_camera_activate(GameMemory *memory,GameAddress camera,GameAddress *request){
    GameplayActiveObjectPlan previous;GameAddress active;uint16_t mode;
    if(game_memory_read_u32(memory,0x624f68u))game_memory_write_u32(memory,0x608248u,0u);
    previous=gameplay_state_active_object_plan(memory);
    if(previous.active==previous.first_previous || previous.active==previous.second_previous)
        gameplay_state_archive_active_object(memory,previous.active);
    gameplay_state_publish_active_object(memory,camera);
    runtime_update_viewport_bounds(memory,game_memory_read_u16(memory,camera+4u)==1u);
    mode=gameplay_state_active_object_mode(memory,&active);
    if(mode==0u || mode==3u)active=gameplay_state_clear_active_object_request(memory,camera,mode);
    else active=(active&0xffff0000u)|mode;
    if(request)*request=active;
    return !memory->fault;
}
int game_camera_restore(GameMemory *memory,GameAddress *request){
    GameAddress camera=game_memory_read_u32(memory,0x58f894u);
    if(request)*request=0u;
    return !memory->fault && (!camera || game_camera_activate(memory,camera,request));
}
int game_camera_bind_scene(GameMemory *memory,GameAddress camera,GameAddress scene){
    if(game_memory_read_u32(memory,camera+0x40u)!=scene){
        game_memory_write_u16(memory,camera+0x38u,0u);
        game_memory_write_u32(memory,camera+0x3cu,
            game_memory_read_u8(memory,scene+12u)==0x20u?16u:0u);
        game_memory_write_u32(memory,camera+0x40u,scene);
    }
    return !memory->fault;
}
typedef struct DisplayPoint {uint16_t offset,x,y;} DisplayPoint;
static void display_points(GameMemory *memory,const DisplayPoint *points,unsigned count,uint32_t x,uint32_t y){
    unsigned i;for(i=0;i<count;i++){
        game_memory_write_u16(memory,0x600000u+points[i].offset,(uint16_t)(x+points[i].x));
        game_memory_write_u16(memory,0x600002u+points[i].offset,(uint16_t)(y+points[i].y));
    }
}
static void display_pair(GameMemory *memory,uint32_t offset,uint32_t x,uint32_t y){
    game_memory_write_u32(memory,0x600000u+offset,x);game_memory_write_u32(memory,0x600004u+offset,y);
}
int game_display_layout(GameMemory *memory,int32_t x,int32_t y,int alternate){
    static const DisplayPoint regular_low[]={
        {0x3f90,0,148},{0x34b8,221,136},{0x3a28,0,140},{0x3478,102,105},
        {0x3678,88,65},{0x3718,24,0},{0x3330,201,104},{0x3608,203,66},
        {0x3730,223,0},{0x4088,97,47},{0x3308,2,0},{0x3490,137,172},
        {0x34a4,165,172},{0x38c0,285,172},{0x4010,15,165}};
    static const DisplayPoint regular_high[]={
        {0x32c0,0,0},{0x3818,0,332},{0x32e0,10,0},{0x34a4,323,373},
        {0x3490,273,373},{0x38c0,584,427},{0x4010,26,356}};
    static const DisplayPoint alternate_low[]={
        {0x4100,0,0},{0x40c0,0,174},{0x3888,244,28},{0x3308,2,153}};
    static const DisplayPoint alternate_high[]={
        {0x4068,0,380},{0x3cf8,513,0},{0x40e8,0,0},{0x32e0,16,400}};
    uint32_t ux=(uint32_t)x,uy=(uint32_t)y,half=game_memory_read_u32(memory,0x6034d0u)>>1u;
    int high=game_memory_read_u32(memory,0x597f80u)!=0u;unsigned i;
    GameAddress center,constants;float cx,cy;
    int active=game_memory_read_u32(memory,0x58f89cu)==game_memory_read_u32(memory,0x58f890u) &&
        game_memory_read_u32(memory,0x59d440u)==0x3f800000u;
    game_memory_write_u32(memory,0x603848u,ux);game_memory_write_u32(memory,0x603844u,uy);
    if(alternate){
        if(high)display_points(memory,alternate_high,sizeof(alternate_high)/sizeof(*alternate_high),ux,uy);
        else display_points(memory,alternate_low,sizeof(alternate_low)/sizeof(*alternate_low),ux,uy);
        for(i=0;i<6u;i++){
            GameAddress first=0x603930u+(high?0x80u:0u)+i*20u;
            GameAddress second=0x603768u-(high?0xd8u:0u)+i*20u;
            game_memory_write_u16(memory,first,(uint16_t)(ux+(high?74u:62u)));
            game_memory_write_u16(memory,first+2u,(uint16_t)(uy+(high?327u:139u)));
            game_memory_write_u16(memory,second,(uint16_t)(ux+(high?376u:209u)));
            game_memory_write_u16(memory,second+2u,(uint16_t)(uy+(high?326u:140u)));
        }
    }else if(high){
        display_points(memory,regular_high,sizeof(regular_high)/sizeof(*regular_high),ux,uy);
        game_memory_write_u16(memory,0x6032e2u,(uint16_t)(uy+(game_memory_read_u32(memory,0x597f88u) && active?312u:440u)));
        game_memory_write_u32(memory,0x60389cu,332u);
        {static const DisplayPoint fixed[]={{0x3830,288,214},{0x3620,291,211}};display_points(memory,fixed,2u,0u,0u);}
        display_pair(memory,0x3328u,ux+586u,uy+378u);
        display_pair(memory,0x3748u,ux+587u,uy+404u);
        game_memory_write_u32(memory,0x604080u,ux+154u);game_memory_write_u32(memory,0x60407cu,uy+438u);
        game_memory_write_u32(memory,0x603754u,uy+419u);game_memory_write_u32(memory,0x603750u,ux+599u);
        display_pair(memory,0x3918u,ux+603u,uy+404u);display_pair(memory,0x3920u,ux+615u,uy+419u);
        display_pair(memory,0x32f8u,259u,439u);display_pair(memory,0x3300u,374u,439u);
    }else{
        display_points(memory,regular_low,sizeof(regular_low)/sizeof(*regular_low),ux,uy);
        game_memory_write_u16(memory,0x60330au,(uint16_t)(uy+((int32_t)game_memory_read_u32(memory,0x597f88u)>1 && active?123u:175u)));
        game_memory_write_u32(memory,0x60389cu,148u);
        {static const DisplayPoint fixed[]={{0x3f60,129,74},{0x3ff8,130,70}};display_points(memory,fixed,2u,0u,0u);}
        display_pair(memory,0x3328u,ux+18u,uy+146u);display_pair(memory,0x3748u,ux+288u,uy+159u);
        game_memory_write_u32(memory,0x604080u,ux+88u);game_memory_write_u32(memory,0x60407cu,uy+176u);
        game_memory_write_u32(memory,0x603754u,uy+168u);game_memory_write_u32(memory,0x603750u,ux+296u);
        display_pair(memory,0x3918u,ux+299u,uy+159u);display_pair(memory,0x3920u,ux+307u,uy+168u);
        display_pair(memory,0x32f8u,146u,166u);display_pair(memory,0x3300u,173u,166u);
    }
    center=high?0x604118u:0x6040e0u;constants=0x5745c8u+(high?8u:0u)+(alternate?16u:0u);
    cx=(float)((long double)x+camera_real(memory,constants));cy=(float)((long double)y+camera_real(memory,constants+4u));
    camera_write_real(memory,center,cx);camera_write_real(memory,center+4u,cy);
    game_memory_write_u32(memory,0x60392cu,camera_integer(cx)-half);
    game_memory_write_u32(memory,0x6038d8u,camera_integer(cy)-half);
    return !memory->fault;
}
int game_display_button_rectangles(GameMemory *memory){
    int high=game_memory_read_u32(memory,0x597f80u)!=0u;
    int alternate=game_memory_read_u32(memory,0x6038a0u)!=0u;
    game_state_build_span_table(memory,high,alternate);
    return !memory->fault;
}
int game_display_circle_template(GameMemory *memory,GameAddress output,uint32_t diameter){
    int32_t x,y,half;uint32_t offset=0u;
    if(diameter>4096u)return 0;
    half=(int32_t)(diameter/2u);
    for(y=-half;y<half;y++)for(x=-half;x<half;x++)
        game_memory_write_u8(memory,output+offset++,x*x+y*y>half*half?0u:255u);
    return !memory->fault;
}
void game_display_sample_scalar(GameMemory *memory,uint32_t bits){
    uint32_t reference=game_memory_read_u32(memory,0x62792cu);float value;memcpy(&value,&bits,4u);
    game_memory_write_u32(memory,0x626e4cu,bits);
    game_memory_write_u32(memory,0x597f5cu,reference-camera_integer(value));
}
int game_display_change_runtime_mode(GameMemory *memory,uint32_t mode,GameAddress *external_refresh){
    GameDisplayScalarPlan plan;uint32_t alternate=game_memory_read_u32(memory,0x6038a0u);
    game_memory_write_u32(memory,0x597f88u,(int32_t)mode<=1?mode:1u);
    if(!game_display_layout(memory,0,0,alternate!=0u) || !game_display_select_transition_scalar(memory,&plan))return 0;
    game_display_sample_scalar(memory,plan.scalar);
    *external_refresh=game_memory_read_u32(memory,0x5840f0u) && !alternate?
        (game_memory_read_u32(memory,0x597f88u)?0x4d84b0u:0x4d81c0u):0u;
    return !memory->fault;
}
int game_display_configure_control(GameMemory *memory,uint32_t mode,const GameDisplayMaskServices *services){
    GameDisplayScalarPlan plan;uint32_t diameter;int high=game_memory_read_u32(memory,0x597f80u)!=0u;
    game_memory_write_u32(memory,0x6038a0u,mode);
    if(!mode)game_memory_write_u32(memory,0x6037e8u,high?30u:19u);
    diameter=high?128u:mode?70u:56u;game_memory_write_u32(memory,0x6034d0u,diameter);
    if(!services || !services->resize || !services->resize(services->context,diameter))return 0;
    if(!game_display_layout(memory,0,0,mode!=0u) || !game_display_select_transition_scalar(memory,&plan))return 0;
    if(!plan.bypass)game_display_sample_scalar(memory,plan.scalar);
    return game_display_button_rectangles(memory);
}
static uint16_t camera_direction(GameMemory *memory,float x,float z){
    long double scale=camera_real(memory,0x573538u),index;uint16_t angle;
    uint32_t xb,zb;memcpy(&xb,&x,4u);memcpy(&zb,&z,4u);
    if(fabsl((long double)x)<fabsl((long double)z)){
        index=((long double)x*scale)/z+scale;
        angle=game_memory_read_u16(memory,0x62e8b0u+camera_integer(index)*2u);
        return (int32_t)zb>0?(uint16_t)(angle&2047u):(uint16_t)(angle+1024u);
    }
    index=((long double)z*scale)/x+scale;
    angle=game_memory_read_u16(memory,0x62e8b0u+camera_integer(index)*2u);
    return (uint16_t)(((int32_t)xb>0?512u:1536u)-angle);
}
int game_camera_transition_aim(GameMemory *memory,GameAddress camera){
    float delta[3],length_squared,ratio;long double elevation;uint32_t x,z,bits,index;uint16_t pitch;unsigned axis;
    x=game_memory_read_u32(memory,camera+12u);z=game_memory_read_u32(memory,camera+20u);
    if((int32_t)x>0 && (int32_t)x<(int32_t)0x48000000u &&
       (int32_t)z>0 && (int32_t)z<(int32_t)0x48000000u){
        long double minimum=gameplay_geometry_terrain_height(memory,camera_real(memory,camera+12u),camera_real(memory,camera+20u))+camera_real(memory,0x57436cu);
        if(minimum>camera_real(memory,camera+16u))camera_write_real(memory,camera+16u,(float)minimum);
    }
    for(axis=0;axis<3;axis++)delta[axis]=(float)((long double)camera_real(memory,camera+0x34u+axis*4u)-camera_real(memory,camera+12u+axis*4u));
    game_memory_write_u16(memory,camera+6u,camera_direction(memory,delta[0],delta[2]));
    length_squared=(float)(((long double)delta[1]*delta[1]+(long double)delta[2]*delta[2])+(long double)delta[0]*delta[0]);
    elevation=(long double)delta[1]/sqrtl(length_squared);ratio=(float)elevation;
    memcpy(&bits,&ratio,4u);
    if(elevation>camera_real(memory,0x5743a4u)){
        index=camera_integer(((long double)ratio-camera_real(memory,0x5743a4u))*camera_real(memory,0x5743a8u));
        pitch=game_memory_read_u16(memory,0x62b8a8u+index*2u);
    }else if(bits>0xbf666666u){
        index=camera_integer(((long double)camera_real(memory,0x5743acu)-ratio)*camera_real(memory,0x5743a8u));
        pitch=(uint16_t)(0u-game_memory_read_u16(memory,0x62b8a8u+index*2u));
    }else{
        index=camera_integer((long double)ratio*camera_real(memory,0x5743b0u)+camera_real(memory,0x5743b0u));
        pitch=game_memory_read_u16(memory,0x62f8b8u+index*2u);
    }
    game_memory_write_u16(memory,camera+8u,(uint16_t)(0u-pitch));
    return !memory->fault;
}

int game_display_select_transition_scalar(GameMemory *memory,
    GameDisplayScalarPlan *plan) {
    uint32_t mode;
    uint32_t current;
    uint32_t saved;
    uint32_t enabled;
    uint32_t renderer;
    uint32_t candidate;
    if (!memory || !plan) return 0;
    mode = game_memory_read_u32(memory, 0x00597f88u);
    if (memory->fault) return 0;
    plan->scalar = 0u;
    plan->eax = mode;
    plan->ecx = 0u;
    plan->left = mode;
    plan->right = 1u;
    plan->flags = GAME_DISPLAY_SCALAR_COMPARE;
    plan->bypass = (int32_t)mode <= 1;
    plan->has_ecx = 0;
    if (plan->bypass) return 1;

    current = game_memory_read_u32(memory, 0x0058f89cu);
    if (memory->fault) return 0;
    saved = game_memory_read_u32(memory, 0x0058f890u);
    if (memory->fault) return 0;
    plan->eax = saved;
    plan->ecx = current;
    plan->left = current;
    plan->right = saved;
    plan->has_ecx = 1;
    if (current != saved) return 1;

    enabled = game_memory_read_u32(memory, 0x0059d440u);
    if (memory->fault) return 0;
    plan->eax = enabled;
    plan->left = enabled;
    plan->right = 0x3f800000u;
    if (enabled != plan->right) return 1;

    renderer = game_memory_read_u32(memory, 0x006038a0u);
    if (memory->fault) return 0;
    candidate = game_memory_read_u32(memory, 0x00597f80u);
    if (memory->fault) return 0;
    plan->eax = candidate;
    plan->left = candidate;
    plan->right = candidate;
    plan->flags = GAME_DISPLAY_SCALAR_TEST;
    plan->scalar = renderer ? 0x41800000u :
        (candidate ? 0x42c60000u : 0x424c0000u);
    return 1;
}
