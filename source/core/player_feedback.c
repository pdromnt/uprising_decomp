#include "player_feedback.h"
#include <string.h>
#include <math.h>
#include <limits.h>
#include "portable_crt.h"

static int32_t feedback_signed(uint32_t bits){int32_t v;memcpy(&v,&bits,4);return v;}
static uint32_t feedback_truncate(long double v){return (uint32_t)(uint64_t)(!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v);}
static void feedback_store(GameMemory *m,GameAddress p,float v){uint32_t bits;memcpy(&bits,&v,4);game_memory_write_u32(m,p,bits);}

static float feedback_float(GameMemory *memory,GameAddress address){
    uint32_t bits=game_memory_read_u32(memory,address);float value;memcpy(&value,&bits,4u);return value;
}
/*4a39b0/4a3a20 share the existing camera record and force-feedback request.
 *An active impulse cannot be restarted. Expiry clears the actual device request.*/
int game_camera_hit_begin(GameMemory *m,GameAddress camera,const float impulse[3]){
    unsigned i;float squared;uint32_t gain;
    if(!m || m->fault || !impulse)return 0;
    if(game_memory_read_u32(m,camera+0x24)!=UINT32_MAX)return !m->fault;
    game_memory_write_u32(m,camera+0x24,game_memory_read_u32(m,0x5ac6ac));
    for(i=0;i<3;i++)feedback_store(m,camera+24+i*4,impulse[i]);
    if(game_memory_read_u32(m,0x624f68)){
        squared=(float)(((long double)impulse[0]*impulse[0]+(long double)impulse[1]*impulse[1])+(long double)impulse[2]*impulse[2]);
        gain=feedback_truncate(sqrtl(squared)*feedback_float(m,0x574358));
        game_memory_write_u32(m,0x60821c,0);game_memory_write_u32(m,0x608248,1);game_memory_write_u32(m,0x6081ac,gain);
    }
    return !m->fault;
}
int game_camera_hit_sample(GameMemory *m,GameAddress camera,float out[3],int *active){
    uint32_t start,tick;int32_t age;float envelope;long double multiplier;unsigned i;uint32_t phase;
    if(!m || m->fault || !out || !active)return 0;
    *active=0;start=game_memory_read_u32(m,camera+0x24);if(start==UINT32_MAX)return !m->fault;
    tick=game_memory_read_u32(m,0x5ac6ac);age=feedback_signed(tick-start);
    if(age>=16){game_memory_write_u32(m,camera+0x24,UINT32_MAX);memset(out,0,12);
        if(game_memory_read_u32(m,0x624f68))game_memory_write_u32(m,0x608248,0);
        return !m->fault;
    }
    envelope=(float)(age<8?(long double)age*feedback_float(m,0x57435c):
        (long double)(8-(int64_t)age)*feedback_float(m,0x57435c)+feedback_float(m,0x574360));
    phase=feedback_truncate((long double)envelope*feedback_float(m,0x574364))&2047;
    multiplier=sqrtl(envelope)*feedback_float(m,0x6298a8+phase*4);
    for(i=0;i<3;i++)out[i]=(float)((long double)feedback_float(m,camera+24+i*4)*multiplier);
    *active=1;return !m->fault;
}
/*4b3480 publishes to the retained16-entry HUD ring. All float store boundaries
 *are intentional; the camera's third component comes from the CRT RNG.*/
