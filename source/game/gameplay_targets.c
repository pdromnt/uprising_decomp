#include "gameplay_input.h"
#include "game_matrix.h"
#include "gameplay_geometry.h"
#include "menu_sound.h"
#include "portable_crt.h"
#include "level_scene_render.h"
#include "presentation_pose.h"
#include <math.h>
#include <string.h>

static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float real(GameApplication *a,GameAddress p){uint32_t word=get(a,p);float value;memcpy(&value,&word,4);return value;}
static int32_t signed_word(uint32_t word){int32_t value;memcpy(&value,&word,4);return value;}
static int matrix_read(GameApplication *a,GameAddress p,float values[12]){
    unsigned i;for(i=0;i<12;i++)values[i]=real(a,p+i*4);return !a->memory.fault;
}
static uint32_t truncate_word(long double value){
    value=truncl(value);
    if(!isfinite(value) || value<-9223372036854775808.0L || value>=9223372036854775808.0L)return 0;
    return (uint32_t)(uint64_t)(int64_t)value;
}
static int32_t grid_cell(float value){
    int32_t integer=signed_word(truncate_word(value)),cell=integer/512;
    if(integer<0 && integer%512)--cell;
    return cell;
}
/*4b2ea0/4b2ed0 and their shared43aee0 consumer. Retain the original
 * wrapped signed range operation and invalidate the existing cached bounds. */
static int selection_step(GameApplication *a,int increase){
    uint32_t word=get(a,0x603878)+(increase?2u:(uint32_t)-2);
    int32_t value=signed_word(word);
    if(increase){if(value>=16)value=16;}else if(value<=2)value=2;
    word=(uint32_t)value;game_memory_write_u32(&a->memory,0x603878,word);
    game_memory_write_u32(&a->memory,0x603870,(word>>1)|(word&0x80000000u));
    return game_level_scene_invalidate(a);
}
int game_input_selection_previous(GameApplication *a){return selection_step(a,1);}
int game_input_selection_next(GameApplication *a){return selection_step(a,0);}
/*4b3230: lock the current generation, play the real feedback and stamp
 * the existing target state. Invalid/absent targets leave it unlocked. */
int game_input_target_cycle(GameApplication *a){
    GameAddress target;
    if(get(a,0x603710)){game_memory_write_u32(&a->memory,0x603710,0);return !a->memory.fault;}
    target=get(a,0x60387c);
    if(!target || get(a,target+0x14)!=get(a,0x603e38))return !a->memory.fault;
    game_memory_write_u32(&a->memory,0x603710,1);
    if(a->memory.fault || !game_menu_sound(a,135))return 0;
    game_memory_write_u32(&a->memory,0x58fe00,get(a,0x5ac6ac));return !a->memory.fault;
}
static void line_hit_publish(GameApplication *a,const float hit[3]){
    unsigned i;for(i=0;i<3;i++){uint32_t word;memcpy(&word,hit+i,4);game_memory_write_u32(&a->memory,0x5ab4f0+i*4,word);}
}
/*410830: scan x boundaries, then z boundaries in original order. The
 * published hit is a sampled boundary point/terrain height, not a refined ray
 * intersection. Preserve the original skipped starting cell and vertical case. */
