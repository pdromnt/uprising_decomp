#include "menu_widgets.h"
#include <string.h>

extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_sound(GameApplication *,uint32_t);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_complete_research(GameApplication *); /*004a7110 and shared488fd70/488fcd0 consumers. */
extern int game_menu_cheat_action(GameApplication *); /*0052cb80. */
extern int game_menu_palette_transition(GameApplication *,int down,int up); /*0051a840/0051a890 workflow. */
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int focus_action(GameApplication *a,GameAddress p,uint32_t message) {
    /*Focus and pointer input consume the same native control algorithms.
     *Unsupported record classes fail there; no foreign virtual-call adapter. */
    return game_menu_button_input(a,p,message,0,0);
}
/*005221f0: focus links belong to the existing12-byte owner. Down/up wrap
 * only when there is more than one link; activate sends press then release. */
int game_menu_focus_key(GameApplication *a,GameAddress root,uint32_t command) {
    GameAddress head=get(a,root,0),current=get(a,root,8),next;
    if(!head)return !a->memory.fault;
    if(command==0x34) {
        if(!current)return !a->memory.fault;
        if(!focus_action(a,get(a,current,0),0x201))return 0;
        current=get(a,root,8);
        return current && focus_action(a,get(a,current,0),0x202);
    }
    if(command!=0x32 && command!=0x33)return !a->memory.fault;
    if(!current) {
        next=head;
        put(a,root,8,next);return focus_action(a,get(a,next,0),0x20a);
    }
    next=get(a,current,command==0x32?4:8);
    if(!next) {
        if(head==get(a,root,4))return !a->memory.fault;
        next=get(a,root,command==0x32?0:4);
    }
    if(!focus_action(a,get(a,current,0),0x20b))return 0;
    /* The original re-reads the current link after an exit callback. */
    current=get(a,root,8);
    if(current && get(a,current,command==0x32?4:8))next=get(a,current,command==0x32?4:8);
    put(a,root,8,next);return focus_action(a,get(a,next,0),0x20a);
}
/*0052c2e0: ordinary key navigation and the16-byte original cheat ring.
 * The reference uses strstr, with case-sensitive substring matches; it is
 * neither a character-by-character interpreter nor a virtual key mapper. */
static int fallback_key(GameApplication *a,uint32_t key) {
    GameAddress root=get(a,0x5980c4,0),event;uint32_t now,index,state;char text[17];unsigned i;
    if(!root)return !a->memory.fault;
    if((key==13 || key==38 || key==40) &&
       !game_menu_focus_key(a,root,key==13?0x34:key==38?0x33:0x32))return 0;
    now=game_menu_milliseconds(a);
    if((int32_t)(get(a,0x598138,0)+3000)<(int32_t)now) {
        put(a,0x598138,0,now);game_memory_fill_u8(&a->memory,0x628a78,16,'a',0);
    }
    index=get(a,0x598134,0);if(index>=16)return 0;
    game_memory_write_u8(&a->memory,0x628a78+index,(uint8_t)key);
    index=(index+1)%16;put(a,0x598134,0,index);
    for(i=0;i<16;++i)text[i]=(char)game_memory_read_u8(&a->memory,0x628a78+(index+i)%16);
    text[16]=0;event=get(a,0x59d4c4,0);
    if(strstr(text,"all") || strstr(text,"herauf")) {
        if(event)put(a,0x58ac08,0,1);
    } else if(strstr(text,"max")) {
        if(event && get(a,event,0x3c)==11 && !game_menu_complete_research(a))return 0;
    } else if(strstr(text,"happy")) {
        state=get(a,0x598124,0);
        if(state==3) {
            if(!game_menu_sound(a,141) || !game_menu_cheat_action(a))return 0;
        } else put(a,0x598124,0,state==2?0:state+1);
    } else if(strstr(text,"joy"))put(a,0x598124,0,get(a,0x598124,0)==2?3:0);
    else if(strstr(text,"michi")) {
        if(!game_menu_palette_transition(a,1,1))return 0;
    } else if(strstr(text,"pamski")) {
        if(!game_menu_palette_transition(a,0,1))return 0;
    } else return !a->memory.fault;
    put(a,0x598138,0,0);return !a->memory.fault;
}
int game_menu_keyboard_native(GameApplication *a,uint32_t method,GameAddress receiver,
    uint32_t key,uint32_t flags,uint32_t argument) {
    if(method==0x531700){
        extern int game_campaign_keyboard(GameApplication *,uint32_t);
        return game_campaign_keyboard(a,key);
    }
    if(method==0x52c2e0)return fallback_key(a,key);
    if(method==0x521a70)return game_menu_text_entry_key(a,receiver,key,flags);
    if(method==0x5246f0) {
        extern int game_menu_video_action(GameApplication *,uint32_t,uint32_t);
        return game_menu_video_action(a,method,argument);
    }
    if(method==0x5242f0) {
        extern int game_menu_save_action(GameApplication *,uint32_t,GameAddress,uint32_t);
        return game_menu_save_action(a,method,receiver,key);
    }
    /*4cd970's actual callers register531700,52c2e0,5242f0 or5246f0;
     *521d80/521dc0 select the text owner's521a70 through4cd8f0.
     *Every registered implementation above is ordinary native behavior. */
    a->memory.fault=1;return 0;
}
