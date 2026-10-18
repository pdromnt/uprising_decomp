#include "level_run.h"
#include "gameplay_root.h"
#include "gameplay_catalog.h"
#include "gameplay_geometry.h"
#include "gameplay_input.h"
#include "menu_sound.h"
#include "scene_projection_storage.h"
#include "effect_resource_pool.h"
#include "event_queue.h"
#include "level_script.h"
#include "level_objects.h"
#include "level_factory.h"
#include "level_player_slots.h"
#include "game_intrusive_list.h"
#include "state_helper_plans.h"
#include "display_transition.h"
#include "level_player_resources.h"
#include "presentation_instance.h"
#include "mesh_buffer.h"
#include "level_player_scene.h"
#include "gameplay_messages.h"
#include "gameplay_state.h"
#include "game_state_ops.h"
#include "level_effect_runtime.h"
#include "scene_orders.h"
#include "terrain_asset.h"
#include "radar_compositor.h"
#include "menu_network.h"
#include "gameplay_notices.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

/*Distinct remaining executable programs. These are strong native dependencies,
 * not a guest-call port or success placeholders; the candidate must still fail
 * until their actual supported behaviors exist. */
extern int game_level_root_event_apply(GameApplication *,GameAddress,GameAddress); /*47c9a0*/
extern int game_level_scene_update(GameApplication *,GameAddress,GameAddress);
extern int game_level_citadel_finish(GameApplication *,GameAddress); /*549b40*/
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_level_clock_observe_event(GameApplication *,GameAddress);
extern int game_level_catalog_refresh_selection(void *,GameAddress);
extern int game_level_group_remove_member(GameApplication *,GameAddress,unsigned);
extern int game_level_citadel_upgrade(GameApplication *,GameAddress,uint8_t,uint16_t *);
extern int game_application_options_load(GameApplication *,uint32_t);
extern int game_level_display_mask_resize(GameApplication *,uint32_t);
extern int game_level_group_produce(GameApplication *,GameAddress,uint8_t,uint32_t,uint32_t,uint32_t *);
/*The reserve and secondary programs below share existing factories and owners. */
extern int game_level_root_reserve_produce(GameApplication *,GameAddress,uint8_t,uint32_t,uint32_t,float,float,uint32_t *);
extern int game_level_station_produce(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t *);
extern int game_level_imperial_unit_activate(GameApplication *,GameAddress,uint32_t,const float[3]);
extern int game_level_descriptor_unit_refund(GameApplication *,GameAddress,uint8_t,int32_t *);
extern int game_level_group_repair_quote(GameApplication *,GameAddress,GameAddress,uint32_t *,uint32_t *,uint32_t *);
extern int game_level_group_repair_apply(GameApplication *,GameAddress,uint32_t);
extern int game_level_catalog_return_health(GameApplication *,GameAddress,uint8_t,float);
extern int game_level_player_station(GameApplication *,GameAddress,unsigned);
extern int game_level_scene_target_order(void *,GameAddress,uint32_t,GameAddress);
extern int game_level_scene_history_order(GameApplication *,GameAddress,unsigned,GameAddress,const float[3]);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t value){game_memory_write_u32(&a->memory,p,value);}
static int32_t signed_word(uint32_t word){int32_t value;memcpy(&value,&word,4);return value;}
static uint32_t truncate_word(long double value){return (uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);}
static float spawn_real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static void spawn_store(GameApplication *a,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);put(a,p,bits);}
static int spawn_random(GameApplication *a,GameAddress scale,int32_t *out){
    uint32_t index=get(a,0x6308c0);
    if(index==1024)index=0;
    if(index>1024 || a->memory.fault)return 0;
    put(a,0x6308c0,index+1);
    *out=signed_word(truncate_word((long double)spawn_real(a,0x6308c8+index*4)*spawn_real(a,scale)));
    return !a->memory.fault;
}
/*The initializer's piecewise inverse-sine tables are distinct from the camera
 *angle tables. Preserve the original unsigned negative-float bit comparison. */
static uint16_t spawn_slope_angle(GameApplication *a,float v,int upper){
    uint32_t bits,index;GameAddress table;uint16_t result;int negative=0;
    memcpy(&bits,&v,4);
    if(upper){index=truncate_word(((long double)v-spawn_real(a,0x5720f8))*spawn_real(a,0x5720fc));table=0x62b8a8;}
    else if(bits>0xbf666666u){index=truncate_word(((long double)spawn_real(a,0x572100)-v)*spawn_real(a,0x5720fc));table=0x62b8a8;negative=1;}
    else{index=truncate_word((long double)v*spawn_real(a,0x572104)+spawn_real(a,0x572104));table=0x62f8b8;}
    if(index>8192u){a->memory.fault=1;a->memory.fault_address=table;return 0;}
    result=game_memory_read_u16(&a->memory,table+index*2);
    return negative?(uint16_t)(0u-result):result;
}
static void spawn_support_normal(const float u[3],const float v[3],float n[3]){
    float square,length;unsigned i;
    n[0]=(float)((long double)u[1]*v[2]-(long double)u[2]*v[1]);
    n[1]=(float)((long double)u[2]*v[0]-(long double)u[0]*v[2]);
    n[2]=(float)((long double)u[0]*v[1]-(long double)u[1]*v[0]);
    square=(float)(((long double)n[1]*n[1]+(long double)n[2]*n[2])+(long double)n[0]*n[0]);
    length=(float)sqrtl(square);
    if(length!=1.0f && length!=0.0f && !isnan(length))
        for(i=0;i<3;i++)n[i]=(float)((1.0L/length)*n[i]);
}
/*419480: select the real variant mesh/hull, randomized terrain alignment and
 *support plane, then initialize the stationary or falling shared pose. The
 *existing variant asset producers remain required; absent inputs fail here. */
