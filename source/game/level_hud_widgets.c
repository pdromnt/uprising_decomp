#include "level_world_surface.h"
#include "software_raster.h"
#include "game_text_rasterizer.h"
#include "radar_compositor.h"
#include "scene_projection_storage.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
extern int game_application_palette_lines(GameApplication *,const int32_t[][4],unsigned,uint8_t);
extern int game_level_palette_polygon(GameApplication *,const float[][3],const uint8_t *,unsigned,uint32_t);
static uint32_t widget_word(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static int32_t widget_signed(uint32_t bits){int32_t value;memcpy(&value,&bits,4);return value;}
static float widget_number(GameApplication *a,GameAddress p){uint32_t bits=widget_word(a,p);float value;memcpy(&value,&bits,4);return value;}
static int32_t widget_add(int32_t x,int32_t y){return widget_signed((uint32_t)x+(uint32_t)y);}
static int32_t widget_integer(long double value){
    return widget_signed((uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value));
}
static int32_t widget_short(uint32_t value){value&=65535;return value<32768?(int32_t)value:(int32_t)value-65536;}
static float widget_sine(GameApplication *a,uint32_t angle){return widget_number(a,0x6298a8+(angle&2047)*4);}
/*43a030:32-segment circle, including the original clipped endpoint carried
 *into the next segment. All geometry is local; the SDK state is set once. */
int game_level_frame_0043a030(GameApplication *a,int32_t x,int32_t y,int32_t radius,uint8_t color){
    int32_t lines[32][4],bounds[4],previous[2]={widget_add(x,radius),y};unsigned i,count=0;
    float r=(float)radius;
    for(i=0;i<4;i++)bounds[i]=widget_signed(widget_word(a,0x597f50+i*4));
    for(i=1;i<=32;i++){
        int32_t line[4]={previous[0],previous[1],0,0};
        if(i==32){line[2]=widget_add(x,radius);line[3]=y;}
        else{line[2]=widget_add(x,widget_integer((long double)widget_sine(a,i*64+512)*r));
            line[3]=widget_add(y,widget_integer((long double)widget_sine(a,i*64)*r));}
        if(a->memory.fault)return 0;
        if(software_raster_clip_line(line,bounds))memcpy(lines[count++],line,sizeof(line));
        previous[0]=line[2];previous[1]=line[3];
    }
    return game_application_palette_lines(a,(const int32_t (*)[4])lines,count,color);
}
static int widget_ring_quad(GameApplication *a,int32_t x,int32_t y,int32_t inner,int32_t outer,
    float begin,float end,uint8_t first,uint8_t last){
    uint32_t angles[2]={(uint32_t)widget_integer(begin),(uint32_t)widget_integer(end)};
    float positions[4][3];uint8_t colors[4]={first,last,last,first};unsigned i;
    for(i=0;i<4;i++){
        uint32_t angle=angles[(i==1 || i==2)?1:0];float radius=(float)(i<2?inner:outer);
        positions[i][0]=(float)((long double)widget_sine(a,angle+512)*radius+(float)x);
        positions[i][1]=(float)((float)y-(long double)widget_sine(a,angle)*radius);positions[i][2]=0.5f;
    }
    return !a->memory.fault && game_level_palette_polygon(a,(const float (*)[3])positions,colors,4,0);
}
/*4b0ec0: signed16 start/gap angles, full gradient slices and the original
 *fractional tail. The tail endpoint excludes gap/2; retain that reversal rather
 *than replacing it with a mathematically tidier ring.518880 only reads a word. */
int game_level_frame_004b0ec0(GameApplication *a,int32_t x,int32_t y,int32_t inner,int32_t outer,
    float fraction,uint32_t start,int32_t gap,uint8_t color){
    int32_t signed_gap=widget_short((uint32_t)gap),half=signed_gap>=0?signed_gap/2:-1-(int32_t)((-(int64_t)signed_gap-1)/2);
    int32_t sweep=2048-signed_gap,segments=widget_integer((long double)fraction*widget_number(a,0x5744f0));
    float angle=(float)(widget_short(start)+half),step=(float)((long double)sweep*widget_number(a,0x57460c)),shade=(float)color;
    uint32_t fraction_bits;int32_t i;
    (void)widget_word(a,0x597f78);
    if(a->memory.fault || segments>65536)return 0;
    for(i=0;i<segments;i++){
        float next=(float)((long double)angle+step),next_shade=(float)((long double)shade+widget_number(a,0x574610));
        if(!widget_ring_quad(a,x,y,inner,outer,angle,next,(uint8_t)widget_integer(shade),(uint8_t)widget_integer(next_shade)))return 0;
        angle=next;shade=next_shade;
    }
    memcpy(&fraction_bits,&fraction,4);
    if(widget_signed(fraction_bits)<0x3f800000){
        int32_t end=widget_short((uint32_t)widget_integer((long double)(float)sweep*fraction)+start);
        uint8_t last=(uint8_t)(color+(uint8_t)widget_integer((long double)fraction*widget_number(a,0x574600)));
        if(!widget_ring_quad(a,x,y,inner,outer,angle,(float)end,(uint8_t)widget_integer(shade),last))return 0;
    }
    return !a->memory.fault;
}
extern int game_level_frame_004d91f0(GameApplication *,GameAddress);
extern int game_level_frame_text_bytes(GameApplication *,int32_t,int32_t,const uint8_t *,size_t);
extern int game_level_frame_00532780(GameApplication *,uint32_t);
extern int game_level_frame_00532870(GameApplication *,int32_t,int32_t,GameAddress,uint32_t);
extern int game_level_player_catalog_choice(GameApplication *,GameAddress,uint8_t,uint32_t *);
extern int game_application_palette_quad(GameApplication *,int32_t,int32_t,int32_t,int32_t,uint8_t);
/*4b12c0 is the target category icon, not a compass. Replace its jump tables
 *with ordinary kind values.4b38a0, its unknown-kind diagnostic, is a literal ret. */
int game_level_frame_004b12c0(GameApplication *a){
    GameAddress target=widget_word(a,0x60387c),source;uint8_t image[8];unsigned icon=8;
    if(!target)return !a->memory.fault;
    switch(game_memory_read_u8(&a->memory,target+12)){
    case 0x20:case 0x21:case 0x63:case 0x81:icon=6;break;
    case 0x22:case 0x25:icon=10;break;case 0x23:icon=9;break;case 0x24:icon=12;break;
    case 0x26:case 0x64:case 0x83:icon=4;break;case 0x27:case 0x62:case 0x82:icon=0;break;
    case 0x28:icon=11;break;case 0x29:icon=14;break;case 0x40:case 0x60:case 0x80:icon=2;break;
    case 0x77:{uint32_t subtype=widget_word(a,target+0xa0);if(subtype>3)return 0;icon=subtype<2?8:subtype==2?15:16;break;}
    case 0x84:icon=3;break;case 0x85:icon=7;break;case 0x86:icon=1;break;case 0x87:icon=5;break;case 0x88:icon=13;break;
    default:break;
    }
    source=(widget_word(a,0x597f80)?0x603a48:0x603ba0)+icon*20;
    return !a->memory.fault && game_memory_read(&a->memory,source+4,image,8) &&
        game_memory_write(&a->memory,0x604014,image,8) && game_level_frame_004d91f0(a,0x604010);
}
static int widget_text(GameApplication *a,int32_t x,int32_t y,GameAddress source){
    uint8_t bytes[128];unsigned i;
    for(i=0;i<sizeof(bytes);i++){bytes[i]=game_memory_read_u8(&a->memory,source+i);
        if(!bytes[i])return !a->memory.fault && game_level_frame_text_bytes(a,x,y,bytes,i+1);}
    return 0;
}
static GameAddress widget_target_name(GameApplication *a,GameAddress target,uint8_t kind){
    static const GameAddress units[10]={0x59069c,0x5906a4,0x590754,0x590760,0x590784,0x59076c,0x590694,0x590684,0x590778,0x590790};
    static const GameAddress structures[10]={0x5906dc,0x590724,0x5906b8,0x590700,0x5906e8,0x590730,0x5906c4,0x59070c,0x590748,0x59073c};
    static const GameAddress subtypes[4]={0x590798,0x5907a0,0x5907a8,0x5907b4};
    if(kind>=0x20 && kind<=0x29)return units[kind-0x20];
    if(kind>=0x80 && kind<=0x89)return structures[kind-0x80];
    switch(kind){
    case 0x40:return 0x590688;case 0x60:return 0x5906d0;case 0x62:return 0x5906ac;
    case 0x63:return 0x590718;case 0x64:return 0x5906f4;
    case 0x77:{uint32_t subtype=widget_word(a,target+0xa0);return subtype<4?subtypes[subtype]:0x5907c0;}
    default:return 0x5907cc;
    }
}
/*4b1510: catalog tier, target name, faction/name and its color strip. The
 *existing catalog owner supplies the ordinary slot50 query. No stack sprintf,
 *serialized scratch or guest callback is needed for these literal names. */
int game_level_frame_004b1510(GameApplication *a){
    GameAddress target=widget_word(a,0x60387c),catalog,name,faction_name;uint32_t tier,faction;uint8_t kind;
    int32_t x,y;int large;
    if(!target)return !a->memory.fault;
    kind=game_memory_read_u8(&a->memory,target+12);catalog=widget_word(a,0x5f0210+(widget_word(a,target+0x14)&63)*4);
    if(!game_level_player_catalog_choice(a,catalog,kind,&tier) || !game_level_frame_00532780(a,246))return 0;
    name=widget_target_name(a,target,kind);large=widget_word(a,0x597f80)!=0;
    x=widget_short(game_memory_read_u16(&a->memory,0x604010));y=widget_short(game_memory_read_u16(&a->memory,0x604012));
    faction=game_memory_read_u16(&a->memory,catalog+8);
    faction_name=faction==5?(large?0x590804:0x5907e4):faction==4?(large?0x59080c:0x5907ec):catalog+0x39c;
    if(!widget_text(a,widget_add(x,23),widget_add(y,4),large?0x5907f8:0x5907d8) ||
       !game_level_frame_00532870(a,widget_add(x,40),widget_add(y,4),large?0x590800:0x5907e0,tier+1) ||
       !widget_text(a,widget_add(x,3),widget_add(y,20),name) || !widget_text(a,widget_add(x,3),widget_add(y,27),faction_name))return 0;
    return !a->memory.fault && game_application_palette_quad(a,widget_add(x,26),widget_add(y,15),widget_add(x,43),widget_add(y,16),
        game_memory_read_u8(&a->memory,catalog+12));
}
static void widget_put(GameApplication *a,GameAddress p,uint32_t value){game_memory_write_u32(&a->memory,p,value);}
static void widget_float(GameApplication *a,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);widget_put(a,p,bits);}
static int widget_clip(GameApplication *a,const float box[4]){
    unsigned i;
    for(i=0;i<4;i++){
        int32_t value=widget_integer(box[i]),hi=widget_signed(widget_word(a,0x597f68+(i%2)*4)),
            lo=widget_signed(widget_word(a,0x597f70+(i%2)*4));
        if(value>hi)value=hi;
        if(value<lo)value=lo;
        widget_put(a,0x597f50+i*4,(uint32_t)value);
    }
    return !a->memory.fault;
}
/*4b2170: strategic allocation triangle and its exclusive-state caption.
 *The existing allocation/viewport records remain authoritative. No temporary
 *polygon packets or guest variadic text are needed. */
