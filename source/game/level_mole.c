#include "level_objects.h"
#include "level_effect_runtime.h"
#include "presentation_instance.h"
#include "gameplay_geometry.h"
#include "mesh_buffer.h"
#include "menu_sound.h"
#include "level_player_resources.h"
#include "model_assets.h"
#include "game_matrix.h"
#include "effect_initialization.h"
#include <math.h>
#include <string.h>

/*Class0c retains its owned10c legacy record. The ordinary projectile owner
 *covers b4..e0; target/shape/sound/mesh fields beyond it have one owner here. */
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t integer(uint32_t word){int32_t n;memcpy(&n,&word,4);return n;}
static float real(GameApplication *a,GameAddress p){uint32_t word=get(a,p);float f;memcpy(&f,&word,4);return f;}
static uint32_t bits(float f){uint32_t word;memcpy(&word,&f,4);return word;}
static uint32_t truncate_word(long double v){return (uint32_t)(uint64_t)(!isfinite(v)||v< -0x1p63L||v>=0x1p63L?INT64_MIN:(int64_t)v);}
static void store(GameApplication *a,GameAddress p,long double f){put(a,p,bits((float)f));}
static uint16_t sine_angle(GameApplication *a,float value){uint32_t lookup;uint16_t angle;
    if(integer(bits(value))>0x3f666666){lookup=truncate_word(((long double)value-real(a,0x574094))*real(a,0x574098));
        angle=game_memory_read_u16(&a->memory,0x62b8a8+lookup*2);
    }else if(bits(value)>0xbf666666u){lookup=truncate_word(((long double)real(a,0x57409c)-value)*real(a,0x574098));
        angle=(uint16_t)-game_memory_read_u16(&a->memory,0x62b8a8+lookup*2);
    }else{lookup=truncate_word((long double)value*real(a,0x5740a0)+real(a,0x5740a0));
        angle=game_memory_read_u16(&a->memory,0x62f8b8+lookup*2);}
    return angle;
}
static uint16_t heading(GameApplication *a,float x,float z){uint32_t lookup;uint16_t angle;
    if(!((bits(x)|bits(z))&0x7fffffffu))return 0;
    if(fabsl(z)>fabsl(x)){
        lookup=truncate_word((long double)x*real(a,0x5740a0)/z+real(a,0x5740a0));
        angle=game_memory_read_u16(&a->memory,0x62e8b0+lookup*2);
        return integer(bits(z))>0?angle&2047u:(uint16_t)(angle+1024);
    }
    lookup=truncate_word((long double)z*real(a,0x5740a0)/x+real(a,0x5740a0));
    return (uint16_t)((integer(bits(x))>0?512:1536)-game_memory_read_u16(&a->memory,0x62e8b0+lookup*2));
}
static uint8_t cell_flags(GameApplication *a,float x,float z){
    uint32_t ix=truncate_word(x),iz=truncate_word(z);
    return game_memory_read_u8(&a->memory,get(a,0x597d90)+(iz&0xfffffe00u)*4+(uint32_t)(integer(ix&0xfffffe3fu)>>6)+5);
}
/*48ddf0: validate the borrowed target, rotate the horizontal velocity toward
 *it within the signed-short turn budget, then adjust speed to the terrain. */
