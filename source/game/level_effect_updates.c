#include "level_effect_runtime.h"
#include "level_factory.h"
#include "level_objects.h"
#include "level_player_slots.h"
#include "gameplay_geometry.h"
#include "gameplay_spatial.h"
#include "gameplay_leaves.h"
#include "game_intrusive_list.h"
#include "effect_resource_pool.h"
#include "presentation_collision.h"
#include "projectile.h"
#include "model_assets.h"
#include "menu_sound.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
typedef struct EffectContext {GameApplication *app;GameAddress object;PresentationPose *pose;} EffectContext;
extern int game_level_scene_contacts(GameApplication *,GameAddress,GameAddress,const float[3],int *);
extern int game_level_contact_response(GameApplication *,GameAddress,GameAddress,GameAddress,const uint32_t[3],const float[3]);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float spawn_real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static void spawn_store(GameApplication *a,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);game_memory_write_u32(&a->memory,p,bits);}
extern int game_level_scene_killed(GameApplication *,GameAddress,GameAddress,GameAddress);
static int cell(float value){int32_t n=!isfinite(value) || value< -0x1p31f || value>=0x1p31f?INT32_MIN:(int32_t)value;
    return n>=0?n/4096:-1-(int)((-(int64_t)n-1)/4096);}
static int place(void *c,uint32_t identity,const float p[3]){EffectContext *e=c;uint16_t angles[3]={0};return game_level_object_place_pose(e->app,e->object,identity,p,angles,0);}
static int detach(void *c){EffectContext *e=c;game_list_detach_node(&e->app->memory,e->object,4,8);return !e->app->memory.fault;}
static int insert(void *c){EffectContext *e=c;
    gameplay_spatial_insert(&e->app->memory,0x5f0800,e->object,cell(e->pose->position[0]),cell(e->pose->position[2]),(e->pose->flags&0x400)!=0);
    return !e->app->memory.fault;
}
static int reinsert(void *c){return detach(c) && insert(c);}
static int inside(void *c,int *out){EffectContext *e=c;*out=gameplay_geometry_world_bounds_contains(&e->app->memory,e->object);return !e->app->memory.fault;}
static int reset(void *c){EffectContext *e=c;uint32_t method=get(e->app,get(e->app,e->object)+0x20);
    if(method==0x4412f0)return game_level_arrival_reset(e->app,e->object);
    return method==0x5362d0 && game_level_presentation_reset(e->app,e->object,1);}
static ParticleEffectServices particle_services(EffectContext *e){ParticleEffectServices s={e,place,inside,reinsert,reset};return s;}
static ExpansionEffectServices expansion_services(EffectContext *e){ExpansionEffectServices s={e,place,detach,insert,reset};return s;}
static int sequence(GameApplication *a,GameAddress catalog,uint32_t *out){uint32_t n=get(a,catalog+24);game_memory_write_u32(&a->memory,catalog+24,n+1);*out=(n<<6)|game_memory_read_u16(&a->memory,catalog+8);return !a->memory.fault;}
static int publish(GameApplication *a,GameAddress catalog,GameAddress child){uint32_t flags=get(a,child+16),slot=(flags&1)?((flags&64)?0x80:0x1c):0xe4;
    if(a->memory.fault || !level_player_slots_attach_scene_member(catalog,child,slot))return 0;
    game_list_insert_after(&a->memory,child,catalog+slot,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(child);return 0;}return 1;
}
/* Keep the original failure visible while letting the existing owner retire
 * a partially initialized, unpublished effect. No allocation changes owner. */
