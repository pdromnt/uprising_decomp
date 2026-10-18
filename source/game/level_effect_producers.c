#include "level_effect_runtime.h"
#include "level_factory.h"
#include "level_objects.h"
#include "level_player_slots.h"
#include "level_effects.h"
#include "sprite_fragment.h"
#include "game_intrusive_list.h"
#include "gameplay_geometry.h"
#include "gameplay_leaves.h"
#include "terrain_asset.h"
#include "menu_sound.h"
#include "projectile.h"
#include "level_player_resources.h"
#include "level_run.h"
#include "runtime_node_pool.h"
#include <string.h>
#include <math.h>
int game_level_fragment_impact(GameApplication *,GameAddress,uint8_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int effect_sound(void *,GameAddress,int16_t,const float[3]);
/*432d70: the actual projectile terrain consumer transfers its borrowed mesh
 *descriptor into a newly owned attachment effect, then retires the projectile.
 *The attachment's existing factory/pose/model and player member owners persist. */
int game_level_projectile_attachment_impact(GameApplication *a,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);AttachmentEmission request={0};
    GameAddress header,child,catalog;unsigned profile,i;
    if(!pose || a->memory.fault)return 0;
    if(!effect_sound(a,object,0,pose->position))return 0;
    header=get(a,get(a,object+0x54)+0x50);
    for(profile=0;profile<3;profile++){
        uint32_t v=get(a,0x5a0488+profile*8),h=get(a,header+0x14),w=get(a,0x5a048c+profile*8),f=get(a,header+0x1c);
        int32_t vertices,needed_vertices,faces,needed_faces;
        memcpy(&vertices,&v,4);memcpy(&needed_vertices,&h,4);memcpy(&faces,&w,4);memcpy(&needed_faces,&f,4);
        if(vertices>=needed_vertices && faces>=needed_faces)break;
    }
    if(a->memory.fault)return 0;
    child=game_level_factory_take(a,10,(uint16_t)profile);if(!child)return 0;
    if(!attachment_effect_configure(&a->memory,child,header,get(a,object+0x3c),game_memory_read_u16(&a->memory,object+0x44)))goto failed;
    memcpy(request.position,pose->position,sizeof(request.position));
    for(i=0;i<3;i++)request.angles[i]=game_memory_read_u16(&a->memory,object+0x24+i*2);
    request.mode=75;request.duration=50;request.source=get(a,object+0x14);catalog=get(a,0x58973c);
    if(get(a,get(a,child)+0xac)!=0x554d80 || !game_level_attachment_emit(a,catalog,child,&request))goto failed;
    return get(a,get(a,object)+0x20)==0x5362d0 && game_level_presentation_reset(a,object,1);
failed:(void)game_level_factory_destroy(a,child);return 0;
}
typedef struct EffectPlacement {GameApplication *app;GameAddress object;} EffectPlacement;
static int height(void *c,float x,float z,long double *out){GameApplication *a=c;*out=gameplay_geometry_terrain_height(&a->memory,x,z);return !a->memory.fault;}
static int effect_height(void *c,float x,float z,long double *out){EffectPlacement *p=c;return height(p->app,x,z,out);}
static int effect_place(void *c,uint32_t identity,const float position[3],uint16_t angle){EffectPlacement *p=c;return game_level_object_place(p->app,p->object,identity,position,angle);}
static int sequence(void *c,GameAddress catalog,uint32_t *out){GameApplication *a=c;uint32_t n,next;
    if(level_player_slots_owns_player(catalog)){
        if(!level_player_slots_next_owner_sequence(&a->memory,catalog,out,&next))return 0;
    }else{n=get(a,catalog+0x18);next=n+1;*out=(n<<6)|game_memory_read_u16(&a->memory,catalog+8);}
    game_memory_write_u32(&a->memory,catalog+0x18,next);return !a->memory.fault;
}
static int publish(void *c,GameAddress catalog,GameAddress child){GameApplication *a=c;uint32_t flags=get(a,child+16),slot=(flags&1)?((flags&64)?0x80:0x1c):0xe4;
    if(a->memory.fault || !level_player_slots_attach_scene_member(catalog,child,slot))return 0;
    game_list_insert_after(&a->memory,child,catalog+slot,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(child);return 0;}return 1;
}
static int32_t aircraft_signed(uint32_t bits){int32_t n;memcpy(&n,&bits,4);return n;}
extern int game_level_aircraft_fire(GameApplication *,GameAddress);
/*55bc70: timed payload release, then the original delivery destination/arrival
 *transaction. Command, payload and animation keep their existing owners. */