int game_level_spawn_initialize(GameApplication *a,GameAddress object,uint32_t id,
        const float input[3],uint32_t variant,uint32_t payload,uint32_t mode){
    GameMemory *m=&a->memory;PresentationPose *pose=presentation_instance_pose(object);
    GameAddress source;uint32_t count,bits;float normal[3],p[3],s,c,projected,ratio;
    float xp,xm,zp,zm,h[4],u[3],v[3],support[3],landing;long double projection,dot;
    uint16_t angles[3];int32_t random,jitter[3];unsigned i;
    if(!pose || variant>8 || m->fault)return 0;
    put(a,object+0xd4,variant);
    if(variant!=3){int32_t amount=signed_word(payload);if(amount<1)payload=1;
        else if(variant==1 && amount>100)payload=100;}
    put(a,object+0xd8,payload);
    if(!get(a,0x5acd38+variant*4) && !game_level_spawn_variant_load(a,variant))return 0;
    source=get(a,0x5acd38+variant*4);count=get(a,0x5acce8+variant*4);
    if(!source || count>20 || m->fault)return 0;
    source=get(a,source+0x50);
    if(!source || !mesh_buffer_bind(m,pose->model,source))return 0;
    pose->hull=get(a,0x5acd10+variant*4);pose->vertex_count=(uint16_t)count;
    pose->radius=(float)((long double)spawn_real(a,source+0x20)*spawn_real(a,0x5720ec));
    for(i=0;i<3;i++)pose->dimensions[i]=spawn_real(a,source+0x24+i*4);
    game_memory_write_u8(m,object+0xdc,(uint8_t)mode);
    if(!spawn_random(a,0x5720f4,&random))return 0;
    angles[0]=(uint16_t)random;
    if(!gameplay_geometry_terrain_normal_values(m,normal,input[0],input[2]))return 0;
    s=spawn_real(a,0x6298a8+((uint32_t)angles[0]&2047)*4);
    c=spawn_real(a,0x6298a8+(((uint32_t)angles[0]+512)&2047)*4);
    projection=(long double)s*normal[2]-(long double)c*normal[0];projected=(float)projection;
    angles[2]=spawn_slope_angle(a,projected,projection>spawn_real(a,0x5720f8));
    if(s==0.0f)ratio=(float)(((long double)normal[2]-(long double)projected*s)/
        ((long double)spawn_real(a,0x6298a8+(((uint32_t)angles[2]+512)&2047)*4)*c));
    else ratio=(float)(((long double)projected*c+normal[0])/
        ((long double)spawn_real(a,0x6298a8+(((uint32_t)angles[2]+512)&2047)*4)*s));
    memcpy(&bits,&ratio,4);angles[1]=spawn_slope_angle(a,ratio,signed_word(bits)>0x3f666666);
    if(!spawn_random(a,0x572108,&random))return 0;
    angles[1]=(uint16_t)((uint32_t)angles[1]+(uint32_t)random-10u);
    if(!spawn_random(a,0x572108,&random))return 0;
    angles[2]=(uint16_t)((uint32_t)angles[2]+(uint32_t)random-10u);
    xp=(float)((long double)input[0]+pose->dimensions[0]);xm=(float)((long double)input[0]-pose->dimensions[0]);
    zp=(float)((long double)input[2]+pose->dimensions[2]);zm=(float)((long double)input[2]-pose->dimensions[2]);
    h[0]=gameplay_geometry_terrain_height(m,xp,zp);h[1]=gameplay_geometry_terrain_height(m,xm,zp);
    h[2]=gameplay_geometry_terrain_height(m,xm,zm);h[3]=gameplay_geometry_terrain_height(m,xp,zm);
    u[0]=(float)((long double)xp-xm);u[1]=(float)((long double)h[0]-h[1]);u[2]=0;
    v[0]=0;v[1]=(float)((long double)h[2]-h[1]);v[2]=(float)((long double)zm-zp);
    spawn_support_normal(u,v,support);
    dot=(((long double)h[3]-h[1])*support[1]+(long double)u[0]*support[0])+
        (long double)v[2]*support[2];
    if(dot>spawn_real(a,0x5720f0)){
        u[0]=0;u[1]=(float)((long double)h[1]-h[2]);u[2]=(float)((long double)zp-zm);
        v[0]=(float)((long double)xp-xm);v[1]=(float)((long double)h[3]-h[2]);v[2]=0;
        spawn_support_normal(u,v,support);
    }
    landing=(float)(gameplay_geometry_terrain_height(m,input[0],input[2])+
        ((long double)pose->dimensions[1]-spawn_real(a,0x57210c))/support[1]);
    spawn_store(a,object+0xe4,landing);memcpy(p,input,sizeof(p));p[1]=landing;
    if(mode&1){p[1]=(float)((long double)landing+spawn_real(a,0x572110));
        for(i=0;i<3;i++){if(!spawn_random(a,0x572114,&random))return 0;jitter[i]=signed_word((uint32_t)random-30u);
            angles[i]=(uint16_t)(((uint32_t)angles[i]-60u*(uint32_t)jitter[i])&2047);
            if(i && angles[i]>1024)angles[i]=(uint16_t)(angles[i]-2048);}
    }
    if(!presentation_pose_export(m,object,pose) || !game_level_scene_object_place(a,object,id,p,angles))return 0;
    memcpy(pose->local_position,pose->position,sizeof(pose->position));
    for(i=0;i<3;i++)pose->local_angles[i]=pose->angles[2-i];
    pose->local_phase=0;
    if(!presentation_pose_build_spawn_controller(m,pose,(mode&1)!=0) || !presentation_pose_transform_vertices(m,pose,1))return 0;
    put(a,object+0xb0,0);
    for(i=0;i<3;i++)spawn_store(a,object+0xb4+i*4,(mode&1)?0.0f:(float)((long double)pose->world_matrix[i*4+2]*0.0f));
    game_memory_write_u16(m,object+0xcc,(mode&1)?(uint16_t)jitter[0]:0);
    game_memory_write_u16(m,object+0xd0,(mode&1)?(uint16_t)jitter[1]:0);
    game_memory_write_u16(m,object+0xce,(mode&1)?(uint16_t)jitter[2]:0);
    if(!(mode&1) && (mode&2))put(a,object+0xe0,get(a,0x624f8c)==10?get(a,0x5ac6ac):0);
    return presentation_pose_export(m,object,pose) && !m->fault;
}
static GameAddress spawn_acquire(void *c){return game_level_factory_take(c,0x10,0);}
static int spawn_initialize(void *c,GameAddress o,uint32_t id,const float p[3],uint32_t variant,uint32_t payload,uint32_t mode){
    GameApplication *a=c;
    return get(a,get(a,o)+0xac)==0x419480 && game_level_spawn_initialize(a,o,id,p,variant,payload,mode);
}
static int spawn_publish(void *c,GameAddress root,GameAddress o){
    GameApplication *a=c;uint32_t head=state_helper_flagged_member_offset(get(a,o+16));
    if(level_player_slots_owns_player(root) && !level_player_slots_attach_scene_member(root,o,head))return 0;
    game_list_insert_after(&a->memory,o,root+head,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(o);return 0;}return 1;
}
static int spawn_recycle(void *c,GameAddress o){GameApplication *a=c;GameAddress pool=get(a,o+0x50);
    if(!pool || a->memory.fault)return 0;
    game_list_insert_after(&a->memory,o,pool,4,8);return !a->memory.fault;
}
static int spawn_sound(void *c){SoundPlaybackRequest r={125,0,1,0,0,0};return game_menu_sound_play(c,&r);}
int game_level_root_spawn(GameApplication *a,GameAddress root,uint32_t mode,uint32_t width,uint32_t first,uint32_t last,uint32_t variant,uint32_t payload,int *out){
    GameAddress result;GameplayRootSpawnServices s={a,spawn_acquire,spawn_initialize,spawn_publish,spawn_recycle,spawn_sound};
    if(!gameplay_root_random_spawn(&a->memory,root,mode,width,first,last,variant,payload,&s,&result))return 0;
    *out=result!=0;return 1;
}
/*480b30 and4808b0 share the actual pickup factory, initializer, publication
 *and sound. Return the new identity so the script registry can borrow it. */
int game_level_root_pickup_create(GameApplication *a,GameAddress root,int falling,
        const float position[3],uint32_t minimum,uint32_t maximum,uint32_t variant,uint32_t payload,GameAddress *out){
    GameplayRootSpawnServices services={a,spawn_acquire,spawn_initialize,spawn_publish,spawn_recycle,spawn_sound};
    GameAddress object;uint32_t sequence,identity;
    *out=0;
    if(falling)return gameplay_root_random_spawn(&a->memory,root,1,2048,minimum,maximum,variant,payload,&services,out);
    object=spawn_acquire(a);if(!object)return 0;
    sequence=get(a,root+24);put(a,root+24,sequence+1);
    identity=(sequence<<6)|game_memory_read_u16(&a->memory,root+8);
    if(!spawn_initialize(a,object,identity,position,variant,payload,2)){
        if(!a->memory.fault)(void)spawn_recycle(a,object);return 0;
    }
    if(!spawn_publish(a,root,object) || (root==get(a,0x5f0208) && !spawn_sound(a)))return 0;
    *out=object;return !a->memory.fault;
}
/*481430 calls the actual slot44 health getter for players and selected
 *descriptors as well as vehicles. Share its existing native implementation. */
static int maximum(void *context,GameAddress object,uint32_t *out){
    GameApplication *a=context;GameAddress method=get(a,get(a,object)+0x44);
    int ok=game_level_scene_property(a,object,method,out);
    if(!ok)fprintf(stderr,"native root health getter failed: object=%08x method=%08x\n",object,method);
    return ok;
}
static int idle(void *c,GameAddress root){GameApplication *a=c;int ok=gameplay_root_choose_action(&a->memory,root,a,maximum);
    if(!ok)fprintf(stderr,"native root failed: action selection root=%08x\n",root);return ok;}
