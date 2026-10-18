#include "menu_choice_controls.h"
#include "model_assets.h"
#include "game_matrix.h"
#include "game_initializers.h"
#include <math.h>
#include <string.h>
extern int game_level_geometry_prepare(GameApplication *);
extern int game_level_geometry_finalize(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
extern GameAddress game_menu_controller_construct(GameApplication *);
extern void game_menu_controller_destroy(GameApplication *,GameAddress);
extern int game_menu_register_region(GameApplication *,GameAddress,GameAddress,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister(GameApplication *,GameAddress,GameAddress,int);
extern uint32_t game_menu_milliseconds(GameApplication *);
/*Actual scene/backend submission remain mandatory native dependencies. */
extern int game_model_graph_update(GameApplication *,GameAddress);
extern int game_menu_preview_render(GameApplication *,GameAddress); /*44de80*/
static int64_t signed_word(uint32_t value){return value<0x80000000u?(int64_t)value:(int64_t)value-INT64_C(4294967296);}
static void store_real(GameApplication *a,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);put(a,p,bits);}
static int64_t camera_truncate(long double value) {
    if(!isfinite(value) || value < -9223372036854775808.0L || value >= 9223372036854775808.0L)return INT64_MIN;
    return (int64_t)truncl(value);
}
/*44b590: record a relative movement and orientation transition in the same
 * camera allocation. Angles and elapsed time retain their fixed-width wrap. */
int game_menu_preview_camera_transition(GameApplication *a,GameAddress camera,const float position[3],
        uint16_t roll,uint16_t pitch,uint16_t heading,uint32_t duration) {
    const uint16_t angles[3]={heading,pitch,roll};unsigned i;uint16_t difference;
    if(!camera || !position)return 0;
    for(i=0;i<3;++i) {
        store_real(a,camera+0x38+i*4,(float)((long double)position[i]-real(a,camera+0xc+i*4)));
        difference=(uint16_t)(angles[i]-game_memory_read_u16(&a->memory,camera+6+i*2));
        if(!i && difference<0x8000u && difference>1024)difference=(uint16_t)(difference-2048);
        game_memory_write_u16(&a->memory,camera+0x30+i*2,difference);
    }
    put(a,camera+0x2c,0);put(a,camera+0x44,12);put(a,camera+0x28,duration);
    return !a->memory.fault;
}
/*44b600: advance by an ordinary signed duration, or finish immediately. The
 * original accumulates truncated angle increments and float position stores;
 * this is deliberately not a redesigned interpolation curve. */
int game_menu_preview_camera_command(GameApplication *a,GameAddress camera,uint32_t elapsed,uint32_t finish,uint32_t *result) {
    uint32_t current,total,step,next;float position[3];long double fraction;unsigned i;
    if(!camera || !result)return 0;
    current=get(a,camera+0x2c);total=get(a,camera+0x28);step=elapsed;next=current+step;
    if(signed_word(next)>signed_word(total) || finish)step=total-current;
    next=current+step;put(a,camera+0x2c,next);
    fraction=(long double)signed_word(step)/(long double)signed_word(total);
    for(i=0;i<3;++i) {
        uint16_t angle=game_memory_read_u16(&a->memory,camera+6+i*2);
        uint16_t difference=game_memory_read_u16(&a->memory,camera+0x30+i*2);
        int32_t signed_difference=difference<0x8000u?(int32_t)difference:(int32_t)difference-65536;
        position[i]=(float)((long double)real(a,camera+0xc+i*4)+real(a,camera+0x38+i*4)*fraction);
        store_real(a,camera+0xc+i*4,position[i]);
        angle=(uint16_t)(angle+(uint16_t)(uint64_t)camera_truncate(signed_difference*fraction));
        game_memory_write_u16(&a->memory,camera+6+i*2,angle);
    }
    if(!game_matrix_camera_view(&a->memory,0x5cedf0,position,
        game_memory_read_u16(&a->memory,camera+10),game_memory_read_u16(&a->memory,camera+8),
        game_memory_read_u16(&a->memory,camera+6)))return 0;
    if(next==total){put(a,camera+0x44,10);*result=11;}else *result=get(a,camera+0x44);
    return !a->memory.fault;
}
static int capture_end(GameApplication *a,GameAddress preview) {
    GameAddress parent=get(a,preview+0x1a4),capture=get(a,0x59d4c0);
    if(!parent)return !a->memory.fault;
    if(parent==capture)return 0;
    if(!game_menu_unregister(a,capture,preview,0))return 0;
    game_menu_controller_destroy(a,capture);put(a,0x59d4c0,parent);put(a,preview+0x1a4,0);
    return !a->memory.fault;
}
/*44d430/44d650: the preview owns its camera, display surface, depth/work page
 * and model graph. Its base surface stays in this same bounded allocation. */
GameAddress game_menu_research_preview_create(GameApplication *a,uint32_t width,uint32_t height) {
    GameAddress p,camera,child,work;uint64_t count=(uint64_t)width*height*2u;float position[3]={0,0,1435};
    if(!count || count>SIZE_MAX)return 0;
    p=scene_projection_storage_allocate(0x1bc,"portable research preview");if(!p)return 0;
    if(!game_menu_image_surface_initialize(a,p,width,height,0)){scene_projection_storage_free(p);return 0;}
    put(a,p,0x5731e8);
    camera=scene_projection_storage_allocate(0x48,"portable preview camera");if(!camera)goto fail;
    put(a,p+0x174,camera);game_initializer_004a3980(&a->memory,camera);put(a,camera,0x573178);
    game_memory_write_u16(&a->memory,camera+6,0xfc00);game_memory_write_u16(&a->memory,camera+8,0);game_memory_write_u16(&a->memory,camera+10,0);
    store_real(a,camera+0xc,0);store_real(a,camera+0x10,0);store_real(a,camera+0x14,1435);
    if(!game_matrix_camera_view(&a->memory,0x5cedf0,position,0,0,0xfc00))goto fail;
    put(a,0x59d438,1);
    child=game_menu_image_surface_create(a,width,height,1);if(!child)goto fail;
    put(a,p+0x178,child);put(a,child+0x13c,254);
    if(!game_menu_image_mode(a,child,101))goto fail;
    if(!game_menu_register_tick(a,get(a,0x59d4c0),p,0x44db20,0) ||
       !game_menu_register_region(a,get(a,0x59d4c0),p,p+0x20,0x201,0x44d9d0,0) ||
       !game_menu_register_region(a,get(a,0x59d4c0),p,p+0x194,0x204,0x44da70,0))goto fail;
    work=scene_projection_storage_allocate((size_t)count,"portable preview depth work");if(!work)goto fail;
    put(a,p+0x17c,work);put(a,p+0x16c,10);
    put(a,p+0x194,0);put(a,p+0x198,0);put(a,p+0x19c,640);put(a,p+0x1a0,480);
    if(a->memory.fault)goto fail;
    return p;
fail:
    (void)game_menu_research_preview_destroy(a,p);return 0;
}
int game_menu_research_preview_destroy(GameApplication *a,GameAddress p) {
    GameAddress owned;
    if(!capture_end(a,p) || !game_menu_unregister(a,get(a,0x59d4c0),p,0))return 0;
    owned=get(a,p+0x17c);if(owned && !scene_projection_storage_free(owned))return 0;put(a,p+0x17c,0);
    owned=get(a,p+0x174);
    if(owned){put(a,owned,0x5743bc);if(!scene_projection_storage_free(owned))return 0;}put(a,p+0x174,0);
    if(!game_menu_widget_destroy(a,get(a,p+0x178)))return 0;
    put(a,p+0x178,0);
    if(!game_menu_research_preview_stop(a,p) || !game_menu_widget_visible(a,p,0) ||
       !game_menu_text_surface_release(a,p))return 0;
    put(a,p,0x573180);return scene_projection_storage_free(p) && !a->memory.fault;
}
/*44d870..44db17: one temporary input controller owns a drag; release restores
 * its parent and cancels the appropriate rotation/zoom state. */
int game_menu_research_preview_pointer(GameApplication *a,GameAddress p,uint32_t method,int32_t x,int32_t y) {
    GameAddress controller,parent;unsigned release;
    if(method==0x44d9b0){put(a,p+0x18c,(uint32_t)x);put(a,p+0x190,(uint32_t)y);return !a->memory.fault;}
    if(method==0x44da20 || method==0x44dac0) {
        if(!capture_end(a,p))return 0;
        if(method==0x44da20)put(a,p+0x1a8,0);
        else {put(a,p+0x1ac,0);put(a,p+0x1b4,0);}
        return !a->memory.fault;
    }
    if(method!=0x44d9d0 && method!=0x44da70)return 0;
    if(get(a,p+0x1a4))return 0;
    parent=get(a,0x59d4c0);controller=game_menu_controller_construct(a);if(!controller)return 0;
    put(a,p+0x1a4,parent);put(a,0x59d4c0,controller);
    release=method==0x44d9d0?0x202:0x205;
    if(!game_menu_register_region(a,controller,p,p+0x194,0x200,0x44d9b0,0) ||
       !game_menu_register_tick(a,controller,p,0x44db20,0) ||
       !game_menu_register_region(a,controller,p,p+0x194,release,method==0x44d9d0?0x44da20:0x44dac0,0)) {
        (void)capture_end(a,p);return 0;
    }
    game_memory_write_u16(&a->memory,p+0x1b0,0);game_memory_write_u16(&a->memory,p+0x1b2,0);
    put(a,p+0x184,(uint32_t)x);put(a,p+0x188,(uint32_t)y);put(a,p+0x18c,(uint32_t)x);put(a,p+0x190,(uint32_t)y);
    put(a,p+0x1b4,0);put(a,p+0x1b8,0);put(a,p+(method==0x44d9d0?0x1a8:0x1ac),1);
    return !a->memory.fault;
}
static uint16_t angle_delta(long double value) {
    int64_t integer=!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value;
    return (uint16_t)(uint64_t)integer;
}
int game_menu_research_preview_tick(GameApplication *a,GameAddress p) {
    GameAddress model=get(a,p+0x180),camera=get(a,p+0x174);uint32_t now,result,bits;unsigned i;
    float local[12],next[12],yaw[12]={0},pitch[12]={0},position[3];uint16_t angle;
    if(!model)return !a->memory.fault;
    if(get(a,p+0x1ac)) {
        float delta=(float)((long double)signed_word(get(a,p+0x190)-get(a,p+0x188))*real(a,0x5851b8)*real(a,0x5731bc));
        long double candidate=(long double)real(a,camera+0x14)+delta;
        long double radius=real(a,get(a,model+0x50)+0x20);
        if(!(candidate>=radius*real(a,0x5731bc) && candidate<=radius*real(a,0x5731c0)))delta=0;
        store_real(a,p+0x1b4,delta);
    }
    if(get(a,p+0x1a8)) {
        game_memory_write_u16(&a->memory,p+0x1b2,angle_delta((long double)signed_word(get(a,p+0x18c)-get(a,p+0x184))*real(a,0x5851b8)));
        game_memory_write_u16(&a->memory,p+0x1b0,angle_delta((long double)signed_word(get(a,p+0x190)-get(a,p+0x188))*real(a,0x5851b8)));
    }
    now=game_menu_milliseconds(a);
    if(signed_word(get(a,p+0x1b8)+25)<signed_word(now)) {
        put(a,p+0x1b8,now);put(a,p+0x184,get(a,p+0x18c));put(a,p+0x188,get(a,p+0x190));
    }
    for(i=0;i<12;++i)local[i]=real(a,model+4+i*4);
    angle=game_memory_read_u16(&a->memory,p+0x1b2);
    yaw[0]=yaw[10]=real(a,0x6298a8+((angle+512u)&2047u)*4);
    yaw[2]=real(a,0x6298a8+(angle&2047u)*4);yaw[8]=-yaw[2];yaw[5]=1;
    game_matrix_compose_values(next,yaw,local);
    angle=game_memory_read_u16(&a->memory,p+0x1b0);
    pitch[0]=1;pitch[5]=pitch[10]=real(a,0x6298a8+((angle+512u)&2047u)*4);
    pitch[9]=real(a,0x6298a8+(angle&2047u)*4);pitch[6]=-pitch[9];
    game_matrix_compose_values(local,pitch,next);
    for(i=0;i<12;++i){memcpy(&bits,&local[i],4);put(a,model+4+i*4,bits);}
    store_real(a,camera+0x14,(float)((long double)real(a,camera+0x14)+real(a,p+0x1b4)));
    for(i=0;i<3;++i)position[i]=real(a,camera+0xc+i*4);
    if(!game_matrix_camera_view(&a->memory,0x5cedf0,position,
            game_memory_read_u16(&a->memory,camera+10),game_memory_read_u16(&a->memory,camera+8),game_memory_read_u16(&a->memory,camera+6)) ||
       !game_model_graph_update(a,model))return 0;
    if(get(a,p+0x16c)==12) {
        if(!game_menu_preview_camera_command(a,camera,get(a,0x5addec),0,&result))return 0;
        if(result==11)put(a,p+0x16c,10);
    }
    if(!game_menu_preview_render(a,p))return 0;
    put(a,p+0x18,1);
    if(!get(a,p+0x1c))for(i=0;i<4;++i)put(a,p+0x30+i*4,get(a,p+0x20+i*4));
    put(a,p+0x1c,1);return !a->memory.fault;
}
int game_menu_research_preview_draw(GameApplication *a,GameAddress p,const GameUiRect *clip) {
    GameAddress child=get(a,p+0x178);
    if(!get(a,p+0x180))return !a->memory.fault;
    return clip && game_menu_widget_position(a,child,(uint32_t)clip->left,(uint32_t)clip->top,0,0) &&
        game_menu_widget_draw(a,child,clip);
}
/*44d810: stop through the actual model owner before invalidating its handle. */
int game_menu_research_preview_stop(GameApplication *a,GameAddress preview) {
    GameAddress model=get(a,preview+0x180);
    if(model && !game_application_scene_model_release(a,model))return 0;
    put(a,preview+0x180,0);return !a->memory.fault;
}
/*44d740: actual mesh load, camera pose, shared view matrix and finalization.
 * Geometry preparation and material/multi-mesh services are mandatory native
 * dependencies shared with game startup, never opaque guest callbacks. */
int game_menu_research_preview_load(GameApplication *a,GameAddress preview,GameAddress source) {
    GameAddress model,controller;char name[256];unsigned i;float position[3]={0,0,0};uint32_t bits;
    game_memory_write_u16(&a->memory,preview+0x1b0,0);
    game_memory_write_u16(&a->memory,preview+0x1b2,5);put(a,preview+0x1b4,0);
    if(!game_menu_research_preview_stop(a,preview) || !game_level_geometry_prepare(a))return 0;
    for(i=0;i<sizeof(name);++i){name[i]=(char)game_memory_read_u8(&a->memory,source+i);if(!name[i])break;}
    if(i==sizeof(name) || a->memory.fault)goto failed;
    model=game_model_file_load(a,name,UINT32_MAX);
    put(a,preview+0x180,model);if(!model)goto failed;
    put(a,model+0x58,1);controller=get(a,preview+0x174);
    position[2]=(float)((long double)real(a,get(a,model+0x50)+0x20)*real(a,0x5731b8));
    put(a,controller+0xc,0);put(a,controller+0x10,0);
    memcpy(&bits,&position[2],4);put(a,controller+0x14,bits);
    game_memory_write_u16(&a->memory,controller+6,0xfc00);
    game_memory_write_u16(&a->memory,controller+8,0);game_memory_write_u16(&a->memory,controller+10,0);
    if(!game_matrix_camera_view(&a->memory,0x5cedf0,position,0,0,0xfc00))goto failed;
    /*5326d0's signed16 sector test for this prescribed camera heading. */
    put(a,0x59d438,1);
    return !a->memory.fault && game_level_geometry_finalize(a);
failed:
    (void)game_level_geometry_finalize(a);return 0;
}
