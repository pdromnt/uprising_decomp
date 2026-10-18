#include "campaign_world.h"
#include "level_settings.h"
#include "menu_sound.h"
#include <stdio.h>
#include <string.h>
extern int game_level_description_load(GameApplication *);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static int text(GameApplication *a,GameAddress p,char *out,size_t capacity) {
    size_t i;for(i=0;i<capacity;++i){out[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!out[i])return !a->memory.fault;}return 0;
}
/*52fd00: select the actual mission, read its settings and objective through
 * existing native readers, then request the briefing route. */
int game_menu_campaign_launch(GameApplication *a) {
    GameAddress root=get(a,0x585408),planet;char level[128],path[256],objective[512];
    if(!root)return 0;planet=game_campaign_mission(a,get(a,root+0x1c),get(a,root+0x20));
    if((!planet || get(a,planet+0x2c)) && get(a,0x60286c)==UINT32_MAX)return game_menu_sound(a,0x92);
    if(!planet || !text(a,planet+0xb0,level,sizeof(level)))return 0;
    if(!game_memory_read_u8(&a->memory,0x5efec0) && !game_memory_write(&a->memory,0x5efec0,"PLAYER",7))return 0;
    if(!game_memory_write(&a->memory,0x629808,level,strlen(level)+1) || !game_menu_sound(a,0x91))return 0;
    if(snprintf(path,sizeof(path),"grids\\%s",level)>=(int)sizeof(path) ||
       !game_level_settings_read(a,path) || !game_level_description_load(a) ||
       !text(a,0x5ac9e0,objective,sizeof(objective)) ||
       !game_memory_write(&a->memory,0x628c00,objective,strlen(objective)+1))return 0;
    game_memory_write_u32(&a->memory,0x5910a8,1);
    return !a->memory.fault && game_menu_request(a,get(a,0x59d4c0),69,1000,31);
}
