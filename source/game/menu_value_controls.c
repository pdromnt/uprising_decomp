#include "menu_widgets.h"
#include "scene_projection_storage.h"
#include "state_helper_plans.h"
#include "game_text_rasterizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int game_menu_register_delivery(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_register_region(GameApplication *,GameAddress,GameAddress,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_unregister(GameApplication *,GameAddress,GameAddress,int);
extern int game_menu_image_compose(GameApplication *,GameAddress,GameAddress,int32_t,int32_t,int,int);
extern int game_menu_accelerated_text_begin(GameApplication *,GameAddress,const GameUiRect *,const GameUiRect *,GameTextTarget *,int32_t *);
extern int game_menu_accelerated_text_end(GameApplication *);

static uint32_t get(GameApplication *a,GameAddress p,unsigned o) {return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v) {game_memory_write_u32(&a->memory,p+o,v);}
static void dirty(GameApplication *a,GameAddress p) {
    unsigned o;if(!get(a,p,0x1c))for(o=0;o<16;o+=4)put(a,p,0x30+o,get(a,p,0x20+o));
    put(a,p,0x1c,1);
}
static char *string_read(GameApplication *a,GameAddress source,size_t *length) {
    char *text;size_t i;
    for(i=0;i<4096 && (uint64_t)source+i<=UINT32_MAX;++i) {
        if(!game_memory_read_u8(&a->memory,source+(uint32_t)i))break;
        if(a->memory.fault)return NULL;
    }
    if(i==4096 || a->memory.fault)return NULL;
    text=malloc(i+1);if(!text)return NULL;
    if(!game_memory_read(&a->memory,source,text,i+1)){free(text);return NULL;}
    *length=i;return text;
}
/* 0054cc90/0054d160: the value label belongs to this same legacy record. */
static int value_label(GameApplication *a,GameAddress p) {
    uint32_t value=get(a,get(a,p,0x78),0);char text[64];unsigned i;
    if(value==0xffffffbbu)strcpy(text,"DEF");
    else if(get(a,p,0)==0x5774e8u)strcpy(text,value?"ON":"OFF");
    else if(value==UINT32_MAX)strcpy(text,"OFF");
    else snprintf(text,sizeof(text),"%d",(int32_t)(value+get(a,p,0x6c)));
    for(i=0;i<=strlen(text);++i)game_memory_write_u8(&a->memory,p+0x8c+i,(uint8_t)text[i]);
    return !a->memory.fault;
}
/* 0054c320/0054d0a0. Unused trailing retail arguments are not a calling
 * convention to emulate. The value and label have explicit ordinary arguments. */
