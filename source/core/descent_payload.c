#include "descent_payload.h"
#include "game_initializers.h"
#include "game_intrusive_list.h"
#include "gameplay_geometry.h"
#include "gameplay_leaves.h"
#include <math.h>
#include <string.h>

static float real_at(GameMemory *memory,GameAddress address){
    uint32_t bits=game_memory_read_u32(memory,address);float value;memcpy(&value,&bits,4u);return value;
}
static void put_real(GameMemory *memory,GameAddress address,float value){
    uint32_t bits;memcpy(&bits,&value,4u);game_memory_write_u32(memory,address,bits);
}
int descent_model_resize(GameMemory *memory,GameAddress model,float width,float height){
    static const GameAddress horizontal[7]={0x5739b0u,0x5739b8u,0x5739bcu,0u,0x5739c0u,0x5739c4u,0x5739c8u};
    GameAddress vertices=game_memory_read_u32(memory,model+0x40u);unsigned row,column;
    put_real(memory,model+0x4cu,width);put_real(memory,model+0x50u,height);
    for(row=0u;row<2u;row++)for(column=0u;column<7u;column++){
        GameAddress point=vertices+(row*7u+column)*12u;
        put_real(memory,point,horizontal[column]?(float)((long double)width*real_at(memory,horizontal[column])):0.0f);
        put_real(memory,point+4u,(float)((long double)height*real_at(memory,row?0x5739c8u:0x5739b0u)));
        put_real(memory,point+8u,0.0f);
    }
    return !memory->fault;
}
int descent_model_release(GameMemory *memory,GameAddress model,int deleting,const GamePresentationStorageServices *services){
    unsigned index;if(!services || !services->release)return 0;
    game_memory_write_u32(memory,model,0x573a10u);
    for(index=0u;index<3u;index++){
        GameAddress field=model+0x40u+index*4u,allocation=game_memory_read_u32(memory,field);
        if(memory->fault || (allocation && !services->release(services->context,allocation)))return 0;
        game_memory_write_u32(memory,field,0u);
    }
    game_memory_write_u32(memory,model,0x571fa0u);
    return !memory->fault && (!deleting || services->release(services->context,model));
}
int descent_model_construct(GameMemory *memory,GameAddress model,const GamePresentationStorageServices *services){
    static const size_t sizes[3]={0xa8u,0xe0u,0xf0u};unsigned index;
    if(!services || !services->allocate || !services->release)return 0;
    game_memory_write_u32(memory,model,0x573a10u);
    for(index=0u;index<12u;index++)put_real(memory,model+4u+index*4u,index==0u || index==5u || index==10u?1.0f:0.0f);
    game_memory_write_u32(memory,model+0x34u,0u);game_memory_write_u32(memory,model+0x38u,0u);game_memory_write_u8(memory,model+0x3cu,0u);
    for(index=0u;index<3u;index++)game_memory_write_u32(memory,model+0x40u+index*4u,0u);
    for(index=0u;index<3u;index++){
        GameAddress allocation=services->allocate(services->context,sizes[index]);
        game_memory_write_u32(memory,model+0x40u+index*4u,allocation);
        if(!allocation || memory->fault){(void)descent_model_release(memory,model,0,services);return 0;}
    }
    if(!descent_model_resize(memory,model,1000.0f,10000.0f)){(void)descent_model_release(memory,model,0,services);return 0;}
    game_initializer_record_catalog_records(memory,game_memory_read_u32(memory,model+0x48u));
    return !memory->fault;
}
GameAddress descent_model_clone(GameMemory *memory,GameAddress source,const GamePresentationStorageServices *services){
    static const unsigned sizes[3]={0xa8u,0xe0u,0xf0u};GameAddress model;unsigned array,offset;
    if(!services || !services->allocate)return 0u;
    model=services->allocate(services->context,0x54u);if(!model)return 0u;
    if(!descent_model_construct(memory,model,services)){(void)services->release(services->context,model);return 0u;}
    /* Clone keeps the constructor's width/height and matrix; only arrays copy. */
    for(array=0u;array<3u;array++){
        GameAddress input=game_memory_read_u32(memory,source+0x40u+array*4u),output=game_memory_read_u32(memory,model+0x40u+array*4u);
        for(offset=0u;offset<sizes[array];offset+=4u)game_memory_write_u32(memory,output+offset,game_memory_read_u32(memory,input+offset));
    }
    if(memory->fault){(void)descent_model_release(memory,model,1,services);return 0u;}
    return model;
}
static int finish_payload(GameMemory *memory,GameAddress object){
    game_memory_write_u32(memory,object,0x573a30u);game_memory_write_u8(memory,object+12u,0x0bu);
    game_memory_write_u32(memory,object+16u,game_memory_read_u32(memory,object+16u)|0xc00u);
    game_memory_write_u8(memory,object+0xb4u,1u);return !memory->fault;
}
int descent_payload_construct(GameMemory *memory,GameAddress object,const PresentationInstanceDefinition *definition){
    PresentationInstanceDefinition base;if(!definition)return 0;
    base=*definition;base.type=0x0bu;base.flags|=0xc00u;
    return presentation_instance_create(memory,object,&base,0) && finish_payload(memory,object);
}
int descent_payload_copy(GameMemory *memory,GameAddress object,GameAddress source,const PresentationConstructionServices *services){
    return presentation_instance_create_copy(memory,object,source,0,services) && finish_payload(memory,object);
}
typedef struct DescentClone {GameMemory *memory;const GamePresentationStorageServices *storage;GameAddress last;} DescentClone;
static int factory_clone(void *context,GameAddress source,GameAddress *output){
    DescentClone *clone=context;*output=descent_model_clone(clone->memory,source,clone->storage);clone->last=*output;return *output!=0u;
}
GameAddress descent_payload_template(GameMemory *memory,uint16_t slot,const GamePresentationStorageServices *services){
    GameAddress object,model;PresentationInstanceDefinition definition={0};
    if(slot || !services || !services->allocate || !services->release)return 0u;
    object=game_memory_read_u32(memory,0x5adc64u);if(object || memory->fault)return object;
    if(game_memory_read_u32(memory,0x624f8cu)==10u)return 0u;
    model=services->allocate(services->context,0x54u);if(!model)return 0u;
    if(!descent_model_construct(memory,model,services)){(void)services->release(services->context,model);return 0u;}
    object=services->allocate(services->context,0xb8u);
    definition.type=0x0bu;definition.flags=0xc00u;definition.dimensions[0]=definition.dimensions[2]=500.0f;
    definition.dimensions[1]=5000.0f;definition.radius=0x447a0000u;definition.model=model;
    if(!object || !descent_payload_construct(memory,object,&definition)){
        (void)descent_model_release(memory,model,1,services);if(object)(void)services->release(services->context,object);return 0u;
    }
    game_memory_write_u32(memory,0x5adc64u,object);return memory->fault?0u:object;
}
int descent_payload_fill(GameMemory *memory,uint16_t slot,int32_t count,const GamePresentationStorageServices *services){
    GameAddress source=descent_payload_template(memory,slot,services);int32_t index;
    DescentClone clone={memory,services,0u};PresentationConstructionServices construction={&clone,factory_clone};
    if(!source)return 0;
    for(index=0;index<count;index++){
        GameAddress object=services->allocate(services->context,0xb8u);
        if(!object)return 0;
        clone.last=0u;
        if(!descent_payload_copy(memory,object,source,&construction)){
            if(clone.last)(void)descent_model_release(memory,clone.last,1,services);
            (void)services->release(services->context,object);return 0;
        }
        (void)game_list_insert_after(memory,object,source,4u,8u);if(memory->fault)return 0;
    }
    return !memory->fault;
}
GameAddress descent_payload_take(GameMemory *memory,uint16_t slot,const GamePresentationStorageServices *services){
    GameAddress source=descent_payload_template(memory,slot,services),object;if(!source)return 0u;
    object=game_memory_read_u32(memory,source+4u);
    if(!object){if(!descent_payload_fill(memory,slot,1,services))return 0u;object=game_memory_read_u32(memory,source+4u);}
    if(!object || memory->fault)return 0u;
    (void)game_list_detach_node(memory,object,8u,4u);return memory->fault?0u:object;
}
int descent_payload_reset(GameMemory *memory,GameAddress object,const DescentUpdateServices *services){
    GameAddress model=game_memory_read_u32(memory,object+0x54u);
    return services && services->contacts.reset && descent_model_resize(memory,model,1000.0f,10000.0f) && services->contacts.reset(services->contacts.context,object);
}
int descent_payload_step(GameMemory *memory,GameAddress object,const DescentUpdateServices *services){
    GameAddress model=game_memory_read_u32(memory,object+0x54u),node,head;float height=real_at(memory,object+0x1cu);unsigned visits=0u,axis;
    long double floor=gameplay_geometry_terrain_height(memory,real_at(memory,object+0x18u),real_at(memory,object+0x20u))+real_at(memory,0x5739f8u),next;
    const PresentationArticulatedServices *contacts;
    if(!services || !services->move)return 0;
    contacts=&services->contacts;
    if(floor>=height){
        next=(long double)real_at(memory,model+0x4cu)*real_at(memory,0x5739d8u);
        next=next*real_at(memory,0x5739fcu)+real_at(memory,0x573a00u);
        next=next*real_at(memory,0x5739c8u)*real_at(memory,0x573a04u)+height;
    }else next=(long double)height-real_at(memory,0x573a08u);
    put_real(memory,object+0x1cu,(float)next);
    if(memory->fault || !services->move(contacts->context,object,object+0x18u))return 0;
    node=game_memory_read_u32(memory,object+0xb0u);
    if(!node)return !memory->fault;
    do{
        GameAddress receiver=game_memory_read_u32(memory,node+12u);float position[3];
        if(++visits>65536u)return 0;
        for(axis=0u;axis<3u;axis++)position[axis]=real_at(memory,node+16u+axis*4u);
        if(memory->fault || !contacts->command || !contacts->command(contacts->context,receiver,object,position))return 0;
        node=game_memory_read_u32(memory,node+4u);
    }while(node && !memory->fault);
    head=game_memory_read_u32(memory,object+0xb0u);
    if(memory->fault || !contacts->release_commands || !contacts->release_commands(contacts->context,head))return 0;
    game_memory_write_u32(memory,object+0xb0u,0u);return !memory->fault;
}
int descent_payload_advance(GameMemory *memory,GameAddress object,const DescentUpdateServices *services){
    GameAddress model=game_memory_read_u32(memory,object+0x54u),table,method;float height,dimension,parameter,width;uint32_t bits;int32_t signed_bits;
    long double difference=(long double)real_at(memory,object+0x1cu)-gameplay_geometry_terrain_height(memory,real_at(memory,object+0x18u),real_at(memory,object+0x20u));
    if(!services)return 0;
    height=(float)difference;memcpy(&bits,&height,4u);memcpy(&signed_bits,&bits,4u);
    /* The second threshold is the original signed comparison of stored bits. */
    if(difference>real_at(memory,0x5739f0u) && signed_bits<INT32_C(0x45a0f000))
        game_terrain_impact(memory,real_at(memory,object+0x18u),real_at(memory,object+0x20u),500);
    table=game_memory_read_u32(memory,object);
    if(bits>0x80000000u){
        method=game_memory_read_u32(memory,table+0x20u);
        return !memory->fault && (method==0x471a70u?descent_payload_reset(memory,object,services):services->phase && services->phase(services->contacts.context,object,method));
    }
    dimension=real_at(memory,object+0x30u);
    parameter=(float)((long double)real_at(memory,0x5739d4u)-fabsl((long double)height-dimension)/dimension);
    width=(float)(gameplay_geometry_square_root(parameter)*real_at(memory,0x5739acu));
    if(memory->fault || !descent_model_resize(memory,model,width,10000.0f))return 0;
    method=game_memory_read_u32(memory,game_memory_read_u32(memory,object)+0x88u);
    return !memory->fault && (method==0x4719a0u?descent_payload_step(memory,object,services):services->phase && services->phase(services->contacts.context,object,method));
}
static int32_t signed_word(uint32_t bits){int32_t value;memcpy(&value,&bits,4u);return value;}
int descent_dirty_rectangle(GameMemory *memory,const uint16_t bounds[4]){
    GameAddress output=game_memory_read_u32(memory,0x60426cu);int32_t edge[4],limits[2];unsigned index;
    if(game_memory_read_u32(memory,0x594454u)-output==0xfffffe70u){game_memory_write_u32(memory,0x604488u,0x7fffffffu);return !memory->fault;}
    for(index=0u;index<4u;index++)edge[index]=(int32_t)bounds[index]-((bounds[index]&0x8000u)?65536:0);
    limits[0]=signed_word(game_memory_read_u32(memory,0x62792cu));limits[1]=signed_word(game_memory_read_u32(memory,0x627928u));
    if(edge[2]<0 || edge[0]>=limits[0] || edge[3]<0 || edge[1]>=limits[1])return !memory->fault;
    for(index=0u;index<2u;index++){
        if(edge[index]<0)edge[index]=0;
        if(edge[index+2u]>=limits[index])edge[index+2u]=limits[index];
    }
    for(index=0u;index<4u;index++)game_memory_write_u16(memory,output+index*2u,(uint16_t)edge[index]);
    game_memory_write_u32(memory,0x60426cu,output+8u);return !memory->fault;
}
static uint16_t integer_low_word(float value){
    uint64_t bits=0u;
    if((long double)value>=-9223372036854775808.0L && (long double)value<9223372036854775808.0L)bits=(uint64_t)(int64_t)value;
    return (uint16_t)bits;
}
static void midpoint(float output[3],const float first[3],const float second[3],float half){
    unsigned axis;for(axis=0u;axis<2u;axis++)output[axis]=(float)(((long double)first[axis]+second[axis])*half);
}
static void interpolate(float output[3],const float first[3],const float second[3],float fraction){
    unsigned axis;for(axis=0u;axis<2u;axis++)output[axis]=(float)(((long double)second[axis]-first[axis])*fraction+first[axis]);
}
static void clip_column(float top[3],float bottom[3],float minimum,float maximum){
    long double slope=((long double)bottom[0]-top[0])/((long double)bottom[1]-top[1]);
    /* Preserve the original endpoint adjustments, including its extrapolation. */
    top[1]=minimum;top[0]=(float)(((long double)bottom[1]-top[1])*slope+top[0]);
    bottom[1]=maximum;bottom[0]=(float)(((long double)bottom[1]-top[1])*slope+bottom[0]);
}
int descent_enqueue_quad(GameMemory *memory,GameAddress record){
    GameAddress head=game_memory_read_u32(memory,0x59626cu),tail=game_memory_read_u32(memory,0x596270u);
    if(head)game_memory_write_u32(memory,tail+0x24u,record);
    else game_memory_write_u32(memory,0x59626cu,record);
    game_memory_write_u32(memory,0x596270u,record);game_memory_write_u32(memory,record+0x24u,0u);return !memory->fault;
}
int descent_model_present(GameMemory *memory,GameAddress model,const DescentRenderServices *services){
    float position[3],camera[3],vertices[14][3],cosine,sine,half,aspect,cos_width,sin_height,left;
    GameAddress player,output,polygons;uint32_t angle,tick;int32_t pitch,half_pitch;unsigned row,column,index,axis;uint16_t bounds[4];
    long double depth,reciprocal,projection,horizontal,vertical,half_width,half_height,sin_width,cos_height,left_extended,offset;
    player=game_memory_read_u32(memory,0x58f890u);angle=game_memory_read_u16(memory,player+10u);pitch=(int32_t)angle-((angle&0x8000u)?65536:0);
    half_pitch=pitch>=0?pitch/2:-1-((-1-pitch)/2);angle=(uint32_t)(-half_pitch)&2047u;
    cosine=real_at(memory,0x6298a8u+((angle+512u)&2047u)*4u);sine=real_at(memory,0x6298a8u+angle*4u);
    position[0]=real_at(memory,model+0x10u);position[2]=real_at(memory,model+0x30u);
    position[1]=(float)gameplay_geometry_terrain_height(memory,position[0],position[2]);
    if(memory->fault || !services || !services->surface || !services->surface(services->context,position,real_at(memory,model+0x4cu),15.0f))return 0;
    camera[0]=(float)(((long double)position[2]*real_at(memory,0x5cedf8u)+(long double)position[1]*real_at(memory,0x5cedf4u))+
        (long double)position[0]*real_at(memory,0x5cedf0u)+real_at(memory,0x5cedfcu));
    camera[1]=(float)(((long double)position[1]*real_at(memory,0x5cee04u)+(long double)position[2]*real_at(memory,0x5cee08u))+
        (long double)position[0]*real_at(memory,0x5cee00u)+real_at(memory,0x5cee0cu));
    depth=((long double)position[2]*real_at(memory,0x5cee18u)+(long double)position[1]*real_at(memory,0x5cee14u))+
        (long double)position[0]*real_at(memory,0x5cee10u)+real_at(memory,0x5cee1cu);
    camera[2]=(float)depth;if(!(depth>real_at(memory,0x5739d0u)))return !memory->fault;
    half=real_at(memory,0x5739c8u);aspect=real_at(memory,0x6290f0u);
    reciprocal=(long double)real_at(memory,0x5739d4u)/camera[2];projection=(long double)real_at(memory,0x6290f4u)*reciprocal;
    horizontal=(long double)camera[0]*projection;vertical=((long double)camera[1]*projection)*aspect;
    half_width=(long double)real_at(memory,model+0x4cu)*(projection*half);
    half_height=((projection*half)*real_at(memory,model+0x50u))*aspect;
    cos_width=(float)((long double)cosine*half_width);sin_width=half_width*sine;
    sin_height=(float)((long double)sine*half_height);cos_height=half_height*cosine;
    left_extended=horizontal-cos_width;left=(float)left_extended;
    vertices[6][0]=(float)((long double)real_at(memory,0x6290c8u)-(left_extended-sin_height));
    vertices[6][1]=(float)((long double)real_at(memory,0x6290ccu)-((vertical-sin_width)+cos_height));
    vertices[0][0]=(float)((long double)real_at(memory,0x6290c8u)-((horizontal-sin_height)+cos_width));
    vertices[0][1]=(float)((long double)real_at(memory,0x6290ccu)-((vertical+cos_height)+sin_width));
    vertices[7][0]=(float)((long double)real_at(memory,0x6290c8u)-((horizontal+cos_width)+sin_height));
    vertices[7][1]=(float)((long double)real_at(memory,0x6290ccu)-((vertical-cos_height)+sin_width));
    vertices[13][0]=(float)((long double)real_at(memory,0x6290c8u)-((long double)sin_height+left));
    vertices[13][1]=(float)((long double)real_at(memory,0x6290ccu)-((vertical-cos_height)-sin_width));
    vertices[0][2]=vertices[6][2]=vertices[7][2]=vertices[13][2]=(float)reciprocal;
    clip_column(vertices[0],vertices[7],real_at(memory,0x6290d0u),real_at(memory,0x6290d8u));
    clip_column(vertices[6],vertices[13],real_at(memory,0x6290d0u),real_at(memory,0x6290d8u));
    midpoint(vertices[3],vertices[6],vertices[0],half);midpoint(vertices[10],vertices[13],vertices[7],half);
    offset=((long double)real_at(memory,model+0x4cu)*real_at(memory,0x5739d8u))*real_at(memory,0x5739dcu);
    vertices[3][2]=vertices[10][2]=(float)((long double)real_at(memory,0x5739d4u)/((long double)camera[2]-offset));
    for(row=0u;row<2u;row++){
        index=row*7u;interpolate(vertices[index+1u],vertices[index],vertices[index+3u],real_at(memory,0x5739e0u));
        interpolate(vertices[index+5u],vertices[index+3u],vertices[index+6u],real_at(memory,0x5739e4u));
        midpoint(vertices[index+2u],vertices[index+3u],vertices[index+1u],half);
        midpoint(vertices[index+4u],vertices[index+3u],vertices[index+5u],half);
    }
    for(row=0u;row<2u;row++)for(column=1u;column<=5u;column+=4u)
        vertices[row*7u+column][2]=(float)((long double)real_at(memory,0x5739d4u)/((long double)camera[2]-(long double)real_at(memory,0x629b50u)*offset));
    for(row=0u;row<2u;row++)for(column=2u;column<=4u;column+=2u)
        vertices[row*7u+column][2]=(float)((long double)real_at(memory,0x5739d4u)/((long double)camera[2]-offset*real_at(memory,0x629dfcu)));
    output=game_memory_read_u32(memory,model+0x44u);polygons=game_memory_read_u32(memory,model+0x48u);
    for(index=0u;index<14u;index++)for(axis=0u;axis<3u;axis++)put_real(memory,output+index*16u+axis*4u,vertices[index][axis]);
    tick=game_memory_read_u32(memory,0x5ac6acu);
    for(index=0u;index<6u;index++){
        uint8_t colors[4],bright=(uint8_t)((tick>>3u)&1u);GameAddress record=polygons+index*40u;bright=(uint8_t)(bright-0x50u);
        if(index==0u || index==5u){colors[0]=colors[1]=0x11u;colors[2]=colors[3]=0x1fu;}
        else if(index==1u || index==4u){colors[0]=colors[1]=bright;colors[2]=colors[3]=(uint8_t)(((uint32_t)bright+0xbfu)/2u);}
        else{colors[0]=colors[1]=(uint8_t)(((uint32_t)bright+0xbfu)/2u);colors[2]=colors[3]=0xbfu;}
        game_memory_write_u32(memory,record+0x14u,output);
        for(axis=0u;axis<4u;axis++)game_memory_write_u8(memory,record+0x18u+axis,colors[axis]);
    }
    for(index=0u;index<6u;index++)if(!descent_enqueue_quad(memory,polygons+index*40u))return 0;
    bounds[0]=integer_low_word(vertices[6][1]>vertices[0][1]?vertices[0][1]:vertices[6][1]);
    bounds[1]=integer_low_word(vertices[0][0]>vertices[7][0]?vertices[7][0]:vertices[0][0]);
    bounds[2]=(uint16_t)(integer_low_word(vertices[7][1]<vertices[13][1]?vertices[13][1]:vertices[7][1])+1u);
    bounds[3]=(uint16_t)(integer_low_word(vertices[6][0]<vertices[13][0]?vertices[13][0]:vertices[6][0])+1u);
    return !memory->fault && descent_dirty_rectangle(memory,bounds);
}
/* Shared projected surface records are appended to the existing 410h owner.
   The coefficients intentionally retain the original rounding schedule. */
