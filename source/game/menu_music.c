#include "menu_music.h"
#include "application_config.h"
#include "scene_projection_storage.h"
#include "portable_crt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static GameAddress record(GameApplication *a){return get(a,0x59d828,0);}
static uint32_t error(GameApplication *a,GameAddress p,uint32_t code) {
    char detail[256],text[512];unsigned i;
    if(!code)return 0;
    game_music_device_error(code,detail,sizeof(detail));
    snprintf(text,sizeof(text),"%lu: %s",(unsigned long)code,detail);put(a,p,0x74,code);
    for(i=0;i<sizeof(text) && text[i];++i)game_memory_write_u8(&a->memory,p+0x78+i,(uint8_t)text[i]);
    game_memory_write_u8(&a->memory,p+0x78+i,0);return code;
}
static uint32_t status(GameApplication *a,GameAddress p,uint32_t item,uint32_t track) {
    uint32_t v=0,code;put(a,p,0x4c,item);put(a,p,0x50,track);
    code=game_music_device_status(a,get(a,p,0x18),item,track,&v);put(a,p,0x48,v);
    return code?error(a,p,code):v;
}
/* 00541ca0: exclude data/very short tracks; retain original MSF arithmetic. */
static int tracks(GameApplication *a,GameAddress p) {
    uint32_t count,i,available=0;GameAddress old,flags;
    if(!status(a,p,5,0)) {put(a,p,0,0x20c);return 0;}
    if(get(a,p,0)==0x20e)return 1;
    count=status(a,p,3,0);if(count>255)return 0;
    put(a,p,0x598,count);put(a,p,0x594,UINT32_MAX);
    flags=scene_projection_storage_allocate(count+1,"portable music track flags");if(!flags)return 0;
    game_memory_fill_u8(&a->memory,flags,count+1,0,0);
    for(i=1;i<=count;++i)if(status(a,p,0x4001,i)==0x440) {
        uint32_t length=status(a,p,1,i),seconds=(length&255)*60+((length>>8)&255);
        if((int32_t)(seconds-1)>5){game_memory_write_u8(&a->memory,flags+i,1);++available;}
    }
    old=get(a,p,0x59c);put(a,p,0x59c,flags);if(old)scene_projection_storage_free(old);
    put(a,p,0x5a4,available);put(a,p,0,available?0x20d:0x20c);
    return !a->memory.fault && available!=0;
}
/* 00540d90/00541020: one authoritative playlist allocation, original reverse
 * random selection without replacement. No host-owned competing state copy. */
static int playlist(GameApplication *a,GameAddress p,int shuffle) {
    uint32_t count=get(a,p,0x5a4),total=get(a,p,0x598),i,n=0;GameAddress list,old,flags=get(a,p,0x59c);
    uint8_t eligible[256];
    if(total>255 || count>total)return 0;
    if(!count)return 1;
    list=scene_projection_storage_allocate(count,"portable music playlist");if(!list)return 0;
    for(i=0;i<=total;++i)eligible[i]=game_memory_read_u8(&a->memory,flags+i);
    if(shuffle)for(n=count;n;--n) {
        uint32_t choice=portable_crt_rand_step(&a->memory)%n+1;
        for(i=1;i<=total;++i)if(eligible[i] && !--choice)break;
        if(i>total){scene_projection_storage_free(list);return 0;}
        game_memory_write_u8(&a->memory,list+n-1,(uint8_t)i);eligible[i]=0;
    } else for(i=1;i<=total;++i)if(eligible[i])game_memory_write_u8(&a->memory,list+n++,(uint8_t)i);
    old=get(a,p,0x5a0);put(a,p,0x5a0,list);if(old)scene_projection_storage_free(old);
    put(a,p,0x594,0);return !a->memory.fault;
}
/* 00542430: current-track notifications drive subsequent native playback. */
int game_menu_music_play(GameApplication *a,GameAddress p,int32_t selection) {
    uint32_t count,index,track,code;
    if(get(a,p,0)==0x20c && !tracks(a,p))return 0;
    count=get(a,p,0x5a4);index=get(a,p,0x594);
    if(selection>=0 && (uint32_t)selection<count)index=(uint32_t)selection;
    if(!count || index>=count || !get(a,p,0x5a0))return 0;
    put(a,p,0x594,index);put(a,p,0x590,0);
    if(!status(a,p,7,0))return 0;
    track=game_memory_read_u8(&a->memory,get(a,p,0x5a0)+index);
    code=game_music_device_play(a,get(a,p,0x18),track,track+1,index+1<count);
    if(code){error(a,p,code);return 0;}
    put(a,p,0,0x20e);return !a->memory.fault;
}
int game_menu_music_start(GameApplication *a,uint32_t selection) {
    GameAddress p=record(a);uint32_t device=0,volume=0,code,mode,track,i;
    GameConfig *configuration=game_application_config(a);
    if(p && !game_menu_shutdown_systems(a))return 0;
    p=scene_projection_storage_allocate(0x5a8,"portable music owner");if(!p)return 0;
    game_memory_fill_u8(&a->memory,p,0x5a8,0,0);put(a,0x59d828,0,p);
    put(a,p,0,0x20c);put(a,p,0x594,UINT32_MAX);
    mode=game_config_number(configuration,"music","mode",0);if(mode>2)mode=0;
    put(a,p,0xc,mode==1);put(a,p,0x10,mode==2);
    code=game_music_device_open(a,&device,&volume);put(a,p,0x18,device);put(a,p,0x1c,device!=0);put(a,p,0x20,volume);
    if(code){error(a,p,code);return 0;}
    if(!tracks(a,p))return 0;
    if(!playlist(a,p,mode==2))return 0;
    track=game_config_number(configuration,"music","track",0);
    if(mode!=2 && track)for(i=0;i<get(a,p,0x5a4);i++)if(game_memory_read_u8(&a->memory,get(a,p,0x5a0)+i)==track){selection=i;break;}
    (void)game_menu_music_play(a,p,(int32_t)selection);return !a->memory.fault;
}
/*005426c0: stop only an active/paused disc, retaining its owned track catalog.
 * The original caller ignores MCI failure; keep it as status/error data. */
