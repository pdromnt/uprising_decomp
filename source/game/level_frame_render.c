#include "level_world_surface.h"
#include "level_world_render.h"
#include "ui_geometry.h"
#include "game_matrix.h"
#include "game_text_rasterizer.h"
#include <math.h>
#include <string.h>
extern int game_application_frame_depth_state(GameApplication *,int);
extern int game_level_frame_004d8b50(GameApplication *,const uint32_t[4],uint32_t[4],const uint32_t[2],float);
static int hud_image(void *context,const GameUiRect *source,const GameUiRect *destination,
    const GameUiImage *image,uint32_t depth,int depth_write){
    GameApplication *a=context;float oow;
    uint32_t src[4]={(uint32_t)source->left,(uint32_t)source->top,(uint32_t)source->right,(uint32_t)source->bottom};
    uint32_t dst[4]={(uint32_t)destination->left,(uint32_t)destination->top,(uint32_t)destination->right,(uint32_t)destination->bottom};
    uint32_t descriptor[2]={(uint32_t)image->pitch|((uint32_t)image->height<<16),image->pixels};
    memcpy(&oow,&depth,4);
    return game_application_world_state(a,WORLD_DEPTH_WRITE,depth_write) &&
        game_level_frame_004d8b50(a,src,dst,descriptor,oow);
}
/*The constructor's canonical 20-byte records feed the bitmap renderer directly.
 *No guest calling convention or duplicate sprite representation is involved. */
int game_level_frame_004d91f0(GameApplication *a,GameAddress node){
    GameUiImageOps ops={a,hud_image};
    return game_ui_draw_image_list(&a->memory,node,&ops)==GAME_UI_RESULT_OK;
}
int game_level_frame_004d89e0(GameApplication *a,GameAddress pixels){
    GameUiImageOps ops={a,hud_image};
    return game_ui_draw_marker(&a->memory,pixels,&ops)==GAME_UI_RESULT_OK;
}
/*515ff0 ->4d9170: borrow the control's existing 24-byte image record. The
 *original clips the destination before constructing its full-source rectangle. */