int game_level_mole_steer(GameApplication *a,GameAddress o){
    GameAddress target=get(a,o+0xe4),owner;uint32_t identity=get(a,o+20);float point[3],dx,dy,dz,vx,vz,speed,length,cosine,rx,rz,scale;
    long double dot;uint16_t turn;int sign;
    if(target && get(a,target+20)!=get(a,o+0xe8)){put(a,o+0xe4,0);put(a,o+0xe8,0);target=0;}
    if(!target){unsigned index=identity&63u;
        if(index>=4)goto clear;
        owner=get(a,0x5f0210+index*4);target=get(a,owner+0x3c4);
        if(!target || get(a,target+20)!=get(a,owner+0x3c8) || !((get(a,owner+0x3c8)^identity)&63u) || (get(a,target+16)&0x4000))goto clear;
        put(a,o+0xe4,target);put(a,o+0xe8,get(a,owner+0x3c8));
    }
    {float origin[3];unsigned i;for(i=0;i<3;i++)origin[i]=real(a,o+24+i*4);
        if(!presentation_target_position(&a->memory,target,origin,point))return 0;}
    vx=real(a,o+0xb4);vz=real(a,o+0xbc);
    dx=(float)((long double)point[0]-real(a,o+24));dy=(float)((long double)point[1]-real(a,o+28));dz=(float)((long double)point[2]-real(a,o+32));
    speed=(float)sqrtl((float)((long double)vz*vz+(long double)vx*vx));
    length=(float)sqrtl((float)((long double)dy*dy+(long double)dz*dz+(long double)dx*dx));
    if(!(bits(speed)&0x7fffffffu) || !(bits(length)&0x7fffffffu))return !a->memory.fault;
    dot=((long double)dz*vz+(long double)dx*vx)/((long double)length*speed);cosine=(float)dot;
    if(isnan(dot) || dot<real(a,0x574090))cosine=-1.0f;
    else if(!(real(a,0x574080)>cosine))cosine=real(a,0x574080);
    turn=(uint16_t)(512-sine_angle(a,cosine));
    dot=(long double)dx*vz-(long double)dz*vx;sign=isnan(dot)||dot<real(a,0x57407c)?-1:1;
    if((int16_t)game_memory_read_u16(&a->memory,o+0xf4)<(int16_t)turn)turn=game_memory_read_u16(&a->memory,o+0xf4);
    turn=(uint16_t)(turn*sign);
    {long double s=real(a,0x6298a8+(turn&2047u)*4),c=real(a,0x6298a8+((turn+512)&2047u)*4);
        rx=(float)(s*vz+c*vx);rz=(float)(-s*vx+c*vz);}
    (void)gameplay_geometry_terrain_height(&a->memory,real(a,o+24),real(a,o+32));
    scale=1.0f;
    if(!get(a,o+0xf8)){float height_delta;
        height_delta=(float)(gameplay_geometry_terrain_height(&a->memory,(float)((long double)real(a,o+24)+rx),
            (float)((long double)real(a,o+32)+rz))-real(a,o+28)+real(a,0x574078));
        /*The squared input is binary32; the returned root remains extended
         *through speed division, and only the final scale is stored. */
        long double projected_length=sqrtl((float)((long double)height_delta*height_delta+(long double)rz*rz+(long double)rx*rx));
        scale=(float)((long double)real(a,get(a,o+0xe0)+0x1c)/projected_length);
    }
    store(a,o+0xb4,(long double)scale*rx);store(a,o+0xbc,(long double)scale*rz);
    if(get(a,o+0xf8)){float v=(float)((long double)real(a,o+0xb8)-real(a,0x5740a4)),limit=real(a,0x5740a8);
        store(a,o+0xb8,(isnan(v)||isnan(limit)||limit<v)?v:limit);}
    return !a->memory.fault;
clear:put(a,o+0xe4,0);put(a,o+0xe8,0);return !a->memory.fault;
}
static void shrink(GameApplication *a,GameAddress o,unsigned field,long double step){
    float previous=real(a,o+field);long double next,zero=real(a,0x57407c);
    if(!(bits(previous)&0x7fffffffu))return;
    if(integer(bits(previous))>0){next=(long double)previous-step;if(!(isnan(next)||zero<next))next=zero;}
    else{next=(long double)previous+step;if(!(isnan(next)||zero>next))next=zero;}
    store(a,o+field,next);
}
/*48e1d0: actual landing/velocity, heading/spin, receding hump dimensions and
 *terrain-normal orientation. The existing pose owns world/model transforms. */