static int spawn_discard(GameApplication *a,GameAddress child){
    int fault=a->memory.fault,writing=a->memory.fault_write;
    GameAddress address=a->memory.fault_address;size_t size=a->memory.fault_size;
    a->memory.fault=0;(void)game_level_factory_destroy(a,child);
    if(fault){a->memory.fault=fault;a->memory.fault_address=address;
        a->memory.fault_size=size;a->memory.fault_write=writing;}
    return 0;
}
static int spawn_sound(GameApplication *a,PresentationPose *pose,int16_t sound){
    GameAddress reference=get(a,0x58f890);SoundPlaybackRequest request={0};
    long double dx=(long double)pose->position[0]-spawn_real(a,reference+12);
    long double dz=(long double)pose->position[2]-spawn_real(a,reference+20);
    request.sound=sound;request.distance_squared=(float)(dz*dz+dx*dx);request.volume=1;request.pitch=2.5f;
    return !a->memory.fault && game_menu_sound_play(a,&request);
}
/*536790/536940/5368b0: the spawn family's actual gravity, pose refresh,
 *bounds, spatial/contact queue and terrain-probe path.535240 is an empty
 *retail terrain response, so the outer419c50 landing test owns settling. */
static int flight_reset(GameApplication *a,GameAddress o){
    GameAddress method=get(a,get(a,o)+0x20);
    if(method==0x419c20)return game_level_spawn_reset(a,o);
    if(method==0x40a060 || method==0x40a430 || method==0x5362d0)return game_level_presentation_reset(a,o,1);
    return 0;
}
int game_level_presentation_flight(GameApplication *a,GameAddress o){
    PresentationPose *pose=presentation_instance_pose(o);GameAddress table;
    float previous[3],normal[3];GameAddress record;unsigned i,visits=0;int rollback,hit;
    int spawn,motion,drag;
    if(!a || !pose || a->memory.fault)return 0;
    table=get(a,o);spawn=game_memory_read_u8(&a->memory,o+12)==0x10;
    motion=game_memory_read_u8(&a->memory,o+12)==0x11 &&
        get(a,table+0xa8)==0x547bf0 && get(a,table+0xa4)==0x547cd0;
    drag=get(a,table+0xa8)==0x40a000;
    if((!motion && ((!drag && get(a,table+0xa8)!=0x536940) || get(a,table+0xa4)!=0x5368b0)) ||
       get(a,table+0x78)!=0x534cd0 || get(a,table+0xa0)!=0x536a50 ||
       get(a,table+8)!=(spawn?0x419ee0u:0x535260u) || get(a,table+12)!=0x5354d0)return 0;
    memcpy(previous,pose->position,sizeof(previous));
    /*536790 calls the actual force/move programs before its common contact
     *and spatial work. The player's motion seed overrides both; applying the
     *base gravity here loses547bf0/547cd0's terrain and angular behavior. */
    if(motion){Projectile *state=presentation_instance_projectile(o);
        if(!state || !projectile_motion_update(&a->memory,state,pose) ||
           !projectile_motion_post(&a->memory,state,pose))return 0;
    }else{
        if(drag){if(!presentation_physical_drag(&a->memory,o))return 0;}
        else spawn_store(a,o+0xb8,(float)((long double)spawn_real(a,o+0xb8)-spawn_real(a,0x5765b8)));
        for(i=0;i<3;i++){
            uint16_t angle=(uint16_t)((pose->angles[i]+game_memory_read_u16(&a->memory,o+(i==0?0xcc:i==1?0xd0:0xce)))&2047);
            pose->position[i]=(float)((long double)pose->position[i]+spawn_real(a,o+0xb4+i*4));
            if(i && angle>1024)angle=(uint16_t)(angle-2048);pose->angles[i]=angle;
        }
        if(!(spawn?presentation_pose_build_spawn_world(&a->memory,pose,1):presentation_pose_build_world(&a->memory,pose)) ||
           !presentation_pose_transform_vertices(&a->memory,pose,0))return 0;
    }
    if(!presentation_pose_export(&a->memory,o,pose))return 0;
    if(!gameplay_geometry_world_bounds_contains(&a->memory,o))return !a->memory.fault && flight_reset(a,o);
    game_list_detach_node(&a->memory,o,4,8);
    if(a->memory.fault || !game_level_scene_contacts(a,o,0x534cd0,previous,&rollback))return 0;
    record=get(a,o+0xb0);
    while(record && !a->memory.fault){GameAddress other=get(a,record+12);float p[3];uint32_t velocity[3],zero[3]={0};
        if(++visits>65536)return 0;
        for(i=0;i<3;i++){p[i]=spawn_real(a,record+16+i*4);velocity[i]=get(a,o+0xb4+i*4);}
        if(!game_level_contact_response(a,other,o,get(a,get(a,other)+0x2c),velocity,p) ||
           !game_level_contact_response(a,o,other,get(a,table+0x2c),zero,p))return 0;
        if(!pose->properties[0])return !a->memory.fault;
        record=get(a,record+4);
    }
    record=get(a,o+0xb0);
    if(record){if(!game_level_factory_release_pool_member(a,record))return 0;game_memory_write_u32(&a->memory,o+0xb0,0);}
    if(!presentation_terrain_contact(&a->memory,pose,normal,&hit))return 0;
    if(hit){GameAddress response=get(a,table+0x84);
        if(response==0x535fd0){
            if(!presentation_pose_interpolate(&a->memory,pose) || !presentation_pose_export(&a->memory,o,pose))return 0;
        }else if(response==0x40a0c0 || response==0x40a480){
            /*Physical templates share their real impact/reset program with
             *the death callback. Its terrain-normal argument is unused. */
            if(!game_level_scene_killed(a,o,0,response))return 0;
        }else if(motion && response==0x547fe0){
            if(!projectile_motion_settle(&a->memory,presentation_instance_projectile(o),pose))return 0;
        }else if(response!=0x535240)return 0; /*Retail empty terrain callback. */
        if(!pose->properties[0])return !a->memory.fault;
    }
    gameplay_spatial_insert(&a->memory,0x5f0800,o,cell(pose->position[0]),cell(pose->position[2]),(pose->flags&0x400)!=0);
    return !a->memory.fault;
}
int game_level_spawn_contact(GameApplication *a,GameAddress o,GameAddress other,const uint32_t velocity[3],const float p[3]){
    PresentationPose *pose=presentation_instance_pose(o);GameAddress child,catalog;EffectInitialization effect={0};
    if(!pose || a->memory.fault)return 0;
    if(game_memory_read_u8(&a->memory,other+12)==0x20){
        catalog=get(a,0x5f0210+(pose->identity&63)*4);child=game_level_factory_take(a,0x41,0);if(!child)return 0;
        effect.subtype=0x10;effect.profile=effect_initialization_profile(&a->memory,5);effect.radius=100;effect.mark_kind=-1;
        effect.scale[0]=effect.scale[1]=(float)((long double)pose->radius*spawn_real(a,0x57211c));memcpy(effect.position,pose->position,12);
        if(!game_level_effect_emit(a,catalog,child,&effect))return spawn_discard(a,child);
        return game_level_spawn_reset(a,o);
    }
    if((game_memory_read_u8(&a->memory,o+0xdc)&1) && !(get(a,other+16)&0x400))
        return game_level_scene_killed(a,o,other,0x42d9a0) && game_level_spawn_reset(a,o);
    return game_level_contact_response(a,o,other,0x42da70,velocity,p);
}
/*419c50: periodic positional audio, actual flight/settling, and timed transfer
 *into the already owned mode1 arrival animation. */