int game_menu_music_stop(GameApplication *a,GameAddress p) {
    uint32_t state,code;if(!p)return !a->memory.fault;
    put(a,p,0x590,0);state=get(a,p,0);
    if(state==0x20e || state==0x211) {
        code=game_music_device_stop(a,get(a,p,0x18));put(a,p,0x74,code);
        if(code)error(a,p,code);
        put(a,p,0,0x20d);
    }
    return !a->memory.fault;
}
/* Called by the native retry worker while no audio disc is available. */
int game_menu_music_refresh(GameApplication *a) {
    GameAddress p=record(a);
    if(!p || get(a,p,0)!=0x20c || !tracks(a,p))return 0;
    if(!playlist(a,p,get(a,p,0x10)!=0))return 0;
    (void)game_menu_music_play(a,p,-1);return !a->memory.fault;
}
/* 0052c680/720/810/8b0: shared ordinary menu/game audio consumers. */
int game_menu_music_action(GameApplication *a,uint32_t method,uint32_t direction) {
    GameAddress p=record(a);uint32_t index,count;
    if(!p)return 0;
    if(method==0x52c680 || method==0x52c720 || method==0x52c810) {
        put(a,p,0xc,method==0x52c680);put(a,p,0x10,method==0x52c720);
        if(!playlist(a,p,method==0x52c720))return 0;
    } else if(method==0x52c8b0) {
        if(get(a,p,0x5a0)) {
            index=get(a,p,0x594);count=get(a,p,0x5a4);
            if(direction==1){++index;if(index==count)index=count-1;}
            else {--index;if((int32_t)index<0)index=0;}
            put(a,p,0x594,index);
        }
    } else return 0;
    {GameConfig *c=game_application_config(a);GameAddress list=get(a,p,0x5a0);uint32_t at=get(a,p,0x594);
     if(c && (!game_config_set_number(c,"music","mode",get(a,p,0x10)?2:get(a,p,0xc)?1:0) ||
        (list && at<get(a,p,0x5a4) && !game_config_set_number(c,"music","track",game_memory_read_u8(&a->memory,list+at)))))return 0;}
    (void)game_menu_music_play(a,p,-1);return !a->memory.fault;
}
int game_menu_music_title(GameApplication *a,char *text,size_t capacity) {
    GameAddress p=record(a),list;uint32_t index;
    if(!p || get(a,p,0)==0x20c){snprintf(text,capacity,"--");return !a->memory.fault;}
    index=get(a,p,0x594);list=get(a,p,0x5a0);
    if(list){if(index>=get(a,p,0x5a4))return 0;index=game_memory_read_u8(&a->memory,list+index);}
    snprintf(text,capacity,"%d",(int32_t)index);return !a->memory.fault;
}
static void update_mode(GameApplication *a,GameAddress p) {
    uint32_t mode=status(a,p,4,0);
    if(mode==0x20d && get(a,p,0x590))mode=0x211;
    if(mode!=0x211)put(a,p,0x590,0);
    put(a,p,0,mode==0x212?0x20c:mode);
}
int game_menu_music_notify(GameApplication *a,uint32_t notification,uintptr_t device) {
    GameAddress p=record(a);uint32_t mode,count,index;
    if(!p || device!=get(a,p,0x18))return 1;
    mode=get(a,p,0);
    if(mode==0x20d) {if(notification&(4|1))update_mode(a,p);return !a->memory.fault;}
    if(mode!=0x20e)return !a->memory.fault;
    if(notification&4)update_mode(a,p);
    if(notification&8) {
        put(a,p,0x590,0);mode=get(a,p,0);
        if(mode==0x20e || mode==0x211) {
            uint32_t code=game_music_device_stop(a,get(a,p,0x18));
            if(code)error(a,p,code);else put(a,p,0,0x20d);
        }
        update_mode(a,p);
    }
    if(notification&1) {
        count=get(a,p,0x5a4);index=get(a,p,0x594);
        if(get(a,p,0xc) || get(a,p,0x10)) {
            if(count-index==1 && count)index=0;
            else if(get(a,p,0x5a0)){++index;if(index==count)index=count-1;}
            put(a,p,0x594,index);
        }
        (void)game_menu_music_play(a,p,-1);
    }
    return !a->memory.fault;
}
int game_menu_shutdown_systems(GameApplication *a) {
    GameAddress p,allocation;uint32_t device;unsigned i;
    game_music_worker_join(a);p=record(a);if(!p)return 1;
    put(a,p,0x590,0);device=get(a,p,0x18);
    if(device){
        uint32_t code;
        (void)game_music_device_stop(a,device);code=game_music_device_close(a,device);
        if(code){error(a,p,code);return 0;}
        put(a,p,0x18,0);put(a,p,0x1c,0);
    }
    for(i=0;i<2;++i){allocation=get(a,p,i?0x5a0:0x59c);if(allocation && !scene_projection_storage_free(allocation))return 0;put(a,p,i?0x5a0:0x59c,0);}
    if(!scene_projection_storage_free(p))return 0;
    put(a,0x59d828,0,0);return !a->memory.fault;
}
