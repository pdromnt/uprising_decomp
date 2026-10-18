#include "level_run.h"
#include "level_factory.h"
#include "level_objects.h"
#include "level_player_resources.h"
#include "level_player_slots.h"
#include "level_player_scene.h"
#include "vehicle_death.h"
#include "gameplay_geometry.h"
#include "gameplay_leaves.h"
#include "game_intrusive_list.h"
#include "menu_sound.h"
#include "level_effect_runtime.h"
#include "spatial_sample.h"
#include "entity_queries.h"
#include "portable_crt.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
/* Distinct remaining programs have real arguments and mandatory native links. */
extern int game_level_vehicle_death_detach(GameApplication *,GameAddress,GameAddress,GameAddress);
extern int game_level_effect_burst(GameApplication *,GameAddress,GameAddress,uint16_t,int32_t,int32_t);
extern int game_level_mesh_fragments(GameApplication *,GameAddress,GameAddress,GameAddress,const float[12],const float[3],int32_t,int32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float real(GameApplication *a,GameAddress p){uint32_t v=get(a,p);float f;memcpy(&f,&v,4);return f;}
typedef struct Emitter {GameApplication *app;GameAddress object;} Emitter;
static int place(void *c,const AttachmentEmission *request,const uint16_t motion[3]){
    Emitter *e=c;(void)motion;
    return game_level_object_place_angles(e->app,e->object,request->identity,request->position,request->angles) &&
        attachment_effect_finish_placement(&e->app->memory,e->object);
}
static int height(void *c,float x,float z,long double *out){Emitter *e=c;*out=gameplay_geometry_terrain_height(&e->app->memory,x,z);return !e->app->memory.fault;}
static int sound(void *c,const float p[3],int16_t id,float pitch,uint32_t flags,int force,uint32_t *voice){
    Emitter *e=c;GameApplication *a=e->app;GameAddress reference=get(a,0x58f890);SoundPlaybackRequest request={0};
    long double dx=(long double)p[0]-real(a,reference+12),dz=(long double)p[2]-real(a,reference+20);
    request.sound=id;request.distance_squared=(float)(dz*dz+dx*dx);request.volume=1;request.pitch=pitch;request.flags=flags;request.force=force;
    if(a->memory.fault || !game_menu_sound_play(a,&request))return 0;
    if(voice)*voice=get(a,get(a,0x628a90));
    return !a->memory.fault;
}
static int announcement(void *c,int16_t id){Emitter *e=c;SoundPlaybackRequest request={0};request.sound=id;request.volume=1;return game_menu_sound_play(e->app,&request);}
static int initialize_attachment(GameApplication *a,GameAddress catalog,GameAddress child,AttachmentEmission *request){
    Emitter context={a,child};AttachmentEmissionServices services={&context,place,height,sound,announcement};uint32_t n=get(a,catalog+0x18);
    game_memory_write_u32(&a->memory,catalog+0x18,n+1);request->identity=(n<<6)|game_memory_read_u16(&a->memory,catalog+8);
    return !a->memory.fault && attachment_effect_emit(&a->memory,child,request,&services);
}
static int publish_attachment(GameApplication *a,GameAddress catalog,GameAddress child){
    uint32_t flags=get(a,child+16),slot=(flags&1)?((flags&64)?0x80:0x1c):0xe4;
    if(a->memory.fault)return 0;
    if(!level_player_slots_attach_scene_member(catalog,child,slot))return 0;
    game_list_insert_after(&a->memory,child,catalog+slot,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(child);return 0;}return 1;
}
int game_level_attachment_emit(GameApplication *a,GameAddress catalog,GameAddress child,AttachmentEmission *request){
    return initialize_attachment(a,catalog,child,request) && publish_attachment(a,catalog,child);
}
static int begin(void *c,GameAddress o,GameAddress other,GameAddress table){GameApplication *a=c;
    return game_level_vehicle_death_detach(a,o,other,get(a,table+0x14)) && game_level_effect_burst(a,o,get(a,table+0x18),6,5,-1);}
static int death_sound(void *c,const float p[3],int16_t id){Emitter e={c,0};return sound(&e,p,id,0,0,0,NULL);}
static int death_stop_voice(void *c,uint32_t voice){return game_menu_sound_stop(c,0x81,voice);}
static int mesh(void *c,GameAddress o,GameAddress method,GameAddress header,const float matrix[12],const float velocity[3]){return game_level_mesh_fragments(c,o,method,header,matrix,velocity,3,-1);}
static int variant(void *c,GameAddress o,GameAddress catalog,uint32_t *out){GameApplication *a=c;
    if(get(a,get(a,o)+0x60)!=0x53c550)return 0;
    return game_level_player_catalog_value(a,catalog,game_memory_read_u8(&a->memory,o+12),3,(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0,out);}
static GameAddress acquire(void *c,uint16_t profile){return game_level_factory_take(c,10,profile);}
static int emit(void *c,GameAddress catalog,GameAddress child,AttachmentEmission *request){return game_level_attachment_emit(c,catalog,child,request);}
static int discard(void *c,GameAddress child){return game_level_factory_destroy(c,child);}
static int anchors_emit(void *c,GameAddress catalog,GameAddress child,AttachmentEmission *request){return emit(c,catalog,child,request);}
int game_level_attachment_anchors(GameApplication *a,GameAddress o,int32_t count,GameAddress records,const float velocity[3]){
    AttachmentAnchorServices services={a,acquire,anchors_emit,discard};
    return attachment_effect_anchors(&a->memory,o,count,records,velocity,&services);
}
static int legacy_anchors(GameApplication *a,GameAddress o,int32_t count,GameAddress records,const float velocity[3]){
    AttachmentAnchorServices services={a,acquire,anchors_emit,discard};
    GameAddress method=get(a,get(a,o)+0x1c);
    if(method==0x4a6800)return attachment_effect_legacy_anchors(&a->memory,o,count,records,velocity,0,&services);
    return method==0x5356b0 && game_level_attachment_anchors(a,o,count,records,velocity);
}
static int32_t signed_bits(uint32_t bits){int32_t value;memcpy(&value,&bits,4);return value;}
/*40e680 keeps the two random streams distinct: throttled local acknowledgments
 *use CRT rand; positional death screams use the gameplay sample cursor. */
int game_level_class40_notice(GameApplication *a,GameAddress o,uint32_t reason){
    GameAddress local,reference;uint32_t tick,choice;SoundPlaybackRequest request={0};
    if(!a || a->memory.fault)return 0;
    if(reason>2)return 1;
    request.volume=1;
    if(reason==1){
        choice=effect_initialization_random(&a->memory,real(a,0x571efc));
        if(choice>6)return !a->memory.fault;
        request.sound=(int16_t)(56+choice);reference=get(a,0x58f890);
        {long double x=(long double)real(a,o+24)-real(a,reference+12),z=(long double)real(a,o+32)-real(a,reference+20);
            request.distance_squared=(float)(z*z+x*x);}
    }else{
        local=get(a,0x5f0208);tick=get(a,0x5ac6ac);
        if(signed_bits(tick-get(a,local+0x58c))<=288 ||
            (get(a,o+20)&63)!=game_memory_read_u16(&a->memory,local+8))return !a->memory.fault;
        game_memory_write_u32(&a->memory,local+0x58c,tick);
        request.sound=reason==2?55:(int16_t)(63+((portable_crt_rand_step(&a->memory)*12u)>>15));
        game_memory_write_u16(&a->memory,local+0x598,(uint16_t)request.sound);
    }
    return !a->memory.fault && game_menu_sound_play(a,&request);
}
/*546630: this class uses the ordinary CRT RNG, distinct from the shared
 *1024-entry gameplay sample cursor. Preserve the local announcement throttle. */
int game_level_infantry_notice(GameApplication *a,GameAddress o,uint32_t reason){
    GameAddress local;uint32_t tick,choice;SoundPlaybackRequest request={0};
    if(!a || a->memory.fault)return 0;
    if(reason>1)return 1; /*The original method returns immediately. */
    local=get(a,0x5f0208);tick=get(a,0x5ac6ac);
    if(signed_bits(tick-get(a,local+0x58c))<=288 ||
       (get(a,o+20)&63)!=game_memory_read_u16(&a->memory,local+8))return !a->memory.fault;
    game_memory_write_u32(&a->memory,local+0x58c,tick);
    choice=(portable_crt_rand_step(&a->memory)*(reason?3u:4u))>>15;
    game_memory_write_u16(&a->memory,local+0x598,(uint16_t)((reason?80:76)+choice));
    request.sound=(int16_t)game_memory_read_u16(&a->memory,local+0x598);request.volume=1;
    return !a->memory.fault && game_menu_sound_play(a,&request);
}
/*55c190 keeps gun/model ownership until reset, but publishes the original
 *mesh debris and either loose parts or one retained-anchor attachment. */
static int class29_death(GameApplication *a,GameAddress o,GameAddress other){
    GameAddress table=get(a,o),catalog=get(a,0x5f0210+(get(a,o+20)&63)*4),header;
    float p[3],matrix[12],velocity[3]={0,40,0},anchors_velocity[3];unsigned i;uint32_t slot;
    if(!begin(a,o,other,table))return 0;
    for(i=0;i<3;i++)p[i]=real(a,o+24+i*4);
    if(!death_sound(a,p,(get(a,0x5ac6ac)&1)?130:131))return 0;
    if(other && (get(a,other+16)&2))for(i=0;i<3;i++)velocity[i]=(float)((long double)real(a,other+0xb4+i*4)*real(a,0x5733f4)+(i==1?real(a,0x5733f8):0.0L));
    for(i=0;i<12;i++)matrix[i]=real(a,o+0x80+i*4);
    header=get(a,get(a,o+0x54)+0x50);
    if(!game_level_mesh_fragments(a,o,get(a,table+0x30),header,matrix,velocity,3,-1) ||
       !game_level_player_catalog_value(a,catalog,0x29,3,(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0,&slot) || slot>=4)return 0;
    anchors_velocity[0]=real(a,o+0xb4);anchors_velocity[1]=60;anchors_velocity[2]=real(a,o+0xb8);
    return get(a,table+0x1c)==0x5356b0 && game_level_attachment_anchors(a,o,(int32_t)get(a,0x5cee20+slot*4),get(a,0x5cee30+slot*4),anchors_velocity);
}
int game_level_aircraft_killed(GameApplication *a,GameAddress o,GameAddress other){
    GameAddress table=get(a,o),catalog,model,header,parts,child=0;uint32_t slot,choice;int32_t count;unsigned i,profile;
    float p[3],matrix[12],velocity[3],square;long double length;AttachmentEmission request={0};
    SoundPlaybackRequest audio={0};float anchor_velocity[3]={0,40,0};
    catalog=get(a,0x5f0210+(get(a,o+20)&63)*4);
    if(!game_level_vehicle_death_detach(a,o,other,get(a,table+0x14)) || !game_level_effect_burst(a,o,get(a,table+0x18),0,2,-1))return 0;
    for(i=0;i<3;i++)p[i]=real(a,o+24+i*4);
    {GameAddress reference=get(a,0x58f890);long double x=(long double)p[0]-real(a,reference+12),z=(long double)p[2]-real(a,reference+20);
        audio.sound=(get(a,0x5ac6ac)&1)?130:131;audio.volume=1;
        audio.distance_squared=(float)(((long double)(float)(x*x+z*z))*real(a,0x577ca4));
        if(!game_menu_sound_play(a,&audio))return 0;
    }
    if(other && (get(a,other+16)&2))for(i=0;i<3;i++)velocity[i]=(float)((long double)real(a,other+0xb4+i*4)*real(a,0x577ca8)+(i==1?real(a,0x577ca0):0.0L));
    else{velocity[0]=real(a,o+0xb4);velocity[1]=0;velocity[2]=real(a,o+0xb8);}
    model=get(a,o+0x54);header=get(a,model+0x50);for(i=0;i<12;i++)matrix[i]=real(a,o+0x80+i*4);
    if(!game_level_mesh_fragments(a,o,get(a,table+0x30),header,matrix,velocity,3,3) ||
       !game_level_player_catalog_value(a,catalog,0x26,3,(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0,&slot) || slot>=11)return 0;
    velocity[0]=real(a,o+0xb4);velocity[1]=real(a,o+0x15c);velocity[2]=real(a,o+0xb8);
    square=(float)(((long double)velocity[1]*velocity[1]+(long double)velocity[0]*velocity[0])+(long double)velocity[2]*velocity[2]);
    length=gameplay_geometry_square_root(square);
    velocity[0]=(float)(length*real(a,0x6298a8+(game_memory_read_u16(&a->memory,o+36)&2047)*4));
    velocity[1]=(float)(length*real(a,0x6298a8+(game_memory_read_u16(&a->memory,o+38)&2047)*4));
    velocity[2]=(float)(length*real(a,0x6298a8+((game_memory_read_u16(&a->memory,o+36)+512)&2047)*4));
    for(i=0;i<3;i++){uint32_t bits;memcpy(&bits,velocity+i,4);game_memory_write_u32(&a->memory,o+(i==0?0xb4:i==1?0x15c:0xb8),bits);}
    choice=effect_initialization_random(&a->memory,real(a,0x577cac));
    count=signed_bits(get(a,0x63a2c0+slot*4));parts=get(a,0x63a290+slot*4);
    velocity[1]=0;
    if(signed_bits(choice)<0 || signed_bits(choice)>2)
        return get(a,table+0x1c)==0x5356b0 && game_level_attachment_anchors(a,o,count,parts,velocity);
    for(profile=0;profile<3;profile++)if(signed_bits(get(a,0x5a0488+profile*8))>=signed_bits(get(a,header+0x14)) &&
        signed_bits(get(a,0x5a048c+profile*8))>=signed_bits(get(a,header+0x1c)))break;
    child=game_level_factory_take(a,10,(uint16_t)profile);if(!child)return 0;
    if(!attachment_effect_configure(&a->memory,child,header,get(a,o+0x3c),game_memory_read_u16(&a->memory,o+0x44)))goto failed;
    memcpy(request.position,p,12);memcpy(request.velocity,velocity,12);request.angles[0]=game_memory_read_u16(&a->memory,o+36)&2047;
    for(i=1;i<3;i++){uint16_t angle=game_memory_read_u16(&a->memory,o+36+i*2)&2047;request.angles[i]=angle>1024?(uint16_t)(angle-2048):angle;}
    request.mode=2;request.flags=(get(a,0x5ac6ac)&3)?0x9f:0x8f;request.source=get(a,o+20);
    if(get(a,get(a,child)+0xac)!=0x554d80 || !initialize_attachment(a,catalog,child,&request) ||
       !attachment_effect_retain_anchors(&a->memory,child,count,parts,16,anchor_velocity) || !publish_attachment(a,catalog,child))goto failed;
    return !a->memory.fault;
failed:{int saved=a->memory.fault;GameAddress address=a->memory.fault_address;a->memory.fault=0;
    (void)game_level_factory_destroy(a,child);a->memory.fault=saved;a->memory.fault_address=address;return 0;}
}
/*543830: actual class27 death program, including its two debris routes.
 *The scene/model/attachment owners persist; no generated call or state mirror.
 *Its gun owners survive until544410 reset or final factory destruction. */
static int infantry_death(GameApplication *a,GameAddress o,GameAddress other){
    GameAddress table=get(a,o),catalog,model,header,child=0,parts;
    uint32_t slot,choice;int32_t count;unsigned i,profile;
    float p[3],matrix[12],velocity[3],anchor_velocity[3]={0,40,0},squared;
    long double speed;AttachmentEmission request={0};
    int32_t voice=signed_bits(get(a,o+0x178));
    if(a->memory.fault || (voice>=0 && !game_menu_sound_stop(a,129,(uint32_t)voice)))return 0;
    catalog=get(a,0x5f0210+(get(a,o+20)&63)*4);
    if(!game_level_vehicle_death_detach(a,o,other,get(a,table+0x14)) ||
       !game_level_effect_burst(a,o,get(a,table+0x18),0,2,-1))return 0;
    for(i=0;i<3;i++)p[i]=real(a,o+24+i*4);
    if(!death_sound(a,p,(get(a,0x5ac6ac)&1)?130:131))return 0;
    if(other && (get(a,other+16)&2)){
        for(i=0;i<3;i++)velocity[i]=(float)((long double)real(a,other+0xb4+i*4)*real(a,0x576f94)+(i==1?real(a,0x576f98):0.0L));
    }else{velocity[0]=real(a,o+0xb4);velocity[1]=real(a,o+0x148);velocity[2]=real(a,o+0xb8);}
    model=get(a,o+0x54);header=get(a,model+0x50);
    for(i=0;i<12;i++)matrix[i]=real(a,o+0x80+i*4);
    if(a->memory.fault || !game_level_mesh_fragments(a,o,get(a,table+0x30),header,matrix,velocity,3,3) ||
       !variant(a,o,catalog,&slot) || slot>=11)return 0;
    velocity[0]=real(a,o+0xb4);velocity[1]=real(a,o+0x148);velocity[2]=real(a,o+0xb8);
    squared=(float)(((long double)velocity[1]*velocity[1]+(long double)velocity[2]*velocity[2])+(long double)velocity[0]*velocity[0]);
    speed=sqrtl((long double)squared);
    velocity[0]=(float)(speed*real(a,0x6298a8+(game_memory_read_u16(&a->memory,o+0x24)&2047)*4));
    velocity[1]=(float)(speed*real(a,0x6298a8+(game_memory_read_u16(&a->memory,o+0x26)&2047)*4));
    velocity[2]=(float)(speed*real(a,0x6298a8+((game_memory_read_u16(&a->memory,o+0x24)+512)&2047)*4));
    for(i=0;i<3;i++){uint32_t bits;memcpy(&bits,velocity+i,4);game_memory_write_u32(&a->memory,o+(i==0?0xb4:i==1?0x148:0xb8),bits);}
    choice=effect_initialization_random(&a->memory,real(a,0x576f9c));
    count=signed_bits(get(a,0x631af8+slot*4));parts=get(a,0x631ac8+slot*4);
    if(a->memory.fault)return 0;
    if(signed_bits(choice)<0 || signed_bits(choice)>2)
        return get(a,table+0x1c)==0x5356b0 && game_level_attachment_anchors(a,o,count,parts,velocity);
    for(profile=0;profile<3;profile++)
        if(signed_bits(get(a,0x5a0488+profile*8))>=signed_bits(get(a,header+0x14)) &&
           signed_bits(get(a,0x5a048c+profile*8))>=signed_bits(get(a,header+0x1c)))break;
    if(a->memory.fault || !(child=game_level_factory_take(a,10,(uint16_t)profile)))return 0;
    if(!attachment_effect_configure(&a->memory,child,header,get(a,o+0x3c),game_memory_read_u16(&a->memory,o+0x44)))goto failed;
    memcpy(request.position,p,12);memcpy(request.velocity,velocity,12);
    request.angles[0]=game_memory_read_u16(&a->memory,o+0x24)&2047;
    for(i=1;i<3;i++){uint16_t angle=game_memory_read_u16(&a->memory,o+0x24+i*2)&2047;
        request.angles[i]=angle>1024?(uint16_t)(angle-2048):angle;}
    /*The original chooses the lesser vertical speed, including its unordered
     *branch; this is a cap, not an upward minimum. */
    if(!(real(a,0x576fa0)>request.velocity[1]))request.velocity[1]=real(a,0x576fa0);
    request.mode=2;request.flags=(get(a,0x5ac6ac)&1)?0x9f:0x8f;request.source=get(a,o+20);
    if(get(a,get(a,child)+0xac)!=0x554d80 || !initialize_attachment(a,catalog,child,&request) ||
       !attachment_effect_retain_anchors(&a->memory,child,count,parts,16,anchor_velocity) ||
       !publish_attachment(a,catalog,child))goto failed;
    return get(a,table+0x58)==0x546630 && game_level_infantry_notice(a,o,1);
failed:{int saved_fault=a->memory.fault;GameAddress saved_address=a->memory.fault_address;
    a->memory.fault=0;(void)game_level_factory_destroy(a,child);
    if(saved_fault){a->memory.fault=saved_fault;a->memory.fault_address=saved_address;}return 0;}
}
/*482970 owns the whole death transaction: sound and fragments, then either
 *immediate anchor debris or one retained attachment with transferred effects. */
static int small_record_death(GameApplication *a,GameAddress o,GameAddress other){
    GameAddress table=get(a,o),owner,model,header,counts,records,child=0;
    uint32_t variant,old_count=0,saved_children[4];int active,count,transferred=0;unsigned i,profile;
    float position[3],matrix[12],mesh_velocity[3]={0,40,0},anchor_velocity[3]={0,100,0};
    AttachmentEmission request={0};uint8_t other_type;
    if(a->memory.fault || !game_level_vehicle_death_detach(a,o,other,get(a,table+0x14)))return 0;
    owner=get(a,0x5f0210+(get(a,o+20)&63u)*4u);
    for(i=0;i<3;i++)position[i]=real(a,o+24+4*i);
    if(!death_sound(a,position,(get(a,0x5ac6ac)&1)?130:131))return 0;
    model=get(a,o+0x54);header=get(a,model+0x50);
    for(i=0;i<12;i++)matrix[i]=real(a,model+4+4*i);
    if(a->memory.fault || !game_level_mesh_fragments(a,o,get(a,table+0x30),header,matrix,mesh_velocity,signed_bits(get(a,header+0x14))/5,-1))return 0;
    if(get(a,table+0x7c)!=0x482b90 || !game_level_player_catalog_value(a,owner,game_memory_read_u8(&a->memory,o+12),3,0,&variant))return 0;
    if(variant>UINT32_MAX/4u || !native_entity_type_activity(&a->memory,o,&active))return 0;
    counts=get(a,o+0x8c);records=get(a,o+0x90);
    if((uint64_t)counts+4u*variant+4u>UINT32_MAX || (uint64_t)records+4u*variant+4u>UINT32_MAX)return 0;
    count=signed_bits(get(a,counts+4u*variant));records=get(a,records+4u*variant);
    other_type=other?game_memory_read_u8(&a->memory,other+12):0;
    if(active || other_type==11 || other_type==22 || (other_type==0x41 && (get(a,other+0xbc)==4 || get(a,other+0xbc)==10)))
        return !a->memory.fault && game_level_effect_burst(a,o,get(a,table+0x18),6,4,-1) && legacy_anchors(a,o,count,records,anchor_velocity);
    for(profile=0;profile<3;profile++){
        if(signed_bits(get(a,0x5a0488+profile*8))>=signed_bits(get(a,header+0x14)) &&
           signed_bits(get(a,0x5a048c+profile*8))>=signed_bits(get(a,header+0x1c)))break;
    }
    child=game_level_factory_take(a,10,(uint16_t)profile);if(!child)return 0;
    if(!attachment_effect_configure(&a->memory,child,header,get(a,o+0x3c),game_memory_read_u16(&a->memory,o+0x44)))goto failed;
    memcpy(request.position,position,sizeof(position));
    for(i=0;i<3;i++)request.angles[i]=game_memory_read_u16(&a->memory,o+0x24+i*2);
    request.mode=75;request.flags=0x6b2;request.source=get(a,o+20);
    if(get(a,get(a,child)+0xac)!=0x554d80 || !initialize_attachment(a,owner,child,&request) ||
       !attachment_effect_retain_anchors(&a->memory,child,count,records,16,anchor_velocity))goto failed;
    old_count=get(a,o+0x88);for(i=0;i<4;i++)saved_children[i]=get(a,o+0x6c+i*4);
    if(a->memory.fault || !attachment_effect_transfer_children(&a->memory,child,o+0x6c,o+0x88))goto failed;
    transferred=1;
    if(publish_attachment(a,owner,child))return 1;
failed:{
    int fault=a->memory.fault;GameAddress fault_address=a->memory.fault_address;
    a->memory.fault=0;
    if(transferred){
        for(i=0;i<4;i++){game_memory_write_u32(&a->memory,o+0x6c+i*4,saved_children[i]);game_memory_write_u32(&a->memory,child+0xf0+i*4,0);}
        game_memory_write_u32(&a->memory,o+0x88,old_count);game_memory_write_u32(&a->memory,child+0x100,0);
    }
    (void)game_level_factory_destroy(a,child);
    if(fault){a->memory.fault=fault;a->memory.fault_address=fault_address;}
    return 0;
    }
}
static int anchors(void *c,GameAddress o,GameAddress method,int32_t count,GameAddress records,const float velocity[3]){GameApplication *a=c;
    if(method!=0x5356b0)return 0;
    return game_level_attachment_anchors(a,o,count,records,velocity);}
static int terrain(void *c,float x,float z,uint32_t extent){GameApplication *a=c;game_terrain_impact(&a->memory,x,z,(int32_t)extent);return !a->memory.fault;}
/*4bf870 records a loss tick for the matching imperial primary payload. It
 *does not remove the group or its member. Preserve its live collection reads. */
static int group_remove(void *c,GameAddress group,uint32_t id){
    GameApplication *a=c;uint32_t index=0,items=get(a,0x639a70);GameAddress member,payload;
    while(!a->memory.fault && (int32_t)index<(int16_t)game_memory_read_u16(&a->memory,0x639a74)){
        member=get(a,items+4*index);
        if(get(a,member+0x54)==get(a,0x5f0220)){
            member=get(a,get(a,member+0x18));
            if(member){payload=get(a,member+0x48);if(get(a,payload+20)==id){
                game_memory_write_u32(&a->memory,get(a,group+0x328)+4*index,get(a,0x5ac6ac));return !a->memory.fault;
            }}
        }
        index++;
    }
    return !a->memory.fault;
}
static int station_begin(void *c,GameAddress object,GameAddress other){Emitter *e=c;
    return begin(e->app,object,other,get(e->app,object));
}
static int station_sound(void *c,const float p[3],int32_t id){Emitter *e=c;return death_sound(e->app,p,(int16_t)id);}
static int station_mesh(void *c,GameAddress object,GameAddress header,const float matrix[12],const float velocity[3]){Emitter *e=c;
    return game_level_mesh_fragments(e->app,object,get(e->app,get(e->app,object)+0x30),header,matrix,velocity,3,-1);
}
static int station_variant(void *c,GameAddress owner,uint32_t *out){Emitter *e=c;
    return game_level_player_catalog_value(e->app,owner,game_memory_read_u8(&e->app->memory,e->object+12),3,0,out);
}
static int station_anchors(void *c,GameAddress object,int32_t count,GameAddress records,const float velocity[3]){Emitter *e=c;
    return get(e->app,get(e->app,object)+0x1c)==0x5356b0 && game_level_attachment_anchors(e->app,object,count,records,velocity);
}
static GameAddress station_acquire(void *c,uint16_t profile){Emitter *e=c;return acquire(e->app,profile);}
static int station_emit(void *c,GameAddress owner,GameAddress child,AttachmentEmission *request){Emitter *e=c;return emit(e->app,owner,child,request);}
static int station_discard(void *c,GameAddress child){Emitter *e=c;return discard(e->app,child);}
int game_level_station_payload_death(GameApplication *a,GameAddress object,GameAddress other){
    Emitter e={a,object};PresentationArticulatedDeathServices services={&e,station_begin,station_sound,station_mesh,station_variant,station_anchors,station_acquire,station_emit,station_discard};
    if(a->memory.fault)return 0;
    if(get(a,object)==0x572c78)return presentation_instance_articulated_death(&a->memory,object,other,&services);
    return get(a,object)==0x5753b0 && spatial_sample_death(&a->memory,object,other,&services);
}
int game_level_scene_killed(GameApplication *a,GameAddress o,GameAddress other,GameAddress method){
    VehicleDeathServices services={a,begin,death_sound,mesh,variant,anchors,acquire,emit,discard,terrain,group_remove,death_stop_voice};
    if(method==0x433eb0){
        uint32_t subtype=get(a,o+0x64);float velocity[3]={0,30,0};
        AttachmentAnchorServices emission={a,acquire,anchors_emit,discard};
        if(subtype>=15)return 0;
        if(other && game_memory_read_u8(&a->memory,other+12)==0x20)velocity[1]=15;
        else if(!game_level_effect_burst(a,o,get(a,get(a,o)+0x18),6,2,131))return 0;
        if(get(a,get(a,o)+0x1c)!=0x4a6800)return 0;
        return attachment_effect_legacy_anchors(&a->memory,o,signed_bits(get(a,0x5ace38+subtype*4)),
            get(a,0x5acdf8+subtype*4),velocity,(uint16_t)(subtype>=2 && subtype<=4?0xb010:0xf010),&emission);
    }
    if(method==0x55c190)return game_level_aircraft_killed(a,o,other);
    if(method==0x40e180)return game_level_class40_killed(a,o,other);
    if(method==0x40a480)return get(a,get(a,o)+0x20)==0x40a430 && game_level_presentation_reset(a,o,1);
    if(method==0x40a0c0){
        float position[3];unsigned i;(void)other;
        for(i=0;i<3;++i)position[i]=real(a,o+24+i*4);
        if(a->memory.fault || get(a,get(a,o)+0x20)!=0x40a060)return 0;
        return game_level_physical_impact(a,o,position) && game_level_presentation_reset(a,o,1);
    }
    if(method==0x555b60)return game_level_attachment_killed(a,o,other);
    if(method==0x42d9a0){
        uint32_t count;int32_t amount;GameAddress table=get(a,o);
        /*The original notifies with reason1; it never revives source health. */
        if(!game_level_scene_notice(a,o,get(a,table+0x58),1))return 0;
        count=effect_initialization_random(&a->memory,real(a,0x57217c))+1u;memcpy(&amount,&count,4);
        return !a->memory.fault && game_level_effect_burst(a,o,get(a,table+0x18),6,amount,-1);
    }
    if(method==0x450da0)return class29_death(a,o,other);
    if(method==0x4074d0 || method==0x4e75f0)return vehicle_presentation_death(&a->memory,o,other,&services);
    if(method==0x543830)return infantry_death(a,o,other);
    if(method==0x4f6f90)return game_level_player_death_effect(a,o,other);
    if(method==0x482970)return small_record_death(a,o,other);
    if(method==0x4384e0 || method==0x5101d0)return game_level_station_payload_death(a,o,other);
    fprintf(stderr,"Unreconstructed scene death %08x object=%08x\n",(unsigned)method,(unsigned)o);
    a->memory.fault=1;a->memory.fault_address=method;return 0;
}
