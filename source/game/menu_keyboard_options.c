#include "menu_widgets.h"
#include <string.h>
extern int game_menu_bindings_labels(GameApplication *,unsigned,unsigned);
extern int game_menu_bindings_defaults(GameApplication *);
extern int game_menu_bindings_load(GameApplication *);
extern int game_menu_bindings_save(GameApplication *);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_input_mode(GameApplication *,unsigned);
extern int game_menu_input_begin(GameApplication *);
extern int game_menu_input_reset(GameApplication *);
extern int game_menu_register_delivery(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int read_text(GameApplication *a,GameAddress p,char *text,size_t capacity) {
    size_t i;for(i=0;i<capacity;++i){text[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!text[i])return !a->memory.fault;}
    return 0;
}
static GameAddress owned_button(GameApplication *a,GameAddress normal,GameAddress hover,uint32_t row) {
    GameAddress frames[4]={normal,hover,0,0};
    GameAddress p=game_menu_button_borrowed_create(a,UINT32_MAX,frames,0,UINT32_MAX,row,0,0);
    if(p){put(a,p,0x68,0);put(a,p,0x6c,0x54be80);put(a,p,0x50,UINT32_MAX);put(a,p,0x54,UINT32_MAX);}
    else {if(normal)(void)game_menu_widget_destroy(a,normal);if(hover)(void)game_menu_widget_destroy(a,hover);}
    return p;
}
/* 0054b5d0: two owned offscreen frames, original key-name table and colors.
 * The selector owns this row. Binding words retain their established owner. */
GameAddress game_menu_binding_row_create(GameApplication *a,unsigned row,unsigned key) {
    GameAddress frames[2]={0,0},label;char action[512],binding[128];unsigned i;
    if(row>=73)return 0;
    label=get(a,0x5944b0+row*12,0);
    if(!read_text(a,label,action,sizeof(action)) ||
       !read_text(a,key>=1 && key<221?get(a,0x5948d8+key*4,0):0x59dc14,binding,sizeof(binding)))return 0;
    for(i=0;i<2;++i) {
        uint32_t color=i?0x0200ff00u:0x02529c94u;
        frames[i]=game_menu_image_surface_create(a,420,12,0);if(!frames[i])goto fail;
        if(!game_menu_text_surface_fill(a,frames[i],0) ||
           !game_menu_text_surface_paint(a,frames[i],3,action,color,0,0,0,0) ||
           !game_menu_text_surface_paint(a,frames[i],3,binding,color,290,0,0,0))goto fail;
    }
    return owned_button(a,frames[0],frames[1],row);
fail:
    for(i=0;i<2;++i)if(frames[i])(void)game_menu_widget_destroy(a,frames[i]);
    return 0;
}
/* 0051f930: replace one intrusive item, transferring its successor and
 * selection IDs. The caller retires the returned old owner. */
int game_menu_selector_replace(GameApplication *a,GameAddress selector,GameAddress old,GameAddress replacement,GameAddress *removed) {
    GameAddress item=get(a,selector,0x5c),previous=0;unsigned visits=0;
    *removed=0;if(!replacement)return 0;
    while(item && item!=old && ++visits<65536){previous=item;item=get(a,item,0x4c);}
    if(!item || visits>=65536)return 0;
    if(!get(a,selector,0x1c)){unsigned o;for(o=0;o<16;o+=4)put(a,selector,0x30+o,get(a,selector,0x20+o));}
    put(a,selector,0x18,1);put(a,selector,0x1c,1);
    put(a,replacement,0x4c,get(a,item,0x4c));
    if(!game_menu_widget_position(a,replacement,get(a,item,0x20),get(a,item,0x24),previous?get(a,selector,0x40):0,0))return 0;
    if(previous)put(a,previous,0x4c,replacement);else put(a,selector,0x5c,replacement);
    put(a,item,0x4c,0);*removed=item;
    if(get(a,selector,0x60)==item)put(a,selector,0x60,replacement);
    if(get(a,selector,0x58)==item) {
        put(a,selector,0x58,replacement);
        if(!game_menu_button_input(a,replacement,0x20a,0,0))return 0;
    }
    return !a->memory.fault;
}
/* 0054bd30: evict an existing use of the physical key, replace its row,
 * then update the selected action and refresh the original localized slot. */
int game_menu_binding_remap(GameApplication *a,GameAddress selector,unsigned key,uint32_t *result) {
    GameAddress selected=get(a,selector,0x58),item,previous=0,replacement,removed;uint32_t old_action,action,row;unsigned i,visits=0;
    *result=0;if(!selected || key==1 || key>=256)return !a->memory.fault;
    for(i=0;i<5;++i)if(get(a,0x59dbac+i*4,0)==selected)return !a->memory.fault;
    row=get(a,selected,0x64);if(row>=73)return 0;
    action=get(a,0x5944b0+row*12,8);old_action=get(a,0x6047f0+key*4,0);
    if(old_action) {
        item=get(a,selector,0x5c);
        while(item && ++visits<65536) {
            uint32_t candidate=get(a,item,0x64);
            if(candidate<73 && get(a,0x5944b0+candidate*12,8)==old_action){previous=item;break;}
            item=get(a,item,0x4c);
        }
        if(visits>=65536)return 0;
        if(previous) {
            replacement=game_menu_binding_row_create(a,get(a,previous,0x64),0);if(!replacement)return 0;
            if(!game_menu_selector_replace(a,selector,previous,replacement,&removed)){(void)game_menu_widget_destroy(a,removed?removed:replacement);return 0;}
            if(!game_menu_widget_destroy(a,removed))return 0;
        }
    }
    selected=get(a,selector,0x58);row=get(a,selected,0x64);
    for(i=0;i<256;++i)if(get(a,0x6047f0+i*4,0)==action)put(a,0x6047f0+i*4,0,0);
    put(a,0x6047f0+key*4,0,action);
    replacement=game_menu_binding_row_create(a,row,key);if(!replacement)return 0;
    if(!game_menu_selector_replace(a,selector,selected,replacement,&removed)){(void)game_menu_widget_destroy(a,removed?removed:replacement);return 0;}
    if(!game_menu_widget_destroy(a,removed) || !game_menu_bindings_labels(a,key,1))return 0;
    *result=1;return !a->memory.fault;
}
static GameAddress header(GameApplication *a,GameAddress label,uint32_t width,uint32_t height,int green) {
    GameMenuTextStyle style={green?0:43,green?255:83,green?0:28,3,width,height,0,0,0};
    GameAddress image=game_menu_text_image_create(a,label,&style);
    return image?owned_button(a,image,0,0):0;
}
/* 0054b7d0: fixed explanatory rows followed by the enabled original action
 * table. Twelve-byte table records remain immutable data, not execution. */
int game_menu_bindings_build(GameApplication *a,GameAddress selector) {
    static const struct {GameAddress label;unsigned height,slot;int green;} headers[]={
        {0x59dd04,15,0,1},{0x59dcb4,30,2,0},{0x59dd10,15,1,1},
        {0x59dc24,70,4,0},{0x59dd20,15,3,1}};
    GameAddress item=0;unsigned i,key;
    if(!game_menu_selector_clear(a,selector))return 0;
    for(i=0;i<5;++i)put(a,0x59dbac+i*4,0,0);
    for(i=0;i<5;++i) {
        item=header(a,headers[i].label,437,headers[i].height,headers[i].green);if(!item)goto fail;
        put(a,0x59dbac+headers[i].slot*4,0,item);
        {GameAddress owned=item;item=0;if(!game_menu_selector_append(a,selector,owned,0,0))goto fail;}
    }
    for(i=0;i<73;++i)if(get(a,0x5944b0+i*12,4)) {
        if(i && i%7==0) {
            item=header(a,0x59dd38,20,13,0);if(!item)goto fail;
            {GameAddress owned=item;item=0;if(!game_menu_selector_append(a,selector,owned,0,0))goto fail;}
        }
        for(key=0;key<221 && get(a,0x6047f0+key*4,0)!=get(a,0x5944b0+i*12,8);++key){}
        item=game_menu_binding_row_create(a,i,key);if(!item)goto fail;
        {GameAddress owned=item;item=0;if(!game_menu_selector_append(a,selector,owned,0,0))goto fail;}
    }
    return !a->memory.fault;
fail:
    if(item)(void)game_menu_widget_destroy(a,item);
    (void)game_menu_selector_clear(a,selector);
    for(i=0;i<5;++i)put(a,0x59dbac+i*4,0,0);
    return 0;
}
/* Native callbacks retain the original save/restore/cancel boundary. */
int game_menu_keyboard_option_action(GameApplication *a,uint32_t method,GameAddress receiver,uint32_t argument) {
    GameAddress controller=get(a,0x59d4c0,0),settings=get(a,0x5943c8,0);
    switch(method) {
    case 0x54be80:return !a->memory.fault;
    case 0x523740:
        return game_menu_bindings_defaults(a) && game_menu_request(a,controller,200,1001,UINT32_MAX);
    case 0x523770:
        return game_menu_bindings_save(a) && game_menu_request(a,controller,69,1000,4);
    case 0x523790:
        (void)game_menu_bindings_load(a);return !a->memory.fault && game_menu_request(a,controller,69,1000,4);
    case 0x5237d0:put(a,settings,0x44,!!get(a,receiver,0x94));return !a->memory.fault;
    case 0x523800: {
        uint32_t frame;
        if(!game_menu_animation_advance(a,get(a,get(a,0x59d4c4,0),0xe4),&frame))return 0;
        put(a,settings,0x50,frame);return !a->memory.fault;
    }
    case 0x523830:put(a,settings,0x48,get(a,argument,0));return !a->memory.fault;
    case 0x523850:put(a,settings,0x4c,get(a,argument,0));return !a->memory.fault;
    default:return 0;
    }
}
/* 004929e0 state5: construct/configure/use/retire the keyboard options page. */
int game_menu_keyboard_state(GameApplication *a,GameAddress e,uint32_t phase) {
    static const GameAddress names[3][4]={{0x58cf00,0x58cee0,0x58cec0,0},
        {0x58cf68,0x58cf44,0x58cf20,0},{0x58cfd4,0x58cfb0,0x58cf8c,0}};
    static const uint32_t callbacks[]={0x523770,0x523740,0x523790};GameAddress p;unsigned i;uint32_t bits;float value;
    if(!phase) {
        p=game_menu_image_create(a,100,0x58ce9c);if(!p)goto fail;put(a,0x58b0bc,0,p);
        if(!game_menu_widget_position(a,p,0,0,0,0) || !game_menu_widget_visible(a,p,1))goto fail;
        p=game_menu_selector_create(a,200,5,0x54b7d0,0x59dbf0);if(!p)goto fail;put(a,p,0,0x577400);put(a,0x58b0c0,0,p);
        if(!game_menu_selector_resize(a,p,437,256) || !game_menu_widget_position(a,p,102,124,10,0) ||
           !game_menu_widget_visible(a,p,1) || !game_menu_widget_enable(a,p,1))goto fail;
        for(i=0;i<3;++i) {
            p=game_menu_button_create(a,201+i,names[i],0,UINT32_MAX,0,callbacks[i]);if(!p)goto fail;
            put(a,0x58b0c4+i*4,0,p);put(a,p,0x50,144);put(a,p,0x54,i==2?146:145);
            if(!game_menu_widget_position(a,p,i==0?94:i==1?264:456,393,10,0) ||
               !game_menu_widget_visible(a,p,1) || !game_menu_widget_enable(a,p,1))goto fail;
        }
        {GameAddress images[4]={0x58d040,0x58d01c,0x58cff8,0};
            p=game_menu_checkbox_create(a,images,0x5237d0);}
        if(!p)goto fail;
        put(a,0x58b0d0,0,p);put(a,p,0x50,UINT32_MAX);put(a,p,0x54,145);
        if(!game_menu_widget_position(a,p,237,84,10,0) || !game_menu_widget_visible(a,p,1) || !game_menu_widget_enable(a,p,1))goto fail;
        for(i=0;i<4;++i){static const unsigned frames[]={0x58,0x5c,0x60,0x8c};GameAddress f=get(a,p,frames[i]);if(f && !game_menu_image_mode(a,f,101))goto fail;}
        if(get(a,get(a,0x5943c8,0),0x44)==1){put(a,p,0x44,2);put(a,p,0x94,1);put(a,p,0x1c,1);}
        for(i=0;i<2;++i) {
            bits=get(a,get(a,0x5943c8,0),i?0x4c:0x48);memcpy(&value,&bits,4);
            p=game_menu_slider_create(a,UINT32_MAX,i?0x58d084:0x58d064,100,13,i?0x523850:0x523830,6,value,1,0,0);
            if(!p)goto fail;
            put(a,0x58b0d4+i*4,0,p);
            if(!game_menu_widget_position(a,p,365,i?94:80,10,0) || !game_menu_widget_visible(a,p,1) || !game_menu_widget_enable(a,p,1))goto fail;
        }
        if(!game_menu_widget_position(a,get(a,e,0x60),544,383,12,0) || !game_menu_widget_visible(a,get(a,e,0x60),1) ||
           !game_menu_bindings_build(a,get(a,0x58b0c0,0)) || !game_menu_widget_enable(a,get(a,e,0xe0),1) ||
           !game_menu_widget_visible(a,get(a,e,0xe4),1))goto fail;
        p=get(a,e,0xe4);bits=get(a,get(a,0x5943c8,0),0x50);
        if(bits>=get(a,p,0x2fc))goto fail;
        put(a,p,0x300,bits);put(a,p,0xb4,get(a,p,0x16c+bits*4));put(a,p,0x1c,1);put(a,p,0x18,1);
        if(!game_menu_input_begin(a) || !game_menu_input_mode(a,8))goto fail;
        return !a->memory.fault;
    }
    if(phase!=1)return !a->memory.fault;
    if(!game_menu_slider_cancel(a) || !game_menu_input_mode(a,0) || !game_menu_input_reset(a) ||
       !game_menu_widget_visible(a,get(a,e,0x60),0) || !game_menu_widget_enable(a,get(a,e,0xe0),0) ||
       !game_menu_widget_visible(a,get(a,e,0xe4),0))return 0;
    for(i=0;i<8;++i){GameAddress slot=0x58b0bc+i*4;p=get(a,slot,0);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,slot,0,0);}
    return !a->memory.fault;
fail:(void)game_menu_keyboard_state(a,e,1);return 0;
}
