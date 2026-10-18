#include "portable_sdl.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "portable_hooks.h"
#include "portable_input_devices.h"
/* Original application globals have one window thread. Hook ownership remains
 * with that application's platform; bind before installation and clear only
 * after unhooking. No host callback parameter is projected into a 32-bit word. */
static GameApplication *hook_application;
extern void *game_menu_window_handle(GameApplication *);
void game_application_hooks_bind(GameApplication *application) {hook_application=application;}
static uint32_t get(GameMemory *m,GameAddress address) {return game_memory_read_u32(m,address);}
/* 00506210, WH_CBT: literal original switch cases1/3/5/8/9. */
LRESULT CALLBACK game_application_message_hook(int code,WPARAM word,LPARAM parameter) {
    GameApplication *a=hook_application;GameMemory *m;uint32_t mode;HWND window;
    if(!a || code<0 || game_application_isolated_input(a))return CallNextHookEx(0,code,word,parameter);
    m=&a->memory;mode=get(m,0x624f8cu);window=game_menu_window_handle(a);
    switch(code) {
    case HCBT_MINMAX:case HCBT_SYSCOMMAND:
        if(word==SC_KEYMENU && get(m,0x6270f4u))return 1;
        if(mode==8u || mode==10u)return 1;
        break;
    case HCBT_CREATEWND:
        if(!get(m,0x597090u) && get(m,0x597088u)) {
            uint32_t tick=(uint32_t)SDL_GetTicks();
            game_memory_write_u32(m,0x5970a0u,tick);
            game_memory_write_u32(m,0x597090u,(uint32_t)((int32_t)tick%300000));
        }
        if(!get(m,0x597f90u) && (mode==8u || mode==10u) && (HWND)word!=window)return 1;
        break;
    case HCBT_ACTIVATE:case HCBT_SETFOCUS:
        if((mode==8u || mode==10u) && (HWND)word!=window)return 1;
        break;
    default:break;
    }
    return CallNextHookEx(0,code,word,parameter);
}
/* 00506c30, WH_GETMESSAGE. Preserve special/game modes and scalar message
 * filtering. The one-shot startup diagnostic goes through the application's
 * owned isolated log instead of opening a retail-relative writable stream. */
LRESULT CALLBACK game_application_keyboard_hook(int code,WPARAM word,LPARAM parameter) {
    GameApplication *a=hook_application;GameMemory *m;uint32_t mode,diagnostic;
    MSG *message=(MSG *)parameter;
    if(!a)return CallNextHookEx(0,code,word,parameter);
    if(code==HC_ACTION && message && game_application_isolated_input(a) &&
       ((message->message>=WM_KEYFIRST && message->message<=WM_KEYLAST) ||
        (message->message>=WM_MOUSEFIRST && message->message<=WM_MOUSELAST))){
        message->message=WM_NULL;message->wParam=0;message->lParam=0;
        return CallNextHookEx(0,code,word,parameter);
    }
    m=&a->memory;diagnostic=get(m,0x597090u);
    if(diagnostic && diagnostic!=UINT32_MAX) {
        char number[32];snprintf(number,sizeof(number),"%d",(int32_t)diagnostic);
        game_application_log(a,"startup diagnostic: %s\n",number);
        game_memory_write_u32(m,0x597090u,UINT32_MAX);
        game_memory_write_u32(m,0x5970a0u,0u);
    }
    mode=get(m,0x624f8cu);
    if(code==HC_ACTION && message && (get(m,0x5eff04u) || mode==8u || mode==10u) &&
        (message->message==WM_SYSKEYDOWN || message->message==WM_ACTIVATEAPP)) {
        if(message->wParam==VK_F4)exit(244); /* Original immediate exit244. */
        message->message=WM_NULL;message->wParam=0;message->lParam=0;
    }
    return CallNextHookEx(0,code,word,parameter);
}
