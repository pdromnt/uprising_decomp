#include "level_world_surface.h"
#include "level_world_render.h"
#include "game_text_rasterizer.h"
#include "portable_crt.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint32_t word(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float number(GameApplication *a,GameAddress p){uint32_t bits=word(a,p);float value;memcpy(&value,&bits,4);return value;}
static int32_t hud_signed(uint32_t bits){int32_t value;memcpy(&value,&bits,4);return value;}
static int32_t hud_add(int32_t x,int32_t y){return hud_signed((uint32_t)x+(uint32_t)y);}
static int32_t hud_sub(int32_t x,int32_t y){return hud_signed((uint32_t)x-(uint32_t)y);}
static int32_t hud_shift(int32_t x,unsigned shift){return x>=0?x/(1<<shift):-1-(int32_t)((-(int64_t)x-1)/(1<<shift));}
static int32_t hud_integer(long double value){
    uint32_t bits=(uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);
    return hud_signed(bits);
}
extern int game_application_glide_line(GameApplication *,const int32_t[4],uint8_t);
static int hud_line(GameApplication *a,int32_t x,int32_t y,int32_t xx,int32_t yy,uint8_t color){
    int32_t line[4]={x,y,xx,yy};return game_application_glide_line(a,line,color);
}
extern int game_level_palette_polygon(GameApplication *,const float[][3],const uint8_t *,unsigned,uint32_t);
extern int game_application_palette_quad(GameApplication *,int32_t,int32_t,int32_t,int32_t,uint8_t);
extern int game_level_scene_property(GameApplication *,GameAddress,GameAddress,uint32_t *);
extern int game_level_frame_004aeab0(GameApplication *);
extern int game_level_frame_004d89e0(GameApplication *,GameAddress);
extern int game_level_frame_004d91f0(GameApplication *,GameAddress);
extern int game_level_frame_004d8b50(GameApplication *,const uint32_t[4],uint32_t[4],const uint32_t[2],float);
/*Remaining concrete HUD programs retain their actual arguments and remain
 *strong shipping dependencies until reconstructed. None dispatches a CPU. */
extern int game_level_frame_0043a030(GameApplication *,int32_t,int32_t,int32_t,uint8_t);
extern int game_level_frame_004b0ec0(GameApplication *,int32_t,int32_t,int32_t,int32_t,float,uint32_t,int32_t,uint8_t);
extern int game_level_frame_004b12c0(GameApplication *);
extern int game_level_frame_004b1510(GameApplication *);
extern int game_level_frame_004b2170(GameApplication *,const float[6]);
extern int game_level_frame_004add00(GameApplication *);
extern int game_level_frame_004b0a10(GameApplication *);
extern int game_level_screen_palette(GameApplication *,GameAddress);
extern int game_menu_projection_bounds(GameApplication *,uint32_t,uint32_t,uint32_t,uint32_t);
extern int game_level_messages_update(GameApplication *);
extern int game_level_frame_00532780(GameApplication *,uint32_t);
extern int game_level_frame_00532870(GameApplication *,int32_t,int32_t,GameAddress,uint32_t);
extern int game_level_frame_text_bytes(GameApplication *,int32_t,int32_t,const uint8_t *,size_t);
static void hud_put(GameApplication *a,GameAddress p,uint32_t bits){game_memory_write_u32(&a->memory,p,bits);}
static void hud_number(GameApplication *a,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);hud_put(a,p,bits);}
static int hud_read_text(GameApplication *a,GameAddress p,uint8_t *bytes,size_t size,size_t *length){
    size_t i;for(i=0;i<size;i++){bytes[i]=game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!bytes[i]){*length=i;return !a->memory.fault;}}
    return 0;
}
static int hud_center_text(GameApplication *a,const uint8_t *bytes,size_t length,const int32_t bounds[4]){
    int32_t width=hud_signed(game_text_width_bytes(&a->memory,bytes,length));
    int32_t x=hud_add(hud_add(bounds[0],hud_shift(hud_sub(hud_sub(bounds[2],bounds[0]),width),1)),1);
    int32_t y=hud_add(bounds[1],hud_shift(hud_sub(hud_sub(bounds[3],bounds[1]),5),1));
    return !a->memory.fault && game_level_frame_text_bytes(a,x,y,bytes,length);
}
extern uint32_t game_menu_milliseconds(GameApplication *);
/*548c70: consume the timed message published by548ab0. The message record
 *remains authoritative; scratch text and geometry have only frame lifetime. */