int game_level_frame_004b2170(GameApplication *a,const float triangle[6]){
    static const GameAddress gates[3]={0x604114,0x6040fc,0x6040a4};
    static const GameAddress saved[4]={0x603814,0x6037f8,0x60372c,0x603f74};
    static const GameAddress viewport[4]={0x6290d4,0x6290d0,0x6290dc,0x6290d8};
    static const unsigned indices[3][3]={{0,3,1},{1,3,2},{2,3,0}};
    float box[4],original[4],points[4][3];unsigned i,j;int ok=0;
    int32_t pulse=0;int special;
    if(!triangle)return 0;
    for(i=0;i<4;i++){original[i]=widget_number(a,viewport[i]);
        widget_float(a,saved[i],original[i]);}
    box[0]=box[1]=0;
    box[2]=(float)((long double)widget_signed(widget_word(a,0x627928))-widget_number(a,0x5744e4));
    box[3]=(float)((long double)widget_signed(widget_word(a,0x62792c))-widget_number(a,0x5744e4));
    widget_float(a,0x6290d4,box[0]);widget_float(a,0x6290d0,box[1]);
    widget_float(a,0x6290dc,box[2]);widget_float(a,0x6290d8,box[3]);
    if(!widget_clip(a,box))goto restore;
    for(i=0;i<3;i++){points[i][0]=triangle[i*2];points[i][1]=triangle[i*2+1];points[i][2]=0.5f;}
    points[3][0]=(float)((((long double)triangle[4]-triangle[2])*widget_number(a,0x604114)+
        ((long double)triangle[0]-triangle[2])*widget_number(a,0x6040fc))+triangle[2]);
    points[3][1]=(float)(((long double)triangle[1]-triangle[3])*widget_number(a,0x6040fc)+triangle[3]);points[3][2]=0.5f;
    special=widget_word(a,0x594d38)!=0;
    if(special && widget_word(a,0x594d3c)){
        uint32_t tick=widget_word(a,0x5ac6ac);
        pulse=-(int32_t)(((tick&16)?tick&15:15-(tick&15))/2);
    }
    for(i=0;i<3;i++)if(widget_signed(widget_word(a,gates[i]))>0){
        float vertices[3][3];uint8_t colors[3];
        unsigned base=special?(i==0?144:i==1?160:64):194+i*3;
        unsigned center=special?(i==0?159:i==1?175:79)+(unsigned)pulse:base+7;
        for(j=0;j<3;j++){memcpy(vertices[j],points[indices[i][j]],sizeof(vertices[j]));colors[j]=(uint8_t)(j==1?center:base);}
        if(!game_level_palette_polygon(a,(const float (*)[3])vertices,colors,3,0x20000000))goto restore;
    }
    widget_put(a,0x59d46c,0);
    for(i=0;i<3;i++)if(widget_word(a,gates[i])==0x3f800000){
        uint8_t measure[32];unsigned length;int32_t left=widget_integer(points[1][0]),right=widget_integer(points[2][0]),width,delta,x,y;
        for(length=0;length<sizeof(measure);length++){
            measure[length]=game_memory_read_u8(&a->memory,0x590890+i*16+length);if(!measure[length])break;
        }
        if(length==sizeof(measure))goto restore_text;
        width=widget_signed(game_text_width_bytes(&a->memory,measure,length+1));
        delta=widget_signed((uint32_t)right-(uint32_t)width-(uint32_t)left);
        x=widget_add(left,delta>=0?delta/2:-1-(int32_t)((-(int64_t)delta-1)/2));
        y=widget_add(widget_integer(points[1][1]),1);
        if(!widget_text(a,x,y,0x590898+i*16))goto restore_text;
        break;
    }
    ok=!a->memory.fault;
restore_text:
    widget_put(a,0x59d46c,1);
restore:
    widget_float(a,0x6290d4,original[0]);widget_float(a,0x6290d0,original[1]);
    widget_float(a,0x6290dc,original[2]);widget_float(a,0x6290d8,original[3]);
    if(!widget_clip(a,original))ok=0;
    return ok;
}
extern int game_level_selection_vertices(GameApplication *,GameAddress,const float[][2],const float[][2],unsigned,uint32_t,uint32_t);
extern int game_application_render_line(GameApplication *,const int32_t[4],uint8_t);
extern int game_application_palette_point(GameApplication *,int32_t,int32_t,uint8_t);
static int32_t widget_shift(int32_t value,unsigned bits){return value>=0?value/(1<<bits):-1-(int32_t)((-(int64_t)value-1)/(1<<bits));}
static int radar_line(GameApplication *a,int32_t x,int32_t y,const int8_t offsets[4],uint8_t color){
    int32_t line[4]={widget_add(x,offsets[0]),widget_add(y,offsets[1]),widget_add(x,offsets[2]),widget_add(y,offsets[3])};
    return game_application_render_line(a,line,color);
}
/*Actual minimap symbols: local strokes retain original order and palette
 *alternation, including repeated strokes and the selected diamond's center. */