static int camera(void *c,GameAddress camera,GameAddress scene){GameApplication *a=c;return game_camera_bind_scene(&a->memory,camera,scene);}
static int sound(void *c){int ok=game_menu_sound_update(c);if(!ok)fprintf(stderr,"native root failed: sound\n");return ok;}
static int notice(void *c,GameAddress root){
    GameApplication *a=c;EffectResourcePool *pool=effect_resource_pool_find(0x5cee40);GameAddress event;
    if(!pool)return 0;
    event=effect_resource_pool_acquire_kind(&a->memory,pool,3);if(!event)return 0;
    put(a,event+12,(game_menu_milliseconds(a)-get(a,0x5ac90c))>>5);
    game_memory_write_u8(&a->memory,event+16,0x19);put(a,event+20,game_memory_read_u16(&a->memory,root+8));
    put(a,root+0x580,1);
    if(a->memory.fault || !game_event_queue_insert_at(&a->memory,root+0x458,root+0x45c,event)){
        GameMemory cleanup=a->memory;cleanup.fault=0;(void)effect_resource_pool_return(&cleanup,pool,event);return 0;}
    return !get(a,0x5eff04) || game_level_clock_observe_event(a,event);
}
/*47dfe0 routes a packet to its matching controller, or returns unmatched
 * packets to their actual native pool. The receiving ordered queue owns it. */
static int network(void *c,GameAddress root){
    GameApplication *a=c;GameAddress event,player,group,member,queue;unsigned visits=0;
    while((event=get(a,root+0x460)) && get(a,event+12)==get(a,0x5ac6ac)){
        uint32_t identity=get(a,event+20);queue=0;
        if(a->memory.fault || ++visits>65536)return 0;
        put(a,root+0x460,get(a,event+4));put(a,event+4,0);
        if(get(a,root+0x464)==event)put(a,root+0x464,0);
        player=get(a,root+0x3bc);
        if(get(a,player+0x14)==identity)queue=player+0x13c;
        else{unsigned groups=0;group=get(a,root+0x294);
            while(group){
                if(++groups>65536)return 0;
                member=get(a,get(a,group+0x18));
                if(member && get(a,get(a,member+0x48)+0x14)==identity){queue=get(a,member+0x48)+0x264;break;}
                group=get(a,group+0x64);
            }
        }
        if(a->memory.fault)return 0;
        if(queue){if(!game_event_queue_insert_at(&a->memory,queue+0x28,queue+0x2c,event))return 0;}
        else{EffectResource *record=effect_resource_find(event);
            if(!record || effect_resource_pool_return(&a->memory,record->owner,event)!=1)return 0;}
    }
    return !a->memory.fault;
}
static int value(void *c,GameAddress group,uint32_t *out){
    GameApplication *a=c;int ok;*out=0;
    if(!get(a,get(a,group+0x18)))return !a->memory.fault;
    ok=game_level_player_catalog_value(a,get(a,group+0x54),0x23,5,0,out);
    if(!ok)fprintf(stderr,"native root failed: citadel income group=%08x owner=%08x\n",group,get(a,group+0x54));return ok;
}
static int credit(void *c,GameAddress root,GameAddress method,uint32_t amount){
    GameApplication *a=c;
    if(method==0x47f250)return gameplay_catalog_credit(&a->memory,root,amount);
    if(method==0x477750)return !a->memory.fault; /*Actual ret4 base credit. */
    fprintf(stderr,"native root failed: credit method=%08x\n",method);
    return 0;
}
static int spawn(void *c,GameAddress root,uint32_t first,uint32_t second,uint32_t kind,uint32_t payload,int *out){return game_level_root_spawn(c,root,1,0x800,first,second,kind,payload,out);}
static int control_voice_clear(void *a,GameAddress bank){return game_menu_sound_queue_clear(a,bank);}
static int control_voice_submit(void *a,GameAddress bank,const int16_t sounds[8],int *accepted){return game_menu_sound_queue_submit(a,bank,sounds,accepted);}
static int control_mask_resize(void *a,uint32_t diameter){return game_level_display_mask_resize(a,diameter);}
static int control_sound_stop(void *a,int16_t sound){return game_menu_sound_stop(a,sound,0);}
/*49ed80/4a2230 and root events31/32 use the real mode, sound, radar and
 *display owners. Queue/widget classes are selected by the existing native
 *tables; an unsupported class faults rather than calling a guest method. */
static int root_selector_reveal(void *c,GameAddress position,float radius){
    return game_radar_reveal(&((GameApplication *)c)->memory,position,radius);
}
static int root_selector_perform(void *c,GameRootControlAction action,uint32_t value){
    GameApplication *a=c;
    switch(action){
    case GAME_ROOT_OPEN_SOUND:case GAME_ROOT_CONFIRM_SOUND:return game_menu_sound(a,value);
    case GAME_ROOT_LOOP_SOUND:{
        uint32_t bits=0x3f6147ae;float volume;SoundPlaybackRequest request={(int16_t)value,0,0,0,1,0};
        memcpy(&volume,&bits,4);request.volume=volume;return game_menu_sound_play(a,&request);
    }
    case GAME_ROOT_STOP_LOOP_SOUND:return game_menu_sound_stop(a,(int16_t)value,0);
    case GAME_ROOT_SET_APPLICATION_MODE:return game_application_options_load(a,value);
    case GAME_ROOT_SET_DISPLAY_CONTROL:{GameDisplayMaskServices services={a,control_mask_resize};return game_display_configure_control(&a->memory,value,&services);}
    case GAME_ROOT_REFRESH_SELECTOR:return gameplay_selector_refresh(&a->memory,value!=0,root_selector_reveal,a);
    }
    return 0;
}
static int root_selector_playing(void *c,uint32_t *out){
    int playing;if(!game_menu_sound_playing(c,9,0,&playing))return 0;*out=(uint32_t)playing;return 1;
}
static int root_selector_queue(void *c,GameAddress queue,int begin){
    GameApplication *a=c;GameAddress method=get(a,get(a,queue)+(begin?12u:16u));
    if(a->memory.fault)return 0;
    if(begin && method==0x5533a0){
        GamePresentationFeedbackServices voice={a,control_voice_clear,control_voice_submit};
        return gameplay_controller_activate_attachment(&a->memory,queue,&voice);
    }
    if(!begin && method==0x5535b0){
        GameplayControllerServices reset={0};reset.context=a;reset.stop_sound=control_sound_stop;
        return gameplay_controller_reset_attachment(&a->memory,queue,&reset);
    }
    fprintf(stderr,"native selector attachment failed: queue=%08x method=%08x begin=%d\n",(unsigned)queue,(unsigned)method,begin);return 0;
}
static int root_selector_event(GameApplication *a,GameAddress root,GameAddress event){
    GameRootControlServices services={a,root_selector_perform,root_selector_playing,root_selector_queue,NULL};
    return game_root_control_event(&a->memory,root,event,&services);
}
static int station_ready(GameApplication *a,GameAddress group,GameAddress *payload){
    GameAddress member=get(a,get(a,group+0x18));*payload=0;
    if(member && get(a,member+0x10)==1){*payload=get(a,member+0x48);return *payload && get(a,*payload+0x2b4)==30;}
    return 0;
}
/*47cc77/47cf23: station control reuses the embedded input attachment and its
 *cached matrices. Returning to Wraith uses the existing shared reset program. */
