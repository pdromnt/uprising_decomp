#include "software_raster.h"
#include <math.h>
#include <limits.h>
#include <string.h>
#define MAX_EDGES 25u
typedef struct Plane {float dx,dy;} Plane;
typedef struct Edge {
    int32_t first,last,x,whole;uint32_t fraction,step;
    float field[3],advance[3];
} Edge;
static int32_t signed_word(uint32_t v){return v<=INT32_MAX?(int32_t)v:(int32_t)((int64_t)v-INT64_C(4294967296));}
/*The raster kernel selects downward FIST rounding. Preserve its numeric
 * result explicitly, without an emulated control word/register/flag state. */
static uint32_t integer_floor(long double v){
    long double n=floorl(v);
    if(!isfinite(n) || n<INT32_MIN || n>=2147483648.0L)return UINT32_C(0x80000000);
    return (uint32_t)(int32_t)n;
}
static int32_t fixed_integer(uint32_t fixed){
    uint32_t high=fixed>>16;return high<32768u?(int32_t)high:(int32_t)high-65536;
}
static int span(size_t bytes,int32_t width,int32_t height,int32_t stride,unsigned pixel_bytes){
    uint64_t row=(uint64_t)(uint32_t)width*pixel_bytes;
    return width>0 && height>0 && width<=8192 && height<=8192 && stride>0 && row<=(uint32_t)stride &&
        (uint64_t)(uint32_t)stride*(uint32_t)(height-1)+row<=bytes;
}
int software_raster_clip_line(int32_t c[4],const int32_t bounds[4]){
    float first=0,last=1,delta[2];unsigned axis,end;
    int32_t original[4];
    if(!c || !bounds || bounds[0]>=bounds[2] || bounds[1]>=bounds[3])return 0;
    memcpy(original,c,sizeof(original));
    for(axis=0;axis<2;++axis){
        int64_t d=(int64_t)c[axis+2]-c[axis];
        float low,high;
        if(d<INT32_MIN || d>INT32_MAX)return 0;
        delta[axis]=(float)d;
        if(!d){if(c[axis]<bounds[axis] || c[axis]>=bounds[axis+2])return 0;continue;}
        low=(float)(((long double)bounds[axis]-c[axis])/delta[axis]);
        high=(float)(((long double)bounds[axis+2]-1-c[axis])/delta[axis]);
        if(d<0){float swap=low;low=high;high=swap;}
        if(low>last || high<first)return 0;
        if(low>first)first=low;
        if(high<last)last=high;
    }
    for(end=0;end<2;++end)for(axis=0;axis<2;++axis){
        if((end && last==1) || (!end && first==0))continue;
        long double n=truncl((long double)delta[axis]*(end?last:first));
        int64_t value=(int64_t)original[axis]+(int64_t)n;
        if(value<bounds[axis])value=bounds[axis];
        if(value>=bounds[axis+2])value=(int64_t)bounds[axis+2]-1;
        c[end*2+axis]=(int32_t)value;
    }
    return 1;
}
int software_raster_line(const SoftwareRasterFrame *f,const int32_t c[4],uint8_t color){
    int64_t dx,dy,x,y,error_x=0,error_y=0,steps,i;int sx,sy;
    if(!f || !f->color || !c || !span(f->color_bytes,f->width,f->height,f->color_stride,1))return 0;
    if(c[0]<0 || c[0]>=f->width || c[2]<0 || c[2]>=f->width ||
       c[1]<0 || c[1]>=f->height || c[3]<0 || c[3]>=f->height)return 0;
    dx=(int64_t)c[2]-c[0];dy=(int64_t)c[3]-c[1];sx=(dx>0)-(dx<0);sy=(dy>0)-(dy<0);
    if(dx<0)dx=-dx;
    if(dy<0)dy=-dy;
    steps=dx>dy?dx:dy;x=c[0];y=c[1];
    /*The first accumulator advance does not move. The original remaining
     * counter starts at steps+1 and draws again when it reaches zero. */
    for(i=0;i<=steps+1;++i){
        f->color[(size_t)y*(size_t)f->color_stride+(size_t)x]=color;
        error_x+=dx;error_y+=dy;
        if(error_x>steps){error_x-=steps;x+=sx;}
        if(error_y>steps){error_y-=steps;y+=sy;}
    }
    return 1;
}
static uint16_t depth_read(const uint8_t *p){return (uint16_t)((uint32_t)p[0]|((uint32_t)p[1]<<8));}
static void depth_write(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static Plane gradient(const RenderArenaVertex *const *v,const float values[3],float inverse){
    long double a=(long double)values[1]-values[2],b=(long double)values[0]-values[2];Plane p;
    float x0=(float)((long double)v[0]->position[0]-v[2]->position[0]);
    float x1=(float)((long double)v[1]->position[0]-v[2]->position[0]);
    float y0=(float)((long double)v[0]->position[1]-v[2]->position[1]);
    float y1=(float)((long double)v[1]->position[1]-v[2]->position[1]);
    p.dx=(float)((a*y0-b*y1)*inverse);p.dy=(float)((b*x1-a*x0)*inverse);return p;
}
static void edge_step(Edge *e,const Plane planes[3]){
    uint64_t next=(uint64_t)e->fraction+e->step;unsigned i;int32_t carry=(int32_t)(next>>32);
    e->fraction=(uint32_t)next;e->x+=e->whole+carry;
    for(i=0;i<3;++i){long double value=e->field[i];if(carry)value+=planes[i].dx;
        e->field[i]=(float)(value+e->advance[i]);}
}
int software_raster_polygon(const SoftwareRasterFrame *f,uint32_t flags,uint8_t color,
    const RenderArenaVertex *const *v,const RenderArenaUv *uv,uint32_t count,
    const SoftwareRasterTexture *texture){
    Edge edges[MAX_EDGES];Plane planes[3]={{0}};float field_values[MAX_EDGES][3];
    uint32_t edge_count=0,i,j;int32_t first=INT32_MAX,last=INT32_MIN,y;float inverse;
    long double determinant;int textured=(flags&1u)!=0,depth=(flags&0x18u)!=0;
    /*Perspective blocks, scalar color, secondary textures, fog/blend and point
     * packets remain explicit reconstruction work, never a flat substitute. */
    if(flags&~UINT32_C(0x119))return -1;
    if(!f || !v || count<3 || count>MAX_EDGES || !f->color ||
       !span(f->color_bytes,f->width,f->height,f->color_stride,1) ||
       (depth && (!f->depth || !span(f->depth_bytes,f->width,f->height,f->depth_stride,2))) ||
       (textured && (!uv || !texture || !texture->pixels || texture->width<=0 || texture->height<=0 ||
        (uint64_t)(uint32_t)texture->width*(uint32_t)texture->height>texture->bytes)))return 0;
    for(i=0;i<count;++i){if(!v[i])return 0;
        for(j=0;j<3;++j)if(!isfinite(v[i]->position[j]))return 0;
        field_values[i][0]=textured?uv[i].u:0;field_values[i][1]=textured?uv[i].v:0;field_values[i][2]=v[i]->position[2];}
    determinant=((long double)v[1]->position[0]-v[2]->position[0])*((long double)v[0]->position[1]-v[2]->position[1])-
        ((long double)v[0]->position[0]-v[2]->position[0])*((long double)v[1]->position[1]-v[2]->position[1]);
    if(determinant==0)return 1;
    inverse=(float)(1.0L/determinant);
    for(i=0;i<3;++i){float values[3]={field_values[0][i],field_values[1][i],field_values[2][i]};planes[i]=gradient(v,values,inverse);}
    for(i=0;i<count;++i){uint32_t next=(i+1)%count,low=i,high=next;Edge e={0};long double slope,x_at,dy;uint32_t start_fixed,delta_fixed;
        int32_t yi=signed_word(integer_floor(v[i]->position[1])),yn=signed_word(integer_floor(v[next]->position[1]));
        if(yi==yn)continue;
        if(yi>yn){low=next;high=i;}
        e.first=signed_word(integer_floor(v[low]->position[1]));e.last=signed_word(integer_floor(v[high]->position[1]));
        if(e.first==INT32_MAX || e.first==INT32_MIN || e.last==INT32_MIN)return 0;
        ++e.first;
        slope=((long double)v[high]->position[0]-v[low]->position[0])/((long double)v[high]->position[1]-v[low]->position[1]);
        dy=(long double)e.first-v[low]->position[1];x_at=(long double)v[low]->position[0]+dy*slope;
        start_fixed=integer_floor(x_at*65536.0L);delta_fixed=integer_floor(slope*65536.0L);
        e.x=fixed_integer(start_fixed)+1;e.fraction=start_fixed<<16;e.whole=fixed_integer(delta_fixed);e.step=delta_fixed<<16;
        for(j=0;j<3;++j){float row_offset=(float)dy,x_offset=(float)((long double)e.x-v[low]->position[0]);
            e.field[j]=(float)(((long double)planes[j].dy*row_offset+(long double)planes[j].dx*x_offset)+field_values[low][j]);
            e.advance[j]=(float)((long double)planes[j].dy+(long double)planes[j].dx*e.whole);}
        if(e.first<first)first=e.first;
        if(e.last>last)last=e.last;
        edges[edge_count++]=e;
    }
    if(edge_count<2)return 1;
    if(first< -8192 || last>16384)return 0;
    for(y=first;y<=last;++y){Edge *left=NULL,*right=NULL;
        for(i=0;i<edge_count;++i){Edge *e=&edges[i];if(y<e->first || y>e->last)continue;
            if(!left || e->x<left->x || (e->x==left->x && e->fraction<left->fraction))left=e;
            if(!right || e->x>right->x || (e->x==right->x && e->fraction>right->fraction))right=e;}
        if(left && right && y>=0 && y<f->height){int32_t x=left->x,end=right->x;
            uint32_t u=integer_floor((long double)left->field[0]*65536.0L),vv=integer_floor((long double)left->field[1]*65536.0L),z=integer_floor((long double)left->field[2]*65536.0L);
            uint32_t du=integer_floor((long double)planes[0].dx*65536.0L),dv=integer_floor((long double)planes[1].dx*65536.0L),dz=integer_floor((long double)planes[2].dx*65536.0L);
            if(end>f->width)end=f->width;
            if(x<0){uint32_t skip=(uint32_t)(-(int64_t)x);u+=du*skip;vv+=dv*skip;z+=dz*skip;x=0;}
            for(;x<end;++x,u+=du,vv+=dv,z+=dz){uint8_t sample=color;uint16_t z16=(uint16_t)(z>>16);uint8_t *at_depth=NULL;
                if(x<0)continue;
                if(textured){int64_t offset=(int64_t)fixed_integer(vv)*texture->width+fixed_integer(u);
                    if(offset<0 || (uint64_t)offset>=texture->bytes)return 0;
                    sample=texture->pixels[(size_t)offset];}
                if((flags&0x100u) && sample==254)continue;
                if(depth){at_depth=f->depth+(size_t)y*f->depth_stride+(uint32_t)x*2u;
                    if((flags&0x10u) && !(z16<depth_read(at_depth)))continue;}
                f->color[(size_t)y*f->color_stride+(uint32_t)x]=sample;
                if(flags&8u)depth_write(at_depth,z16);
            }
        }
        for(i=0;i<edge_count;++i)if(y>=edges[i].first && y<edges[i].last)edge_step(&edges[i],planes);
    }
    return 1;
}