static int radar_symbol(GameApplication *a,GameAddress object,uint32_t flags,int32_t x,int32_t y,uint8_t faction,uint8_t first,uint8_t second,int selected){
    static const int8_t diamond[4][4]={{0,-3,3,0},{3,0,0,3},{0,3,-3,0},{-3,0,0,-3}};
    static const int8_t player[3][4]={{0,-1,0,0},{-1,1,-1,0},{1,1,1,0}};
    static const int8_t spawn[4][4]={{-1,0,0,-1},{0,1,1,0},{-1,-1,1,1},{-1,1,1,-1}};
    static const int8_t tower[2][4]={{0,-1,0,1},{-1,0,1,0}};
    static const int8_t vehicle[5][4]={{-1,-1,1,-1},{-1,0,1,0},{-1,1,1,1},{-1,0,1,0},{0,-1,0,1}};
    static const int8_t box[3][4]={{-1,-1,1,-1},{-1,0,1,0},{-1,1,1,1}};
    static const int8_t cross[2][4]={{-1,0,1,0},{0,-1,0,1}};
    const int8_t (*strokes)[4];unsigned count,i;uint8_t kind=game_memory_read_u8(&a->memory,object+12),center=255;
    if(selected){strokes=diamond;count=4;center=first;}
    else if(kind==0x40)return game_application_palette_point(a,x,y,second);
    else if(kind==0x20){strokes=player;count=3;}
    else if(kind==0x10){strokes=spawn;count=4;}
    else if(kind==0x88){
        if(!radar_line(a,x,y,tower[0],faction) || !radar_line(a,x,y,tower[1],faction))return 0;
        return game_application_palette_point(a,widget_add(x,1),widget_add(y,1),first) &&
            game_application_palette_point(a,widget_add(x,-1),widget_add(y,-1),first) &&
            game_application_palette_point(a,widget_add(x,-1),widget_add(y,1),second) &&
            game_application_palette_point(a,widget_add(x,1),widget_add(y,-1),second);
    }
    else if(kind==0x23){strokes=vehicle;count=5;}
    else if(flags&0x180){strokes=box;count=3;}
    else if(kind==0xa1){strokes=box;count=3;center=0;}
    else{strokes=cross;count=2;center=first;}
    for(i=0;i<count;i++){
        uint8_t color=selected || kind==0x20?faction:kind==0x10?(i<2?second:first):
            kind==0x23?(i<3?second:first):flags&0x180?faction:kind==0xa1?255:second;
        if(!radar_line(a,x,y,strokes[i],color))return 0;
    }
    if(selected || (kind!=0x20 && kind!=0x10 && kind!=0x23))return game_application_palette_point(a,x,y,center);
    return !a->memory.fault;
}
/*4add00: native map composition, canonical spatial-cell markers and five
 *objective indicators. Borrow the existing owned target image; all geometry
 *is local. No CPU drawing callback or serialized stack packet is involved. */
