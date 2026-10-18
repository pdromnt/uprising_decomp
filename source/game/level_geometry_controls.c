#include "level_geometry.h"
#include "asset_bitmap.h"
#include "game_text_rasterizer.h"
#include "scene_projection_storage.h"
#include "display_transition.h"
#include "gameplay_state.h"
#include "radar_compositor.h"
#include "level_player_resources.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t value){game_memory_write_u32(&a->memory,p,value);}
/*415e40/416940/416b30/416d20 share the same verified bounded control record.
 *Container rows exclusively own controls; global selectors only borrow IDs. */
static int text_copy(GameApplication *a,GameAddress output,GameAddress source){
    uint8_t bytes[64]={0};unsigned i;
    if(source)for(i=0;i<63;++i){bytes[i]=game_memory_read_u8(&a->memory,source+i);if(!bytes[i])break;}
    return !a->memory.fault && game_memory_write(&a->memory,output,bytes,sizeof(bytes));
}
static int text_set(GameApplication *a,GameAddress control,GameAddress source,int secondary){
    GameAddress output=control+(secondary?0x5c:0x18);uint8_t bytes[64];
    if(!text_copy(a,output,source) || !game_memory_read(&a->memory,output,bytes,sizeof(bytes)))return 0;
    put(a,control+(secondary?0x9c:0x58),game_text_width_bytes(&a->memory,bytes,strlen((char *)bytes)));
    return !a->memory.fault;
}
/*165b0/16740: each button state owns its transformed image. Alternate states
 *replace palette index6e with66. Reverse padded source rows once; preserve the
 *verified metadata layout while keeping the pixel owner as a fixed-width ID. */
static int state_image(GameApplication *a,GameAddress control,unsigned slot,const AssetBitmap *source){
    GameAddress id;uint8_t *output;size_t bytes;unsigned row,column;
    GameAddress record=control+0xc0+slot*24;
    if(!source || !source->pixels || !source->width || !source->height ||
       source->height>SIZE_MAX/source->width)return 0;
    bytes=(size_t)source->width*source->height;
    id=scene_projection_storage_allocate(bytes,"overview control state");
    output=scene_projection_storage_borrow_bytes(id,bytes);
    if(!output){if(id)scene_projection_storage_free(id);return 0;}
    for(row=0;row<source->height;++row)for(column=0;column<source->width;++column){
        uint8_t value=source->pixels[(size_t)(source->height-1-row)*source->stride+column];
        output[(size_t)row*source->width+column]=slot>=2 && value==0x6e?0x66:value;
    }
    put(a,record,0);put(a,record+4,source->width);put(a,record+8,source->height);
    put(a,record+12,source->stride);put(a,record+16,0);put(a,record+20,id);
    return !a->memory.fault;
}
static int control_release(GameApplication *a,GameAddress control){
    unsigned i;uint32_t table;size_t bytes;int ok=1;if(!control)return 1;
    table=get(a,control);
    if(table==0x571ff8)bytes=0x130;
    else if(table==0x571fc8 || table==0x571fe0)bytes=0x128;
    else return 0;
    if(!scene_projection_storage_borrow_bytes(control,bytes))return 0;
    for(i=0;i<4;++i){GameAddress slot=control+0xc0+i*24+20,pixels=get(a,slot);
        if(pixels && !scene_projection_storage_free(pixels)){ok=0;continue;}
        put(a,slot,0);
    }
    return ok && !a->memory.fault && scene_projection_storage_free(control);
}
static GameAddress control_create(GameApplication *a,unsigned kind,uint32_t method,uint32_t data,
        GameAddress title,uint32_t identifier,const AssetBitmap *normal,const AssetBitmap *pressed){
    static const GameAddress tables[]={0x571fc8,0x571fe0,0x571ff8};
    GameAddress control;unsigned i;
    if(kind>=3)return 0;
    control=scene_projection_storage_allocate(kind==2?0x130:0x128,"overview control");if(!control)return 0;
    put(a,control,tables[kind]);put(a,control+4,kind==1?identifier:0);
    put(a,control+8,1);put(a,control+12,1);put(a,control+16,1);
    put(a,control+0xa0,UINT32_MAX);put(a,control+0xa4,UINT32_MAX);
    put(a,control+0xb8,method);put(a,control+0xbc,data);
    if(!text_set(a,control,title,0))goto fail;
    if(normal && pressed)for(i=0;i<4;++i)if(!state_image(a,control,i,i%2?pressed:normal))goto fail;
    if(a->memory.fault)goto fail;
    return control;
fail:
    (void)control_release(a,control);return 0;
}
/*417100/4171b0: one bounded row owner; callbacks borrow its child IDs. */
static GameAddress container_create(void){return scene_projection_storage_allocate(0x404,"overview control rows");}
static int container_append(GameApplication *a,GameAddress container,GameAddress control){
    uint32_t count=get(a,container);
    if(!control || !container || count>=256 || a->memory.fault)return 0;
    put(a,container+4+count*4,control);put(a,container,count+1);return !a->memory.fault;
}
static int container_release(GameApplication *a,GameAddress container){
    uint32_t i,count;if(!container)return 1;count=get(a,container);
    if(count>256 || !scene_projection_storage_borrow_bytes(container,0x404))return 0;
    for(i=0;i<count;++i){GameAddress slot=container+4+i*4;
        if(!control_release(a,get(a,slot)))return 0;
        put(a,slot,0);
    }
    return !a->memory.fault && scene_projection_storage_free(container);
}
static int control_caption(GameApplication *a,GameAddress control,const char *text){
    uint8_t bytes[64]={0};size_t length=strlen(text);
    if(length>=sizeof(bytes))return 0;
    memcpy(bytes,text,length);
    if(!game_memory_write(&a->memory,control+0x5c,bytes,sizeof(bytes)))return 0;
    put(a,control+0x9c,game_text_width_bytes(&a->memory,bytes,length));
    return !a->memory.fault;
}
extern int game_level_descriptor_unit_refund(GameApplication *,GameAddress,uint8_t,int32_t *);
/*434ae0/4836a0: deployment menu construction. The existing container owns
 *every button; selector publications only borrow those IDs. Layout is assigned
 *by the existing mouse-panel layout when opened. Retail's initial rectangle
 *comes from uninitialized stack words; native storage starts with zero bounds. */
