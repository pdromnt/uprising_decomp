#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define DIRECTINPUT_VERSION 0x0500
#include <windows.h>
#include <dinput.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>
#include "portable_input_devices.h"

typedef HRESULT (WINAPI *NativeInputCreate)(HINSTANCE,DWORD,LPDIRECTINPUTA *,IUnknown *);
typedef struct GameInputDevices {
    GameApplication *application;HMODULE library;LPDIRECTINPUTA factory;
    LPDIRECTINPUTDEVICEA keyboard,mouse;LPDIRECTINPUTDEVICE2A joystick;
    LPDIRECTINPUTEFFECT calibration;HANDLE keyboard_event,mouse_event;HWND window;
    int panel_motion_owner;
    int32_t panel_motion_remainder[2];
} GameInputDevices;
/* The two popup lists halve relative mouse counts in the original game.
 * Modern high-rate mice often deliver single-count samples: truncating each
 * sample loses slow movement entirely. Carry the half-count within this input
 * owner, keeping button order and ordinary camera input unchanged. */
static void panel_motion_event(GameInputDevices *d,int panel,GameInputEvent *event){
    int axis;int32_t raw;int64_t total,whole;
    if(d->panel_motion_owner!=panel){
        d->panel_motion_owner=panel;
        d->panel_motion_remainder[0]=d->panel_motion_remainder[1]=0;
    }
    if(!panel || (event->offset!=0 && event->offset!=4))return;
    axis=event->offset==4;memcpy(&raw,&event->value,4);
    total=(int64_t)raw+d->panel_motion_remainder[axis];
    whole=(total/2)*2;
    if(whole>INT32_MAX)whole=INT32_MAX-1;
    if(whole<INT32_MIN)whole=INT32_MIN;
    d->panel_motion_remainder[axis]=(int32_t)(total-whole);
    event->value=(uint32_t)whole;
}
extern void *game_menu_window_handle(GameApplication *);
/* Host key-state polling obeys the same isolation policy as buffered devices. */
uint16_t game_application_key_state(GameApplication *a,unsigned key,int asynchronous){
    if(game_application_isolated_input(a))return 0;
    return (uint16_t)(asynchronous?GetAsyncKeyState((int)(key&255u)):GetKeyState((int)(key&255u)));
}
static const uint32_t input_slots[]={0x604494u,0x604c1cu,0x604c2cu,0x604530u,0x6044ccu,0x604d6cu,0x604d68u};
/* IDs select native owned services. None encodes a host pointer or HANDLE. */
static uint32_t input_id(unsigned slot) {return 0xf6000000u+slot*16u;}
int game_input_device_wait(GameApplication *a,uint32_t milliseconds) {
    GameInputDevices *d=a->input_devices;HANDLE events[2];DWORD result;
    if(!d || !d->keyboard_event || !d->mouse_event)return -1;
    events[0]=d->keyboard_event;events[1]=d->mouse_event;
    result=MsgWaitForMultipleObjects(2,events,FALSE,milliseconds,QS_ALLINPUT);
    if(result==WAIT_FAILED)return -1;
    if(result==WAIT_TIMEOUT)return 3;
    return result<=WAIT_OBJECT_0+2?(int)(result-WAIT_OBJECT_0):-1;
}
static void publish(GameInputDevices *d,unsigned slot,int present) {
    game_memory_write_u32(&d->application->memory,input_slots[slot],present?input_id(slot):0u);
}
static void capability_words(GameMemory *m,const JOYCAPSA *c) {
    unsigned i;const DWORD words[]={c->wXmin,c->wXmax,c->wYmin,c->wYmax,c->wZmin,c->wZmax,
        c->wNumButtons,c->wPeriodMin,c->wPeriodMax,c->wRmin,c->wRmax,c->wUmin,c->wUmax,
        c->wVmin,c->wVmax,c->wCaps,c->wMaxAxes,c->wNumAxes,c->wMaxButtons};
    game_memory_write_u16(m,0x604d70u,c->wMid);game_memory_write_u16(m,0x604d72u,c->wPid);
    game_memory_write(m,0x604d74u,c->szPname,32u);
    for(i=0;i<19u;++i)game_memory_write_u32(m,0x604d94u+i*4u,words[i]);
    game_memory_write(m,0x604de0u,c->szRegKey,32u);
    game_memory_write(m,0x604e00u,c->szOEMVxD,260u);
}
static int property_optional(HRESULT status,int missing_allowed) {
    return status==DI_OK || status==E_NOTIMPL || (missing_allowed && status==DIERR_NOTFOUND);
}
static HRESULT joystick_dword(LPDIRECTINPUTDEVICE2A device,REFGUID property,DWORD value) {
    DIPROPDWORD p={{sizeof(p),sizeof(p.diph),0,DIPH_DEVICE},value};
    return IDirectInputDevice2_SetProperty(device,property,&p.diph);
}
static HRESULT joystick_range(LPDIRECTINPUTDEVICE2A device,DWORD offset,LONG min,LONG max) {
    DIPROPRANGE p={{sizeof(p),sizeof(p.diph),offset,DIPH_BYOFFSET},min,max};
    return IDirectInputDevice2_SetProperty(device,DIPROP_RANGE,&p.diph);
}
static int joystick_read(LPDIRECTINPUTDEVICE2A device,GameJoystickSample *output) {
    DIJOYSTATE2 state;HRESULT status;
    int32_t positions[8],velocity[8],acceleration[8],force[8];
    status=IDirectInputDevice2_GetDeviceState(device,sizeof(state),&state);
    if(status!=DI_OK)return (int32_t)status;
    positions[0]=state.lX;positions[1]=state.lY;positions[2]=state.lZ;
    positions[3]=state.lRx;positions[4]=state.lRy;positions[5]=state.lRz;
    positions[6]=state.rglSlider[0];positions[7]=state.rglSlider[1];
    velocity[0]=state.lVX;velocity[1]=state.lVY;velocity[2]=state.lVZ;
    velocity[3]=state.lVRx;velocity[4]=state.lVRy;velocity[5]=state.lVRz;
    velocity[6]=state.rglVSlider[0];velocity[7]=state.rglVSlider[1];
    acceleration[0]=state.lAX;acceleration[1]=state.lAY;acceleration[2]=state.lAZ;
    acceleration[3]=state.lARx;acceleration[4]=state.lARy;acceleration[5]=state.lARz;
    acceleration[6]=state.rglASlider[0];acceleration[7]=state.rglASlider[1];
    force[0]=state.lFX;force[1]=state.lFY;force[2]=state.lFZ;
    force[3]=state.lFRx;force[4]=state.lFRy;force[5]=state.lFRz;
    force[6]=state.rglFSlider[0];force[7]=state.rglFSlider[1];
    memcpy(output->position,positions,sizeof(positions));memcpy(output->velocity,velocity,sizeof(velocity));
    memcpy(output->acceleration,acceleration,sizeof(acceleration));memcpy(output->force,force,sizeof(force));
    memcpy(output->pov,state.rgdwPOV,sizeof(output->pov));memcpy(output->buttons,state.rgbButtons,sizeof(output->buttons));
    return (int32_t)status;
}
static int joystick_sample(LPDIRECTINPUTDEVICE2A device,GameJoystickSample *output){
    HRESULT status=IDirectInputDevice2_Poll(device);
    return status==DI_OK?joystick_read(device,output):(int32_t)status;
}
static void publish_sample(GameMemory *m,GameAddress address,const GameJoystickSample *s) {
    unsigned i;
    for(i=0;i<8u;++i)game_memory_write_u32(m,address+i*4u,(uint32_t)s->position[i]);
    for(i=0;i<4u;++i)game_memory_write_u32(m,address+32u+i*4u,s->pov[i]);
    game_memory_write(m,address+48u,s->buttons,sizeof(s->buttons));
    for(i=0;i<8u;++i)game_memory_write_u32(m,address+176u+i*4u,(uint32_t)s->velocity[i]);
    for(i=0;i<8u;++i)game_memory_write_u32(m,address+208u+i*4u,(uint32_t)s->acceleration[i]);
    for(i=0;i<8u;++i)game_memory_write_u32(m,address+240u+i*4u,(uint32_t)s->force[i]);
}
/* 004cf900: try attached devices in order. Release every rejected device.
 * System joystick caps preserve the original first/second-device probe. */