int game_level_frame_004add00(GameApplication *a){
    static const GameAddress viewport[4]={0x6290d4,0x6290d0,0x6290dc,0x6290d8};
    static const GameAddress saved[4]={0x603814,0x6037f8,0x60372c,0x603f74};
    float old[4],box[4],source_x,source_y,offset_x,offset_y,sine,cosine,half;GameAddress root,pose;
    GameRadarInput input;GameRadarGeometry geometry;GameRadarResult composed;
    uint32_t width,height,diameter,zoom;uint8_t *pixels;int32_t x0,x1,z0,z1,range,x,z;unsigned i,visits=0;int ok=0,blink;
    for(i=0;i<4;i++){old[i]=widget_number(a,viewport[i]);widget_float(a,saved[i],old[i]);}
    box[0]=box[1]=0;box[2]=(float)((long double)widget_signed(widget_word(a,0x627928))-widget_number(a,0x5744e4));
    box[3]=(float)((long double)widget_signed(widget_word(a,0x62792c))-widget_number(a,0x5744e4));
    for(i=0;i<4;i++)widget_float(a,viewport[i],box[i]);
    if(!widget_clip(a,box))goto restore;
    root=widget_word(a,0x5f0208);pose=widget_word(a,root+0x448);
    source_x=widget_number(a,pose+0x64);source_y=widget_number(a,pose+0x6c);diameter=widget_word(a,0x6034d0);zoom=widget_word(a,0x603878);
    input.screen_x=widget_signed(widget_word(a,0x60392c)+(diameter>>1));input.screen_y=widget_signed(widget_word(a,0x6038d8)+(diameter>>1));
    input.heading=game_memory_read_u16(&a->memory,widget_word(a,0x58f890)+6);
    input.source_x=source_x;input.source_y=source_y;input.source_scale=(float)widget_signed(zoom<<12);
    width=game_memory_read_u16(&a->memory,0x602680);height=game_memory_read_u16(&a->memory,0x602682);
    if(!width || !height || width>2048 || height>2048 || (width*height&3))goto restore;
    pixels=(uint8_t *)(void *)scene_projection_storage_borrow_u32(widget_word(a,0x602684),width*height/4);
    if(!pixels)goto restore;
    composed=game_ui_compose_radar_geometry(&a->memory,&input,pixels,(size_t)width*height,&geometry);
    if(composed==GAME_RADAR_FAULT || (composed==GAME_RADAR_OK && !game_level_selection_vertices(a,0x602680,
        (const float (*)[2])geometry.positions,(const float (*)[2])geometry.uv,32,(uint32_t)geometry.source_x,(uint32_t)geometry.source_y)))goto restore;
    x=widget_shift(widget_integer(source_x),12);z=widget_shift(widget_integer(source_y),12);range=widget_signed(widget_word(a,0x603870));
    x0=widget_signed((uint32_t)x-(uint32_t)range);if(x0<0)x0=0;
    z0=widget_signed((uint32_t)z-(uint32_t)range);if(z0<0)z0=0;
    x1=widget_add(x,range);if(x1>=32)x1=31;z1=widget_add(z,range);if(z1>=32)z1=31;
    sine=widget_sine(a,input.heading);cosine=widget_sine(a,input.heading+512);half=widget_number(a,0x5745b8);
    offset_x=(float)(((long double)(uint32_t)x0+range)*widget_number(a,0x5745b4)-source_x);
    offset_y=(float)(((long double)(uint32_t)z0+range)*widget_number(a,0x5745b4)-source_y);
    blink=widget_signed(widget_word(a,0x5acbe8))%12<=5;
    if((uint32_t)x0<=31 && (uint32_t)z0<=31 && (uint32_t)x1<=31 && (uint32_t)z1<=31)
    for(z=z0;z<=z1;z++)for(x=x0;x<=x1;x++){
        GameAddress object=widget_word(a,0x5f0804+((uint32_t)x*32+(uint32_t)z)*12);
        while(object){
            uint32_t flags=widget_word(a,object+0x10);GameAddress next=widget_word(a,object+4);
            if(++visits>65536)goto restore;
            if(!(flags&0x200800)){
                unsigned at=object==widget_word(a,root+0x3bc)?0x64:0x18;
                long double inverse=(long double)widget_number(a,0x5744e4)/widget_signed(zoom<<12);
                float u=(float)(((long double)widget_number(a,object+at)-(uint32_t)x0*4096+offset_x)*inverse-half);
                float v=(float)(((long double)widget_number(a,object+at+8)-(uint32_t)z0*4096+offset_y)*inverse-half);
                float rotated_x=(float)((long double)half-((long double)cosine*u-(long double)sine*v));
                float rotated_y=(float)((long double)half-((long double)cosine*v+(long double)sine*u));uint32_t ux,uy;
                memcpy(&ux,&rotated_x,4);memcpy(&uy,&rotated_y,4);
                if(ux<=0x80000000 && widget_signed(ux)<=0x3f800000 && uy<=0x80000000 && widget_signed(uy)<=0x3f800000){
                    int32_t sx=widget_signed(widget_word(a,0x60392c)+(uint32_t)widget_integer((long double)rotated_x*diameter));
                    int32_t sy=widget_signed(widget_word(a,0x6038d8)+(uint32_t)widget_integer((long double)rotated_y*diameter));
                    uint32_t dx=(uint32_t)sx-(diameter>>1)-widget_word(a,0x60392c),dy=(uint32_t)sy-(diameter>>1)-widget_word(a,0x6038d8);
                    if(dx*dx+dy*dy<=(diameter>>1)*(diameter>>1)){
                        uint32_t affiliation=widget_word(a,object+0x14);GameAddress catalog=widget_word(a,0x5f0210+(affiliation&63)*4);
                        uint8_t color=game_memory_read_u8(&a->memory,catalog+15);
                        if(!radar_symbol(a,object,flags,sx,sy,color,blink?255:color,blink?color:255,affiliation==widget_word(a,0x603e38)))goto restore;
                    }
                }
            }
            object=next;if(a->memory.fault)goto restore;
        }
    }
    for(i=0;i<5;i++){
        GameAddress marker=0x602538+i*8;int32_t sx,sy,dx,dy,cx=input.screen_x,cy=input.screen_y;uint32_t radius=(diameter>>1)-2,distance;
        float distance_float,radius_float;long double inverse,u,v;
        uint8_t color=game_memory_read_u8(&a->memory,0x58de78+i*2+((widget_word(a,0x5ac6ac)&8)>>3));
        if(widget_word(a,marker)==0xbf800000 || widget_word(a,marker+4)==0xbf800000)continue;
        inverse=(long double)widget_number(a,0x5744e4)/widget_signed(zoom<<12);
        u=((long double)widget_number(a,0x5745c0)-widget_number(a,marker)-(uint32_t)x0*4096+offset_x)*inverse-half;
        v=((long double)widget_number(a,marker+4)-(uint32_t)z0*4096+offset_y)*inverse-half;
        sx=widget_add(widget_signed(widget_word(a,0x60392c)),widget_integer(((long double)half-(cosine*u-sine*v))*diameter));
        sy=widget_add(widget_signed(widget_word(a,0x6038d8)),widget_integer(((long double)half-(cosine*v+sine*u))*diameter));
        dx=widget_signed((uint32_t)sx-(uint32_t)cx);dy=widget_signed((uint32_t)sy-(uint32_t)cy);
        distance=(uint32_t)dx*(uint32_t)dx+(uint32_t)dy*(uint32_t)dy;distance_float=(float)widget_signed(distance);radius_float=(float)(uint32_t)(radius*radius);
        if(radius_float>=distance_float){
            static const int8_t diagonals[2][4]={{-2,-2,2,2},{2,-2,-2,2}};
            if(!radar_line(a,sx,sy,diagonals[0],color) || !radar_line(a,sx,sy,diagonals[1],color))goto restore;
        }else{
            float ratio=(float)((long double)radius_float/distance_float);long double scale=sqrtl(ratio);
            if(!game_level_frame_0043a030(a,widget_add(cx,widget_integer((long double)dx*scale)),
                widget_add(cy,widget_integer((long double)dy*scale)),10,color))goto restore;
        }
    }
    ok=!a->memory.fault;
restore:
    for(i=0;i<4;i++)widget_float(a,viewport[i],old[i]);
    if(!widget_clip(a,old))ok=0;
    return ok;
}
extern int game_menu_projection_bounds(GameApplication *,uint32_t,uint32_t,uint32_t,uint32_t);
/*Descriptor44/50 and their payload consumers. Original addresses identify
 *ordinary native programs; no guest calling convention is interpreted. */