static int deployment_menu(GameApplication *a,int alternate){
    static const struct {GameAddress title;uint16_t choice;uint8_t type;} primary[]={
        {0x584050,0x29c,0x40},{0x584058,0x2a0,0x21},{0x584060,0x2a4,0x27},
        {0x584064,0x2b0,0x26},{0x58406c,0x2d4,11},{0x584074,0,0x22},{0x58407c,0,0x24}
    };
    static const uint16_t choices[]={0,0,0x2ec,0x2f0,0x2f8,0x2f4,0x2fc,0x2dc,
        0x300,0x304,0x308,0x30c,0x310,0x314,0x318};
    static const GameAddress captions[]={0x589ac4,0x589acc,0x589ad4,0x589ae0,
        0x589ae8,0x589af0,0x589af8,0x589b00,0x589b08,0x589b14,0x589b1c,
        0x589b28,0x589b30,0x589b38,0x589b40};
    GameAddress owner,container,publication=alternate?0x5f023c:0x5aced4;
    GameAddress slots=alternate?0x5f0250:0x5ace80,count_at=alternate?0x5f022c:0x5acefc;
    unsigned i,count=0,limit=alternate?15:7;
    if(!a || a->memory.fault || get(a,publication))return 0;
    owner=get(a,0x5f0208);
    put(a,alternate?0x589abc:0x584048,0);put(a,alternate?0x589ac0:0x58404c,0);
    container=container_create();if(!container)return 0;
    put(a,publication,container);put(a,count_at,0);
    for(i=0;i<limit;i++)put(a,slots+i*4,0);
    for(i=0;i<limit;i++){
        unsigned choice=alternate?choices[i]:primary[i].choice;
        GameAddress title=alternate?0x596938+i*13:primary[i].title,control;
        if(choice && (get(a,owner+choice)&0x80000000u))continue;
        if(a->memory.fault)goto fail;
        /*These are release callbacks (bc), not press callbacks (b8). */
        control=control_create(a,0,0,alternate?0x4848b0:0x435b50,title,0,NULL,NULL);
        if(!control)goto fail;
        if(!container_append(a,container,control)){(void)control_release(a,control);goto fail;}
        put(a,control+0x124,i);put(a,slots+count*4,control);
        if(alternate){if(!text_set(a,control,captions[i],1))goto fail;}
        else{char caption[64];int32_t price;int length;
            put(a,0x5acee0+count*4,i);
            if(!game_level_descriptor_unit_refund(a,owner,primary[i].type,&price))goto fail;
            length=snprintf(caption,sizeof(caption),"$%4ld",(long)price);
            if(length<0 || (size_t)length>=sizeof(caption) || !control_caption(a,control,caption))goto fail;
        }
        put(a,count_at,++count);
    }
    put(a,alternate?0x5f0238:0x5aced0,count*10);
    put(a,alternate?0x5f0234:0x5aced8,count*14);
    return !a->memory.fault;
fail:{int fault=a->memory.fault,writing=a->memory.fault_write;GameAddress at=a->memory.fault_address;size_t size=a->memory.fault_size;
    a->memory.fault=0;
    if(container_release(a,container)){
        put(a,publication,0);put(a,count_at,0);
        for(i=0;i<limit;i++)put(a,slots+i*4,0);
    }
    if(fault){a->memory.fault=fault;a->memory.fault_address=at;a->memory.fault_size=size;a->memory.fault_write=writing;}
    return 0;}
}
int game_level_deployment_menu_begin(GameApplication *a){return deployment_menu(a,0);}
int game_level_deployment_actions_begin(GameApplication *a){return deployment_menu(a,1);}
static int pair_load(GameApplication *a,GameAddress normal_name,GameAddress pressed_name,
        ResourceBlob blobs[2],AssetBitmap images[2]){
    GameAddress names[2]={normal_name,pressed_name};unsigned i,j;char path[260];
    memset(blobs,0,2*sizeof(*blobs));
    for(i=0;i<2;++i){
        for(j=0;j<sizeof(path);++j){path[j]=(char)game_memory_read_u8(&a->memory,names[i]+j);if(!path[j])break;}
        if(j==sizeof(path) || a->memory.fault || !game_application_load_blob(a,path,&blobs[i]) ||
            !asset_bitmap8_parse(&blobs[i],&images[i]))return 0;
    }
    return 1;
}
static int image_control(GameApplication *a,unsigned kind,GameAddress container,GameAddress publication,
        uint32_t method,uint32_t tag,GameAddress normal_name,GameAddress pressed_name,GameAddress secondary){
    ResourceBlob blobs[2]={{0}};AssetBitmap images[2];GameAddress control=0;int ok=0;
    if(!pair_load(a,normal_name,pressed_name,blobs,images))goto done;
    control=control_create(a,kind,method,0,0,0,&images[0],&images[1]);if(!control)goto done;
    put(a,control+0x120,tag);
    if(secondary && !text_set(a,control,secondary,1))goto done;
    if(!container_append(a,container,control))goto done;
    put(a,publication,control);control=0;ok=!a->memory.fault;
done:
    if(control)(void)control_release(a,control);
    resource_blob_release(&blobs[0]);resource_blob_release(&blobs[1]);return ok;
}
static int selector_reveal(void *context,GameAddress position,float radius){
    return game_radar_reveal(&((GameApplication *)context)->memory,position,radius);
}
int game_level_geometry_controls_initialize(GameApplication *a){
    static const struct {GameAddress slot,title,secondary;uint32_t method,data,tag,mask;} buttons[]={
        {0x6026a0,0x58e030,0x58e070,0x4a20e0,0,0,1},
        {0x6027e0,0x58e07c,0x58e0bc,0x4a2150,0,0,2},
        {0x6027d0,0,0,0x4a2730,0,0,0},{0x6027d4,0,0,0x4a2730,0,1,0},
        {0x6027d8,0,0,0x4a2730,0,2,0},{0x6027dc,0,0,0x4a2730,0,3,0},
        {0x6027c8,0,0x58e144,0,0x4a21d0,4,1},
        {0x602624,0x58e154,0x58e198,0x4a2290,0,0,16},
        {0x6025b0,0x58e1b0,0x58e1f0,0x4a23b0,0,0,32},
        {0x6025d8,0x58e204,0x58e248,0x4a24d0,0,0,64}
    };
    static const struct {GameAddress slot,normal,pressed,secondary;uint32_t method;} toggles[]={
        {0x6025dc,0x58e25c,0x58e2ac,0,0x4a27b0},
        {0x602634,0x58e324,0x58e36c,0x58e3e0,0x4a27e0},
        {0x602614,0x58e3fc,0x58e444,0x58e4b8,0x4a27e0}
    };
    static const struct {uint32_t tag;GameAddress files[4];} rows[]={
        {10,{0x58e4d4,0x58e4f4,0x58e590,0x58e5b0}},
        {1,{0x58e64c,0x58e66c,0x58e708,0x58e728}},
        {2,{0x58e7c4,0x58e7e4,0x58e880,0x58e8a0}},
        {3,{0x58e93c,0x58e95c,0x58e9f8,0x58ea18}},
        {4,{0x58eab4,0x58ead4,0x58eb70,0x58eb90}},
        {6,{0x58ec2c,0x58ec4c,0x58ece8,0x58ed08}},
        {7,{0x58eda4,0x58edc4,0x58ee60,0x58ee80}},
        {8,{0x58ef1c,0x58ef3c,0x58efd8,0x58eff8}},
        {9,{0x58f094,0x58f0b4,0x58f150,0x58f170}},
        {5,{0x58f20c,0x58f22c,0x58f2c8,0x58f2e8}}
    };
    static const unsigned availability[]={0x2d8,0x29c,0x2a0,0x2a4,0x2b0,0x29c,0x2a0,0x2a4,0x2b0,0x2d4};
    GameAddress base,rebels,imperials,control,history,player=get(a,0x5f0208);unsigned i,index=0;int16_t count;
    uint16_t bits=game_memory_read_u16(&a->memory,0x639a74);
    if(!player || get(a,0x60259c) || get(a,0x602620) || get(a,0x60261c) || get(a,0x602698) || a->memory.fault)return 0;
    base=container_create();put(a,0x60259c,base);if(!base)goto fail;
    rebels=container_create();put(a,0x602620,rebels);if(!rebels)goto fail;
    imperials=container_create();put(a,0x60261c,imperials);if(!imperials)goto fail;
    put(a,0x602678,0);
    for(i=0;i<sizeof(buttons)/sizeof(buttons[0]);++i){
        control=control_create(a,0,buttons[i].method,buttons[i].data,buttons[i].title,0,NULL,NULL);
        if(!control)goto fail;
        put(a,control+12,0);put(a,control+0x120,buttons[i].tag);put(a,control+0x124,buttons[i].mask);
        if((buttons[i].secondary && !text_set(a,control,buttons[i].secondary,1)) ||
           !container_append(a,base,control)){(void)control_release(a,control);goto fail;}
        put(a,buttons[i].slot,control);
    }
    for(i=0;i<sizeof(toggles)/sizeof(toggles[0]);++i)
        if(!image_control(a,1,base,toggles[i].slot,toggles[i].method,0,
                toggles[i].normal,toggles[i].pressed,toggles[i].secondary))goto fail;
    put(a,0x602618,0);
    for(i=0;i<sizeof(rows)/sizeof(rows[0]);++i){
        if(get(a,player+availability[i])==UINT32_MAX)continue;
        if(a->memory.fault)goto fail;
        if(!image_control(a,2,rebels,0x602570+index*4,0x4a25f0,rows[i].tag,rows[i].files[0],rows[i].files[1],0) ||
           !image_control(a,2,imperials,0x602500+index*4,0x4a25f0,rows[i].tag,rows[i].files[2],rows[i].files[3],0))goto fail;
        put(a,0x602618,++index);
    }
    put(a,0x6025f0,0);put(a,0x60260c,0);put(a,0x602598,0);put(a,0x6025b4,0);
    memcpy(&count,&bits,sizeof(count));if(count<0)goto fail;
    if(count){history=scene_projection_storage_allocate((size_t)count*12,"overview entity history");
        if(!history)goto fail;
        put(a,0x602698,history);}
    put(a,0x6025a0,0);put(a,0x6025b8,UINT32_MAX);
    if(!gameplay_selector_refresh(&a->memory,1,selector_reveal,a))goto fail;
    return !a->memory.fault;
fail:
    (void)game_level_geometry_controls_release(a);return 0;
}
int game_level_geometry_controls_release(GameApplication *a){
    static const GameAddress owners[]={0x60259c,0x602620,0x60261c};
    static const GameAddress aliases[]={0x6026a0,0x6027e0,0x6027d0,0x6027d4,0x6027d8,0x6027dc,
        0x6027c8,0x602624,0x6025b0,0x6025d8,0x6025dc,0x602634,0x602614,0x602678};
    unsigned i;int ok=1;GameAddress history;
    for(i=0;i<sizeof(owners)/sizeof(owners[0]);++i){GameAddress container=get(a,owners[i]);
        if(!container_release(a,container)){ok=0;continue;}put(a,owners[i],0);}
    if(!ok)return 0;
    for(i=0;i<sizeof(aliases)/sizeof(aliases[0]);++i)put(a,aliases[i],0);
    for(i=0;i<10;++i){put(a,0x602570+i*4,0);put(a,0x602500+i*4,0);}
    history=get(a,0x602698);if(history && !scene_projection_storage_free(history))return 0;
    put(a,0x602698,0);put(a,0x602618,0);return !a->memory.fault;
}
