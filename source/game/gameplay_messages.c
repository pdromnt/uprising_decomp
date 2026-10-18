#include "gameplay_messages.h"
#include "menu_widgets.h"
#include "game_text_rasterizer.h"
#include <string.h>

extern int game_application_glyph_begin(GameApplication *,GameTextTarget *,int32_t *);
extern int game_application_glyph_end(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t signed_word(uint32_t v){return v<=INT32_MAX?(int32_t)v:(int32_t)((int64_t)v-4294967296LL);}
static uint32_t half(uint32_t v){return (v>>1)|(v&0x80000000u);}
static int text(GameApplication *a,GameAddress p,uint8_t bytes[128],size_t *length){
    size_t i;for(i=0;i<128;++i){bytes[i]=game_memory_read_u8(&a->memory,p+(GameAddress)i);
        if(a->memory.fault)return 0;
        if(!bytes[i]){*length=i;return 1;}
    }
    return 0;
}
/*52d530: three priority rings in their existing record. Full queues discard
 *new entries; the current displayed message suppresses exact duplicates. */
int game_message_enqueue_bytes(GameApplication *a,GameAddress work,const char *source,uint32_t category){
    uint8_t bytes[128],current[128],formatted[132];size_t length=0,shown;uint32_t read,write,group;
    if(!a || !work || a->memory.fault)return 0;
    if(!source)return 1;
    while(length<sizeof(bytes) && source[length]){bytes[length]=(uint8_t)source[length];++length;}
    if(length==sizeof(bytes))return 0;
    bytes[length]=0;
    if(!length)return 1;
    if(get(a,0x604760)==7){unsigned i;
        if(length>123)return 0;
        formatted[0]=11;formatted[1]='2';formatted[2]='4';formatted[3]='6';memcpy(formatted+4,bytes,length+1);
        for(i=0;i<7;++i)put(a,work+0x518+i*4,i==3?get(a,work+0x540):0);
        if(!game_memory_write(&a->memory,work,formatted,length+5))return 0;
        put(a,work+0x50c,1);put(a,work+0x500,0);return !a->memory.fault;
    }
    if(!text(a,work+0x480,current,&shown))return 0;
    if(shown==length && !memcmp(bytes,current,length+1))return 1;
    group=category==0?0:category==1?1:2;read=get(a,work+0x500+group*4);write=get(a,work+0x50c+group*4);
    if(read>2 || write>2 || a->memory.fault)return 0;
    if(write<2 && write-read!=UINT32_MAX){
        if(!game_memory_write(&a->memory,work+group*0x180+write*128,bytes,length+1))return 0;
        put(a,work+0x50c+group*4,write+1);
    }else if(read){
        if(!game_memory_write(&a->memory,work+group*0x180+write*128,bytes,length+1))return 0;
        put(a,work+0x50c+group*4,0);
    }
    return !a->memory.fault;
}
int game_message_enqueue(GameApplication *a,GameAddress work,GameAddress source,uint32_t category){
    uint8_t bytes[128];size_t length;
    if(!a || !work || a->memory.fault)return 0;
    if(!source)return 1;
    return text(a,source,bytes,&length) && game_message_enqueue_bytes(a,work,(const char *)bytes,category);
}
/*52d7e0 drains priority0 before1 before2 and advances only that ring. */
int game_message_dequeue(GameApplication *a,GameAddress work,GameAddress output){
    unsigned group;uint32_t read,write;uint8_t bytes[128];size_t length;
    if(!a || !work || !output || a->memory.fault)return 0;
    for(group=0;group<3;++group){
        read=get(a,work+0x500+group*4);write=get(a,work+0x50c+group*4);
        if(read>2 || write>2 || a->memory.fault)return 0;
        if(read==write)continue;
        if(!text(a,work+group*0x180+read*128,bytes,&length) || !game_memory_write(&a->memory,output,bytes,length+1))return 0;
        put(a,work+0x500+group*4,read==2?0:read+1);return !a->memory.fault;
    }
    game_memory_write_u8(&a->memory,output,0);return !a->memory.fault;
}
int game_input_message(GameApplication *a,GameAddress source){
    return a && game_message_enqueue(a,get(a,0x628a88),source,0);
}
/*52d940: real queue consumption, glyph width, vertical reveal/retirement,
 *horizontal scrolling and expiry. Pixels borrow the existing backend lock. */
int game_level_messages_update(GameApplication *a){
    GameAddress work;uint32_t tick,bottom,top,y,width,left,right,x;uint8_t bytes[128];size_t length;
    GameTextTarget target;int32_t fallback;int ok;
    if(!a || a->memory.fault)return 0;
    work=get(a,0x628a88);if(!work)return 0;tick=get(a,0x5acbe8);bottom=get(a,work+0x540);top=get(a,work+0x538);
    if(get(a,work+0x530)){
        y=get(a,work+0x524);
        if((signed_word(bottom)>=signed_word(y) && signed_word(top+1)<signed_word(y)) ||
           ((signed_word(bottom)<signed_word(y) || signed_word(top+1)>=signed_word(y)) && signed_word(top-5)<signed_word(y))){
            if(signed_word(tick-get(a,work+0x518))>=1){--y;put(a,work+0x524,y);put(a,work+0x518,tick);}
        }
        if(top-y==5 || top-y==UINT32_MAX)put(a,work+0x530,0);
    }else if(!get(a,work+0x51c)){
        if(!game_message_dequeue(a,work,work+0x480) || !text(a,work+0x480,bytes,&length))return 0;
        width=game_text_width_bytes(&a->memory,bytes,length);put(a,work+0x51c,width);
        if(a->memory.fault)return 0;
        if(!width)return 1;
        left=get(a,work+0x534);right=get(a,work+0x53c);x=half(right-left);
        if(signed_word(right-left)<signed_word(width))put(a,work+0x52c,1);
        else{x+=half(width);put(a,work+0x52c,0);}
        put(a,work+0x520,x);put(a,work+0x524,bottom);put(a,work+0x530,1);put(a,work+0x528,tick+33);return !a->memory.fault;
    }else{
        if(signed_word(get(a,work+0x528))<=signed_word(tick)){
            put(a,work+0x530,1);put(a,work+0x51c,0);
        }else if(get(a,work+0x52c)){
            /*The original compares measured width+35 with horizontal offset,
             *then wraps subtraction against the last scroll tick. */
            x=get(a,work+0x520);
            if(signed_word(get(a,work+0x51c)+35)>signed_word(x) && signed_word(tick-get(a,work+0x518))>=1){
                put(a,work+0x520,x+2);put(a,work+0x518,tick);put(a,work+0x528,tick+8);
            }
        }
    }
    put(a,0x59d450,get(a,work+0x534));put(a,0x59d458,top);
    put(a,0x59d454,get(a,work+0x53c));put(a,0x59d45c,bottom);
    put(a,0x62970c,get(a,work+0x53c)-get(a,work+0x520));put(a,0x629708,get(a,work+0x524));
    if(a->memory.fault)return 0;
    if(!game_application_glyph_begin(a,&target,&fallback))return 0;
    (void)game_text_rasterize(&a->memory,work+0x480,&target,fallback);ok=game_application_glyph_end(a);
    put(a,0x59d450,0);put(a,0x59d458,0);put(a,0x59d454,get(a,0x5ac6a0));put(a,0x59d45c,get(a,0x5ac6a4));
    return ok && !a->memory.fault;
}
