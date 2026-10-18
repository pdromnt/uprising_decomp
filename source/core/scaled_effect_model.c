#include "scaled_effect_model.h"
#include "portable_crt.h"
#include "game_matrix.h"
#include "gameplay_geometry.h"
#include <math.h>
#include <string.h>

static const unsigned fields[5]={0x40,0x44,0x4c,0x50,0x54};
static const size_t sizes[5]={0x510,0x6c0,0x1b0,0x360,0x360};
static float real(GameMemory *m,GameAddress p){uint32_t bits=game_memory_read_u32(m,p);float value;memcpy(&value,&bits,4);return value;}
static void store(GameMemory *m,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);game_memory_write_u32(m,p,bits);}

int scaled_effect_model_release(GameMemory *m,GameAddress model,int deleting,
        const GamePresentationStorageServices *s){
    unsigned i;
    if(!m || !model || !s || !s->release)return 0;
    for(i=0;i<5;i++){
        static const unsigned order[5]={0,1,2,4,3};
        unsigned field=fields[order[i]];
        GameAddress p=game_memory_read_u32(m,model+field);
        if(m->fault || (p && !s->release(s->context,p)))return 0;
        game_memory_write_u32(m,model+field,0);
    }
    game_memory_write_u32(m,model,0x571fa0);
    return !m->fault && (!deleting || s->release(s->context,model));
}

/* Each triangle consumes three CRT draws. The first selects a direction;
 * the other two offset that same direction, rather than each other.
 * Preserve extended intermediates and the original binary32 stores. */
static void vertex(GameMemory *m,GameAddress position,GameAddress uv,
        unsigned random,unsigned direction,int copied){
    unsigned radial=(random>>3)&7,axial=(random>>6)&3;
    long double r=(long double)radial*real(m,0x5750e0);
    long double h=(long double)axial*real(m,0x5750e4);
    long double scale=(long double)real(m,copied?0x5750ec:0x5750d8)*r+real(m,0x5750e8);
    GameAddress angle=0x608268+(direction&7)*12,texture=0x6082c8+(radial*8+axial)*8;
    store(m,position,(float)(((long double)real(m,angle)*h)*scale));
    store(m,position+4,(float)(((long double)real(m,angle+4)*h)*scale));
    store(m,position+8,(float)(r-real(m,0x5750d8)));
    game_memory_write_u32(m,uv,game_memory_read_u32(m,texture));
    game_memory_write_u32(m,uv+4,game_memory_read_u32(m,texture+4));
}

int scaled_effect_model_construct(GameMemory *m,GameAddress model,uint32_t material,
        int copied,const GamePresentationStorageServices *s){
    GameAddress arrays[5]={0};unsigned i,triangle;
    if(!m || m->fault || !model || !s || !s->allocate || !s->release)return 0;
    game_memory_write_u32(m,model,0x575128);
    for(i=0;i<12;i++)store(m,model+4+i*4,(i==0 || i==5 || i==10)?1.0f:0.0f);
    game_memory_write_u32(m,model+0x34,0);game_memory_write_u32(m,model+0x38,0);
    game_memory_write_u8(m,model+0x3c,0);
    for(i=0;i<5;i++)game_memory_write_u32(m,model+fields[i],0);
    for(i=0;i<5;i++){
        arrays[i]=s->allocate(s->context,sizes[i]);
        game_memory_write_u32(m,model+fields[i],arrays[i]);
        if(!arrays[i] || m->fault)goto failed;
    }
    for(triangle=0;triangle<36;triangle++){
        GameAddress packet=model+0x58+triangle*40,pointers=arrays[2]+triangle*12;
        game_memory_write_u32(m,packet,3);
        game_memory_write_u32(m,packet+4,pointers);
        game_memory_write_u32(m,packet+8,copied?0x119:0x11d);
        game_memory_write_u32(m,packet+16,arrays[4]+triangle*24);
        game_memory_write_u32(m,packet+20,material);
        for(i=0;i<3;i++)game_memory_write_u32(m,pointers+i*4,arrays[1]+(triangle*3+i)*16);
    }
    for(triangle=0;triangle<36 && !m->fault;triangle++){
        unsigned random=portable_crt_rand_step(m)&255,direction=random&7;
        vertex(m,arrays[0]+triangle*36,arrays[3]+triangle*24,random,direction,copied);
        random=portable_crt_rand_step(m)&255;
        vertex(m,arrays[0]+triangle*36+12,arrays[3]+triangle*24+8,random,direction+(random&3)-2,copied);
        random=portable_crt_rand_step(m)&255;
        vertex(m,arrays[0]+triangle*36+24,arrays[3]+triangle*24+16,random,direction+(random&3)-1,copied);
    }
    if(!m->fault)return 1;
failed:
    /* Keep allocation addresses in locals so a memory fault cannot prevent
     * practical construction cleanup through another read of the record. */
    for(i=0;i<5;i++)if(arrays[i]){
        (void)s->release(s->context,arrays[i]);
        game_memory_write_u32(m,model+fields[i],0);
    }
    return 0;
}

