#include "application_entry.h"
#include <string.h>
#include <stdio.h>

static int application_failure(GameApplication *app, const char *stage) {
    fprintf(stderr,"native application failed: %s; memory fault=%d at %08lx size=%lu\n",
        stage, app->memory.fault, (unsigned long)app->memory.fault_address,
        (unsigned long)app->memory.fault_size);
    fflush(stderr);
    return 1;
}

static int same_option(const char *a, const char *b) {
    for (;;) {
        unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return 0;
        if (!x) return 1;
    }
}

/* 005057f0: real process root. The
 * instruction reference is preserved; CRT startup/SEH are supplied by the host. */
int game_application_main(GameApplication *app, int argc, char **argv) {
    GameMemory *m = &app->memory;
    char level[64] = "";
    int32_t calendar[9];
    int index;
    if (!game_application_services_initialize(app, 1000u, 2000u))
        return application_failure(app,"shared services");
    game_memory_write_u32(m, 0x597dd4u, 0u);
    game_memory_write_u32(m, 0x597dd0u, 1u);
    for (index = 1; index < argc; ++index) {
        const char *arg = argv[index];
        size_t length = strlen(arg);
        if (length >= 4u && same_option(arg + length - 4u, ".lfl")) {
            /* Original destination is 64 bytes; never reproduce its overflow. */
            if (length >= sizeof(level)) return 1;
            memcpy(level, arg, length + 1u);
        } else if (same_option(arg, "-generatebinary")) {
            game_memory_write_u32(m, 0x597dd4u, 1u);
            game_memory_write_u32(m, 0x597dd0u, 0u);
        } else if (same_option(arg, "-dontusebinary"))
            game_memory_write_u32(m, 0x597dd0u, 0u);
        else if (same_option(arg, "-dontshowintro"))
            game_memory_write_u32(m, 0x58abe4u, 0u);
        else if (strstr(arg, "-testscript"))
            game_memory_write_u32(m, 0x59708cu, 1u);
        else game_application_log(app, "unknown command: \"%s\"\n", arg);
    }
    game_memory_write_u32(m, 0x624f8cu, 0u);
    if (!game_application_directory(app, "scrshts"))
        return game_application_error(app, "couldn't create directory", "slick");
    if (!game_application_options_initialize(app)) return application_failure(app,"options");
    if (!game_application_calendar(app, calendar)) return application_failure(app,"calendar");
    if (!game_application_menu_loop(app,level)) return application_failure(app,"menu/level loop");
    game_application_services_release(app);
    return m->fault ? 1 : 0;
}