int game_input_world_line(GameApplication *a,const float from[3],const float to[3],float hit[3],int *clear){
    float delta[3],step=real(a,0x571f24);unsigned i,pass,visits=0;
    hit[0]=hit[1]=hit[2]=-1.0f;*clear=0;line_hit_publish(a,hit);
    for(i=0;i<3;i++)delta[i]=(float)((long double)to[i]-from[i]);
    if(fabsl(delta[0])>real(a,0x571f2c) || fabsl(delta[2])>real(a,0x571f2c))return !a->memory.fault;
    if(delta[0]==0)delta[0]=1;
    if(delta[2]==0)delta[2]=1;
    for(pass=0;pass<2;pass++){
        unsigned axis=pass?2u:0u,other=pass?0u:2u;
        int positive=delta[axis]>0;
        int32_t first=grid_cell(from[axis]),last=grid_cell(to[axis]);
        float boundary,limit,coordinate,slope,height_slope,ground;
        first=signed_word((uint32_t)first+(positive?1u:UINT32_MAX));
        if(first==last)continue;
        boundary=(float)signed_word((uint32_t)first<<9);
        limit=(float)((long double)last*step);
        slope=(float)((long double)delta[other]/delta[axis]);
        height_slope=(float)((long double)delta[1]/fabsl(delta[other]));
        while(positive?!(boundary>limit):!(boundary<limit)){
            long double height;
            if(!positive && to[axis]>boundary)break;
            if(++visits>1024){a->memory.fault=1;return 0;}
            coordinate=(float)(((long double)boundary-from[axis])*slope+from[other]);
            ground=(float)gameplay_geometry_terrain_height(&a->memory,pass?coordinate:boundary,pass?boundary:coordinate);
            if(a->memory.fault)return 0;
            height=fabsl((long double)coordinate-from[other])*height_slope+from[1];
            if(!(height>ground)){
                hit[axis]=boundary;hit[other]=coordinate;hit[1]=ground;line_hit_publish(a,hit);return !a->memory.fault;
            }
            boundary=(float)((long double)boundary+(positive?step:-step));
        }
    }
    *clear=1;return !a->memory.fault;
}
/*411840: original heading/pitch lookup and finite endpoint feed the actual
 * terrain boundary scan. Callers consume the hit, including its -1 sentinel. */
int game_input_camera_ray(GameApplication *a,const float origin[3],uint16_t heading,uint16_t pitch,float hit[3]){
    uint16_t inverted=(uint16_t)(0u-pitch);float endpoint[3];int clear;
    long double horizontal=(long double)real(a,0x6298a8+((inverted+512u)&2047u)*4)*real(a,0x571f44);
    endpoint[0]=(float)((long double)real(a,0x6298a8+(heading&2047u)*4)*horizontal+origin[0]);
    endpoint[1]=(float)((long double)real(a,0x6298a8+(inverted&2047u)*4)*real(a,0x571f44)+origin[1]);
    endpoint[2]=(float)(horizontal*real(a,0x6298a8+((heading+512u)&2047u)*4)+origin[2]);
    return game_input_world_line(a,origin,endpoint,hit,&clear);
}
static int selected_position(GameApplication *a,GameAddress entity,GameAddress source,uint32_t *first,uint32_t *second){
    float from[3],to[3];unsigned i;
    /*483290 chooses the nearest transformed aim point relative to the caller.
     *Both unit deployment and destination input use the same real override. */
    for(i=0;i<3;i++)from[i]=real(a,source+i*4);
    if(!presentation_target_position(&a->memory,entity,from,to))return 0;
    memcpy(first,to,4);memcpy(second,to+2,4);return !a->memory.fault;
}
static int nearby(GameApplication *a,GameAddress source,int target,uint32_t *first,uint32_t *second,int *found){
    GameAddress selected=get(a,0x60387c);
    *found=0;
    if(selected && get(a,selected+0x14)==get(a,0x603e38)){
        uint8_t kind=game_memory_read_u8(&a->memory,selected+0xc);
        if(!target || (kind!=39 && kind!=38)){
            if(!selected_position(a,selected,source,first,second))return 0;
            *found=1;return !a->memory.fault;
        }
    }
    if(get(a,0x603758)!=0xbf800000u){*first=get(a,0x603758);*second=get(a,0x603760);*found=1;}
    return !a->memory.fault;
}
int game_input_nearby_destination(GameApplication *a,GameAddress source,uint32_t *first,uint32_t *second,int *found){
    return nearby(a,source,0,first,second,found);
}
int game_input_nearby_target(GameApplication *a,GameAddress source,uint32_t *first,uint32_t *second,int *found){
    return nearby(a,source,1,first,second,found);
}
/*4d6e60: actual vehicle/attachment matrix composition and camera heading
 * choose a forward world target. Matrix kernels and angle lookup already run
 * C; all intermediates are local values, not allocated compatibility records. */