int game_level_frame_00548c70(GameApplication *a){
    uint32_t tick=(game_menu_milliseconds(a)-word(a,0x5ac90c))>>5,end=word(a,0x639950);
    if(word(a,0x63993c)){
        uint32_t start=word(a,0x639938);long double span=(long double)hud_signed(end-start)*number(a,0x5773cc);
        float limit=number(a,0x5773d0),duration,factor;int show=0;
        int32_t left=hud_signed(word(a,0x639958)),top=hud_signed(word(a,0x63995c));
        int32_t right=hud_signed(word(a,0x639960)),bottom=hud_signed(word(a,0x639964));
        int32_t cx=hud_shift(hud_add(right,left),1),cy=hud_shift(hud_add(bottom,top),1),w,h;
        uint8_t bytes[256],shade;size_t length=0,at=0;unsigned line=0;
        duration=(float)(span<limit?span:limit);
        if((long double)start+duration>(float)tick){long double ratio=(long double)hud_signed(tick-start)/duration;factor=(float)(ratio*ratio);}
        else if((long double)end-duration<(float)tick){long double ratio=(long double)hud_signed(end-tick)/duration;factor=(float)(ratio*ratio);}
        else{factor=1;show=1;}
        w=hud_integer((long double)hud_sub(right,left)*factor);h=hud_integer((long double)hud_sub(bottom,top)*factor);
        left=hud_sub(cx,hud_shift(w,1));right=hud_add(cx,hud_shift(hud_add(w,1),1));
        top=hud_sub(cy,hud_shift(h,1));bottom=hud_add(cy,hud_shift(hud_add(h,1),1));
        shade=(uint8_t)hud_integer(((long double)number(a,0x5773d4)-factor)*number(a,0x5773d8));
        game_memory_write_u8(&a->memory,0x639830,shade);
        hud_put(a,0x639940,(uint32_t)left);hud_put(a,0x639944,(uint32_t)top);
        hud_put(a,0x639948,(uint32_t)right);hud_put(a,0x63994c,(uint32_t)bottom);
        if(word(a,0x5840f0)){
            if(!hud_line(a,hud_sub(left,1),hud_sub(top,1),hud_sub(left,1),bottom,5) ||
               !hud_line(a,hud_sub(left,1),hud_add(bottom,1),right,hud_add(bottom,1),5) ||
               !hud_line(a,left,hud_sub(top,1),hud_add(right,1),hud_sub(top,1),13) ||
               !hud_line(a,hud_add(right,1),top,hud_add(right,1),hud_add(bottom,1),13))return 0;
            if(shade){
                float offset=number(a,0x574d1c),p[4][3];uint8_t colors[4]={shade,shade,shade,shade};unsigned i;
                /*4e97d0 defines only opposite palette corners. Define its
                 *uninitialized other corners consistently; that lane parity
                 *is uncertain, rather than retaining guest-stack garbage. */
                p[0][0]=p[3][0]=(float)((long double)hud_sub(left,1)+offset);
                p[1][0]=p[2][0]=(float)((long double)hud_sub(right,1)+offset);
                p[0][1]=p[1][1]=(float)((long double)hud_sub(top,1)+offset);
                p[2][1]=p[3][1]=(float)((long double)hud_sub(bottom,1)+offset);
                for(i=0;i<4;i++)p[i][2]=.25f;
                if(!game_level_palette_polygon(a,(const float (*)[3])p,colors,4,0x800))return 0;
            }else if(!game_application_palette_quad(a,hud_sub(left,1),hud_sub(top,1),hud_sub(right,1),hud_sub(bottom,1),0))return 0;
        }
        /*strncpy's forced final NUL and strtok's skipped empty lines. */
        if(show)for(length=0;length<255;length++){bytes[length]=game_memory_read_u8(&a->memory,0x639838+(uint32_t)length);if(!bytes[length])break;}
        bytes[length]=0;
        hud_put(a,0x59d46c,0);
        while(at<length && bytes[at]){
            size_t begin,count;while(at<length && bytes[at]=='\n')++at;begin=at;
            while(at<length && bytes[at]!='\n')++at;count=at-begin;if(!count)break;
            bytes[at]=0;
            if(!game_level_frame_text_bytes(a,hud_add(hud_signed(word(a,0x639958)),1),
                hud_add(hud_signed(word(a,0x63995c)),(int32_t)(line*6+1)),bytes+begin,count+1)){
                hud_put(a,0x59d46c,1);return 0;
            }
            ++line;++at;
        }
        hud_put(a,0x59d46c,1);
    }
    if(tick>end)hud_put(a,0x63993c,0);
    return !a->memory.fault;
}
static int hud_clip(GameApplication *a,int32_t left,int32_t top,int32_t right,int32_t bottom){
    int32_t values[4]={left,top,right,bottom};unsigned i;
    for(i=0;i<4;i++){
        int32_t hi=hud_signed(word(a,0x597f68+(i%2)*4)),lo=hud_signed(word(a,0x597f70+(i%2)*4));
        if(values[i]>hi)values[i]=hi;
        if(values[i]<lo)values[i]=lo;
        hud_put(a,0x597f50+i*4,(uint32_t)values[i]);
    }
    return !a->memory.fault;
}
static int hud_health(GameApplication *a,GameAddress object,float *fraction,uint32_t *maximum){
    if(!game_level_scene_property(a,object,word(a,word(a,object)+0x44),maximum))return 0;
    *fraction=(float)((long double)hud_signed(word(a,object+0x58))/hud_signed(*maximum));return !a->memory.fault;
}
static int hud_sprite_image(GameApplication *a,GameAddress destination,GameAddress source){
    uint8_t image[8];return game_memory_read(&a->memory,source+4,image,8) && game_memory_write(&a->memory,destination+4,image,8);
}
static int hud_gradient_rect(GameApplication *a,int32_t left,int32_t top,int32_t right,int32_t bottom,uint8_t first,uint8_t last){
    float points[4][3]={{(float)left,(float)top,0.5f},{(float)((long double)right+number(a,0x574d1c)),(float)top,0.5f},
        {(float)((long double)right+number(a,0x574d1c)),(float)((long double)bottom+number(a,0x574d1c)),0.5f},
        {(float)left,(float)((long double)bottom+number(a,0x574d1c)),0.5f}};
    /*Original4e9700 also leaves color lane1 undefined; use its right gradient. */
    uint8_t colors[4]={first,last,last,first};
    return !a->memory.fault && game_level_palette_polygon(a,(const float (*)[3])points,colors,4,0);
}
static int hud_health_caption(GameApplication *a,GameAddress object,const int32_t box[4],uint8_t pulse,GameAddress label){
    float fraction;uint32_t maximum;uint8_t bytes[32];size_t length;int32_t split;
    if(!hud_health(a,object,&fraction,&maximum))return 0;
    if(fraction==number(a,0x5744f4)){
        if(!game_application_palette_quad(a,box[0],box[1],box[2],box[3],247))return 0;
    }else{
        split=hud_add(box[0],hud_integer((long double)hud_sub(box[2],box[0])*fraction));
        if(!hud_gradient_rect(a,box[0],box[1],split,box[3],176,(uint8_t)(hud_integer((long double)fraction*number(a,0x574600))-80)) ||
           !game_application_palette_quad(a,hud_add(split,1),box[1],box[2],box[3],pulse))return 0;
    }
    if(!hud_read_text(a,label,bytes,sizeof(bytes),&length))return 0;
    {int32_t width=hud_signed(game_text_width_bytes(&a->memory,bytes,length+1));
     int32_t x=hud_shift(hud_add(hud_add(hud_sub(box[2],width),box[0]),1),1);
     int32_t y=hud_shift(hud_sub(hud_add(box[1],box[3]),4),1);
     return !a->memory.fault && game_level_frame_text_bytes(a,x,y,bytes,length+1);}
}
static int hud_direction_labels(GameApplication *a,GameAddress pose){
    uint8_t pitch[2],yaw[32];int n;int32_t bounds[4];unsigned i;
    pitch[0]=(uint8_t)(hud_shift(hud_integer(((long double)number(a,0x5745c0)-number(a,pose+0x18))*number(a,0x5745c4)),17)+97);pitch[1]=0;
    for(i=0;i<4;i++)bounds[i]=hud_signed(word(a,0x603748+i*4));
    if(!hud_center_text(a,pitch,sizeof(pitch),bounds))return 0;
    n=snprintf((char *)yaw,sizeof(yaw),"%d",hud_add(hud_shift(hud_integer((long double)number(a,pose+0x20)*number(a,0x5745c4)),17),1));
    for(i=0;i<4;i++)bounds[i]=hud_signed(word(a,0x603918+i*4));
    return n>=0 && (size_t)n<sizeof(yaw) && hud_center_text(a,yaw,(size_t)n+1,bounds);
}
static int hud_heading_indicator(GameApplication *a,GameAddress player,int large){
    int32_t x=large?540:260,y=large?60:20,radius=large?24:16;long double vx=number(a,player+0x19c),vz=number(a,player+0x1a4),speed=number(a,player+0x128);
    long double heading=((long double)number(a,0x5744e4)-(vx*vx+vz*vz)/(speed*speed))*number(a,0x5745e8),current=number(a,0x58fe14);float target;unsigned i;
    static const GameAddress spokes[2][2]={{0x629bd8,0x62a3d8},{0x62b574,0x629d74}};
    if(heading<number(a,0x5744f4))heading=(long double)(int32_t)portable_crt_rand_step(&a->memory)*number(a,0x5745ec)*number(a,0x5745f0);
    target=(float)heading;
    if(current>target){current-=number(a,0x5745f4);if(current<target)current=target;}
    else{current+=number(a,0x5745f4);if(current>target)current=target;}
    hud_number(a,0x58fe14,(float)current);
    if(!game_level_frame_0043a030(a,x,y,radius,255) || !game_level_frame_0043a030(a,x,y,1,153))return 0;
    {uint16_t angle=(uint16_t)hud_integer((long double)number(a,0x58fe14)+number(a,0x5745f8));
     long double xx=(long double)radius*number(a,0x6298a8+(angle&2047)*4)*number(a,0x5745fc);
     long double yy=(long double)radius*number(a,0x6298a8+((angle+512)&2047)*4)*number(a,0x5745fc);
     if(!hud_line(a,x,y,hud_add(x,hud_integer(xx)),hud_add(y,hud_integer(yy)),153))return 0;}
    for(i=0;i<2;i++){
        long double xx=(long double)number(a,spokes[i][0])*radius,yy=(long double)number(a,spokes[i][1])*radius;
        if(!hud_line(a,hud_add(x,hud_integer(xx*number(a,0x5745fc))),hud_add(y,hud_integer(yy*number(a,0x5745fc))),
            hud_add(x,hud_integer(xx)),hud_add(y,hud_integer(yy)),255))return 0;
    }
    return !a->memory.fault;
}
/*4afcc0: actual primary/alternate HUD, weapon image selection, messages,
 *health displays, local direction/banner strings and original RNG consumers.
 *All callbacks have ordinary native signatures; unresolved widgets block the
 *shipping link rather than dropping their behavior or re-entering a CPU. */
