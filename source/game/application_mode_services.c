#include "application_entry.h"
#include "application_mode.h"
#include "ui_frame.h"
#include "game_text_rasterizer.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_screen_mask(GameApplication *,int);
extern int game_menu_present_frame(GameApplication *,uint32_t);
extern int game_menu_palette(GameApplication *,GameAddress);
extern int game_menu_fullscreen_image(GameApplication *,GameAddress,uint32_t);
extern int game_level_default_viewport(GameApplication *);
static int mask(void *a,int enabled){return game_menu_screen_mask(a,enabled);}
static int present(void *a,uint32_t interval){return game_menu_present_frame(a,interval);}
static int palette(void *a,GameAddress address){return game_menu_palette(a,address);}
static int fullscreen(void *a,GameAddress address,uint32_t stride){return game_menu_fullscreen_image(a,address,stride);}
static uint32_t get(GameMemory *m,GameAddress address){return game_memory_read_u32(m,address);}
/*00548ab0: these mode callers use literal messages (the extra asset vararg is
 * unused by every actual format). Keep the existing message record authoritative;
 * its later gameplay presentation/expiry consumer remains separate work. */
int game_application_timed_message_bytes(GameApplication *a,const uint8_t *text,size_t length,int32_t duration) {
    GameMemory *m=&a->memory;size_t start;uint32_t width=0,lines=0,x,y;
    if(length>=256 || m->fault)return 0;
    if(!game_memory_write(m,0x639838,text,length+1))return 0;
    game_memory_write_u32(m,0x639938,(game_menu_milliseconds(a)-get(m,0x5ac90c))>>5);
    /*Original signed arithmetic: duration is seconds, multiplied by1000,
     *then divided by32 with downward rounding and added to the current tick. */
    {uint32_t product=(uint32_t)duration*1000u;
     uint32_t ticks=(product>>5)|((product&0x80000000u)?0xf8000000u:0);
     game_memory_write_u32(m,0x639950,(int32_t)ticks<0?UINT32_MAX:get(m,0x639938)+ticks);}
    game_memory_write_u32(m,0x63993c,1);
    /*strtok skips empty lines; the original copied scratch buffer has a final
     * forced NUL. Width includes one extra pixel before centered bounds. */
    for(start=0;start<length;) {
        size_t end;uint32_t measured;
        if(text[start]=='\n'){++start;continue;}
        for(end=start;end<length && text[end]!='\n';++end){}
        measured=game_text_width_bytes(m,text+start,end-start)+1;
        if(measured>width)width=measured;
        ++lines;start=end;
    }
    x=(uint32_t)((int32_t)get(m,0x5ac6a0)>>1);y=(uint32_t)((int32_t)get(m,0x5ac6a4)>>2);
    game_memory_write_u32(m,0x639958,x-(width>>1));game_memory_write_u32(m,0x639960,x+((width+1)>>1));
    game_memory_write_u32(m,0x63995c,y-lines*3);game_memory_write_u32(m,0x639964,y+lines*3);
    return !m->fault;
}
int game_application_timed_message(GameApplication *a,GameAddress label,int32_t duration) {
    uint8_t text[256];size_t length;
    for(length=0;length<sizeof(text);++length){text[length]=game_memory_read_u8(&a->memory,label+(uint32_t)length);if(!text[length])break;}
    if(length==sizeof(text) || a->memory.fault)return 0;
    return game_application_timed_message_bytes(a,text,length,duration);
}
/*The actual HUD-cycle callers pass one integer to548ab0's format. Build
 * bounded local bytes and publish through the same authoritative HUD record. */
int game_application_timed_integer_message(GameApplication *a,GameAddress label,int32_t duration,int32_t value){
    uint8_t text[256];char number[16];size_t length=0,input=0;int converted=0,n;
    n=snprintf(number,sizeof number,"%ld",(long)value);
    if(n<0 || (size_t)n>=sizeof number)return 0;
    while(input<256){
        uint8_t ch=game_memory_read_u8(&a->memory,label+(uint32_t)input++);
        if(a->memory.fault)return 0;
        if(!ch){text[length]=0;return game_application_timed_message_bytes(a,text,length,duration);}
        if(ch=='%'){
            ch=game_memory_read_u8(&a->memory,label+(uint32_t)input++);
            if(ch=='d' && !converted){
                if(length+(size_t)n>=sizeof text)return 0;
                memcpy(text+length,number,(size_t)n);length+=(size_t)n;converted=1;continue;
            }
            if(ch!='%')return 0;
        }
        if(length+1>=sizeof text)return 0;
        text[length++]=ch;
    }
    return 0;
}
static int mode_message(GameApplication *a,GameAddress label){return game_application_timed_message(a,label,-1);}
/*0051a070/004af770: scene offset is a float value; conversion preserves the
 * original truncate-to-int64 then low-word behavior without an x87 emulator. */
static int scene_offset(GameMemory *m,float value) {
    uint32_t bits,offset;
    memcpy(&bits,&value,4);
    if(!isfinite(value) || (long double)value<-9223372036854775808.0L || (long double)value>=9223372036854775808.0L)offset=0;
    else offset=(uint32_t)(uint64_t)(int64_t)truncl(value);
    game_memory_write_u32(m,0x626e4c,bits);game_memory_write_u32(m,0x597f5c,get(m,0x62792c)-offset);
    return !m->fault;
}
/* 0052d3f0: the existing0x544-byte work owner holds nine message slots and
 * the viewport. Mode changes reset that owner directly; no secondary mirror. */
