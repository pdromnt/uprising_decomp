#include "level_player_scene.h"
#include "level_player_resources.h"
#include "level_effect_runtime.h"
#include "level_factory.h"
#include "player_effects.h"
#include "gameplay_geometry.h"
#include "menu_sound.h"
#include <string.h>
extern int game_level_vehicle_death_detach(GameApplication *,GameAddress,GameAddress,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int begin(void *c,GameAddress o,GameAddress other,GameAddress table,GameAddress header){GameApplication *a=c;float matrix[12],velocity[3]={0,40,0};unsigned i;
    SoundPlaybackRequest request={16,0,1,0,0,0};GameAddress reference=get(a,0x58f890);
    long double dx=(long double)real(a,o+24)-real(a,reference+12),dz=(long double)real(a,o+32)-real(a,reference+20);
    request.distance_squared=(float)(dz*dz+dx*dx);
    if(a->memory.fault || !game_menu_sound_play(a,&request) || !game_level_vehicle_death_detach(a,o,other,get(a,table+0x14)))return 0;
    for(i=0;i<12;i++)matrix[i]=real(a,o+0x80+i*4);
    return !a->memory.fault && game_level_mesh_fragments(a,o,get(a,table+0x30),header,matrix,velocity,(int32_t)get(a,header+20)/5,-1);
}
static int variant(void *c,GameAddress catalog,uint8_t kind,uint32_t *out){return game_level_player_catalog_value(c,catalog,kind,3,0,out);}
static int height(void *c,float x,float z,long double *detail,long double *terrain){GameApplication *a=c;
    *detail=gameplay_geometry_detail_height(&a->memory,x,z);*terrain=gameplay_geometry_terrain_height(&a->memory,x,z);return !a->memory.fault;
}
static GameAddress acquire(void *c,uint16_t profile){return game_level_factory_take(c,10,profile);}
static int emit(void *c,GameAddress catalog,GameAddress child,AttachmentEmission *request){return game_level_attachment_emit(c,catalog,child,request);}
static int anchors(void *c,GameAddress o,GameAddress method,int32_t count,GameAddress records,const float velocity[3]){
    return method==0x5356b0 && game_level_attachment_anchors(c,o,count,records,velocity);
}
static int discard(void *c,GameAddress child){return game_level_factory_destroy(c,child);}
int game_level_player_death_effect(GameApplication *a,GameAddress o,GameAddress other){
    PlayerDeathEffectServices services={a,begin,variant,height,acquire,emit,anchors,discard};
    return a && player_death_effect(&a->memory,o,other,&services);
}