int game_input_screen_target(GameApplication *a,uint32_t *first,uint32_t *second,int *found){
    GameMemory *m=&a->memory;GameAddress local=get(a,0x5f0208),vehicle=get(a,local+0x3bc),active;
    float position[3];uint16_t angles[2];unsigned i;
    *found=0;
    if(signed_word(get(a,vehicle+0x58))<=0)return !m->fault;
    active=get(a,local+0x448);
    if(vehicle==active){
        float left[12],right[12],combined[12],attachment[12],matrix[12],anchor[3];
        GameAddress camera;
        if(!matrix_read(a,get(a,vehicle+0x54)+4,left) || !matrix_read(a,get(a,vehicle+0x270)+4,right))return 0;
        game_matrix_compose_values(combined,left,right);
        if(!matrix_read(a,get(a,vehicle+0x228)+4,attachment))return 0;
        game_matrix_compose_values(matrix,combined,attachment);
        camera=get(a,0x58f89c);
        for(i=0;i<3;i++)anchor[i]=real(a,camera+0x5c+i*4);
        game_matrix_transform_point_values(position,matrix,anchor);
        if(!game_matrix_extract_heading_pitch(m,matrix,angles))return 0;
        position[0]=(float)((long double)real(a,0x6298a8+(angles[0]&2047u)*4)*real(a,0x5749f8)+position[0]);
        position[2]=(float)((long double)real(a,0x6298a8+((angles[0]+512u)&2047u)*4)*real(a,0x5749f8)+position[2]);
    }else{
        GameAddress camera;float origin[3];uint32_t xword;
        if(signed_word(get(a,active+0x58))<=0 || game_memory_read_u8(m,active+0xc)!=35)return !m->fault;
        camera=get(a,0x58f890);
        if(game_memory_read_u16(m,camera+4)!=3)return !m->fault;
        for(i=0;i<3;i++)origin[i]=real(a,camera+12+i*4);
        angles[0]=game_memory_read_u16(m,camera+6);angles[1]=game_memory_read_u16(m,camera+8);
        if(m->fault || !game_input_camera_ray(a,origin,angles[0],angles[1],position))return 0;
        memcpy(&xword,position,4);
        if(xword==0xbf800000u){
            position[0]=(float)((long double)real(a,0x6298a8+(angles[0]&2047u)*4)*real(a,0x574a00)+origin[0]);
            position[2]=(float)((long double)real(a,0x6298a8+((angles[0]+512u)&2047u)*4)*real(a,0x574a00)+origin[2]);
        }
    }
    memcpy(first,position,4);memcpy(second,position+2,4);*found=1;return !m->fault;
}
/*412420: sample the original narrow signed angular interval using the shared
 * portable CRT generator. Coordinates remain floats, not encoded pointers. */
int game_input_scatter_point(GameApplication *a,int16_t width,uint16_t heading,float radius,const float origin[3],float point[3]){
    int32_t product=(int32_t)portable_crt_rand_step(&a->memory)*(int32_t)width;
    int32_t half=width/2;uint16_t angle;
    if(width<0 && width%2)--half;
    angle=(uint16_t)(heading+(uint16_t)(product/32768-half));
    point[2]=(float)((long double)real(a,0x6298a8+((angle+512u)&2047u)*4)*radius+origin[2]);
    point[0]=(float)((long double)real(a,0x6298a8+(angle&2047u)*4)*radius+origin[0]);
    return !a->memory.fault;
}
static void ground_center(GameApplication *a,float point[3]){
    unsigned axis;for(axis=0;axis<3;axis+=2)
        point[axis]=(float)((long double)signed_word(truncate_word(point[axis])&0xfffffe00u)+real(a,0x574a0c));
}
static int ground_blocked(GameApplication *a,const float point[3]){
    uint32_t x=(uint32_t)grid_cell(point[0])&255u,z=(uint32_t)grid_cell(point[2])&255u;
    return !!(game_memory_read_u8(&a->memory,0x610520+z*32+(x>>3))&(1u<<(x&7)));
}
/*4d71c0: camera terrain hit, tile-centre quantization and bounded randomized
 * fallback around the active object's existing origin. The final failed draw
 * still advances RNG, exactly as the original twenty-attempt loop does. */
