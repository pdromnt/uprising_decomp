#include "level_run.h"
#include "level_world_surface.h"
#include "menu_network.h"
#include "game_text_rasterizer.h"
#include <stdio.h>
#include <string.h>

extern uint32_t game_menu_milliseconds(GameApplication *);
extern uint16_t game_application_key_state(GameApplication *,unsigned,int);
extern int game_application_render_mode(GameApplication *,int);
extern int game_application_frame_present(GameApplication *);
extern int game_application_palette_lines(GameApplication *,const int32_t[][4],unsigned,uint8_t);
extern int game_level_palette_polygon(GameApplication *,const float[][3],const uint8_t *,unsigned,uint32_t);
extern int game_level_frame_text_bytes(GameApplication *,int32_t,int32_t,const uint8_t *,size_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t signed_word(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static int32_t shift(uint32_t v,unsigned n){return signed_word((v>>n)|((v&0x80000000u)?(~0u<<(32-n)):0));}
static void wire_word(uint8_t *p,uint32_t v){unsigned i;for(i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint32_t read_word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int cancelled(GameApplication *a){return (game_application_key_state(a,0x1b,1)&0x8000)!=0;}
/*548fb0: literal/two-integer captions only, matching every synchronization
 *caller. Text and wire bytes stay local; existing frame/backend owners draw. */
static int caption(GameApplication *a,GameAddress format,int first,int second){
    uint8_t text[256];size_t length=0,at,start,end;unsigned conversions=0,rows=0;uint32_t width=0,x,y,left,right,top,bottom;
    for(at=0;at<256;at++){
        uint8_t c=game_memory_read_u8(&a->memory,format+(uint32_t)at);
        if(!c)break;
        if(c=='%'){
            char number[16];int n;
            if(game_memory_read_u8(&a->memory,format+(uint32_t)++at)!='d' || conversions>=2)return 0;
            n=snprintf(number,sizeof(number),"%d",conversions++?second:first);
            if(n<0 || length+(size_t)n>=sizeof(text))return 0;
            memcpy(text+length,number,(size_t)n);length+=(size_t)n;
        }else{if(length+1>=sizeof(text))return 0;text[length++]=c;}
    }
    if(at==256 || a->memory.fault)return 0;
    text[length]=0;
    if(!game_memory_write(&a->memory,0x639970,text,length+1))return 0;
    for(start=0;start<length;start=end){
        if(text[start]=='\n'){end=start+1;continue;}
        for(end=start;end<length && text[end]!='\n';end++){}
        {uint32_t measured=game_text_width_bytes(&a->memory,text+start,end-start)+1;if(measured>width)width=measured;}rows++;
    }
    x=(uint32_t)shift(get(a,0x5ac6a0),1);y=(uint32_t)shift(get(a,0x5ac6a4),2);
    left=x-(uint32_t)shift(width,1);right=x+(uint32_t)shift(width+1,1);top=y-rows*3;bottom=y+rows*3;
    put(a,0x639958,left);put(a,0x639960,right);put(a,0x63995c,top);put(a,0x639964,bottom);
    if(!game_level_texture_frame(a) || !game_application_render_mode(a,1) ||
       (get(a,0x597f60) && !game_level_world_surface_draw(a)))return 0;
    if(get(a,0x5840f0)){
        int32_t dark[2][4]={{signed_word(left-1),signed_word(top-1),signed_word(left-1),signed_word(bottom)},
            {signed_word(left-1),signed_word(bottom+1),signed_word(right),signed_word(bottom+1)}};
        int32_t light[2][4]={{signed_word(left),signed_word(top-1),signed_word(right+1),signed_word(top-1)},
            {signed_word(right+1),signed_word(top),signed_word(right+1),signed_word(bottom+1)}};
        uint8_t shade=game_memory_read_u8(&a->memory,0x639830),colors[4]={shade,shade,shade,shade};float p[4][3],offset;uint32_t bits=get(a,0x574d1c);unsigned i;
        memcpy(&offset,&bits,4);
        p[0][0]=p[3][0]=(float)signed_word(left);p[1][0]=p[2][0]=(float)((long double)signed_word(right)+offset);
        p[0][1]=p[1][1]=(float)signed_word(top);p[2][1]=p[3][1]=(float)((long double)signed_word(bottom)+offset);
        for(i=0;i<4;i++)p[i][2]=.25f;
        if(!game_application_palette_lines(a,(const int32_t (*)[4])dark,2,5) || !game_application_palette_lines(a,(const int32_t (*)[4])light,2,13) ||
           !game_level_palette_polygon(a,(const float (*)[3])p,colors,4,0x800))return 0;
    }
    put(a,0x59d46c,0);rows=0;
    for(start=0;start<length;start=end){
        uint8_t separator;int drawn;
        if(text[start]=='\n'){end=start+1;continue;}
        for(end=start;end<length && text[end]!='\n';end++){}
        /*The rasterizer consumes a bounded NUL-terminated line, as548fb0's
         *strtok output did. Keep the separator for the next local iteration. */
        separator=text[end];text[end]=0;
        drawn=game_level_frame_text_bytes(a,signed_word(left+1),signed_word(top+rows*6+1),text+start,end-start+1);
        text[end]=separator;
        if(!drawn){
            put(a,0x59d46c,1);return 0;
        }rows++;
    }
    put(a,0x59d46c,1);return !a->memory.fault && game_application_frame_present(a);
}
static int receive(GameApplication *a,uint32_t *sender,uint8_t data[256],uint32_t *size){
    uint32_t to=0;*size=256;*sender=0;
    if(game_network_receive_result(a,sender,&to,data,size)!=0)return 0;
    if(*size>256){a->memory.fault=1;return 0;}
    return game_memory_write(&a->memory,0x5f0050,data,*size);
}
/*549560: count remote ready clients, skipping inactive lanes. */
static int ready_clients(GameApplication *a){
    unsigned i,total=0,reported=0;int ready[4]={1,0,0,0};uint8_t data[256];uint32_t sender,size;
    for(i=1;i<4;i++){ready[i]=!get(a,get(a,0x5f0210+i*4)+0x468);if(!ready[i])total++;}
    if(!caption(a,0x59da4c,0,(int)total))return 0;
    (void)game_menu_milliseconds(a);(void)game_menu_milliseconds(a);
    while(!a->memory.fault){
        unsigned count=0;
        if(cancelled(a))return 0;
        if(receive(a,&sender,data,&size) && sender && size>=4 && data[0]==13){
            uint16_t lane=(uint16_t)((uint16_t)data[2]|(uint16_t)data[3]<<8);
            if(lane>=4)return 0;
            ready[lane]=1;
            for(i=0;i<4;i++)if(ready[i])count++;
            if(count-1!=reported){reported=count-1;if(!caption(a,0x59da98,(int)reported,(int)total))return 0;}
        }
        if(ready[0] && ready[1] && ready[2] && ready[3])return 1;
    }
    return 0;
}
static int host_sync(GameApplication *a){
    uint32_t sent[6]={0},last=0,sender,size,start,elapsed;unsigned probe=0,i;int ready[4];uint8_t data[256],message[8]={14,0,0,0,0,0,0,0};
    if(!ready_clients(a))return 0;
    start=game_menu_milliseconds(a);put(a,0x5ac90c,start);
    for(i=0;i<4;i++){GameAddress root=get(a,0x5f0210+i*4);ready[i]=!get(a,root+0x468) || root==get(a,0x5f0208);}
    put(a,0x639968,game_menu_milliseconds(a)-start+18*get(a,0x5efee8)+2000);
    if(!caption(a,0x59d990,0,0))return 0;
    while(probe<6 && !a->memory.fault){
        if(cancelled(a))return 0;
        elapsed=game_menu_milliseconds(a)-start;
        if(elapsed>last+3*get(a,0x5efee8)){
            uint32_t result;
            message[1]=(uint8_t)probe;wire_word(message+4,get(a,0x639968));
            result=game_network_send_result(a,get(a,get(a,0x5f0208)+0x3c0),0,message,8);
            if(result){char diagnostic[128];snprintf(diagnostic,sizeof(diagnostic),"native clock probe send failed: probe=%u result=%08x",probe,(unsigned)result);game_application_log(a,"%s\n",diagnostic);}
            last=game_menu_milliseconds(a)-start;sent[probe++]=last;
        }
        if(receive(a,&sender,data,&size) && sender && size>=8 && data[0]==14){
            unsigned lane=4;uint32_t midpoint;if(data[1]>=probe)return 0;
            midpoint=(sent[data[1]]+game_menu_milliseconds(a)-start)>>1;data[0]=15;wire_word(data+4,read_word(data+4)-midpoint);
            if(!game_memory_write(&a->memory,0x5f0050,data,8))return 0;
            (void)game_network_send_result(a,get(a,get(a,0x5f0208)+0x3c0),sender,data,8);
            for(i=0;i<4;i++)if(get(a,get(a,0x5f0210+i*4)+0x3c0)==sender){lane=i;break;}
            if(lane>=4)return 0;
            ready[lane]=1;
        }
        if(probe>3 && ready[0] && ready[1] && ready[2] && ready[3])return 1;
    }
    return 0;
}
static int client_sync(GameApplication *a){
    uint32_t began=game_menu_milliseconds(a),last=0,sender,size;int waiting=1;uint8_t data[256],ready[4]={13,0,0,0};
    if(!caption(a,0x59d9d4,0,0))return 0;
    while(!a->memory.fault){
        if(cancelled(a))return 0;
        if(waiting && (!last || game_menu_milliseconds(a)-began>last+500)){
            uint16_t lane=game_memory_read_u16(&a->memory,get(a,0x5f0208)+8);
            last=game_menu_milliseconds(a)-began;ready[2]=(uint8_t)lane;ready[3]=(uint8_t)(lane>>8);
            if(!game_memory_write(&a->memory,0x5f0050,ready,4))return 0;
            (void)game_network_send_result(a,get(a,0x5f0038),get(a,get(a,0x5eff28)+0x3c0),ready,4);
        }
        if(!receive(a,&sender,data,&size) || !sender || !size)continue;
        if(data[0]==14){
            if(size<8)return 0;
            waiting=0;(void)game_menu_milliseconds(a);put(a,0x639968,read_word(data+4));
            wire_word(data+4,game_menu_milliseconds(a));if(!game_memory_write(&a->memory,0x5f0050,data,8))return 0;
            (void)game_network_send_result(a,get(a,get(a,0x5f0208)+0x3c0),sender,data,8);
            if(!caption(a,0x59da10,0,0))return 0;
        }else if(data[0]==15){if(size<8)return 0;put(a,0x5ac90c,read_word(data+4));return !a->memory.fault;}
    }
    return 0;
}
/*5491e0/549400: real host/client entry, bounded portable wire records. The SDK
 *owns transport; no COM pointers or guest-call stack enter game execution. */
int game_level_network_synchronize(GameApplication *a,int host){
    char diagnostic[192];
    if(!a || a->memory.fault)return 0;
    snprintf(diagnostic,sizeof(diagnostic),"native clock synchronization: host=%d local=%08x player=%u transport=%u latency=%u",host,
        (unsigned)get(a,0x5f0208),(unsigned)get(a,get(a,0x5f0208)+0x3c0),(unsigned)get(a,0x5f0038),(unsigned)get(a,0x5efee8));
    game_application_log(a,"%s\n",diagnostic);
    return host?host_sync(a):client_sync(a);
}
