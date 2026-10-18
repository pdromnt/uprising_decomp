#include "vehicle_death.h"
#include "game_matrix.h"
#include <math.h>
#include <string.h>
static uint32_t get(GameMemory *m,GameAddress p){return game_memory_read_u32(m,p);}
static int32_t integer(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static float real(GameMemory *m,GameAddress p){uint32_t v=get(m,p);float f;memcpy(&f,&v,4);return f;}
static uint32_t random_word(GameMemory *m,GameAddress multiplier){
    uint32_t n=get(m,0x6308c0);long double v;
    if(n==1024)n=0;
    if(n>1023){m->fault=1;return 0;}
    v=(long double)real(m,0x6308c8+4*n)*real(m,multiplier);
    game_memory_write_u32(m,0x6308c0,n+1);
    return (uint32_t)(uint64_t)(!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v);
}
static void read_matrix(GameMemory *m,GameAddress p,float out[12]){unsigned i;for(i=0;i<12;i++)out[i]=real(m,p+4*i);}
/* Inline death transforms retain extended intermediates until the three position
 * stores. These are local affine calculations, never persistent state mirrors. */
static void position(GameMemory *m,GameAddress o,int second,float out[3]){
    float world[12],local[12],hinge[12];unsigned r,c;
    read_matrix(m,o+0x80,world);read_matrix(m,o+0x180,local);
    if(second)read_matrix(m,o+0x150,hinge);
    for(r=0;r<3;r++){
        long double v=(long double)local[3]*world[r*4];
        v+=(long double)local[7]*world[r*4+1];v+=(long double)local[11]*world[r*4+2];
        v+=world[r*4+3];
        if(second)for(c=0;c<3;c++){
            long double basis=(long double)local[c]*world[r*4];
            basis+=(long double)local[c+4]*world[r*4+1];basis+=(long double)local[c+8]*world[r*4+2];
            v+=basis*hinge[c*4+3];
        }
        out[r]=(float)v;
    }
}
int vehicle_presentation_death(GameMemory *m,GameAddress o,GameAddress other,const VehicleDeathServices *s){
    GameAddress catalog,table,header,child,bones;uint32_t variant,start,end;unsigned i,part,profile;int grouped;
    float p[3],matrix[12],velocity[3]={0,40,0},up[3]={0,100,0};uint16_t heading;
    if(!m || m->fault || !s || !s->begin || !s->sound || !s->mesh || !s->variant ||
       !s->anchors || !s->acquire || !s->emit || !s->discard || !s->terrain || !s->group_remove)return 0;
    catalog=get(m,0x5f0210+(get(m,o+20)&63)*4);table=get(m,o);
    grouped=game_memory_read_u8(m,o+12)==0x21;
    if(m->fault || !s->begin(s->context,o,other,table))return 0;
    if(grouped && integer(get(m,o+0x144))>=0 && (!s->stop_voice || !s->stop_voice(s->context,get(m,o+0x144))))return 0;
    if(grouped)up[1]=0;
    for(i=0;i<3;i++)p[i]=real(m,o+0x18+4*i);
    if(!s->sound(s->context,p,get(m,0x5ac6ac)&1?0x82:0x83))return 0;
    if(other && (get(m,other+16)&2))for(i=0;i<3;i++){
        long double v=(long double)real(m,other+0xb4+4*i)*real(m,grouped?0x574cd4:0x571494);
        if(i==1)v+=real(m,grouped?0x574cd8:0x571498);
        velocity[i]=(float)v;
    }
    read_matrix(m,o+0x80,matrix);header=get(m,get(m,o+0x54)+0x50);
    if(m->fault || !s->mesh(s->context,o,get(m,table+0x30),header,matrix,velocity) ||
       !s->variant(s->context,o,catalog,&variant))return 0;
    if(variant>=256){m->fault=1;return 0;}
    if(!s->anchors(s->context,o,get(m,table+0x1c),integer(get(m,get(m,o+0x258)+4*variant)),get(m,get(m,o+0x25c)+4*variant),up))return 0;
    for(part=0;part<2;part++){
        AttachmentEmission request={0};
        header=get(m,get(m,o+(part?0x1b0:0x1b4))+0x50);
        for(profile=0;profile<3;profile++)if(integer(get(m,0x5a0488+8*profile))>=integer(get(m,header+0x14)) &&
            integer(get(m,0x5a048c+8*profile))>=integer(get(m,header+0x1c)))break;
        if(m->fault || !(child=s->acquire(s->context,(uint16_t)profile)))return 0;
        start=(uint32_t)(int32_t)(int16_t)game_memory_read_u16(m,o+(part?0x1e0:0x1e2));
        end=(uint32_t)(int32_t)(int16_t)game_memory_read_u16(m,o+(part?0x44:0x1e0));
        bones=get(m,o+0x3c)+start*28;
        if(integer(end-start)<0 || end-start>20 || !attachment_effect_configure(m,child,header,bones,(uint16_t)(end-start)))goto fail;
        if(grouped){float world[12],yaw[12],hinge[12],middle[12],pose[12];
            read_matrix(m,o+0x80,world);read_matrix(m,o+0x180,yaw);game_matrix_compose_values(middle,world,yaw);
            if(part){read_matrix(m,o+0x150,hinge);game_matrix_compose_values(pose,middle,hinge);}else memcpy(pose,middle,sizeof(pose));
            for(i=0;i<3;i++){
                if(part){unsigned row=i*4;long double translation=(long double)world[row]*yaw[3];
                    translation+=(long double)world[row+1]*yaw[7];translation+=(long double)world[row+2]*yaw[11];
                    translation+=world[row+3];
                    /*The inline second-part path rounds its intermediate basis,
                     * but carries the parent translation until the final store. */
                    translation+=(long double)middle[row+2]*hinge[11];
                    translation+=(long double)middle[row+1]*hinge[7];translation+=(long double)middle[row]*hinge[3];
                    request.position[i]=(float)translation;
                }else request.position[i]=pose[i*4+3];
            }
            if(!game_matrix_extract_angles(m,pose,request.angles))goto fail;
        }else position(m,o,part,request.position);
        heading=(uint16_t)((game_memory_read_u16(m,o+0x24)+game_memory_read_u16(m,o+0x14c))&2047);
        if(!grouped){request.angles[0]=heading;request.angles[1]=part?game_memory_read_u16(m,o+0x14a):0;}
        request.velocity[0]=(float)integer(random_word(m,grouped?0x574ce0:0x5714a4)+20);
        request.velocity[1]=(float)integer(random_word(m,grouped?0x574ce0:0x5714a4)+30);
        request.velocity[2]=(float)integer(random_word(m,grouped?0x574ce0:0x5714a4)+20);
        if(random_word(m,grouped?0x574ce4:0x5714a8))request.velocity[0]=-request.velocity[0];
        if(random_word(m,grouped?0x574ce4:0x5714a8))request.velocity[2]=-request.velocity[2];
        if((grouped || !part) && other && game_memory_read_u8(m,other+12)==12)request.velocity[1]=(float)((long double)request.velocity[1]+real(m,grouped?0x574ce8:0x5714ac));
        request.duration=part?0:random_word(m,grouped?0x574cec:0x5714b0)+60;request.mode=part?0:3;request.flags=0x17;request.source=get(m,o+20);
        if(m->fault || !s->emit(s->context,catalog,child,&request))goto fail;
        continue;
fail:  (void)s->discard(s->context,child);return 0;
    }
    if(!s->terrain(s->context,real(m,o+0x18),real(m,o+0x20),1024))return 0;
    if(game_memory_read_u8(m,o+12)==0x23 && game_memory_read_u16(m,catalog+8)==4)
        return !m->fault && s->group_remove(s->context,get(m,catalog+0x2e0),get(m,o+20));
    return !m->fault;
}