static int station_select(GameApplication *a,GameAddress root,int cycle){
    GameAddress player=get(a,root+0x3bc),previous=get(a,root+0x448),group=0,origin,node,payload=0,camera;
    uint32_t previous_queue;unsigned visits=0;int unavailable=0,offline=0;char text[96];
    GamePresentationFeedbackServices voice={a,control_voice_clear,control_voice_submit};
    GameplayControllerServices reset={0};GameDisplayMaskServices display={a,control_mask_resize};
    if(!player || !previous)return 0;
    if(!get(a,player+0x58) || get(a,root+0x5a0) || (cycle?(previous==player):(previous!=player)))return !a->memory.fault;
    if(cycle){
        origin=get(a,get(a,previous+0x294)+4);if(!origin)return 0;
        node=get(a,origin+0x64);
        while(node!=origin){
            if(++visits>65536)return 0;
            if(!node)node=get(a,root+0x294);if(!node)return 0;
            if(station_ready(a,node,&payload)){group=node;break;}
            node=get(a,node+0x64);if(a->memory.fault)return 0;
        }
        if(!group){group=origin;payload=get(a,get(a,get(a,origin+0x18))+0x48);}
        if(payload==previous)return !a->memory.fault;
        previous_queue=previous+0x264;
        if(get(a,get(a,previous_queue)+16)!=0x5535b0)return 0;
        reset.context=a;reset.stop_sound=control_sound_stop;
        if(!gameplay_controller_reset_attachment(&a->memory,previous_queue,&reset))return 0;
    }else{
        for(node=get(a,root+0x294);node;node=get(a,node+0x64)){
            GameAddress member=get(a,get(a,node+0x18));
            if(++visits>65536 || a->memory.fault)return 0;
            if(!member)continue;
            if(get(a,member+0x10)!=1){unavailable=1;continue;}
            payload=get(a,member+0x48);
            if(payload && get(a,payload+0x2b4)==30){group=node;break;}
            offline=1;
        }
        if(!group){
            if(get(a,0x58973c)!=get(a,0x5f0208))return !a->memory.fault;
            return game_message_enqueue(a,get(a,0x628a88),unavailable || offline?0x5898d0:0x5898e4,1);
        }
        previous_queue=player+0x13c;
        if(get(a,get(a,previous_queue)+16)!=0x558c30)return 0;
        put(a,previous_queue+4,0);put(a,previous_queue+8,0);
    }
    if(!payload || get(a,get(a,payload+0x264)+12)!=0x5533a0)return 0;
    put(a,root+0x448,payload);
    if(!cycle)put(a,root+0x450,payload);
    if(!cycle && root==get(a,0x5f0208)){
        camera=get(a,0x58f8a8);if(camera && !game_camera_bind_scene(&a->memory,camera,payload))return 0;
    }
    if(!gameplay_controller_activate_attachment(&a->memory,payload+0x264,&voice))return 0;
    if(cycle){
        put(a,root+0x450,payload);
        if(root==get(a,0x5f0208)){
            camera=get(a,0x58f8a8);if(camera && !game_camera_bind_scene(&a->memory,camera,payload))return 0;
        }
    }
    if(get(a,0x58973c)!=get(a,0x5f0208))return !a->memory.fault;
    if(cycle){if(!game_menu_sound_stop(a,129,0))return 0;}
    else{
        if(!game_display_configure_control(&a->memory,1,&display) || !game_menu_sound_stop(a,9,0))return 0;
    }
    camera=get(a,0x58f8a0);if(!camera)return 0;
    put(a,camera+0x28,payload);
    if(!game_camera_activate(&a->memory,camera,NULL) || !game_menu_sound(a,14))return 0;
    /*47cda5: camera activation and input mode are separate operations. */
    if(!cycle && !game_application_options_load(a,2))return 0;
    if(cycle)snprintf(text,sizeof(text),"CITADEL TOWER: %d",signed_word(get(a,group)));
    else{
        uint32_t letter=74u-truncate_word((long double)spawn_real(a,payload+24)*spawn_real(a,0x573e38)*spawn_real(a,0x573e3c));
        uint32_t number=truncate_word((long double)spawn_real(a,payload+32)*spawn_real(a,0x573e38)*spawn_real(a,0x573e3c));
        snprintf(text,sizeof(text),"CITADEL TOWER LOCATION: %c %d",(int)letter,signed_word(number));
    }
    return game_message_enqueue_bytes(a,get(a,0x628a88),text,1);
}
static int production_kind(GameApplication *a,GameAddress member,uint8_t *kind){
    GameAddress method=get(a,get(a,member)+0x44),payload;
    if(method==0x53d8d0)*kind=0;
    else if(method==0x53ff60)*kind=11;
    else if(method==0x53f2c0){
        payload=get(a,member+0x48);
        switch(get(a,get(a,payload)+0x90)){
        case 0x51ba30:*kind=0x40;break;case 0x51bb10:*kind=0x21;break;
        case 0x51bba0:*kind=0x27;break;case 0x51bc30:*kind=0x26;break;default:return 0;
        }
    }else return 0;
    return !a->memory.fault;
}
static int production_busy(GameApplication *a,GameAddress member,uint32_t *value){
    GameAddress method=get(a,get(a,member)+0x50),payload=get(a,member+0x48);
    if(method==0x53d900){*value=0;return !a->memory.fault;}
    if(method==0x53f2f0){
        uint32_t elapsed=get(a,0x5ac6ac)-get(a,payload+0xac),duration=get(a,payload+0xa8);
        if(get(a,member+16)!=1)*value=100;
        else if(signed_word(elapsed)>=signed_word(duration))*value=0;
        else{
            int32_t divisor=signed_word(duration),numerator=signed_word(0u-elapsed*100u);
            if(!divisor || (numerator==INT32_MIN && divisor==-1))return 0;
            *value=(uint32_t)(numerator/divisor)+100u;
        }
        return !a->memory.fault;
    }
    if(method==0x53f400){
        float current=spawn_real(a,payload+0xc4),maximum=spawn_real(a,payload+0xc8);
        if(get(a,member+16)!=1)*value=100;
        else if(!(current<maximum))*value=0;
        else *value=truncate_word(spawn_real(a,0x575484)-(long double)current*spawn_real(a,0x575484)/maximum);
        return !a->memory.fault;
    }
    return 0;
}
static int production_pay(GameApplication *a,GameAddress root,uint32_t cost){
    GameAddress method=get(a,get(a,root)+0x30);
    if(method==0x477740)return !a->memory.fault;
    return (method==0x47f230 || method==0x479490) && gameplay_catalog_spend(&a->memory,root,cost);
}
/*47fd70: repair the descriptor found in this controller's actual station list.
 *Paid health includes the descriptor's bias18; charging uses signed retail
 *money arithmetic and the existing catalog owner. */
int game_level_root_descriptor_repair(GameApplication *a,GameAddress root,uint8_t group_id,uint8_t slot){
    GameAddress group,descriptor=0;uint32_t price,health,maximum,amount,fee;float rate;int32_t missing;unsigned visits=0;char text[96];
    for(group=get(a,root+0x294);group;group=get(a,group+0x64)){
        int count;
        if(++visits>65536 || a->memory.fault)return 0;
        if(get(a,group)!=group_id)continue;
        count=(int16_t)game_memory_read_u16(&a->memory,group+0x24);if(count<=0 || slot>=(unsigned)count)return 0;
        descriptor=get(a,get(a,group+0x18)+(unsigned)slot*4);break;
    }
    if(!descriptor)return !a->memory.fault;
    if(!game_level_group_repair_quote(a,descriptor,root,&price,&health,&maximum))return 0;
    rate=(float)(((long double)(float)signed_word(price)/signed_word(maximum))*spawn_real(a,0x573e64));
    /*The original rereads maximum and health after its virtual queries. */
    if(!game_level_group_repair_quote(a,descriptor,root,&price,&health,&maximum))return 0;
    missing=signed_word(maximum-health-get(a,descriptor+0x18));
    if(missing<=0)return root!=get(a,0x5f0208)?!a->memory.fault:game_message_enqueue(a,get(a,0x628a88),0x589a70,1);
    amount=(uint32_t)missing;fee=truncate_word((long double)missing*rate);
    if(signed_word(fee)>signed_word(get(a,root+0x22c))){
        fee=get(a,root+0x22c);amount=truncate_word((long double)signed_word(fee)/rate);
    }
    if(!amount)return !a->memory.fault;
    if(!game_level_group_repair_apply(a,descriptor,amount) || !production_pay(a,root,fee))return 0;
    if(root!=get(a,0x5f0208))return !a->memory.fault;
    snprintf(text,sizeof(text),"Repaired upgrade for $%d",signed_word(fee));
    return game_message_enqueue_bytes(a,get(a,0x628a88),text,1);
}
static int reserve_mask(GameApplication *a,int x,int z){
    return x<0 || x>=256 || z<0 || z>=256 ||
        (game_memory_read_u8(&a->memory,0x610520+(unsigned)z*32+(unsigned)x/8)&(1u<<(x&7)));
}
/*4104f0 is the raw cell floor, deliberately distinct from514dd0's interpolated
 *surface. Borrow the decoded terrain owner when present. */