static int32_t surface_integer(long double value){
    int64_t integer;uint32_t low;
    if(!(value>=-9223372036854775808.0L && value<9223372036854775808.0L))return 0;
    integer=(int64_t)value;low=(uint32_t)(uint64_t)integer;return signed_word(low);
}
int descent_surface_submit(GameMemory *memory,const float point[3],float width,float height,int *accepted){
    GameAddress output=game_memory_read_u32(memory,0x58fdf4u),start=game_memory_read_u32(memory,0x58fdf0u);
    long double depth,reciprocal,x,y,half_y,row_step,column_step,a,b,c;
    float stored_depth,screen_x,screen_y,half_x,stored_half_y,row_normal,column_normal,negative_column;
    int32_t top,bottom,left,right,clip_top,clip_bottom,clip_left,clip_right,row_delta,column_delta;
    float coefficients[6];uint16_t bounds[4];unsigned index;
    *accepted=0;
    if(!game_memory_read_u32(memory,0x58fdecu) || start+0x410u<=output)return !memory->fault;
    game_memory_write_u32(memory,0x603280u,game_memory_read_u32(memory,0x603280u)+1u);*accepted=1;
    depth=((long double)point[0]*real_at(memory,0x5cee10u)+(long double)point[1]*real_at(memory,0x5cee14u))+
        (long double)point[2]*real_at(memory,0x5cee18u)+real_at(memory,0x5cee1cu);
    stored_depth=(float)depth;if(!(depth>real_at(memory,0x5744d0u)))return !memory->fault;
    reciprocal=(long double)real_at(memory,0x5744d4u)/stored_depth;
    x=((long double)point[0]*real_at(memory,0x5cedf0u)+(long double)point[1]*real_at(memory,0x5cedf4u))+
        (long double)point[2]*real_at(memory,0x5cedf8u)+real_at(memory,0x5cedfcu);
    y=((long double)point[0]*real_at(memory,0x5cee00u)+(long double)point[1]*real_at(memory,0x5cee04u))+
        (long double)point[2]*real_at(memory,0x5cee08u)+real_at(memory,0x5cee0cu);
    screen_x=(float)((long double)real_at(memory,0x6290c8u)-(x*reciprocal)*real_at(memory,0x6290f4u));
    screen_y=(float)((long double)real_at(memory,0x6290ccu)-((y*reciprocal)*real_at(memory,0x6290f0u))*real_at(memory,0x6290f4u));
    half_x=(float)((long double)real_at(memory,0x6290f4u)*(reciprocal*width));
    half_y=(reciprocal*width)*((long double)real_at(memory,0x6290f0u)*real_at(memory,0x6290f4u));stored_half_y=(float)half_y;
    top=surface_integer((long double)screen_y-half_y);
    bottom=signed_word((uint32_t)surface_integer((long double)stored_half_y+screen_y)+1u);
    left=surface_integer((long double)screen_x-half_x);
    right=signed_word((uint32_t)surface_integer((long double)half_x+screen_x)+1u);
    clip_top=surface_integer(real_at(memory,0x6290d0u));clip_bottom=surface_integer(real_at(memory,0x6290d8u));
    clip_right=surface_integer(real_at(memory,0x6290dcu));clip_left=surface_integer(real_at(memory,0x6290d4u));
    if(clip_top>bottom || clip_bottom<=top || clip_left>right || clip_right<=left)return !memory->fault;
    if(clip_top<top)clip_top=top;
    if(clip_bottom>bottom)clip_bottom=bottom;
    if(clip_left<left)clip_left=left;
    if(clip_right>right)clip_right=right;
    if(clip_bottom<=clip_top || clip_right<=clip_left)return !memory->fault;
    row_delta=signed_word((uint32_t)clip_top-(uint32_t)top);column_delta=signed_word((uint32_t)clip_left-(uint32_t)left);
    row_step=((long double)real_at(memory,0x5744d4u)/signed_word((uint32_t)bottom-(uint32_t)top))*real_at(memory,0x5744d8u);
    column_step=((long double)real_at(memory,0x5744d4u)/signed_word((uint32_t)right-(uint32_t)left))*real_at(memory,0x5744d8u);
    a=(width*(row_step*row_step))*real_at(memory,0x5744d8u);
    b=width*column_step;c=(width*(column_step*column_step))*real_at(memory,0x5744d8u);
    row_normal=(float)((long double)row_delta*row_step-real_at(memory,0x5744d4u));negative_column=(float)-c;
    column_normal=(float)((long double)column_delta*column_step-real_at(memory,0x5744d4u));
    coefficients[0]=(float)((long double)width-((long double)width*row_normal)*row_normal);
    coefficients[1]=(float)((long double)width-((long double)width*column_normal)*column_normal);
    coefficients[2]=(float)(((row_step*width)*real_at(memory,0x5744d8u)+(long double)(float)row_delta*(-a))+row_step*row_step);
    coefficients[3]=(float)((column_step*column_step+(long double)negative_column*(float)column_delta)+real_at(memory,0x5744d8u)*b);
    coefficients[4]=(float)-a;coefficients[5]=negative_column;
    game_memory_write_u32(memory,output,(uint32_t)clip_top);game_memory_write_u32(memory,output+4u,(uint32_t)clip_left);
    game_memory_write_u32(memory,output+8u,(uint32_t)clip_bottom);game_memory_write_u32(memory,output+12u,(uint32_t)clip_right);
    for(index=0u;index<6u;index++)put_real(memory,output+16u+index*4u,coefficients[index]);
    put_real(memory,output+40u,stored_depth);put_real(memory,output+44u,width);put_real(memory,output+48u,height);
    game_memory_write_u32(memory,0x58fdf4u,output+52u);
    bounds[0]=(uint16_t)clip_top;bounds[1]=(uint16_t)clip_left;bounds[2]=(uint16_t)clip_bottom;bounds[3]=(uint16_t)clip_right;
    return !memory->fault && descent_dirty_rectangle(memory,bounds);
}