int game_level_spawn_update(GameApplication *a,GameAddress o){
    PresentationPose *pose=presentation_instance_pose(o);uint32_t tick=get(a,0x5ac6ac),mode,id;GameAddress child,catalog;unsigned i;
    if(!pose || a->memory.fault)return 0;
    if(!(((pose->identity>>4)+tick)&31) && !spawn_sound(a,pose,123))return 0;
    mode=game_memory_read_u8(&a->memory,o+0xdc);
    if(mode&1){
        if(!game_level_presentation_flight(a,o))return 0;
        /*419caf tests C0: unordered height comparisons skip settling. */
        if(spawn_real(a,o+0xe4)>=pose->position[1]){
            mode=game_memory_read_u8(&a->memory,o+0xdc)&~1u;game_memory_write_u8(&a->memory,o+0xdc,(uint8_t)mode);
            if(mode&2)game_memory_write_u32(&a->memory,o+0xe0,tick);
            if(!spawn_sound(a,pose,0))return 0;
            pose->position[1]=spawn_real(a,o+0xe4);
            for(i=0;i<3;i++){spawn_store(a,o+0xb4+i*4,0);game_memory_write_u16(&a->memory,o+(i==0?0xcc:i==1?0xd0:0xce),0);}
        }
    }
    mode=game_memory_read_u8(&a->memory,o+0xdc);
    if((mode&3)==2 && get(a,o+0xe0)+get(a,0x5acc3c)==tick){
        catalog=get(a,0x5f0210+(pose->identity&63)*4);child=game_level_factory_take(a,0x45,0);if(!child)return 0;
        if(!sequence(a,catalog,&id) || !game_level_descriptor_transfer(a,child,id,o) || !publish(a,catalog,child))
            return spawn_discard(a,child);
    }
    return presentation_pose_export(&a->memory,o,pose) && !a->memory.fault;
}
/*41a1f0: interpolate the local pose until the landing height, then refresh
 *controller vertices and invoke the real native graph submission service.
 *Mutable clones submit through their existing owned packets and vertex buffer. */
