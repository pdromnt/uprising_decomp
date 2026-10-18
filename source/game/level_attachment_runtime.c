#include "level_effect_runtime.h"
#include "level_factory.h"
#include "level_player_slots.h"
#include "attachment_effect.h"
#include "gameplay_geometry.h"
#include "game_intrusive_list.h"
#include "menu_sound.h"
#include <string.h>
extern int game_level_scene_contacts(GameApplication *,GameAddress,GameAddress,const float[3],int *);
extern int game_level_contact_response(GameApplication *,GameAddress,GameAddress,GameAddress,const uint32_t[3],const float[3]);
typedef struct AttachmentContext {GameApplication *app;GameAddress object;} AttachmentContext;
static AttachmentUpdateServices update_services(AttachmentContext *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int reset(void *c){AttachmentContext *e=c;return game_level_attachment_reset(e->app,e->object);}
static int post(void *c,GameAddress other){AttachmentContext *e=c;AttachmentUpdateServices services=update_services(e);return attachment_effect_post(&e->app->memory,e->object,other,&services);}
static int sound(void *c,const float p[3],int16_t id){AttachmentContext *e=c;GameApplication *a=e->app;GameAddress reference=get(a,0x58f890);SoundPlaybackRequest request={0};
    long double dx=(long double)p[0]-real(a,reference+12),dz=(long double)p[2]-real(a,reference+20);
    request.sound=id;request.distance_squared=(float)(dz*dz+dx*dx);request.volume=1;
    return !a->memory.fault && game_menu_sound_play(a,&request);
}
static int wind(void *c,const float p[3],uint32_t voice){AttachmentContext *e=c;GameApplication *a=e->app;GameAddress reference,bank;long double dx,dz,ground;float pitch,volume;int playing;
    if(!game_menu_sound_playing(a,5,voice,&playing))return 0;
    if(!playing)return 1;
    ground=gameplay_geometry_terrain_height(&a->memory,p[0],p[2]);pitch=attachment_effect_pitch(&a->memory,p[1],ground);
    reference=get(a,0x58f890);dx=(long double)p[0]-real(a,reference+12);dz=(long double)p[2]-real(a,reference+20);bank=get(a,0x628a90);
    volume=sound_playback_attenuation(&a->memory,get(a,bank+20),(float)(dz*dz+dx*dx));
    return !a->memory.fault && game_menu_sound_volume(a,5,voice,volume) && game_menu_sound_frequency(a,get(a,0x628a90),5,voice,pitch);
}
static int stop(void *c,uint32_t voice){AttachmentContext *e=c;int playing;
    if(!game_menu_sound_playing(e->app,5,voice,&playing))return 0;
    return !playing || game_menu_sound_stop(e->app,5,voice);
}
static int burst(void *c,uint16_t kind,int32_t count,int32_t id){AttachmentContext *e=c;return game_level_effect_burst(e->app,e->object,0x42d6d0,kind,count,id);}
static int mesh(void *c,GameAddress descriptor,const float matrix[12],const float velocity[3]){AttachmentContext *e=c;return game_level_mesh_fragments(e->app,e->object,0x42d4c0,descriptor,matrix,velocity,5,-1);}
static int anchors(void *c,int32_t count,GameAddress records,const float velocity[3]){AttachmentContext *e=c;return game_level_attachment_anchors(e->app,e->object,count,records,velocity);}
static int surface(void *c,const float p[3]){AttachmentContext *e=c;return game_level_surface_particles(e->app,e->object,p,6,0x6297a0);}
static int animation(void *c,const float p[3],uint8_t kind,float size){AttachmentContext *e=c;GameApplication *a=e->app;GameAddress child=game_level_factory_take(a,0x41,0);EffectInitialization request={0};
    if(!child)return 0;
    request.profile=effect_initialization_profile(&a->memory,6);request.subtype=kind;request.scale[0]=request.scale[1]=size;request.mark_kind=-1;memcpy(request.position,p,12);
    if(game_level_effect_emit(a,get(a,0x58973c),child,&request))return 1;
    (void)game_level_factory_destroy(a,child);return 0;
}
static int expansion(void *c,const float p[3],float width,float height,GameAddress *out){AttachmentContext *e=c;GameApplication *a=e->app;PresentationPose *pose=presentation_instance_pose(e->object);GameAddress catalog,child;uint32_t n,id,flags,slot;
    if(!pose || !(child=game_level_factory_take(a,0x44,0)))return 0;
    *out=child;game_memory_write_u32(&a->memory,e->object+0xec,child);catalog=get(a,0x5f0210+(pose->identity&63)*4);n=get(a,catalog+24);
    game_memory_write_u32(&a->memory,catalog+24,n+1);id=(n<<6)|game_memory_read_u16(&a->memory,catalog+8);
    if(!game_level_expansion_start(a,child,id,p,width,height,-1))goto failed;
    flags=get(a,child+16);slot=(flags&1)?((flags&64)?0x80:0x1c):0xe4;
    if(!level_player_slots_attach_scene_member(catalog,child,slot))goto failed;
    game_list_insert_after(&a->memory,child,catalog+slot,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(child);return 0;}return 1;
failed:
    if(!game_level_factory_destroy(a,child))return 0;
    game_memory_write_u32(&a->memory,e->object+0xec,0);*out=0;return 0;
}
static int damage(void *c,GameAddress other,int32_t *out){AttachmentContext *e=c;return game_level_contact_damage(e->app,e->object,other,out);}
static int impact(void *c,uint8_t flags){AttachmentContext *e=c;return game_level_fragment_impact(e->app,e->object,flags);}
static int gather(void *c,const float previous[3]){AttachmentContext *e=c;int rollback;return game_level_scene_contacts(e->app,e->object,get(e->app,get(e->app,e->object)+0x78),previous,&rollback);}
static int peer(void *c,GameAddress other,const float velocity[3],GameAddress point,const float p[3]){AttachmentContext *e=c;uint32_t bits[3];(void)point;memcpy(bits,velocity,12);return game_level_contact_response(e->app,other,e->object,get(e->app,get(e->app,other)+0x2c),bits,p);}
static int advance(void *c){AttachmentContext *e=c;AttachmentUpdateServices update=update_services(e);AttachmentContactServices contact={e,damage,&update};AttachmentTerrainServices terrain={e,stop,impact,&update};AttachmentAdvanceServices services={e,gather,peer,&contact,&terrain};return attachment_effect_advance(&e->app->memory,e->object,&services);}
static AttachmentUpdateServices update_services(AttachmentContext *e){AttachmentUpdateServices services={e,reset,post,animation,expansion,surface,wind,advance,sound,burst,mesh,anchors};return services;}
int game_level_attachment_update(GameApplication *a,GameAddress o){AttachmentContext e={a,o};AttachmentUpdateServices services=update_services(&e);return attachment_effect_update(&a->memory,o,&services);}
int game_level_attachment_motion(GameApplication *a,GameAddress o){AttachmentContext e={a,o};return advance(&e);}
int game_level_attachment_killed(GameApplication *a,GameAddress o,GameAddress other){AttachmentContext e={a,o};return post(&e,other);}
int game_level_attachment_contact(GameApplication *a,GameAddress o,GameAddress other){AttachmentContext e={a,o};AttachmentUpdateServices update=update_services(&e);AttachmentContactServices services={&e,damage,&update};return attachment_effect_contact(&a->memory,o,other,&services);}
