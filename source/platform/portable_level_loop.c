#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "level_run.h"
#include "menu_pause.h"
#include "portable_input_devices.h"
#include "portable_sdl.h"
extern int game_menu_input_poll(GameApplication *,GameAddress);
extern int game_level_frame(GameApplication *); /*418470, actual world/render actions*/
extern int game_level_joystick_update(GameApplication *); /*4d0f80*/
extern int game_menu_pointer_capture(GameApplication *,int);

/*417590: the playable loop drains SDL input, advances the world and retains
 * the original pause/focus, termination and SDL joystick routes. */
int game_level_game_loop(GameApplication *a) {
    GameMemory *m=&a->memory;int captured=1;
    unsigned frames=0;DWORD sample_start=(uint32_t)SDL_GetTicks();
    int observe=GetEnvironmentVariableA("UPRISING_RECOMP_NATIVE_OBSERVE",NULL,0)!=0;
    const char *stage="pointer confinement";
    if(!game_menu_pointer_capture(a,1)){(void)game_menu_pointer_capture(a,0);return 0;}
    while(!m->fault) {
        stage="SDL events";
        if(!game_menu_pump_events(a,game_memory_read_u32(m,0x5910a4)!=0))goto fail;
        /* Pause/options keep this loop alive. Confine only active gameplay;
         * the HWND owner remembers the request across focus/minimize events. */
        int capture=!game_memory_read_u32(m,0x5acbe4) && !game_memory_read_u32(m,0x5910a4);
        if(capture!=captured){
            stage="pointer confinement";
            if(!game_menu_pointer_capture(a,capture))goto fail;
            captured=capture;
        }
        /* The existing termination/reload owners are authoritative. */
        if(game_memory_read_u32(m,0x5ac970) || game_memory_read_u32(m,0x58360c) ||
           game_memory_read_u32(m,0x59d4c8)) {
            goto done;
        }
        stage="input poll";
        if(!game_menu_input_poll(a,game_memory_read_u32(m,0x627904)))goto fail;
        /* Buffered input and simulation share each iteration; sustained input
         * cannot starve frames as it could with separate HANDLE wakeups. */
        {
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
                    DWORD elapsed=(uint32_t)SDL_GetTicks()-sample_start;
                    fprintf(stderr,"native gameplay presented: frames=%u tick=%u elapsed_ms=%lu fps=%.2f\n",
                        frames,game_memory_read_u32(m,0x5ac6ac),(unsigned long)elapsed,elapsed?frames*1000.0/elapsed:0.0);
                    fflush(stderr);
                }
            }else SDL_Delay(10);
        }
        stage="joystick";if(game_memory_read_u32(m,0x604768) && !game_level_joystick_update(a))goto fail;
    }
fail:
    game_application_log(a,"native gameplay loop failed: %s\n",stage);
    (void)game_menu_pointer_capture(a,0);
    return 0;
done:
    return game_menu_pointer_capture(a,0) && !m->fault;
}