int game_input_ground_target(GameApplication *a,uint32_t *x,uint32_t *z,int *found){
    GameMemory *m=&a->memory;GameAddress active=get(a,get(a,0x5f0208)+0x448),camera=get(a,0x58f890);
    int vehicle=game_memory_read_u8(m,active+12)==32;uint16_t heading,pitch;float point[3],origin[3],radius;
    uint32_t bits;unsigned axis,attempt;
    *found=0;heading=(uint16_t)(game_memory_read_u16(m,active+0x78)+game_memory_read_u16(m,active+(vehicle?0x26e:0x1ba)));
    pitch=game_memory_read_u16(m,active+(vehicle?0x226:0x1b8));radius=vehicle?7000.0f:10000.0f;
    for(axis=0;axis<3;++axis)origin[axis]=real(a,camera+12+axis*4);
    if(m->fault || !game_input_camera_ray(a,origin,heading,pitch,point))return 0;
    memcpy(&bits,point,4);
    if(bits!=0xbf800000u){
        ground_center(a,point);memcpy(&bits,point,4);
        if(bits!=0xbf800000u && !ground_blocked(a,point))goto selected;
    }
    for(axis=0;axis<3;++axis)origin[axis]=real(a,active+0x64+axis*4);
    bits=0x40360b61u;{float width;memcpy(&width,&bits,4);
        for(attempt=0;attempt<=20;++attempt){
            if(!game_input_scatter_point(a,(int16_t)(uint16_t)truncate_word(width),heading,radius,origin,point))return 0;
            ground_center(a,point);
            if(attempt==20)return !m->fault;
            width=(float)((long double)width+real(a,0x574a14));
            if(!ground_blocked(a,point))goto selected;
        }
    }
    return !m->fault;
selected:
    memcpy(x,point,4);memcpy(z,point+2,4);*found=1;return !m->fault;
}
/*Both original keyboard callers publish when either query succeeds. Failed
 * outputs formerly came from uninitialized stack slots; use explicit zeros. */
int game_input_target_coordinates(GameApplication *a,uint16_t coordinates[4],int *found){
    uint32_t words[4]={0,0,0,0};int screen,ground;unsigned i;
    if(!game_input_screen_target(a,words,words+1,&screen) || !game_input_ground_target(a,words+2,words+3,&ground))return 0;
    *found=screen || ground;
    for(i=0;i<4;++i){float value;memcpy(&value,words+i,4);coordinates[i]=(uint16_t)truncate_word((long double)value*real(a,0x5749d8));}
    return !a->memory.fault;
}

/*411910: finite horizontal terrain march used by the presentation selector.
 * Retain explicit float publications between samples and original bit bounds. */