int game_level_mole_move(GameApplication *a,GameAddress o){
    float ground,normal[3],s,c,cosine,denominator,value;long double dot;uint16_t yaw,pitch,roll;
    PresentationPose *pose=presentation_instance_pose(o);
    store(a,o+24,(long double)real(a,o+24)+real(a,o+0xb4));store(a,o+32,(long double)real(a,o+32)+real(a,o+0xbc));
    ground=(float)(gameplay_geometry_terrain_height(&a->memory,real(a,o+24),real(a,o+32))+real(a,0x574078));
    if(get(a,o+0xf8)){
        dot=(long double)real(a,o+28)-ground;
        if(isnan(dot) || dot<real(a,0x5740ac)){float v[3]={
            (float)((long double)real(a,o+0xb4)*real(a,0x5740b0)),(float)((long double)real(a,o+0xb8)*real(a,0x5740b4)),
            (float)((long double)real(a,o+0xbc)*real(a,0x5740b0))};
            store(a,o+28,ground);if(!game_level_fragments_spawn(a,o,30,v))return 0;put(a,o+0xb8,0);put(a,o+0xf8,0);
        }else store(a,o+28,(long double)real(a,o+28)+real(a,o+0xb8));
    }else store(a,o+28,ground);
    yaw=heading(a,real(a,o+0xb4),real(a,o+0xbc));game_memory_write_u16(&a->memory,o+36,yaw);
    game_memory_write_u16(&a->memory,o+0x100,(game_memory_read_u16(&a->memory,o+0x100)+130)&2047);
    if(game_memory_read_u8(&a->memory,get(a,o+0xd4)+12)==0x20){
        shrink(a,o,0x104,((long double)real(a,o+0x38)+real(a,0x574078))*real(a,0x5740b8));
        shrink(a,o,0x108,(long double)real(a,get(a,get(a,0x5f06a8)+0x50)+0x20)*real(a,0x5740bc));
    }
    if(!gameplay_geometry_terrain_normal_values(&a->memory,normal,real(a,o+24),real(a,o+32)))return 0;
    s=real(a,0x6298a8+(yaw&2047u)*4);c=real(a,0x6298a8+((yaw+512)&2047u)*4);
    dot=(long double)s*normal[2]-(long double)c*normal[0];cosine=(float)dot;
    if(isnan(dot)||dot<real(a,0x574090))cosine=-1;else if(!(real(a,0x574080)>cosine))cosine=real(a,0x574080);
    pitch=sine_angle(a,cosine);game_memory_write_u16(&a->memory,o+40,pitch);
    denominator=real(a,0x6298a8+((pitch+512)&2047u)*4);
    value=(float)((bits(s)&0x7fffffffu)?((long double)c*cosine+normal[0])/((long double)denominator*s):
        ((long double)normal[2]-(long double)s*cosine)/((long double)denominator*c));
    if(bits(value)>0xbf800000u)value=-1;else if(!(real(a,0x574080)>value))value=real(a,0x574080);
    roll=sine_angle(a,value);game_memory_write_u16(&a->memory,o+38,roll);
    if(!pose || a->memory.fault)return 0;
    return presentation_pose_build_world(&a->memory,pose) && presentation_pose_transform_vertices(&a->memory,pose,0);
}
int game_level_mole_reset(GameApplication *a,GameAddress o){
    GameAddress mesh=get(a,o+0xfc);
    if(!game_menu_sound_stop(a,25,get(a,o+0xf0)) || (mesh && !mesh_buffer_clear(&a->memory,mesh)))return 0;
    put(a,o+0xec,0);return game_level_presentation_reset(a,o,1);
}
int game_level_mole_terrain_contact(GameApplication *a,GameAddress o,float normal[3],int *hit){
    uint8_t flags=cell_flags(a,real(a,o+24),real(a,o+32));normal[0]=normal[2]=0;normal[1]=1;
    *hit=((flags&0x20) && !(flags&0x50)) || integer(get(a,o+0xec)+get(a,o+0xdc)-1)<integer(get(a,0x5ac6ac));
    return !a->memory.fault;
}
int game_level_mole_contact(GameApplication *a,GameAddress o,GameAddress other,const float point[3]){
    uint32_t flags=get(a,other+16);int projectile=(flags&4)!=0;float v[3]={0,20,0};GameAddress shooter;
    if(projectile){shooter=get(a,other+0xd4);
        if(!shooter || game_memory_read_u8(&a->memory,shooter+12)!=0x20 || shooter==get(a,o+0xd4))return !a->memory.fault;
    }else if(!(flags&0x1c0))return !a->memory.fault;
    if(!game_level_fragments_spawn(a,o,15,v) || !game_level_projectile_impact(a,o,point,other,0,0x42fa00))return 0;
    return projectile || get(a,other+88)?game_level_mole_reset(a,o):!a->memory.fault;
}
/*515330/540cb0: bounded terrain material lookup and actual mesh face binding. */
static int bind_ground_material(GameApplication *a,GameAddress o,float x,float z){
    uint32_t ix=truncate_word(x)-(bits(x)>0x80000000u),iz=truncate_word(z)-(bits(z)>0x80000000u),n,i;
    GameAddress cell=get(a,0x597d90)+(iz&0x1fe00u)*4+((ix&0x1fe00u)>>6);
    GameAddress material=get(a,0x597d94)+(game_memory_read_u16(&a->memory,cell+2)&0x3fffu)*8;
    GameAddress mesh=get(a,o+0xfc),container=get(a,mesh+0x50),faces=get(a,container+0x18);
    n=get(a,container+0x1c);if(n>32767 || n>game_memory_read_u16(&a->memory,mesh+0x5e))return 0;
    for(i=0;i<n;i++)put(a,faces+i*40+20,material);return !a->memory.fault;
}
static float sound_distance(GameApplication *a,const float p[3]){
    GameAddress reference=get(a,0x58f890);long double dx=(long double)p[0]-real(a,reference+12),dz=(long double)p[2]-real(a,reference+20);
    return (float)(dx*dx+dz*dz);
}
/*48dc20: existing projectile placement plus real target, hump and sound state.
 *The caller's native point is copied before the original player-height change. */
