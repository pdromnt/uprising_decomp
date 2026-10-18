#include "menu_settings.h"
#include "display_transition.h"
#include "ui_frame.h"

extern int game_menu_screen_mask(GameApplication *,int enabled);
extern int game_menu_present_frame(GameApplication *,uint32_t interval);
extern int game_menu_palette(GameApplication *,GameAddress);
extern int game_menu_fullscreen_image(GameApplication *,GameAddress,uint32_t stride);
extern int game_application_timed_integer_message(GameApplication *,GameAddress,int32_t,int32_t);

static int screen_mask(void *context,int enabled) {return game_menu_screen_mask(context,enabled);}
static int present(void *context,uint32_t interval) {return game_menu_present_frame(context,interval);}
static int palette(void *context,GameAddress address) {return game_menu_palette(context,address);}
static int fullscreen(void *context,GameAddress address,uint32_t stride) {return game_menu_fullscreen_image(context,address,stride);}

/* Connect 004afc00 and its actual 004d81c0/004d84b0 consumers to the existing
 * layout and frame algorithms. Stable method IDs select native functions only. */
int game_menu_render_quality(GameApplication *a,uint32_t quality) {
    GameAddress refresh;
    GameUiFrameOps ops={a,screen_mask,present,palette,fullscreen};
    if(!game_display_change_runtime_mode(&a->memory,quality,&refresh))return 0;
    switch(refresh) {
    case 0:return !a->memory.fault;
    case 0x4d81c0u:return game_ui_frame_run(&a->memory,GAME_UI_FRAME_CLEAR,&ops);
    case 0x4d84b0u:return game_ui_frame_run(&a->memory,GAME_UI_FRAME_PALETTE_TRANSITION,&ops);
    default:return 0;
    }
}
/*4afb10 and both keyboard callers: cycle the current HUD, update its actual
 * renderer/layout/scalar and frame consumers, then publish the timed label. */
int game_input_display_cycle(GameApplication *a){
    GameMemory *m=&a->memory;GameDisplayScalarPlan plan;GameUiFrameKind kind;
    GameUiFrameOps ops={a,screen_mask,present,palette,fullscreen};
    uint32_t old=game_memory_read_u32(m,0x597f88),mode,alternate;int32_t signed_old,signed_mode;
    GameAddress renderer,label;
    signed_old=old<=INT32_MAX?(int32_t)old:(int32_t)((int64_t)old-4294967296LL);
    mode=signed_old>1?0:1u-old;game_memory_write_u32(m,0x597f88,mode);
    renderer=game_memory_read_u32(m,0x5943c8);game_memory_write_u32(m,renderer+0x38,mode);
    alternate=game_memory_read_u32(m,0x6038a0);
    if(!game_display_layout(m,0,0,alternate!=0) || !game_display_select_transition_scalar(m,&plan))return 0;
    game_display_sample_scalar(m,plan.scalar);
    if(game_memory_read_u32(m,0x5840f0) && (alternate==0 || alternate==1)){
        kind=!mode?GAME_UI_FRAME_CLEAR:alternate?GAME_UI_FRAME_WORK_IMAGE:GAME_UI_FRAME_PALETTE_TRANSITION;
        if(!game_ui_frame_run(m,kind,&ops))return 0;
    }
    label=game_memory_read_u32(m,0x604760)==2?0x595d90:0x595d34;
    mode=game_memory_read_u32(m,0x597f88);
    signed_mode=mode<=INT32_MAX?(int32_t)mode:(int32_t)((int64_t)mode-4294967296LL);
    return !m->fault && game_application_timed_integer_message(a,label,1,signed_mode);
}