int game_input_camera_terrain_limit(GameApplication *a,const float origin[3],uint16_t pitch,
        uint16_t heading,float distance,float hit[3],int *found){
    uint16_t inverse=(uint16_t)(0u-pitch);float step=real(a,0x571f24);
    float direction[3],increment[3],accumulated[3],previous[3],point[3];
    float clearance,travel=512.0f,limit;unsigned i,iterations=0;uint32_t bits;
    long double ground;
    *found=0;
    direction[0]=real(a,0x6298a8+(heading&2047u)*4);
    direction[1]=(float)((long double)real(a,0x6298a8+(inverse&2047u)*4)/
        real(a,0x6298a8+((inverse+512u)&2047u)*4));
    direction[2]=real(a,0x6298a8+((heading+512u)&2047u)*4);
    ground=gameplay_geometry_terrain_height(&a->memory,origin[0],origin[2]);
    clearance=(float)((long double)origin[1]-ground);
    if((long double)origin[1]-ground<real(a,0x571f1c) || isnan(ground)){
        memcpy(hit,origin,3*sizeof(float));*found=1;return !a->memory.fault;
    }
    limit=(float)((long double)distance+step);memcpy(&bits,&limit,4);
    if(signed_word(bits)<=0x44000000)goto miss;
    for(i=0;i<3;i++){
        increment[i]=(float)((long double)direction[i]*step);
        accumulated[i]=increment[i];previous[i]=origin[i];
    }
    do{
        float gap;
        if(++iterations>65536)goto miss;
        for(i=0;i<3;i++)point[i]=(float)((long double)origin[i]+accumulated[i]);
        for(i=0;i<3;i+=2){
            memcpy(&bits,point+i,4);
            if(bits>0x80000000u || signed_word(bits)>0x48000000)goto miss;
        }
        ground=gameplay_geometry_terrain_height(&a->memory,point[0],point[2]);
        if(ground<point[1] || isnan(ground)){
            clearance=(float)((long double)point[1]-(float)ground);
            for(i=0;i<3;i++){
                accumulated[i]=(float)((long double)accumulated[i]+increment[i]);
                previous[i]=point[i];
            }
            {long double next=(long double)travel+step;
             travel=(float)next;if(!(next<limit) && !isnan(next))goto miss;}
        }else{
            long double difference=ground-point[1],fraction;
            gap=(float)difference;memcpy(&bits,&clearance,4);
            if(difference<real(a,0x571f14) || isnan(difference) || signed_word(bits)<=0)goto miss;
            fraction=(long double)gap/((long double)clearance+gap);
            for(i=0;i<3;i++)hit[i]=(float)((long double)point[i]-((long double)point[i]-previous[i])*fraction);
            *found=1;return !a->memory.fault;
        }
    }while(!a->memory.fault);
miss:
    hit[0]=hit[1]=hit[2]=-1.0f;return !a->memory.fault;
}

