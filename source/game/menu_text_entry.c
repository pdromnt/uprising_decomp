#include "menu_widgets.h"
#include "scene_projection_storage.h"
#include "state_helper_plans.h"
#include <string.h>
#include <ctype.h>
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_register_region(GameApplication *,GameAddress,GameAddress,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_unregister(GameApplication *,GameAddress,GameAddress,int);
extern int game_menu_controller_key(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern uint32_t game_menu_milliseconds(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int read_text(GameApplication *a,GameAddress p,char text[128]) {
    unsigned i;for(i=0;i<128;++i){text[i]=(char)game_memory_read_u8(&a->memory,p+i);if(!text[i])return !a->memory.fault;}return 0;
}
/* 00521e20: the blinking underscore belongs only to presentation. The record's
 * inline text remains authoritative, and no second writable text owner is kept. */
static int refresh(GameApplication *a,GameAddress p) {
    char text[128];size_t n;GameAddress image=get(a,p,0x74);int ok;
    if(!read_text(a,p+0x78,text))return 0;
    n=strlen(text);
    if(get(a,p,0x6c) && (!n || text[n-1]!='_')) {
        if(n+1>=sizeof(text))return 0;
        text[n++]='_';text[n]=0;
    }
    put(a,image,0x178,get(a,p,0x58));put(a,image,0x17c,get(a,p,0x5c));put(a,image,0x180,get(a,p,0x60));
    if(!game_menu_image_mode(a,image,100) || !game_menu_text_surface_fill(a,image,0))return 0;
    ok=game_menu_text_image_set_bytes(a,image,text,0);
    if(get(a,p,0x6c) && n && text[n-1]=='_')text[--n]=0;
    return ok && game_memory_write(&a->memory,p+0x78,text,n+1) && game_menu_widget_base_state(a,p,get(a,p,0x44));
}
/* 00521750: one legacy owner contains text and ten fixed-width native callback
 * descriptors; it owns the ordinary text image, never a host pointer word. */
GameAddress game_menu_text_entry_create_bytes(GameApplication *a,uint32_t id,uint32_t width,uint32_t height,uint32_t font,const char *initial) {
    GameAddress p=scene_projection_storage_allocate(0x1d4,"portable menu text entry");
    GameMenuTextStyle style={0,255,0,font,width,height,0,0,0};char text[128];unsigned i;
    if(!p)return 0;
    game_memory_fill_u8(&a->memory,p,0x1d4,0,0);state_helper_construct_packet(&a->memory,p,id);
    put(a,p,0,0x575b60);put(a,p,0x50,UINT32_MAX);put(a,p,0x54,UINT32_MAX);
    put(a,p,0x28,width);put(a,p,0x2c,height);put(a,p,0xf8,font);put(a,p,0x1cc,123);put(a,p,0x5c,255);
    for(i=0;i<10;++i){game_memory_write_u8(&a->memory,p+0x100+i*20,255);put(a,p,0x108+i*20,UINT32_MAX);put(a,p,0x10c+i*20,UINT32_MAX);put(a,p,0x110+i*20,UINT32_MAX);}
    if(!initial || strlen(initial)>=sizeof(text))goto fail;
    memcpy(text,initial,strlen(initial)+1);
    for(i=0;text[i];++i)text[i]=(char)toupper((unsigned char)text[i]);
    if(!game_memory_write(&a->memory,p+0x78,text,strlen(text)+1))goto fail;
    put(a,p,0x74,game_menu_text_image_create_bytes(a,"",&style));
    if(!get(a,p,0x74) || !refresh(a,p))goto fail;
    return p;
  fail:(void)game_menu_widget_destroy(a,p);return 0;
}
GameAddress game_menu_text_entry_create(GameApplication *a,uint32_t id,uint32_t width,uint32_t height,uint32_t font,GameAddress initial) {
    char text[128];if(!read_text(a,initial,text))return 0;
    return game_menu_text_entry_create_bytes(a,id,width,height,font,text);
}
int game_menu_text_entry_enable(GameApplication *a,GameAddress p,int enable) {
    static const uint32_t messages[]={0x202,0x201,0x20a,0x20b,0x201};
    static const uint32_t methods[]={0x5232b0,0x5232c0,0x523290,0x5232a0,0x5232c0};
    GameAddress controller=get(a,0x59d4c0,0);unsigned i;
    if(!!get(a,p,0x48)==!!enable)return !a->memory.fault;
    if(!enable){if(!game_menu_unregister(a,controller,p,0))return 0;put(a,p,0x48,0);return !a->memory.fault;}
    put(a,p,0x48,1);
    for(i=0;i<5;++i)if(!game_menu_register_region(a,controller,p,p+0x20,messages[i],methods[i],0))return 0;
    return !a->memory.fault;
}
int game_menu_text_entry_activate(GameApplication *a,GameAddress p) {
    GameAddress controller=get(a,0x59d4c0,0);
    if(!game_menu_text_entry_enable(a,p,0) || !game_menu_text_entry_enable(a,p,1) ||
       !game_menu_controller_key(a,controller,0,UINT32_MAX))return 0;
    put(a,controller,16,p);put(a,controller,20,0x521a70);put(a,controller,28,UINT32_MAX);put(a,p,0x68,0);
    return game_menu_register_tick(a,controller,p,0x521ef0,0);
}
int game_menu_text_entry_tick(GameApplication *a,GameAddress p) {
    uint32_t now=game_menu_milliseconds(a);
    if((int32_t)(get(a,p,0x68)+500u)>(int32_t)now)return !a->memory.fault;
    put(a,p,0x68,now);put(a,p,0x6c,1u-get(a,p,0x6c));return refresh(a,p);
}
int game_menu_text_entry_set(GameApplication *a,GameAddress p,GameAddress source) {
    char text[128];unsigned i;if(!read_text(a,source,text))return 0;
    for(i=0;text[i];++i)text[i]=(char)toupper((unsigned char)text[i]);
    return game_memory_write(&a->memory,p+0x78,text,strlen(text)+1) && refresh(a,p);
}
int game_menu_text_entry_copy(GameApplication *a,GameAddress p,GameAddress to,size_t capacity) {
    char text[128];size_t n;if(!read_text(a,p+0x78,text))return 0;
    n=strlen(text);if(n && text[n-1]=='_'){text[--n]=0;game_memory_write_u8(&a->memory,p+0x78+(uint32_t)n,0);}
    return n<capacity && game_memory_write(&a->memory,to,text,n+1);
}
int game_menu_text_entry_key(GameApplication *a,GameAddress p,uint32_t key,uint32_t flags) {
    char text[128];size_t n;uint32_t i,count=get(a,p,0x1c8),callback;
    if(key==16 || key==95 || (!get(a,p,0x64) && (flags&0x1000000)))return !a->memory.fault;
    if(count>9 || !read_text(a,p+0x78,text))return 0;
    for(i=0;i<count;++i)if((int32_t)(int8_t)game_memory_read_u8(&a->memory,p+0x100+i*20)==(int32_t)key) {
        callback=get(a,p,0x104+i*20);
        if(callback)return game_menu_button_callback(a,callback,p,1006,key);
        if(get(a,p,0x108+i*20)==UINT32_MAX)return !a->memory.fault;
        return game_menu_request(a,get(a,0x59d4c0,0),get(a,p,0x108+i*20),get(a,p,0x10c+i*20),get(a,p,0x110+i*20));
    }
    n=strlen(text);
    if(key==8) {
        if(n && !(n==1 && text[0]=='_')) {
            int cursor=text[n-1]=='_';text[--n]=0;if(cursor)text[n-1]='_';
        }
    } else {
        uint32_t limit=get(a,p,0x1cc);
        if(get(a,p,0x1d0) && limit==n && n) {memmove(text,text+1,n);--n;}
        if(limit>n && n<123) {
            if(n && text[n-1]=='_')text[n-1]=(char)key;
            else {text[n++]=(char)key;text[n]=0;}
            for(i=0;text[i];++i)text[i]=(char)toupper((unsigned char)text[i]);
        }
    }
    if(!game_memory_write(&a->memory,p+0x78,text,strlen(text)+1) || !refresh(a,p))return 0;
    callback=get(a,p,0x70);return !callback || game_menu_button_callback(a,callback,p,0,0);
}
int game_menu_text_entry_draw(GameApplication *a,GameAddress p,const GameUiRect *clip) {
    GameAddress image=get(a,p,0x74);
    return game_menu_widget_position(a,image,get(a,p,0x20),get(a,p,0x24),get(a,p,0x40),0) && game_menu_widget_draw(a,image,clip);
}