static int production_kind(GameApplication *a,GameAddress record,uint8_t *kind){
    GameAddress method=widget_word(a,widget_word(a,record)+0x44);
    if(method==0x53d8d0)*kind=0;
    else if(method==0x53ff60)*kind=11;
    else if(method==0x53f2c0){
        GameAddress payload=widget_word(a,record+0x48);
        switch(widget_word(a,widget_word(a,payload)+0x90)){
        case 0x51ba30:*kind=0x40;break;case 0x51bb10:*kind=0x21;break;
        case 0x51bba0:*kind=0x27;break;case 0x51bc30:*kind=0x26;break;default:return 0;
        }
    }else return 0;
    return !a->memory.fault;
}
static int production_remaining(GameApplication *a,GameAddress record,int32_t *value){
    GameAddress method=widget_word(a,widget_word(a,record)+0x50),payload=widget_word(a,record+0x48);
    *value=0;
    if(method==0x53d900)return !a->memory.fault;
    if(method!=0x53f2f0 && method!=0x53f400)return 0;
    if(widget_word(a,record+0x10)!=1){*value=100;return !a->memory.fault;}
    if(method==0x53f2f0){
        int32_t elapsed=widget_signed(widget_word(a,0x5ac6ac)-widget_word(a,payload+0xac));
        int32_t interval=widget_signed(widget_word(a,payload+0xa8)),numerator=widget_signed(0u-(uint32_t)elapsed*100u);
        if(elapsed>=interval)return !a->memory.fault;
        if(!interval || (numerator==INT32_MIN && interval==-1))return 0;
        *value=widget_add(numerator/interval,100);
    }else{
        float current=widget_number(a,payload+0xc4),maximum=widget_number(a,payload+0xc8),scale=widget_number(a,0x575484);
        if(current<maximum)*value=widget_integer((long double)scale-(long double)current*scale/maximum);
    }
    return !a->memory.fault;
}
static int production_string(GameApplication *a,GameAddress source,uint8_t bytes[128]){
    unsigned i;for(i=0;i<128;i++){bytes[i]=game_memory_read_u8(&a->memory,source+i);if(!bytes[i])return !a->memory.fault;}return 0;
}
static int production_gradient(GameApplication *a,int32_t left,int32_t top,int32_t right,int32_t bottom){
    float extend=widget_number(a,0x574d1c),p[4][3]={{(float)left,(float)top,0.5f},
        {(float)((long double)right+extend),(float)top,0.5f},
        {(float)((long double)right+extend),(float)((long double)bottom+extend),0.5f},
        {(float)left,(float)((long double)bottom+extend),0.5f}};
    /*4e9700's original color lane1 is undefined; retain the same explicit
     *right-gradient policy as the existing health bars. Device parity is open. */
    uint8_t colors[4]={104,111,111,104};
    return !a->memory.fault && game_level_palette_polygon(a,(const float (*)[3])p,colors,4,0);
}
static int production_caption(GameApplication *a,const uint8_t *bytes,int32_t left,int32_t y,int32_t width,int plus_one){
    int32_t measured=widget_signed(game_text_width_bytes(&a->memory,bytes,strlen((const char *)bytes)+1));
    int32_t delta=widget_signed((uint32_t)width-(uint32_t)measured+(uint32_t)plus_one);
    return !a->memory.fault && game_level_frame_text_bytes(a,widget_add(left,widget_shift(delta,1)),y,bytes,strlen((const char *)bytes)+1);
}
/*4b0a10 is the five-category production/status display. Its collection owner,
 *descriptor callbacks, cooldown/progress programs, caps, labels and free counts
 *are all consumed directly. Each category resets its minimum while the last
 *queried remaining value carries across categories as in the original. */