/*4a4aa0: active tracking camera's borrowed scene identity. */
int game_level_camera_subject(GameApplication *a,GameAddress *subject){
    *subject=get(a,get(a,0x58f890)+0x28);return !a->memory.fault;
}
extern int game_campaign_project_point(GameApplication *,float[3]);
/*4b2f00: closest eligible projected object under the current cursor. */
int game_level_scene_overlay(GameApplication *a,GameAddress object){
    GameAddress matrix;float point[3];uint32_t depth;unsigned i;
    if(get(a,0x603710) || get(a,get(a,0x5f0208)+0x448)==object)return !a->memory.fault;
    matrix=get(a,object+0x54);
    for(i=0;i<3;i++)point[i]=real(a,matrix+0x10+i*0x10);
    if(!game_campaign_project_point(a,point))return 0;
    if(!((long double)signed_word(get(a,0x5acce0))>=point[0]) ||
       !((long double)signed_word(get(a,0x5acce4))>=point[1]) ||
       !((long double)signed_word(get(a,0x5ac920))<=point[0]) ||
       !((long double)signed_word(get(a,0x5ac924))<=point[1]))return !a->memory.fault;
    memcpy(&depth,point+2,4);
    if(signed_word(depth)<=0x41200000)return !a->memory.fault;
    if(!get(a,0x6038e4) || point[2]<real(a,0x603880) || isnan(point[2]) || isnan(real(a,0x603880))){
        game_memory_write_u32(&a->memory,0x6038e4,object);
        game_memory_write_u32(&a->memory,0x603880,depth);
    }
    return !a->memory.fault;
}
/*4b2fe0: locked-generation grace period, camera ray and occlusion selection. */
int game_level_scene_overlay_flush(GameApplication *a){
    GameAddress object,camera,position,player;float origin[3],target[3],hit[3],distance=8192.0f;
    float terrain_delta[3],object_delta[3];uint16_t pitch,heading;unsigned i;int found;
    if(get(a,0x603710)){
        object=get(a,0x60387c);
        if(get(a,object+0x14)==get(a,0x603e38) && get(a,0x5ac6ac)-get(a,0x58fe00)<=320u)return !a->memory.fault;
        game_memory_write_u32(&a->memory,0x603710,0);
    }
    game_memory_write_u32(&a->memory,0x60387c,0);game_memory_write_u32(&a->memory,0x603e38,0);
    object=get(a,0x6038e4);camera=get(a,0x58f890);
    for(i=0;i<3;i++)origin[i]=real(a,camera+12+i*4);
    if(object){
        float dx,dz,argument;
        position=object+((get(a,object+16)&1)?0x64:0x18);
        dx=(float)((long double)origin[0]-real(a,position));
        dz=(float)((long double)origin[2]-real(a,position+8));
        argument=(float)((long double)dz*dz+(long double)dx*dx);
        distance=(float)gameplay_geometry_square_root(argument);
    }
    if(game_memory_read_u16(&a->memory,camera+4)==1){
        player=get(a,get(a,0x5f0208)+0x448);
        heading=game_memory_read_u16(&a->memory,player+0x24);
        if(game_memory_read_u8(&a->memory,player+12)==0x20){
            heading=(uint16_t)(heading+game_memory_read_u16(&a->memory,player+0x26c));
            pitch=game_memory_read_u16(&a->memory,player+0x224);
        }else{
            heading=(uint16_t)(heading+game_memory_read_u16(&a->memory,player+0x14c));
            pitch=game_memory_read_u16(&a->memory,player+0x14a);
        }
        for(i=0;i<3;i++)origin[i]=real(a,player+24+i*4);
    }else{
        pitch=game_memory_read_u16(&a->memory,camera+8);heading=game_memory_read_u16(&a->memory,camera+6);
    }
    if(!game_input_camera_terrain_limit(a,origin,pitch,heading,distance,hit,&found))return 0;
    for(i=0;i<3;i++){uint32_t word;memcpy(&word,hit+i,4);game_memory_write_u32(&a->memory,0x603758+i*4,word);}
    object=get(a,0x6038e4);if(!object)return !a->memory.fault;
    if(get(a,0x603758)!=0xbf800000u){
        long double terrain_squared,object_squared;
        camera=get(a,0x58f890);position=object+((get(a,object+16)&1)?0x64:0x18);
        for(i=0;i<3;i++){
            target[i]=real(a,camera+12+i*4);
            terrain_delta[i]=(float)((long double)target[i]-hit[i]);
            object_delta[i]=(float)((long double)target[i]-real(a,position+i*4));
        }
        terrain_squared=(long double)terrain_delta[1]*terrain_delta[1]+(long double)terrain_delta[2]*terrain_delta[2];
        terrain_squared+=(long double)terrain_delta[0]*terrain_delta[0];
        object_squared=(long double)object_delta[2]*object_delta[2]+(long double)object_delta[1]*object_delta[1];
        object_squared+=(long double)object_delta[0]*object_delta[0];
        if(!(object_squared<=terrain_squared) && !isnan(object_squared) && !isnan(terrain_squared))return !a->memory.fault;
    }
    game_memory_write_u32(&a->memory,0x60387c,object);
    game_memory_write_u32(&a->memory,0x603e38,get(a,object+0x14));return !a->memory.fault;
}