int game_level_aircraft_delivery(GameApplication *a,GameAddress o){
    GameAddress catalog=get(a,0x58973c),arrival=0;RuntimeNode *node;uint32_t tick=get(a,0x5ac6ac),remaining,choice,id;
    if(!game_level_aircraft_fire(a,o))return 0;
    if(get(a,o+0x154)==0x0fffffff)game_memory_write_u32(&a->memory,o+0x154,tick);
    if(get(a,o+0x154)==get(a,0x5ac6ac) && get(a,o+0x158)){
        if(get(a,get(a,o)+0x1c0)!=0x55ae60 || !game_level_aircraft_release(a,o,0x55ae60))return 0;
        remaining=get(a,o+0x158)-1u;game_memory_write_u32(&a->memory,o+0x158,remaining);
        if(remaining){
            if(!game_level_player_catalog_choice(a,get(a,0x58973c),0x26,&choice))return 0;
            game_memory_write_u32(&a->memory,o+0x154,get(a,0x5ac6ac)+get(a,0x5a0818+(get(a,o+0x160)+choice*10u-remaining)*4u));
        }
    }
    if(get(a,o+0xe0)!=5)return !a->memory.fault;
    node=runtime_node_pool_find(get(a,o+0xdc));if(!node || !node->live || node->in_pool || a->memory.fault)return 0;
    if(aircraft_signed(node->parameter)>0){
        uint32_t speed,cursor;uint16_t yaw;float radius,p[3];long double random;int64_t n;
        if(!game_level_player_catalog_value(a,get(a,0x58973c),0x26,5,(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0,&speed))return 0;
        radius=(float)((long double)aircraft_signed(speed)*aircraft_signed(node->parameter));
        /*412380, spread1024: signed heading plus RNG offset minus half spread. */
        cursor=get(a,0x6308c0);if(cursor==1024)cursor=0;if(cursor>=1024)return 0;
        game_memory_write_u32(&a->memory,0x6308c0,cursor+1);
        random=(long double)1024*real(a,0x6308c8+cursor*4);
        n=!isfinite(random) || random< -0x1p63L || random>=0x1p63L?INT64_MIN:(int64_t)random;
        yaw=(uint16_t)(game_memory_read_u16(&a->memory,o+36)+(uint16_t)n-512u);
        p[2]=(float)((long double)real(a,0x6298a8+((yaw+512u)&2047u)*4)*radius+real(a,o+32));
        p[0]=(float)((long double)real(a,0x6298a8+(yaw&2047u)*4)*radius+real(a,o+24));
        p[1]=(float)((long double)aircraft_signed(get(a,0x597f7c))+real(a,0x577c94));
        if(get(a,get(a,o)+0x170)!=0x55b1d0 || !game_level_aircraft_destination(a,o,p))return 0;
        node->parameter=0;return !a->memory.fault;
    }
    if(aircraft_signed(node->parameter)<= -1)return !a->memory.fault;
    game_memory_write_u8(&a->memory,o+0xd8,game_memory_read_u8(&a->memory,o+0xd8)|8);
    arrival=game_level_factory_take(a,0x45,0);if(!arrival)return 0;
    /*476440 merely allocates the sequence.440d80 mode1 transfers this aircraft
     *through the actual descriptor-transition constructor, rather than mode0. */
    catalog=get(a,0x58973c);
    if(!sequence(a,catalog,&id) || !game_level_descriptor_transfer(a,arrival,id,o) || !publish(a,catalog,arrival)){
        (void)game_level_factory_destroy(a,arrival);return 0;
    }
    node->parameter=UINT32_MAX;return !a->memory.fault;
}
static int aircraft_random(GameApplication *a,GameAddress scale,int32_t *out){
    uint32_t index=get(a,0x6308c0);long double sample;int64_t n;
    if(index==1024)index=0;if(index>=1024 || a->memory.fault)return 0;
    sample=(long double)real(a,0x6308c8+index*4)*real(a,scale);
    game_memory_write_u32(&a->memory,0x6308c0,index+1);
    n=!isfinite(sample) || sample< -0x1p63L || sample>=0x1p63L?INT64_MIN:(int64_t)sample;
    *out=aircraft_signed((uint32_t)(uint64_t)n);return !a->memory.fault;
}
/*55ae60/55af90: paired aircraft release programs, not a guest ABI thunk.
 *Catalog selection, RNG, initialization and publication use real arguments. */
int game_level_aircraft_release(GameApplication *a,GameAddress o,GameAddress method){
    GameAddress catalog,child=0;uint32_t alternate,slot,id,tick;float p[3],velocity[3];
    uint16_t angles[3]={512,0,0},spin[3]={0,85,11};unsigned i;int32_t n,sign;
    if(!a || a->memory.fault || (method!=0x55ae60 && method!=0x55af90))return 0;
    tick=get(a,0x5ac6ac);
    if(method==0x55af90){
        uint32_t remaining=get(a,o+0x184),due=get(a,o+0x188)+16;
        if(!remaining || aircraft_signed(due)>aircraft_signed(tick))return !a->memory.fault;
        game_memory_write_u32(&a->memory,o+0x184,remaining-1);
        game_memory_write_u32(&a->memory,o+0x188,tick);
        catalog=get(a,0x5f0210+(get(a,o+20)&63u)*4);
    }else catalog=get(a,0x58973c);
    if(!game_level_scene_property(a,o,get(a,get(a,o)+0x60),&alternate) ||
       !game_level_player_catalog_value(a,catalog,0x26,14,alternate,&slot) || slot>UINT16_MAX)return 0;
    child=game_level_factory_take(a,method==0x55ae60?4:0x0e,(uint16_t)slot);if(!child)return 0;
    for(i=0;i<3;i++)p[i]=real(a,o+0x18+i*4);
    if(method==0x55af90){float spread,x;
        if(!aircraft_random(a,0x577c6c,&n))goto fail;
        spread=(float)((long double)n+real(a,0x577c70));
        if(!aircraft_random(a,0x577c74,&sign))goto fail;
        x=sign?-spread:spread;velocity[0]=(float)((long double)real(a,o+0xb4)*real(a,0x577c78)+x);
        velocity[1]=-20;
        if(!aircraft_random(a,0x577c74,&sign))goto fail;
        x=sign?-spread:spread;velocity[2]=(float)((long double)real(a,o+0xb8)*real(a,0x577c78)+x);
    }else{uint32_t choice;uint16_t heading=game_memory_read_u16(&a->memory,o+0x24);
        float x,z,s,c;
        if(!game_level_player_catalog_choice(a,catalog,0x26,&choice) || choice>(UINT32_MAX-0x5a095fu)/8u)goto fail;
        x=real(a,0x5a0958+choice*8);z=real(a,0x5a095c+choice*8);
        if(get(a,o+0x158)&1)x=-x;
        s=real(a,0x6298a8+(heading&2047u)*4);c=real(a,0x6298a8+((heading+512u)&2047u)*4);
        p[0]=(float)((long double)p[0]+(float)((long double)x*c-(long double)z*s));
        p[2]=(float)((long double)p[2]+(float)((long double)x*s+(long double)z*c));
        velocity[0]=real(a,o+0xb4);velocity[1]=0;velocity[2]=real(a,o+0xb8);
    }
    if(a->memory.fault || !sequence(a,catalog,&id) ||
       !game_level_physical_launch(a,child,id,p,angles,velocity,spin,o) || !publish(a,catalog,child))goto fail;
    if(method==0x55ae60)game_memory_write_u8(&a->memory,o+0xd8,game_memory_read_u8(&a->memory,o+0xd8)|8);
    return !a->memory.fault;
fail:{int fault=a->memory.fault;GameAddress at=a->memory.fault_address;
    a->memory.fault=0;(void)game_level_factory_destroy(a,child);
    if(fault){a->memory.fault=fault;a->memory.fault_address=at;}return 0;}
}
static int initialize_animation(GameApplication *a,GameAddress catalog,GameAddress child,EffectInitialization *request){
    EffectAnimation *animation=presentation_instance_animation(child);PresentationPose *pose=presentation_instance_pose(child);
    EffectModel *model=pose?effect_model_find(pose->model):NULL;EffectPlacement p={a,child};EffectInitializationServices services={&p,effect_height,effect_place};
    return animation && pose && model && sequence(a,catalog,&request->identity) &&
        effect_initialize(&a->memory,animation,pose,model,request,&services);
}
int game_level_effect_emit(GameApplication *a,GameAddress catalog,GameAddress child,EffectInitialization *request){
    return initialize_animation(a,catalog,child,request) && publish(a,catalog,child);
}
/*434820: contact and fuse expiry share one actual detonation. The small
 *ground record borrows its catalog recipe; its animation and debris acquire
 *normal factory owners and membership before the source returns to its pool. */
int game_level_position_shot_detonate(GameApplication *a,GameAddress object){
    EffectInitialization request={0};GameAddress catalog,recipe,child;uint16_t selector;
    const float zero[3]={0,0,0};float placed[3];unsigned i;
    if(!a || a->memory.fault)return 0;
    catalog=get(a,0x5f0210+(get(a,object+20)&63u)*4);recipe=get(a,object+0x64);
    child=game_level_factory_take(a,0x41,0);if(!child)return 0;
    selector=game_memory_read_u16(&a->memory,recipe+0x2c);
    if(selector>6)goto failed; /*Retail leaves the profile undefined. */
    request.profile=effect_initialization_profile(&a->memory,selector);
    request.subtype=game_memory_read_u8(&a->memory,object+0x6c);
    request.scale[0]=request.scale[1]=real(a,recipe+0x3c);
    request.contact_armed=get(a,recipe+0x30);request.loop_control=get(a,recipe+0x34);
    request.placement_flags=1;request.mark_kind=-1;
    for(i=0;i<3;i++)request.position[i]=real(a,object+24+i*4);
    if(a->memory.fault || !game_level_effect_emit(a,catalog,child,&request))goto failed;
    /*The published animation remains owned even if a later service fails. */
    if(!game_level_fragments_spawn(a,object,10,zero))return 0;
    for(i=0;i<3;i++)placed[i]=real(a,child+24+i*4);
    if(!effect_sound(a,child,(int16_t)game_memory_read_u16(&a->memory,recipe+0x38),placed))return 0;
    return game_level_scene_reset(a,object,get(a,get(a,object)+0x20));
failed:{int fault=a->memory.fault;GameAddress address=a->memory.fault_address;
    a->memory.fault=0;(void)game_level_factory_destroy(a,child);
    if(fault){a->memory.fault=fault;a->memory.fault_address=address;}return 0;}
}
/*40a0e0: class4 impact uses the26 catalog's two banks, selected by the
 *emitter's real property. Initialize, mark terrain, publish, then play sound. */
int game_level_physical_impact(GameApplication *a,GameAddress object,const float point[3]){
    GameAddress catalog,emitter,record,child;uint32_t alternate,word;uint16_t selector_word;
    int16_t selector;int32_t radius;EffectInitialization request={0};unsigned bank;
    child=game_level_factory_take(a,0x41,0);if(!child)return 0;
    catalog=get(a,0x5f0210+(get(a,object+20)&63u)*4);emitter=get(a,object+0xd4);
    if(!catalog || !emitter || !game_level_scene_property(a,emitter,get(a,get(a,emitter)+0x60),&alternate) ||
       !game_level_player_catalog_record(a,catalog,0x26,&record))goto fail;
    bank=alternate?84u:0u;selector_word=(uint16_t)get(a,record+bank+0x3c);memcpy(&selector,&selector_word,2);
    if(selector<0 || selector>6)goto fail; /*Original scratch-profile value is undefined. */
    request.profile=effect_initialization_profile(&a->memory,(unsigned)selector);
    request.subtype=game_memory_read_u8(&a->memory,emitter+12);
    request.contact_armed=get(a,record+bank+0x44);request.loop_control=get(a,record+bank+0x48);
    word=get(a,record+bank+0x4c);memcpy(&radius,&word,4);request.radius=(float)radius;
    request.placement_flags=1;request.mark_delay=4;request.mark_radius=0x41000000;request.mark_kind=1;
    memcpy(request.position,point,12);
    if(a->memory.fault || !initialize_animation(a,catalog,child,&request))goto fail;
    game_terrain_impact(&a->memory,point[0],point[2],1024);
    if(a->memory.fault || !publish(a,catalog,child))goto fail;
    {float placed[3];unsigned i;for(i=0;i<3;++i)placed[i]=real(a,child+24+i*4);
     return effect_sound(a,child,16,placed);}
fail:
    (void)game_level_factory_destroy(a,child);return 0;
}
/*42fa00/430460/430880: acquire and initialize the actual impact animation, mark terrain
 *before publication, then play the recipe's sound. The published effect keeps
 *its existing catalog owner even if the device rejects sound playback. */
int game_level_projectile_impact(GameApplication *a,GameAddress object,
        const float point[3],GameAddress attachment,int terrain,GameAddress method){
    Projectile *projectile=presentation_instance_projectile(object);
    PresentationPose *pose=presentation_instance_pose(object);
    EffectInitialization request;uint16_t selector;GameAddress child,catalog;
    if(!projectile || !pose || (method!=0x42fa00u && method!=0x430460u && method!=0x430880u && method!=0x5480b0u))return 0;
    if(!get(a,projectile->recipe+0x30u) && !game_terrain_cell_presence(&a->memory,object,0))return !a->memory.fault;
    child=game_level_factory_take(a,0x41u,0);if(!child)return 0;
    catalog=get(a,0x5f0210u+(pose->identity&63u)*4u);
    if(terrain && method==0x430880u){
        uint32_t x,z,index;uint8_t flags;
        if(!isfinite(pose->position[0]) || !isfinite(pose->position[2]) ||
           pose->position[0]<0 || pose->position[0]>=131072 ||
           pose->position[2]<0 || pose->position[2]>=131072)goto failed;
        x=(uint32_t)pose->position[0];z=(uint32_t)pose->position[2];index=(z>>9)*256+(x>>9);
        flags=a->memory.terrain && a->memory.terrain->cells?a->memory.terrain->cells[index].flags:
            game_memory_read_u8(&a->memory,get(a,0x597d90)+(z&0xfffffe00u)*4+((x&0xfffffe3fu)>>6)+5);
        if(a->memory.fault)goto failed;
        if(flags&0x50u){
            const float velocity[3]={0,20,0};GameAddress owner=get(a,child+0x50);
            if(!owner || !game_level_fragments_spawn(a,object,15,velocity))goto failed;
            /*Original43095b returns the unused animation to its factory's
             * ready chain. It never initializes/publishes an impact here. */
            (void)game_list_insert_after(&a->memory,child,owner,4,8);
            return !a->memory.fault && effect_sound(a,object,4,pose->position);
        }
    }
    if(!projectile_impact_request(&a->memory,projectile,point,attachment,terrain,&request,&selector))goto failed;
    request.profile=(uint32_t)(int16_t)selector<=6u?effect_initialization_profile(&a->memory,selector):0;
    if(!initialize_animation(a,catalog,child,&request))goto failed;
    if(terrain && method==0x430460u)game_terrain_impact(&a->memory,request.position[0],request.position[2],0x40);
    if(a->memory.fault || !publish(a,catalog,child))goto failed;
    if(!effect_sound(a,child,(int16_t)game_memory_read_u16(&a->memory,projectile->recipe+0x38u),request.position))return 0;
    if(method==0x5480b0u){float matrix[12],velocity[3]={0,0,0};unsigned i;GameAddress model=pose->model;
        /*5480b0 shares the impact request (both zero thresholds are verified),
         *then emits the inherited model's fragments with its actual matrix. */
        for(i=0;i<12;i++)matrix[i]=real(a,model+4+i*4);
        return !a->memory.fault && game_level_mesh_fragments(a,object,get(a,get(a,object)+0x30),get(a,model+0x50),matrix,velocity,3,3);
    }
    return !a->memory.fault;
failed:
    (void)game_level_factory_destroy(a,child);return 0;
}
static GameAddress acquire_animation(void *c){return game_level_factory_take(c,0x41,0);}
static GameAddress acquire_fragment(void *c){return game_level_factory_take(c,5,0);}
static int emit(void *c,GameAddress catalog,GameAddress child,EffectInitialization *request){return game_level_effect_emit(c,catalog,child,request);}
static int discard(void *c,GameAddress child){return game_level_factory_destroy(c,child);}
static int effect_sound(void *c,GameAddress child,int16_t id,const float position[3]){GameApplication *a=c;SoundPlaybackRequest request={0};GameAddress reference=get(a,0x58f890);long double dx,dz;(void)child;
    dx=(long double)position[0]-real(a,reference+12);dz=(long double)position[2]-real(a,reference+20);
    request.sound=id;request.distance_squared=(float)(dz*dz+dx*dx);request.volume=1;
    return !a->memory.fault && game_menu_sound_play(a,&request);
}
static int burst_sound(void *c,GameAddress child,int16_t id){GameApplication *a=c;float p[3];unsigned i;for(i=0;i<3;i++)p[i]=real(a,child+24+4*i);return effect_sound(a,child,id,p);}
int game_level_effect_burst(GameApplication *a,GameAddress o,GameAddress method,uint16_t selector,int32_t count,int32_t id){
    EffectBurstServices services={a,acquire_animation,emit,burst_sound,discard};
    if(method!=0x42d6d0)return 0;
    return effect_burst(&a->memory,o,selector,count,(int16_t)id,&services);
}
static int fragment_place(void *c,GameAddress o,const FragmentRequest *request){uint16_t angles[3]={request->angle,0,0};return game_level_object_place_angles(c,o,request->identity,request->position,angles);}
int game_level_fragment_reset(GameApplication *a,GameAddress o){
    if(get(a,get(a,o)+0x20)!=0x43d2e0)return 0;
    game_memory_write_u32(&a->memory,o+0xdc,0);game_memory_write_u32(&a->memory,o+0xd4,0);
    return game_level_presentation_reset(a,o,1);
}
static int fragment_reset(void *c,GameAddress o){return game_level_fragment_reset(c,o);}
static int impact(void *c,GameAddress o,uint8_t flags){return game_level_fragment_impact(c,o,flags);}
static int visible(void *c,GameAddress o,int *out){GameApplication *a=c;*out=game_terrain_cell_presence(&a->memory,o,0)!=0;return !a->memory.fault;}
static FragmentServices fragment_services(GameApplication *a){FragmentServices s={a,height,fragment_place,fragment_reset,impact,effect_sound};return s;}
/*40d949..40d9fd: the actual kind9 mount, recipe and pooled catalog publication.
 *The ground object's434780 placement uses its existing scene/model owner. */
int game_level_class40_reaction_effect(GameApplication *a,GameAddress o){
    GameAddress catalog=get(a,0x58973c),child=0;uint32_t slot,alternate,id;float p[3];uint16_t angles[3]={0};unsigned i;
    if(!game_level_player_catalog_mount(a,catalog,9,0x40,5,&slot) || slot>UINT16_MAX)return 0;
    child=game_level_factory_take(a,9,(uint16_t)slot);if(!child)return 0;
    for(i=0;i<3;i++)p[i]=real(a,o+24+i*4);
    p[1]=(float)(gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+real(a,child+0x30));
    if(!game_level_scene_property(a,o,get(a,get(a,o)+0x60),&alternate) ||
       !sequence(a,catalog,&id) || get(a,get(a,child)+0x78)!=0x434780 ||
       !game_level_position_shot_place(a,child,id,p,angles,get(a,catalog+(alternate?0x1c0:0x1bc)),
           game_memory_read_u8(&a->memory,o+12)) || !publish(a,catalog,child))goto failed;
    for(i=0;i<3;i++)p[i]=real(a,o+24+i*4);
    return effect_sound(a,o,3,p);
failed:{int fault=a->memory.fault;GameAddress at=a->memory.fault_address;
    a->memory.fault=0;(void)game_level_factory_destroy(a,child);
    if(fault){a->memory.fault=fault;a->memory.fault_address=at;}return 0;}
}
/*40e180: retain the real pooled effect/catalog owners. Laser fragments borrow
 *profile6; their model retains its bank independently of the dying actor. */
extern int game_level_vehicle_death_detach(GameApplication *,GameAddress,GameAddress,GameAddress);
static float death_random_speed(GameApplication *a,GameAddress scale,int32_t bias){
    uint32_t word=effect_initialization_random(&a->memory,real(a,scale))+(uint32_t)bias;
    int32_t value;memcpy(&value,&word,4);return (float)value;
}
int game_level_class40_killed(GameApplication *a,GameAddress o,GameAddress attacker){
    GameAddress catalog,child=0;unsigned type,i,j;FragmentRequest r={0};
    FragmentServices services=fragment_services(a);
    if(!game_level_vehicle_death_detach(a,o,attacker,get(a,get(a,o)+0x14)))return 0;
    catalog=get(a,0x5f0210+(get(a,o+20)&63u)*4);
    if(!attacker)return !a->memory.fault;
    type=game_memory_read_u8(&a->memory,attacker+12);
    if(!game_level_class40_notice(a,o,1))return 0;
    if(type==2){
        float x,z,root;long double extended;
        if(!game_terrain_cell_presence(&a->memory,o,0))return !a->memory.fault;
        child=game_level_factory_take(a,5,0);if(!child)return 0;
        x=real(a,attacker+0xb4);z=real(a,attacker+0xbc);
        if(x==0 && z==0){x=0;z=0;}
        else{
            extended=gameplay_geometry_square_root((float)((long double)z*z+(long double)x*x));
            root=(float)extended;
            if(!isnan(extended) && extended!=real(a,0x571e94) && root!=0){
                long double factor=(long double)real(a,0x571e94)/root;
                x=(float)(x*factor);z=(float)(z*factor);
            }
            x=(float)((long double)x*real(a,0x571ef0));z=(float)((long double)z*real(a,0x571ef0));
        }
        r.velocity[0]=x;r.velocity[1]=24;r.velocity[2]=z;
        r.angle=game_memory_read_u16(&a->memory,o+0x24);
        r.profile=get(a,get(a,get(a,o+0x54)+0x110)+0x18);
        r.flags=13;r.source_identity=get(a,o+20);
        for(j=0;j<3;j++)r.position[j]=real(a,o+24+j*4);
        if(a->memory.fault || !sequence(a,catalog,&r.identity) ||
           !fragment_begin(&a->memory,child,&r,&services) || !publish(a,catalog,child))goto failed;
        return 1;
    }
    if(type==0x20){
        EffectInitialization request={0};EffectAnimation *animation;PresentationPose *pose;EffectModel *model;
        EffectPlacement placement;EffectInitializationServices initialization;
        child=game_level_factory_take(a,0x41,0);if(!child)return 0;
        animation=presentation_instance_animation(child);pose=presentation_instance_pose(child);
        model=pose?effect_model_find(pose->model):NULL;
        if(!animation || !model || !sequence(a,catalog,&request.identity))goto failed;
        for(j=0;j<3;j++)request.position[j]=real(a,o+24+j*4);
        request.subtype=game_memory_read_u8(&a->memory,o+12);
        request.profile=effect_initialization_profile(&a->memory,4);
        request.scale[0]=request.scale[1]=280;request.mark_delay=-1;
        placement.app=a;placement.object=child;initialization.context=&placement;
        initialization.terrain_height=effect_height;initialization.place=effect_place;
        if(a->memory.fault || !effect_initialize(&a->memory,animation,pose,model,&request,&initialization) ||
           !publish(a,catalog,child))goto failed;
        child=0;
    }
    if(!game_terrain_cell_presence(&a->memory,o,0))return !a->memory.fault;
    for(i=0;i<6;i++){
        memset(&r,0,sizeof(r));r.profile=get(a,0x5ad8b0+i*4);
        r.velocity[0]=death_random_speed(a,0x571ee4,-20);
        r.velocity[1]=death_random_speed(a,0x571ee8,0);
        r.velocity[1]=(float)((long double)r.velocity[1]+death_random_speed(a,0x571eec,10));
        r.velocity[2]=death_random_speed(a,0x571ee4,-20);
        child=game_level_factory_take(a,5,0);if(!child)return 0;
        r.lifetime=1;r.source_identity=get(a,o+20);
        for(j=0;j<3;j++)r.position[j]=real(a,o+24+j*4);
        if(a->memory.fault || !sequence(a,catalog,&r.identity) ||
           !fragment_begin(&a->memory,child,&r,&services))goto failed;
        game_memory_write_u16(&a->memory,child+0xd8,8);
        if(a->memory.fault || !publish(a,catalog,child))goto failed;
        child=0;
    }
    return 1;
failed:{int fault=a->memory.fault;GameAddress at=a->memory.fault_address;
    a->memory.fault=0;if(child)(void)game_level_factory_destroy(a,child);
    if(fault){a->memory.fault=fault;a->memory.fault_address=at;}return 0;}
}
int game_level_mesh_fragments(GameApplication *a,GameAddress o,GameAddress method,GameAddress header,const float matrix[12],const float velocity[3],int32_t stride,int32_t id){
    FragmentSpawnServices services={0};services.fragment=fragment_services(a);services.visible=visible;services.acquire=acquire_fragment;services.sequence=sequence;services.publish=publish;services.destroy=discard;
    if(method!=0x42d4c0)return 0;
    return fragment_mesh_spawn(&a->memory,o,header,matrix,velocity,stride,(int16_t)id,&services);
}
int game_level_fragment_update(GameApplication *a,GameAddress o){FragmentServices services=fragment_services(a);return fragment_update(&a->memory,o,&services);}
int game_level_fragment_motion(GameApplication *a,GameAddress o){FragmentServices services=fragment_services(a);return fragment_motion(&a->memory,o,&services);}
int game_level_fragments_spawn(GameApplication *a,GameAddress o,int32_t count,const float velocity[3]){
    FragmentSpawnServices services={0};services.fragment=fragment_services(a);services.visible=visible;
    services.acquire=acquire_fragment;services.sequence=sequence;services.publish=publish;services.destroy=discard;
    return fragment_spawn(&a->memory,o,count,velocity,&services);
}
static int trail_place(void *c,GameAddress o,const LevelEffectRequest *request){return game_level_object_place_angles(c,o,request->identity,request->position,request->angles);}
static int trail_reset(void *c,GameAddress o){return game_level_presentation_reset(c,o,1);}
static int trail_sound(void *c,int32_t id,const float p[3]){return effect_sound(c,0,(int16_t)id,p);}
static LevelEffectServices trail_services(GameApplication *a){LevelEffectServices services;game_level_effect_factory_services(a,&services.factory);services.place=trail_place;services.reset=trail_reset;services.sound=trail_sound;return services;}
int game_level_fragment_impact(GameApplication *a,GameAddress o,uint8_t flags){LevelEffectServices services=trail_services(a);return level_effect_fragment_event(&a->memory,o,flags,&services)>0;}
int game_level_particle_emit(GameApplication *a,GameAddress o,const float position[3],const float velocity[3],float scale,GameAddress recipe,uint16_t angle,uint32_t second,uint32_t third){
    LevelEffectRequest request={0};LevelEffectServices services=trail_services(a);request.scale=scale;request.recipe=recipe;request.angles[0]=angle;request.angles[1]=(uint16_t)second;request.angles[2]=(uint16_t)third;
    memcpy(request.position,position,12);memcpy(request.velocity,velocity,12);
    return level_effect_emit(&a->memory,o,&request,&services)>=0;
}
int game_level_trail_update(GameApplication *a,GameAddress o){LevelEffectServices services=trail_services(a);return level_effect_update(&a->memory,o,&services)>0;}