static float reserve_floor(GameApplication *a,float x,float z){
    uint32_t bx,bz,ix,iz;memcpy(&bx,&x,4);memcpy(&bz,&z,4);
    ix=truncate_word(x)-(bx>0x80000000u);iz=truncate_word(z)-(bz>0x80000000u);
    ix=(ix&0x1fe00u)>>9;iz=(iz&0x1fe00u)>>9;
    if(a->memory.terrain && a->memory.terrain->cells)return a->memory.terrain->cells[iz*256+ix].height;
    return (int16_t)game_memory_read_u16(&a->memory,get(a,0x597d90)+(iz*256+ix)*8);
}
static int reserve_sequence(GameApplication *a,GameAddress root,uint32_t *id){
    if(level_player_slots_owns_player(root)){
        uint32_t next;if(!level_player_slots_next_owner_sequence(&a->memory,root,id,&next))return 0;put(a,root+24,next);
    }else *id=game_state_next_owner_sequence(&a->memory,root);
    return !a->memory.fault;
}
static int reserve_publish(GameApplication *a,GameAddress root,GameAddress object){
    unsigned head=state_helper_flagged_member_offset(get(a,object+16));
    if(level_player_slots_owns_player(root) && !level_player_slots_attach_scene_member(root,object,head))return 0;
    game_list_insert_after(&a->memory,object,root+head,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(object);return 0;}return 1;
}
static int reserve_return(GameApplication *a,GameAddress object){
    GameAddress head=get(a,object+0x50);if(!head)return 0;
    game_list_insert_after(&a->memory,object,head,4,8);return !a->memory.fault;
}
/*47fec0: production caps, target-following AAV placement, widening view-heading
 *search, route/collision checks, arrival publication and pending-unit census.
 *Partial batches retain their accepted units; the unused instance returns to
 *its existing factory. No layout copy or general execution callback is used. */
int game_level_root_reserve_produce(GameApplication *a,GameAddress root,uint8_t kind,uint32_t count,uint32_t heading_range,float base_radius,float step,uint32_t *reason){
    uint32_t produced=0,census=kind==0x21?0x504:kind==0x27?0x508:kind==0x40?0x500:0;
    uint32_t limit=kind==0x21?8:kind==0x27?7:16;
    if(!reason || !a || a->memory.fault)return 0;
    while(signed_word(produced)<signed_word(count)){
        GameAddress unit,arrival,controlled,target;uint32_t variant,id,wide=0,trial=0;float radius=step,p[3]={0};int clear=0;
        if(census && signed_word(get(a,root+census))>=(int32_t)limit){*reason=produced?0:8;return !a->memory.fault;}
        if(!game_level_player_catalog_value(a,root,kind,3,0,&variant) || variant>UINT16_MAX)return 0;
        unit=game_level_factory_take(a,kind,(uint16_t)variant);if(!unit){fprintf(stderr,"native reserve acquisition failed: kind=%02x variant=%u\n",kind,variant);return 0;}
        target=get(a,root+0x3c4);
        if(kind==0x27 && target && get(a,target+20)==get(a,root+0x3c8)){
            /*47ffb5 supplies the selected target's own origin to slot38. */
            float source[3];unsigned axis;
            for(axis=0;axis<3;axis++)source[axis]=spawn_real(a,target+24+axis*4);
            if(!presentation_target_position(&a->memory,target,source,p))goto failed;
            p[1]=(float)((long double)reserve_floor(a,p[0],p[2])+spawn_real(a,0x59d8a8));
            if(game_memory_read_u8(&a->memory,target+12)==0x26){
                float candidate[2];(void)gameplay_geometry_nearby_location(&a->memory,p[0],p[2],4000,candidate);
                p[0]=candidate[0];p[2]=candidate[1];if(a->memory.fault)goto failed;
            }
        }else{
            do{
                float distance;uint16_t heading;int16_t spread=(int16_t)heading_range;int half=spread/2-(spread<0 && (spread&1));
                uint32_t offset;uint8_t cell_x,cell_z;int blocked=0;
                if(trial++>=10){trial=0;wide++;radius=(float)((long double)radius+step);}
                controlled=get(a,root+0x448);if(!controlled)goto failed;
                heading=(uint16_t)(game_memory_read_u16(&a->memory,controlled+0x24)+
                    game_memory_read_u16(&a->memory,controlled+(game_memory_read_u8(&a->memory,controlled+12)==0x20?0x26c:0x14c)));
                distance=(float)((long double)signed_word(effect_initialization_random(&a->memory,(float)signed_word(truncate_word(radius))))+base_radius);
                offset=effect_initialization_random(&a->memory,(float)spread);
                heading=(uint16_t)(heading+offset-half);
                p[2]=(float)((long double)spawn_real(a,0x6298a8+((heading+512)&2047u)*4)*distance+spawn_real(a,get(a,get(a,0x58973c)+0x448)+32));
                p[0]=(float)((long double)spawn_real(a,0x6298a8+(heading&2047u)*4)*distance+spawn_real(a,get(a,get(a,0x58973c)+0x448)+24));
                p[0]=(float)((long double)signed_word(truncate_word(p[0])&0xfffffe00u)+spawn_real(a,0x573e6c));
                p[2]=(float)((long double)signed_word(truncate_word(p[2])&0xfffffe00u)+spawn_real(a,0x573e6c));
                if(kind==0x27)p[1]=(float)(gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+spawn_real(a,0x59d8a8));
                else{
                    cell_x=(uint8_t)(truncate_word(p[0])>>9);cell_z=(uint8_t)(truncate_word(p[2])>>9);
                    blocked=reserve_mask(a,cell_x,cell_z) ||
                        (reserve_mask(a,(int)cell_x-1,cell_z) && reserve_mask(a,(int)cell_x+1,cell_z) &&
                         reserve_mask(a,cell_x,(int)cell_z-1) && reserve_mask(a,cell_x,(int)cell_z+1));
                    if(!blocked && get(a,root+0x3bc)==controlled){
                        float destination[3]={spawn_real(a,controlled+24),spawn_real(a,controlled+28),spawn_real(a,controlled+32)};
                        blocked=!scene_order_segment_clear(&a->memory,p,destination);
                    }
                    if(!blocked)p[1]=(float)(gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+spawn_real(a,unit+0x30));
                }
                clear=0;if(!blocked && !gameplay_geometry_position_clear(&a->memory,unit,p,&clear))goto failed;
                if(a->memory.fault)goto failed;
            }while(!clear && wide<10);
            if(wide==10){int returned;*reason=produced?0:6;returned=reserve_return(a,unit);
                if(!returned)fprintf(stderr,"native reserve return failed: unit=%08x kind=%02x source=%08x\n",unit,kind,get(a,unit+0x50));return returned;}
        }
        arrival=game_level_factory_take(a,0x45,0);if(!arrival)goto failed;
        if(!reserve_sequence(a,root,&id) || !game_level_arrival_produce(a,arrival,id,p,unit) || !reserve_publish(a,root,arrival)){
            fprintf(stderr,"native reserve arrival failed: unit=%08x arrival=%08x kind=%02x fault=%d\n",unit,arrival,kind,a->memory.fault);
            (void)game_level_factory_destroy(a,arrival);return 0;
        }
        if(census)put(a,root+census,get(a,root+census)+1);
        produced++;continue;
failed:fprintf(stderr,"native reserve placement failed: unit=%08x kind=%02x fault=%d\n",unit,kind,a->memory.fault);(void)reserve_return(a,unit);return 0;
    }
    if(get(a,get(a,root)+0x60)!=0x47f420)return 0;
    {uint32_t field=kind==0x21?0x4d0:kind==0x27?0x4d4:kind==0x40?0x4cc:kind==0x26?0x4d8:kind==11?0x4e4:kind==0x22?0x4dc:kind==0x24?0x4e0:0;
        if(field)put(a,root+field,get(a,root+field)-1);}
    *reason=0;return !a->memory.fault;
}
static int production_feedback(GameApplication *a,GameAddress root,uint8_t kind,uint32_t reason,uint32_t cost){
    static const GameAddress messages[6]={0x5899d0,0x5899b0,0x589a3c,0x589a20,0x589a00,0x5899e8};
    GameAddress text=0;char bytes[96];
    if(!reason || root!=get(a,0x5f0208))return !a->memory.fault;
    if(!game_menu_sound(a,123))return 0;
    if(reason>=1 && reason<=6)text=messages[reason-1];
    else if(reason==7){
        snprintf(bytes,sizeof(bytes),"Unit costs $%d to produce",signed_word(cost));
        return game_message_enqueue_bytes(a,get(a,0x628a88),bytes,1);
    }else if(reason==8)text=kind==0x21?0x589974:kind==0x27?0x589984:kind==0x40?0x589960:0;
    return !text?!a->memory.fault:game_message_enqueue(a,get(a,0x628a88),text,1);
}
/*47f5b0: actual station selection, affordability, deployment and reserve
 *fallback. Method IDs select normal native algorithms with domain arguments. */
