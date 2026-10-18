#include "player_effects.h"
#include "game_matrix.h"
#include <math.h>
#include <limits.h>
#include <string.h>
static uint32_t get(GameMemory *m,GameAddress p){return game_memory_read_u32(m,p);}
static int32_t integer(uint32_t bits){int32_t v;memcpy(&v,&bits,4);return v;}
static float real(GameMemory *m,GameAddress p){uint32_t bits=get(m,p);float v;memcpy(&v,&bits,4);return v;}
static uint32_t random_word(GameMemory *m,GameAddress multiplier){
    uint32_t n=get(m,0x6308c0);long double v;
    if(n==1024)n=0;
    if(n>1023){m->fault=1;return 0;}
    v=(long double)real(m,0x6308c8+n*4)*real(m,multiplier);game_memory_write_u32(m,0x6308c0,n+1);
    return (uint32_t)(uint64_t)(!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v);
}
static void matrix(GameMemory *m,GameAddress p,float out[12]){unsigned i;for(i=0;i<12;i++)out[i]=real(m,p+i*4);}
/*The inlined first compose stores its basis, but the double-compose arm keeps
 *the translation extended through the hinge addition. The other arm stores
 *the complete first matrix before calling the ordinary shared compose.*/
static void first_compose(float out[12],long double translation[3],const float world[12],const float local[12]){
    unsigned r,c;
    for(r=0;r<3;r++){
        unsigned base=r*4;
        for(c=0;c<3;c++){
            long double v=(long double)world[base]*local[c];v+=(long double)world[base+1]*local[c+4];v+=(long double)world[base+2]*local[c+8];out[base+c]=(float)v;
        }
        translation[r]=(long double)local[7]*world[base+1];translation[r]+=(long double)local[3]*world[base];translation[r]+=(long double)local[11]*world[base+2];translation[r]+=world[base+3];
        out[base+3]=(float)translation[r];
    }
}
static void normal(const float a[3],const float b[3],const float c[3],float out[3],float one){
    float u[3],v[3],squared,length;long double root,factor;unsigned i;
    for(i=0;i<3;i++){u[i]=(float)((long double)a[i]-b[i]);v[i]=(float)((long double)c[i]-b[i]);}
    out[0]=(float)((long double)v[2]*u[1]-(long double)v[1]*u[2]);
    out[1]=(float)((long double)v[0]*u[2]-(long double)v[2]*u[0]);
    out[2]=(float)((long double)v[1]*u[0]-(long double)v[0]*u[1]);
    squared=(float)(((long double)out[2]*out[2]+(long double)out[1]*out[1])+(long double)out[0]*out[0]);
    root=sqrtl(squared);length=(float)root;
    if(!isnan(root) && !isnan(one) && root!=one && length!=0){factor=(long double)one/length;for(i=0;i<3;i++)out[i]=(float)((long double)out[i]*factor);}
}
int player_effect_clearance(GameMemory *m,GameAddress o,const PlayerDeathEffectServices *s,int *out){
    float points[4][3],world[12],local[3],n[3],one;long double detail,terrain,side,threshold;uint32_t bits;unsigned i,j;
    if(!m || m->fault || !s || !s->height || !out)return 0;
    matrix(m,o+0x80,world);one=real(m,0x574d3c);
    for(i=0;i<4;i++){
        for(j=0;j<3;j++)local[j]=real(m,o+0x16c+i*12+j*4);
        game_matrix_transform_point_values(points[i],world,local);
    }
    for(i=0;i<4;i++){
        if(m->fault || !s->height(s->context,points[i][0],points[i][2],&detail,&terrain))return 0;
        points[i][1]=(float)(terrain+(float)detail);
    }
    normal(points[0],points[1],points[2],n,one);
    side=((long double)points[3][1]-points[1][1])*n[1];
    side+=((long double)points[3][2]-points[1][2])*n[2];
    side+=((long double)points[3][0]-points[1][0])*n[0];
    if(side>real(m,0x574d30))normal(points[1],points[2],points[3],n,one);
    memcpy(&bits,n+1,4);
    if(integer(bits)<=0 || integer(bits)>0x3f800000){n[0]=n[2]=0;n[1]=1;}
    threshold=(long double)n[1]*points[1][1]+(long double)n[2]*points[1][2];
    threshold+=(long double)n[0]*points[1][0];
    threshold-=(long double)real(m,o+32)*n[2];threshold-=(long double)real(m,o+24)*n[0];
    threshold+=real(m,o+0x30);threshold/=n[1];threshold+=real(m,0x574d6c);
    *out=threshold>real(m,o+28);return !m->fault;
}
static GameAddress configure(GameMemory *m,GameAddress header,GameAddress bones,int32_t count,const PlayerDeathEffectServices *s){
    unsigned profile;GameAddress child;
    for(profile=0;profile<3;profile++)if(integer(get(m,0x5a0488+profile*8))>=integer(get(m,header+20)) && integer(get(m,0x5a048c+profile*8))>=integer(get(m,header+28)))break;
    if(m->fault || count<0 || count>20 || !(child=s->acquire(s->context,(uint16_t)profile)))return 0;
    if(attachment_effect_configure(m,child,header,bones,(uint16_t)count))return child;
    (void)s->discard(s->context,child);return 0;
}
static int32_t bone(GameMemory *m,GameAddress p){uint16_t bits=game_memory_read_u16(m,p);int16_t value;memcpy(&value,&bits,2);return value;}
static int emit_attachment(GameMemory *m,GameAddress o,GameAddress other,GameAddress catalog,int second,int clearance,const PlayerDeathEffectServices *s){
    GameAddress child,header=get(m,get(m,o+(second?0x228:0x270))+0x50);int32_t start=bone(m,o+(second?0x22c:0x274)),end=bone(m,o+(second?0x44:0x22c));
    float world[12],first[12],hinge[12],composed[12],final[12];long double translation[3];AttachmentEmission request={0};unsigned i;
    child=configure(m,header,get(m,o+0x3c)+(uint32_t)start*28,end-start,s);if(!child)return 0;
    matrix(m,o+0x80,world);matrix(m,o+0x278,first);first_compose(composed,translation,world,first);
    if(second){
        matrix(m,o+0x230,hinge);game_matrix_compose_values(final,composed,hinge);
        if(clearance)for(i=0;i<3;i++){
            long double v=translation[i];v+=(long double)hinge[11]*composed[i*4+2];v+=(long double)hinge[7]*composed[i*4+1];v+=(long double)hinge[3]*composed[i*4];final[i*4+3]=(float)v;
        }
    }else memcpy(final,composed,48);
    for(i=0;i<3;i++)request.position[i]=final[i*4+3];
    if(!game_matrix_extract_angles(m,final,request.angles))goto fail;
    request.velocity[0]=(float)integer(random_word(m,0x574e44)+20);
    request.velocity[1]=(float)integer(random_word(m,0x574df0)+50);
    if(second)request.velocity[1]=(float)((long double)request.velocity[1]+real(m,0x574df0));
    request.velocity[2]=(float)integer(random_word(m,0x574e44)+20);
    if(random_word(m,0x574d78))request.velocity[0]=-request.velocity[0];
    if(random_word(m,0x574d78))request.velocity[2]=-request.velocity[2];
    if(other && game_memory_read_u8(m,other+12)==12)request.velocity[1]=(float)((long double)request.velocity[1]+real(m,0x574e48));
    request.source=get(m,o+20);request.flags=clearance?0x716:0x17;
    request.mode=second?0:3;request.duration=second?0:random_word(m,0x574e4c)+60;
    request.deadline=clearance?(second?0x4a:0x50):0;
    if(m->fault || !s->emit(s->context,catalog,child,&request))goto fail;
    return 1;
fail:(void)s->discard(s->context,child);return 0;
}
int player_death_effect(GameMemory *m,GameAddress o,GameAddress other,const PlayerDeathEffectServices *s){
    GameAddress catalog,table,header,child,records;uint32_t variant;int clearance;unsigned i;float up[3]={0,0,0};
    if(!m || m->fault || !s || !s->begin || !s->variant || !s->acquire || !s->emit || !s->anchors || !s->discard)return 0;
    catalog=get(m,0x5f0210+(get(m,o+20)&63)*4);table=get(m,o);header=get(m,get(m,o+0x54)+0x50);
    if(m->fault || !s->begin(s->context,o,other,table,header) || !s->variant(s->context,catalog,0x20,&variant) || !player_effect_clearance(m,o,s,&clearance))return 0;
    if(clearance){AttachmentEmission request={0};
        child=configure(m,header,get(m,o+0x3c),bone(m,o+0x274),s);if(!child)return 0;
        for(i=0;i<3;i++){request.position[i]=real(m,o+24+i*4);request.angles[i]=game_memory_read_u16(m,o+0x24+i*2);}
        request.source=get(m,o+20);request.flags=0x2b6;request.duration=0x91;
        if(m->fault || !s->emit(s->context,catalog,child,&request)){(void)s->discard(s->context,child);return 0;}
        up[1]=(float)integer(random_word(m,0x574df0)+50);
        /*5563e0 retains anchors for the already published primary attachment.
         *Its555850/555b40 consumers own the later emissions.*/
        records=get(m,0x608148+variant*4);
        if(!attachment_effect_retain_anchors(m,child,integer(get(m,0x608144+variant*4)),records,16,up))return 0;
    }else{
        if(!s->variant(s->context,catalog,game_memory_read_u8(m,o+12),&variant))return 0;
        up[1]=(float)integer(random_word(m,0x574df0)+50);
        if(m->fault || !s->anchors(s->context,o,get(m,table+0x1c),integer(get(m,0x608144+variant*4)),get(m,0x608148+variant*4),up))return 0;
    }
    return !m->fault && emit_attachment(m,o,other,catalog,0,clearance,s) && emit_attachment(m,o,other,catalog,1,clearance,s);
}