GameAddress game_menu_value_create(GameApplication *a,uint32_t id,GameAddress value,GameAddress label,
    uint32_t step,uint32_t minimum,uint32_t maximum,uint32_t format_offset,int boolean) {
    GameAddress p=scene_projection_storage_allocate(0x11c,"portable menu value control"),string;
    GameAddress background=get(a,0x59dbd8,0),images[4],child;size_t length;char *text;unsigned i;
    uint32_t width,height;
    if(!p)return 0;
    game_memory_fill_u8(&a->memory,p,0x11c,0,0);state_helper_construct_packet(&a->memory,p,id);
    put(a,p,0,boolean?0x5774e8:0x577450);put(a,p,0x50,UINT32_MAX);put(a,p,0x54,UINT32_MAX);
    put(a,p,0x78,value);put(a,p,0x7c,step);put(a,p,0x60,maximum);put(a,p,0x88,minimum);put(a,p,0x6c,format_offset);
    put(a,p,0x84,1);put(a,p,0x10c,1);put(a,p,0xf4,15);
    text=string_read(a,label,&length);if(!text)goto fail;
    string=scene_projection_storage_allocate(length+1,"portable value label");
    if(!string){free(text);goto fail;}
    put(a,p,0xd4,string);
    if(!game_memory_write(&a->memory,string,text,length+1)){free(text);goto fail;}free(text);
    for(i=0;i<2;++i) {
        GameAddress base=i?0x59dbcc:0x59dbc0;
        images[0]=get(a,base,0);images[1]=get(a,base,4);images[2]=get(a,base,8);images[3]=images[0];
        child=game_menu_button_borrowed_create(a,UINT32_MAX,images,id,1008,i?UINT32_MAX:1,UINT32_MAX,UINT32_MAX);
        if(!child)goto fail;
        put(a,p,i?0xd0:0xcc,child);put(a,child,0,0x5774a0);put(a,child,0x94,p);
    }
    if(!background)goto fail;
    width=get(a,background,0x28)-get(a,background,0x20);height=get(a,background,0x2c)-get(a,background,0x24);
    child=game_menu_image_surface_create(a,width,height,0);if(!child)goto fail;
    put(a,p,0xd8,child);put(a,child,0x10,1);
    put(a,p,0xdc,4);put(a,p,0xe0,5);put(a,p,0xe4,8);put(a,p,0xe8,44);put(a,p,0xf0,34);
    child=get(a,p,0xcc);put(a,p,0xec,width-(get(a,child,0x28)-get(a,child,0x20))-6);
    put(a,p,0x70,width);put(a,p,0x74,height);put(a,p,0x28,width);put(a,p,0x2c,height);
    put(a,p,0x38,width);put(a,p,0x3c,height);
    if(!value_label(a,p))goto fail;
    return p;
fail:(void)game_menu_widget_destroy(a,p);return 0;
}
int game_menu_value_enable(GameApplication *a,GameAddress p,int enable) {
    GameAddress controller=get(a,0x59d4c0,0);
    if(!!get(a,p,0x48)==!!enable)return !a->memory.fault;
    if(!game_menu_widget_enable(a,get(a,p,0xcc),enable) || !game_menu_widget_enable(a,get(a,p,0xd0),enable))return 0;
    if(!enable) {
        if(!game_menu_unregister(a,controller,p,0))return 0;
        put(a,p,0x48,0);return !a->memory.fault;
    }
    if(!game_menu_register_delivery(a,controller,p,1008,0x54e020))return 0;
    if(get(a,p,0xfc) && get(a,p,0x100) &&
       (!game_menu_register_region(a,controller,p,p+0x20,0x20a,0x523290,0) ||
        !game_menu_register_region(a,controller,p,p+0x20,0x20b,0x5232a0,0)))return 0;
    put(a,p,0x48,1);return !a->memory.fault;
}
/* 0054c860/0054c8e0/0054c960: paging enables controls only inside their owner. */
int game_menu_value_position(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z,int relative) {
    GameAddress owner;
    if(!game_menu_widget_base_position(a,p,x,y,z,relative))return 0;
    owner=get(a,p,0x58);if(!owner)return 0;
    if((int32_t)get(a,p,0x20)>=(int32_t)get(a,owner,0x20) &&
       (int32_t)get(a,p,0x28)<=(int32_t)get(a,owner,0x28)) {
        if(get(a,p,0x108) && !get(a,p,0x48))return game_menu_value_enable(a,p,1);
    } else {
        if(!get(a,p,0x108))put(a,p,0x108,get(a,p,0x48));
        return game_menu_value_enable(a,p,0);
    }
    return !a->memory.fault;
}
int game_menu_value_release(GameApplication *a,GameAddress p) {
    static const unsigned slots[]={0xd8,0xcc,0xd0};unsigned i;GameAddress label;
    for(i=0;i<3;++i) {
        if(!game_menu_widget_destroy(a,get(a,p,slots[i])))return 0;
        put(a,p,slots[i],0);
    }
    label=get(a,p,0xd4);if(label && !scene_projection_storage_free(label))return 0;
    put(a,p,0xd4,0);
    if(get(a,p,0x48) && !game_menu_unregister(a,get(a,0x59d4c0,0),p,0))return 0;
    put(a,p,0x48,0);return !a->memory.fault;
}
/* 0054cb10: debit the one shared budget, or leave it untouched if ineligible. */
static int budget_change(GameApplication *a,GameAddress p,uint32_t next,int *accepted) {
    uint32_t old=get(a,get(a,p,0x78),0),cost=0;GameAddress table=get(a,p,0x64),budget=get(a,p,0x80);
    int increasing=(int32_t)next>(int32_t)old;
    *accepted=1;
    if(next!=old) {
        uint32_t index=increasing?next:old;
        if(table)cost=get(a,table+index*176u,0x50);
        else if((table=get(a,p,0x68))!=0)cost=get(a,table+index*112u,0x30);
        else cost=get(a,p,0x84);
        if(!increasing)cost=0u-cost;
    }
    cost=get(a,budget,0)-cost*get(a,p,0x10c);
    if((int32_t)cost<0)*accepted=0;else put(a,budget,0,cost);
    return !a->memory.fault;
}
static int notify(GameApplication *a,GameAddress p,unsigned slot) {
    uint32_t callback=get(a,p,slot);GameAddress table=get(a,p,0x64);uint32_t kind=0;
    if(!callback)return !a->memory.fault;
    if(!table){table=get(a,p,0x68);kind=1;}
    return !table || game_menu_button_callback(a,callback,p,kind,table);
}
/* 0054cbc0 reached through 0054e020: wrapped step arithmetic, defaults,
 * bounds and budget eligibility before publishing to the original value owner. */
