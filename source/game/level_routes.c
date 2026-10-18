#include "level_routes.h"
#include "level_movement_paths.h"
#include "scene_projection_storage.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*The existing parser owns definitions; these are the sole live route records
 * read by movement, faction streaming and teardown. No typed writable mirror. */
struct GameLevelRoutes {
    GameAddress blocks[2+LEVEL_MOVEMENT_PATHS_MAX*3];
    unsigned block_count,gate_count,ordinary_count;
    GameAddress gates,ordinary;
    int gate_enabled;
};
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float scalar(GameApplication *a,GameAddress p) {
    uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;
}
static void put_float(GameApplication *a,GameAddress p,float v) {
    uint32_t bits;memcpy(&bits,&v,4);put(a,p,bits);
}
static GameAddress allocate(GameApplication *a,size_t size,const char *name) {
    struct GameLevelRoutes *r=a->level_routes;GameAddress p;
    if(r->block_count>=sizeof(r->blocks)/sizeof(r->blocks[0]))return 0;
    p=scene_projection_storage_allocate(size?size:1,name);
    if(p)r->blocks[r->block_count++]=p;
    return p;
}
static int gate_name(const char *name) {
    return tolower((unsigned char)name[0])=='g' && tolower((unsigned char)name[1])=='a' &&
        tolower((unsigned char)name[2])=='t' && tolower((unsigned char)name[3])=='e';
}
static uint32_t integer(long double v) {
    /*560db0 truncates, including the original indefinite result out of range. */
    return !isfinite(v)||v<-2147483648.0L||v>=2147483648.0L?0x80000000u:(uint32_t)(int32_t)v;
}
static uint16_t heading(GameApplication *a,const float v[3]) {
    long double scale=scalar(a,0x573898),index;uint16_t angle;uint32_t bits;
    if(fabsl(v[0])<fabsl(v[2])) {
        index=(long double)v[0]*scale/v[2]+scale;
        if(!isfinite(index)||index<0||index>=2049){a->memory.fault=1;return 0;}
        angle=game_memory_read_u16(&a->memory,0x62e8b0+integer(index)*2);
        memcpy(&bits,&v[2],4);
        angle=(int32_t)bits>0?(uint16_t)(angle&2047):(uint16_t)(angle+1024);
    } else {
        index=(long double)v[2]*scale/v[0]+scale;
        if(!isfinite(index)||index<0||index>=2049){a->memory.fault=1;return 0;}
        memcpy(&bits,&v[0],4);
        angle=(uint16_t)(((int32_t)bits>0?512:1536)-
            game_memory_read_u16(&a->memory,0x62e8b0+integer(index)*2));
    }
    return (uint16_t)(angle+512);
}
/*476440/476460/42d370: stable identity allocation and faction list insertion. */
static uint32_t identity(GameApplication *a,GameAddress faction) {
    uint32_t n=get(a,faction+0x18);put(a,faction+0x18,n+1);
    return (n<<6)|game_memory_read_u16(&a->memory,faction+8);
}
static void attach(GameApplication *a,GameAddress faction,GameAddress entity) {
    uint32_t flags=get(a,entity+0x10);
    GameAddress root=faction+(!(flags&1)?0xe4:(flags&64)?0x80:0x1c),previous=get(a,root+0x48);
    put(a,entity+0x4c,root);put(a,entity+0x48,previous);
    if(previous)put(a,previous+0x4c,entity);
    put(a,root+0x48,entity);
}
/*45f7f0/45f700's mode1 occupancy publications for both established grids. */
static void mark(GameApplication *a,const float p[3]) {
    unsigned x=(integer(p[0])>>9)&255,z=(integer(p[2])>>9)&255;
    GameAddress first=0x60a4c8+0x6058+z*32+x/8;
    GameAddress second=0x5cf038+0x11834+(z/2)*16+(x/2)/8;
    game_memory_write_u8(&a->memory,first,(uint8_t)(game_memory_read_u8(&a->memory,first)|(1u<<(x&7))));
    game_memory_write_u8(&a->memory,second,(uint8_t)(game_memory_read_u8(&a->memory,second)|(1u<<((x/2)&7))));
}
/*45f1f0: construct the actual chain of gate entities along each segment. */
static int construct_gate(GameApplication *a,GameAddress record,const LevelMovementPath *path) {
    unsigned segment,j;float step=scalar(a,0x57389c);GameAddress node;
    if(strlen(path->name)>=32 || !(step>0) || !isfinite(step))return 0;
    if(!game_memory_write(&a->memory,record,path->name,strlen(path->name)+1))return 0;
    put(a,record+0x20,0);put(a,record+0x24,1);
    for(segment=0;segment+1<path->point_count;++segment) {
        float base[3],end[3],delta[3],unit[3],increment[3],offset[3]={0},distance=0,length;
        const LevelMovementPoint *p=&path->points[segment],*q=p+1;
        uint16_t angle;long double sum;
        base[0]=p->x*512.0f;base[1]=p->y*512.0f;base[2]=p->z*512.0f;
        end[0]=q->x*512.0f;end[1]=q->y*512.0f;end[2]=q->z*512.0f;
        for(j=0;j<3;++j)delta[j]=(float)((long double)end[j]-base[j]);
        sum=(long double)delta[1]*delta[1]+(long double)delta[2]*delta[2]+(long double)delta[0]*delta[0];
        length=(float)sqrtl((float)sum);
        if(!(length>0) || !isfinite(length))return 0;
        for(j=0;j<3;++j) {
            unit[j]=length==1.0f?delta[j]:(float)((long double)delta[j]/length);
            increment[j]=(float)((long double)unit[j]*step);
        }
        angle=heading(a,unit);if(a->memory.fault)return 0;
        do {
            float position[3];GameAddress faction=get(a,0x5f0210);uint32_t id;
            for(j=0;j<3;++j)position[j]=(float)((long double)base[j]+offset[j]);
            node=game_level_gate_create(a);if(!node || !faction)return 0;
            put(a,node+0xcc,get(a,record+0x20));put(a,record+0x20,node);
            id=identity(a,faction);
            if(!game_level_gate_place(a,node,id,position,angle,segment==0 && distance==0))return 0;
            attach(a,faction,node);
            for(j=0;j<3;++j)offset[j]=(float)((long double)offset[j]+increment[j]);
            {float next=(float)((long double)distance+step);if(!(next>distance))return 0;distance=next;}
        }while(distance<length && !a->memory.fault);
    }
    for(node=get(a,record+0x20);node && !a->memory.fault;node=get(a,node+0xcc)) {
        float position[3]={scalar(a,node+0x18),scalar(a,node+0x1c),scalar(a,node+0x20)};
        mark(a,position);
    }
    return !a->memory.fault;
}
static int begin(void *context,uint32_t count,const LevelMovementPath *paths) {
    GameApplication *a=context;struct GameLevelRoutes *r=a->level_routes;unsigned i,gates=0;
    r->gate_enabled=get(a,0x5ac688)!=0;
    for(i=0;i<count;++i)if(r->gate_enabled && gate_name(paths[i].name))++gates;
    if(gates && !(r->gates=allocate(a,gates*4,"movement gate table")))return 0;
    if(count>gates && !(r->ordinary=allocate(a,(count-gates)*4,"movement ordinary table")))return 0;
    put(a,0x5e147c,0);put(a,0x5e1480,0);put(a,0x60589c,0);put(a,0x605880,0);
    return !a->memory.fault;
}
static int construct(void *context,uint32_t index,const LevelMovementPath *path,int *gate) {
    GameApplication *a=context;struct GameLevelRoutes *r=a->level_routes;GameAddress record,name,points;unsigned i;
    (void)index;*gate=r->gate_enabled && gate_name(path->name);
    record=allocate(a,*gate?0x28:0x10,*gate?"movement gate record":"movement ordinary record");
    if(!record)return 0;
    if(*gate) {
        if(!construct_gate(a,record,path))return 0;
        put(a,r->gates+r->gate_count++*4,record);
    }else {
        points=allocate(a,path->point_count*12,"movement route points");
        name=allocate(a,strlen(path->name)+1,"movement route name");
        if(!points || !name || !game_memory_write(&a->memory,name,path->name,strlen(path->name)+1))return 0;
        for(i=0;i<path->point_count;++i) {
            put_float(a,points+i*12,path->points[i].x*512.0f);
            put_float(a,points+i*12+4,path->points[i].y*512.0f);
            put_float(a,points+i*12+8,path->points[i].z*512.0f);
        }
        put(a,record,points);put(a,record+4,path->point_count);put(a,record+8,(uint32_t)path->flags);put(a,record+12,name);
        put(a,r->ordinary+r->ordinary_count++*4,record);
    }
    return !a->memory.fault;
}
static int publish(void *context) {
    GameApplication *a=context;struct GameLevelRoutes *r=a->level_routes;
    put(a,0x5e147c,r->gate_count);put(a,0x5e1480,r->gates);
    put(a,0x60589c,r->ordinary_count);put(a,0x605880,r->ordinary);
    return !a->memory.fault;
}
int game_level_routes_load(GameApplication *a,const char *path,int32_t offset,uint32_t *next) {
    if(a->level_routes)return 0;
    a->level_routes=calloc(1,sizeof(*a->level_routes));if(!a->level_routes)return 0;
    /*Failure leaves attempted allocations owned for the level rollback. Gate
     * entities belong to the existing faction/entity owner, never this table. */
    return level_movement_paths_load(path,offset,a,begin,construct,publish,next);
}
/*Gate archives contain each segment's phase, but omit the route's cached
 *last command. Repair that legacy reload inconsistency without changing the
 *wire format. The head carries the latest command even while later segments
 *are waiting for the13-tick propagation, or still finishing an earlier move. */
int game_level_gate_restore_route_flags(GameApplication *a){
    uint32_t count=get(a,0x5e147c),i;GameAddress rows=get(a,0x5e1480);
    if(a->memory.fault || count>65536)return 0;
    for(i=0;i<count;i++){
        GameAddress record=get(a,rows+i*4),head;uint32_t state;
        if(!record)return 0;
        head=get(a,record+0x20);if(!head)continue;
        state=get(a,head+0xb8);if(a->memory.fault || state>3)return 0;
        put(a,record+0x24,state==0 || state==2);
    }
    return !a->memory.fault;
}
int game_level_routes_release(GameApplication *a) {
    struct GameLevelRoutes *r=a->level_routes;
    if(r) {
        while(r->block_count) {
            if(!scene_projection_storage_free(r->blocks[r->block_count-1]))return 0;
            --r->block_count;
        }
        free(r);a->level_routes=NULL;
    }
    level_movement_paths_unload();
    put(a,0x5e147c,0);put(a,0x5e1480,0);put(a,0x60589c,0);put(a,0x605880,0);
    return !a->memory.fault;
}
