#include "terrain_edge_stream.h"

#include <math.h>
#include <string.h>

enum { TERRAIN_EDGE_CAPACITY=8 };

static int32_t integer_part(long double value) {
    if (!isfinite(value) || value >= 9223372036854775808.0L ||
        value < -9223372036854775808.0L) return 0;
    return (int32_t)(int64_t)truncl(value);
}

/* The source converts toward zero, then subtracts one for negative float
   bit patterns other than negative zero. Keep its exact-grid convention. */
static int32_t grid_boundary(float value) {
    uint32_t bits;
    int32_t integer=integer_part((long double)value);
    memcpy(&bits,&value,sizeof(bits));
    return bits>0x80000000u?integer-1:integer;
}

static int edge_side(const GameplayTerrainPoint *a,
    const GameplayTerrainPoint *b,const GameplayTerrainPoint *c) {
    long double first=((long double)a->z-b->z)*((long double)c->x-b->x);
    long double second=((long double)c->z-b->z)*((long double)a->x-b->x);
    return second-first>0.0L;
}

static int edge_emit(GameplayTerrainEdge *edge,
    const GameplayTerrainPoint *a,const GameplayTerrainPoint *b) {
    float az=(float)((long double)a->z*0.001953125L);
    float bz=(float)((long double)b->z*0.001953125L);
    int32_t ar=grid_boundary(az),br=grid_boundary(bz);
    float ax,bx,low_x,high_x,low_z,high_z;
    long double slope,column;
    if(ar==br)return 0;
    ax=(float)((long double)a->x*0.001953125L);
    bx=(float)((long double)b->x*0.001953125L);
    if(br<ar){
        edge->first_row=br;edge->last_row=ar;
        low_x=bx;high_x=ax;low_z=bz;high_z=az;
    }else{
        edge->first_row=ar;edge->last_row=br;
        low_x=ax;high_x=bx;low_z=az;high_z=bz;
    }
    edge->first_column=grid_boundary(low_x);
    edge->last_column=grid_boundary(high_x);
    slope=((long double)high_x-low_x)/((long double)high_z-low_z);
    column=(long double)low_x+((long double)edge->first_row+1.0L-low_z)*slope;
    edge->column_16_16=integer_part(column*65536.0L);
    edge->column_step_16_16=integer_part(slope*65536.0L);
    return 1;
}

int gameplay_terrain_edge_stream_build(const GameplayTerrainPoint points[5],
    GameplayTerrainEdgeStream *stream) {
    static const uint8_t pairs[8][2]={{4,1},{1,2},{2,3},{3,4},
        {1,0},{2,0},{3,0},{4,0}};
    static const uint8_t bits[8]={0x80,0x10,0x20,0x40,1,2,4,8};
    uint32_t mask=0,count=0,i,j;
    int32_t farthest_row,endpoint[2]={0,0};
    unsigned slot=1;
    if(!points || !stream)return 0;
    memset(stream,0,sizeof(*stream));
    if(edge_side(&points[4],&points[1],&points[2]))mask=0xf0u;
    if(edge_side(&points[1],&points[0],&points[2]))mask^=0x13u;
    if(edge_side(&points[3],&points[0],&points[4]))mask^=0x4cu;
    if(edge_side(&points[4],&points[0],&points[1]))mask^=0x89u;
    if(edge_side(&points[2],&points[0],&points[3]))mask^=0x26u;
    for(i=0;i<TERRAIN_EDGE_CAPACITY;i++){
        if((mask&bits[i]) && edge_emit(&stream->edges[count],
                &points[pairs[i][0]],&points[pairs[i][1]]))++count;
    }
    for(i=1;i<count;i++){
        GameplayTerrainEdge current=stream->edges[i];
        j=i;
        while(j && current.first_row<stream->edges[j-1].first_row){
            stream->edges[j]=stream->edges[j-1];--j;
        }
        stream->edges[j]=current;
    }
    if(count){
        farthest_row=stream->edges[0].last_row;
        endpoint[0]=stream->edges[0].last_column;
        for(i=1;i<count;i++){
            if(farthest_row<=stream->edges[i].last_row){
                farthest_row=stream->edges[i].last_row;
                endpoint[slot]=stream->edges[i].last_column;
                slot=1u-slot;
            }
        }
    }
    stream->edges[count].column_16_16=(int32_t)((uint32_t)endpoint[0]<<16);
    stream->edges[count+1].column_16_16=(int32_t)((uint32_t)endpoint[1]<<16);
    stream->count=count;
    return 1;
}
