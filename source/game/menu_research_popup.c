#include "menu_choice_controls.h"
#include "menu_sound.h"
#include <stdio.h>
#include <string.h>
/*The preview is a distinct resource owner. Its actual stop/load methods and
 * non-default locale conversion remain native link dependencies. */
extern int game_menu_research_preview_stop(GameApplication *,GameAddress); /*44d810*/
extern int game_menu_research_preview_load(GameApplication *,GameAddress,GameAddress); /*44d740*/
extern int game_menu_locale_uppercase(GameApplication *,char *,size_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int64_t signed_value(uint32_t v){return v<0x80000000u?(int64_t)v:(int64_t)v-INT64_C(4294967296);}
static int text(GameApplication *a,GameAddress source,char *out,size_t capacity) {
    size_t i;for(i=0;i<capacity;++i){out[i]=(char)game_memory_read_u8(&a->memory,source+(uint32_t)i);if(!out[i])return !a->memory.fault;}return 0;
}
static int position(GameApplication *a,GameAddress slot,uint32_t x,uint32_t y){return game_menu_widget_position(a,get(a,slot),x,y,2,0);}
static int label(GameApplication *a,GameAddress slot,const char *value,int mode){return game_menu_text_image_set_bytes(a,get(a,slot),value,mode);}
static int append_text(GameApplication *a,GameAddress source,char *out,size_t capacity) {
    size_t used=strlen(out);return source && used<capacity && text(a,source,out+used,capacity-used);
}
/*524760/5248e0/5248f0/524ab0: the counter and its increment button publish
 * current/next profile descriptions through the same existing text owner. */
int game_menu_research_hover(GameApplication *a,uint32_t method,GameAddress row,uint32_t vehicle,GameAddress table) {
    GameAddress owner=get(a,0x58abe8),buffer,name,description,profile;
    char value[2048]="";uint32_t index;int next=method==0x5248f0;
    if(method==0x5248e0 || method==0x524ab0) {
        buffer=method==0x5248e0?0x5ac9e0:0x627ca8;
        return !owner || game_menu_text_image_set(a,owner,buffer,0);
    }
    if(method!=0x524760 && !next)return 0;
    index=get(a,get(a,row+0x78));buffer=next?0x6283a8:0x627ca8;
    if(next) {
        if(!text(a,0x598368,value,sizeof(value)))return 0;
        if(signed_value(index+1)>signed_value(get(a,row+0x60)) || index==0xffffffbb) {
            if(!append_text(a,0x5983ac,value,sizeof(value)))return 0;
            goto publish;
        }
        ++index;
    } else if(index==UINT32_MAX || index==0xffffffbb) {
        if(!text(a,0x598300,value,sizeof(value)))return 0;
        goto publish;
    }
    /*A not-owned upgrade uses index-1 only before incrementing it above. */
    if(index>=10 || !table)return 0;
    profile=table+index*(vehicle?112:176);
    name=get(a,profile+(vehicle?0x6c:0xac));description=get(a,profile+(vehicle?0x68:0xa8));
    if(!append_text(a,name?name:next?0x59837c:0x598338,value,sizeof(value)) ||
       !append_text(a,next?0x598378:0x598334,value,sizeof(value)) ||
       !append_text(a,description?description:next?0x598388:0x598344,value,sizeof(value)))return 0;
publish:
    if(!game_memory_write(&a->memory,buffer,value,strlen(value)+1))return 0;
    return !owner || game_menu_text_image_set(a,owner,buffer,0);
}
/*512120: metadata and captions come from the selected row's existing profiles.
 * Keep the original relative label positions and reuse the owned text widgets. */
static int captions(GameApplication *a) {
    GameAddress p=get(a,0x5977c0),units,vehicles,caption=0,description=0;
    char title[1024]=" ",name[1024],detail[1024],value[1024],level[32];uint32_t index;
    if(!position(a,0x5977c8,62,274) || !position(a,0x5977cc,445,274) || !position(a,0x5977bc,241,298))return 0;
    if(!p)strcpy(title,"No tech selected");
    else {
        units=get(a,p+0xb4);vehicles=get(a,p+0xb8);
        index=get(a,get(a,p+0xac));if(get(a,0x5977d8))++index;
        if(!units && !vehicles) {
            int length;if(!text(a,get(a,p+0x94),name,sizeof(name)))return 0;
            length=snprintf(value,sizeof(value),"%ld %s\nPURCHASED FOR THE NEXT MISSION",(long)signed_value(get(a,get(a,p+0xac))),name);
            if(length<0 || (size_t)length>=sizeof(value) || !game_memory_write(&a->memory,0x626690,value,(size_t)length+1))return 0;
            caption=0x626690;
            if(!position(a,0x5977c8,130,334) || !label(a,0x5977cc," ",0))return 0;
        } else if(signed_value(index)<0) {
            caption=0x597cb4;description=0x597ccc;
            if(!position(a,0x5977c8,170,334) || !position(a,0x5977bc,240,352) || !label(a,0x5977cc," ",0))return 0;
        } else if(signed_value(index)>signed_value(get(a,p+0xa8))) {
            caption=0x597ce4;
            if(!position(a,0x5977c8,170,334) || !label(a,0x5977cc," ",0))return 0;
        } else {
            GameAddress profile;unsigned i;
            if(index>=10)return 0;
            profile=(units?units:vehicles)+index*(units?176:112);
            if(!text(a,get(a,profile+(units?0xac:0x6c)),name,sizeof(name)))return 0;
            if(get(a,0x5a1eb0)) {if(!game_menu_locale_uppercase(a,name,sizeof(name)))return 0;}
            else for(i=0;name[i];++i)if(name[i]>='a' && name[i]<='z')name[i]=(char)(name[i]-'a'+'A');
            if(!game_memory_write(&a->memory,0x626690,name,strlen(name)+1))return 0;
            caption=0x626690;description=get(a,profile+(units?0xa8:0x68));
            snprintf(level,sizeof(level),"LEVEL: %lu",(unsigned long)(index+1));
            if(!label(a,0x5977cc,level,0))return 0;
        }
        if(description) {
            if(!text(a,description,detail,sizeof(detail)) || strlen(detail)+1>=sizeof(title))return 0;
            strcpy(title+1,detail);
        }
    }
    if(!label(a,0x5977bc,title,0))return 0;
    if(caption)return !a->memory.fault && game_menu_text_image_set(a,get(a,0x5977c8),caption,0);
    return position(a,0x5977c8,170,334) && label(a,0x5977c8,"UNKNOWN TECHNOLOGY",0);
}
/*5124a0: publish the same wrapped price and its signed decimal/grouped label. */
static int price(GameApplication *a) {
    GameAddress p=get(a,0x5977c0),units,vehicles;uint32_t cost=0,index;int64_t number;char value[64];
    put(a,0x5977e0,0);
    if(p) {
        units=get(a,p+0xb4);vehicles=get(a,p+0xb8);
        if(!units && !vehicles)cost=get(a,p+0xa0);
        else {
            index=get(a,get(a,p+0xac));if(get(a,0x5977d8))++index;
            if(signed_value(index)<0) {if(!label(a,0x5977b8,"N/A",1))return 0;}
            else if(signed_value(index)<=signed_value(get(a,p+0xa8))) {
                if(index>=10)return 0;
                cost=get(a,(units?units:vehicles)+index*(units?176:112)+(units?0x50:0x30));
            }
        }
        cost*=get(a,p+0xb0);put(a,0x5977e0,cost);
    }
    number=signed_value(cost);
    if(number>=1000)snprintf(value,sizeof(value),"%ld,%03ld",(long)(number/1000),(long)(number%1000));
    else snprintf(value,sizeof(value),"%ld",(long)number);
    return !a->memory.fault && label(a,0x5977b8,value,1);
}
/*512620: visibility -> captions -> price -> preview, in actual consumer order.
 * The current row is borrowed; preview and text services retain their own owners. */
int game_menu_research_popup_refresh(GameApplication *a) {
    GameAddress p,preview,name;uint32_t index;int visible;
    visible=get(a,0x5977d8)!=0 || get(a,0x58ac04)==2;
    if(!game_menu_widget_visible(a,get(a,0x5977b8),visible) || !game_menu_widget_visible(a,get(a,0x5977b0),visible))return 0;
    put(a,0x58fd3c,1);
    if(!captions(a) || !price(a))return 0;
    p=get(a,0x5977c0);
    if(p) {
        preview=get(a,0x5977c4);
        if(preview && !game_menu_research_preview_stop(a,preview))return 0;
        if(get(a,p+0xb4) || get(a,p+0xb8)) {
            int64_t offset;
            index=get(a,get(a,p+0xac));if(get(a,0x5977d8))++index;
            /*Index-1 reads the original row'sbc cache for an unowned technology.
             * Preserve that bounded legacy access instead of failing the route. */
            offset=0xc4+signed_value(index)*8;
            if(offset<0 || offset+4>0x114)return 0;
            name=get(a,p+(uint32_t)offset);
            if(name && game_memory_read_u8(&a->memory,name) && preview && !game_menu_research_preview_load(a,preview,name))return 0;
        }
    }
    put(a,0x58fd3c,1);return !a->memory.fault;
}
/*52f260: the faction's real balance label uses different thresholds from the
 * popup price (exactly1000 remains ungrouped here). */
int game_menu_faction_budget_feedback(GameApplication *a) {
    int64_t amount=signed_value(get(a,0x5e1448));char value[64];
    if(amount>=1000000)snprintf(value,sizeof(value),"%ld,%03ld,%03ld",(long)(amount/1000000),(long)(amount%1000000/1000),(long)(amount%1000));
    else if(amount>1000)snprintf(value,sizeof(value),"%ld,%03ld",(long)(amount/1000),(long)(amount%1000));
    else snprintf(value,sizeof(value),"%ld",(long)amount);
    return !a->memory.fault && label(a,0x59c6b4,value,1);
}
/*512720: purchase through the actual selected parameter, budget and checkbox
 * callbacks. Sound/checkbox consumers can mutate selection, so re-read it. */
int game_menu_research_purchase(GameApplication *a) {
    GameAddress p,field,account;uint32_t value,balance,cost;
    if(!get(a,0x5977d8) && get(a,0x58ac04)!=2)return game_menu_sound(a,146);
    p=get(a,0x5977c0);if(!p)return !a->memory.fault;
    field=get(a,p+0xac);value=get(a,field);
    if(signed_value(value)>=signed_value(get(a,p+0xa8)))return !a->memory.fault;
    account=get(a,p+0x9c);cost=get(a,0x5977e0);balance=get(a,account)-cost;
    if(balance&0x80000000u)return game_menu_sound(a,146);
    if(a->memory.fault || !game_menu_sound(a,145))return 0;
    p=get(a,0x5977c0);field=get(a,p+0xac);put(a,field,get(a,field)+1);
    cost=get(a,0x5977e0);p=get(a,0x5977c0);account=get(a,p+0x9c);put(a,account,get(a,account)-cost);
    if(!game_menu_button_input(a,get(a,0x5977ac),0x202,0,0) ||
       !game_menu_button_input(a,get(a,0x5977ac),0x201,0,0))return 0;
    return game_menu_faction_budget_feedback(a);
}
