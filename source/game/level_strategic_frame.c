#include "level_run.h"
#include "level_scene_render.h"
#include "level_world_render.h"
#include "level_player_resources.h"
#include "gameplay_catalog.h"
#include "gameplay_geometry.h"
#include "game_initializers.h"
#include "gameplay_chat.h"
#include "gameplay_messages.h"
#include "game_text_rasterizer.h"
#include "ui_geometry.h"
#include "gameplay_clipper.h"
#include "level_world_surface.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/*Native strategic consumers below share live owners and real value arguments.
 *There is no generated-execution fallback for an unsupported getter. */
extern int game_level_strategic_map_marks(GameApplication *); /*4a1830*/
extern int game_level_strategic_map_units(GameApplication *); /*4a1290*/
extern int game_level_strategic_descriptor_details(GameApplication *,GameAddress,GameAddress *,uint32_t *,uint32_t *);
extern int game_input_panel_hover(GameApplication *,GameAddress,int32_t,int32_t,GameAddress *);
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_application_palette_quad(GameApplication *,int32_t,int32_t,int32_t,int32_t,uint8_t);
extern int game_application_palette_lines(GameApplication *,const int32_t[][4],unsigned,uint8_t);
extern int game_application_render_mode(GameApplication *,int);
extern int game_level_screen_present(GameApplication *,uint32_t); /*55ce8c*/
extern int game_application_palette_point(GameApplication *,int32_t,int32_t,uint8_t);
extern int game_level_frame_004d8ab0(GameApplication *,int32_t,int32_t);
extern int game_level_control_bitmap(GameApplication *,GameAddress,const GameUiRect *);
extern int game_menu_projection_bounds(GameApplication *,uint32_t,uint32_t,uint32_t,uint32_t);
extern int game_level_frame_text_bytes(GameApplication *,int32_t,int32_t,const uint8_t *,size_t);
extern int game_level_frame_004d8b50(GameApplication *,const uint32_t[4],uint32_t[4],const uint32_t[2],float);
extern int game_level_frame_004d89e0(GameApplication *,GameAddress);
extern int game_level_frame_004d9570(GameApplication *);
extern int game_level_frame_004a3390(GameApplication *,int32_t,int32_t,int32_t,int32_t,uint8_t,uint8_t,float);
extern int game_level_frame_004b2170(GameApplication *,const float[6]);
extern int game_level_frame_00548c70(GameApplication *);
extern int game_level_frame_004c6eb0(GameApplication *);
extern int game_level_frame_00515f40(GameApplication *,int);
extern int game_level_scene_property(GameApplication *,GameAddress,GameAddress,uint32_t *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t integer(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static float floating(uint32_t v){float f;memcpy(&f,&v,4);return f;}
static float real(GameApplication *a,GameAddress p){return floating(get(a,p));}
static void store(GameApplication *a,GameAddress p,float f){uint32_t b;memcpy(&b,&f,4);put(a,p,b);}
static int32_t shift(uint32_t v,unsigned n){int32_t s=integer(v);return s>=0?s/(1<<n):-1-(int32_t)((-(int64_t)s-1)/(1<<n));}
static uint32_t truncated(long double v){return (uint32_t)(uint64_t)(!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v);}
static int rectangle(GameApplication *a,uint32_t l,uint32_t t,uint32_t r,uint32_t b,unsigned color){return game_application_palette_quad(a,integer(l),integer(t),integer(r),integer(b),(uint8_t)color);}
static int line(GameApplication *a,uint32_t l,uint32_t t,uint32_t r,uint32_t b,unsigned color){int32_t points[1][4]={{integer(l),integer(t),integer(r),integer(b)}};return game_application_palette_lines(a,(const int32_t (*)[4])points,1,(uint8_t)color);}
static int string(GameApplication *a,GameAddress p,char *bytes,size_t size){size_t i;for(i=0;i<size;i++){bytes[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!bytes[i])return !a->memory.fault;}return 0;}
static int text(GameApplication *a,int32_t x,int32_t y,const char *bytes){return game_level_frame_text_bytes(a,x,y,(const uint8_t *)bytes,strlen(bytes)+1);}
static int caption(GameApplication *a,uint32_t x,uint32_t y,GameAddress format,int32_t value){char spec[64],bytes[128];int n;if(!string(a,format,spec,sizeof(spec)))return 0;n=snprintf(bytes,sizeof(bytes),spec,value);return n>=0 && (size_t)n<sizeof(bytes) && text(a,integer(x),integer(y),bytes);}
static int clip(GameApplication *a,uint32_t l,uint32_t t,uint32_t r,uint32_t b){uint32_t values[4]={l,t,r,b};unsigned i;for(i=0;i<4;i++){int32_t v=integer(values[i]),lo=integer(get(a,0x597f70+4*(i&1))),hi=integer(get(a,0x597f68+4*(i&1)));if(v>hi)v=hi;if(v<lo)v=lo;put(a,0x597f50+i*4,(uint32_t)v);}return !a->memory.fault;}
static int image(GameApplication *a,const uint32_t source[4],GameAddress destination,float depth){uint32_t dest[4],descriptor[2];unsigned i;int ok;for(i=0;i<4;i++)dest[i]=get(a,destination+i*4);descriptor[0]=get(a,0x596100);descriptor[1]=get(a,0x596104);ok=game_level_frame_004d8b50(a,source,dest,descriptor,depth);for(i=0;i<4;i++)put(a,destination+i*4,dest[i]);return ok && !a->memory.fault;}
/*4161c0: both fixed buttons and moving panel buttons borrow their existing
 *image states and text. Painting precedes the moving button's easing step. */
static int control_draw(GameApplication *a,GameAddress w){
    uint32_t l=get(a,w+0xa8),t=get(a,w+0xb4),r=get(a,w+0xb0),b=get(a,w+0xac),pressed=get(a,w+4),width;char bytes[64];
    if(get(a,w+12)){
        if(get(a,w+0xd4) && get(a,w+0xec)){
            GameUiRect bounds={integer(l),integer(t),integer(r),integer(b)};
            unsigned state=(get(a,w+16)==0?2u:0u)+(pressed!=0);
            if(!game_level_control_bitmap(a,w+0xc0+state*24,&bounds))return 0;
        }else{
            if(!rectangle(a,l+1,t+1,r-1,b-1,110))return 0;
            if(pressed){
                if(!rectangle(a,l,t,r,t+1,105) || !rectangle(a,r-1,t+2,r,b,105) ||
                   !rectangle(a,l,t+2,l+1,b,246) || !rectangle(a,l+2,b-1,r-2,b,246) ||
                   !game_application_palette_point(a,integer(l),integer(t+1),246) || !game_application_palette_point(a,integer(r-1),integer(b),246))return 0;
            }else if(!rectangle(a,l+2,t,r,t+1,246) || !rectangle(a,r-1,t+2,r,b-2,246) ||
                    !rectangle(a,l,t,l+1,b,105) || !rectangle(a,l+2,b-1,r,b,105) ||
                    !game_application_palette_point(a,integer(l+1),integer(t),246) || !game_application_palette_point(a,integer(r),integer(b-1),246))return 0;
        }
        width=get(a,w+0x58);
        if(width){
            uint32_t x=l+(uint32_t)shift(r-width-l,1)+(pressed?0u:1u),y=(uint32_t)shift(b+t-6,1)+(pressed?2u:1u);
            game_memory_write_u8(&a->memory,0x59d448,pressed?246:105);
            if(!string(a,w+0x18,bytes,sizeof(bytes)) || !text(a,integer(x),integer(y),bytes))return 0;
        }
    }
    if(get(a,w+0x9c) && get(a,w+8) && get(a,w+0x14) && game_menu_milliseconds(a)-get(a,w+0x14)>=100u){
        uint32_t x=get(a,w+0xa0),y=get(a,w+0xa4);
        if(x==UINT32_MAX && y==UINT32_MAX){x=(uint32_t)shift(r+l-get(a,w+0x9c),1);y=(uint32_t)shift(b+t-4,1);}
        game_memory_write_u8(&a->memory,0x59d448,246);
        if(!string(a,w+0x5c,bytes,sizeof(bytes)) || !text(a,integer(x),integer(y),bytes))return 0;
    }
    return !a->memory.fault;
}
static int panel_draw(GameApplication *a,GameAddress panel){
    unsigned i;uint32_t count=get(a,panel);
    if(integer(count)<=0)return !a->memory.fault;
    if(count>256)return 0;
    for(i=0;i<count;i++){
        GameAddress w=get(a,panel+4+i*4),method=get(a,get(a,w)+8);unsigned axis;
        if(method!=0x416a20 && method!=0x416c40 && method!=0x416e10)return 0;
        if(!control_draw(a,w))return 0;
        if(method==0x416e10)for(axis=0;axis<2;axis++){
            uint32_t remaining=get(a,w+0x128+axis*4),magnitude;int32_t signed_remaining=integer(remaining);uint32_t step;
            if(!remaining)continue;
            magnitude=signed_remaining<0?0u-remaining:remaining;step=(uint32_t)shift(magnitude,2);if(integer(step)<=1)step=1;
            put(a,w+(axis?0xb4:0xa8),get(a,w+(axis?0xb4:0xa8))+(signed_remaining>0?step:0u-step));
            put(a,w+(axis?0xac:0xb0),get(a,w+(axis?0xac:0xb0))+(signed_remaining>0?step:0u-step));
            remaining+=signed_remaining>0?0u-step:step;
            if((signed_remaining>0 && integer(remaining)<0) || (signed_remaining<0 && integer(remaining)>0))remaining=0;
            put(a,w+0x128+axis*4,remaining);
        }
        count=get(a,panel);if(count>256)return 0;
    }
    return !a->memory.fault;
}
static int hover_caption(GameApplication *a){
    static const GameAddress formats[10]={0x58f650,0x58f66c,0x58f684,0x58f698,0x58f6b0,0x58f6c8,0x58f6e4,0x58f6fc,0x58f714,0x58f730};
    GameAddress selected=0;uint32_t id,price;char spec[64],bytes[64];size_t i;int n;
    if(!game_input_panel_hover(a,get(a,0x602678),integer(get(a,0x5ac9c8)),integer(get(a,0x5ac9cc)),&selected))return 0;
    if(selected && (id=get(a,selected+0x120))>=1 && id<=10){
        if(!game_level_player_catalog_unit_price(a,get(a,0x5f0208),(int16_t)id,&price) || !string(a,formats[id-1],spec,sizeof(spec)))return 0;
        n=snprintf(bytes,sizeof(bytes),spec,integer(price));if(n<0 || n>=63)return 0;
        for(i=0;i<63;i++)game_memory_write_u8(&a->memory,selected+0x5c+(uint32_t)i,i<(size_t)n?(uint8_t)bytes[i]:0);
        put(a,selected+0x9c,game_text_width_bytes(&a->memory,(const uint8_t *)bytes,(size_t)n+1));
    }else if(!game_input_panel_hover(a,get(a,0x602678),integer(get(a,0x5ac9c8)),integer(get(a,0x5ac9cc)),&selected))return 0;
    return !a->memory.fault;
}
static int pulse(GameApplication *a){
    uint32_t v=get(a,0x59d460)+get(a,0x59d464);put(a,0x59d460,v);
    if(v==10)put(a,0x59d464,UINT32_MAX);else if(!v)put(a,0x59d464,1);
    game_memory_write_u8(&a->memory,0x59d44c,110);return !a->memory.fault;
}
static int boost_lamps(GameApplication *a){
    static const uint32_t rows[3][4]={{0x1a8,0x1b2,247,246},{0x19b,0x1a5,164,168},{0x190,0x199,148,152}};
    uint32_t phase=get(a,0x5ac6ac),mode=UINT32_MAX;unsigned i;
    if(get(a,0x597f80)!=1)return 1;
    if(get(a,0x602598))mode=phase&16?(phase&12)>>2:2u-((phase&12)>>2);
    if(mode==0)return rectangle(a,0x17d,0x1a8,0x187,0x1b2,249) && rectangle(a,0x17d,0x1a9,0x185,0x1b2,250);
    if(mode>2 && mode!=UINT32_MAX)return 1;
    if(mode==1 && (!rectangle(a,0x17d,0x19b,0x187,0x1a5,184) || !rectangle(a,0x17d,0x19c,0x185,0x1a5,188)))return 0;
    if(mode==2 && (!rectangle(a,0x17d,0x190,0x187,0x199,152) || !rectangle(a,0x17d,0x191,0x185,0x199,155)))return 0;
    for(i=0;i<3;i++)if(i!=1 || mode!=1)if(!rectangle(a,0x17d,rows[i][0],0x187,rows[i][1],rows[i][2]) || !rectangle(a,0x17d,rows[i][0]+1,0x185,rows[i][1],rows[i][3]))return 0;
    return 1;
}
static int map_grid(GameApplication *a){
    uint32_t l=get(a,0x6027b8),t=get(a,0x6027bc),r=get(a,0x6027c0),b=get(a,0x6027c4),x,y;unsigned i;char bytes[8];
    float width=(float)integer(r-l-1),height=(float)integer(b-t-1),dx=(float)((long double)width*real(a,0x5742e4)),dy=(float)((long double)height*real(a,0x5742e4)),px=0,py=0;
    for(i=0;i<10;i++){
        uint32_t bx,by;memcpy(&bx,&px,4);memcpy(&by,&py,4);
        x=l+truncated((long double)px+(integer(bx)>0?1.0L:-1.0L)*real(a,0x5742dc));
        y=t+truncated((long double)py+(integer(by)>0?1.0L:-1.0L)*real(a,0x5742dc));
        if(get(a,0x60269c) && (!line(a,x,t,x,b-1,246) || !line(a,l,y,r-1,y,246)))return 0;
        if(!get(a,0x597f80)){
            if(!line(a,l-7,y,l-6,y,110) || !line(a,x,b+6,x,b+5,110))return 0;
            y+=truncated((long double)height*real(a,0x5742e8))-1;
            if(i){snprintf(bytes,sizeof(bytes),"%u",10-i);if(!text(a,integer(l-5),integer(y),bytes))return 0;}
            else{if(!text(a,integer(l-7),integer(y),"1"))return 0;if(!text(a,integer(get(a,0x62970c)+3),integer(get(a,0x629708)),"0"))return 0;}
            bytes[0]=(char)('a'+i);bytes[1]=0;if(!text(a,integer(x+truncated((long double)width*real(a,0x5742e8))),integer(b+1),bytes))return 0;
        }
        px=(float)((long double)px+dx);py=(float)((long double)py+dy);
    }
    {long double vx=(long double)width*real(a,0x5742ec),vy=(long double)height*real(a,0x5742ec);float sx=(float)vx,sy=(float)vy;
     x=l+truncated((long double)sx+(vx>real(a,0x5742d0)?1.0L:-1.0L)*real(a,0x5742dc));
     y=t+truncated((long double)sy+(vy>real(a,0x5742d0)?1.0L:-1.0L)*real(a,0x5742dc));}
    if(get(a,0x60269c) && (!line(a,x,t,x,b-1,246) || !line(a,l,y,r-1,y,246)))return 0;
    return line(a,l-7,y,l-6,y,110) && line(a,x,b+6,x,b+5,110);
}
static int map_coordinates(GameApplication *a){
    uint32_t x=get(a,0x5ac9c8),y=get(a,0x5ac9cc),l=get(a,0x6027b8),t=get(a,0x6027bc),r=get(a,0x6027c0),b=get(a,0x6027c4);float fx,fy;
    if(integer(l)>integer(x) || integer(r)<integer(x) || integer(t)>integer(y) || integer(b)<integer(y))return 1;
    fx=(float)((long double)integer((x-l)<<17)/integer(r-l));fy=(float)((long double)integer((b-y-1)<<17)/integer(b-t));
    game_memory_write_u8(&a->memory,0x59d448,255);
    if(!caption(a,get(a,0x597f80)?182:92,get(a,0x597f80)?23:9,0x58f3e8,shift(truncated((long double)fx*real(a,0x5742cc)),17)+'a'))return 0;
    return caption(a,get(a,0x62970c)+6,get(a,0x629708),0x58f3ec,shift(truncated((long double)fy*real(a,0x5742cc)),17)+1);
}
static int map_positions(GameApplication *a){
    unsigned i;uint32_t l=get(a,0x6027b8),t=get(a,0x6027bc),r=get(a,0x6027c0),b=get(a,0x6027c4);
    for(i=0;i<5;i++){
        uint32_t bx=get(a,0x602538+i*8),by=get(a,0x60253c+i*8),x,y;uint8_t color;
        if(bx==0xbf800000 || by==0xbf800000 || bx>0x80000000u || integer(bx)>0x48000000 || by>0x80000000u || integer(by)>0x48000000)continue;
        x=l+(uint32_t)shift(truncated((long double)integer(r-l)*floating(bx)),17);
        y=b-(uint32_t)shift(truncated((long double)integer(b-t)*floating(by)),17)-1;
        color=game_memory_read_u8(&a->memory,0x58de78+i*2+((get(a,0x5ac6ac)&8)>>3));
        if(!line(a,x-2,y-2,x+2,y+2,color) || !line(a,x+2,y-2,x-2,y+2,color))return 0;
    }
    return !a->memory.fault;
}
/*4a1f20/4a2000 share the original5x5 palette-point backend and cursor history. */
static int strategic_pointer(GameApplication *a,int32_t x,int32_t y,uint8_t color,int active){
    static const uint8_t arrow[25]={0,0,0,1,0,0,0,0,1,0,0,1,1,1,0,1,1,1,1,1,0,1,1,1,0};
    static const uint8_t cross[25]={1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,1,0,1,0,1,0,0,0,1};
    unsigned row,column;const uint8_t *marks=active?cross:arrow;
    for(row=0;row<5;row++)for(column=0;column<5;column++)if(marks[row*5+column])
        if(!game_application_palette_point(a,integer((uint32_t)x+column-(active?2u:3u)),integer((uint32_t)y+row-(active?2u:0u)),color))return 0;
    return game_level_frame_004d8ab0(a,integer(get(a,0x5ac9c8)),integer(get(a,0x5ac9cc)));
}
static int member_statistics(GameApplication *a){
    static const uint8_t selected_colors[4]={153,169,246,246},colors[4]={148,165,105,105};
    GameAddress group=get(a,0x6025a0);unsigned i;uint32_t count;
    if(!group)return 1;
    if(!game_menu_projection_bounds(a,get(a,0x6026a8)+1,get(a,0x6026b4)+1,get(a,0x6026b0),get(a,0x6026ac)))return 0;
    count=(uint32_t)(int32_t)(int16_t)game_memory_read_u16(&a->memory,group+0x24);if(integer(count)<=0)return !a->memory.fault;if(count>65536)return 0;
    for(i=0;i<count;i++){
        GameAddress record=get(a,get(a,group+0x18)+i*4),name;uint32_t health,maximum,ratio,index,y=get(a,0x6026b4)+i*6+2,x=get(a,0x6026a8)+2;char label[9];
        if(record){
            if(!game_level_strategic_descriptor_details(a,record,&name,&health,&maximum) || !maximum)return 0;
            if(integer(health*100u)==INT32_MIN && integer(maximum)==-1)return 0;
            ratio=(uint32_t)(integer(health*100u)/integer(maximum));index=truncated((long double)integer(ratio)*real(a,0x57431c));if(index>=4)return 0;
            game_memory_write_u8(&a->memory,0x59d448,get(a,group+0x54)==get(a,0x5f0208) && i==get(a,0x6025b8)?colors[index]:selected_colors[index]);
            if(i==get(a,0x6025b8) && !rectangle(a,get(a,0x6026a8),y,get(a,0x6026b0),y+4,246))return 0;
            {unsigned n;for(n=0;n<8;n++){label[n]=(char)game_memory_read_u8(&a->memory,name+n);if(!label[n])break;}label[n]=0;}
            if(!text(a,integer(x),integer(y),label) || !caption(a,get(a,0x6026b0)-18,y,0x58f44c,integer(ratio)))return 0;
        }else{game_memory_write_u8(&a->memory,0x59d448,105);if(!text(a,integer(x),integer(y),"NA") || !text(a,integer(get(a,0x6026b0)-18),integer(y),"---%"))return 0;}
    }
    return !a->memory.fault;
}
/*49eff0: the strategic frame consumes the same application/world/UI owners.
 *Integer positions wrap explicitly; numeric stores match the original C ABI
 *data, not its register/stack execution. New ordinary service ports above are
 *still link blockers until their actual shared consumers are reconstructed. */
int game_level_strategic_present(GameApplication *a){
    static const uint32_t decoration[5][5]={{119,404,270,437,0},{107,454,281,463,110},{416,52,597,92,110},{489,97,631,192,110},{180,22,195,28,105}};
    uint32_t started,flags,selected,root,word;unsigned i;int32_t n,width,height;float center[2],bounds[4];
    if(!a || a->memory.fault)return 0;
    started=game_menu_milliseconds(a);flags=get(a,0x6027e4);
    put(a,0x6027e4,flags&~1u);
    if(flags&2u){
        n=(int16_t)game_memory_read_u16(&a->memory,0x639a74);root=get(a,0x5f0208);selected=get(a,root+0x448);
        for(i=0;(int32_t)i<n;i++){
            GameAddress group=get(a,get(a,0x639a70)+i*4);float x=real(a,selected+0x18),z=real(a,selected+0x20),left=real(a,group+4),top=real(a,group+8);
            if((left<=x || isnan(left) || isnan(x)) && real(a,group+12)>=x && (top<=z || isnan(top) || isnan(z)) && real(a,group+16)>=z){put(a,0x6025b8,0);put(a,0x6025a0,group);break;}
        }
        put(a,0x6027e4,get(a,0x6027e4)&~2u);
    }
    if(!hover_caption(a))return 0;
    /* 49f1f4..49f232 tests the Wraith's two movement components and yaw.
     * 6025a0 is a selected citadel descriptor, not the player scene: reading
     * its +1a8 yaw ran past that descriptor after a freshly claimed citadel
     * became the selected O-menu group. */
    root=get(a,0x5f0208);word=get(a,root+0x3bc);
    if(!(fabsf(real(a,word+0x19c))>real(a,0x5742d4)) &&
       !(fabsf(real(a,word+0x1a4))>real(a,0x5742d4)) &&
       !game_memory_read_u16(&a->memory,word+0x1a8)){put(a,0x6040a4,0);put(a,0x6040fc,0);put(a,0x604114,0x3f800000);}
    else game_initializer_004b2d80(&a->memory);
    width=integer(get(a,0x6025e8)-get(a,0x6025e0));height=integer(get(a,0x6025ec)-get(a,0x6025e4));
    center[0]=(float)((long double)integer(get(a,0x6025e8)+get(a,0x6025e0))*real(a,0x5742dc));center[1]=(float)((long double)integer(get(a,0x6025ec)+get(a,0x6025e4))*real(a,0x5742dc));
    bounds[0]=(float)integer(get(a,0x6025e0));bounds[1]=(float)integer(get(a,0x6025e4));bounds[2]=(float)integer(get(a,0x6025e8));bounds[3]=(float)integer(get(a,0x6025ec));
    if(!gameplay_geometry_projection_setup(&a->memory,0x6290c8,center,(float)width,(float)height,bounds) || !game_level_world_present(a))return 0;
    root=get(a,0x5f0208);selected=get(a,0x6025a0);n=integer(get(a,0x602618));if(n>256)return 0;
    for(i=0;(int32_t)i<n;i++){
        GameAddress widget=get(a,(get(a,0x597f80)?0x602500:0x602570)+i*4);int enabled=gameplay_catalog_unit_enabled(&a->memory,root,(int16_t)get(a,widget+0x120));
        put(a,widget+0x10,enabled && selected && get(a,selected+0x54)==root && game_memory_read_u16(&a->memory,selected+0x24)!=game_memory_read_u16(&a->memory,selected+0x26));
    }
    if(!pulse(a) || !game_level_texture_frame(a) || !game_application_render_mode(a,1))return 0;
    {uint32_t source[4]={0,0,(uint32_t)(int32_t)(int16_t)game_memory_read_u16(&a->memory,0x596100),(uint32_t)(int32_t)(int16_t)game_memory_read_u16(&a->memory,0x596102)};if(!image(a,source,0x6027b8,0.25f))return 0;}
    for(i=0;i<4;i++)if(!rectangle(a,decoration[i][0],decoration[i][1],decoration[i][2],decoration[i][3],decoration[i][4]))return 0;
    if(!rectangle(a,get(a,0x6025f8),get(a,0x602604),get(a,0x602600),get(a,0x6025fc),110) || !rectangle(a,get(a,0x602688),get(a,0x602694),get(a,0x602690),get(a,0x60268c),110) ||
       !rectangle(a,get(a,0x6024f0)-1,get(a,0x6024f4)-1,get(a,0x6024f8)+1,get(a,0x6024fc)+1,0) || !rectangle(a,180,22,195,28,105))return 0;
    if(!game_level_frame_004d89e0(a,get(a,0x5960f0)))return 0;
    /*The preserved overlay seeds the monitor's glass after the native tiled
     * artwork and before world submission. Use the active remote
     * camera's actual11-bit heading, not the main camera or a CPU stack hook. */
    {GameRemoteSkyFrame sky;char name[128];GameAddress camera=get(a,0x58f890);
        for(i=0;i<sizeof(name);i++){name[i]=(char)game_memory_read_u8(&a->memory,0x626dd0+i);if(!name[i])break;}
        if(i==sizeof(name) || a->memory.fault)return 0;
        sky.name=name;sky.bounds[0]=integer(get(a,0x6025e0));sky.bounds[1]=integer(get(a,0x6025e4));
        sky.bounds[2]=integer(get(a,0x6025e8));sky.bounds[3]=integer(get(a,0x6025ec));
        sky.yaw=camera?(game_memory_read_u16(&a->memory,camera+6)&2047)/2048.0f:0;
        sky.scale=real(a,0x6290f4);sky.speed=real(a,0x626d50);
        if(a->memory.fault || !game_application_remote_sky(a,&sky))return 0;
    }
    if(!game_level_world_render(a,get(a,0x6252f8)) || !game_level_colored_render(a,get(a,0x59626c)) ||
       !game_level_projected_render(a,PROJECTED_PLAIN) || !game_level_projected_render(a,PROJECTED_TEXTURED) || !game_level_projected_render(a,PROJECTED_ALPHA) || !game_level_frame_004d9570(a))return 0;
    if(selected){
        uint32_t source[4];GameAddress point=get(a,selected+0x1c);uint32_t w=(uint32_t)(int32_t)(int16_t)game_memory_read_u16(&a->memory,0x596100),h=(uint32_t)(int32_t)(int16_t)game_memory_read_u16(&a->memory,0x596102);
        uint32_t cx=w-(uint32_t)shift(truncated((long double)integer(w)*real(a,point)),17)-1,cy=h-(uint32_t)shift(truncated((long double)integer(h)*real(a,point+8)),17)-1,half=(uint32_t)shift(get(a,0x6024f8)-get(a,0x6024f0),1);
        source[0]=integer(cx-half)<0?0:cx-half;source[1]=integer(cy-half)<0?0:cy-half;source[2]=cx+half+1;source[3]=cy+half+1;if(w<=source[2])source[2]=w-1;if(h<=source[3])source[3]=h-1;
        put(a,0x6024f8,get(a,0x6024f8)+1);put(a,0x6024fc,get(a,0x6024fc)+1);
        if(!image(a,source,0x6024f0,1.0f))return 0;
        put(a,0x6024f8,get(a,0x6024f8)-1);put(a,0x6024fc,get(a,0x6024fc)-1);
    }
    if(!clip(a,get(a,0x602528)+1,get(a,0x602534)+1,get(a,0x602530)-1,get(a,0x60252c)-1) || !panel_draw(a,get(a,get(a,0x597f80)?0x60261c:0x602620)) ||
       !clip(a,0,0,get(a,0x5ac6a0),get(a,0x5ac6a4)) || !game_menu_projection_bounds(a,0,0,get(a,0x5ac6a0),get(a,0x5ac6a4)))return 0;
    store(a,0x6290d0,0);store(a,0x6290d4,0);store(a,0x6290dc,(float)integer(get(a,0x5ac6a0)-1));store(a,0x6290d8,(float)integer(get(a,0x5ac6a4)-1));
    if(!boost_lamps(a))return 0;
    game_memory_write_u8(&a->memory,0x59d448,246);
    if(!caption(a,get(a,0x6025f8)+2,get(a,0x602604)+(get(a,0x597f80)?2:0),get(a,0x597f80)?0x58f414:0x58f410,integer(get(a,get(a,0x5f0208)+0x22c))))return 0;
    game_memory_write_u8(&a->memory,0x59d448,110);
    if(!map_grid(a) || !map_coordinates(a) || !panel_draw(a,get(a,0x60259c)))return 0;
    {GameAddress subject=get(a,get(a,0x58f8a8)+0x40);uint32_t maximum,l=get(a,0x6025e0),r=get(a,0x6025e8),quarter=(uint32_t)shift(r-l,2),middle=(uint32_t)shift(r+l,1);float health;
     if(!game_level_scene_property(a,subject,get(a,get(a,subject)+0x44),&maximum))return 0;
     health=(float)((long double)integer(get(a,subject+0x58))/integer(maximum));
     if(!game_level_frame_004a3390(a,integer(middle-quarter),integer(get(a,0x6025e4)+1),integer(middle+quarter),integer(get(a,0x6025e4)+2),176,144,health))return 0;}
    if(!game_level_strategic_map_marks(a))return 0;
    {char bytes[256];snprintf(bytes,sizeof(bytes),"\013255%.1f",(double)real(a,0x58de84));if(!text(a,0,0,bytes) || !string(a,0x602638,bytes,sizeof(bytes)) || !text(a,0,6,bytes))return 0;}
    if(!game_level_strategic_map_units(a))return 0;
    {static const uint32_t triangles[2][6]={{0x43938000,0x42fc0000,0x438d8000,0x43110000,0x43998000,0x43110000},{0x4415c000,0x43a00000,0x4411c000,0x43ad0000,0x44198000,0x43ad0000}};float points[6];for(i=0;i<6;i++)points[i]=floating(triangles[get(a,0x597f80)!=0][i]);if(!game_level_frame_004b2170(a,points))return 0;}
    if(!map_positions(a) || !member_statistics(a) || !game_level_messages_update(a))return 0;
    game_memory_write_u8(&a->memory,0x59d448,246);
    if(!game_level_frame_00548c70(a) || !game_level_chat_frame(a))return 0;
    {uint32_t x=get(a,0x5ac9c8),y=get(a,0x5ac9cc);int active=get(a,0x6025b4) && integer(x)>=integer(get(a,0x6027b8)) && integer(x)<integer(get(a,0x6027c0)) && integer(y)>=integer(get(a,0x6027bc)) && integer(y)<integer(get(a,0x6027c4));
     if(!strategic_pointer(a,integer(x),integer(y),active?153:169,active))return 0;}
    if(!game_level_screen_present(a,1))return 0;
    if(get(a,0x5acbe4) && (!game_level_frame_004c6eb0(a) || !game_level_frame_00515f40(a,0)))return 0;
    word=get(a,0x58de8c)+(game_menu_milliseconds(a)-started);put(a,0x58de8c,word);put(a,0x58de88,get(a,0x58de88)+1);
    if(integer(word)>=1000){store(a,0x58de84,(float)((long double)integer(get(a,0x58de88))/integer(word)*real(a,0x574320)));put(a,0x58de88,0);put(a,0x58de8c,0);}
    word=get(a,0x5ac6ac);game_memory_write_u8(&a->memory,0x602608,(uint8_t)(word&16?word&15:15u-(word&15u)));
    return !a->memory.fault;
}
/*4133f0: the existing quad owns its vertices, UVs and material ID. Clipping
 *borrows the current frame arena; no host pointer is stored in a record. */
static int rotated_quad(GameApplication *a,GameAddress p,const float position[2]){
    uint16_t angle=game_memory_read_u16(&a->memory,get(a,0x58f890)+6)&2047u;
    long double c=real(a,0x6298a8+((angle+512u)&2047u)*4),s=real(a,0x6298a8+angle*4);
    long double half=real(a,0x571f74),wc=(long double)real(a,p+0xc0)*c*half,hc=(long double)real(a,p+0xc4)*c*half;
    long double ws=(long double)real(a,p+0xc0)*s*half,hs=(long double)real(a,p+0xc4)*s*half;
    float stored_wc=(float)wc,stored_hs=(float)hs,x[4],y[4],u,v,bounds[4];
    GameplayClipCursors cursors;GameplayClipStatus status;GameAddress tail=0,packet=p+0x98,head;unsigned i;
    if(!p || a->memory.fault)return 0;
    x[0]=(float)(((long double)stored_wc-stored_hs)+position[0]);y[0]=(float)((hc+ws)+position[1]);
    x[1]=(float)(((long double)stored_hs+stored_wc)+position[0]);y[1]=(float)((ws-hc)+position[1]);
    x[2]=(float)(((long double)stored_hs-stored_wc)+position[0]);y[2]=(float)(((long double)position[1]-hc)-ws);
    x[3]=(float)(((long double)position[0]-stored_hs)-stored_wc);y[3]=(float)((hc-ws)+position[1]);
    u=(float)(((long double)(int16_t)game_memory_read_u16(&a->memory,p+0x90)-real(a,0x571f90))*half);
    v=(float)(((long double)(int16_t)game_memory_read_u16(&a->memory,p+0x92)-real(a,0x571f90))*half);
    for(i=0;i<4;i++){store(a,p+i*16,x[i]);store(a,p+i*16+4,y[i]);store(a,p+i*16+8,0.5f);}
    store(a,p+0x50,u);store(a,p+0x54,v);store(a,p+0x58,u);
    put(a,p+0x5c,0);put(a,p+0x60,0);put(a,p+0x64,0);put(a,p+0x68,0);store(a,p+0x6c,v);
    put(a,packet,4);put(a,packet+4,p+0x40);put(a,packet+8,0x101);put(a,packet+12,0);put(a,packet+16,p+0x50);
    cursors.vertex=get(a,0x604378);cursors.packet=get(a,0x604270);cursors.uv=get(a,0x604480);cursors.scalar=0;
    bounds[0]=bounds[1]=0;bounds[2]=(float)((long double)integer(get(a,0x627928))-real(a,0x571f70));bounds[3]=(float)((long double)integer(get(a,0x62792c))-real(a,0x571f70));
    head=gameplay_clipper_screen_chain_state(&a->memory,packet,&tail,bounds,&cursors,&status);
    put(a,0x604378,cursors.vertex);put(a,0x604270,cursors.packet);put(a,0x604480,cursors.uv);
    if(a->memory.fault)return 0;
    if(!head)return 1;
    put(a,packet+8,get(a,packet+8)|4u);
    return game_level_world_render(a,head);
}
static int outline(GameApplication *a,uint32_t l,uint32_t t,uint32_t r,uint32_t b,uint8_t color){
    return line(a,l,t,r,t,color) && line(a,r,t,r,b,color) && line(a,r,b,l,b,color) && line(a,l,b,l,t,color);
}
/*4a1830: recolor the retained icon scratch in place, then draw selected/local
 *group marks and the controlled unit's rendered heading. */
int game_level_strategic_map_marks(GameApplication *a){
    int32_t count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);unsigned i,j,k;GameAddress selected,subject,camera;
    for(i=0;(int32_t)i<count;i++){
        GameAddress mark=get(a,0x602698)+i*12,group,owner,first;uint32_t width,height,source[4],destination[4],descriptor[2];int draw=1,ok;
        if(!get(a,mark+8))continue;
        group=get(a,get(a,0x639a70)+i*4);owner=get(a,group+0x54);if(!owner)continue;first=get(a,get(a,group+0x18));
        if(owner==get(a,0x5f0208) && (!first || integer(get(a,get(a,first+0x48)+0x12c))<640))draw=integer(get(a,0x5ac6ac))%16<12;
        if(!draw)continue;
        width=(uint32_t)(int32_t)(int16_t)game_memory_read_u16(&a->memory,0x602628);height=(uint32_t)(int32_t)(int16_t)game_memory_read_u16(&a->memory,0x60262a);
        if(integer(width)<0 || integer(height)<0 || (uint64_t)width*height>65536)return 0;
        source[0]=source[1]=0;source[2]=width;source[3]=height;
        destination[0]=get(a,mark)-(uint32_t)shift(width,1);destination[1]=get(a,mark+4)-(uint32_t)shift(height,1);
        destination[2]=destination[0]+width;destination[3]=destination[1]+height;
        for(j=0;j<width*height;j++){
            uint8_t pixel=game_memory_read_u8(&a->memory,get(a,0x60262c)+j);
            for(k=0;k<4;k++)if(pixel==game_memory_read_u8(&a->memory,0x594458+k)){
                game_memory_write_u8(&a->memory,get(a,0x60256c)+j,game_memory_read_u8(&a->memory,owner+12+k));break;
            }
        }
        descriptor[0]=get(a,0x602568);descriptor[1]=get(a,0x60256c);
        ok=game_level_frame_004d8b50(a,source,destination,descriptor,0.5f);if(!ok || a->memory.fault)return 0;
        count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
    }
    selected=get(a,0x6025a0);
    if(selected){
        uint32_t span=get(a,0x597f80)?16u:8u;GameAddress mark=get(a,0x602698)+get(a,selected)*12;
        uint8_t color=(uint8_t)(game_memory_read_u8(&a->memory,0x602608)-80u);
        if(!outline(a,get(a,mark)-span,get(a,mark+4)-span,get(a,mark)+span,get(a,mark+4)+span,color) ||
           !outline(a,get(a,0x6024f0),get(a,0x6024f4),get(a,0x6024f8)+2,get(a,0x6024fc)+2,color))return 0;
        if(get(a,selected+0x54)==get(a,0x5f0208)){
            game_memory_write_u8(&a->memory,0x59d448,246);
            if(!caption(a,get(a,0x602688)+2,get(a,0x602694)+(get(a,0x597f80)?2u:0u),0x58f460,integer(get(a,selected+0x14))))return 0;
        }
    }
    subject=get(a,get(a,0x5f0208)+0x448);camera=get(a,0x58f890);
    {uint32_t bx=get(a,subject+0x64),bz=get(a,subject+0x6c),x,y,l=get(a,0x6027b8),t=get(a,0x6027bc),r=get(a,0x6027c0),b=get(a,0x6027c4);uint16_t direction,saved;uint8_t kind;float position[2];int ok;
     /*The original's invalid-coordinate branch reads uninitialized locals.
      *Reject that malformed frame rather than retaining stack garbage. */
     if(bx>0x80000000u || integer(bx)>0x48000000 || bz>0x80000000u || integer(bz)>0x48000000)return 0;
     x=l+(uint32_t)shift(truncated((long double)integer(r-l)*floating(bx)),17);
     y=b-(uint32_t)shift(truncated((long double)integer(b-t)*floating(bz)),17)-1;
     x=r-x+l;position[0]=(float)integer(x);position[1]=(float)integer(y);
     direction=game_memory_read_u16(&a->memory,subject+0x78);kind=game_memory_read_u8(&a->memory,subject+12);
     if(kind==0x20)direction=(uint16_t)(direction+game_memory_read_u16(&a->memory,subject+0x26e));
     else if(kind==0x23)direction=(uint16_t)(direction+game_memory_read_u16(&a->memory,subject+0x1ba));
     saved=game_memory_read_u16(&a->memory,camera+6);game_memory_write_u16(&a->memory,camera+6,(uint16_t)(1024u-direction));
     ok=rotated_quad(a,get(a,0x602630),position);
     {GameMemory cleanup=a->memory;cleanup.fault=0;
      game_memory_write_u16(&cleanup,camera+6,saved);if(cleanup.fault)return 0;}
     if(!ok)return 0;}
    put(a,0x62970c,0);put(a,0x629708,20);put(a,0x58de90,(get(a,0x58de90)+1)&63u);return !a->memory.fault;
}
/*4a1290: action captions and the two actual palette-colored triangle buttons. */
int game_level_strategic_map_units(GameApplication *a){
    static const GameAddress buttons[3]={0x602624,0x6025b0,0x6025d8};unsigned i,j;char first[8],second[8];
    for(i=0;i<3;i++){
        GameAddress button=get(a,buttons[i]);uint32_t l=get(a,button+0xa8),b=get(a,button+0xac),r=get(a,button+0xb0),t=get(a,button+0xb4),w,extra=0,x,y;
        if(!rectangle(a,l,b+1,r+2,t-1,110))return 0;
        game_memory_write_u8(&a->memory,0x59d448,get(a,button+0x14)?246:105);
        if(!string(a,i==0?0x58f418:i==1?0x58f428:0x58f434,first,sizeof(first)))return 0;
        w=game_text_width_bytes(&a->memory,(const uint8_t *)first,strlen(first)+1);
        if(i==2){if(!string(a,0x58f430,second,sizeof(second)))return 0;extra=game_text_width_bytes(&a->memory,(const uint8_t *)second,strlen(second)+1)+2;}
        x=l+(uint32_t)shift(r-w-extra-l,1);y=t+(uint32_t)shift(b-t-5,1);
        if(!text(a,integer(x),integer(y),first))return 0;
        if(i==2 && !text(a,integer(get(a,0x62970c)+game_text_width_bytes(&a->memory,(const uint8_t *)second,strlen(second)+1)+2),integer(get(a,0x629708)),second))return 0;
    }
    for(i=0;i<2;i++){
        static const float coordinates[2][3][2]={{{118,437},{118,406},{101,421}},{{272,406},{272,437},{289,421}}};
        GameAddress color=get(a,get(a,i?0x6027e0:0x6026a0)+0x14)?0x5ad780:0x5ad560;GameRenderVertex vertices[3]={{0}};
        for(j=0;j<3;j++){vertices[j].x=coordinates[i][j][0];vertices[j].y=coordinates[i][j][1];vertices[j].oow=0.5f;
            vertices[j].r=game_memory_read_u8(&a->memory,color+2);vertices[j].g=game_memory_read_u8(&a->memory,color+1);vertices[j].b=game_memory_read_u8(&a->memory,color);}
        if(a->memory.fault || !game_application_surface_polygon(a,vertices,3))return 0;
    }
    return !a->memory.fault;
}
/*The roster's actual descriptor families borrow live health and catalog
 *maximums. Name IDs select the verified native constant getters for the eight
 *producer classes and the two articulated classes; no retail code is decoded
 *or called by shipping. */
int game_level_strategic_descriptor_name(GameApplication *a,GameAddress record,GameAddress *name){
    static const struct {GameAddress method,name;} names[]={
        {0x51ba40,0x59808c},{0x51bb20,0x598098},{0x51bbb0,0x5980a0},{0x51bc40,0x5980a4},
        {0x53d310,0x59d55c},{0x53d3e0,0x59d568},{0x53d4b0,0x59d574},{0x53d580,0x59d580},
        {0x439620,0x5840e0},{0x511130,0x597780}
    };
    GameAddress table=get(a,record),payload=get(a,record+0x48),method=get(a,table+0x1c);unsigned i;
    if(!payload || !name || a->memory.fault)return 0;
    if(method==0x53e4a0){*name=0x59d5dc;return !a->memory.fault;}
    if(method!=0x53f190 && method!=0x53fc60)return 0;
    method=get(a,get(a,payload)+(method==0x53f190?0x88:0xa0));
    for(i=0;i<sizeof(names)/sizeof(names[0]);i++)if(names[i].method==method){*name=names[i].name;return !a->memory.fault;}
    fprintf(stderr,"Unreconstructed strategic name getter %08x\n",(unsigned)method);return 0;
}
int game_level_strategic_descriptor_details(GameApplication *a,GameAddress record,
        GameAddress *name,uint32_t *health,uint32_t *maximum){
    GameAddress table=get(a,record),payload=get(a,record+0x48),h=get(a,table+0x20),m=get(a,table+0x24);
    if(!payload || !health || !maximum || a->memory.fault)return 0;
    if(h!=0x53e4b0 && h!=0x53f1a0 && h!=0x53fc70)return 0;
    if(m!=0x53e4c0 && m!=0x53f1b0 && m!=0x53fc80)return 0;
    *health=get(a,payload+0x58);
    return game_level_scene_property(a,payload,get(a,get(a,payload)+0x44),maximum) &&
        game_level_strategic_descriptor_name(a,record,name);
}
