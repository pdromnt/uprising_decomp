#include "level_scene_render.h"
#include "game_matrix.h"
#include "display_transition.h"
#include "gameplay_geometry.h"
#include "gameplay_messages.h"
#include "player_feedback.h"
#include "menu_runtime_tables.h"
#include <string.h>
#include <math.h>
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t b=get(a,p);float v;memcpy(&v,&b,4);return v;}
static void store(GameApplication *a,GameAddress p,float v){uint32_t b;memcpy(&b,&v,4);put(a,p,b);}
static int32_t signed_word(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static int32_t cell(float value){
    long double v=truncl(value);int32_t low;
    if(!isfinite(v) || v<-9223372036854775808.0L || v>=9223372036854775808.0L)low=0;
    else low=signed_word((uint32_t)(uint64_t)(int64_t)v);
    return low>=0?low/4096:-1-((-(int64_t)low-1)/4096);
}
/*499f40: consume the original sector-relative visibility lists directly.
 * Each sector owns81 bounded entries; publish actual valid spatial cells. */
static int visible_cells(GameApplication *a,float x,float z,uint16_t heading){
    int sector=(int16_t)heading,row=cell(x),column=cell(z),count;unsigned i,written=0;
    sector=sector>=0?sector/64:-1-((-sector-1)/64);
    count=(int16_t)game_memory_read_u16(&a->memory,0x5ff810+(uint32_t)sector*2);
    if(count>81)return 0;
    for(i=0;count>0 && i<(unsigned)count;i++){
        GameAddress p=0x5ff850+(uint32_t)sector*324+i*4;
        int r=row+(int16_t)game_memory_read_u16(&a->memory,p);
        int c=column+(int16_t)game_memory_read_u16(&a->memory,p+2);
        if(r>=0 && c>=0 && r<32 && c<32){
            game_memory_write_u16(&a->memory,0x5f06b8+written*4,(uint16_t)r);
            game_memory_write_u16(&a->memory,0x5f06ba+written*4,(uint16_t)c);written++;
        }
    }
    put(a,0x6020d0,written);return !a->memory.fault;
}
/*Shared actual412740/5322f0/532520/5326d0/499f40 consumers. */
static int publish_pose(GameApplication *a,GameAddress camera,uint16_t basis_angle,uint16_t roll,int update_sector){
    float position[3];uint16_t heading,pitch;int16_t sector;unsigned i;
    gameplay_geometry_setup_angle_basis(&a->memory,basis_angle);
    for(i=0;i<3;i++)position[i]=real(a,camera+12+i*4);
    heading=game_memory_read_u16(&a->memory,camera+6);pitch=game_memory_read_u16(&a->memory,camera+8);
    if(!game_matrix_camera_projection(&a->memory,0x629088,position,roll,pitch,heading,(float)signed_word(get(a,0x597f7c))) ||
       !game_matrix_camera_view(&a->memory,0x5cedf0,position,roll,pitch,heading))return 0;
    sector=(int16_t)heading;if(update_sector)put(a,0x59d438,((sector>256 && sector<768)||(sector>1280 && sector<1792))?0:1);
    return visible_cells(a,position[0],position[2],heading);
}
static int publish_camera(GameApplication *a,GameAddress camera,uint16_t basis_angle){
    return publish_pose(a,camera,basis_angle,game_memory_read_u16(&a->memory,camera+10),1);
}
/*4a4020/4a43a0: player camera's actual attachment heading, asymmetric pitch
 * response and translated yaw/pitch chain. Preserve the original two uses of
 * the camera offset rather than inventing a simpler orbit camera. */
static int player_camera(GameApplication *a,GameAddress camera,uint16_t offset_heading){
    GameAddress object=get(a,camera+0x28),model=get(a,object+0x54),mount=get(a,object+0x270);
    float left[12],right[12],frame[12],offset[3],position[3];uint16_t angles[2],heading,pitch;
    float translated[12]={1,0,0,0,0,1,0,0,0,0,1,0};
    float tilted[12]={1,0,0,0,0,0,0,0,0,0,0,0};
    float yawed[12]={0,0,0,0,0,1,0,0,0,0,0,0};
    float world[12]={1,0,0,0,0,1,0,0,0,0,1,0};unsigned i;
    int32_t response=(int16_t)game_memory_read_u16(&a->memory,object+0x226)+56;
    int32_t scaled=response*3,divisor=response<0?8:2;
    scaled=scaled>=0?scaled/divisor:-1-((-scaled-1)/divisor);pitch=(uint16_t)scaled;
    for(i=0;i<12;i++){left[i]=real(a,model+4+i*4);right[i]=real(a,mount+4+i*4);}
    game_matrix_compose_values(frame,left,right);
    if(!game_matrix_extract_heading_pitch(&a->memory,frame,angles))return 0;
    heading=(uint16_t)((angles[0]+offset_heading)&2047);
    game_memory_write_u16(&a->memory,camera+6,heading);game_memory_write_u16(&a->memory,camera+8,pitch);
    game_memory_write_u16(&a->memory,camera+10,0);
    for(i=0;i<3;i++){
        offset[i]=real(a,camera+0x2c+i*4);translated[i*4+3]=offset[i];world[i*4+3]=real(a,object+0x64+i*4);
    }
    tilted[5]=tilted[10]=real(a,0x6298a8+((pitch+512u)&2047u)*4);
    tilted[9]=real(a,0x6298a8+(pitch&2047u)*4);tilted[6]=-tilted[9];
    yawed[0]=yawed[10]=real(a,0x6298a8+((heading+512u)&2047u)*4);
    yawed[2]=real(a,0x6298a8+(heading&2047u)*4);yawed[8]=-yawed[2];
    game_matrix_compose_values(tilted,tilted,translated);
    game_matrix_compose_values(world,world,yawed);game_matrix_compose_values(world,world,tilted);
    game_matrix_transform_point_values(position,world,offset);
    {float ground=(float)gameplay_geometry_terrain_height(&a->memory,position[0],position[2]);
     float margin=real(a,0x574380);
     if(!((long double)position[1]+margin>=ground))position[1]=(float)((long double)ground+margin);}
    for(i=0;i<3;i++)store(a,camera+12+i*4,position[i]);
    return !a->memory.fault && publish_camera(a,camera,0);
}
static uint32_t truncate_value(long double value){
    value=truncl(value);
    if(!isfinite(value) || value<-9223372036854775808.0L || value>=9223372036854775808.0L)return 0;
    return (uint32_t)(uint64_t)(int64_t)value;
}
/*4f8800: original signed pitch-dependent vehicle roll. */
static uint16_t vehicle_roll(GameApplication *a,GameAddress object){
    int16_t pitch=(int16_t)game_memory_read_u16(&a->memory,object+0x226);
    int16_t roll=(int16_t)game_memory_read_u16(&a->memory,object+0x2cc);
    if(pitch>-113)return (uint16_t)roll;
    if(pitch<-284)return 0;
    return (uint16_t)truncate_value(((long double)-113-pitch)*real(a,0x574dd0)*roll);
}
/*4a3c40: chase camera follows both live attachment matrices; shake alters
 * this frame's actual view/visible cells and then restores the stored pose. */
static int chase_camera(GameApplication *a,GameAddress camera){
    GameAddress object=get(a,camera+0x28),model=get(a,object+0x54),mount=get(a,object+0x270),child=get(a,object+0x228);
    float left[12],right[12],frame[12],offset[3],position[3],shake[3]={0};uint16_t angles[2],roll;unsigned i;int active,result;
    for(i=0;i<12;i++){left[i]=real(a,model+4+i*4);right[i]=real(a,mount+4+i*4);}
    game_matrix_compose_values(frame,left,right);
    for(i=0;i<12;i++)right[i]=real(a,child+4+i*4);
    game_matrix_compose_values(frame,frame,right);
    for(i=0;i<3;i++)offset[i]=real(a,camera+0x5c+i*4);
    game_matrix_transform_point_values(position,frame,offset);
    {uint32_t x,z;memcpy(&x,position,4);memcpy(&z,position+2,4);
     if(signed_word(x)>0 && signed_word(x)<0x48000000 && signed_word(z)>0 && signed_word(z)<0x48000000){
        long double floor=gameplay_geometry_terrain_height(&a->memory,position[0],position[2])+real(a,0x57436c);
        if(floor>position[1])position[1]=(float)floor;
     }}
    if(!game_matrix_extract_heading_pitch(&a->memory,frame,angles))return 0;
    game_memory_write_u16(&a->memory,camera+6,angles[0]);game_memory_write_u16(&a->memory,camera+8,angles[1]);
    roll=vehicle_roll(a,object);
    if(get(a,0x5ac6b0))roll=(uint16_t)(roll+truncate_value((long double)(int16_t)game_memory_read_u16(&a->memory,object+0x74)*real(a,0x574374)));
    game_memory_write_u16(&a->memory,camera+10,roll);
    if(!game_camera_hit_sample(&a->memory,camera,shake,&active))return 0;
    for(i=0;i<3;i++)store(a,camera+12+i*4,active?(float)((long double)position[i]+shake[i]):position[i]);
    result=publish_camera(a,camera,(uint16_t)(0u-roll));
    if(active)for(i=0;i<3;i++)store(a,camera+12+i*4,position[i]);
    return result && !a->memory.fault;
}
/*4a4510/4a4830: orbit around the actual scene, then advance the idle turn or
 * recenter it after movement. Keep the original presentation-before-update. */
static int orbit_scene_camera(GameApplication *a,GameAddress camera){
    float offset[3],position[3],frame[12];unsigned i;
    uint16_t heading=game_memory_read_u16(&a->memory,camera+0x38)&2047;
    GameAddress object=get(a,camera+0x40);
    float translation[12]={1,0,0,0,0,1,0,0,0,0,1,0};
    float tilt[12]={1,0,0,0,0,0,0,0,0,0,0,0};
    float yaw[12]={0,0,0,0,0,1,0,0,0,0,0,0};
    for(i=0;i<3;i++){
        offset[i]=(float)((long double)real(a,camera+0x2c+i*4)*real(a,0x574384));
        translation[3+i*4]=offset[i];
    }
    tilt[5]=real(a,0x62a3c4);tilt[6]=-real(a,0x629bc4);
    tilt[9]=real(a,0x629bc4);tilt[10]=tilt[5];
    yaw[0]=real(a,0x6298a8+((heading+512)&2047)*4);yaw[10]=yaw[0];
    yaw[2]=real(a,0x6298a8+heading*4);yaw[8]=-yaw[2];
    for(i=0;i<12;i++)frame[i]=i%5==0?1:0;
    for(i=0;i<3;i++)frame[3+i*4]=real(a,object+24+i*4);
    game_matrix_compose_values(tilt,tilt,translation);
    game_matrix_compose_values(frame,frame,yaw);
    game_matrix_compose_values(frame,frame,tilt);
    game_matrix_transform_point_values(position,frame,offset);
    for(i=0;i<3;i++)store(a,camera+12+i*4,position[i]);
    {float floor=(float)gameplay_geometry_terrain_height(&a->memory,position[0],position[2]);
     float margin=real(a,0x574380);
     if((long double)position[1]+margin<floor)store(a,camera+16,(float)((long double)floor+margin));}
    game_memory_write_u16(&a->memory,camera+6,heading);
    game_memory_write_u16(&a->memory,camera+8,199);
    game_memory_write_u16(&a->memory,camera+10,0);
    return publish_camera(a,camera,0);
}
static void orbit_recenter(GameApplication *a,GameAddress camera,int32_t elapsed){
    uint16_t raw=game_memory_read_u16(&a->memory,camera+0x38);
    int32_t angle,step,next;
    if(!raw)return;
    angle=raw&2047;if(angle>1024)angle-=2048;
    step=signed_word((uint32_t)elapsed*80u);
    next=signed_word((uint32_t)angle+(angle>0?0u-(uint32_t)step:(uint32_t)step));
    if((angle>0 && next<=0)||(angle<=0 && next>=0))next=0;
    game_memory_write_u16(&a->memory,camera+0x38,(uint16_t)next&2047);
}
static int orbit_camera(GameApplication *a,GameAddress camera){
    uint32_t now=get(a,0x5acbe8),wait;GameAddress object=get(a,camera+0x40);
    GameAddress player=get(a,get(a,0x5f0208)+0x3bc);
    int32_t elapsed=signed_word(now-get(a,camera+0x44));
    if(elapsed>10)elapsed=1;
    put(a,camera+0x44,now);
    if(!(object==player?player_camera(a,camera,game_memory_read_u16(&a->memory,camera+0x38)):
            orbit_scene_camera(a,camera)))return 0;
    if(get(a,0x5acbe4))return !a->memory.fault;
    if(game_memory_read_u8(&a->memory,object+12)==0x23)goto rotate;
    if(fabsl(real(a,player+0x19c))>real(a,0x574360) ||
       fabsl(real(a,player+0x1a4))>real(a,0x574360) || game_memory_read_u16(&a->memory,player+0x1a8)){
        put(a,camera+0x3c,16);orbit_recenter(a,camera,elapsed);
        return !a->memory.fault;
    }
    wait=get(a,camera+0x3c);
    if(wait){
        if(game_memory_read_u16(&a->memory,camera+0x38))orbit_recenter(a,camera,elapsed);
        else put(a,camera+0x3c,wait-1);
        return !a->memory.fault;
    }
rotate:
    game_memory_write_u16(&a->memory,camera+0x38,
        (uint16_t)((uint32_t)elapsed*20u+game_memory_read_u16(&a->memory,camera+0x38))&2047);
    return !a->memory.fault;
}
/*4a5b30: publish the composed mount frame in the camera's existing record. */
static int mount_camera(GameApplication *a,GameAddress camera){
    GameAddress object=get(a,camera+0x28),model=get(a,object+0x54);
    GameAddress mount=get(a,object+0x1b4),base=get(a,object+0x1b0);
    float left[12],right[12],frame[12],offset[3],position[3];unsigned i;
    for(i=0;i<12;i++){left[i]=real(a,model+4+i*4);right[i]=real(a,mount+4+i*4);}
    game_matrix_compose_values(frame,left,right);
    for(i=0;i<12;i++)right[i]=real(a,base+4+i*4);
    game_matrix_compose_values(frame,frame,right);
    for(i=0;i<12;i++)store(a,camera+0x2c+i*4,frame[i]);
    for(i=0;i<3;i++)offset[i]=real(a,camera+0x5c+i*4);
    game_matrix_transform_point_values(position,frame,offset);
    for(i=0;i<3;i++)store(a,camera+12+i*4,position[i]);
    game_memory_write_u16(&a->memory,camera+6,(game_memory_read_u16(&a->memory,object+0x1ba)+game_memory_read_u16(&a->memory,object+0x78))&2047);
    game_memory_write_u16(&a->memory,camera+8,game_memory_read_u16(&a->memory,object+0x1b8));
    game_memory_write_u16(&a->memory,camera+10,0);
    return publish_pose(a,camera,0,0,0);
}
/*4a56b0: three cubic spans, with the existing resume polynomial in span zero.
 * Original binary32 stores are retained at each Horner step. */
static int spline_camera(GameApplication *a,GameAddress camera){
    int32_t elapsed=signed_word(get(a,0x5acbe8)-get(a,camera+0x170));
    int32_t phase=elapsed%210,span=phase/70;unsigned axis,term;
    float factor=(float)((long double)(phase%70)*real(a,0x5743a0));
    GameAddress coefficients;float delta[3],ratio;long double length,value;uint16_t heading,pitch;uint32_t index,bits;
    if(span)put(a,camera+0x16c,0);
    coefficients=get(a,camera+0x16c)?camera+0x13c:camera+0x40+(uint32_t)span*48;
    for(axis=0;axis<3;axis++){
        float p=real(a,coefficients+axis*4);
        for(term=1;term<4;term++){
            p=(float)((long double)p*factor);
            p=(float)((long double)p+real(a,coefficients+term*12+axis*4));
        }
        store(a,camera+12+axis*4,p);
    }
    if(signed_word(get(a,camera+12))>0 && signed_word(get(a,camera+12))<0x48000000 &&
       signed_word(get(a,camera+20))>0 && signed_word(get(a,camera+20))<0x48000000){
        long double floor=gameplay_geometry_terrain_height(&a->memory,real(a,camera+12),real(a,camera+20))+real(a,0x57436c);
        if(floor>real(a,camera+16))store(a,camera+16,(float)floor);
    }
    for(axis=0;axis<3;axis++)delta[axis]=(float)((long double)real(a,camera+0x34+axis*4)-real(a,camera+12+axis*4));
    if(!game_menu_pair_heading(&a->memory,delta[0],delta[2],real(a,0x573538),&heading))return 0;
    game_memory_write_u16(&a->memory,camera+6,heading);
    length=sqrtl((float)(((long double)delta[1]*delta[1]+(long double)delta[2]*delta[2])+(long double)delta[0]*delta[0]));
    value=(long double)delta[1]/length;ratio=(float)value;memcpy(&bits,&ratio,4);
    if(value>real(a,0x5743a4)){
        index=truncate_value(((long double)ratio-real(a,0x5743a4))*real(a,0x5743a8));
        if(index>2048)return 0;
        pitch=game_memory_read_u16(&a->memory,0x62b8a8+index*2);
    }else if(bits>0xbf666666u){
        index=truncate_value(((long double)real(a,0x5743ac)-ratio)*real(a,0x5743a8));
        if(index>2048)return 0;
        pitch=(uint16_t)(0u-game_memory_read_u16(&a->memory,0x62b8a8+index*2));
    }else{
        index=truncate_value((long double)ratio*real(a,0x5743b0)+real(a,0x5743b0));
        if(index>2048)return 0;
        pitch=game_memory_read_u16(&a->memory,0x62f8b8+index*2);
    }
    game_memory_write_u16(&a->memory,camera+8,(uint16_t)(0u-pitch));
    return publish_pose(a,camera,0,0,1);
}
int game_level_camera_present(GameApplication *a,GameAddress camera,GameAddress method){
    if(method==0x4a56b0)return spline_camera(a,camera);
    if(method==0x4a5b30)return mount_camera(a,camera);
    if(method==0x4a4510)return orbit_scene_camera(a,camera);
    if(method==0x4a4830)return orbit_camera(a,camera);
    if(method==0x4a3c40)return chase_camera(a,camera);
    if(method==0x4a43a0)return player_camera(a,camera,0);
    if(method==0x4a4d50){ /*Free camera. */
        game_memory_write_u16(&a->memory,camera+10,0);return publish_camera(a,camera,0);
    }
    if(method==0x4a4ab0){ /*Tracking camera, shared follow and target consumers. */
        GameAddress object=get(a,camera+0x28);unsigned i;
        if(!object || signed_word(get(a,object+0x58))<=0){
            return game_message_enqueue(a,get(a,0x628a88),0x58f8b4,1) && game_camera_restore(&a->memory,NULL);
        }
        for(i=0;i<3;i++)put(a,camera+12+i*4,get(a,object+24+i*4));
        if(signed_word(get(a,camera+12))>0 && signed_word(get(a,camera+12))<0x48000000 &&
           signed_word(get(a,camera+20))>0 && signed_word(get(a,camera+20))<0x48000000){
            long double floor=gameplay_geometry_terrain_height(&a->memory,real(a,camera+12),real(a,camera+20))+real(a,0x57436c);
            if(floor>real(a,camera+16))store(a,camera+16,(float)floor);
        }
        object=get(a,camera+0x28);
        game_memory_write_u16(&a->memory,camera+6,game_memory_read_u16(&a->memory,object+((get(a,object+16)&0x200)?0x148:0x24))&2047);
        game_memory_write_u16(&a->memory,camera+8,game_memory_read_u16(&a->memory,object+0x26)&2047);
        game_memory_write_u16(&a->memory,camera+10,game_memory_read_u16(&a->memory,object+0x28)&2047);
        return publish_camera(a,camera,0);
    }
    return 0; /*Unknown method IDs cannot silently enter a CPU fallback. */
}
