#include "campaign_world.h"
#include "menu_widgets.h"
#include "scene_projection_storage.h"
#include <stdio.h>
#include <string.h>
extern int game_menu_text_wrapped_height(GameApplication *,unsigned,uint32_t,const char *,uint32_t *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int append(GameApplication *a,char *text,size_t capacity,GameAddress source) {
    size_t used=strlen(text),i;if(!source)return 1;
    for(i=0;used+i<capacity;++i){text[used+i]=(char)game_memory_read_u8(&a->memory,source+(uint32_t)i);if(!text[used+i])return !a->memory.fault;}
    return 0;
}
/*44cff0: derive display text directly from the selected owned planet. */
int game_campaign_caption_text(GameApplication *a,char *text,size_t capacity) {
    GameAddress root=get(a,0x585408),p=game_campaign_mission(a,get(a,root+0x1c),get(a,root+0x20));char reward[64];int32_t value;
    if(!p || !capacity)return 0;text[0]=0;
    if(get(a,p+0x2c))return append(a,text,capacity,0x5852e8);
    if(!append(a,text,capacity,0x585264) || !append(a,text,capacity,get(a,p+0x14c)?get(a,p+0x14c):0x58527c) ||
       !append(a,text,capacity,0x58528c) || !append(a,text,capacity,0x585290))return 0;
    value=(int32_t)get(a,p+0x150);
    if(value){if(value>=1000)snprintf(reward,sizeof(reward),"%d,%03d credits\n",value/1000,value%1000);else snprintf(reward,sizeof(reward),"%d credits\n",value);
        if(strlen(text)+strlen(reward)>=capacity)return 0;strcat(text,reward);
    }else if(!append(a,text,capacity,0x5852bc))return 0;
    if(!append(a,text,capacity,0x5852c4) || (get(a,p+0x56c) && !append(a,text,capacity,0x5852c8)) ||
       (get(a,p+0x554) && !append(a,text,capacity,0x5852dc)))return 0;
    return !a->memory.fault;
}
/*44d210: the selector owns its one content button and that button owns the
 * actual text surface. No retained copy of mission state needs synchronizing. */
int game_campaign_caption_update(GameApplication *a,GameAddress caption) {
    GameAddress root=get(a,0x585408),planet=game_campaign_mission(a,get(a,root+0x1c),get(a,root+0x20)),selector=get(a,caption+0xc),p,row,images[4]={0};
    GameMenuTextStyle style={0,255,0,1,206,0,0,0,0};char text[4088];uint32_t height;
    if(!planet || !game_menu_text_image_set(a,get(a,caption+8),planet+0x30,1))return 0;
    if(get(a,planet+0x2c)){
        return game_menu_widget_position(a,get(a,caption+0x1c),get(a,caption+0x20)-1,get(a,caption+0x24)+19,60,0) &&
            game_menu_widget_position(a,selector,(uint32_t)-1000,get(a,caption+0x24)+21,60,0);
    }
    if(!game_menu_widget_position(a,get(a,caption+0x1c),(uint32_t)-1000,get(a,caption+0x24)+21,60,0) ||
       !game_menu_widget_position(a,selector,get(a,caption+0x20),get(a,caption+0x24)+21,60,0) ||
       !game_campaign_caption_text(a,text,sizeof(text)) || !game_menu_text_wrapped_height(a,1,206,text,&height) || !height || height>8192)return 0;
    if(!game_menu_selector_clear(a,selector))return 0;put(a,caption+0x14,0);
    style.height=height;p=game_menu_text_image_create_bytes(a,text,&style);if(!p)return 0;images[0]=p;
    row=game_menu_button_borrowed_create(a,UINT32_MAX,images,0,0,0,0,0);
    if(!row){game_menu_widget_destroy(a,p);return 0;}
    put(a,row+0x68,0); /*Original 51c5c0 row owns its supplied frame. */
    if(!game_menu_selector_append(a,selector,row,0,0)){game_menu_widget_destroy(a,row);return 0;}
    put(a,caption+0x14,row);put(a,0x58fd3c,1);return !a->memory.fault;
}
int game_campaign_caption_destroy(GameApplication *a,GameAddress caption) {
    static const unsigned owned[]={8,0xc,0x1c};unsigned i;
    if(!caption)return 1;
    for(i=0;i<3;++i){GameAddress p=get(a,caption+owned[i]);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,caption+owned[i],0);}
    put(a,caption+0x14,0);return !a->memory.fault && scene_projection_storage_free(caption);
}
/*44cdb0: bounded descriptor owns the title, scrolling text and completed image. */
GameAddress game_campaign_caption_create(GameApplication *a) {
    GameAddress caption=scene_projection_storage_allocate(0x30,"campaign caption"),p;GameMenuTextStyle style={0,255,0,4,206,20,0,0,0};
    if(!caption)return 0;game_memory_fill_u8(&a->memory,caption,0x30,0,0);
    put(a,caption+0x20,26);put(a,caption+0x24,28);put(a,caption+0x28,222);put(a,caption+0x2c,144);
    p=game_menu_text_image_create(a,0x585218,&style);if(!p)goto fail;put(a,caption+8,p);
    if(!game_menu_image_mode(a,p,101) || !game_menu_widget_position(a,p,26,32,51,0) || !game_menu_widget_visible(a,p,1))goto fail;
    p=game_menu_selector_create(a,UINT32_MAX,4,0,0x58521c);if(!p)goto fail;put(a,caption+0xc,p);
    if(!game_menu_selector_resize(a,p,222,124) || !game_menu_widget_position(a,p,26,49,51,0) || !game_menu_widget_visible(a,p,1) || !game_menu_widget_enable(a,p,1))goto fail;
    p=game_menu_image_create(a,UINT32_MAX,0x585240);if(!p)goto fail;put(a,caption+0x1c,p);
    if(!game_menu_widget_position(a,p,(uint32_t)-1000,49,60,0) || !game_menu_widget_visible(a,p,1) || !game_campaign_caption_update(a,caption))goto fail;
    return caption;
fail:(void)game_campaign_caption_destroy(a,caption);return 0;
}