int game_level_mole_launch(GameApplication *a,GameAddress o,uint32_t id,const float input[3],const uint16_t angles[3],GameAddress recipe,GameAddress shooter){
    GameAddress owner=get(a,0x5f0210+(id&63u)*4),target=get(a,shooter+0xf0),mesh=get(a,o+0xfc),hump=get(a,0x5f06a8);
    uint32_t ignored;uint8_t kind=game_memory_read_u8(&a->memory,shooter+12);float point[3];double factor;uint64_t factor_bits;
    SoundPlaybackRequest sound={0};
    if(!game_level_player_catalog_mount(a,owner,12,kind,5,&ignored) || !mesh || !hump ||
       !mesh_buffer_bind(&a->memory,mesh,get(a,hump+0x50)))return 0;
    if(target && get(a,target+20)==get(a,shooter+0xf4) && ((get(a,shooter+0xf4)^id)&63u) && !(get(a,target+16)&0x4000)){
        put(a,o+0xe4,target);put(a,o+0xe8,get(a,shooter+0xf4));
    }else{put(a,o+0xe4,0);put(a,o+0xe8,0);}
    memcpy(point,input,12);
    if(kind==0x20){if(!bind_ground_material(a,o,point[0],point[2]))return 0;
        point[1]=(float)(gameplay_geometry_terrain_height(&a->memory,point[0],point[2])+real(a,0x574078));}
    game_memory_write_u16(&a->memory,o+0x100,0);
    if(kind==0x20){store(a,o+0x104,-((long double)real(a,o+0x38)+real(a,0x574078)));put(a,o+0x108,get(a,get(a,hump+0x50)+0x20));}
    else{put(a,o+0x104,0);put(a,o+0x108,0);}
    if(!game_level_projectile_launch(a,o,id,point,angles,recipe,shooter,0x42f970,0))return 0;
    sound.sound=25;sound.volume=1;sound.flags=1;sound.force=1;sound.distance_squared=sound_distance(a,point);
    if(!game_menu_sound_play(a,&sound))return 0;put(a,o+0xf0,get(a,get(a,0x628a90)));put(a,o+0xf8,kind==0x27);
    put(a,o+0xec,get(a,recipe+0x24)*(get(a,o+0xe4)?2u:1u));
    factor_bits=(uint64_t)get(a,0x574088)|((uint64_t)get(a,0x57408c)<<32);memcpy(&factor,&factor_bits,8);
    game_memory_write_u16(&a->memory,o+0xf4,(uint16_t)truncate_word((long double)real(a,recipe+0x1c)*real(a,0x574084)*factor));
    return !a->memory.fault;
}
/*48eb00: controller matrices for the owned hump mesh and projectile model.
 *The actor model receives the additional X-spin composition; the hump uses
 *the opposite spin. Both reuse ordinary orientation/matrix arithmetic. */
