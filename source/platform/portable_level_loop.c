#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "level_run.h"
#include "menu_pause.h"
#include "portable_input_devices.h"
extern int game_menu_input_poll(GameApplication *,GameAddress);
extern int game_menu_event_forward(GameApplication *,const MSG *);
extern int game_level_frame(GameApplication *); /*418470, actual world/render actions*/
extern int game_level_force_feedback_update(GameApplication *); /*4d05e0/4fa190*/
extern int game_level_joystick_update(GameApplication *); /*4d0f80*/
extern int game_menu_pointer_capture(GameApplication *,int);

/*417590: the playable loop waits on the input owner's full-width events,
 * drains actual Windows messages, advances the world on timeout and retains
 * the original pause/focus, termination, force-feedback and joystick routes. */
int game_level_game_loop(GameApplication *a) {
    GameMemory *m=&a->memory;MSG message;int waiting,captured=1;
    unsigned frames=0;DWORD sample_start=GetTickCount();
    int observe=GetEnvironmentVariableA("UPRISING_RECOMP_NATIVE_OBSERVE",NULL,0)!=0;
    const char *stage="input wait";
    if(!game_menu_pointer_capture(a,1)){(void)game_menu_pointer_capture(a,0);return 0;}
    while(!m->fault) {
        /* Pause/options keep this loop alive. Confine only active gameplay;
         * the HWND owner remembers the request across focus/minimize events. */
        int capture=!game_memory_read_u32(m,0x5acbe4) && !game_memory_read_u32(m,0x5910a4);
        if(capture!=captured){
            stage="pointer confinement";
            if(!game_menu_pointer_capture(a,capture))goto fail;
            captured=capture;
        }
        /*A graphics backend can pump/dispatch the wakeup message itself.
         *The existing termination/reload owners are authoritative, not the
         *continued presence of WM_USER+7 in the host queue. */
        if(game_memory_read_u32(m,0x5ac970) || game_memory_read_u32(m,0x58360c) ||
           game_memory_read_u32(m,0x59d4c8)) {
            /*Retire this loop's wakeup before a reload resets its end flag. */
            while(PeekMessageA(&message,NULL,WM_USER+7,WM_USER+7,PM_REMOVE)){}
            goto done;
        }
        stage="input wait";
        waiting=game_input_device_wait(a,0);
        if(waiting<0)goto fail;
        if(waiting<2) {
            stage="input poll";if(!game_menu_input_poll(a,game_memory_read_u32(m,0x627904)))goto fail;
        }else if(waiting==2) {
            while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)) {
                if(message.message==WM_QUIT)game_memory_write_u32(m,0x59d4c8,1);
                if(message.message==WM_QUIT || message.message==WM_USER+7)goto done;
                stage="event forwarding";if(game_memory_read_u32(m,0x5910a4) && !game_menu_event_forward(a,&message))goto fail;
                TranslateMessage(&message);DispatchMessageA(&message);
            }
        }else {
            /*A background peer still advances the shared clock/world. Its
             *physical input devices remain unacquired until focus returns. */
            if((game_memory_read_u32(m,0x6270f4) || game_memory_read_u32(m,0x5eff04)) &&
               !game_memory_read_u32(m,0x627070)) {
                /*The supported callback is a stable ID selecting real C. An
                 * unconverted callback stays an explicit failure, never CPU. */
                stage="frame callback";if(game_memory_read_u32(m,0x583b10)!=0x418470)goto fail;
                if(game_memory_read_u32(m,0x5acbe4))game_level_pause_mark(a);
                stage="gameplay frame";if(!game_level_frame(a))goto fail;
                ++frames;
                if(frames==1)game_application_log(a,"%s\n","native gameplay frame presented");
                if(observe && (frames==1 || !(frames&255u))){
                    DWORD elapsed=GetTickCount()-sample_start;
                    fprintf(stderr,"native gameplay presented: frames=%u tick=%u elapsed_ms=%lu fps=%.2f\n",
                        frames,game_memory_read_u32(m,0x5ac6ac),(unsigned long)elapsed,elapsed?frames*1000.0/elapsed:0.0);
                    fflush(stderr);
                }
            }else WaitMessage();
        }
        stage="force feedback";if(game_memory_read_u32(m,0x624f68) && !game_level_force_feedback_update(a))goto fail;
        stage="joystick";if(game_memory_read_u32(m,0x604768) && !game_level_joystick_update(a))goto fail;
    }
fail:
    game_application_log(a,"native gameplay loop failed: %s\n",stage);
    (void)game_menu_pointer_capture(a,0);
    return 0;
done:
    return game_menu_pointer_capture(a,0) && !m->fault;
}
