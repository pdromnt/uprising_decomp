#include "campaign_world.h"
#include "menu_widgets.h"
#include <stdio.h>
#include <string.h>
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_campaign_scene_update(GameApplication *,GameAddress);
extern int game_campaign_scene_reveal(GameApplication *,GameAddress);
extern int game_campaign_caption_update(GameApplication *,GameAddress);
extern int game_menu_register_observer(GameApplication *,GameAddress,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t value){game_memory_write_u32(&a->memory,p,value);}
static int64_t signed_word(uint32_t word){return word&0x80000000u?(int64_t)word-INT64_C(4294967296):word;}
/*531700: the campaign's existing sixteen-byte keyboard history, clock gate,
 * reveal/progression routes and credit/caption publication share actual owners. */
int game_campaign_keyboard(GameApplication *a,uint32_t key) {
    uint32_t now=game_menu_milliseconds(a),cursor=get(a,0x59c730),i,count;char history[17],credits[64];
    GameAddress scene=get(a,0x59c6c4),root=get(a,0x585408),system;int32_t balance;
    if(cursor>=16)return 0;
    if(signed_word(get(a,0x59c734)+3000)<signed_word(now)){
        put(a,0x59c734,now);game_memory_fill_u8(&a->memory,0x628ba0,16,'a',0);}
    game_memory_write_u8(&a->memory,0x628ba0+cursor,(uint8_t)key);cursor=(cursor+1)&15;put(a,0x59c730,cursor);
    for(i=0;i<16;++i)history[i]=(char)game_memory_read_u8(&a->memory,0x628ba0+((cursor+i)&15));history[16]=0;
    if(strstr(history,"nofog")){
        put(a,0x59c734,0);if(!scene)return !a->memory.fault;if(!root || (count=get(a,root+8))>7)return 0;
        for(i=0;i<count;++i){system=game_campaign_system(a,i);if(!system || !get(a,system+0x10))break;}
        {uint32_t end=i+3;while(i<count && i<end){system=game_campaign_system(a,i++);if(!system)return 0;put(a,system+0x10,1);}}
        return game_campaign_scene_update(a,scene) && game_campaign_scene_reveal(a,scene);
    }
    if(!strstr(history,"dread"))return !a->memory.fault;
    put(a,0x59c734,0);if(!scene || get(a,scene+0x10)==12)return !a->memory.fault;
    if(!root || !game_campaign_complete(a,get(a,root+0x1c),get(a,root+0x20)))return 0;
    system=game_campaign_system(a,get(a,root+0x1c));if(!system)return 0;
    if(get(a,system+4)==2){put(a,0x60286c,0);
        if(!game_campaign_save(a,"saves\\temp") || !game_menu_register_observer(a,get(a,0x59d4c0),0x524730,30))return 0;}
    if(!game_campaign_caption_update(a,get(a,0x59c6cc)))return 0;
    balance=(int32_t)signed_word(get(a,0x5e1448));
    if(balance>=1000000)snprintf(credits,sizeof(credits),"%d,%03d,%03d",balance/1000000,(balance%1000000)/1000,balance%1000);
    else if(balance>1000)snprintf(credits,sizeof(credits),"%d,%03d",balance/1000,balance%1000);
    else snprintf(credits,sizeof(credits),"%d",balance);
    return game_menu_text_image_set_bytes(a,get(a,0x59c6b4),credits,1) && game_campaign_scene_reveal(a,scene);
}