int game_player_hit_marker(GameMemory *m,const float point[3],uint8_t kind,int32_t amount){
    GameAddress player,history,camera,slot;uint32_t tick,index,next;uint16_t phase;
    float dx,dz,rotated_x,rotated_z,x,z,scale,amplitude,impulse[3];long double s,c,pulse;unsigned i;
    if(!m || m->fault || !point)return 0;
    player=game_memory_read_u32(m,game_memory_read_u32(m,0x5f0208)+0x3bc);
    phase=(uint16_t)(0u-game_memory_read_u16(m,player+0x24));
    s=feedback_float(m,0x6298a8+((uint32_t)phase&2047)*4);c=feedback_float(m,0x6298a8+(((uint32_t)phase+512)&2047)*4);
    dx=(float)((long double)feedback_float(m,player+24)-point[0]);dz=(float)((long double)feedback_float(m,player+32)-point[2]);
    rotated_z=(float)(c*dz-s*dx);rotated_x=(float)(c*dx+s*dz);
    tick=game_memory_read_u32(m,0x5ac6ac);
    pulse=(long double)(tick&16)*feedback_float(m,0x57460c);pulse*=feedback_float(m,0x574628);
    pulse-=feedback_float(m,0x57462c);pulse+=feedback_float(m,0x5744e4);
    pulse*=(long double)feedback_float(m,0x5744e4)/feedback_float(m,player+0x38);
    x=(float)((long double)rotated_x*pulse);z=(float)((long double)rotated_z*pulse);
    history=game_memory_read_u32(m,0x60386c);index=game_memory_read_u32(m,0x603874);
    if(!history || index>=16 || (uint64_t)history+256u>UINT32_MAX)return 0;
    slot=history+index*16;
    i=game_memory_read_u32(m,0x597f80)==1;
    scale=feedback_float(m,i?0x574630:0x574634);
    game_memory_write_u32(m,slot,feedback_truncate((long double)x*scale)+(i?152:88));
    game_memory_write_u32(m,slot+4,feedback_truncate((long double)z*scale)+(i?442:175));
    game_memory_write_u32(m,slot+8,tick);game_memory_write_u32(m,slot+12,kind==11 || kind==38 || kind==39?80:144);
    next=(index+1)&15;game_memory_write_u32(m,0x603874,next);
    if(next==game_memory_read_u32(m,0x6039a8))game_memory_write_u32(m,0x6039a8,(next+1)&15);
    game_memory_write_u32(m,0x603d0c,0u-feedback_truncate((long double)x*feedback_float(m,0x574578)));
    game_memory_write_u32(m,0x603d10,0u-feedback_truncate((long double)z*feedback_float(m,0x574578)));game_memory_write_u32(m,0x604064,tick);
    camera=game_memory_read_u32(m,0x58f89c);
    if(camera==game_memory_read_u32(m,0x58f890)){
        amplitude=(float)((long double)amount*feedback_float(m,0x574638));
        impulse[0]=(float)((long double)x*amplitude);impulse[1]=(float)((long double)z*amplitude*feedback_float(m,0x57463c));
        impulse[2]=(float)(((long double)portable_crt_rand_step(m)*feedback_float(m,0x574640)-feedback_float(m,0x5745b8))*feedback_float(m,0x574578));
        if(!game_camera_hit_begin(m,camera,impulse))return 0;
    }
    return !m->fault;
}
uint32_t game_presentation_location_token(GameMemory *memory,float coordinate,int horizontal){
    long double scaled=((long double)coordinate*feedback_float(memory,0x571f64u))*feedback_float(memory,0x571f68u);
    uint32_t integer=0u;
    /* Original FTOL returns the low word of a truncated signed 64-bit result. */
    if(scaled>=-9223372036854775808.0L && scaled<9223372036854775808.0L)integer=(uint32_t)(int64_t)scaled;
    return horizontal?48u-integer:integer+29u;
}
int game_presentation_deactivation_feedback(GameMemory *memory,GameAddress object,uint32_t reason,int16_t announcement,
    const GamePresentationFeedbackServices *services){
    GameAddress owner,bank;uint32_t horizontal,vertical;int accepted;int16_t sounds[8];unsigned index;
    if(reason!=1u && reason!=3u)return !memory->fault;
    owner=game_memory_read_u32(memory,0x5f0208u);
    if(game_memory_read_u16(memory,owner+8u)!=(game_memory_read_u32(memory,object+0x14u)&63u))return !memory->fault;
    bank=game_memory_read_u32(memory,0x628a90u);
    if(memory->fault || !services || !services->clear_queue || !services->submit_queue ||
       !services->clear_queue(services->context,bank))return 0;
    vertical=game_presentation_location_token(memory,feedback_float(memory,object+0x20u),0);
    horizontal=game_presentation_location_token(memory,feedback_float(memory,object+0x18u),1);
    for(index=0u;index<8u;index++)sounds[index]=-1;
    if(reason==1u){sounds[0]=announcement;sounds[1]=0x5d;sounds[2]=(int16_t)horizontal;sounds[3]=(int16_t)vertical;sounds[4]=0x5e;}
    else{sounds[0]=0x7f;sounds[1]=announcement;sounds[2]=0x5d;sounds[3]=(int16_t)horizontal;sounds[4]=(int16_t)vertical;sounds[5]=0x60;}
    bank=game_memory_read_u32(memory,0x628a90u);
    if(memory->fault || !services->submit_queue(services->context,bank,sounds,&accepted))return 0;
    if(accepted){
        uint32_t tick;
        owner=game_memory_read_u32(memory,0x5f0208u);
        game_memory_write_u16(memory,owner+0x598u,(uint16_t)(reason==1u?announcement:0x7f));
        tick=game_memory_read_u32(memory,0x5ac6acu);
        owner=game_memory_read_u32(memory,0x5f0208u);
        game_memory_write_u32(memory,owner+0x58cu,tick);
    }
    return !memory->fault;
}

int game_player_update_hit_timer(GameMemory *memory,
    const GamePlayerHitTimerOps *ops, uint32_t *value) {
    GameAddress target;
    uint32_t tick;
    uint32_t start_tick;
    uint32_t elapsed;
    if (!memory || !ops || !ops->read_tick || !ops->scale_elapsed)
        return 0;
    target = game_memory_read_u32(memory, 0x0063c948u);
    if (memory->fault || !ops->read_tick(ops->context, target, &tick))
        return 0;
    game_memory_write_u32(memory, 0x00627930u, 3u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, 0x00627900u, 0u);
    if (memory->fault) return 0;
    start_tick = game_memory_read_u32(memory, 0x005ac90cu);
    if (memory->fault) return 0;
    elapsed = ops->scale_elapsed(ops->context, tick, start_tick);
    game_memory_write_u32(memory, 0x00627078u, elapsed);
    if (memory->fault) return 0;
    if (value) *value = elapsed;
    return 1;
}