static BOOL CALLBACK joystick_candidate(const DIDEVICEINSTANCEA *instance,void *context) {
    GameInputDevices *d=context;GameMemory *m=&d->application->memory;
    LPDIRECTINPUTDEVICEA base=0;LPDIRECTINPUTDEVICE2A device=0;
    DIDEVCAPS caps={0};JOYCAPSA multimedia={0};GameJoystickSample sample;
    unsigned i;uint32_t capability;static const DWORD offsets[]={0,4,8,12,16,20,24,28};
    if(IDirectInput_CreateDevice(d->factory,&instance->guidInstance,&base,0)!=DI_OK)return DIENUM_CONTINUE;
    if(IDirectInputDevice_QueryInterface(base,&IID_IDirectInputDevice2A,(void **)&device)!=DI_OK)goto reject;
    IDirectInputDevice_Release(base);base=0;
    if(IDirectInputDevice2_SetDataFormat(device,&c_dfDIJoystick2)!=DI_OK ||
        IDirectInputDevice2_SetCooperativeLevel(device,d->window,DISCL_EXCLUSIVE|DISCL_FOREGROUND)!=DI_OK)goto reject;
    caps.dwSize=sizeof(caps);
    if(IDirectInputDevice2_GetCapabilities(device,&caps)!=DI_OK)goto reject;
    if(joyGetDevCapsA(0,&multimedia,sizeof(multimedia))!=JOYERR_NOERROR &&
        joyGetDevCapsA(1,&multimedia,sizeof(multimedia))!=JOYERR_NOERROR)goto reject;
    capability_words(m,&multimedia);capability=multimedia.wCaps;
    if(!property_optional(joystick_dword(device,DIPROP_AUTOCENTER,1),1) ||
        !property_optional(joystick_dword(device,DIPROP_AXISMODE,0),1))goto reject;
    game_memory_write_u32(m,0x6044d0u,1000u);game_memory_write_u32(m,0x6044d4u,1000u);
    if(!property_optional(joystick_dword(device,DIPROP_DEADZONE,0),1))goto reject;
    for(i=0;i<8u;++i) {
        int optional=i==3u || i==4u;LONG range=i<6u?10000:i==6u?512:256;
        if(i==2u && !(capability&JOYCAPS_HASZ))continue;
        if(i==5u && !(capability&JOYCAPS_HASR))continue;
        if(i>=6u && (capability&(JOYCAPS_HASU|JOYCAPS_HASV))!=(JOYCAPS_HASU|JOYCAPS_HASV))continue;
        if(!property_optional(joystick_range(device,offsets[i],-range,range),optional))goto reject;
    }
    if(IDirectInputDevice2_Acquire(device)!=DI_OK || joystick_sample(device,&sample)!=DI_OK)goto reject;
    publish_sample(m,0x604650u,&sample);publish_sample(m,0x604538u,&sample);
    if((capability&12u)==12u) {
        game_memory_write_u32(m,0x604d5cu,(uint32_t)sample.position[6]);
        game_memory_write_u32(m,0x604d60u,(uint32_t)sample.position[7]);
    }
    {const DWORD words[]={44u,caps.dwFlags,caps.dwDevType,caps.dwAxes,caps.dwButtons,caps.dwPOVs,
        caps.dwFFSamplePeriod,caps.dwFFMinTimeResolution,caps.dwFirmwareRevision,caps.dwHardwareRevision,caps.dwFFDriverVersion};
     for(i=0;i<11u;++i)game_memory_write_u32(m,0x604bf0u+i*4u,words[i]);}
    game_memory_write_u32(m,0x604c30u,(caps.dwFlags&0x100u)?1u:0u);
    d->joystick=device;publish(d,3,1);return DIENUM_STOP;
reject:
    if(base)IDirectInputDevice_Release(base);
    if(device){IDirectInputDevice2_Unacquire(device);IDirectInputDevice2_Release(device);}
    return DIENUM_CONTINUE;
}
static BOOL CALLBACK calibration_kind(const DIEFFECTINFOA *info,void *context) {
    *(GUID *)context=info->guid;return DIENUM_STOP;
}
/* 004cff80: first periodic effect, real two-axis Cartesian definition. */
static int calibration_begin(GameInputDevices *d) {
    GUID guid={0};DWORD axes[2]={0,4};LONG direction[2]={0,1};
    DIPERIODIC periodic={10000u,0,0,100000u};DIEFFECT effect={0};HRESULT status;
    status=IDirectInputDevice2_EnumEffects(d->joystick,calibration_kind,&guid,DIEFT_PERIODIC);
    if(status!=DI_OK)return 0;
    effect.dwSize=sizeof(effect);effect.dwFlags=DIEFF_CARTESIAN|DIEFF_OBJECTOFFSETS;
    effect.dwDuration=INFINITE;effect.dwGain=10000u;effect.dwTriggerButton=INFINITE;
    effect.cAxes=2;effect.rgdwAxes=axes;effect.rglDirection=direction;
    effect.cbTypeSpecificParams=sizeof(periodic);effect.lpvTypeSpecificParams=&periodic;
    status=IDirectInputDevice2_CreateEffect(d->joystick,&guid,&effect,&d->calibration,0);
    if(status!=DI_OK)return 0;
    publish(d,4,1);return 1;
}
static HRESULT create_buffered(GameInputDevices *d,int mouse) {
    LPDIRECTINPUTDEVICEA *slot=mouse?&d->mouse:&d->keyboard;
    HANDLE *event=mouse?&d->mouse_event:&d->keyboard_event;
    DIPROPDWORD buffer={{sizeof(buffer),sizeof(buffer.diph),0,DIPH_DEVICE},mouse?32u:64u};HRESULT status;
    status=IDirectInput_CreateDevice(d->factory,mouse?&GUID_SysMouse:&GUID_SysKeyboard,slot,0);
    if(FAILED(status))return status;
    status=IDirectInputDevice_SetDataFormat(*slot,mouse?&c_dfDIMouse:&c_dfDIKeyboard);if(FAILED(status))return status;
    status=IDirectInputDevice_SetCooperativeLevel(*slot,d->window,DISCL_NONEXCLUSIVE|DISCL_FOREGROUND);if(FAILED(status))return status;
    *event=CreateEventA(0,FALSE,FALSE,0);if(!*event)return HRESULT_FROM_WIN32(GetLastError());
    status=IDirectInputDevice_SetEventNotification(*slot,*event);if(FAILED(status))return status;
    status=IDirectInputDevice_SetProperty(*slot,DIPROP_BUFFERSIZE,&buffer.diph);if(FAILED(status))return status;
    if(!game_application_isolated_input(d->application))(void)IDirectInputDevice_Acquire(*slot);
    publish(d,mouse?2u:1u,1);publish(d,mouse?6u:5u,1);
    return DI_OK;
}
int game_input_devices_begin(GameApplication *a) {
    GameInputDevices *d;NativeInputCreate create;FARPROC entry;HRESULT status;GameAddress settings;
    JOYINFOEX probe={0};unsigned i;
    if(a->input_devices)return 0;
    d=calloc(1,sizeof(*d));if(!d)return 0;d->application=a;d->window=game_menu_window_handle(a);
    a->input_devices=d;
    d->library=LoadLibraryExA("dinput.dll",0,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!d->library)goto fail;
    entry=GetProcAddress(d->library,"DirectInputCreateA");if(!entry)goto fail;
    {typedef char same_width[sizeof(create)==sizeof(entry)?1:-1];(void)sizeof(same_width);memcpy(&create,&entry,sizeof(create));}
    status=create(game_application_instance(a),0x500u,&d->factory,0);
    if(FAILED(status))goto fail;
    publish(d,0,1);
    if(FAILED(create_buffered(d,0)) || FAILED(create_buffered(d,1)))goto fail;
    settings=game_memory_read_u32(&a->memory,0x5943c8u);
    if(game_application_isolated_input(a))game_memory_write_u32(&a->memory,0x604768u,0);
    if(!game_application_isolated_input(a) && game_memory_read_u32(&a->memory,settings+0x50u)) {
        game_memory_write_u32(&a->memory,0x604768u,0u);
        if(joyGetNumDevs()) {
            probe.dwSize=sizeof(probe);probe.dwFlags=0x400u;
            if(joyGetPosEx(0,&probe)==JOYERR_NOERROR || joyGetPosEx(1,&probe)==JOYERR_NOERROR) {
                const DWORD words[]={52u,probe.dwFlags,probe.dwXpos,probe.dwYpos,probe.dwZpos,probe.dwRpos,
                    probe.dwUpos,probe.dwVpos,probe.dwButtons,probe.dwButtonNumber,probe.dwPOV,probe.dwReserved1,probe.dwReserved2};
                for(i=0;i<13u;++i)game_memory_write_u32(&a->memory,0x604498u+i*4u,words[i]);
                status=IDirectInput_EnumDevices(d->factory,DIDEVTYPE_JOYSTICK,joystick_candidate,d,DIEDFL_ATTACHEDONLY);
                if(status==DI_OK && d->joystick &&
                    (game_memory_read_u32(&a->memory,0x604c30u) || calibration_begin(d)))
                    game_memory_write_u32(&a->memory,0x604768u,1u);
            }
        }
        game_memory_write_u32(&a->memory,0x594d30u,2u-game_memory_read_u32(&a->memory,settings+0x50u));
    }
    if(a->memory.fault)goto fail;
    return 1;
fail:
    game_input_devices_end(a);game_application_log(a,"native input: %s\n","DirectInput device initialization failed");return 0;
}
int game_menu_input_acquire(GameApplication *a,unsigned which) {
    GameInputDevices *d=a->input_devices;
    if(which>2u)return 1; /* Original selector has no matching device. */
    if(!d)return 1;       /* Legitimate window messages before input creation. */
    if(game_application_isolated_input(a))return 1;
    if(which==0u && d->keyboard)(void)IDirectInputDevice_Acquire(d->keyboard);
    if(which==1u && d->mouse)(void)IDirectInputDevice_Acquire(d->mouse);
    if(which==2u && d->joystick)(void)IDirectInputDevice2_Acquire(d->joystick);
    return 1; /* Original ignores acquisition HRESULT, including focus loss. */
}
int game_menu_input_unacquire(GameApplication *a) {
    GameInputDevices *d=a->input_devices;if(!d)return 1;
    if(d->keyboard)(void)IDirectInputDevice_Unacquire(d->keyboard);
    if(d->mouse)(void)IDirectInputDevice_Unacquire(d->mouse);
    if(d->joystick)(void)IDirectInputDevice2_Unacquire(d->joystick);
    return 1;
}
void game_input_devices_end(GameApplication *a) {
    GameInputDevices *d=a->input_devices;unsigned i;if(!d)return;
    /*SDK release may pump focus messages. Detach first so those callbacks
     *cannot reacquire a device while this owner is being retired. */
    a->input_devices=NULL;
    for(i=0;i<7u;++i)publish(d,i,0);
    if(d->calibration){IDirectInputEffect_Stop(d->calibration);IDirectInputEffect_Release(d->calibration);}
    if(d->keyboard)(void)IDirectInputDevice_Unacquire(d->keyboard);
    if(d->mouse)(void)IDirectInputDevice_Unacquire(d->mouse);
    if(d->joystick)(void)IDirectInputDevice2_Unacquire(d->joystick);
    if(d->joystick)IDirectInputDevice2_Release(d->joystick);
    if(d->keyboard)IDirectInputDevice_Release(d->keyboard);
    if(d->mouse)IDirectInputDevice_Release(d->mouse);
    if(d->keyboard_event)CloseHandle(d->keyboard_event);
    if(d->mouse_event)CloseHandle(d->mouse_event);
    if(d->factory)IDirectInput_Release(d->factory);
    if(d->library)FreeLibrary(d->library);
    free(d);
}
int32_t game_input_device_events(GameApplication *a,unsigned which,GameInputEvent *events,unsigned *count) {
    GameInputDevices *d=a->input_devices;LPDIRECTINPUTDEVICEA device;
    DIDEVICEOBJECTDATA buffer[64];DWORD used;HRESULT status;unsigned i;
    if(!d || which>1u || !count || *count>64u || (*count && !events))return (int32_t)DIERR_INVALIDPARAM;
    if(game_application_isolated_input(a)){*count=0;return DI_OK;}
    device=which?d->mouse:d->keyboard;if(!device)return (int32_t)DIERR_NOTINITIALIZED;
    used=*count;status=IDirectInputDevice_GetDeviceData(device,sizeof(buffer[0]),buffer,&used,0);
    if(FAILED(status)){*count=0;return (int32_t)status;}
    if(used>64u){*count=0;return (int32_t)DIERR_INVALIDPARAM;}
    for(i=0;i<used;++i){events[i].offset=buffer[i].dwOfs;events[i].value=buffer[i].dwData;
        events[i].time=buffer[i].dwTimeStamp;events[i].sequence=buffer[i].dwSequence;
        if(which){
            int panel=game_memory_read_u32(&a->memory,0x604760)==1?
                (game_memory_read_u32(&a->memory,0x584048)?1:game_memory_read_u32(&a->memory,0x589abc)?2:0):0;
            panel_motion_event(d,panel,&events[i]);
        }
    }
    *count=(unsigned)used;return (int32_t)status;
}
int32_t game_input_device_joystick(GameApplication *a,GameJoystickSample *output) {
    GameInputDevices *d=a->input_devices;
    if(!d || !d->joystick)return (int32_t)DIERR_NOTINITIALIZED;
    if(!output)return (int32_t)DIERR_INVALIDPARAM;
    return joystick_sample(d->joystick,output);
}
int32_t game_input_device_joystick_poll(GameApplication *a){
    GameInputDevices *d=a->input_devices;
    return d && d->joystick?(int32_t)IDirectInputDevice2_Poll(d->joystick):(int32_t)DIERR_NOTINITIALIZED;
}
int32_t game_input_device_joystick_read(GameApplication *a,GameJoystickSample *output){
    GameInputDevices *d=a->input_devices;
    if(!d || !d->joystick)return (int32_t)DIERR_NOTINITIALIZED;
    return output?joystick_read(d->joystick,output):(int32_t)DIERR_INVALIDPARAM;
}
/*The calibration/periodic effect already belongs to this native input owner.
 *The game supplies values; SDK pointers remain full-width and synchronous. */
int32_t game_input_device_pulse(GameApplication *a,GameInputPulseAction action,uint32_t magnitude,uint32_t period){
    GameInputDevices *d=a->input_devices;DIPERIODIC periodic={0};DIEFFECT effect={0};
    if(!d || !d->calibration)return (int32_t)DIERR_NOTINITIALIZED;
    if(action==GAME_INPUT_PULSE_START)return (int32_t)IDirectInputEffect_Start(d->calibration,1,0);
    if(action==GAME_INPUT_PULSE_STOP)return (int32_t)IDirectInputEffect_Stop(d->calibration);
    if(action!=GAME_INPUT_PULSE_CONFIGURE)return (int32_t)DIERR_INVALIDPARAM;
    periodic.dwMagnitude=magnitude;periodic.dwPeriod=period;effect.dwSize=sizeof(effect);
    effect.cbTypeSpecificParams=sizeof(periodic);effect.lpvTypeSpecificParams=&periodic;
    return (int32_t)IDirectInputEffect_SetParameters(d->calibration,&effect,DIEP_TYPESPECIFICPARAMS);
}