int game_level_root_produce(GameApplication *a,GameAddress root,uint8_t kind,uint32_t x,uint32_t z){
    GameAddress group,member,selected=0;uint32_t cost,allowed,reason=2,last_busy=0,availability=0;
    int32_t price;unsigned visits=0;float best=0x1.fffffep127f;
    if(!game_level_descriptor_unit_refund(a,root,kind,&price))return 0;cost=(uint32_t)price;
    if(kind==0x22 || kind==0x28 || kind==0x25 || kind==0x24){
        int have_station=0,in_range=0;best=14000;
        for(group=get(a,root+0x294);group;group=get(a,group+0x64)){
            int count=(int16_t)game_memory_read_u16(&a->memory,group+0x24);unsigned i;
            if(++visits>65536 || count<0 || count>32767)return 0;
            member=get(a,get(a,group+0x18));if(!member)continue;have_station=1;
            for(i=0;i<(unsigned)count;i++){
                GameAddress p=get(a,group+0x1c)+i*12,controlled=get(a,root+0x448);
                float distance=gameplay_geometry_fast_distance(&a->memory,spawn_real(a,p),spawn_real(a,p+8),spawn_real(a,controlled+24),spawn_real(a,controlled+32));
                if(distance<=best || isnan(distance)){
                    in_range=1;if(!get(a,member+0x4c))continue;
                    if(gameplay_catalog_unit_affordable(&a->memory,root,cost,&allowed)!=1)return 0;
                    if(!allowed){reason=7;goto fallback;}
                    selected=member;best=(float)distance;
                }
            }
        }
        if(selected){
            if(!game_level_group_produce(a,selected,kind,x,z,&reason))return 0;
            if(!reason && !production_pay(a,root,cost))return 0;
        }else reason=!have_station?5:in_range?3:4;
    }else{
        switch(kind){
        case 11:availability=get(a,root+0x214);break;
        case 0x21:availability=get(a,root+0x208)+get(a,root+0x21c);break;
        case 0x26:availability=get(a,root+0x210)+get(a,root+0x224);break;
        case 0x27:availability=get(a,root+0x20c)+get(a,root+0x220);break;
        case 0x40:availability=get(a,root+0x204)+get(a,root+0x218);break;
        }
        if(!availability)goto fallback;
        if(gameplay_catalog_unit_affordable(&a->memory,root,cost,&allowed)!=1)return 0;
        if(!allowed){reason=7;goto fallback;}
        for(group=get(a,root+0x294);group;group=get(a,group+0x64)){
            int count=(int16_t)game_memory_read_u16(&a->memory,group+0x24);unsigned i;
            if(++visits>65536 || count<0)return 0;
            for(i=1;i<(unsigned)count;i++){
                uint8_t type;member=get(a,get(a,group+0x18)+i*4);if(!member)continue;
                if(!production_kind(a,member,&type)){fprintf(stderr,"native production kind failed: member=%08x kind=%02x\n",member,kind);return 0;}if(type!=kind)continue;
                if(!production_busy(a,member,&last_busy)){fprintf(stderr,"native production readiness failed: member=%08x kind=%02x\n",member,kind);return 0;}if(last_busy)continue;
                if(kind==0x26){
                    GameAddress payload=get(a,member+0x48);
                    long double dx=(long double)spawn_real(a,payload+24)-x,dz=(long double)spawn_real(a,payload+32)-z;
                    long double distance=dx*dx+dz*dz;
                    if(distance<best || isnan(distance)){selected=member;best=(float)distance;}
                }else{
                    if(!game_level_station_produce(a,member,x,z,&reason)){fprintf(stderr,"native station production failed: member=%08x kind=%02x\n",member,kind);return 0;}
                    if(!reason){if(!production_pay(a,root,cost))return 0;goto fallback;}
                }
            }
        }
        if(kind==0x26 && selected){
            if(!game_level_station_produce(a,selected,x,z,&reason))return 0;
            if(!reason && !production_pay(a,root,cost))return 0;
        }else if(last_busy)reason=1;
    }
fallback:
    if((reason==7 || reason==2 || reason==1) && (kind==0x21 || kind==0x27 || kind==0x40)){
        uint32_t field=kind==0x21?0x4b8:kind==0x27?0x4bc:0x4b4;
        if(get(a,root+field)){
            if(!game_level_root_reserve_produce(a,root,kind,kind==0x40?x:1,kind==0x40?0x80:0xc7,6000,kind==0x40?500:1000,&reason))return 0;
            if(!reason)put(a,root+field,get(a,root+field)-1);
        }
    }else if((reason==7 || reason==2 || reason==1) && kind==0x26 && get(a,root+0x4c0)){
        uint32_t variant,id;GameAddress unit;float p[3],candidate[2];
        if(!gameplay_geometry_circle_location(&a->memory,(float)x,(float)z,15000,candidate) ||
           !game_level_player_catalog_value(a,root,kind,3,0,&variant) || variant>UINT16_MAX)return 0;
        unit=game_level_factory_take(a,kind,(uint16_t)variant);if(!unit)return 0;
        p[0]=candidate[0];p[1]=0;p[2]=candidate[1];
        if(level_player_slots_owns_player(root)){
            uint32_t next;if(!level_player_slots_next_owner_sequence(&a->memory,root,&id,&next))return 0;put(a,root+24,next);
        }else id=game_state_next_owner_sequence(&a->memory,root);
        if(!game_level_imperial_unit_activate(a,unit,id,p))return 0;
        {unsigned head=state_helper_flagged_member_offset(get(a,unit+16));
            if(level_player_slots_owns_player(root) && !level_player_slots_attach_scene_member(root,unit,head))return 0;
            game_list_insert_after(&a->memory,unit,root+head,0x48,0x4c);}
        if(get(a,get(a,root)+0x60)!=0x47f420)return 0;
        put(a,root+0x4d8,get(a,root+0x4d8)-1);
        reason=0;put(a,root+0x4c0,get(a,root+0x4c0)-1);
    }
    return !a->memory.fault && production_feedback(a,root,kind,reason,cost);
}
/*47d38b: recover a unit through the existing mode1 departure/transfer owner.
 *Health fractions return production time; currency uses the unscaled refund
 *query and the real catalog credit method. No duplicate unit state is created. */
