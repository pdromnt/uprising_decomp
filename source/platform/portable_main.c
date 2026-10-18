#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include "application_entry.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    GameApplication application = {0};
    int result;
    (void)previous;
    if (!game_application_platform_start(&application, instance, command, show))
        return 1;
    result = game_application_main(&application, __argc, __argv);
    if (!game_application_platform_stop(&application) && !result) result = 1;
    return result;
}