int game_level_control_bitmap(GameApplication *a,GameAddress image,const GameUiRect *bounds){
    GameMemory *m=&a->memory;GameUiRect destination=*bounds,source;GameUiImage bitmap;
    uint32_t v;int32_t clip[4];unsigned i;
    for(i=0;i<4;i++){v=game_memory_read_u32(m,0x597f50+i*4);memcpy(clip+i,&v,4);}
    if(m->fault)return 0;
    if(destination.left>=clip[2] || destination.right<clip[0] || destination.top>=clip[3] || destination.bottom<clip[1])return 1;
    if(destination.left<clip[0])destination.left=clip[0];
    if(destination.top<clip[1])destination.top=clip[1];
    if(destination.right>=clip[2])destination.right=clip[2]-1;
    if(destination.bottom>=clip[3])destination.bottom=clip[3]-1;
    bitmap.pitch=(uint16_t)game_memory_read_u32(m,image+4);bitmap.height=(uint16_t)game_memory_read_u32(m,image+8);
    bitmap.pixels=game_memory_read_u32(m,image+20);source.left=source.top=0;source.right=bitmap.pitch;source.bottom=bitmap.height;
    return !m->fault && hud_image(a,&source,&destination,&bitmap,0x3f000000,0);
}
int game_level_frame_004d8ab0(GameApplication *a,int32_t x,int32_t y){
    uint32_t index=game_memory_read_u32(&a->memory,0x60545c);
    int32_t sx=(int32_t)(x&65535),sy=(int32_t)(y&65535);
    if(index>1 || a->memory.fault)return 0;
    if(sx>=32768)sx-=65536;
    if(sy>=32768)sy-=65536;
    game_memory_write_u32(&a->memory,0x605448+index*8,(uint32_t)sx);
    game_memory_write_u32(&a->memory,0x60544c+index*8,(uint32_t)sy);
    game_memory_write_u32(&a->memory,0x60545c,1-index);
    return !a->memory.fault;
}
static uint32_t word(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float number(GameApplication *a,GameAddress p){uint32_t b=word(a,p);float v;memcpy(&v,&b,4);return v;}
static float integer_coordinate(float value){
    long double v=truncl(value);uint32_t low;int32_t result;
    if(!isfinite(v) || v<-9223372036854775808.0L || v>=9223372036854775808.0L)low=0;
    else low=(uint32_t)(uint64_t)(int64_t)v;
    memcpy(&result,&low,4);return (float)result;
}
static float signed_coordinate(GameApplication *a,GameAddress p){uint32_t b=word(a,p);int32_t v;memcpy(&v,&b,4);return (float)v;}
extern int game_level_palette_polygon(GameApplication *,const float[][3],const uint8_t *,unsigned,uint32_t);
extern int game_application_render_line(GameApplication *,const int32_t[4],uint8_t);
extern int game_application_options_load(GameApplication *,uint32_t);
static int32_t hud_integer(long double value){
    uint32_t low;int32_t result;
    low=(uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);
    memcpy(&result,&low,4);return result;
}
static int32_t hud_signed(uint32_t bits){int32_t value;memcpy(&value,&bits,4);return value;}
static int32_t hud_add(int32_t x,int32_t y){return hud_signed((uint32_t)x+(uint32_t)y);}
static int32_t hud_sub(int32_t x,int32_t y){return hud_signed((uint32_t)x-(uint32_t)y);}
static int32_t hud_shift(int32_t x,unsigned shift){return x>=0?x/(1<<shift):-1-(int32_t)((-(int64_t)x-1)/(1<<shift));}
static int hud_line(GameApplication *a,int32_t x,int32_t y,int32_t xx,int32_t yy,uint8_t color){
    int32_t line[4]={x,y,xx,yy};return game_application_render_line(a,line,color);
}
extern int game_campaign_project_point(GameApplication *,float[3]);
extern int game_level_scene_property(GameApplication *,GameAddress,GameAddress,uint32_t *);
extern int game_application_palette_quad(GameApplication *,int32_t,int32_t,int32_t,int32_t,uint8_t);
extern int game_application_palette_point(GameApplication *,int32_t,int32_t,uint8_t);
extern int game_menu_projection_bounds(GameApplication *,uint32_t,uint32_t,uint32_t,uint32_t);
extern int game_level_frame_text_bytes(GameApplication *,int32_t,int32_t,const uint8_t *,size_t);
extern int game_level_frame_00532870(GameApplication *,int32_t,int32_t,GameAddress,uint32_t);
int game_level_frame_004b1ea0(GameApplication *,float,float,float,float,float,float);
int game_level_frame_004a3390(GameApplication *,int32_t,int32_t,int32_t,int32_t,uint8_t,uint8_t,float);
static int overview_text(GameApplication *a,GameAddress address,int32_t x,int32_t y,int align){
    uint8_t bytes[256];size_t n;int32_t width;
    for(n=0;n<sizeof(bytes);n++){bytes[n]=game_memory_read_u8(&a->memory,address+(uint32_t)n);if(!bytes[n])break;}
    if(n==sizeof(bytes) || a->memory.fault)return 0;
    width=hud_signed(game_text_width_bytes(&a->memory,bytes,n+1));
    if(align)x=hud_sub(x,align==1?hud_shift(width,1):width);
    return !a->memory.fault && game_level_frame_text_bytes(a,x,y,bytes,n+1);
}
/*4ae7b0: barycentric strategic marker. Keep the two float stores before integer
 *conversion, and the original distinct small/large four-line patterns. */
int game_level_frame_004ae7b0(GameApplication *a){
    float x=(float)((((long double)number(a,0x603860)-number(a,0x603858))*number(a,0x604114)+
        ((long double)number(a,0x603850)-number(a,0x603858))*number(a,0x6040fc))+number(a,0x603858));
    float y=(float)(((long double)number(a,0x603854)-number(a,0x60385c))*number(a,0x6040fc)+number(a,0x60385c));
    int32_t ix=hud_integer(x),iy=hud_integer(y),xm=hud_sub(ix,1),ym=hud_sub(iy,1),xp=hud_add(ix,1),yp=hud_add(iy,1);
    if(word(a,0x597f80)==1)return
        hud_line(a,xm,iy,ix,ym,169) && hud_line(a,ix,yp,xp,iy,169) &&
        hud_line(a,hud_sub(ix,2),hud_sub(iy,2),hud_add(ix,2),hud_add(iy,2),153) &&
        hud_line(a,hud_add(ix,2),hud_sub(iy,2),hud_sub(ix,2),hud_add(iy,2),153);
    return hud_line(a,xm,iy,ix,ym,169) && hud_line(a,ix,yp,xp,iy,169) &&
        hud_line(a,xm,ym,xp,yp,153) && hud_line(a,xp,ym,xm,yp,153);
}
/*4b28d0: real mode5 overview, triangle/marker, paired selection diagonals,
 *three resource bars and labels. All packets and strings are local or borrowed;
 *the existing records, projection and glyph cursor remain the only owners. */
int game_level_frame_004b28d0(GameApplication *a){
    static const GameAddress fractions[3]={0x6040fc,0x6040a4,0x604114};
    static const GameAddress labels[3]={0x5908c0,0x5908d4,0x5908e8};
    uint32_t large;int32_t thickness,left,top,spacing,width,x,y;unsigned i;GameAddress work;
    if(!a || a->memory.fault || !game_menu_projection_bounds(a,0,0,word(a,0x5ac6a0),word(a,0x5ac6a4)) ||
       !game_level_frame_004b1ea0(a,number(a,0x603850),number(a,0x603854),number(a,0x603858),number(a,0x60385c),number(a,0x603860),number(a,0x603864)) ||
       !game_level_frame_004ae7b0(a))return 0;
    x=hud_integer(number(a,0x6038b8));y=hud_integer(number(a,0x6038bc));
    if(!hud_line(a,hud_sub(x,1),hud_sub(y,1),hud_add(x,1),hud_add(y,1),153) ||
       !hud_line(a,hud_add(x,1),hud_sub(y,1),hud_sub(x,1),hud_add(y,1),153))return 0;
    large=word(a,0x597f80)!=0;thickness=large?6:4;left=large?16:10;top=large?120:64;spacing=large?30:20;width=large?80:50;
    if(!word(a,0x594d38)){
        for(i=0;i<3;i++)if(!game_application_palette_quad(a,left,top+(int32_t)i*spacing,left+width,top+(int32_t)i*spacing+thickness,247))return 0;
        for(i=0;i<3;i++)if(!game_application_palette_quad(a,left,top+(int32_t)i*spacing,
            hud_add(left,hud_integer((long double)width*number(a,fractions[i]))),top+(int32_t)i*spacing+thickness,246))return 0;
    }else for(i=0;i<3;i++)if(!game_level_frame_004a3390(a,left,top+(int32_t)i*spacing,left+width,top+(int32_t)i*spacing+thickness,176,144,number(a,fractions[i])))return 0;
    game_memory_write_u8(&a->memory,0x59d448,246);
    for(i=0;i<3;i++){
        long double percent=(long double)number(a,fractions[i])*number(a,0x574500);
        float stored=(float)percent;
        uint32_t rounded=(uint32_t)hud_integer((long double)stored+(percent>number(a,0x5744f4)?number(a,0x5745b8):-number(a,0x5745b8)));
        if(!game_level_frame_00532870(a,left,top+(int32_t)i*spacing-6,labels[i],rounded))return 0;
    }
    if(!overview_text(a,0x5908fc,hud_integer(number(a,0x603850)),hud_sub(hud_integer(number(a,0x603854)),6),1) ||
       !overview_text(a,0x59090c,hud_sub(hud_integer(number(a,0x603858)),2),hud_add(hud_integer(number(a,0x60385c)),2),0) ||
       !overview_text(a,0x590914,hud_add(hud_integer(number(a,0x603860)),2),hud_add(hud_integer(number(a,0x603864)),2),2))return 0;
    work=word(a,0x628a88);
    return game_menu_projection_bounds(a,word(a,work+0x534),word(a,work+0x538),word(a,work+0x53c),word(a,work+0x540));
}
/*4a3390: viewport clipping, gradient fill and the animated empty portion.
 *Original packet color lane1 is uninitialized; define it as the right-edge
 *gradient. That lane's retail parity remains uncertain, rather than reading an
 *uninitialized native local or preserving guest-stack dependence. */
int game_level_frame_004a3390(GameApplication *a,int32_t left,int32_t top,int32_t right,int32_t bottom,
    uint8_t filled,uint8_t empty,float fraction){
    float vt=number(a,0x6290d0),vl=number(a,0x6290d4),vb=number(a,0x6290d8),vr=number(a,0x6290dc);
    int32_t split;uint32_t bits,tick=word(a,0x5ac6ac);uint8_t pulse,gradient,colors[4];float positions[4][3];
    if(a->memory.fault)return 0;
    if(!((long double)top<vb) || !((long double)bottom>=vt))return 1;
    if(vt>top)top=hud_integer(vt);else if(!(vb>bottom))bottom=hud_sub(hud_integer(vb),1);
    if(left<vl){if(right<vl)return 1;left=hud_integer(vl);}else if(!(vr>left))return 1;
    if(right<0 || right<vl)return 1;
    if(!(vr>right)){if(!(left<vr))return 1;right=hud_sub(hud_integer(vr),1);}
    pulse=(uint8_t)((tick&16)?tick&15:15-(tick&15));
    split=hud_add(left,hud_integer((long double)hud_sub(right,left)*fraction));
    gradient=(uint8_t)(filled+(uint8_t)hud_integer((long double)fraction*number(a,0x574350)));
    colors[0]=colors[3]=filled;colors[1]=colors[2]=gradient;
    positions[0][0]=positions[3][0]=(float)left;positions[0][1]=positions[1][1]=(float)top;
    positions[1][0]=positions[2][0]=(float)((long double)split+number(a,0x5742d4));
    positions[2][1]=positions[3][1]=(float)((long double)bottom+number(a,0x5742d4));
    positions[0][2]=positions[1][2]=positions[2][2]=positions[3][2]=0.5f;
    if(!game_level_palette_polygon(a,(const float (*)[3])positions,colors,4,0))return 0;
    memcpy(&bits,&fraction,4);
    if(hud_signed(bits)>=0x3f800000)return !a->memory.fault;
    return game_application_palette_quad(a,(bits&0x7fffffff)?hud_add(split,1):left,top,right,bottom,(uint8_t)(empty+pulse));
}
int game_level_frame_004ae920(GameApplication *a,int32_t x,int32_t y,uint8_t color){
    static const uint8_t pixels[][2]={{0,0},{0,1},{0,2},{1,2},{2,2},
        {4,0},{4,1},{4,2},{5,0},{6,0},{6,1},{6,2},{5,2},
        {8,0},{8,1},{8,2},{9,0},{10,0},{9,2},{10,2},
        {12,0},{12,1},{12,2},{13,1},{14,0},{14,2}};
    unsigned i;for(i=0;i<sizeof(pixels)/sizeof(pixels[0]);i++)
        if(!game_application_palette_point(a,hud_add(x,pixels[i][0]),hud_add(y,pixels[i][1]),color))return 0;
    return 1;
}
/*Both consumers transform all eight canonical scene bounds corners. Selected
 *units reject the complete box when any projected depth is below10. */
static int hud_object_bounds(GameApplication *a,GameAddress object,int reject_near,int32_t box[4],int *visible){
    float matrix[12],extent[3];GameAddress pose=word(a,object+0x54)+4;unsigned i,j;
    box[0]=box[1]=INT32_MAX;box[2]=box[3]=INT32_MIN;*visible=1;
    for(i=0;i<12;i++)matrix[i]=number(a,pose+i*4);
    for(i=0;i<3;i++)extent[i]=number(a,object+0x2c+i*4);
    if(a->memory.fault)return 0;
    for(i=0;i<8;i++){
        float local[3],point[3];int32_t x,y;
        for(j=0;j<3;j++)local[j]=(i&(1u<<j))?extent[j]:-extent[j];
        game_matrix_transform_point_values(point,matrix,local);
        if(!game_campaign_project_point(a,point))return 0;
        if(reject_near){uint32_t bits;memcpy(&bits,point+2,4);if(hud_signed(bits)<0x41200000){*visible=0;return 1;}}
        x=hud_integer(point[0]);y=hud_integer(point[1]);
        if(x<box[0])box[0]=x;
        if(y<box[1])box[1]=y;
        if(x>box[2])box[2]=x;
        if(y>box[3])box[3]=y;
    }
    return !a->memory.fault;
}
int game_level_frame_004af040(GameApplication *a,GameAddress object){
    int32_t box[4];int visible;
    if(!hud_object_bounds(a,object,1,box,&visible))return 0;
    if(!visible)return 1;
    return hud_line(a,box[0],box[1],box[0],box[3],247) && hud_line(a,box[0],box[3],box[2],box[3],247) &&
        hud_line(a,box[2],box[3],box[2],box[1],247) && hud_line(a,box[2],box[1],box[0],box[1],247);
}
int game_level_frame_004aeab0(GameApplication *a){
    GameAddress target=word(a,0x60387c),root;uint32_t count,found=0,cell_count,cell;unsigned visits=0;
    if(target){
        int32_t box[4],l,t,r,b,qw,qh,h;uint32_t maximum,allied,method;uint8_t color;int visible;float fraction;
        if(!hud_object_bounds(a,target,0,box,&visible))return 0;
        l=box[0];t=box[1];r=box[2];b=box[3];qw=hud_shift(hud_sub(r,l),2);qh=hud_shift(hud_sub(b,t),2);
        method=game_memory_read_u8(&a->memory,target+12)==0x23?0x5545c0:word(a,word(a,target)+0x44);
        if(!game_level_scene_property(a,target,method,&maximum))return 0;
        fraction=(float)((long double)hud_signed(word(a,target+0x58))/hud_signed(maximum));
        if(!game_level_frame_004a3390(a,hud_add(hud_add(l,qw),1),hud_sub(t,4),hud_sub(hud_sub(r,qw),1),hud_sub(t,2),176,144,fraction))return 0;
        root=word(a,0x5f0208);allied=word(a,0x5f0160+(game_memory_read_u16(&a->memory,root+8)&63)*24+(word(a,target+0x14)&63)*4);
        if(allied)color=246;else{uint32_t blink=word(a,0x58fe10)^1;game_memory_write_u32(&a->memory,0x58fe10,blink);color=blink<1?148:153;}
        if(word(a,0x603710) && !game_level_frame_004ae920(a,hud_sub(hud_shift(hud_add(l,r),1),7),hud_sub(t,8),246))return 0;
        h=hud_shift(qw<qh?qw:qh,1);
        if(allied){
            int32_t lines[12][4]={{l,hud_add(t,h),hud_add(l,h),t},
                {hud_add(hud_add(l,h),1),t,hud_add(l,qw),t},{l,hud_add(hud_add(t,h),1),l,hud_add(t,qh)},
                {hud_sub(r,h),t,r,hud_add(t,h)},{hud_sub(hud_sub(r,h),1),t,hud_sub(r,qw),t},
                {r,hud_add(hud_add(t,h),1),r,hud_add(t,qh)},{hud_sub(r,h),b,r,hud_sub(b,h)},
                {hud_sub(hud_sub(r,h),1),b,hud_sub(r,qw),b},{r,hud_sub(hud_sub(b,h),1),r,hud_sub(b,qh)},
                {l,hud_sub(b,h),hud_add(l,h),b},{hud_add(hud_add(l,h),1),b,hud_add(l,qw),b},
                {l,hud_sub(hud_sub(b,h),1),l,hud_sub(b,qh)}};
            unsigned i;for(i=0;i<12;i++)if(!game_application_render_line(a,lines[i],color))return 0;
        }else{
            int32_t lines[8][4]={{l,t,hud_add(l,qw),t},{l,hud_add(t,1),l,hud_add(t,qh)},
                {r,t,hud_sub(r,qw),t},{r,hud_add(t,1),r,hud_add(t,qh)},
                {r,b,hud_sub(r,qw),b},{r,hud_sub(b,1),r,hud_sub(b,qh)},
                {l,b,hud_add(l,qw),b},{l,hud_sub(b,1),l,hud_sub(b,qh)}};
            unsigned i;for(i=0;i<8;i++)if(!game_application_render_line(a,lines[i],color))return 0;
        }
    }
    root=word(a,0x5f0208);count=word(a,root+0x444);cell_count=word(a,0x6020d0);
    if(hud_signed(count)<=0 || hud_signed(cell_count)<=0)return !a->memory.fault;
    if(count>30 || cell_count>1024)return 0;
    for(cell=0;cell<cell_count && found<count;cell++){
        int32_t x=(int16_t)game_memory_read_u16(&a->memory,0x5f06b8+cell*4),y=(int16_t)game_memory_read_u16(&a->memory,0x5f06ba+cell*4);
        int64_t at=0x5f0804+(int64_t)(x*32+y)*12;GameAddress object;
        if(at<0 || at>UINT32_MAX-4)return 0;
        object=word(a,(GameAddress)at);
        while(object){unsigned i;
            if(++visits>65536)return 0;
            for(i=0;i<count;i++)if(word(a,root+0x3cc+i*4)==object){
                ++found;if(!game_level_frame_004af040(a,object))return 0;break;
            }
            object=word(a,object+4);if(a->memory.fault)return 0;
        }
    }
    return !a->memory.fault;
}
/*4b1ea0: three independently shaded triangles and the original outer outline.
 *Local positions replace the old stack packets, retaining the palette gradient
 *and translucency flags consumed by43a240/4e9560. */
int game_level_frame_004b1ea0(GameApplication *a,float ax,float ay,float bx,float by,float cx,float cy){
    float points[4][3]={{ax,ay,0.25f},{bx,by,0.25f},{cx,cy,0.25f},{0,0,0.25f}};
    static const unsigned indices[3][3]={{0,3,1},{1,3,2},{2,3,0}};
    static const GameAddress gates[3]={0x604114,0x6040fc,0x6040a4};
    unsigned triangle,i;int32_t line[4],x[3],y[3];
    points[3][0]=(float)((((long double)cx-bx)*number(a,0x604114)+
        ((long double)ax-bx)*number(a,0x6040fc))+(long double)bx);
    points[3][1]=(float)((long double)by+((long double)ay-by)*number(a,0x6040fc));
    if(a->memory.fault)return 0;
    for(triangle=0;triangle<3;triangle++){
        uint32_t bits=word(a,gates[triangle]);int32_t gate;float vertices[3][3];uint8_t colors[3];
        memcpy(&gate,&bits,4);if(gate<=0)continue;
        for(i=0;i<3;i++){memcpy(vertices[i],points[indices[triangle][i]],sizeof(vertices[i]));
            colors[i]=(uint8_t)(14+triangle*2+(i==1?5:0));}
        if(!game_level_palette_polygon(a,(const float (*)[3])vertices,colors,3,0x800))return 0;
    }
    for(i=0;i<3;i++){x[i]=hud_integer(points[i][0]);y[i]=hud_integer(points[i][1]);}
    /*Original outline shifts the first and second x endpoints left one pixel. */
    x[0]=hud_integer((long double)x[0]-1);x[1]=hud_integer((long double)x[1]-1);
    for(i=0;i<3;i++){unsigned j=(i+1)%3;line[0]=x[i];line[1]=y[i];line[2]=x[j];line[3]=y[j];
        if(!game_application_render_line(a,line,15))return 0;}
    return !a->memory.fault;
}
/*4b26a0: expanding selector preview followed by the curved transition to
 *mode5. The existing mode owner receives the actual mode change. */
int game_level_frame_004b26a0(GameApplication *a){
    uint32_t phase=word(a,0x6037fc),elapsed=word(a,0x5ac6ac)-word(a,0x6038e8);
    int32_t ticks;float center_x,center_y;long double extent,t;
    int32_t x,y,half,height,lower;
    memcpy(&ticks,&elapsed,4);t=ticks<10?(long double)ticks*number(a,0x574618):1.0L;
    if(phase==0){
        t=(float)t;
        if(ticks>=10){game_memory_write_u32(&a->memory,0x6037fc,1);
            game_memory_write_u32(&a->memory,0x6038e8,word(a,0x5ac6ac));}
        center_x=(float)(((long double)number(a,0x603e30)-number(a,0x6037f0))*t+number(a,0x6037f0));
        center_y=(float)(((long double)number(a,0x603e34)-number(a,0x6037f4))*t+number(a,0x6037f4));
        extent=number(a,0x60409c);
    }else if(phase==1){
        if(ticks>=10 && !game_application_options_load(a,5))return 0;
        if(ticks<10){t*=t;t=(float)t;}
        extent=((long double)number(a,0x6032dc)-number(a,0x60409c))*t+number(a,0x60409c);
        center_x=number(a,0x603e30);center_y=number(a,0x603e34);
    }else return 0; /*Original higher phases read uninitialized stack values. */
    x=hud_integer(center_x);y=hud_integer(center_y);height=hud_integer(extent);
    half=hud_integer(extent*number(a,0x57461c));lower=hud_integer(extent*number(a,0x5745b8));
    if(a->memory.fault)return 0;
    return game_level_frame_004b1ea0(a,(float)x,(float)hud_integer((long double)y-height),
        (float)hud_integer((long double)x-half),(float)hud_integer((long double)y+lower),
        (float)hud_integer((long double)x+half),(float)hud_integer((long double)y+lower));
}
/*4d7680: clear depth inside the current viewport, plus both mode7 panels.
 * The fourth coordinate store follows a pushed argument in the original;
 * account for that changed stack-relative offset when recovering the quad. */
int game_application_render_mode(GameApplication *a,int enabled){
    GameRenderVertex vertices[4]={{0}};float left,right,top,bottom;unsigned i,panel;
    if(!game_application_frame_depth_state(a,enabled))return 0;
    if(!enabled)return 1;
    left=integer_coordinate(number(a,0x6290d4));right=integer_coordinate(number(a,0x6290dc));
    top=integer_coordinate(number(a,0x6290d0));bottom=integer_coordinate(number(a,0x6290d8));
    vertices[0].x=vertices[3].x=left;vertices[1].x=vertices[2].x=right;
    vertices[0].y=vertices[1].y=top;vertices[2].y=vertices[3].y=bottom;
    for(i=0;i<4;i++)vertices[i].oow=65536.0f;
    if(a->memory.fault || !game_application_surface_polygon(a,vertices,4))return 0;
    if(word(a,0x604760)==7)for(panel=0;panel<2;panel++){
        GameAddress bounds=panel?0x6024f0:0x6027b8;
        left=signed_coordinate(a,bounds);top=signed_coordinate(a,bounds+4);
        right=signed_coordinate(a,bounds+8);bottom=signed_coordinate(a,bounds+12);
        vertices[0].x=vertices[3].x=left;vertices[1].x=vertices[2].x=right;
        vertices[0].y=vertices[1].y=top;vertices[2].y=vertices[3].y=bottom;
        if(a->memory.fault || !game_application_surface_polygon(a,vertices,4))return 0;
    }
    return !a->memory.fault;
}