static int root_unit_reclaim(GameApplication *a,GameAddress root,uint32_t identity){
    GameAddress player=get(a,root+0x3bc),unit=0,arrival,method;uint32_t flags,maximum,id,amount;
    int32_t price;uint8_t kind;float fraction,refund_fraction;long double ratio;
    if(!player)return 0;if(!get(a,player+0x58))return !a->memory.fault;
    if(gameplay_catalog_lookup(&a->memory,root,get(a,get(a,root)+4),identity,NULL,NULL,&unit)!=1)return 0;
    if(!unit)return !a->memory.fault;
    flags=get(a,unit+16);if(flags&0x20000u)return !a->memory.fault;
    kind=game_memory_read_u8(&a->memory,unit+12);
    if(!(flags&0x80u)){
        if(kind==0x29)return game_menu_sound(a,123);
        if(!game_level_scene_property(a,unit,get(a,get(a,unit)+0x44),&maximum))return 0;
        ratio=(long double)signed_word(get(a,unit+0x58))/signed_word(maximum);
        if(kind==0x40)ratio*=spawn_real(a,0x573e4c);
        fraction=(float)ratio;
        refund_fraction=kind==0x26?(float)((long double)signed_word(get(a,unit+0x158))/signed_word(get(a,unit+0x160))):fraction;
        if(!game_level_catalog_return_health(a,root,kind,fraction) || !game_level_descriptor_unit_refund(a,root,kind,&price))return 0;
        amount=truncate_word((long double)price*refund_fraction);method=get(a,get(a,root)+0x38);
        if(method==0x47f250){if(!gameplay_catalog_credit(&a->memory,root,amount))return 0;}
        else if(method==0x47f290 || method==0x4794b0 || method==0x4794e0){
            put(a,root+0x22c,get(a,root+0x22c)+amount);put(a,root+0x150,get(a,root+0x150)+amount);
        }else if(method!=0x477760)return 0; /*Actual empty base credit. */
    }
    arrival=game_level_factory_take(a,0x45,0);if(!arrival)return 0;
    if(!reserve_sequence(a,root,&id) || !game_level_descriptor_transfer(a,arrival,id,unit) || !reserve_publish(a,root,arrival)){
        (void)game_level_factory_destroy(a,arrival);return 0;
    }
    return !a->memory.fault;
}
/*47c9cb: remove the real network player, issue local termination or the remote
 *notice/death camera, then destroy the final clock and publish single-player
 *state. Root/controller, message, camera and clock owners remain authoritative. */
