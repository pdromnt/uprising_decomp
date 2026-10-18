#include "gameplay_chat.h"
#include "gameplay_messages.h"
#include "gameplay_catalog.h"
#include "menu_network.h"
#include "sound_playback.h"
#include "scene_projection_storage.h"
#include "menu_widgets.h"
#include "game_text_rasterizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int game_application_glyph_begin(GameApplication *,GameTextTarget *,int32_t *);
extern int game_application_glyph_end(GameApplication *);
extern int game_level_frame_004d8b50(GameApplication *,const uint32_t[4],uint32_t[4],const uint32_t[2],float);

/*Only the allocation identity lives here. The four published, rotating text
 *slots and editing buffer remain the game's authoritative records. */
typedef struct ChatOwner {GameApplication *app;GameAddress page;struct ChatOwner *next;} ChatOwner;
static ChatOwner *owners;
extern int game_menu_sound_play(GameApplication *,const SoundPlaybackRequest *);
extern int game_menu_accelerated_text_begin(GameApplication *,GameAddress,const GameUiRect *,const GameUiRect *,GameTextTarget *,int32_t *);
extern int game_menu_accelerated_text_end(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int text(GameApplication *a,GameAddress p,char *out,size_t capacity){size_t i;
    for(i=0;i<capacity;++i){out[i]=(char)game_memory_read_u8(&a->memory,p+(GameAddress)i);if(!out[i])return !a->memory.fault;}return 0;
}
static int sound(GameApplication *a,unsigned id,float distance,float pitch){
    SoundPlaybackRequest request={(int16_t)id,distance,1.0f,pitch,0,0};return game_menu_sound_play(a,&request);
}
static int message(GameApplication *a,GameAddress value){return game_message_enqueue(a,get(a,0x628a88),value,1);}
int game_input_chat_prompt(GameApplication *a){return message(a,0x594480);}
int game_level_chat_release(GameApplication *a){ChatOwner **link=&owners;GameMemory cleanup=a->memory;unsigned i;cleanup.fault=0;
    while(*link){ChatOwner *owner=*link;if(owner->app!=a){link=&owner->next;continue;}
        for(i=0;i<4;++i){GameAddress slot=0x629890+i*4,p=game_memory_read_u32(&cleanup,slot);
            if(p>=owner->page && p<owner->page+160)game_memory_write_u32(&cleanup,slot,0);}
        if(!scene_projection_storage_free(owner->page))return 0;*link=owner->next;free(owner);
    }
    game_memory_write_u32(&cleanup,0x629888,0);game_memory_write_u32(&cleanup,0x6298a0,0);return !cleanup.fault;
}
/*5390a0: four40-byte line owners, with no audio device or CPU callback. */
int game_level_scene_audio_initialize(GameApplication *a){ChatOwner *owner;GameAddress page;unsigned i;
    if(a->memory.fault)return 0;page=scene_projection_storage_allocate(204,"game text history and format scratch");if(!page)return 0;
    owner=malloc(sizeof(*owner));if(!owner){scene_projection_storage_free(page);return 0;}
    if(!game_level_chat_release(a)){scene_projection_storage_free(page);free(owner);return 0;}
    owner->app=a;owner->page=page;owner->next=owners;owners=owner;
    for(i=0;i<4;++i)put(a,0x629890+i*4,page+i*40);
    put(a,0x629888,0);put(a,0x6298a0,0);return !a->memory.fault;
}
static int line_write(GameApplication *a,GameAddress destination,const char *source){char bytes[40]={0};size_t n=0;
    /*Retail's variadic sprintf accepts uncontrolled percent sequences here.
     *Ordinary chat is literal text; preserve its39-character display limit
     *without importing that undefined vararg behavior. */
    while(n<39 && source[n]){bytes[n]=source[n];++n;}return destination && game_memory_write(&a->memory,destination,bytes,sizeof(bytes));
}
/*539120: full history rotates allocation IDs, then reuses the oldest line. */
int game_chat_line_append(GameApplication *a,const char *source){uint32_t count=get(a,0x629888);unsigned i;GameAddress first;
    if(count>4 || !source)return 0;
    if(count==4){first=get(a,0x629890);for(i=0;i<3;++i)put(a,0x629890+i*4,get(a,0x629894+i*4));put(a,0x62989c,first);count=3;}
    if(!line_write(a,get(a,0x629890+count*4),source))return 0;
    put(a,0x629888,count+1);put(a,0x6298a0,UINT32_MAX);return !a->memory.fault;
}
/*5391b0/539210/539260: update/remove the active line and publish its time. */
int game_chat_line_replace(GameApplication *a,const char *source){uint32_t count=get(a,0x629888);
    if(count>4 || !source)return 0;if(count && !line_write(a,get(a,0x62988c+count*4),source))return 0;
    if(count)put(a,0x6298a0,UINT32_MAX);return !a->memory.fault;
}
static int edit_publish(GameApplication *a){char value[128];return text(a,0x594d70,value,sizeof(value)) && game_chat_line_replace(a,value);}
static void edit_finish(GameApplication *a){put(a,0x6298a0,get(a,0x5ac6ac));put(a,0x594d6c,0);}
/*4d2b8c: stop the controlled player's movement and begin a prefixed line. */
int game_input_chat_begin(GameApplication *a){char name[33],format[64],line[128];int length;unsigned i;GameAddress root=get(a,0x5f0208),control=get(a,root+0x3bc)+0x13c;
    put(a,0x594d6c,1);put(a,control+4,get(a,control+4)&0xffff0000u);put(a,control+8,get(a,control+8)&0xffff0000u);
    for(i=0;i<5;++i)put(a,control+0x14+i*4,0);
    if(!sound(a,123,0,2.0f) || !game_input_chat_prompt(a) || !text(a,root+0x39c,name,sizeof(name)) || !text(a,0x595c28,format,sizeof(format)))return 0;
    length=snprintf(line,sizeof(line),format,name);if(length<0 || length>=128)return 0;
    if(!game_memory_write(&a->memory,0x594d70,line,(size_t)length+1))return 0;
    put(a,0x594df0,(uint32_t)length);put(a,0x594df4,(uint32_t)length);return game_chat_line_append(a,line);
}
static int equal(GameApplication *a,const char *left,GameAddress address){char right[64];unsigned i;
    if(!text(a,address,right,sizeof(right)))return 0;
    for(i=0;;++i){unsigned x=(unsigned char)left[i],y=(unsigned char)right[i];
        if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;if(x!=y)return 0;if(!x)return 1;}
}
/*43e650's text packet: byte47 plus unterminated payload, excluding self. */
static int send_line(GameApplication *a,const char *line,uint32_t to){uint8_t packet[128];size_t length=strlen(line);GameAddress root=get(a,0x5f0208);
    if(!length || length>=128 || to==get(a,root+0x3c0))return !a->memory.fault;
    packet[0]=0x47;memcpy(packet+1,line,length);
    if(!game_memory_write(&a->memory,0x5ad8c8,packet,length+1))return 0;
    /*Retail continues editing after a transport error. The native transport
     *owns diagnostics; its unresolved platform implementation remains a blocker. */
    (void)game_network_send(a,get(a,0x5f0038),to,packet,(uint32_t)length+1);return !a->memory.fault;
}
static int credit(GameApplication *a,uint32_t amount){GameAddress root=get(a,0x5f0208);uint32_t method=get(a,get(a,root)+0x34);
    if(method==0x47f250)return gameplay_catalog_credit(&a->memory,root,amount);
    if(method==0x477750)return !a->memory.fault; /*Actual base implementation returns. */
    a->memory.fault=1;a->memory.fault_address=method;return 0;
}
int game_input_chat_keyboard(GameApplication *a,const GameInputEvent *e){uint32_t length,minimum;char line[128],*command;unsigned i;GameAddress message=0;
    if(!(e->value&128))return !a->memory.fault;
    length=get(a,0x594df0);minimum=get(a,0x594df4);if(length>=128 || minimum>length)return 0;
    if(e->offset==1){uint32_t count=get(a,0x629888);if(count>4 || !sound(a,122,0,0))return 0;
        put(a,0x629888,count?count-1:0);edit_finish(a);return !a->memory.fault;}
    if(e->offset==28){
        game_memory_write_u8(&a->memory,0x594d70+length,0);if(!text(a,0x594d70,line,sizeof(line)))return 0;
        command=strchr(line,':');if(!command || !command[1])return 0;command+=2;
        if(!get(a,0x5eff04)){
            if(equal(a,command,0x595c48)){uint32_t v=get(a,0x594d3c)^1;put(a,0x594d3c,v);if(v)message=0x595c50;}
            else if(equal(a,command,0x595c5c)){uint32_t v=get(a,0x594d38)+1;int64_t s=v<=INT32_MAX?v:(int64_t)v-4294967296LL;v=(uint32_t)(s%3);put(a,0x594d38,v);if(v)message=0x595c64;}
            else if(equal(a,command,0x595c6c)){uint32_t v=get(a,0x594d40)^1;put(a,0x594d40,v);if(v)message=0x595c78;}
            else if(equal(a,command,0x595c84)){if(!get(a,0x594d38))put(a,0x594d38,1);put(a,0x594d3c,1);put(a,0x594d40,1);message=0x595c8c;}
            else if(equal(a,command,0x595c94) || equal(a,command,0x595cac)){
                int large=equal(a,command,0x595cac);if(!credit(a,large?5000:100))return 0;message=large?0x595cbc:0x595ca0;
            }else if(equal(a,command,0x595cd0))put(a,get(a,get(a,0x628a90)+0x38)+0x18,0x3727c5ac);
        }else if(get(a,0x5accd4))for(i=0;i<4;++i){GameAddress player=get(a,0x5f0210+i*4);
            if(get(a,player+0x468) && get(a,0x5f0040+i*4) && !send_line(a,line,get(a,player+0x3c0)))return 0;}
        if((message && !game_message_enqueue(a,get(a,0x628a88),message,1)) || !game_chat_line_replace(a,line))return 0;
        put(a,0x6298a0,get(a,0x5ac6ac));game_memory_write_u8(&a->memory,0x594d70,0);put(a,0x594df0,0);put(a,0x594df4,0);
        if(!sound(a,123,0,2.0f))return 0;put(a,0x594d6c,0);return !a->memory.fault;
    }
    if(e->offset==14){if(length==minimum)return !a->memory.fault;--length;put(a,0x594df0,length);game_memory_write_u8(&a->memory,0x594d70+length,0);
        return edit_publish(a) && sound(a,124,0.7f,0);}
    if(e->offset>=256)return 0;
    if(length<39){uint8_t letter=game_memory_read_u8(&a->memory,0x594c50+e->offset);if(letter!='_'){
        game_memory_write_u8(&a->memory,0x594d70+length,letter);put(a,0x594df0,++length);game_memory_write_u8(&a->memory,0x594d70+length,0);
        if(!sound(a,124,0.7f,0))return 0;}}
    return edit_publish(a);
}
/*539270: restore the matching transition strip, rasterize the real history,
 *blink the editing cursor, then rotate/expire the oldest completed line.
 *The existing bitmap and font backends borrow pixels during their own locks. */
int game_level_chat_frame(GameApplication *a){
    uint32_t mode=get(a,0x604760),count=get(a,0x629888),target=0,tick=get(a,0x5ac6ac),time=get(a,0x6298a0);unsigned i;
    GameAddress background=0,last;GameTextTarget surface={0};GameUiRect strip={0,0,200,24};
    int32_t fallback;ChatOwner *owner;char line[42];size_t length;
    if(count>4 || a->memory.fault)return 0;
    if(mode>=1 && mode<=7)target=get(a,0x53942c+(mode-1)*4);
    if(target==0x5392b0 && get(a,0x597f88))background=get(a,0x5960e8);
    else if(target==0x5392c0 && get(a,0x597f88))background=get(a,0x5960ec);
    else if(target==0x5392d0)background=get(a,0x5960f0);
    else if(target && target!=0x5392b0 && target!=0x5392c0 && target!=0x5392f5){a->memory.fault=1;a->memory.fault_address=target;return 0;}
    if(background){AssetBitmap bitmap={0};bitmap.width=bitmap.stride=640;bitmap.height=480;bitmap.pixel_bytes=307200;bitmap.top_down=1;
        bitmap.pixels=scene_projection_storage_borrow_bytes(background,bitmap.pixel_bytes);
        if(!bitmap.pixels)return 0;
        if(get(a,0x5840f0)){
            uint32_t r[4]={0,0,200,24},image[2]={640u|(480u<<16),background};
            if(!game_level_frame_004d8b50(a,r,r,image,1.0f))return 0;
        }else if(!game_menu_draw_bitmap(a,&bitmap,&strip,&strip,0,0))return 0;}
    if(!count)return !a->memory.fault;
    put(a,0x59d468,0);put(a,0x62970c,1);put(a,0x629708,1);
    game_memory_write_u8(&a->memory,0x59d448,get(a,0x594d6c)?255:0);
    game_memory_write_u8(&a->memory,0x59d44c,get(a,0x594d6c)?0:255);
    put(a,0x59d450,0);put(a,0x59d458,0);put(a,0x59d454,640);put(a,0x59d45c,480);
    if(!game_application_glyph_begin(a,&surface,&fallback))return 0;
    for(i=0;i+1<count;++i){(void)game_text_rasterize(&a->memory,get(a,0x629890+i*4),&surface,fallback);put(a,0x629708,get(a,0x629708)+6);}
    last=get(a,0x629890+(count-1)*4);
    if(time==UINT32_MAX){
        for(owner=owners;owner && owner->app!=a;owner=owner->next){}
        if(!owner || !text(a,last,line,40)){(void)game_application_glyph_end(a);return 0;}
        length=strlen(line);line[length++]=(tick&16)?'_':' ';line[length]=0;
        (void)game_text_rasterize_bytes(&a->memory,(const uint8_t *)line,length+1,&surface,fallback);
    }else (void)game_text_rasterize(&a->memory,last,&surface,fallback);
    if(!game_application_glyph_end(a))return 0;put(a,0x59d468,1);
    {uint32_t elapsed=tick-time;int64_t signed_elapsed=elapsed<=INT32_MAX?elapsed:(int64_t)elapsed-4294967296LL;
        if(time!=UINT32_MAX && signed_elapsed>=64){GameAddress first=get(a,0x629890);put(a,0x6298a0,tick);
            for(i=0;i+1<count;++i)put(a,0x629890+i*4,get(a,0x629894+i*4));put(a,0x629890+(count-1)*4,first);put(a,0x629888,count-1);}}
    return !a->memory.fault;
}
