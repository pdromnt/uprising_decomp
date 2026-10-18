#include "level_load.h"
#include "recyclable_records.h"
#include "scene_projection_storage.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef GAME_NAV_SAMPLE_TRACE
#define GAME_NAV_SAMPLE_TRACE(a,row,radius,point,include) ((void)0)
#endif

typedef struct NavigationRing {unsigned count;uint8_t x[240],z[240];} NavigationRing;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t signed_word(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static float real(GameApplication *a,GameAddress p){uint32_t v=get(a,p);float f;memcpy(&f,&v,4);return f;}
static uint32_t truncate_word(long double value){
    value=truncl(value);
    if(!isfinite(value) || value< -0x1p63L || value>=0x1p63L)return 0;
    return (uint32_t)(uint64_t)(int64_t)value;
}
static void circle_pixel(uint8_t bitmap[800],int x,int z){
    bitmap[(unsigned)z*10+(unsigned)x/8]|=(uint8_t)(1u<<(x&7));
}
/*4c8450 builds each circle with integer-exact1-radius arithmetic, then walks
 *its raster in the original neighbor order. These are temporary local values;
 *the bitmap rows below remain the sole published navigation representation. */
static int circle(unsigned radius,NavigationRing *ring){
    static const int dx[8]={1,1,1,0,0,-1,-1,-1},dz[8]={0,1,-1,1,-1,0,1,-1};
    uint8_t bitmap[800]={0};int x=0,z=(int)radius,decision=1-(int)radius,cx=40,cz=40+(int)radius;
    unsigned i,limit=radius*6;
    circle_pixel(bitmap,40,40+z);circle_pixel(bitmap,40,40-z);
    circle_pixel(bitmap,40+z,40);circle_pixel(bitmap,40-z,40);
    do{
        ++x;
        if(decision<0)decision+=2*x+1;
        else{--z;decision+=2*(x-z)+1;}
        circle_pixel(bitmap,40+x,40+z);circle_pixel(bitmap,40+x,40-z);
        circle_pixel(bitmap,40-x,40+z);circle_pixel(bitmap,40-x,40-z);
        circle_pixel(bitmap,40+z,40+x);circle_pixel(bitmap,40+z,40-x);
        circle_pixel(bitmap,40-z,40+x);circle_pixel(bitmap,40-z,40-x);
    }while(z>x);
    ring->count=1;ring->x[0]=(uint8_t)cx;ring->z[0]=(uint8_t)cz;
    for(;;){
        int found=0;
        bitmap[cz*10+cx/8]&=(uint8_t)~(1u<<(cx&7));
        for(i=0;i<8;++i){int nx=cx+dx[i],nz=cz+dz[i];
            if(nx<0 || nx>=80 || nz<0 || nz>=80 || !(bitmap[nz*10+nx/8]&(1u<<(nx&7))))continue;
            if(ring->count>=limit || ring->count>=sizeof(ring->x))return 0;
            cx=nx;cz=nz;ring->x[ring->count]=(uint8_t)cx;ring->z[ring->count]=(uint8_t)cz;
            ++ring->count;found=1;break;
        }
        if(!found)return 1;
    }
}
static int blocked(GameApplication *a,int x,int z){
    if(x<0 || x>=256 || z<0 || z>=256)return 1;
    return (game_memory_read_u8(&a->memory,0x610520+(unsigned)z*32+(unsigned)x/8)&(1u<<(x&7)))!=0;
}
static int clear_neighbors(GameApplication *a,unsigned x,unsigned z){
    unsigned i;
    for(i=0;i<8;++i){
        int64_t nx=(int64_t)x+signed_word(get(a,0x594378+i*8));
        int64_t nz=(int64_t)z+signed_word(get(a,0x59437c+i*8));
        if(nx<0 || nx>=256 || nz<0 || nz>=256 || blocked(a,(int)nx,(int)nz))return 0;
    }
    return !a->memory.fault;
}
static int random_sample(GameApplication *a,int *include){
    uint32_t index=get(a,0x6308c0),value;
    if(index==1024){index=0;put(a,0x6308c0,0);}
    if(index>1023){a->memory.fault=1;return 0;}
    value=truncate_word((long double)real(a,0x6308c8+index*4)*real(a,0x574928));
    put(a,0x6308c0,index+1);*include=signed_word(value)<signed_word(get(a,0x604250));return !a->memory.fault;
}
int game_level_navigation_routes_release(GameApplication *a,GameAddress owner){
    GameMemory cleanup=a->memory;GameAddress rows;uint32_t count;unsigned i;
    /*A bounded view of the same bytes permits failure cleanup while retaining
     *the caller's error. It creates no second writable state or serialized copy. */
    cleanup.fault=0;rows=game_memory_read_u32(&cleanup,owner);count=game_memory_read_u32(&cleanup,owner+4);
    if(cleanup.fault)return 0;
    if(rows){
        if(signed_word(count)>32767){a->memory.fault=1;return 0;}
        for(i=0;signed_word(i)<signed_word(count);++i){GameAddress row=game_memory_read_u32(&cleanup,rows+i*4);
            if(row && !scene_projection_storage_free(row))return 0;
            game_memory_write_u32(&cleanup,rows+i*4,0);
        }
        if(!scene_projection_storage_free(rows))return 0;
        game_memory_write_u32(&cleanup,owner,0);
    }
    game_memory_write_u32(&cleanup,owner+4,0);return !cleanup.fault;
}
/*4c8450: existing citadel bounds and actual route searches select cells on
 *radius16..39 contours. Rows are800 bytes, addressed by local80x80 coordinates.
 *Fixed-width IDs address owned pages; no host pointer is stored in a row/table. */
int game_level_navigation_routes_build(GameApplication *a,GameAddress owner){
    NavigationRing rings[24];GameMemory *m=&a->memory;GameAddress rows,route=0;
    uint16_t encoded_count=game_memory_read_u16(m,0x639a74);
    int count=encoded_count<32768?encoded_count:(int)encoded_count-65536;unsigned i,r;
    memset(rings,0,sizeof(rings));
    for(r=0;r<24;++r)if(!circle(r+16,&rings[r])){m->fault=1;return 0;}
    if(!game_level_navigation_routes_release(a,owner))return 0;
    put(a,owner+4,(uint32_t)count);
    if(count<=0)return !m->fault;
    rows=scene_projection_storage_allocate((size_t)count*4,"navigation row IDs");
    if(!rows)return 0;
    put(a,owner,rows);
    for(i=0;i<(unsigned)count && !m->fault;++i){
        GameAddress row=scene_projection_storage_allocate(800,"citadel navigation bitmap");
        GameAddress citadel=get(a,get(a,0x639a70)+i*4);uint8_t sx,sz;
        if(!row)return 0;
        put(a,rows+i*4,row);
        if(!game_memory_fill_u32(m,row,200,0,0))return 0;
        sx=(uint8_t)((truncate_word(((long double)real(a,citadel+4)+real(a,citadel+12))*real(a,0x574934))>>9)&255);
        sz=(uint8_t)((truncate_word(((long double)real(a,citadel+8)+real(a,citadel+16))*real(a,0x574934))>>9)&255);
        for(r=0;r<24 && !m->fault;++r){
            NavigationRing *ring=&rings[r];uint8_t x[240]={0},z[240]={0};unsigned p,first,begin,end,visits=0;
            for(p=0;p<ring->count;++p){int tx=(int)sx+ring->x[p]-40,tz=(int)sz+ring->z[p]-40;
                if(!blocked(a,tx,tz)){x[p]=(uint8_t)tx;z[p]=(uint8_t)tz;}}
            for(first=0;first<ring->count && (x[first] || z[first]);++first){}
            /*An entirely clear contour deliberately publishes no cells. */
            if(first==ring->count)continue;
            ++first;while(first<ring->count && !x[first] && !z[first])++first;
            if(first==ring->count)continue;
            begin=first;
            do{
                unsigned length;
                if(++visits>ring->count){m->fault=1;return 0;}
                end=(begin+1)%ring->count;while(x[end] || z[end])end=(end+1)%ring->count;
                length=(end+ring->count-begin)%ring->count;
                if(length>1 && length<25){
                    uint32_t found;unsigned attempts=1,stop=end,start=begin;int admitted;
                    route=recyclable_record_allocate(0x24);
                    if(!route || !recyclable_outer_initialize(m,route,NULL))goto failed;
                    /*The contour point is the search origin; the citadel is
                     *the destination. Reversing them changes probe admission. */
                    found=recyclable_route_begin(m,0x60a4c8,x[begin],z[begin],sx,sz,route,0xbf800000,NULL);
                    while(found){++attempts;found=recyclable_route_continue(m,0x60a4c8,route,NULL);if(attempts>=10)break;}
                    if(attempts<10 && signed_word(get(a,route+0x18))<0x3f800000){
                        if(length>5){start=(start+1)%ring->count;stop=(stop+ring->count-1)%ring->count;}
                        for(p=start;p!=stop;p=(p+1)%ring->count){
                            admitted=1;
                            if(length>10){
                                if(!random_sample(a,&admitted))goto failed;
                                GAME_NAV_SAMPLE_TRACE(a,i,r,p,admitted);
                            }
                            if(admitted && clear_neighbors(a,x[p],z[p])){
                                GameAddress at=row+(unsigned)ring->z[p]*10+ring->x[p]/8;
                                game_memory_write_u8(m,at,(uint8_t)(game_memory_read_u8(m,at)|(1u<<(ring->x[p]&7))));
                            }
                        }
                    }
                    if(!recyclable_record_destroy(m,route,1,NULL))goto failed;
                    route=0;
                }
                begin=end;while(!x[begin] && !z[begin])begin=(begin+1)%ring->count;
            }while(begin!=first && !m->fault);
        }
    }
    return !m->fault;
failed:
    if(route){GameMemory cleanup=*m;cleanup.fault=0;(void)recyclable_record_destroy(&cleanup,route,1,NULL);}
    return 0;
}