GameAddress scaled_effect_model_clone(GameMemory *m,GameAddress source,
        const GamePresentationStorageServices *s){
    GameAddress result;uint32_t material;
    if(!m || m->fault || !s || !s->allocate || !s->release ||
       game_memory_read_u32(m,source)!=0x575128)return 0;
    material=game_memory_read_u32(m,source+0x6c);
    if(m->fault)return 0;
    result=s->allocate(s->context,0x5f8);if(!result)return 0;
    /* Retail clones generate fresh geometry with their distinct radial scale. */
    if(scaled_effect_model_construct(m,result,material,1,s))return result;
    (void)s->release(s->context,result);return 0;
}
/*4fb790: texture scrolling, the beam frame and spin, followed by the existing
 *depth/screen clippers and frame queue. Scratch matrices are ordinary C values;
 *the model's five arrays remain their original exclusive storage. */
int scaled_effect_model_present(GameMemory *m,GameAddress model,const float from[3],const float to[3],float phase){
    float frame[12],rotation[12]={0},product[12],second[12],camera[12],view[12];
    float direction[3],side[3],side_length,radius,scroll,squared,bounds[4];
    long double total,radial,norm;GameAddress vertices,rendered,uv,render_uv,pointers,begin,end,head,tail=0;
    int32_t angle,far;unsigned i,row;GameplayClipStatus status={0};
    if(!m || m->fault || !from || !to || game_memory_read_u32(m,model)!=0x575128)return 0;
    vertices=game_memory_read_u32(m,model+0x40);rendered=game_memory_read_u32(m,model+0x44);
    pointers=game_memory_read_u32(m,model+0x4c);uv=game_memory_read_u32(m,model+0x50);render_uv=game_memory_read_u32(m,model+0x54);
    for(i=0;i<36;i++){
        game_memory_write_u32(m,model+0x58+i*40,3);
        game_memory_write_u32(m,model+0x5c+i*40,pointers+i*12);
        game_memory_write_u32(m,model+0x68+i*40,render_uv+i*24);
    }
    scroll=(float)((long double)phase*real(m,0x5750f0));
    for(i=0;i<108;i++){
        game_memory_write_u32(m,render_uv+i*8,game_memory_read_u32(m,uv+i*8));
        store(m,render_uv+i*8+4,(float)((long double)real(m,uv+i*8+4)+scroll));
    }
    norm=(long double)phase*real(m,0x5750f4);
    angle=(!isfinite(norm) || norm>=2147483648.0L || norm< -2147483648.0L)?INT32_MIN:(int32_t)norm;
    angle&=2047;
    rotation[0]=rotation[5]=real(m,0x6298a8+((angle+512)&2047)*4);
    rotation[1]=-real(m,0x6298a8+angle*4);rotation[4]=-rotation[1];rotation[10]=1;
    radius=(float)((long double)phase*real(m,0x5750f8)+real(m,0x5750fc));
    for(i=0;i<3;i++)direction[i]=(float)((long double)to[i]-from[i]);
    squared=(float)(((long double)direction[2]*direction[2]+(long double)direction[1]*direction[1])+(long double)direction[0]*direction[0]);
    total=gameplay_geometry_square_root(squared);
    norm=(long double)real(m,0x5750d0)/total;
    for(i=0;i<3;i++)direction[i]=(float)(direction[i]*norm);
    side[0]=direction[2];side[1]=0;side[2]=-direction[0];
    squared=(float)((long double)side[2]*side[2]+(long double)side[0]*side[0]);
    norm=gameplay_geometry_square_root(squared);side_length=(float)norm;
    if(!isnan(norm) && norm!=real(m,0x5750d0) && side_length!=0){
        norm=(long double)real(m,0x5750d0)/side_length;
        side[0]=(float)(norm*side[0]);side[2]=(float)(norm*side[2]);
    }
    radial=total*real(m,0x575100);
    for(row=0;row<3;row++){
        unsigned a=(row+1)%3,b=(row+2)%3;
        long double cross=(long double)direction[a]*side[b]-(long double)direction[b]*side[a];
        if(row)cross=(float)cross;
        frame[row*4]=(float)((long double)side[row]*radius);
        frame[row*4+1]=(float)((long double)cross*radius);
        frame[row*4+2]=(float)((long double)direction[row]*radial);
        frame[row*4+3]=(float)(((long double)from[row]+to[row])*real(m,0x5750d8));
    }
    /*Retail applies the local spin in its inline product and again through
     *4033e0. Preserve both products, including their binary32 matrices. */
    game_matrix_compose_values(product,frame,rotation);
    game_matrix_compose_values(second,product,rotation);
    for(i=0;i<12;i++)camera[i]=real(m,0x5cedf0+i*4);
    game_matrix_compose_values(view,camera,second);
    for(i=0;i<108;i++){
        float input[3],output[3];unsigned axis;
        for(axis=0;axis<3;axis++)input[axis]=real(m,vertices+i*12+axis*4);
        game_matrix_transform_point_values(output,view,input);
        for(axis=0;axis<3;axis++)store(m,rendered+i*16+axis*4,output[axis]);
    }
    begin=game_memory_read_u32(m,0x604378);
    {uint32_t bits=game_memory_read_u32(m,0x597f7c);memcpy(&far,&bits,4);}
    head=gameplay_clipper_clip_polygons(m,model+0x58,36,10,
        (float)((long double)far+real(m,0x575104)),0x604378,0x604268,0x604480,0x604270);
    {GameAddress p=head;unsigned visits=0;tail=0;
        while(p && !m->fault){if(++visits>65536)return 0;tail=p;p=game_memory_read_u32(m,p+12);}
        game_memory_write_u32(m,0x604260,head);game_memory_write_u32(m,0x604264,tail);}
    end=game_memory_read_u32(m,0x604378);
    if(m->fault || !gameplay_geometry_project_vertices(m,rendered,rendered+0x6c0,real(m,0x6290c8),real(m,0x6290cc),real(m,0x6290f4),real(m,0x6290f0),real(m,0x574cb0)) ||
       !gameplay_geometry_project_vertices(m,begin,end,real(m,0x6290c8),real(m,0x6290cc),real(m,0x6290f4),real(m,0x6290f0),real(m,0x574cb0)))return 0;
    gameplay_geometry_premultiply_uv(m,head);
    head=gameplay_geometry_filter_backfaces(m,head,real(m,0x574cb4));
    bounds[0]=(float)((long double)real(m,0x6290d4)-real(m,0x5750d8));
    bounds[1]=(float)((long double)real(m,0x6290d0)-real(m,0x5750d8));
    bounds[2]=(float)((long double)real(m,0x6290dc)-real(m,0x5750d0));
    bounds[3]=(float)((long double)real(m,0x6290d8)-real(m,0x5750d0));
    head=gameplay_clipper_screen_chain(m,head,&tail,bounds,0x604378,0x604268,0x604480,0x604270,&status);
    if(head){game_memory_write_u32(m,tail+12,game_memory_read_u32(m,0x604484));game_memory_write_u32(m,0x604484,head);}
    return !m->fault;
}
