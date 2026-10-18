#include "level_run.h"
#include "gameplay_state.h"
#include "game_initializers.h"
#include "gameplay_lookup_plans.h"
#include "display_transition.h"
#include "radar_compositor.h"
#include "scene_projection_storage.h"
#include <math.h>
#include <string.h>

extern int game_application_mode_notify(GameApplication *,GameAddress);
extern int game_menu_pointer_position(GameApplication *,int32_t,int32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static void real_put(GameApplication *a,GameAddress p,float v){uint32_t bits;memcpy(&bits,&v,4);put(a,p,bits);}
static float real_bits(uint32_t bits){float v;memcpy(&v,&bits,4);return v;}
static uint32_t half(uint32_t v){return (v>>1)|((v&0x80000000u)?0x80000000u:0);}

/*4e94e0/4e9530: this is the actual minimap reciprocal table, not a command
 *queue. Radar's native compositor borrows the same bounded4096-byte owner. */
int game_level_radar_tables_begin(GameApplication *a){
    GameAddress table,publication;
    if(!a || a->memory.fault)return 0;
    publication=get(a,0x596268);if(a->memory.fault)return 0;
    if(a->radar_reciprocals)return publication==a->radar_reciprocals &&
        scene_projection_storage_borrow_bytes(publication,4096)!=NULL;
    if(publication)return 0; /*Do not adopt a foreign allocation. */
    table=scene_projection_storage_allocate(4096,"minimap reciprocal table");if(!table)return 0;
    a->radar_reciprocals=table;game_initializer_reciprocal_table(&a->memory,table);
    if(a->memory.fault){(void)game_level_radar_tables_end(a);return 0;}
    return 1;
}
int game_level_radar_tables_end(GameApplication *a){
    GameAddress table,publication,saved_address;uint32_t saved_size;int saved_fault,ok=1;
    if(!a)return 0;
    table=a->radar_reciprocals;if(!table)return 1;
    if(!scene_projection_storage_borrow_bytes(table,4096))return 0;
    saved_fault=a->memory.fault;saved_address=a->memory.fault_address;saved_size=a->memory.fault_size;a->memory.fault=0;
    publication=get(a,0x596268);
    if(!a->memory.fault && publication==table)put(a,0x596268,0);
    else if(publication || a->memory.fault)ok=0;
    if(!scene_projection_storage_free(table))ok=0;else a->radar_reciprocals=0;
    if(a->memory.fault)ok=0;
    if(saved_fault){a->memory.fault=saved_fault;a->memory.fault_address=saved_address;a->memory.fault_size=saved_size;}
    return ok;
}

/*435960/4846d0 and435a00/484760. The existing row objects own their rectangles;
 *these two tables borrow them. Signed, inclusive hit tests retain the last hit. */
static int selection_layout(GameApplication *a,unsigned sibling){
    GameAddress selected=sibling?0x5f0230:0x5ace7c,count=sibling?0x5f022c:0x5acefc;
    GameAddress objects=sibling?0x5f0250:0x5ace80,bounds=sibling?0x5f0240:0x5acec0;
    GameplayViewportLayoutPlan plan=gameplay_state_viewport_layout_plan(&a->memory,1,sibling!=0);
    uint32_t i,top,x,right,bottom,mouse_x,mouse_y;GameAddress row;
    gameplay_state_apply_viewport_layout(&a->memory,&plan,sibling!=0);put(a,selected,UINT32_MAX);
    /*Reject corrupt unbounded walks; each borrowed row is checked by memory. */
    if((int32_t)get(a,count)>65536)return 0;
    for(i=0;!a->memory.fault && (int32_t)i<(int32_t)get(a,count);++i){
        x=get(a,bounds)+1;top=get(a,bounds+4)+i*14+1;
        right=get(a,bounds+8)-1;bottom=top+12;row=get(a,objects+i*4);
        if(!row)return 0;
        put(a,row+0xa8,x);put(a,row+0xac,bottom);put(a,row+0xb0,right);put(a,row+0xb4,top);
        mouse_x=get(a,0x5ac9c8);mouse_y=get(a,0x5ac9cc);
        if((int32_t)x<=(int32_t)mouse_x && (int32_t)right>=(int32_t)mouse_x &&
           (int32_t)top<=(int32_t)mouse_y && (int32_t)bottom>=(int32_t)mouse_y)put(a,selected,i);
    }
    return !a->memory.fault;
}
/*4a28e0: replace the existing scratch and circular mask owners. Allocate both
 *before publishing; failed replacement retains the previous usable pair. */
int game_level_display_mask_resize(GameApplication *a,uint32_t diameter){
    GameAddress old=get(a,0x602684),old_mask=get(a,0x6027cc),scratch,mask;
    uint32_t old_diameter=get(a,0x602610);size_t bytes=(size_t)diameter*diameter,old_bytes;
    if(!diameter || diameter>4096 || (old==old_mask && old))return 0;
    if((old || old_mask) && (!old_diameter || old_diameter>4096))return 0;
    old_bytes=(size_t)old_diameter*old_diameter;
    if((old && !scene_projection_storage_borrow_bytes(old,old_bytes)) ||
       (old_mask && !scene_projection_storage_borrow_bytes(old_mask,old_bytes)))return 0;
    scratch=scene_projection_storage_allocate(bytes,"display radar scratch");if(!scratch)return 0;
    mask=scene_projection_storage_allocate(bytes,"display radar circle");
    if(!mask){scene_projection_storage_free(scratch);return 0;}
    if(!game_display_circle_template(&a->memory,mask,diameter)){
        scene_projection_storage_free(mask);scene_projection_storage_free(scratch);return 0;
    }
    if(old)scene_projection_storage_free(old);
    if(old_mask)scene_projection_storage_free(old_mask);
    put(a,0x602684,scratch);put(a,0x6027cc,mask);put(a,0x602610,diameter);
    game_memory_write_u16(&a->memory,0x602680,(uint16_t)diameter);
    game_memory_write_u16(&a->memory,0x602682,(uint16_t)diameter);
    return !a->memory.fault;
}
/*4ad700: immutable display geometry constants, published into the existing
 *authoritative globals. The three square roots use actual arithmetic. */
void game_level_display_geometry_constants(GameApplication *a){
    static const struct{uint32_t offset,low,high;} values[]={
        {0x3e34,0x42c80000,0x43660000},{0x32dc,0x42340000,0x42c80000},
        {0x3854,0x425c0000,0x43020000},{0x3858,0x43680000,0x43de0000},
        {0x3860,0x439a0000,0x441a0000},{0x3e30,0x43870000,0x44048000},
        {0x3850,0x43870000,0x44048000},{0x385c,0x42f40000,0x438c0000},
        {0x3864,0x42f40000,0x438c0000},{0x409c,0x41800000,0x42000000},
        {0x37f0,0x43960000,0x44160000},{0x40a8,0x43960000,0x44160000},
        {0x40ac,0x40800000,0x42000000},{0x40b4,0x41e00000,0x42a00000},
        {0x40bc,0x41e00000,0x42a00000},{0x37f4,0x41a00000,0x42800000},
        {0x40b0,0x438f8000,0x440f4000},{0x38f4,0xc2180000,0xc2ac0000},
        {0x38fc,0,0},{0x40b8,0x439c8000,0x441cc000},{0x38f0,0xc2860000,0xc3160000},
        {0x390c,0xc2180000,0xc2ac0000},{0x38f8,0x469da800,0x47b11c00},
        {0x3900,0x42980000,0x432c0000},{0x3908,0x42860000,0x43160000},
        {0x3f8c,0xc2060000,0xc2960000},{0x3f78,0,0},{0x3904,0xc610e000,0xc73c2000},
        {0x3f7c,0x42860000,0x43160000},{0x3f84,0xc2060000,0xc2960000},
        {0x3910,0xc67a0000,0xc7857000},{0x3f88,0xc2640000,0xc3010000},
        {0x3f80,0x42640000,0x43010000}
    };
    unsigned i;int high=get(a,0x597f80)==1;long double x,y;float squared;
    for(i=0;i<sizeof(values)/sizeof(values[0]);++i)put(a,0x600000+values[i].offset,high?values[i].high:values[i].low);
    real_put(a,0x60380c,(float)sqrtl(real_bits(high?0x46adf400:0x45889a00)));
    x=real_bits(get(a,0x603f7c));y=real_bits(get(a,0x603f78));squared=(float)(x*x+y*y);
    real_put(a,0x603808,(float)sqrtl(squared));
    x=real_bits(get(a,0x603f8c));y=real_bits(get(a,0x603f88));squared=(float)(x*x+y*y);
    real_put(a,0x603810,(float)sqrtl(squared));
}
/*4af770/51a070: ordinary scalar selection and truncation, no CPU plan/flags. */
int game_level_default_viewport(GameApplication *a){
    float offset=0;uint32_t bits;
    if((int32_t)get(a,0x597f88)>1 && get(a,0x58f89c)==get(a,0x58f890) && get(a,0x59d440)==0x3f800000)
        offset=get(a,0x6038a0)?16.0f:get(a,0x597f80)?99.0f:51.0f;
    memcpy(&bits,&offset,4);put(a,0x626e4c,bits);put(a,0x597f5c,get(a,0x62792c)-(uint32_t)offset);
    return !a->memory.fault;
}
int game_level_display_sync(GameApplication *a,uint32_t value){put(a,0x597f60,value);return !a->memory.fault;}
static int reveal(void *context,GameAddress position,float radius){return game_radar_reveal(&((GameApplication *)context)->memory,position,radius);}
/*5179c0 ignores the supplied settings argument in the preserved executable.
 *It rebuilds the high layout, then publishes the requested view dimensions. */
int game_level_display_visibility(GameApplication *a,int value){
    GameplayPresentationDimensions d;uint32_t dx,dy;GameMemory *m=&a->memory;(void)value;
    gameplay_lookup_initialize_presentation(m);
    if(!gameplay_selector_refresh(m,1,reveal,a) || !selection_layout(a,0) || !selection_layout(a,1) ||
       !game_application_mode_notify(a,get(a,0x628a88)))return 0;
    put(a,0x604064,UINT32_MAX);put(a,0x6037e8,30);put(a,0x6034d0,128);
    if(!game_level_display_mask_resize(a,128) || !game_display_layout(m,0,0,0))return 0;
    game_level_display_geometry_constants(a);
    if((int32_t)get(a,0x597f88)>1 && !game_level_default_viewport(a))return 0;
    if(!game_display_button_rectangles(m))return 0;
    d=gameplay_lookup_publish_dimensions(m);
    if(!game_menu_pointer_position(a,(int32_t)half(d.width),(int32_t)half(d.height)))return 0;
    gameplay_lookup_publish_presentation_center(m,d);
    dx=(d.height>>3)|((d.height&0x80000000u)?0xe0000000u:0);dy=half(d.height);
    put(a,0x5acce0,half(d.width)+dx);put(a,0x5ac920,half(d.width)-dx);
    put(a,0x5acce4,dy+dx);put(a,0x5ac924,dy-dx);
    game_initializer_004b33a0(m);
    if(!game_level_default_viewport(a))return 0;
    put(a,0x59d450,0);put(a,0x59d458,0);put(a,0x59d454,d.width);put(a,0x59d45c,d.height);
    put(a,0x597f90,0);return !m->fault;
}
int game_level_display_masks_release(GameApplication *a){
    static const GameAddress slots[]={0x602684,0x6027cc};unsigned i;GameMemory cleanup=a->memory;cleanup.fault=0;
    for(i=0;i<2;++i){GameAddress p=game_memory_read_u32(&cleanup,slots[i]);
        if(cleanup.fault)return 0;
        if(p && !scene_projection_storage_free(p))return 0;
        game_memory_write_u32(&cleanup,slots[i],0);
    }
    return !cleanup.fault;
}