int game_level_spawn_present(GameApplication *a,GameAddress o){
    PresentationPose *pose=presentation_instance_pose(o);uint32_t word;int32_t delta;unsigned i;
    if(!pose || a->memory.fault)return 0;
    memcpy(pose->local_position,pose->position,sizeof(pose->position));
    for(i=0;i<3;i++)pose->local_angles[i]=pose->angles[2-i];
    if(get(a,0x5eff04) && pose->local_position[1]>spawn_real(a,o+0xe4)){
        word=get(a,0x5acbe8)-get(a,0x5acc80);memcpy(&delta,&word,4);
        for(i=0;i<3;i++){
            pose->local_position[i]=(float)((long double)pose->local_position[i]+(long double)delta*spawn_real(a,o+0xb4+i*4));
            pose->local_angles[2-i]=(uint16_t)((uint32_t)pose->angles[i]+word*game_memory_read_u16(&a->memory,o+(i==0?0xcc:i==1?0xd0:0xce)));
        }
        if(!(pose->local_position[1]>spawn_real(a,o+0xe4)))pose->local_position[1]=spawn_real(a,o+0xe4);
    }
    return presentation_pose_build_spawn_controller(&a->memory,pose,(game_memory_read_u8(&a->memory,o+0xdc)&1)!=0) &&
        presentation_pose_transform_vertices(&a->memory,pose,1) && presentation_pose_export(&a->memory,o,pose) &&
        game_model_graph_update(a,pose->model);
}
int game_level_particle_start(GameApplication *a,GameAddress o,uint32_t identity,const float p[3],GameAddress recipe){
    ParticleEffect *particle=presentation_instance_particle(o);PresentationPose *pose=presentation_instance_pose(o);
    EffectModel *model=pose?effect_model_find(pose->model):NULL;EffectContext e={a,o,pose};ParticleEffectServices services=particle_services(&e);
    return particle && pose && model && particle_effect_initialize(&a->memory,particle,pose,model,identity,p,recipe,&services);
}
int game_level_surface_particles_result(GameApplication *a,GameAddress o,const float p[3],int32_t chance,GameAddress recipe,int *emitted){
    GameAddress child,catalog;uint32_t id;
    if(emitted)*emitted=0;
    if(a->memory.fault)return 0;
    if(!get(a,0x6297b8) || !game_terrain_cell_presence(&a->memory,o,0))return !a->memory.fault;
    if(!particle_effect_surface_allowed(&a->memory,spawn_real(a,o+0x38),p,chance))return !a->memory.fault;
    child=game_level_factory_take(a,0x43,0);if(!child)return 0;
    catalog=get(a,0x5f0210+(get(a,o+0x14)&63)*4);
    if(sequence(a,catalog,&id) && game_level_particle_start(a,child,id,p,recipe) && publish(a,catalog,child)){if(emitted)*emitted=1;return 1;}
    {PresentationPose *child_pose=presentation_instance_pose(child);EffectContext e={a,child,child_pose};
        if(child_pose && child_pose->properties[0] && !reset(&e))return 0;
        (void)game_level_factory_destroy(a,child);return 0;}
}
int game_level_surface_particles(GameApplication *a,GameAddress o,const float p[3],int32_t chance,GameAddress recipe){
    return game_level_surface_particles_result(a,o,p,chance,recipe,NULL);
}
int game_level_particle_update(GameApplication *a,GameAddress o){
    ParticleEffect *particle=presentation_instance_particle(o);PresentationPose *pose=presentation_instance_pose(o);
    EffectModel *model=pose?effect_model_find(pose->model):NULL;EffectContext e={a,o,pose};ParticleEffectServices services=particle_services(&e);
    return particle && pose && model && particle_effect_update(&a->memory,particle,pose,model,get(a,0x5ac6ac),&services);
}
int game_level_expansion_start(GameApplication *a,GameAddress o,uint32_t identity,const float p[3],float width,float height,int32_t duration){
    ExpansionEffect *effect=presentation_instance_expansion(o);PresentationPose *pose=presentation_instance_pose(o);
    EffectModel *model=pose?effect_model_find(pose->model):NULL;EffectContext e={a,o,pose};ExpansionEffectServices services=expansion_services(&e);
    return effect && pose && model && expansion_effect_initialize(&a->memory,effect,pose,model,identity,p,width,height,duration,&services);
}
int game_level_expansion_move(GameApplication *a,GameAddress o,const float p[3]){
    PresentationPose *pose=presentation_instance_pose(o);EffectContext e={a,o,pose};
    if(!pose || !presentation_instance_expansion(o) || !detach(&e))return 0;
    memcpy(pose->position,p,12);
    return presentation_pose_build_world(&a->memory,pose) && insert(&e);
}
int game_level_expansion_update(GameApplication *a,GameAddress o){
    ExpansionEffect *effect=presentation_instance_expansion(o);PresentationPose *pose=presentation_instance_pose(o);
    EffectModel *model=pose?effect_model_find(pose->model):NULL;EffectContext e={a,o,pose};ExpansionEffectServices services=expansion_services(&e);
    return effect && pose && model && expansion_effect_update(&a->memory,effect,pose,model,&services);
}
static int animation_duration(void *c,int32_t *out){EffectContext *e=c;EffectModel *model=effect_model_find(e->pose->model);uint32_t bits;
    if(!model)return 0;
    bits=get(e->app,model->profile+8);memcpy(out,&bits,4);return !e->app->memory.fault;
}
static int animation_attachment(void *c,GameAddress target,uint32_t *id,float matrix[12]){EffectContext *e=c;EffectAnimation *animation=presentation_instance_animation(e->object);PresentationPose *pose=presentation_instance_pose(target);unsigned i;
    *id=pose?pose->identity:get(e->app,target+20);
    if(*id==animation->attachment_identity){if(!detach(e))return 0;
        if(pose)memcpy(matrix,pose->world_matrix,48);else for(i=0;i<12;i++){uint32_t bits=get(e->app,target+0x80+4*i);memcpy(matrix+i,&bits,4);}}
    return !e->app->memory.fault;
}
static int animation_place(void *c,int *out){EffectContext *e=c;if(get(e->app,get(e->app,e->object)+8)!=0x551490)return 0;
    presentation_pose_build_billboard(e->pose);if(!inside(e,out))return 0;return !*out || insert(e);}