int game_level_frame_004afcc0(GameApplication *a){
    const char *stage="palette/projection setup";
    uint32_t maximum,tick,primary,large;GameAddress root,player,pose;float fraction;
    uint8_t pulse,bytes[128];size_t length;int ok=0,complete=0;
    if(!word(a,0x597f88))return !a->memory.fault;
    if(!game_level_screen_palette(a,0x605460) || !game_menu_projection_bounds(a,0,0,word(a,0x5ac6a0),word(a,0x5ac6a4)) ||
       !game_level_frame_004aeab0(a)){
        game_application_log(a,"native HUD failed: %s\n",stage);return 0;
    }
    root=word(a,0x5f0208);player=word(a,root+0x3bc);pose=word(a,root+0x448);
    primary=!word(a,0x6038a0);large=word(a,0x597f80);tick=word(a,0x5ac6ac);
    pulse=(uint8_t)(((tick&16)?tick&15:15-(tick&15))-112);
    /* Both views own a keyed full-screen frame. The marker/list calls below
     * only repaint small regions; they cannot restore the citadel artwork. */
    {uint32_t rectangle[4]={0,0,640,480};
     uint32_t image[2]={640u|(480u<<16),word(a,primary?0x5960e8:0x5960ec)};
     stage="HUD background";
     if(!game_application_world_state(a,WORLD_DEPTH_WRITE,1) ||
        !game_level_frame_004d8b50(a,rectangle,rectangle,image,1))goto restore_palette;}
    if(primary){
        int32_t radius=hud_signed(word(a,0x6037e8)),cx=hud_signed(word(a,0x604080)),cy=hud_signed(word(a,0x60407c));
        uint32_t saved[4];unsigned i;static const GameAddress bounds[4]={0x6290d4,0x6290d0,0x6290dc,0x6290d8};
        static const GameAddress publications[4]={0x603814,0x6037f8,0x60372c,0x603f74};
        stage="primary background";
        if(!game_level_frame_004d89e0(a,word(a,0x5960e8)) || !hud_read_text(a,0x590820,bytes,33,&length))goto restore_palette;
        {int32_t width=hud_signed(game_text_width_bytes(&a->memory,bytes,length+1)),left=hud_shift(hud_sub(hud_signed(word(a,0x5ac6a0)),width),1);
         uint32_t rect[4]={(uint32_t)left,word(a,0x5ac6a4)-6,(uint32_t)hud_add(hud_add(left,width),1),word(a,0x5ac6a4)};
         uint32_t image[2]={640u|(480u<<16),word(a,0x5960e8)};
         if(!game_level_frame_004d8b50(a,rect,rect,image,1))goto restore_palette;}
        stage="heading/health/minimap";
        if(word(a,0x594d38) && !hud_heading_indicator(a,player,large!=0))goto restore_palette;
        if(!game_application_palette_quad(a,hud_sub(cx,radius),hud_sub(cy,radius),hud_add(cx,radius),hud_add(cy,radius),pulse))goto restore_palette;
        for(i=0;i<4;i++){saved[i]=word(a,bounds[i]);hud_put(a,publications[i],saved[i]);}
        hud_number(a,bounds[0],0);hud_number(a,bounds[1],0);
        hud_number(a,bounds[2],(float)((long double)hud_signed(word(a,0x627928))-number(a,0x5744e4)));
        hud_number(a,bounds[3],(float)((long double)hud_signed(word(a,0x62792c))-number(a,0x5744e4)));
        ok=hud_clip(a,0,0,hud_integer(number(a,bounds[2])),hud_integer(number(a,bounds[3]))) && hud_health(a,player,&fraction,&maximum) &&
            game_level_frame_004b0ec0(a,cx,cy,hud_sub(radius,12),hud_add(radius,2),fraction,512,142,176);
        for(i=0;i<4;i++)hud_put(a,bounds[i],saved[i]);
        if(!hud_clip(a,hud_integer(number(a,bounds[0])),hud_integer(number(a,bounds[1])),hud_integer(number(a,bounds[2])),hud_integer(number(a,bounds[3]))) || !ok)goto restore_palette;
        stage="primary instruments";
        if(!game_level_frame_004d91f0(a,0x603830) || !game_application_palette_quad(a,1,344,72,396,97) ||
           !game_level_frame_004b12c0(a) || !game_level_frame_004b1510(a) ||
           !game_application_palette_quad(a,258,438,271,444,97) || !game_application_palette_quad(a,373,438,386,444,97))goto restore_palette;
        stage="weapon instruments";
        for(i=0;i<2;i++){
            uint32_t weapon=word(a,player+0x214+i*4);GameAddress template;
            if(weapon>4096)goto restore_palette;
            template=(large?0x6034d8:0x603348)+weapon*20;
            if(!hud_sprite_image(a,0x603490+i*20,template))goto restore_palette;
            hud_put(a,0x603320+i*4,word(a,(large?0x604028:0x603638)+weapon*4));
        }
        if(!game_level_frame_00532780(a,246))goto restore_palette;
        stage="ammo text";
        for(i=0;i<2;i++){
            GameAddress ammo=word(a,player+0x1cc+i*4);
            if(!game_level_frame_00532870(a,hud_signed(word(a,0x6032f8+i*8)),hud_signed(word(a,0x6032fc+i*8)),0x590618+i*4,word(a,ammo+12)))goto restore_palette;
        }
        stage="weapon sprites";
        if(!game_level_frame_004d91f0(a,0x603490) || !game_level_frame_004d91f0(a,0x6034a4))goto restore_palette;
        stage="message instruments";
        if(!game_application_palette_quad(a,471,342,632,362,97) || !game_level_messages_update(a) ||
           !game_application_palette_quad(a,588,405,599,419,97) || !game_application_palette_quad(a,604,405,615,419,97) ||
           !game_application_palette_quad(a,584,376,628,385,97) || !game_application_palette_quad(a,177,23,463,39,97))goto restore_palette;
        if(!word(a,0x6038a0) && word(a,0x604760)!=5 && word(a,0x604760)!=6){float triangle[6];
            for(i=0;i<6;i++)triangle[i]=number(a,0x6040a8+i*4);
            if(!game_level_frame_004b2170(a,triangle))goto restore_palette;
        }
        {uint32_t icon=word(a,root+0x604);if(icon>4096 || !hud_sprite_image(a,0x6038c0,(large?0x603d18:0x603e40)+icon*20) ||
            !game_level_frame_004d91f0(a,0x6038c0))goto restore_palette;}
        stage="direction labels";
        hud_put(a,0x59d46c,0);ok=hud_direction_labels(a,pose);hud_put(a,0x59d46c,1);
        if(!ok)goto restore_palette;
    }else{
        uint32_t saved=word(a,0x6290d8);int32_t citadel[4]={large?182:227,large?445:183,large?280:278,large?451:189};
        int32_t wraith[4]={large?360:44,large?445:183,large?458:95,large?451:189};
        if(!game_level_frame_004d89e0(a,word(a,0x5960ec)) || !game_level_frame_004d91f0(a,0x603620))goto restore_palette;
        hud_number(a,0x6290d8,(float)hud_sub(hud_signed(word(a,0x62792c)),1));
        ok=game_level_frame_00532780(a,110) && hud_health_caption(a,pose,citadel,pulse,0x590638) &&
            hud_health_caption(a,player,wraith,pulse,0x590648);
        hud_put(a,0x6290d8,saved);if(!ok)goto restore_palette;
    }
    if(!game_level_frame_004add00(a) || !hud_health(a,player,&fraction,&maximum))goto restore_palette;
    {uint32_t color=246;
     if((long double)hud_signed(word(a,player+0x58))<(long double)hud_signed(maximum)*number(a,0x574604) &&
        (portable_crt_rand_step(&a->memory)*3u)/32768u==0)color=0;
     if(!game_level_frame_00532780(a,color) || !game_level_frame_00532870(a,hud_signed(word(a,0x603328)),hud_signed(word(a,0x60332c)),0x590818,word(a,root+0x22c)) ||
        !game_level_frame_004b0a10(a) || !game_level_frame_004d91f0(a,0x6032e0))goto restore_palette;}
    stage="footer";
    {int32_t x=(int16_t)game_memory_read_u16(&a->memory,0x6032e0),y=(int16_t)game_memory_read_u16(&a->memory,0x6032e2);
     int32_t w=(int16_t)game_memory_read_u16(&a->memory,0x6032e4),h=(int16_t)game_memory_read_u16(&a->memory,0x6032e6);
     if(!game_level_frame_00532870(a,hud_add(hud_add(x,w),1),hud_add(y,hud_shift(hud_sub(h,5),1)),0x590844,word(a,root+0x588)))goto restore_palette;}
    {GameAddress parts[4]={word(a,0x594d40)?0x590870:0x59087c,word(a,0x594d3c)?0x590860:0x59086c,
        word(a,0x594d38)==2?0x590854:0x59085c,word(a,0x594d38)?0x590848:0x590850};unsigned i;size_t used=1;
     bytes[0]=12;
     for(i=0;i<4;i++){if(!hud_read_text(a,parts[i],bytes+used,sizeof(bytes)-used,&length))goto restore_palette;used+=length;}
     bytes[used]=0;
     {int32_t width=hud_signed(game_text_width_bytes(&a->memory,bytes,used+1));
      complete=game_level_frame_text_bytes(a,hud_shift(hud_sub(hud_signed(word(a,0x5ac6a0)),width),1),hud_sub(hud_signed(word(a,0x5ac6a4)),6),bytes,used+1);}
    }
restore_palette:
    if(!complete){
        char detail[160];
        snprintf(detail,sizeof(detail),"%s; weapons=%08x/%08x ammo=%08x/%08x",stage,
            (unsigned)word(a,player+0x214),(unsigned)word(a,player+0x218),
            (unsigned)word(a,player+0x1cc),(unsigned)word(a,player+0x1d0));
        game_application_log(a,"native HUD failed: %s\n",detail);
    }
    if(!game_level_screen_palette(a,0x5ad3a8))complete=0;
    return complete && !a->memory.fault;
}
