#include "portable_sdl.h"
#include "portable_input_devices.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* Stored key bindings are stable PC scan IDs, not a host keyboard ABI. */
static const uint8_t scan_ids[SDL_SCANCODE_COUNT]={
 [SDL_SCANCODE_ESCAPE]=1,[SDL_SCANCODE_1]=2,[SDL_SCANCODE_2]=3,[SDL_SCANCODE_3]=4,
 [SDL_SCANCODE_4]=5,[SDL_SCANCODE_5]=6,[SDL_SCANCODE_6]=7,[SDL_SCANCODE_7]=8,
 [SDL_SCANCODE_8]=9,[SDL_SCANCODE_9]=10,[SDL_SCANCODE_0]=11,[SDL_SCANCODE_MINUS]=12,
 [SDL_SCANCODE_EQUALS]=13,[SDL_SCANCODE_BACKSPACE]=14,[SDL_SCANCODE_TAB]=15,
 [SDL_SCANCODE_Q]=16,[SDL_SCANCODE_W]=17,[SDL_SCANCODE_E]=18,[SDL_SCANCODE_R]=19,
 [SDL_SCANCODE_T]=20,[SDL_SCANCODE_Y]=21,[SDL_SCANCODE_U]=22,[SDL_SCANCODE_I]=23,
 [SDL_SCANCODE_O]=24,[SDL_SCANCODE_P]=25,[SDL_SCANCODE_LEFTBRACKET]=26,
 [SDL_SCANCODE_RIGHTBRACKET]=27,[SDL_SCANCODE_RETURN]=28,[SDL_SCANCODE_LCTRL]=29,
 [SDL_SCANCODE_A]=30,[SDL_SCANCODE_S]=31,[SDL_SCANCODE_D]=32,[SDL_SCANCODE_F]=33,
 [SDL_SCANCODE_G]=34,[SDL_SCANCODE_H]=35,[SDL_SCANCODE_J]=36,[SDL_SCANCODE_K]=37,
 [SDL_SCANCODE_L]=38,[SDL_SCANCODE_SEMICOLON]=39,[SDL_SCANCODE_APOSTROPHE]=40,
 [SDL_SCANCODE_GRAVE]=41,[SDL_SCANCODE_LSHIFT]=42,[SDL_SCANCODE_BACKSLASH]=43,
 [SDL_SCANCODE_Z]=44,[SDL_SCANCODE_X]=45,[SDL_SCANCODE_C]=46,[SDL_SCANCODE_V]=47,
 [SDL_SCANCODE_B]=48,[SDL_SCANCODE_N]=49,[SDL_SCANCODE_M]=50,[SDL_SCANCODE_COMMA]=51,
 [SDL_SCANCODE_PERIOD]=52,[SDL_SCANCODE_SLASH]=53,[SDL_SCANCODE_RSHIFT]=54,
 [SDL_SCANCODE_KP_MULTIPLY]=55,[SDL_SCANCODE_LALT]=56,[SDL_SCANCODE_SPACE]=57,
 [SDL_SCANCODE_CAPSLOCK]=58,[SDL_SCANCODE_F1]=59,[SDL_SCANCODE_F2]=60,
 [SDL_SCANCODE_F3]=61,[SDL_SCANCODE_F4]=62,[SDL_SCANCODE_F5]=63,[SDL_SCANCODE_F6]=64,
 [SDL_SCANCODE_F7]=65,[SDL_SCANCODE_F8]=66,[SDL_SCANCODE_F9]=67,[SDL_SCANCODE_F10]=68,
 [SDL_SCANCODE_NUMLOCKCLEAR]=69,[SDL_SCANCODE_SCROLLLOCK]=70,[SDL_SCANCODE_KP_7]=71,
 [SDL_SCANCODE_KP_8]=72,[SDL_SCANCODE_KP_9]=73,[SDL_SCANCODE_KP_MINUS]=74,
 [SDL_SCANCODE_KP_4]=75,[SDL_SCANCODE_KP_5]=76,[SDL_SCANCODE_KP_6]=77,
 [SDL_SCANCODE_KP_PLUS]=78,[SDL_SCANCODE_KP_1]=79,[SDL_SCANCODE_KP_2]=80,
 [SDL_SCANCODE_KP_3]=81,[SDL_SCANCODE_KP_0]=82,[SDL_SCANCODE_KP_PERIOD]=83,
 [SDL_SCANCODE_NONUSBACKSLASH]=86,[SDL_SCANCODE_F11]=87,[SDL_SCANCODE_F12]=88,
 [SDL_SCANCODE_F13]=100,[SDL_SCANCODE_F14]=101,[SDL_SCANCODE_F15]=102,
 [SDL_SCANCODE_INTERNATIONAL1]=115,[SDL_SCANCODE_INTERNATIONAL3]=125,
 [SDL_SCANCODE_KP_EQUALS]=141,[SDL_SCANCODE_KP_ENTER]=156,[SDL_SCANCODE_RCTRL]=157,
 [SDL_SCANCODE_KP_DIVIDE]=181,[SDL_SCANCODE_PRINTSCREEN]=183,[SDL_SCANCODE_RALT]=184,
 [SDL_SCANCODE_PAUSE]=197,[SDL_SCANCODE_HOME]=199,[SDL_SCANCODE_UP]=200,
 [SDL_SCANCODE_PAGEUP]=201,[SDL_SCANCODE_LEFT]=203,[SDL_SCANCODE_RIGHT]=205,
 [SDL_SCANCODE_END]=207,[SDL_SCANCODE_DOWN]=208,[SDL_SCANCODE_PAGEDOWN]=209,
 [SDL_SCANCODE_INSERT]=210,[SDL_SCANCODE_DELETE]=211,[SDL_SCANCODE_LGUI]=219,
 [SDL_SCANCODE_RGUI]=220,[SDL_SCANCODE_APPLICATION]=221
};
enum {INPUT_OK=0,INPUT_OVERFLOW=1};
#define INPUT_INVALID ((int32_t)-2147024809)
#define INPUT_MISSING ((int32_t)-2147024875)
typedef struct InputQueue {GameInputEvent events[256];unsigned first,count;int overflow;} InputQueue;
typedef struct GameInputDevices {
    GameApplication *application;InputQueue queues[2];uint32_t sequence;
    uint8_t keys[256],buttons[3];int acquired,panel_motion_owner;
    int32_t panel_motion_remainder[2];double motion_remainder[3];
    SDL_Joystick *joystick;SDL_Haptic *haptic;int effect,initialized;
    SDL_HapticEffect periodic;
} GameInputDevices;
static void joystick_open(GameInputDevices *);
static void joystick_close(GameInputDevices *d){
    if(d->haptic){SDL_StopHapticEffects(d->haptic);
        if(d->effect>=0)SDL_DestroyHapticEffect(d->haptic,d->effect);
        SDL_CloseHaptic(d->haptic);d->haptic=NULL;}
    if(d->joystick){SDL_CloseJoystick(d->joystick);d->joystick=NULL;}
    d->effect=-1;
    if(d->initialized&SDL_INIT_HAPTIC){SDL_QuitSubSystem(SDL_INIT_HAPTIC);d->initialized&=~SDL_INIT_HAPTIC;}
}
static const uint32_t input_slots[]={0x604494u,0x604c1cu,0x604c2cu,0x604530u,0x6044ccu,0x604d6cu,0x604d68u};
static void publish(GameInputDevices *d,unsigned slot,int present){
    game_memory_write_u32(&d->application->memory,input_slots[slot],present?0xf6000000u+slot*16u:0u);
}
static void enqueue(GameInputDevices *d,unsigned which,uint32_t offset,uint32_t value,uint32_t tick){
    InputQueue *q=&d->queues[which];GameInputEvent *e;
    if(q->count==256){q->first=(q->first+1)%256;--q->count;q->overflow=1;}
    e=&q->events[(q->first+q->count++)%256];
    e->offset=offset;e->value=value;e->time=tick;e->sequence=++d->sequence;
}
static void panel_motion_event(GameInputDevices *d,int panel,GameInputEvent *event){
    int axis;int32_t raw;int64_t total,whole;
    if(d->panel_motion_owner!=panel){d->panel_motion_owner=panel;
        d->panel_motion_remainder[0]=d->panel_motion_remainder[1]=0;}
    if(!panel || (event->offset!=0 && event->offset!=4))return;
    axis=event->offset==4;memcpy(&raw,&event->value,4);
    total=(int64_t)raw+d->panel_motion_remainder[axis];whole=(total/2)*2;
    if(whole>INT32_MAX)whole=INT32_MAX-1;
    if(whole<INT32_MIN)whole=INT32_MIN;
    d->panel_motion_remainder[axis]=(int32_t)(total-whole);event->value=(uint32_t)whole;
}
void game_input_devices_discard(GameApplication *a){
    GameInputDevices *d=a->input_devices;if(!d)return;
    memset(d->queues,0,sizeof(d->queues));memset(d->motion_remainder,0,sizeof(d->motion_remainder));
    d->panel_motion_owner=0;memset(d->panel_motion_remainder,0,sizeof(d->panel_motion_remainder));
}
static void motion(GameInputDevices *d,unsigned axis,double delta,uint32_t tick){
    double total=delta+d->motion_remainder[axis];int32_t whole;
    if(!isfinite(total))return;
    if(total>INT32_MAX)total=INT32_MAX;
    if(total<INT32_MIN)total=INT32_MIN;
    whole=(int32_t)total;d->motion_remainder[axis]=total-whole;
    if(whole)enqueue(d,1,axis*4,(uint32_t)whole,tick);
}
void game_input_devices_event(GameApplication *a,const SDL_Event *e){
    GameInputDevices *d=a->input_devices;uint32_t tick;unsigned key,button;
    if(!d || game_application_isolated_input(a))return;
    if(e->type==SDL_EVENT_JOYSTICK_ADDED && (d->initialized&SDL_INIT_JOYSTICK)){
        if(d->joystick && !SDL_JoystickConnected(d->joystick))joystick_close(d);
        if(!d->joystick)joystick_open(d);
        return;
    }
    if(!d->acquired)return;
    tick=(uint32_t)(e->common.timestamp/1000000u);
    switch(e->type){
    case SDL_EVENT_KEY_DOWN:case SDL_EVENT_KEY_UP:
        if(e->key.repeat || (unsigned)e->key.scancode>=SDL_SCANCODE_COUNT)return;
        key=scan_ids[e->key.scancode];if(!key)return;
        d->keys[key]=e->key.down?128:0;enqueue(d,0,key,d->keys[key],tick);break;
    case SDL_EVENT_MOUSE_MOTION:
        motion(d,0,e->motion.xrel,tick);motion(d,1,e->motion.yrel,tick);break;
    case SDL_EVENT_MOUSE_WHEEL:
        motion(d,2,(e->wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1.0:1.0)*e->wheel.y*120.0,tick);break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:case SDL_EVENT_MOUSE_BUTTON_UP:
        button=e->button.button==SDL_BUTTON_LEFT?0:e->button.button==SDL_BUTTON_RIGHT?1:
            e->button.button==SDL_BUTTON_MIDDLE?2:3;
        if(button==3)return;
        d->buttons[button]=e->button.down?128:0;enqueue(d,1,12+button,d->buttons[button],tick);break;
    default:break;
    }
}
uint16_t game_application_key_state(GameApplication *a,unsigned key,int asynchronous){
    SDL_Scancode scan=SDL_SCANCODE_UNKNOWN;const bool *keys;int count;SDL_Keymod mod;
    if(game_application_isolated_input(a))return 0;
    /* Blocking network synchronization also polls Escape outside the frame
     * pump. Refresh SDL's state there, leaving events queued for normal dispatch. */
    if(asynchronous)SDL_PumpEvents();
    if(SDL_GetKeyboardFocus()!=game_menu_sdl_window(a))return 0;
    switch(key){case 0x10:mod=SDL_KMOD_SHIFT;break;case 0x11:mod=SDL_KMOD_CTRL;break;
    case 0x12:mod=SDL_KMOD_ALT;break;
    case 0x1b:scan=SDL_SCANCODE_ESCAPE;mod=0;break;
    case 0x20:scan=SDL_SCANCODE_SPACE;mod=0;break;
    case 0x21:scan=SDL_SCANCODE_PAGEUP;mod=0;break;
    case 0x22:scan=SDL_SCANCODE_PAGEDOWN;mod=0;break;
    default:scan=SDL_GetScancodeFromKey(key>='A' && key<='Z'?key+32:key,NULL);mod=0;break;}
    if(mod)return (SDL_GetModState()&mod)?0x8000:0;
    keys=SDL_GetKeyboardState(&count);return scan>0 && (int)scan<count && keys[scan]?0x8000:0;
}
static int32_t axis_value(Sint16 value,int range){return (int32_t)((int64_t)value*range/(value<0?32768:32767));}
static uint32_t hat_value(Uint8 hat){
    switch(hat){case SDL_HAT_UP:return 0;case SDL_HAT_RIGHTUP:return 4500;
    case SDL_HAT_RIGHT:return 9000;case SDL_HAT_RIGHTDOWN:return 13500;
    case SDL_HAT_DOWN:return 18000;case SDL_HAT_LEFTDOWN:return 22500;
    case SDL_HAT_LEFT:return 27000;case SDL_HAT_LEFTUP:return 31500;default:return UINT32_MAX;}
}
static int joystick_read(GameInputDevices *d,GameJoystickSample *s){
    int i,n;memset(s,0,sizeof(*s));for(i=0;i<4;++i)s->pov[i]=UINT32_MAX;
    if(!d->joystick)return INPUT_MISSING;
    /* A neutral sample on focus loss/disconnection clears gameplay's held edges. */
    if(!d->acquired || !SDL_JoystickConnected(d->joystick))return INPUT_OK;
    n=SDL_GetNumJoystickAxes(d->joystick);for(i=0;i<n && i<8;++i)
        s->position[i]=axis_value(SDL_GetJoystickAxis(d->joystick,i),i<6?10000:i==6?512:256);
    n=SDL_GetNumJoystickHats(d->joystick);for(i=0;i<n && i<4;++i)s->pov[i]=hat_value(SDL_GetJoystickHat(d->joystick,i));
    n=SDL_GetNumJoystickButtons(d->joystick);for(i=0;i<n && i<128;++i)s->buttons[i]=SDL_GetJoystickButton(d->joystick,i)?128:0;
    return INPUT_OK;
}
static void publish_sample(GameMemory *m,GameAddress address,const GameJoystickSample *s){
    unsigned i;for(i=0;i<8;++i)game_memory_write_u32(m,address+i*4,(uint32_t)s->position[i]);
    for(i=0;i<4;++i)game_memory_write_u32(m,address+32+i*4,s->pov[i]);
    game_memory_write(m,address+48,s->buttons,128);
    for(i=0;i<24;++i)game_memory_write_u32(m,address+176+i*4,0);
}
static void joystick_open(GameInputDevices *d){
    GameApplication *a=d->application;GameMemory *m=&a->memory;SDL_JoystickID *ids;
    int count,i,axes,buttons,hats;uint32_t flags=0;GameJoystickSample sample;
    ids=SDL_GetJoysticks(&count);for(i=0;ids && i<count;++i){d->joystick=SDL_OpenJoystick(ids[i]);if(d->joystick)break;}SDL_free(ids);
    if(!d->joystick)return;
    axes=SDL_GetNumJoystickAxes(d->joystick);buttons=SDL_GetNumJoystickButtons(d->joystick);hats=SDL_GetNumJoystickHats(d->joystick);
    if(axes>2)flags|=1;
    if(axes>5)flags|=2;
    if(axes>6)flags|=4;
    if(axes>7)flags|=8;
    if(hats>0)flags|=16;
    game_memory_fill_u8(m,0x604d70,404,0,0);
    {const char *name=SDL_GetJoystickName(d->joystick);size_t n=name?strlen(name):0;
     if(n>31)n=31;
     if(n)game_memory_write(m,0x604d74,name,n);}
    game_memory_write_u16(m,0x604d70,SDL_GetJoystickVendor(d->joystick));
    game_memory_write_u16(m,0x604d72,SDL_GetJoystickProduct(d->joystick));
    for(i=0;i<3;++i){game_memory_write_u32(m,0x604d94+i*8,(uint32_t)-10000);game_memory_write_u32(m,0x604d98+i*8,10000);}
    game_memory_write_u32(m,0x604dac,buttons);game_memory_write_u32(m,0x604dd0,flags);
    game_memory_write_u32(m,0x604dd4,8);game_memory_write_u32(m,0x604dd8,axes);game_memory_write_u32(m,0x604ddc,128);
    game_memory_write_u32(m,0x6044d0,1000);game_memory_write_u32(m,0x6044d4,1000);
    joystick_read(d,&sample);publish_sample(m,0x604650,&sample);publish_sample(m,0x604538,&sample);
    game_memory_write_u32(m,0x604d5c,(uint32_t)sample.position[6]);game_memory_write_u32(m,0x604d60,(uint32_t)sample.position[7]);
    if(SDL_IsJoystickHaptic(d->joystick) && SDL_InitSubSystem(SDL_INIT_HAPTIC)){
        d->initialized|=SDL_INIT_HAPTIC;d->haptic=SDL_OpenHapticFromJoystick(d->joystick);
        if(d->haptic){SDL_HapticEffectType types=SDL_GetHapticFeatures(d->haptic);
            SDL_HapticEffectType periodic[]={SDL_HAPTIC_SINE,SDL_HAPTIC_TRIANGLE,SDL_HAPTIC_SAWTOOTHUP,SDL_HAPTIC_SAWTOOTHDOWN};
            for(i=0;i<4;++i)if(types&periodic[i]){
                d->periodic.periodic.type=periodic[i];d->periodic.periodic.direction.type=SDL_HAPTIC_CARTESIAN;
                d->periodic.periodic.direction.dir[1]=1;d->periodic.periodic.length=SDL_HAPTIC_INFINITY;
                d->periodic.periodic.period=100;d->periodic.periodic.magnitude=32767;
                d->effect=SDL_CreateHapticEffect(d->haptic,&d->periodic);break;
            }
            (void)SDL_SetHapticAutocenter(d->haptic,100);
        }
    }
    game_memory_write_u32(m,0x604c30,d->effect>=0);publish(d,3,1);publish(d,4,d->effect>=0);
    game_memory_write_u32(m,0x604bf0,44);game_memory_write_u32(m,0x604bf4,d->effect>=0?0x100:0);
    game_memory_write_u32(m,0x604bfc,axes);game_memory_write_u32(m,0x604c00,buttons);game_memory_write_u32(m,0x604c04,hats);
    game_memory_write_u32(m,0x604768,1);
}
int game_input_devices_begin(GameApplication *a){
    GameInputDevices *d;GameAddress settings;
    if(a->input_devices)return 0;
    d=calloc(1,sizeof(*d));if(!d)return 0;
    d->application=a;d->effect=-1;a->input_devices=d;
    d->acquired=!game_application_isolated_input(a) && SDL_GetKeyboardFocus()==game_menu_sdl_window(a);
    publish(d,0,1);publish(d,1,1);publish(d,2,1);publish(d,5,1);publish(d,6,1);
    game_memory_write_u32(&a->memory,0x604768,0);game_memory_write_u32(&a->memory,0x604c30,0);
    settings=game_memory_read_u32(&a->memory,0x5943c8);
    if(!game_application_isolated_input(a) && game_memory_read_u32(&a->memory,settings+0x50)){
#ifdef _WIN32
        /* Xbox BLE can be absent from XInput. Keep other SDL discovery paths
         * available without disabling XInput for controllers that use it. */
        SDL_SetHint(SDL_HINT_JOYSTICK_GAMEINPUT,"1");
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI,"1");
#endif
        if(SDL_InitSubSystem(SDL_INIT_JOYSTICK)){d->initialized|=SDL_INIT_JOYSTICK;joystick_open(d);}
        game_memory_write_u32(&a->memory,0x594d30,2-game_memory_read_u32(&a->memory,settings+0x50));
    }
    if(a->memory.fault){game_input_devices_end(a);return 0;}return 1;
}
int game_menu_input_acquire(GameApplication *a,unsigned which){
    GameInputDevices *d=a->input_devices;
    if(d && which<=2 && !game_application_isolated_input(a))d->acquired=SDL_GetKeyboardFocus()==game_menu_sdl_window(a);
    return 1;
}
int game_menu_input_unacquire(GameApplication *a){
    GameInputDevices *d=a->input_devices;unsigned i;uint32_t tick=(uint32_t)SDL_GetTicks();if(!d)return 1;
    if(!d->acquired)return 1;
    game_input_devices_discard(a);d->acquired=0;
    /* Release commands survive focus loss, so held movement/fire cannot stick. */
    for(i=1;i<256;++i)if(d->keys[i]){enqueue(d,0,i,0,tick);d->keys[i]=0;}
    for(i=0;i<3;++i)if(d->buttons[i]){enqueue(d,1,12+i,0,tick);d->buttons[i]=0;}
    if(d->haptic)(void)SDL_StopHapticEffects(d->haptic);
    return 1;
}
void game_input_devices_end(GameApplication *a){
    GameInputDevices *d=a->input_devices;unsigned i;if(!d)return;a->input_devices=NULL;
    for(i=0;i<7;++i)publish(d,i,0);
    game_memory_write_u32(&a->memory,0x604768,0);game_memory_write_u32(&a->memory,0x604c30,0);
    joystick_close(d);
    if(d->initialized)SDL_QuitSubSystem((SDL_InitFlags)d->initialized);
    free(d);
}
int32_t game_input_device_events(GameApplication *a,unsigned which,GameInputEvent *events,unsigned *count){
    GameInputDevices *d=a->input_devices;InputQueue *q;unsigned used,i;int status;
    if(!d || which>1 || !count || *count>64 || (*count && !events))return INPUT_INVALID;
    if(game_application_isolated_input(a)){*count=0;return INPUT_OK;}
    q=&d->queues[which];used=*count<q->count?*count:q->count;status=q->overflow?INPUT_OVERFLOW:INPUT_OK;q->overflow=0;
    for(i=0;i<used;++i){events[i]=q->events[(q->first+i)%256];
        if(which){int panel=game_memory_read_u32(&a->memory,0x604760)==1?
            (game_memory_read_u32(&a->memory,0x584048)?1:game_memory_read_u32(&a->memory,0x589abc)?2:0):0;
            panel_motion_event(d,panel,&events[i]);}}
    q->first=(q->first+used)%256;q->count-=used;*count=used;return status;
}
int32_t game_input_device_joystick_poll(GameApplication *a){
    GameInputDevices *d=a->input_devices;if(!d || !d->joystick)return INPUT_MISSING;SDL_UpdateJoysticks();return INPUT_OK;
}
int32_t game_input_device_joystick_read(GameApplication *a,GameJoystickSample *s){
    GameInputDevices *d=a->input_devices;return !s?INPUT_INVALID:!d?INPUT_MISSING:joystick_read(d,s);
}
int32_t game_input_device_pulse(GameApplication *a,GameInputPulseAction action,uint32_t magnitude,uint32_t period){
    GameInputDevices *d=a->input_devices;uint32_t ms;
    if(!d || !d->haptic || d->effect<0)return INPUT_MISSING;
    if(action==GAME_INPUT_PULSE_START)return SDL_RunHapticEffect(d->haptic,d->effect,1)?0:INPUT_MISSING;
    if(action==GAME_INPUT_PULSE_STOP)return SDL_StopHapticEffect(d->haptic,d->effect)?0:INPUT_MISSING;
    if(action!=GAME_INPUT_PULSE_CONFIGURE)return INPUT_INVALID;
    if(magnitude>10000)magnitude=10000;
    ms=period/1000;if(!ms)ms=1;if(ms>65535)ms=65535;
    d->periodic.periodic.period=(Uint16)ms;d->periodic.periodic.magnitude=(Sint16)(magnitude*32767u/10000u);
    return SDL_UpdateHapticEffect(d->haptic,d->effect,&d->periodic)?0:INPUT_MISSING;
}