static int animation_contacts(void *c){EffectContext *e=c;GameApplication *a=e->app;GameAddress record,other;int rollback;unsigned i,visits=0;float point[3];const uint32_t zero[3]={0};EffectResourcePool *pool;
    if(!game_level_scene_contacts(a,e->object,get(a,get(a,e->object)+0x78),e->pose->position,&rollback)){
        fprintf(stderr,"native animation contacts failed: object=%08x method=%08x\n",e->object,get(a,get(a,e->object)+0x78));return 0;}
    for(record=get(a,e->object+0xb0);record;record=get(a,record+4)){
        if(++visits>65536){a->memory.fault=1;return 0;}
        other=get(a,record+12);for(i=0;i<3;i++){uint32_t bits=get(a,record+16+4*i);memcpy(point+i,&bits,4);}
        if(!game_level_contact_response(a,other,e->object,get(a,get(a,other)+0x2c),zero,point)){
            fprintf(stderr,"native animation response failed: object=%08x other=%08x kind=%02x method=%08x\n",
                e->object,other,game_memory_read_u8(&a->memory,other+12),get(a,get(a,other)+0x2c));return 0;}
    }
    record=get(a,e->object+0xb0);pool=effect_resource_pool_find(0x631910);
    if(record && (!pool || !effect_resource_pool_return_chain(&a->memory,pool,record)))return 0;
    game_memory_write_u32(&a->memory,e->object+0xb0,0);return !a->memory.fault;
}
static int animation_mark(void *c,int32_t cell_radius,uint32_t strength_bits){EffectContext *e=c;float strength;
    memcpy(&strength,&strength_bits,4);return game_terrain_deform(&e->app->memory,e->pose->position,strength,cell_radius);}
