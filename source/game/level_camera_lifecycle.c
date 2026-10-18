#include "level_run.h"
#include "scene_projection_storage.h"
#include "display_transition.h"
#include <string.h>
static uint32_t camera_word(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void camera_put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float camera_real(GameApplication *a,GameAddress p){uint32_t bits=camera_word(a,p);float value;memcpy(&value,&bits,4);return value;}
static void camera_float(GameApplication *a,GameAddress p,float v){uint32_t bits;memcpy(&bits,&v,4);camera_put(a,p,bits);}
/*4a3980/4a3f20: common history plus player-relative attachment. */
static void camera_base(GameApplication *a,GameAddress p){
    camera_put(a,p,0x5743bc);camera_put(a,p+0x18,0);camera_put(a,p+0x1c,0);camera_put(a,p+0x20,0);camera_put(a,p+0x24,UINT32_MAX);
}
static void camera_player(GameApplication *a,GameAddress p,GameAddress player){
    camera_put(a,p+0x28,player);camera_put(a,p+0x2c,0);
    camera_float(a,p+0x30,(float)((long double)camera_real(a,player+0x30)*camera_real(a,0x57437c)));
    camera_float(a,p+0x34,(float)((long double)camera_real(a,player+0x34)*camera_real(a,0x574378)));
    game_memory_write_u16(&a->memory,p+8,113);
}
/*4a6170: the seven records have no separate child allocation. The application
 *holds ownership; canonical globals and active/previous pointers are borrowers.
 *A failed free retains its owner ID for practical retry. */
int game_level_cameras_end(GameApplication *a){
    unsigned i;int ok=1,saved_fault,saved_write;GameAddress saved_address;size_t saved_size;
    if(!a)return 0;
    saved_fault=a->memory.fault;saved_address=a->memory.fault_address;
    saved_size=a->memory.fault_size;saved_write=a->memory.fault_write;a->memory.fault=0;
    for(i=0;i<7;i++){
        GameAddress p=a->camera_records[i];
        if(!p)continue;
        if(!scene_projection_storage_free(p)){ok=0;continue;}
        a->camera_records[i]=0;
        if(camera_word(a,0x58f898+i*4)==p)camera_put(a,0x58f898+i*4,0);
        if(camera_word(a,0x58f890)==p)camera_put(a,0x58f890,0);
        if(camera_word(a,0x58f894)==p)camera_put(a,0x58f894,0);
    }
    if(a->memory.fault)ok=0;
    if(saved_fault){a->memory.fault=saved_fault;a->memory.fault_address=saved_address;
        a->memory.fault_size=saved_size;a->memory.fault_write=saved_write;}
    return ok;
}
/*4a5dd0: all camera constructors, then the real activation/viewport consumer.
 *Keep verified record layouts; method addresses are stable native identities.
 *No general registers/stack interpreter or generated execution is involved. */
int game_level_cameras_begin(GameApplication *a){
    static const unsigned sizes[7]={0x38,0x68,0x68,0x2c,0x48,0x2c,0x174};
    static const GameAddress tables[7]={0x5743c4,0x5743c0,0x5743d8,0x5743d0,0x5743c8,0x5743cc,0x5743d4};
    static const uint16_t types[7]={1,0,3,2,4,5,6};
    GameAddress player;unsigned i;
    if(!a || a->memory.fault || !game_level_cameras_end(a))return 0;
    player=camera_word(a,camera_word(a,0x5f0208)+0x3bc);
    if(!player || a->memory.fault)return 0;
    for(i=0;i<7;i++){
        GameAddress p=scene_projection_storage_allocate(sizes[i],"level camera");
        if(!p)goto fail;
        a->camera_records[i]=p;camera_base(a,p);camera_put(a,p,tables[i]);
        game_memory_write_u16(&a->memory,p+4,types[i]);
        switch(i){
        case 0:camera_player(a,p,player);break;
        case 1:camera_put(a,p+0x28,player);camera_put(a,p+0x5c,0);camera_put(a,p+0x60,0);camera_float(a,p+0x64,40);break;
        case 2:camera_put(a,p+0x28,0);camera_put(a,p+0x5c,0);camera_float(a,p+0x60,70);camera_float(a,p+0x64,120);break;
        case 3:game_memory_write_u8(&a->memory,p+0x28,0);break;
        case 4:camera_player(a,p,player);game_memory_write_u16(&a->memory,p+0x38,0);
            camera_put(a,p+0x3c,0);camera_put(a,p+0x40,player);camera_put(a,p+0x44,0);break;
        case 5:camera_put(a,p+0x28,0);break;
        case 6:game_memory_write_u16(&a->memory,p+10,0);break;
        }
        camera_put(a,0x58f898+i*4,p);
        if(a->memory.fault)goto fail;
    }
    camera_put(a,0x58f894,0);camera_put(a,0x58f890,0);
    if(game_camera_activate(&a->memory,a->camera_records[1],NULL))return 1;
fail:
    (void)game_level_cameras_end(a);return 0;
}