int game_level_frame_004b0a10(GameApplication *a){
    GameAddress root=widget_word(a,0x5f0208);uint32_t tick=widget_word(a,0x5ac6ac);unsigned category,visits=0;
    int32_t width=widget_signed(widget_word(a,0x603fb0)-widget_word(a,0x603fa8)),
        height=widget_signed(widget_word(a,0x603fb4)-widget_word(a,0x603fac)),remaining=width,minimum=100;
    uint8_t pulse=(uint8_t)((tick&8)?tick&7:7-(tick&7));
    if(!game_level_frame_00532780(a,246) || !game_menu_projection_bounds(a,0,0,widget_word(a,0x5ac6a0),widget_word(a,0x5ac6a4)))return 0;
    for(category=0;category<5;category++){
        GameAddress group=widget_word(a,root+0x294);int found=0,maxed=0;int32_t free_count=-1;
        minimum=100;
        uint8_t kind=game_memory_read_u8(&a->memory,0x59d590+category),name[128],text[160];int length;
        int32_t left=widget_signed(widget_word(a,0x603fa8+category*16)),top=widget_signed(widget_word(a,0x603fac+category*16)),
            right=widget_signed(widget_word(a,0x603fb0+category*16)),bottom=widget_signed(widget_word(a,0x603fb4+category*16));
        while(group){
            int32_t count=widget_short(game_memory_read_u16(&a->memory,group+0x24)),slot;GameAddress array=widget_word(a,group+0x18);
            if(++visits>65536)return 0;
            for(slot=1;slot<count;slot++){
                GameAddress record=widget_word(a,array+(uint32_t)slot*4);uint8_t member_kind;
                if(!record)continue;
                if(!production_kind(a,record,&member_kind))return 0;
                if(member_kind!=kind)continue;
                found=1;if(!production_remaining(a,record,&remaining))return 0;
                if(!remaining)break;
                if(remaining<minimum)minimum=remaining;
            }
            if(found && !remaining)break;
            group=widget_word(a,group+0x64);
        }
        switch(kind){
        case 11:free_count=0;break;
        case 0x21:maxed=widget_signed(widget_word(a,root+0x504))>=8;free_count=widget_signed(widget_word(a,root+0x4b8));break;
        case 0x26:free_count=widget_signed(widget_word(a,root+0x4c0));break;
        case 0x27:maxed=widget_signed(widget_word(a,root+0x508))>=7;free_count=widget_signed(widget_word(a,root+0x4bc));break;
        case 0x40:maxed=widget_signed(widget_word(a,root+0x500))>=16;free_count=widget_signed(widget_word(a,root+0x4b4));break;
        default:break;
        }
        if(maxed && (tick&16)){memcpy(text,"max",4);length=3;}
        else if(widget_word(a,0x594d48) && ((found && !remaining) || free_count) && !maxed && (tick&16)){
            unsigned color=(free_count>0 || (found && !maxed))?255:0;
            if(!production_string(a,0x6044d8+category*16,name))return 0;
            length=snprintf((char *)text,sizeof(text),"\013%03u%s",color,name);
        }else{
            if(!production_string(a,widget_word(a,0x59d598+category*4),name))return 0;
            length=snprintf((char *)text,sizeof(text),"%s",name);
        }
        if(length<0 || (unsigned)length>=sizeof(text))return 0;
        if(!found){if(!game_application_palette_quad(a,left,top,right,bottom,104))return 0;}
        else if(!remaining){if(!game_application_palette_quad(a,left,top,right,bottom,(uint8_t)((maxed?112:104)+pulse)))return 0;}
        else{
            int32_t split=widget_integer((long double)width*widget_signed(100u-(uint32_t)minimum)*widget_number(a,0x574608));
            if(!production_gradient(a,left,top,widget_add(left,split),bottom) ||
                !game_application_palette_quad(a,widget_add(widget_add(left,split),1),top,right,bottom,148))return 0;
        }
        if(!production_caption(a,text,left,widget_add(top,1),width,0))return 0;
        if(free_count){
            if(!game_application_palette_quad(a,left,widget_add(widget_add(top,height),2),right,widget_add(widget_add(bottom,height),2),(uint8_t)(104+pulse)))return 0;
            length=snprintf((char *)text,sizeof(text),"%d free",free_count);
            if(length<0 || (unsigned)length>=sizeof(text) || !production_caption(a,text,left,widget_add(top,9),width,1))return 0;
        }
    }
    return !a->memory.fault;
}
