#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "application_entry.h"

int main(int argc,char **argv){
    GameApplication application={0};int result;
    if(!game_application_platform_start(&application,argv[0]))return 1;
    result=game_application_main(&application,argc,argv);
    if(!game_application_platform_stop(&application) && !result)result=1;
    return result;
}
