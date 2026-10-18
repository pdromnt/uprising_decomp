#include "campaign_world.h"
#include "gameplay_geometry.h"
#include <string.h>
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
/*4e5e20: the shared view/projection globals are bounded numeric values. Rows
 * store to float before perspective division; near clipping compares the
 * extended depth before that store, as in the reference. */
int game_campaign_project_point(GameApplication *a,float point[3]) {
    float view[3];long double depth=0,factor;unsigned row;
    if(!point)return 0;
    for(row=0;row<3;++row){GameAddress m=0x5cedf0+row*16;
        long double value=(long double)point[2]*real(a,m+8)+(long double)point[1]*real(a,m+4);
        value+=(long double)point[0]*real(a,m);value+=real(a,m+12);view[row]=(float)value;if(row==2)depth=value;
    }
    if(!(depth>=real(a,0x574cb4))){point[0]=point[1]=point[2]=-1000;return !a->memory.fault;}
    factor=(long double)real(a,0x574cb0)/view[2];
    point[0]=(float)((long double)real(a,0x6290c8)-factor*view[0]*real(a,0x6290f4));
    point[1]=(float)((long double)real(a,0x6290cc)-factor*view[1]*real(a,0x6290f0)*real(a,0x6290f4));
    point[2]=view[2];return !a->memory.fault;
}
/*45c360: the original picks a projected circular radius from the model's
 * first polygon vertex, not an invented ray/triangle collision algorithm.
 * The live rendering pass owns/projected vertices; this consumer only reads. */
int game_campaign_planet_pick(GameApplication *a,GameAddress planet,int32_t x,int32_t y,float *depth) {
    GameAddress model=get(a,planet),descriptor=get(a,model+0x50),polygon=get(a,descriptor+0x18),vertex;
    float point[3],dx,dy,argument,radius;long double extended_y,distance;
    if(!model || !descriptor || !polygon || !depth || !get(a,descriptor+0x1c))return 0;
    point[0]=real(a,model+0x10);point[1]=real(a,model+0x20);point[2]=real(a,model+0x30);
    if(!game_campaign_project_point(a,point))return 0;*depth=point[2];
    vertex=get(a,get(a,polygon+4));if(!vertex)return 0;
    dx=(float)((long double)real(a,vertex)-point[0]);extended_y=(long double)real(a,vertex+4)-point[1];dy=(float)extended_y;
    argument=(float)((long double)dx*dx+extended_y*dy);radius=(float)gameplay_geometry_square_root(argument);
    dx=(float)((long double)x-point[0]);dy=(float)((long double)y-point[1]);
    argument=(float)((long double)dx*dx+(long double)dy*dy);distance=gameplay_geometry_square_root(argument);
    return !a->memory.fault && !(distance>=radius);
}