static int animation_visible(void *c,int *out){EffectContext *e=c;*out=game_terrain_cell_presence(&e->app->memory,e->object,0)!=0;return !e->app->memory.fault;}
static int animation_emit(void *c){EffectContext *e=c;GameApplication *a=e->app;EffectAnimation *animation=presentation_instance_animation(e->object);GameAddress child,catalog,recipe;uint32_t id;
    if(!particle_effect_emission_allowed(&a->memory,e->pose->radius,e->pose->position))return !a->memory.fault;
    child=game_level_factory_take(a,0x43,0);if(!child)return 0;
    catalog=get(a,0x5f0210+(e->pose->identity&63)*4);
    recipe=(int32_t)animation->emitter>=2 && (int32_t)animation->emitter<=4?0x629738:animation->emitter==5?0x6297a0:0;
    if(recipe && (!sequence(a,catalog,&id) || !game_level_particle_start(a,child,id,e->pose->position,recipe)))goto failed;
    if(publish(a,catalog,child))return 1;
failed:{PresentationPose *pose=presentation_instance_pose(child);EffectContext cleanup={a,child,pose};
        if(pose && pose->properties[0] && !reset(&cleanup))return 0;
        (void)game_level_factory_destroy(a,child);return 0;}
}
static uint32_t tick(void *c){EffectContext *e=c;return get(e->app,0x5ac6ac);}
int game_level_animation_update(GameApplication *a,GameAddress o){
    EffectAnimation *animation=presentation_instance_animation(o);PresentationPose *pose=presentation_instance_pose(o);EffectContext e={a,o,pose};
    EffectAnimationServices services={&e,animation_duration,animation_attachment,animation_place,animation_contacts,animation_mark,animation_visible,animation_emit,reset,tick};
    int ok=animation && pose && effect_animation_update(animation,pose,get(a,0x5ac6ac),&services);
    if(!ok && animation && pose)fprintf(stderr,"native animation failed: object=%08x elapsed=%u armed=%u emitter=%u emitted=%d mark=%d/%d/%d position=%g/%g/%g\n",
        o,get(a,0x5ac6ac)-animation->birth_tick,animation->contact_armed,animation->emitter,animation->emitted,
        animation->mark_kind,animation->mark_delay,animation->last_mark,pose->position[0],pose->position[1],pose->position[2]);
    return ok;
}