int game_level_mole_controller(GameApplication *a,GameAddress o){
    float matrix[12],composed[12],spin[12]={0},position[3];uint16_t phase=game_memory_read_u16(&a->memory,o+0x100),angles[3];
    GameAddress mesh=get(a,o+0xfc),model=get(a,o+0x54);long double s,c;
    position[0]=real(a,o+0x64);position[2]=real(a,o+0x6c);
    angles[0]=game_memory_read_u16(&a->memory,o+0x78);angles[2]=game_memory_read_u16(&a->memory,o+0x74);
    if(get(a,o+0x108)&0x7fffffffu){
        position[1]=(float)((long double)real(a,o+0x108)-real(a,get(a,get(a,0x5f06a8)+0x50)+0x20)-real(a,0x574078)+real(a,o+0x68));
        angles[1]=(uint16_t)(2048-phase);gameplay_geometry_orientation_values(&a->memory,angles,position,matrix);
        /*The hump stores this product before its first row consumes it. */
        {long double sa=real(a,0x6298a8+(angles[0]&2047u)*4),ca=real(a,0x6298a8+((angles[0]+512)&2047u)*4);
            long double sb=real(a,0x6298a8+(angles[1]&2047u)*4),sc=real(a,0x6298a8+(angles[2]&2047u)*4),cc=real(a,0x6298a8+((angles[2]+512)&2047u)*4);
            matrix[0]=(float)(cc*ca+(long double)(float)(sc*sb)*sa);}
        if(!mesh || !game_memory_write(&a->memory,mesh+4,matrix,48))return 0;
    }
    position[1]=(float)((long double)real(a,o+0x68)+real(a,o+0x104));angles[1]=game_memory_read_u16(&a->memory,o+0x76);
    gameplay_geometry_orientation_values(&a->memory,angles,position,matrix);
    /*48ed7b/48edb2 consume the saved binary32 sin(pitch)*cos(yaw),
     *unlike the ordinary shared orientation builder's saved product. */
    {long double sa=real(a,0x6298a8+(angles[0]&2047u)*4),ca=real(a,0x6298a8+((angles[0]+512)&2047u)*4);
        long double sb=real(a,0x6298a8+(angles[1]&2047u)*4),sc=real(a,0x6298a8+(angles[2]&2047u)*4),cc=real(a,0x6298a8+((angles[2]+512)&2047u)*4);
        float saved=(float)(sc*ca);
        matrix[1]=(float)((sb*sa)*cc-saved);matrix[8]=(float)((long double)saved*sb-sa*cc);
        matrix[9]=(float)((sb*ca)*cc+sc*sa);}
    s=real(a,0x6298a8+(phase&2047u)*4);c=real(a,0x6298a8+((phase+512)&2047u)*4);
    spin[0]=1;spin[5]=(float)c;spin[6]=(float)-s;spin[9]=(float)s;spin[10]=(float)c;
    game_matrix_compose_values(composed,matrix,spin);
    if(!model || a->memory.fault)return 0;
    if(!game_memory_write(&a->memory,model+4,composed,48))return 0;
    return !a->memory.fault;
}
/*48e9a0: local display extrapolation, actual controller and both submissions. */
int game_level_mole_present(GameApplication *a,GameAddress o){
    PresentationPose *pose=presentation_instance_pose(o);uint16_t spin;float ground,next,dt;uint32_t elapsed;
    if(!pose || a->memory.fault)return 0;
    memcpy(pose->local_position,pose->position,12);for(unsigned i=0;i<3;i++)pose->local_angles[2-i]=pose->angles[i];
    if(get(a,0x5eff04)){
        elapsed=get(a,0x5acbe8)-get(a,0x5acc80);dt=(float)integer(elapsed);
        pose->local_position[0]=(float)((long double)real(a,o+0xb4)*dt+pose->local_position[0]);
        pose->local_position[2]=(float)((long double)real(a,o+0xbc)*dt+pose->local_position[2]);
        ground=(float)(gameplay_geometry_terrain_height(&a->memory,pose->local_position[0],pose->local_position[2])+real(a,0x574078));
        if(get(a,o+0xf8)){next=(float)((long double)real(a,o+0xb8)*dt+pose->local_position[1]);
            pose->local_position[1]=(isnan(ground)||isnan(next)||ground<next)?next:ground;
        }else pose->local_position[1]=ground;
        spin=(uint16_t)((uint16_t)elapsed*130+pose->local_angles[1]);pose->local_angles[1]=spin&2047;
    }
    if(!game_level_mole_controller(a,o))return 0;
    if(cell_flags(a,pose->local_position[0],pose->local_position[2])&0x50)return !a->memory.fault;
    if(!get(a,o+0xf8) && (get(a,o+0x108)&0x7fffffffu) && !game_model_graph_update(a,get(a,o+0xfc)))return 0;
    return game_model_graph_update(a,pose->model);
}
/*48e7a0: shared flight/contact chain, then positional sound and ground debris. */
int game_level_mole_update(GameApplication *a,GameAddress o){
    uint32_t tick=get(a,0x5ac6ac),voice;GameAddress bank,shooter;int playing;float point[3],velocity[3],volume;
    if(!game_level_projectile_update(a,o,0x42f300))return 0;if(!get(a,o+88))return !a->memory.fault;
    for(unsigned i=0;i<3;i++)point[i]=real(a,o+24+i*4);
    bank=get(a,0x628a90);voice=get(a,o+0xf0);
    if(get(a,bank+0x468) && !(tick&3)){
        if(!game_menu_sound_playing(a,25,voice,&playing))return 0;
        if(playing){volume=sound_playback_attenuation(&a->memory,get(a,bank+0x70),sound_distance(a,point));
            if(!game_menu_sound_volume(a,25,voice,volume))return 0;}
    }
    if(get(a,o+0xf8))return !a->memory.fault;
    if(!bind_ground_material(a,o,point[0],point[2]))return 0;
    shooter=get(a,o+0xd4);if(game_memory_read_u8(&a->memory,shooter+12)==0x20 && integer(get(a,o+0xdc)+25)>=integer(tick))return !a->memory.fault;
    velocity[0]=(float)((long double)real(a,o+0xb4)*real(a,0x5740b4));velocity[1]=0;
    velocity[2]=(float)((long double)real(a,o+0xbc)*real(a,0x5740b4));
    if(!effect_initialization_random(&a->memory,real(a,0x5740c4)) && !game_level_fragments_spawn(a,o,6,velocity))return 0;
    if(!(tick&7)){velocity[1]=5;
        if(!game_level_particle_emit(a,o,point,velocity,(float)((long double)real(a,o+0x38)*real(a,0x5740a4)),
            get(a,0x5ac908)==1?0x58ab48:0x58ab78,game_memory_read_u16(&a->memory,o+36),game_memory_read_u16(&a->memory,o+38),0))return 0;
    }
    return !a->memory.fault;
}
