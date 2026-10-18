#include "gameplay_input.h"
#include "menu_sound.h"
#include "gameplay_messages.h"
#include "gameplay_state.h"
#include <math.h>
#include <string.h>

/*All callbacks registered by434ae0/4836a0/49ca40 select ordinary native behavior.
 * Unsupported IDs fault rather than entering an external CPU callback. */
extern int game_input_nearby_destination(GameApplication *,GameAddress,uint32_t *,uint32_t *,int *);
extern int game_input_screen_target(GameApplication *,uint32_t *,uint32_t *,int *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t signed_word(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static float real(GameApplication *a,GameAddress p){uint32_t v=get(a,p);float f;memcpy(&f,&v,4);return f;}
static float float_word(uint32_t v){float f;memcpy(&f,&v,4);return f;}
static void real_put(GameApplication *a,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);put(a,p,bits);}
static uint32_t truncate_word(long double v){
    v=truncl(v);
    if(!isfinite(v) || v<-9223372036854775808.0L || v>=9223372036854775808.0L)return 0;
    return (uint32_t)(uint64_t)(int64_t)v;
}
static uint16_t accelerated_angle(GameApplication *a,uint32_t scaled){
    uint32_t magnitude=(scaled&0x80000000u)?0u-scaled:scaled;
    long double v=(long double)signed_word(magnitude)*signed_word(scaled);
    v*=real(a,0x594d64);v*=real(a,0x5749f0);
    return (uint16_t)(truncate_word(v)+scaled);
}
/*558a10/558a20: accumulate wrapped angle deltas in the actual command queue. */
static int add_angle(GameApplication *a,GameAddress queue,uint32_t offset,uint16_t value){
    GameMemory *m=&a->memory;
    game_memory_write_u16(m,queue+offset,(uint16_t)(game_memory_read_u16(m,queue+offset)+value));
    return !m->fault;
}
int game_input_panel_close(GameApplication *a,int alternate){
    put(a,alternate?0x589ac0:0x58404c,1);
    put(a,alternate?0x589abc:0x584048,0);
    put(a,alternate?0x5f0228:0x5ace78,get(a,0x5ac6ac));
    return !a->memory.fault;
}
/*417440: scrolling accumulates movement only in the existing sliding widgets. */
static int selector_scroll(GameApplication *a,int forward){
    GameMemory *m=&a->memory;uint32_t index=0;int high=get(a,0x597f80)==1;
    uint32_t page=get(a,0x6025f0),step=high?30u:25u;
    GameAddress panel;
    if(forward){
        uint32_t limit=get(a,0x602618)-(high?5u:4u);
        if(signed_word(limit)<=signed_word(page))return !m->fault;
    }else if(signed_word(page)<=0)return !m->fault;
    if(!game_menu_sound(a,125))return 0;
    put(a,0x6025f0,get(a,0x6025f0)+(forward?1u:UINT32_MAX));
    if(!gameplay_selector_ranges(m,get(a,0x597f80)!=0))return 0;
    panel=get(a,0x602678);step=forward?0u-step:step;
    while(signed_word(index)<signed_word(get(a,panel)) && !m->fault){
        GameAddress w=get(a,panel+4+index*4);uint32_t kind=0;
        if(index>=256 || gameplay_widget_kind(m,w,&kind)!=1){m->fault=1;return 0;}
        if(kind==2)put(a,w+0x128,get(a,w+0x128)+step);
        ++index;
    }
    return !m->fault;
}
static int selector_message(GameApplication *a,GameAddress message){
    return game_message_enqueue(a,get(a,0x628a88),message,1);
}
static int selector_sound(void *c,uint32_t sound){return game_menu_sound(c,sound);}
static int selector_text(void *c,GameAddress message,const char *bytes){
    GameApplication *a=c;
    return bytes?game_message_enqueue_bytes(a,get(a,0x628a88),bytes,1):selector_message(a,message);
}
static int selector_diagnostic(void *c,GameAddress message){
    GameApplication *a=c;char bytes[256];unsigned i;
    for(i=0;i<sizeof(bytes);++i){bytes[i]=(char)game_memory_read_u8(&a->memory,message+i);
        if(a->memory.fault)return 0;
        if(!bytes[i]){game_application_log(a,"selector",bytes);return 1;}}
    return 0;
}
/*Actual4a2290/4a23b0/4a24d0 consumers publish collection commands, not CPU calls. */
int game_input_upgrade_feedback(GameApplication *a,uint16_t status,uint8_t unit){
    GameKeyboardServices keyboard=game_input_keyboard_services(a);GameplayFeedbackServices feedback={0};
    feedback.context=a;feedback.units=keyboard.units;feedback.sound=selector_sound;
    feedback.text=selector_text;feedback.diagnostic=selector_diagnostic;
    return gameplay_collection_feedback(&a->memory,&feedback,(int16_t)status,unit);
}
static int selector_transfer(GameApplication *a,uint32_t method){
    GameMemory *m=&a->memory;GameRootEventServices s=game_input_event_services(a);
    GameAddress chosen=get(a,0x6025a0),message=0;int all=method==0x4a24d0;
    if(!chosen)message=method==0x4a2290?0x58f4ac:all?0x58f574:0x58f510;
    else if(get(a,chosen+0x54)!=get(a,0x5f0208))message=method==0x4a2290?0x58f4c0:all?0x58f588:0x58f524;
    else if(method==0x4a2290){uint32_t selected=get(a,0x6025b8);
        if(!selected)message=0x58f4dc;
        else if(selected==UINT32_MAX)message=0x58f4f8;
    }else if(!get(a,get(a,0x5f0208)+0x22c))message=all?0x58f5a4:0x58f540;
    else if(!all && get(a,0x6025b8)==UINT32_MAX)message=0x58f554;
    if(message){
        if(!game_menu_sound(a,122) || !selector_message(a,message))return 0;
        if(!all)return !m->fault;
        /*The retail bulk callback falls through after this diagnostic. */
    }
    if(!all){
        if(!game_menu_sound(a,121) || !game_root_command_publish(m,&s,get(a,0x5f0208),
            method==0x4a2290?41:42,(uint8_t)get(a,chosen),(uint8_t)get(a,0x6025b8)))return 0;
        if(method==0x4a2290)put(a,0x6025b8,UINT32_MAX);
    }else{
        uint32_t i=0;
        if(!get(a,0x6025a0))return game_menu_sound(a,122) && selector_message(a,0x58f5b8);
        if(!game_menu_sound(a,121))return 0;
        while(signed_word(i)<(int16_t)game_memory_read_u16(m,get(a,0x6025a0)+0x24) && !m->fault){
            chosen=get(a,0x6025a0);
            if(i>=65536){m->fault=1;return 0;}
            if(get(a,get(a,chosen+0x18)+i*4) && !game_root_command_publish(m,&s,get(a,0x5f0208),42,
                (uint8_t)get(a,chosen),(uint8_t)i))return 0;
            ++i;
        }
    }
    return !m->fault;
}
/* Actual shared panel callbacks435b50/4848b0 publish through the player queues. */
int game_input_panel_action(GameApplication *a,uint32_t method,uint32_t argument){
    GameAddress local=get(a,0x5f0208);GameRootEventServices events=game_input_event_services(a);
    if(method==0x4a20e0 || method==0x4a2150)return selector_scroll(a,method==0x4a2150);
    if(method==0x4a21d0)return game_selector_request(&a->memory,&events,(uint8_t)argument);
    if(method==0x4a2290 || method==0x4a23b0 || method==0x4a24d0)return selector_transfer(a,method);
    if(method==0x4a25f0){
        GameKeyboardServices keyboard=game_input_keyboard_services(a);GameplayFeedbackServices feedback={0};
        feedback.context=a;feedback.units=keyboard.units;feedback.sound=selector_sound;
        feedback.text=selector_text;feedback.diagnostic=selector_diagnostic;
        return game_selector_unit_request(&a->memory,&events,&feedback,argument);
    }
    if(method==0x4a2730){
        GameAddress chosen=get(a,0x6025a0);
        if(!chosen)return !a->memory.fault;
        if(!get(a,get(a,chosen+0x18)+argument*4)){
            if(!selector_message(a,0x58f618))return 0;
            put(a,0x6025b8,UINT32_MAX);
        }else if(get(a,get(a,0x6025a0)+0x54)!=local){
            if(!selector_message(a,0x58f634))return 0;
            put(a,0x6025b8,UINT32_MAX);
        }else if((int16_t)game_memory_read_u16(&a->memory,get(a,0x6025a0)+0x24)>signed_word(argument))put(a,0x6025b8,argument);
        return !a->memory.fault;
    }
    if(method==0x4a27b0 || method==0x4a27e0){GameAddress flag=method==0x4a27b0?0x602598:0x6025b4;
        if(!game_menu_sound(a,method==0x4a27b0?133:121))return 0;
        put(a,flag,get(a,flag)^1);return !a->memory.fault;}
    if(method==0x435930 || method==0x4846a0)return game_input_panel_close(a,method==0x4846a0);
    if(method==0x484920)return game_panel_press_clear(&a->memory,get(a,0x5f023c));
    if(method==0x4848b0){
        return game_menu_sound(a,121) && game_input_byte_publish(a,get(a,local+0x3bc)+0x13c,9,(uint8_t)argument) &&
            game_panel_press_clear(&a->memory,get(a,0x5f023c)) && game_input_panel_close(a,1);
    }
    if(method==0x435b50){
        uint32_t first=0,second=0;int found;
        if(argument>=65536u){a->memory.fault=1;return 0;}
        if(!game_menu_sound(a,get(a,0x5acea0+argument*4)?121:123))return 0;
        switch(argument){
        case 0:return game_root_count_publish(&a->memory,&events,local,27,4);
        case 1:case 2:return game_root_signal_publish(&a->memory,&events,local,(uint8_t)(27+argument));
        case 3:case 4:
            if(!game_input_nearby_destination(a,get(a,local+0x448)+0x18,&first,&second,&found))return 0;
            if(found)return game_root_pair_publish(&a->memory,&events,local,(uint8_t)(27+argument),first,second);
            return game_message_enqueue(a,get(a,0x628a88),argument==3?0x5840c0:0x5840d0,1);
        case 5:case 6:
            if(!game_input_screen_target(a,&first,&second,&found))return 0;
            return !found || game_root_pair_publish(&a->memory,&events,local,(uint8_t)(38+argument),first,second);
        default:return !a->memory.fault;
        }
    }
    a->memory.fault=1;return 0;
}
/*4a16e0/4a17b0: toggle the closest existing point, otherwise append to the
 * first empty slot. Float arguments remain IEEE words in the owned five slots. */
static int navigation_remove(GameApplication *a,uint32_t first,uint32_t second){
    unsigned i;int nearest=-1;float closest=16777216.0f;
    for(i=0;i<5;i++){
        GameAddress p=0x602538+i*8;long double dx,dy,distance;
        if(get(a,p)==0xbf800000u && get(a,p+4)==0xbf800000u)continue;
        dx=(long double)real(a,p)-float_word(first);dy=(long double)real(a,p+4)-float_word(second);
        distance=dy*dy+dx*dx;
        if(distance<closest || isnan(distance) || isnan(closest)){nearest=(int)i;closest=(float)distance;}
    }
    if(nearest>=0){put(a,0x602538+(uint32_t)nearest*8,0xbf800000u);put(a,0x60253c+(uint32_t)nearest*8,0xbf800000u);}
    return a->memory.fault?-1:nearest>=0;
}
static int navigation_append(GameApplication *a,uint32_t first,uint32_t second){
    unsigned i;
    for(i=0;i<5;i++){
        GameAddress p=0x602538+i*8;
        if(get(a,p)==0xbf800000u && get(a,p+4)==0xbf800000u){put(a,p,first);put(a,p+4,second);break;}
    }
    return !a->memory.fault;
}
int game_input_selector_pair(GameApplication *a,uint32_t first,uint32_t second){
    int removed=navigation_remove(a,first,second);
    /* The mouse caller ignores the original capacity-result value. */
    return removed>=0 && (removed || navigation_append(a,first,second));
}
/*4df570's ON route removes a matching old point, then appends it again;
 *OFF only removes. Mouse toggling and script setting share the same five slots. */
int game_input_navigation_set(GameApplication *a,uint32_t first,uint32_t second,int enabled){
    return navigation_remove(a,first,second)>=0 && (!enabled || navigation_append(a,first,second));
}
/*4a1140: signed rectangle hit and original fixed-point map coordinates. */
int game_input_selector_hit(GameApplication *a,int32_t x,int32_t y,uint32_t *first,uint32_t *second,int *hit){
    uint32_t left=get(a,0x6027b8),right=get(a,0x6027c0),top=get(a,0x6027bc),bottom=get(a,0x6027c4);
    float value;
    *hit=x>=signed_word(left) && x<=signed_word(right) && y>=signed_word(top) && y<=signed_word(bottom);
    if(*hit){
        value=(float)((long double)signed_word(((uint32_t)x-left)<<17)/signed_word(right-left));memcpy(first,&value,4);
        value=(float)((long double)signed_word((bottom-(uint32_t)y-1u)<<17)/signed_word(bottom-top));memcpy(second,&value,4);
    }
    return !a->memory.fault;
}
/*4a1be0: nearest visible collection, double-click command, local/foreign
 * selection feedback, entity command and queued position utterance. */
int game_input_selector_choose(GameApplication *a,int32_t x,int32_t y,int *accepted){
    GameMemory *m=&a->memory;int32_t count=(int16_t)game_memory_read_u16(m,0x639a74);
    int32_t index=-1,best=0,i;GameAddress rows=get(a,0x602698),objects,chosen,local,entity,list;
    GameRootEventServices events=game_input_event_services(a);uint32_t now,label;
    *accepted=0;
    if(count<=0)return !m->fault;
    for(i=0;i<count;++i){
        GameAddress p=rows+(uint32_t)i*12;uint32_t dx,dy,distance;
        if(!get(a,p+8))continue;
        dx=get(a,p)-(uint32_t)x;dy=get(a,p+4)-(uint32_t)y;distance=dx*dx+dy*dy;
        if(signed_word(distance)<256 && (index<0 || signed_word(distance)<best)){index=i;best=signed_word(distance);}
    }
    if(m->fault)return 0;
    if(index<0){put(a,0x6025b8,UINT32_MAX);put(a,0x58de94,UINT32_MAX);put(a,0x6025a0,0);return !m->fault;}
    objects=get(a,0x639a70);chosen=get(a,objects+(uint32_t)index*4);local=get(a,0x5f0208);now=get(a,0x5ac6ac);
    if(signed_word(get(a,0x58de94))>=0 && signed_word(now-get(a,0x58de94))<=8 &&
        chosen==get(a,0x6025a0) && get(a,chosen+0x54)==local){
        put(a,0x58de94,UINT32_MAX);*accepted=1;
        return game_root_command_publish(m,&events,local,50,2,(uint8_t)index);
    }
    put(a,0x6025a0,chosen);put(a,0x58de94,now);
    if(!get(a,chosen+0x54))label=0x58f464;
    else if(get(a,chosen+0x54)!=local)label=0x58f494;
    else{
        put(a,0x6025b8,0);label=0x58f480;list=get(a,chosen+0x18);
        list=get(a,list);
        if(list){
            entity=get(a,list+0x48);
            if(entity){
                if(!game_root_word_publish(m,&events,local,36,get(a,chosen)))return 0;
                if((get(a,entity+0x14)&63u)==game_memory_read_u16(m,local+8)){
                    long double first=real(a,entity+0x18),second=real(a,entity+0x20);int started;
                    int16_t sounds[8]={87,0,0,-1,-1,-1,-1,-1};
                    first*=real(a,0x571f64);first*=real(a,0x571f68);
                    second*=real(a,0x571f64);second*=real(a,0x571f68);
                    sounds[1]=(int16_t)(uint16_t)(48u-truncate_word(first));sounds[2]=(int16_t)(uint16_t)(29u+truncate_word(second));
                    if(!game_menu_sound_sequence(a,sounds,&started))return 0;
                    if(started){game_memory_write_u16(m,local+0x598,87);put(a,local+0x58c,get(a,0x5ac6ac));}
                }
            }
        }
    }
    *accepted=1;
    return game_message_enqueue(a,get(a,0x628a88),label,1);
}
/*435820/484590: clamp the authoritative cursor and return only the excess.
 * Mouse movement inside a panel does not also rotate the gameplay camera. */
int game_input_panel_clamp(GameApplication *a,int alternate,uint32_t excess[2]){
    uint32_t x=get(a,0x5ac9c8),y=get(a,0x5ac9cc),low,high;
    int tall=!!get(a,0x597f80);
    excess[0]=excess[1]=0;
    low=get(a,alternate?0x5f0240:0x5acec0)+1;
    high=get(a,alternate?0x5f0248:0x5acec8)-1;
    if(signed_word(x)<signed_word(low)){excess[0]=x-low;put(a,0x5ac9c8,low);}
    else if(signed_word(x)>signed_word(high)){excess[0]=x-high;put(a,0x5ac9c8,high);}
    low=(tall?332u:147u)-get(a,alternate?(tall?0x5f0234:0x5f0238):(tall?0x5aced8:0x5aced0));
    high=tall?330u:145u;
    if(signed_word(y)<signed_word(low)){excess[1]=y-low;put(a,0x5ac9cc,low);}
    else if(signed_word(y)>signed_word(high)){excess[1]=y-high;put(a,0x5ac9cc,high);}
    return !a->memory.fault;
}
static int inside(GameApplication *a,GameAddress w,int32_t x,int32_t y){
    return signed_word(get(a,w+0xa8))<=x && signed_word(get(a,w+0xb0))>=x &&
        signed_word(get(a,w+0xb4))<=y && signed_word(get(a,w+0xac))>=y;
}
extern uint32_t game_menu_milliseconds(GameApplication *);
/*4172d0 and4168d0/416f70: only a newly-entered child accepts hover.
 * Preserve the disabled widget's original asymmetric left-edge condition. */
int game_input_panel_hover(GameApplication *a,GameAddress panel,int32_t x,int32_t y,GameAddress *selected){
    uint32_t index=0;*selected=0;
    while(!a->memory.fault && signed_word(get(a,panel))>signed_word(index)){
        GameAddress slot=panel+4+index*4,w=get(a,slot);uint32_t method=get(a,get(a,w));int hit;
        if(index>=256u){a->memory.fault=1;return 0;}
        if(method!=0x4168d0 && method!=0x416f70){a->memory.fault=1;return 0;}
        if(method==0x416f70 && (get(a,w+0x128) || get(a,w+0x12c))){put(a,w+0x14,0);++index;continue;}
        hit=(!get(a,w+8) || signed_word(get(a,w+0xa8))<=x) && signed_word(get(a,w+0xb0))>=x &&
            signed_word(get(a,w+0xb4))<=y && signed_word(get(a,w+0xac))>=y;
        if(!hit)put(a,w+0x14,0);
        else if(!get(a,w+0x14)){
            put(a,w+0x14,game_menu_milliseconds(a));*selected=get(a,slot);return !a->memory.fault;
        }
        ++index;
    }
    return !a->memory.fault;
}
/*435a00/484760: existing button bounds and hover index share their original
 * panel owner. No replacement widget is allocated during opening. */
int game_input_panel_layout(GameApplication *a,int alternate,int high){
    GameAddress bounds=alternate?0x5f0240:0x5acec0,count_at=alternate?0x5f022c:0x5acefc;
    GameAddress items=alternate?0x5f0250:0x5ace80,hit_at=alternate?0x5f0230:0x5ace7c;
    uint32_t index=0,step=high?14u:10u,height=high?12u:8u;
    put(a,hit_at,UINT32_MAX);
    while(signed_word(get(a,count_at))>signed_word(index) && !a->memory.fault){
        GameAddress w=get(a,items+index*4);uint32_t left=get(a,bounds)+1,right=get(a,bounds+8)-1,top=get(a,bounds+4)+index*step+1;
        if(index>=256u){a->memory.fault=1;return 0;}
        put(a,w+0xa8,left);put(a,w+0xac,top+height);put(a,w+0xb0,right);put(a,w+0xb4,top);
        if(inside(a,w,signed_word(get(a,0x5ac9c8)),signed_word(get(a,0x5ac9cc))))put(a,hit_at,index);
        ++index;
    }
    return !a->memory.fault;
}
/*4356f0/484460 and actual keyboard40/42: mutually-exclusive command panels,
 * queued vehicle stop, layout, animation deadline, clamp and initial hover. */
int game_input_command_panel_toggle(GameApplication *a,int alternate){
    GameMemory *m=&a->memory;GameAddress active=alternate?0x589abc:0x584048,pending=alternate?0x589ac0:0x58404c;
    GameAddress bounds=alternate?0x5f0240:0x5acec0;GameAddress ignored;uint32_t excess[2],top;
    int high=get(a,0x597f80)!=0;
    if(get(a,active))return game_input_panel_close(a,alternate);
    if(get(a,alternate?0x584048:0x589abc) && !game_input_panel_close(a,!alternate))return 0;
    if(!game_input_queue_button(a,get(a,get(a,0x5f0208)+0x3bc)+0x13c,6))return 0;
    if(alternate){put(a,bounds,high?578u:277u);put(a,bounds+8,high?638u:318u);}
    else{put(a,bounds,get(a,0x627928)-(high?62u:43u));put(a,bounds+8,get(a,0x627928)-2);}
    if(!get(a,pending)){
        top=high?331u:146u;put(a,bounds+4,top);
        put(a,bounds+12,top+get(a,alternate?(high?0x5f0234:0x5f0238):(high?0x5aced8:0x5aced0)));
    }
    if(!game_input_panel_layout(a,alternate,high))return 0;
    put(a,active,1);put(a,alternate?0x5f0228:0x5ace78,get(a,0x5ac6ac));put(a,pending,0);
    return !m->fault && game_input_panel_clamp(a,alternate,excess) &&
        game_input_panel_hover(a,get(a,alternate?0x5f023c:0x5aced4),signed_word(get(a,0x5ac9c8)),signed_word(get(a,0x5ac9cc)),&ignored);
}
/*416a40/416ac0,416c50/416cc0,416ef0/417000. Callbacks are ordinary
 * address-keyed native actions. Press state stays in each existing widget. */
static int widget_pointer(GameApplication *a,GameAddress w,int pressed,int32_t x,int32_t y,int *handled){
    uint32_t method=get(a,get(a,w)+(pressed?12u:16u)),callback=0,argument=0;
    int hit;
    *handled=0;
    if(pressed){
        switch(method){
        case 0x416a40:case 0x416ef0:
            if(!get(a,w+8))return !a->memory.fault;
            hit=inside(a,w,x,y);put(a,w+4,(uint32_t)hit);
            *handled=hit;break;
        case 0x416c50:
            if(!get(a,w+8) || !inside(a,w,x,y))return !a->memory.fault;
            put(a,w+4,get(a,w+4)<1u);*handled=1;break;
        default:a->memory.fault=1;return 0;
        }
        if(*handled){callback=get(a,w+0xb8);argument=get(a,w+0x120);}
    }else{
        switch(method){
        case 0x416ac0:case 0x417000:
            if(!get(a,w+4))return !a->memory.fault;
            put(a,w+4,0);*handled=1;
            if(inside(a,w,x,y)){callback=get(a,w+0xbc);argument=get(a,w+0x124);}
            break;
        case 0x416cc0:
            if(!inside(a,w,x,y))return !a->memory.fault;
            *handled=1;callback=get(a,w+0xbc);argument=get(a,w+0x124);break;
        default:a->memory.fault=1;return 0;
        }
    }
    if(a->memory.fault)return 0;
    return !callback || game_input_panel_action(a,callback,argument);
}
/*417280/417320: first accepting child wins. Re-read count and selected slot
 * after callbacks, because callbacks may change panel membership. */
int game_input_panel_pointer(GameApplication *a,GameAddress panel,int pressed,int32_t x,int32_t y,GameAddress *selected){
    uint32_t index=0;int handled;
    *selected=0;
    while(!a->memory.fault && signed_word(get(a,panel))>signed_word(index)){
        GameAddress slot=panel+4+index*4,w=get(a,slot);
        if(index>=256u){a->memory.fault=1;return 0;}
        if(!widget_pointer(a,w,pressed,x,y,&handled))return 0;
        if(handled){*selected=get(a,slot);return !a->memory.fault;}
        ++index;
    }
    return !a->memory.fault;
}
static int vehicle_mouse(GameApplication *a,const GameInputEvent *e){
    GameAddress vehicle,queue;uint32_t scaled=e->value,excess[2];
    int axis=e->offset==4,pressed=!!(e->value&128),panel;
    if(e->offset!=0 && e->offset!=4 && e->offset!=12 && e->offset!=13)return !a->memory.fault;
    panel=get(a,0x584048)?1:get(a,0x589abc)?2:0;
    if(e->offset==0 || axis){
        if(panel){
            GameAddress cursor=axis?0x5ac9cc:0x5ac9c8;
            put(a,cursor,get(a,cursor)+truncate_word((long double)signed_word(scaled)*real(a,0x5749d8)));
            if(!game_input_panel_clamp(a,panel==2,excess))return 0;
            scaled=excess[axis];if(!scaled)return !a->memory.fault;
        }
        scaled=truncate_word((long double)signed_word(scaled)*real(a,0x594d68));
        vehicle=get(a,get(a,0x5f0208)+0x3bc);queue=vehicle+0x13c;
        if(!axis)return add_angle(a,queue,(get(a,vehicle+0x2f4)&4)?12u:16u,(uint16_t)(0u-accelerated_angle(a,scaled)));
        return add_angle(a,queue,14u,(!panel && get(a,0x594d44))?(uint16_t)(0u-accelerated_angle(a,scaled)):accelerated_angle(a,scaled));
    }
    if(panel){
        GameAddress selected;
        if(e->offset==13)return game_input_panel_close(a,panel==2);
        return game_input_panel_pointer(a,get(a,panel==2?0x5f023c:0x5aced4),pressed,
            signed_word(get(a,0x5ac9c8)),signed_word(get(a,0x5ac9cc)),&selected);
    }
    queue=get(a,get(a,0x5f0208)+0x3bc)+0x13c;
    return game_input_queue_button(a,queue,(uint8_t)(e->offset==12?(pressed?2:4):(pressed?3:5)));
}
static int overhead_mouse(GameApplication *a,const GameInputEvent *e){
    GameAddress queue=get(a,get(a,0x5f0208)+0x448)+0x264;
    int pressed=!!(e->value&128);
    if(e->offset==0 || e->offset==4){
        uint16_t angle=accelerated_angle(a,truncate_word((long double)signed_word(e->value)*real(a,0x594d68)));
        if(e->offset==0 || get(a,0x594d44))angle=(uint16_t)(0u-angle);
        return add_angle(a,queue,e->offset==0?16u:14u,angle);
    }
    if(e->offset==12 || e->offset==13){
        put(a,e->offset==12?0x594df8:0x594dfc,(uint32_t)pressed);
        if(!pressed && get(a,e->offset==12?0x594dfc:0x594df8))return !a->memory.fault;
        return game_input_queue_button(a,queue,(uint8_t)(pressed?21:22));
    }
    return !a->memory.fault;
}
static int selector_panels(GameApplication *a,int pressed,int32_t x,int32_t y){
    GameAddress selected;
    if(!game_input_panel_pointer(a,get(a,0x60259c),pressed,x,y,&selected))return 0;
    if(selected)return 1;
    return game_input_panel_pointer(a,get(a,0x602678),pressed,x,y,&selected);
}
static int selector_mouse(GameApplication *a,const GameInputEvent *e){
    int32_t x,y;int hit,accepted;uint32_t first,second;
    if(e->offset==0 || e->offset==4){
        GameAddress cursor=e->offset==0?0x5ac9c8:0x5ac9cc;
        uint32_t value=get(a,cursor)+e->value,limit=get(a,e->offset==0?0x627928:0x62792c);
        if(signed_word(value)<0)value=0;
        else if(signed_word(value)>=signed_word(limit))value=limit-1;
        put(a,cursor,value);return !a->memory.fault;
    }
    x=signed_word(get(a,0x5ac9c8));y=signed_word(get(a,0x5ac9cc));
    if(e->offset==12){
        if(e->value&128){
            if(!game_input_selector_hit(a,x,y,&first,&second,&hit))return 0;
            if(!hit)return selector_panels(a,1,x,y);
            if(game_memory_read_u8(&a->memory,0x594d5c) || get(a,0x6025b4))return game_input_selector_pair(a,first,second);
            if(!game_input_selector_choose(a,x,y,&accepted))return 0;
            return !accepted || game_menu_sound(a,124);
        }
        if(!selector_panels(a,0,x,y))return 0;
        put(a,0x6027e4,get(a,0x6027e4)|1u);return !a->memory.fault;
    }
    if(e->offset==13 && !(e->value&128)){
        if(get(a,0x6025b4)){
            put(a,0x6025b4,0);
            if(!game_menu_sound(a,121))return 0;
            put(a,get(a,get(a,0x597f80)?0x602634:0x602614)+4,0);
            return !a->memory.fault;
        }
        {GameRootEventServices events=game_input_event_services(a);return game_selector_request(&a->memory,&events,3);}
    }
    return !a->memory.fault;
}
/*4b1950: intersect an outside point's ray from the opposite vertex with
 * each triangle edge. The point and triangle retain their existing ownership.
 * Explicit float stores preserve the original edge/intermediate rounding. */
static int weight_point_clamp(GameApplication *a){
    static const unsigned start[3]={0,1,2},end[3]={2,0,1},opposite[3]={1,2,0};
    float vx[3],vy[3];unsigned i;
    for(i=0;i<3;i++){vx[i]=real(a,0x603850+i*8);vy[i]=real(a,0x603854+i*8);}
    for(i=0;i<3;i++){
        unsigned s=start[i],e=end[i],o=opposite[i];
        long double x=real(a,0x6038b8),y=real(a,0x6038bc);
        float dy=(float)((long double)vy[s]-vy[e]);
        long double side=(y-vy[s])*((long double)vx[e]-vx[s])+(x-vx[s])*dy;
        if(!(side>=real(a,0x5744f4))){
            long double edge=dy/((long double)vx[s]-vx[e]);
            long double intercept=vy[s]-(long double)vx[s]*edge;
            long double ray=(y-vy[o])/(x-vx[o]);
            long double projected=((long double)vy[o]-(long double)vx[o]*ray-intercept)/(edge-ray);
            real_put(a,0x6038b8,(float)projected);
            real_put(a,0x6038bc,(float)(intercept+projected*edge));
        }
    }
    return !a->memory.fault;
}
/*4b1d60: triangle area ratios become original rounded percentage weights.
 * No duplicated slider object or serialized mirror sits between producer and
 * the weighted camera/command consumers at6040fc/6040a4/604114. */
int game_input_weight_publish(GameApplication *a){
    long double ax=real(a,0x603850),ay=real(a,0x603854),bx=real(a,0x603858),by=real(a,0x60385c);
    long double cx=real(a,0x603860),cy=real(a,0x603864),px=real(a,0x6038b8),py=real(a,0x6038bc);
    float inverse=(float)(real(a,0x574500)/((cx-bx)*(by-ay)));
    float height=(float)(cy-py),width=(float)(cx-px),ratio;
    long double value,scaled;uint32_t word;
    ratio=(float)(fabsl((bx-px)*height-(by-py)*width)*inverse);
    value=ratio;value+=(value>real(a,0x5744f4))?real(a,0x5745b8):-real(a,0x5745b8);
    word=truncate_word(value);real_put(a,0x6040fc,(float)((long double)signed_word(word)*real(a,0x574608)));
    ratio=(float)(fabsl((ay-py)*width-(ax-px)*height)*inverse);
    value=ratio;value+=(value>real(a,0x5744f4))?real(a,0x5745b8):-real(a,0x5745b8);
    word=truncate_word(value);scaled=(long double)signed_word(word)*real(a,0x574608);
    real_put(a,0x6040a4,(float)scaled);
    real_put(a,0x604114,(float)(real(a,0x5744e4)-(scaled+real(a,0x6040fc))));
    return !a->memory.fault;
}
/*4b2db0/4b2df0 are shared by mouse, joystick and force-feedback input. */
int game_input_weight_move(GameApplication *a,unsigned channel,uint32_t value){
    GameAddress coordinate=channel?0x6038bc:0x6038b8;
    long double delta=(long double)signed_word(0u-value)*real(a,0x574620);
    if(channel>1)return 0;
    delta*=((long double)real(a,0x603854)-real(a,0x60385c));
    real_put(a,coordinate,(float)(delta+real(a,coordinate)));
    return weight_point_clamp(a);
}
/* Inline mode5 in4d0220 and4ad6e0. */
int game_input_placement_mouse(GameApplication *a,const GameInputEvent *e){
    if(e->offset==0 || e->offset==4){
        if(!game_input_weight_move(a,e->offset==4,e->value))return 0;
    }else if(e->offset==12)put(a,0x594e00,!!(e->value&128));
    else if(e->offset==13 && (e->value&128)){
        put(a,0x6038b8,get(a,0x603e30));put(a,0x6038bc,get(a,0x603e34));
        if(!game_input_weight_publish(a))return 0;
    }
    if(get(a,0x594e00))return game_input_weight_publish(a);
    return !a->memory.fault;
}
int game_input_game_mouse(GameApplication *a,uint32_t mode,const GameInputEvent *event){
    if(!event || a->memory.fault)return 0;
    switch(mode){
    case 1:return vehicle_mouse(a,event);
    case 2:return overhead_mouse(a,event);
    case 5:return game_input_placement_mouse(a,event);
    case 7:return selector_mouse(a,event);
    default:return !a->memory.fault;
    }
}

extern int game_application_options_load(GameApplication *,uint32_t);
extern int game_menu_pointer_read(GameApplication *,int32_t[2]);
extern int game_menu_pointer_position(GameApplication *,int32_t,int32_t);
/*4132b0: detach the actual selection animation nodes from their shared list. */
static int selection_unlink(GameApplication *a,GameAddress node){
    GameAddress next=get(a,node+12),previous=get(a,node+16);
    if(get(a,0x583520)==node)put(a,0x583520,next);
    if(get(a,0x583524)==node)put(a,0x583524,previous);
    if(previous)put(a,previous+12,next);
    if(next)put(a,next+16,previous);
    put(a,node+12,0);put(a,node+16,0);return !a->memory.fault;
}
/*4b2650/2cf0/2d50 plus the actual vehicle caller: retire animation links,
 * recover the barycentric point, retain cancel weights and enter the selector. */
int game_input_selector_begin(GameApplication *a){
    GameMemory *m=&a->memory;int high=get(a,0x597f80)==1;int32_t point[2],cx,cy;
    long double x,y;float weapons,shields;
    if(!selection_unlink(a,high?0x603830:0x603f60) || !selection_unlink(a,high?0x603620:0x603ff8))return 0;
    put(a,0x6037fc,0);put(a,0x6038e8,get(a,0x5ac6ac));
    weapons=real(a,0x6040fc);shields=real(a,0x604114);
    x=((long double)real(a,0x603860)-real(a,0x603858))*shields+
        ((long double)real(a,0x603850)-real(a,0x603858))*weapons;
    y=((long double)real(a,0x603854)-real(a,0x60385c))*weapons;
    real_put(a,0x6038bc,(float)(y+real(a,0x60385c)));real_put(a,0x6038b8,(float)(x+real(a,0x603858)));
    put(a,0x603800,get(a,0x6040fc));put(a,0x603a40,get(a,0x6040a4));put(a,0x6040d8,get(a,0x604114));
    put(a,0x604490,get(a,0x604760));
    if(m->fault || !game_application_options_load(a,6) || !game_menu_pointer_read(a,point))return 0;
    put(a,0x5ac9c8,(uint32_t)point[0]);put(a,0x5ac9cc,(uint32_t)point[1]);
    cx=signed_word((get(a,0x5ac6a0)>>1)|(get(a,0x5ac6a0)&0x80000000u));
    cy=signed_word((get(a,0x5ac6a4)>>1)|(get(a,0x5ac6a4)&0x80000000u));
    if(point[0]==cx && point[1]==cy)return !m->fault;
    if(m->fault || !game_menu_pointer_position(a,cx,cy))return 0;
    put(a,0x5ac9d0,(uint32_t)cx);put(a,0x5ac9d4,(uint32_t)cy);return !m->fault;
}
/*4d55d0: accept/cancel and preset cycling in the triangle weight selector.
 * The signed comparisons are deliberately on the stored IEEE words. */
int game_input_placement_keyboard(GameApplication *a,const GameInputEvent *e) {
    static const uint32_t weights[4][3]={{0x3f800000,0,0},{0,0x3f800000,0},{0,0,0x3f800000},{0x3eaaaa3b,0x3eaaaa3b,0x3eaaab8a}};
    static const GameAddress labels[4]={0x595da0,0x595dac,0x595db8,0x595dc4};
    static const GameAddress fields[3]={0x6040fc,0x6040a4,0x604114};
    GameMemory *m=&a->memory;uint32_t action;unsigned i;
    if(!(e->value&128) || e->offset>=256 || m->fault)return !m->fault;
    action=get(a,0x6047f0+e->offset*4);
    if(action==37) {
        uint16_t stored=game_memory_read_u16(m,0x603744);int16_t signed_cycle;int cycle;
        memcpy(&signed_cycle,&stored,sizeof signed_cycle);cycle=((int)signed_cycle+1)%4;
        game_memory_write_u16(m,0x603744,(uint16_t)cycle);
        if(cycle<0)return !m->fault;
        for(i=0;i<3;++i)put(a,fields[i],weights[cycle][i]);
        return !m->fault && game_message_enqueue(a,get(a,0x628a88),labels[cycle],1);
    }
    if(action!=36 && action!=47)return !m->fault;
    if(action==47) {
        put(a,0x6040fc,get(a,0x603800));put(a,0x6040a4,get(a,0x603a40));put(a,0x604114,get(a,0x6040d8));
    } else {
        int cycle=3;
        for(i=3;i>0;--i) {
            uint32_t word=get(a,fields[i-1]);int32_t signed_word;memcpy(&signed_word,&word,sizeof signed_word);
            if(signed_word>0x3f733333){cycle=(int)i-1;break;}
        }
        game_memory_write_u16(m,0x603744,(uint16_t)cycle);
        if(cycle<3)for(i=0;i<3;++i)put(a,fields[i],weights[cycle][i]);
        {
            GameKeyboardServices s=game_input_keyboard_services(a);
            if(m->fault || !s.queue_float_pair(a,get(a,get(a,0x5f0208)+0x3bc)+0x13c,get(a,0x604114),get(a,0x6040a4)))return 0;
        }
    }
    return !m->fault && game_application_options_load(a,get(a,0x604490));
}