static int root_player_retire(GameApplication *a,GameAddress root){
    GameAddress player;uint32_t count;char text[160],name[33];size_t i;int length;
    length=snprintf(text,sizeof(text),"Player %u handling quit tick at time %ld\n",
        (unsigned)game_memory_read_u16(&a->memory,root+8),(long)signed_word(get(a,0x5ac6ac)));
    if(length<0 || (size_t)length>=sizeof(text) || a->memory.fault)return 0;
    game_application_log(a,"game",text);
    if(!get(a,root+0x468))return !a->memory.fault;
    if(get(a,0x5accd4)){
        if(!a->network)return 0;
        /*43e630 ignores the transport HRESULT after requesting removal. */
        (void)game_network_player_destroy(a,get(a,root+0x3c0));
    }
    put(a,root+0x468,0);
    if(root==get(a,0x5f0208))return !a->memory.fault && game_level_request_end(a);
    for(i=0;i<32;i++){name[i]=(char)game_memory_read_u8(&a->memory,root+0x39c+(unsigned)i);if(!name[i])break;}
    if(i==32 || a->memory.fault)return 0;
    if(get(a,root+0x584))length=snprintf(text,sizeof(text),"Network Error: Player \"%s\" dropped",name);
    else if(signed_word(get(a,root+0x14c))>0)length=snprintf(text,sizeof(text),"Player \"%s\" left the game",name);
    else if(get(a,root+0x148))length=snprintf(text,sizeof(text),"Player \"%s\" accomplished objective",name);
    else length=snprintf(text,sizeof(text),"Player \"%s\" was defeated",name);
    if(length<0 || (size_t)length>=sizeof(text) || !game_application_timed_message_bytes(a,(const uint8_t *)text,(size_t)length,5))return 0;
    player=get(a,root+0x3bc);
    if(get(a,player+0x58) && get(a,player+0x498)==UINT32_MAX && !game_level_vehicle_end_camera(a,player))return 0;
    count=get(a,0x5ac6b4)-1;put(a,0x5ac6b4,count);
    if(count==1){
        if(get(a,0x5accd4) && !game_level_clock_destroy(a))return 0;
        put(a,0x5accd4,0);put(a,0x5eff04,0);put(a,0x5f0150,1);put(a,0x5eff28,get(a,0x5f0208));
    }
    return !a->memory.fault;
}
static int action_player(void *c,GameAddress player,unsigned operation){return game_level_player_station(c,player,operation);}
static int action_history(void *c,GameAddress object,unsigned action,GameAddress target,const float point[3]){
    return action==11?game_level_scene_target_order(c,object,2,target):game_level_scene_history_order(c,object,action,target,point);
}
static int action_repair(void *c,GameAddress root,uint8_t group,uint8_t slot){return game_level_root_descriptor_repair(c,root,group,slot);}
static int action_produce(void *c,GameAddress root,uint8_t kind,uint32_t x,uint32_t z){return game_level_root_produce(c,root,kind,x,z);}
static int action_sound(void *c,uint32_t sound){return game_menu_sound(c,(int16_t)sound);}
static int action_message(void *c,GameAddress text){GameApplication *a=c;return game_message_enqueue(a,get(a,0x628a88),text,1);}
int game_level_root_controller_action(GameApplication *a,GameAddress root,const uint16_t coordinates[4]){
    GameplayControllerActions s={a,action_player,action_history,action_repair,action_produce,maximum,action_sound,action_message};
    return gameplay_root_controller_action(&a->memory,root,coordinates,&s);
}
int game_level_root_event_apply(GameApplication *a,GameAddress root,GameAddress event){
    GameAddress player,group=0,node,target;uint32_t kind,identity,mask;unsigned i,visits=0;
    float position[3],best;uint16_t heading;long double distance,x,z;
    if(!a || !root || !event || a->memory.fault)return 0;
    kind=game_memory_read_u8(&a->memory,event+16);player=get(a,root+0x3bc);
    switch(kind){
    case 0x19:return root_player_retire(a,root);
    case 0x2d:return root_unit_reclaim(a,root,get(a,event+24));
    case 0x30:{
        GameplayControllerActions services={a,action_player,action_history,action_repair,action_produce,maximum,action_sound,action_message};
        return gameplay_root_history_toggle(&a->memory,root,&services);
    }
    case 0x31:case 0x32:return root_selector_event(a,root,event);
    case 0x20:{
        uint16_t coordinates[4];if(!player)return 0;if(!get(a,player+0x58))return !a->memory.fault;
        for(i=0;i<4;i++)coordinates[i]=game_memory_read_u16(&a->memory,event+24+i*2);
        return game_level_root_controller_action(a,root,coordinates);
    }
    case 0x2a:return game_level_root_descriptor_repair(a,root,game_memory_read_u8(&a->memory,event+24),game_memory_read_u8(&a->memory,event+25));
    case 0x1b:case 0x1c:case 0x1d:case 0x1e:case 0x1f:case 0x2b:case 0x2c:{
        static const uint8_t units[5]={0x40,0x21,0x27,0x26,11};uint8_t unit;
        uint32_t first=0,second=0;
        if(!player)return 0;if(!get(a,player+0x58))return !a->memory.fault;
        unit=kind<=0x1f?units[kind-0x1b]:kind==0x2b?0x22:0x24;
        if(kind==0x1b)first=game_memory_read_u8(&a->memory,event+24);
        else if(kind!=0x1c && kind!=0x1d){first=truncate_word(spawn_real(a,event+24));second=truncate_word(spawn_real(a,event+28));}
        return game_level_root_produce(a,root,unit,first,second);
    }
    case 0x1a:{
        uint16_t status;unsigned index=game_memory_read_u8(&a->memory,event+25);
        uint8_t unit=game_memory_read_u8(&a->memory,event+24);
        if(!player)return 0;
        if(!get(a,player+0x58))return !a->memory.fault;
        if(index>=(unsigned)game_memory_read_u16(&a->memory,0x639a74))return 0;
        group=get(a,get(a,0x639a70)+index*4);if(!group)return 0;
        if(get(a,group+0x54)!=get(a,0x58973c))
            return get(a,0x58973c)!=get(a,0x5f0208)?!a->memory.fault:
                game_message_enqueue(a,get(a,0x628a88),0x589894,1);
        if(!game_level_citadel_upgrade(a,group,unit,&status))return 0;
        return get(a,0x58973c)!=get(a,0x5f0208)?!a->memory.fault:game_input_upgrade_feedback(a,status,unit);
    }
    case 0x21:return station_select(a,root,0);
    case 0x22:return game_level_catalog_refresh_selection(a,root);
    case 0x23:return station_select(a,root,1);
    case 0x24:{
        GameAddress camera;int32_t index=(int16_t)game_memory_read_u16(&a->memory,event+24);
        if(!player)return 0;
        if(!get(a,root+0x5a0) || !get(a,player+0x58))return !a->memory.fault;
        target=player;
        if(index!=-1){
            int32_t count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
            if(index<0 || index>=count)return 0;
            group=get(a,get(a,0x639a70)+(uint32_t)index*4);
            if(group && get(a,group+0x54)==root){
                node=get(a,get(a,group+0x18));if(node)target=get(a,node+0x48);
            }
        }
        put(a,root+0x450,target);
        if(root==get(a,0x5f0208)){
            camera=get(a,0x58f8a8);if(camera && !game_camera_bind_scene(&a->memory,camera,target))return 0;
        }
        put(a,root+0x454,get(a,0x5ac6ac));return !a->memory.fault;
    }
    case 0x29:{
        unsigned index=game_memory_read_u8(&a->memory,event+24),slot=game_memory_read_u8(&a->memory,event+25);
        int32_t count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
        if(!player)return 0;if(!get(a,player+0x58))return !a->memory.fault;
        if(count<=0 || index>=(unsigned)count)return 0;
        group=get(a,get(a,0x639a70)+index*4);
        if(!group || get(a,group+0x54)!=root)return !a->memory.fault;
        return game_level_group_remove_member(a,group,slot);
    }
    case 0x25:case 0x26:case 0x27:case 0x28:
        if(!player)return 0;
        if(get(a,player+0x58) || (int32_t)get(a,root+0x588)<=0)return !a->memory.fault;
        put(a,root+0x588,get(a,root+0x588)-1u);
        heading=game_memory_read_u16(&a->memory,event+24);
        if(kind==0x25){for(i=0;i<3;i++)position[i]=spawn_real(a,player+0x2fc+i*4);}
        else{
            if(kind==0x27 || kind==0x28){
                best=0x1.fffffep127f;
                for(node=get(a,root+0x294);node;node=get(a,node+0x64)){
                    if(++visits>65536 || a->memory.fault)return 0;
                    if(kind==0x28){if(!group || (int32_t)get(a,group+0x58)<(int32_t)get(a,node+0x58))group=node;}
                    else{
                        x=(long double)spawn_real(a,node+0x48)-spawn_real(a,player+24);
                        z=(long double)spawn_real(a,node+0x50)-spawn_real(a,player+32);distance=x*x+z*z;
                        if(isnan(distance) || distance<best){best=(float)distance;group=node;}
                    }
                }
            }
            if(group){
                position[0]=(float)(((long double)spawn_real(a,group+4)+spawn_real(a,group+12))*spawn_real(a,0x573e48));
                position[2]=(float)(((long double)spawn_real(a,group+8)+spawn_real(a,group+16))*spawn_real(a,0x573e48));
                /*4f6bb0 overwrites incoming Y before any use or publication. */
                position[1]=0;
            }else{
                uint32_t start=game_memory_read_u16(&a->memory,root+10);
                for(i=0;i<3;i++)position[i]=spawn_real(a,0x603028+start*12+i*4);
                heading=game_memory_read_u16(&a->memory,0x603058+start*2);
            }
        }
        return !a->memory.fault && game_level_player_respawn(a,player,position,heading);
    case 0x2e:case 0x2f:
        if(!player)return 0;
        if(kind==0x2f && !get(a,player+0x58))return !a->memory.fault;
        target=0;identity=kind==0x2f?get(a,event+24):0;
        if(kind==0x2f){
            GameAddress catalog;uint32_t slot=identity&63u;if(slot>=6)return 0;
            catalog=get(a,0x5f0210+slot*4);if(!catalog)return 0;
            if(gameplay_catalog_lookup(&a->memory,catalog,get(a,get(a,catalog)+4),identity,NULL,NULL,&target)!=1)return 0;
        }
        if(kind==0x2f && (!target || !(get(a,target+16)&0x1c0u))){target=0;identity=0;}
        put(a,root+0x3c4,target);put(a,root+0x3c8,identity);
        if(get(a,root+0x448)==player){put(a,player+0xf0,target);put(a,player+0xf4,identity);}
        return !a->memory.fault;
    case 0x33:case 0x34:
        identity=game_memory_read_u16(&a->memory,root+8);if(identity>=6)return 0;
        mask=game_memory_read_u8(&a->memory,event+24);
        for(i=0;i<4;i++)if(mask&(1u<<i))put(a,0x5f0160+(identity*6+i)*4,kind==0x33);
        if(kind==0x34)put(a,0x5f0160+identity*28,1);
        return !a->memory.fault;
    default:
        return !a->memory.fault; /*Original out-of-range event is a no-op. */
    }
}
static int retire(void *c,GameAddress root,GameAddress event){
    GameApplication *a=c;EffectResource *resource=effect_resource_find(event);
    EffectResourcePool *pool=resource?resource->owner:NULL;
    uint8_t kind=game_memory_read_u8(&a->memory,event+16);
    int applied=game_level_root_event_apply(a,root,event);
    /* The queue already detached this event. This consumer must return it even
     * when applying it fails, without clearing the application's original fault. */
    if(!applied){
        GameMemory cleanup=a->memory;cleanup.fault=0;
        if(!pool || effect_resource_pool_return(&cleanup,pool,event)!=1)
            fprintf(stderr,"native root failed: event retirement %08x\n",event);
        fprintf(stderr,"native root failed: event kind=%02x\n",kind);return 0;
    }
    if(!pool || effect_resource_pool_return(&a->memory,pool,event)!=1){
        fprintf(stderr,"native root failed: event retirement %08x\n",event);return 0;}
    return !a->memory.fault;
}
static int update(void *c,GameAddress object,GameAddress method){
    int ok=game_level_scene_update(c,object,method);
    if(!ok)fprintf(stderr,"native root scene failed: object=%08x method=%08x\n",object,method);
    return ok;
}
static int next(void *c,GameAddress object,GameAddress method,GameAddress *out){
    GameApplication *a=c;unsigned visits=0;if(method!=0x42dfb0){fprintf(stderr,"native root failed: next method=%08x object=%08x\n",method,object);return 0;}
    object=get(a,object+0x48);
    while(object && !get(a,object+0x58)){
        if(a->memory.fault || ++visits>65536)return 0;
        object=get(a,object+0x48);
    }
    *out=object;return !a->memory.fault;
}
static int finish(void *c,GameAddress group){int ok=game_level_citadel_finish(c,group);
    if(!ok)fprintf(stderr,"native root failed: citadel finish group=%08x\n",group);return ok;}
int game_level_root_update(GameApplication *a,GameAddress root){
    GameplayRootServices services={a,idle,camera,sound,notice,network,value,credit,spawn,retire,update,next,finish};
    int ok=gameplay_root_update(&a->memory,root,&services);
    if(!ok)fprintf(stderr,"native root update failed: root=%08x tick=%u active=%08x fault=%d at=%08x size=%zu\n",
        root,get(a,0x5ac6ac),get(a,root+0x448),a->memory.fault,a->memory.fault_address,a->memory.fault_size);
    return ok;
}
