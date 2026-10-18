#include "terrain_asset.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
static uint16_t u16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static int16_t signed16(uint16_t value){return value<32768?(int16_t)value:(int16_t)((int32_t)value-65536);}
static float f32(uint32_t bits){float value;memcpy(&value,&bits,4);return value;}
void terrain_asset_light_direction(float out[3]) {
    long double scale=1.0L/sqrtl(f32(0x403c0479u));
    out[0]=(float)(f32(0xbf2aacdau)*scale);out[1]=(float)scale;out[2]=(float)(f32(0x3f9c6a7fu)*scale);
}
uint8_t terrain_asset_light(int16_t center,int16_t east,int16_t south,int16_t west,int16_t north,const float direction[3],uint32_t *seed) {
    float xs[4],zs[4],sum=0;unsigned i;float value;
    xs[0]=xs[1]=(float)(-(int32_t)(east-center)*512);xs[2]=xs[3]=(float)((int32_t)(west-center)*512);
    zs[0]=zs[3]=(float)(-(int32_t)(south-center)*512);zs[1]=zs[2]=(float)((int32_t)(north-center)*512);
    for(i=0;i<4;i++) {
        float magnitude=(float)((long double)zs[i]*zs[i]+(long double)xs[i]*xs[i]+68719476736.0L);
        long double inverse=1.0L/sqrtl(magnitude);
        float nx=(float)(xs[i]*inverse),nz=(float)(zs[i]*inverse);
        float dot=(float)((long double)direction[0]*nx+((inverse*262144.0L)*direction[1]+(long double)direction[2]*nz));
        if(dot>0)sum=(float)((long double)sum+dot);
    }
    sum=(float)((long double)sum*0.25f);
    *seed=*seed*0x343fdu+0x269ec3u;
    value=(float)((long double)((*seed>>16)&32767u)*0.000030518509447574615f*0.2222222238779068f+sum-0.1111111119389534f);
    if(value<0)value=0;
    if(value>1)value=1;
    return (uint8_t)(int)((long double)value*175.0f);
}
size_t terrain_asset_payload_size(unsigned width,unsigned height) {
    if(!width || !height || width>256 || height>256)return 0;
    return (size_t)width*height*3+((size_t)width+1)*(height+1)*2;
}
void terrain_asset_release(TerrainAsset *terrain){if(terrain){free(terrain->cells);free(terrain->materials);memset(terrain,0,sizeof(*terrain));}}
int terrain_asset_decode(const uint8_t *data,size_t size,unsigned width,unsigned height,int lighting,uint32_t *seed,TerrainAsset *out) {
    TerrainAsset terrain={0};size_t required=terrain_asset_payload_size(width,height),pos=0;unsigned x,y,i;float direction[3];
    if(!data || !out || !seed || !required || size<required)return 0;
    terrain.cells=calloc(65536,sizeof(*terrain.cells));if(!terrain.cells)return 0;
    terrain.width=(uint16_t)width;terrain.height=(uint16_t)height;terrain.minimum=32767;terrain.maximum=-32768;
    for(i=0;i<65536;i++)terrain.cells[i].tile=65535;
    for(i=0;i<4096;i++)terrain.coarse[i]=-1;
    for(y=0;y<height;y++)for(x=0;x<width;x++){terrain.cells[y*256+x].tile=u16(data+pos);pos+=2;}
    for(y=0;y<height;y++)for(x=0;x<width;x++){uint8_t flags=data[pos++];terrain.cells[y*256+x].flags=(uint8_t)(flags|((flags&0x51)?0x20:0));}
    for(y=0;y<height;y++) {
        for(x=0;x<width;x++) {
            int16_t h=(int16_t)(int)((long double)signed16(u16(data+pos))*0.5120000243186951f);pos+=2;
            terrain.cells[y*256+x].height=h;if(h<terrain.minimum)terrain.minimum=h;if(h>terrain.maximum)terrain.maximum=h;
        }
        pos+=2; /* Each row has an unused right-edge sample. */
    }
    for(y=0;y<height/4;y++)for(x=0;x<width/4;x++) {
        int16_t maximum=-32768;unsigned dx,dy;
        for(dy=0;dy<5;dy++)for(dx=0;dx<5;dx++){int16_t h=terrain.cells[((y*4+dy)&255)*256+((x*4+dx)&255)].height;if(h>maximum)maximum=h;}
        terrain.coarse[y*64+x]=maximum;
    }
    terrain_asset_light_direction(direction);
    if(lighting)for(y=0;y<height;y++)for(x=0;x<width;x++) {
        TerrainCell *cell=&terrain.cells[y*256+x];
        cell->light=terrain_asset_light(cell->height,terrain.cells[y*256+((x+1)&255)].height,terrain.cells[((y+1)&255)*256+x].height,
            terrain.cells[y*256+((x-1)&255)].height,terrain.cells[((y-1)&255)*256+x].height,direction,seed);
    }
    *out=terrain;return 1;
}

int terrain_height_step_allowed(const TerrainCell *cells,int x1,int y1,int x2,int y2,float diagonal,float axial) {
    int delta;
    if((unsigned)x1>=256 || (unsigned)y1>=256 || (unsigned)x2>=256 || (unsigned)y2>=256)return 1;
    delta=(int)cells[y1*256+x1].height-(int)cells[y2*256+x2].height;
    if(delta<0)delta=-delta;
    return (float)delta<=(x1==x2 || y1==y2?axial:diagonal);
}
void terrain_build_adjacency(const TerrainCell *cells,float diagonal,float axial,uint8_t output[8192]) {
    static const int pairs[6][4]={{0,0,1,0},{0,0,0,1},{0,0,1,1},{0,1,1,0},{0,1,1,1},{1,1,1,0}};
    unsigned x,y,i;memset(output,0,8192);
    for(y=0;y<256;y++)for(x=0;x<256;x++) {
        int blocked=(cells[y*256+x].flags&0x56)!=0;
        for(i=0;!blocked && i<6;i++)blocked=!terrain_height_step_allowed(cells,
            (int)x+pairs[i][0],(int)y+pairs[i][1],(int)x+pairs[i][2],(int)y+pairs[i][3],diagonal,axial);
        if(blocked || !x || !y || x==255 || y==255)output[y*32+x/8]|=(uint8_t)(1u<<(x&7));
    }
}
