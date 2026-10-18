#include "level_run.h"
#include "gameplay_input.h"
#include "ui_geometry.h"
#include <math.h>
#include <string.h>

extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_projection_bounds(GameApplication *,uint32_t,uint32_t,uint32_t,uint32_t);
extern int game_application_palette_quad(GameApplication *,int32_t,int32_t,int32_t,int32_t,uint8_t);
extern int game_application_palette_point(GameApplication *,int32_t,int32_t,uint8_t);
extern int game_application_palette_lines(GameApplication *,const int32_t[][4],unsigned,uint8_t);
extern int game_level_frame_text_bytes(GameApplication *,int32_t,int32_t,const uint8_t *,size_t);
extern int game_level_frame_00532870(GameApplication *,int32_t,int32_t,GameAddress,uint32_t);
extern int game_level_frame_004d8ab0(GameApplication *,int32_t,int32_t);
extern int game_level_control_bitmap(GameApplication *,GameAddress,const GameUiRect *);
extern int game_input_panel_layout(GameApplication *,int,int);
extern int game_menu_pointer_read(GameApplication *,int32_t[2]);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t integer(uint32_t bits){int32_t v;memcpy(&v,&bits,4);return v;}
static int16_t short_integer(uint16_t bits){int16_t v;memcpy(&v,&bits,2);return v;}
static uint32_t half(uint32_t v){return (v>>1)|(v&0x80000000u);}
static uint32_t truncate_word(long double value){return (uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
static int clip(GameApplication *a,uint32_t left,uint32_t top,uint32_t right,uint32_t bottom){
    uint32_t values[4]={left,top,right,bottom};unsigned i;
    for(i=0;i<4;i++){
        int32_t high=integer(get(a,0x597f68+4*(i&1))),low=integer(get(a,0x597f70+4*(i&1))),value=integer(values[i]);
        if(value>high)value=high;
        if(value<low)value=low;
        put(a,0x597f50+4*i,(uint32_t)value);
    }
    return !a->memory.fault;
}
static int fill(GameApplication *a,uint32_t l,uint32_t t,uint32_t r,uint32_t b,uint8_t color){return game_application_palette_quad(a,integer(l),integer(t),integer(r),integer(b),color);}
static int point(GameApplication *a,uint32_t x,uint32_t y,uint8_t color){return game_application_palette_point(a,integer(x),integer(y),color);}
static int border(GameApplication *a,uint32_t l,uint32_t t,uint32_t r,uint32_t b,uint8_t color){
    int32_t lines[4][4]={{integer(l),integer(t),integer(r),integer(t)},
        {integer(r),integer(t),integer(r),integer(b)},{integer(r),integer(b),integer(l),integer(b)},
        {integer(l),integer(b),integer(l),integer(t)}};
    return game_application_palette_lines(a,lines,4,color);
}
static int text(GameApplication *a,GameAddress source,uint32_t x,uint32_t y,uint8_t color){
    uint8_t bytes[64];size_t n;
    for(n=0;n<sizeof(bytes);n++){bytes[n]=game_memory_read_u8(&a->memory,source+(uint32_t)n);if(!bytes[n])break;}
    if(n==sizeof(bytes) || a->memory.fault)return 0;
    game_memory_write_u8(&a->memory,0x59d448,color);
    return !n || game_level_frame_text_bytes(a,integer(x),integer(y),bytes,n+1);
}
/*4161c0/416a20/416c40/416e10: the constructor's actual control records,
 * including four borrowed image states, tooltip timing and post-draw movement. */
static int control_present(GameApplication *a,GameAddress w){
    GameAddress method=get(a,get(a,w)+8);uint32_t l=get(a,w+0xa8),b=get(a,w+0xac),r=get(a,w+0xb0),t=get(a,w+0xb4),pressed=get(a,w+4);unsigned axis;
    if(method!=0x416a20 && method!=0x416c40 && method!=0x416e10)return 0;
    if(get(a,w+12)){
        if(get(a,w+0xd4) && get(a,w+0xec)){
            unsigned state=(get(a,w+0x10)<1?2:0)+(pressed<1?0:1);
            GameUiRect bounds={integer(l),integer(t),integer(r),integer(b)};
            if(!game_level_control_bitmap(a,w+0xc0+state*24,&bounds))return 0;
        }else{
            if(!fill(a,l+1,t+1,r-1,b-1,110))return 0;
            if(pressed){
                if(!fill(a,l,t,r,t+1,105) || !fill(a,r-1,t+2,r,b,105) ||
                   !fill(a,l,t+2,l+1,b,246) || !fill(a,l+2,b-1,r-2,b,246) ||
                   !point(a,l,t+1,246) || !point(a,r-1,b,246))return 0;
            }else if(!fill(a,l+2,t,r,t+1,246) || !fill(a,r-1,t+2,r,b-2,246) ||
                     !fill(a,l,t,l+1,b,105) || !fill(a,l+2,b-1,r,b,105) ||
                     !point(a,l+1,t,246) || !point(a,r,b-1,246))return 0;
        }
        if(get(a,w+0x58)){
            uint32_t x=l+half(r-get(a,w+0x58)-l)+(pressed?0:1),y=half(b+t-6)+(pressed?2:1);
            if(!text(a,w+0x18,x,y,pressed?246:105))return 0;
        }
    }
    if(get(a,w+0x9c) && get(a,w+8) && get(a,w+0x14) && game_menu_milliseconds(a)-get(a,w+0x14)>=100){
        uint32_t x=get(a,w+0xa0),y=get(a,w+0xa4);
        if(x==UINT32_MAX && y==UINT32_MAX){x=half(r+l-get(a,w+0x9c));y=half(b+t-4);}
        if(!text(a,w+0x5c,x,y,246))return 0;
    }
    if(method==0x416e10)for(axis=0;axis<2;axis++){
        GameAddress pending=w+0x128+axis*4;uint32_t old=get(a,pending),magnitude=old&0x80000000u?0u-old:old,step,delta;
        if(!old)continue;
        step=half(half(magnitude));if(integer(step)<=1)step=1;
        delta=integer(old)>0?step:0u-step;
        put(a,w+(axis?0xb4:0xa8),get(a,w+(axis?0xb4:0xa8))+delta);
        put(a,w+(axis?0xac:0xb0),get(a,w+(axis?0xac:0xb0))+delta);
        old=integer(old)>0?old-step:old+step;
        put(a,pending,(delta&0x80000000u)?(integer(old)<0?old:0):(integer(old)>0?old:0));
    }
    return !a->memory.fault;
}
static int census(GameApplication *a){
    unsigned group_index,member_index;uint32_t count;GameAddress groups=get(a,0x639a70),owner=get(a,0x5f0208);
    for(group_index=0;group_index<7;group_index++)put(a,0x5acea0+group_index*4,0);
    count=(uint32_t)(int32_t)short_integer(game_memory_read_u16(&a->memory,0x639a74));
    for(group_index=0;integer(count)>(int32_t)group_index;group_index++){
        GameAddress group=get(a,groups+group_index*4),members;int member_count;
        if(group_index>=32768)return 0;
        if(get(a,group+0x54)!=owner)continue;
        members=get(a,group+0x18);member_count=short_integer(game_memory_read_u16(&a->memory,group+0x24));
        for(member_index=0;(int)member_index<member_count;member_index++){
            GameAddress record=get(a,members+member_index*4),payload,method;int kind;uint32_t value=0;
            if(!record)continue;
            kind=short_integer(game_memory_read_u16(&a->memory,record+0x14));if(kind<0 || kind>5)continue;
            payload=get(a,record+0x48);
            if(!kind){
                GameAddress camera=get(a,owner+0x448);float dx=(float)((long double)real(a,payload+24)-real(a,camera+24)),dz=(float)((long double)real(a,payload+32)-real(a,camera+32)),distance;
                if(dx<real(a,0x571f14) || isnan(dx))dx=-dx;
                if(dz<real(a,0x571f14) || isnan(dz))dz=-dz;
                distance=(float)(dz<dx || isnan(dx) || isnan(dz)?(long double)dz*real(a,0x571f18)+dx:(long double)dx*real(a,0x571f18)+dz);
                if(distance>real(a,0x572c6c))continue;
                value=get(a,record+0x4c);put(a,0x5aceb4,get(a,0x5aceb4)+value);put(a,0x5aceb8,get(a,0x5aceb8)+get(a,record+0x4c));continue;
            }
            method=get(a,get(a,record)+0x50);
            if(method==0x53d900)value=0;
            else if(method==0x53f2f0 || method==0x53f400){
                if(get(a,record+0x10)!=1)value=100;
                else if(method==0x53f400){
                    float current=real(a,payload+0xc4),maximum=real(a,payload+0xc8);
                    if(current<maximum || isnan(current) || isnan(maximum)){
                        long double scale=real(a,0x575484);
                        value=truncate_word(scale-(long double)current*scale/maximum);
                    }
                }
                else{
                    uint32_t elapsed=get(a,0x5ac6ac)-get(a,payload+0xac),duration=get(a,payload+0xa8);
                    if(integer(elapsed)<integer(duration)){
                        int32_t numerator=integer(0u-elapsed*100u),denominator=integer(duration);
                        if(!denominator || (numerator==INT32_MIN && denominator==-1)){a->memory.fault=1;return 0;}
                        value=(uint32_t)(numerator/denominator)+100u;
                    }
                }
            }else return 0;
            if(!value)put(a,0x5acea0+(unsigned)(kind-1)*4,get(a,0x5acea0+(unsigned)(kind-1)*4)+1);
        }
    }
    return !a->memory.fault;
}
/*435240/4841e0 share animation/control ownership. Their distinct globals and
 *primary availability census remain separate; no native-pointer publication. */
static int panel_frame(GameApplication *a,int alternate){
    GameAddress bounds=alternate?0x5f0240:0x5acec0,closing=alternate?0x589ac0:0x58404c,open=alternate?0x589abc:0x584048;
    GameAddress old_tick=alternate?0x5f0228:0x5ace78,low_height=alternate?0x5f0238:0x5aced0,high_height=alternate?0x5f0234:0x5aced8;
    GameAddress count_at=alternate?0x5f022c:0x5acefc,items=alternate?0x5f0250:0x5ace80,hover=alternate?0x5f0230:0x5ace7c,panel=alternate?0x5f023c:0x5aced4;
    uint32_t high=get(a,0x597f80)!=0,limit=high?331:146,height=get(a,high?high_height:low_height),top=get(a,bounds+4),tick=get(a,0x5ac6ac),speed=high?12:8,index=0;
    GameAddress selected;int32_t target;
    if(!clip(a,get(a,bounds),0,get(a,bounds+8)+1,limit+2) || !game_menu_projection_bounds(a,get(a,bounds),0,get(a,bounds+8)+1,limit+2))return 0;
    put(a,0x59d46c,1);
    if(get(a,closing)){
        top+=(tick-get(a,old_tick))*speed;put(a,bounds+4,top);put(a,bounds+12,top+height);
        if(integer(top)>=integer(limit))put(a,closing,0);
    }else{
        target=integer(limit-height);
        if(integer(top)>target){top+=(get(a,old_tick)-tick)*speed;if(integer(top)<=target)top=(uint32_t)target;put(a,bounds+4,top);put(a,bounds+12,top+height);}
    }
    if(!game_input_panel_layout(a,alternate,high) || !game_input_panel_hover(a,get(a,panel),integer(get(a,0x5ac9c8)),integer(get(a,0x5ac9cc)),&selected) ||
       !fill(a,get(a,bounds),get(a,bounds+4),get(a,bounds+8),get(a,bounds+12),100))return 0;
    while(integer(get(a,get(a,panel)))>(int32_t)index){
        GameAddress w=get(a,get(a,panel)+4+index*4);if(index>=256)return 0;
        put(a,w+0xa0,get(a,bounds)+1);put(a,w+0xa4,get(a,bounds+4)-6);
        if(!control_present(a,w))return 0;
        ++index;
    }
    if(!alternate){
        if(!census(a))return 0;
        for(index=0;integer(get(a,count_at))>(int32_t)index;index++){
            GameAddress w=get(a,items+index*4);uint32_t category=get(a,0x5acee0+index*4),l=get(a,w+0xa8),t=get(a,w+0xb4);
            if(index>=7 || category>=7)return 0;
            put(a,w+0x10,get(a,0x5acea0+category*4)>=1);
            game_memory_write_u8(&a->memory,0x59d448,246);
            if(!game_level_frame_00532870(a,integer(l+1),integer(t-6),0x5840bc,get(a,0x5acea0+category*4)))return 0;
        }
    }
    index=get(a,hover);
    if(index!=UINT32_MAX){GameAddress w;if(index>=get(a,count_at))return 0;w=get(a,items+index*4);
        if(!border(a,get(a,w+0xa8)-1,get(a,w+0xb4)-1,get(a,w+0xb0)+1,get(a,w+0xac)+1,246))return 0;}
    if(get(a,open)){
        /*4a1f20's 5x5 right-side cursor, drawn as actual palette points. */
        static const uint8_t marks[25]={0,0,0,1,0,0,0,0,1,0,0,1,1,1,0,1,1,1,1,1,0,1,1,1,0};unsigned row,column;
        for(row=0;row<5;row++)for(column=0;column<5;column++)if(marks[row*5+column])
            if(!point(a,get(a,0x5ac9c8)+column-3,get(a,0x5ac9cc)+row,169))return 0;
        if(!game_level_frame_004d8ab0(a,integer(get(a,0x5ac9c8)),integer(get(a,0x5ac9cc))))return 0;
    }
    if(!clip(a,0,0,get(a,0x5ac6a0),get(a,0x5ac6a4)))return 0;
    {GameAddress work=get(a,0x628a88);return game_menu_projection_bounds(a,get(a,work+0x534),get(a,work+0x538),get(a,work+0x53c),get(a,work+0x540));}
}
int game_level_frame_00435240(GameApplication *a){return a && !a->memory.fault && panel_frame(a,0);}
int game_level_frame_004841e0(GameApplication *a){return a && !a->memory.fault && panel_frame(a,1);}
/*515f40: system cursor position belongs to the platform; clamping and the
 *two nine-pixel cross segments are ordinary frame behavior. */
int game_level_frame_00515f40(GameApplication *a,int unused){
    int32_t position[2],lines[2][4];unsigned axis;(void)unused;
    if(!a || a->memory.fault || !game_menu_pointer_read(a,position))return 0;
    for(axis=0;axis<2;axis++){
        int32_t maximum=integer(get(a,0x597f68+axis*4)-1u);
        if(position[axis]<0)position[axis]=0;
        else if(position[axis]>maximum)position[axis]=maximum;
    }
    lines[0][0]=integer((uint32_t)position[0]-5u);lines[0][1]=position[1];
    lines[0][2]=integer((uint32_t)position[0]+4u);lines[0][3]=position[1];
    lines[1][0]=position[0];lines[1][1]=integer((uint32_t)position[1]-5u);
    lines[1][2]=position[0];lines[1][3]=integer((uint32_t)position[1]+4u);
    return !a->memory.fault && game_application_palette_lines(a,lines,2,169);
}
