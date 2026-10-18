#include "level_run.h"
#include "vehicle_presentation.h"
#include "menu_sound.h"
/*The scene overlap family remains mandatory native behavior. Both refresh
 *programs below already compare against their exact preserved consumers. */
extern int game_level_scene_overlap(GameApplication *,GameAddress,GameAddress method,
    const float position[3],float contact[3],int *hit); /*5350d0/derived variants*/
static int playing(void *c,int16_t sound,uint32_t voice,int *value){return game_menu_sound_playing(c,sound,voice,value);}
static int stop(void *c,int16_t sound,uint32_t voice){return game_menu_sound_stop(c,sound,voice);}
static int play(void *c,int16_t sound,float volume,float pitch,uint32_t flags){
    SoundPlaybackRequest request={sound,0,volume,pitch,flags,0};return game_menu_sound_play(c,&request);
}
static int volume(void *c,int16_t sound,uint32_t voice,float value){return game_menu_sound_volume(c,sound,voice,value);}
static int refresh(void *c,GameAddress object,GameAddress method){
    GameApplication *a=c;
    if(method==0x5541d0 || method==0x4e68f0 || method==0x408100)return vehicle_presentation_group_geometry(&a->memory,object,1);
    if(method==0x4086d0 || method==0x4e6ee0)return vehicle_presentation_group_geometry(&a->memory,object,3);
    return 0;
}
static int overlap(void *c,GameAddress object,GameAddress method,const float position[3],float contact[3],int *hit){
    return game_level_scene_overlap(c,object,method,position,contact,hit);
}
int game_level_vehicle_input_tick(GameApplication *a,GameAddress queue,uint32_t tick){
    VehicleInputServices services={a,playing,stop,play,volume,refresh,overlap};(void)tick;
    return a && queue>=0x264 && vehicle_presentation_input_tick(&a->memory,queue-0x264,&services);
}