int game_menu_value_change(GameApplication *a,GameAddress p,uint32_t direction,uint32_t *result) {
    GameAddress value=get(a,p,0x78),owner;uint32_t old=get(a,value,0),next;int accepted;
    *result=1;
    if(old==0xffffffbbu)next=get(a,p,(int32_t)direction<0?0x60:0x88);
    else {
        next=old+direction*get(a,p,0x7c);
        if((int32_t)next<(int32_t)get(a,p,0x88) || (int32_t)next>(int32_t)get(a,p,0x60)) {
            if(!get(a,p,0x5c)){*result=0;return !a->memory.fault;}
            next=0xffffffbb;
        }
    }
    if(get(a,p,0x80)) {
        if(!budget_change(a,p,next,&accepted))return 0;
        if(!accepted)return 1;
    }
    put(a,value,0,next);if(!value_label(a,p))return 0;
    owner=get(a,p,0x58);if(owner)dirty(a,owner);
    if(!notify(a,p,0xfc))return 0;
    dirty(a,p);
    return !get(a,p,0xf8) || game_menu_button_callback(a,get(a,p,0xf8),p,UINT32_MAX,UINT32_MAX);
}
int game_menu_value_hover(GameApplication *a,GameAddress p,int inside) {
    GameAddress owner=get(a,p,0x58);
    put(a,p,0x104,!!inside);dirty(a,p);if(owner)dirty(a,owner);
    return notify(a,p,inside?0xfc:0x100);
}
/* 0054ccf0: compose once into the owned surface, present its clipped rectangle,
 * and then publish the child hit regions at their final screen coordinates. */
int game_menu_value_draw(GameApplication *a,GameAddress p,const GameUiRect *clip) {
    GameAddress surface=get(a,p,0xd8),background=get(a,p,0x110),child;GameUiRect visible,source;
    uint32_t x=get(a,p,0x20),y=get(a,p,0x24);int selected=!!get(a,p,0x104),restored;
    size_t length;char *label;unsigned i;
    restored=game_menu_text_surface_restore(a,surface);if(restored<0)return 0;
    if(selected && background)background=get(a,p,0x114);
    if(!background)background=get(a,0x59dbd8,0);
    if(!game_menu_image_compose(a,surface,background,0,0,0,selected && !get(a,p,0x110)))return 0;
    if(!get(a,p,0x110)) {
        label=string_read(a,get(a,p,0xd4),&length);if(!label)return 0;
        i=game_menu_text_surface_paint(a,surface,1,label,0x02529c94,(int32_t)get(a,p,0xdc),(int32_t)get(a,p,0xe0),1,0);
        free(label);if(!i)return 0;
    }
    if(!get(a,0x5840f8,0)) {
        label=string_read(a,p+0x8c,&length);if(!label)return 0;
        i=game_menu_text_surface_paint(a,surface,2,label,0x02529c94,(int32_t)get(a,p,0xe4),(int32_t)get(a,p,0xe8),0,0);
        free(label);if(!i)return 0;
    }
    if(!game_menu_widget_position(a,surface,x,y,10,0))return 0;
    visible.left=(int32_t)x;visible.top=(int32_t)y;visible.right=(int32_t)get(a,p,0x28);visible.bottom=(int32_t)get(a,p,0x2c);
    if(clip) {
        if(visible.left<clip->left)visible.left=clip->left;
        if(visible.top<clip->top)visible.top=clip->top;
        if(visible.right>clip->right)visible.right=clip->right;
        if(visible.bottom>clip->bottom)visible.bottom=clip->bottom;
    }
    if(visible.left<visible.right && visible.top<visible.bottom) {
        if(!get(a,p,0x110))for(i=0;i<2;++i) {
            child=get(a,p,i?0xd0:0xcc);
            if(!game_menu_widget_position(a,child,get(a,p,0xec),get(a,p,0xf0)+(i?get(a,p,0xf4):0),10,0) ||
               !game_menu_image_compose(a,surface,get(a,child,0x58),(int32_t)get(a,child,0x20),(int32_t)get(a,child,0x24),1,0))return 0;
        }
        source.left=(int32_t)((uint32_t)visible.left-x);source.top=(int32_t)((uint32_t)visible.top-y);
        source.right=(int32_t)((uint32_t)visible.right-x);source.bottom=(int32_t)((uint32_t)visible.bottom-y);
        if(!game_menu_text_surface_draw(a,surface,&source,&visible,0))return 0;
        if(get(a,0x5840f8,0) && (int32_t)(get(a,p,0x28)-x-2u)<=(visible.right-visible.left)) {
            GameTextTarget target;int32_t fallback;int ok;
            put(a,0x59d468,0,0);game_memory_write_u8(&a->memory,0x59d448,246);game_memory_write_u8(&a->memory,0x59d44c,0);
            put(a,0x62970c,0,(uint32_t)visible.left+get(a,p,0xe4)+5);
            put(a,0x629708,0,(uint32_t)visible.top+get(a,p,0xe8)+3);
            if(!game_menu_accelerated_text_begin(a,0,&visible,&visible,&target,&fallback))return 0;
            (void)game_text_rasterize(&a->memory,p+0x8c,&target,fallback);ok=game_menu_accelerated_text_end(a);
            put(a,0x59d468,0,1);if(!ok)return 0;
        }
    }
    for(i=0;i<2;++i)if(!game_menu_widget_position(a,get(a,p,i?0xd0:0xcc),x+get(a,p,0xec),y+get(a,p,0xf0)+(i?get(a,p,0xf4):0),10,0))return 0;
    return !a->memory.fault;
}
