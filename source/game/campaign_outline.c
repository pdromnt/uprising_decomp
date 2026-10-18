#include "campaign_planet.h"
#include "menu_widgets.h"
#include "gameplay_geometry.h"
#include <math.h>
#include <string.h>
extern int game_campaign_project_point(GameApplication *,float[3]);
extern int game_menu_frame_rectangles(GameApplication *,const GameUiRect *,const uint8_t *,size_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int integer(long double value,int32_t *out){if(!isfinite(value) || value<INT32_MIN || value>=2147483648.0L)return 0;*out=(int32_t)value;return 1;}
/*45bff0: the projected descriptor radius, original six-pixel margin and two
 * palette borders. Bounds are the preview's actual origin/dimensions. */
int game_campaign_planet_selection_draw(GameApplication *a,GameAddress planet,const GameUiRect *bounds){
    GameAddress model=get(a,planet),descriptor=get(a,model+0x50);float center[3],edge[3],dx,dy,radius;long double yd,sum;
    GameUiRect rectangles[2];static const uint8_t colors[]={47,31};int32_t cx,cy,left,top,right,bottom;unsigned i;
    if(!bounds || !model || !descriptor)return 0;center[0]=real(a,model+0x10);center[1]=real(a,model+0x20);center[2]=real(a,model+0x30);
    memcpy(edge,center,sizeof(edge));edge[0]=(float)((long double)edge[0]+real(a,descriptor+0x20));
    if(!game_campaign_project_point(a,center) || !game_campaign_project_point(a,edge))return 0;
    dx=(float)((long double)edge[0]-center[0]);yd=(long double)edge[1]-center[1];dy=(float)yd;sum=(long double)dx*dx+yd*dy;
    radius=(float)((gameplay_geometry_square_root((float)sum)+real(a,0x5737a4)+real(a,0x5737a4))*real(a,0x5737a8)*real(a,0x5737ac));
    if(!integer(center[0],&cx) || !integer(center[1],&cy))return !a->memory.fault;
    if((long double)cx-radius<0 || (long double)cy-radius<0)return !a->memory.fault;
    if(!integer((long double)center[0]+radius,&right) || !integer((long double)center[1]+radius,&bottom))return !a->memory.fault;
    /*The last retail preflight adds origin.x to height, not origin.y. */
    if((int64_t)right+bounds->left>bounds->right || (int64_t)bottom+bounds->top>(int64_t)bounds->bottom-bounds->top+bounds->left)return !a->memory.fault;
    for(i=0;i<2;++i){float r=i?(float)((long double)radius-real(a,0x573798)):radius;
        if(!integer((long double)center[0]-r,&left) || !integer((long double)center[1]-r,&top) ||
           !integer((long double)center[0]+r,&right) || !integer((long double)center[1]+r,&bottom))return !a->memory.fault;
        if((int64_t)left+bounds->left<INT32_MIN || (int64_t)right+bounds->left>INT32_MAX ||
           (int64_t)top+bounds->top<INT32_MIN || (int64_t)bottom+bounds->top>INT32_MAX)return 0;
        rectangles[i].left=left+bounds->left;rectangles[i].right=right+bounds->left;
        rectangles[i].top=top+bounds->top;rectangles[i].bottom=bottom+bounds->top;}
    return !a->memory.fault && game_menu_frame_rectangles(a,rectangles,colors,2);
}