int game_application_mode_notify(GameApplication *app,GameAddress work) {
    GameMemory *m=&app->memory;uint32_t mode=game_memory_read_u32(m,0x604760u),bounds[4];unsigned i;
    if(!work)return 0;
    if(mode==7u) {
        static const GameAddress source[]={0x6025c8u,0x6025d4u,0x6025d0u,0x6025ccu};
        for(i=0;i<4;++i)bounds[i]=game_memory_read_u32(m,source[i]);
    } else if(mode==2u) {bounds[0]=103;bounds[1]=11;bounds[2]=216;bounds[3]=17;}
    else if(!game_memory_read_u32(m,0x597f80u)) {bounds[0]=120;bounds[1]=150;bounds[2]=208;bounds[3]=157;}
    else {bounds[0]=480;bounds[1]=348;bounds[2]=618;bounds[3]=356;}
    for(i=0;i<4;++i)game_memory_write_u32(m,work+0x534u+i*4u,bounds[i]);
    for(i=0;i<7;++i)game_memory_write_u32(m,work+0x518u+i*4u,i==3?bounds[3]:0);
    for(i=0;i<9;++i)game_memory_write_u8(m,work+i*0x80u,0);
    for(i=0;i<6;++i)game_memory_write_u32(m,work+0x500u+i*4u,0);
    return !m->fault;
}

static GameAddress field_address(GameMemory *m, ApplicationModeField field) {
    static const GameAddress addresses[] = {
        0x604760u, 0u, 0u, 0x60464cu, 0x5840f0u, 0x597f88u,
        0x584048u, 0x589abcu, 0x628a88u, 0x604114u, 0x6040a4u,
        0x6040fcu, 0x603800u, 0x603a40u, 0x6040d8u
    };
    if ((unsigned)field >= sizeof(addresses)/sizeof(addresses[0])) return 0;
    if (field == APPLICATION_MODE_ROOT_PRESENTATION || field == APPLICATION_MODE_ROOT_FLAG)
        return game_memory_read_u32(m, 0x5f0208u) +
            (field == APPLICATION_MODE_ROOT_PRESENTATION ? 0x588u : 0x294u);
    return addresses[field];
}
static int read_field(void *context, ApplicationModeField field, uint32_t *value) {
    GameMemory *m = &((GameApplication *)context)->memory;
    GameAddress address = field_address(m, field);
    if (!address) return 0;
    *value = game_memory_read_u32(m, address);
    return !m->fault;
}
static int write_field(void *context, ApplicationModeField field, uint32_t value) {
    GameMemory *m = &((GameApplication *)context)->memory;
    GameAddress address = field_address(m, field);
    if (!address) return 0;
    game_memory_write_u32(m, address, value);
    return !m->fault;
}
static int perform(void *context, ApplicationModeAction action) {
    GameApplication *app = context;
    GameMemory *m = &app->memory;
    if (action <= APPLICATION_MODE_LOAD_ALTERNATE) {
        GameAddress label;
        if (action == APPLICATION_MODE_LOAD_FALLBACK)
            return mode_message(app,0x595e5cu);
        if (action == APPLICATION_MODE_LOAD_ROOT) {
            label = 0x595e38u;
        } else {
            uint32_t selector = game_memory_read_u32(m, 0x60464cu);
            if (selector > (UINT32_MAX - 0x594d50u)/4u) return 0;
            label = action == APPLICATION_MODE_LOAD_SELECTOR ? 0x595e50u : 0x595e44u;
        }
        return !m->fault && mode_message(app,label);
    }
    switch (action) {
    case APPLICATION_MODE_CLOSE_THREE: game_memory_write_u32(m,0x63993c,0);return !m->fault;
    case APPLICATION_MODE_START_SCENE: return scene_offset(m,0.0f);
    case APPLICATION_MODE_START_DEFAULT: return game_level_default_viewport(app);
    case APPLICATION_MODE_RESOURCE_EMPTY:
    case APPLICATION_MODE_RESOURCE_GAME:
    case APPLICATION_MODE_RESOURCE_ONE:
    case APPLICATION_MODE_RESOURCE_TWO:
    case APPLICATION_MODE_RESOURCE_SEVEN: {
        GameUiFrameOps ops={app,mask,present,palette,fullscreen};
        GameUiFrameKind kind=action==APPLICATION_MODE_RESOURCE_ONE?GAME_UI_FRAME_PALETTE_TRANSITION:
            action==APPLICATION_MODE_RESOURCE_TWO?GAME_UI_FRAME_WORK_IMAGE:
            action==APPLICATION_MODE_RESOURCE_SEVEN?GAME_UI_FRAME_MARKER_IMAGE:GAME_UI_FRAME_CLEAR;
        return game_ui_frame_run(m,kind,&ops);
    }
    case APPLICATION_MODE_STOP_SOUND:
        game_memory_write_u32(m,0x58404c,1);game_memory_write_u32(m,0x584048,0);
        game_memory_write_u32(m,0x5ace78,get(m,0x5ac6ac));return !m->fault;
    case APPLICATION_MODE_STOP_INPUT:
        game_memory_write_u32(m,0x589ac0,1);game_memory_write_u32(m,0x589abc,0);
        game_memory_write_u32(m,0x5f0228,get(m,0x5ac6ac));return !m->fault;
    case APPLICATION_MODE_NOTIFY:
        return game_application_mode_notify(app, game_memory_read_u32(m, 0x628a88u));
    default: return 0;
    }
}

/* 004cf300 uses the existing native mode owner directly. */
int game_application_options_load(GameApplication *app, uint32_t requested) {
    ApplicationModeServices services = {app, read_field, write_field, perform};
    return application_mode_change(requested, &services);
}
/*4b2d50: selector and gameplay keyboard share the existing presentation owner. */
int game_application_capture_presentation(GameApplication *app) {
    ApplicationModeServices services={app,read_field,write_field,perform};
    return application_mode_capture_presentation(&services);
}
