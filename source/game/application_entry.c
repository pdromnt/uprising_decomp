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

/* 005057f0: real process root, including its optional I-Force route. The
 * instruction reference is preserved; CRT startup/SEH are supplied by the host. */
int game_application_main(GameApplication *app, int argc, char **argv) {
    GameMemory *m = &app->memory;
    char level[64] = "";
    int32_t calendar[9];
    int force_feedback = 0, index;
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
        } else if (same_option(arg, "-iforce")) force_feedback = 1;
        else if (same_option(arg, "-generatebinary")) {
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
    if (!game_application_directory(app, "scrshts") &&
        game_memory_read_u32(m, 0x5a1c5cu) == 2u)
        return game_application_error(app, "couldn't create directory", "slick");
    if (force_feedback) {
        uint32_t pair[4]={0,0,0,0};
        int attempt = 0;
        game_application_log(app, "checking for I-FORCE joystick...\n", "");
        if (game_application_force_feedback_open(app)) {
            game_memory_write_u32(m, 0x624f68u, 1u); /* presence, not a DLL pointer */
            if (game_application_force_feedback_detect(app)) {
                while (!game_application_force_feedback_poll(app, pair)) {
                    if (attempt++ > 100) return 0;
                    game_application_sleep(app, 50u);
                }
                game_memory_write_u32(m, 0x624f80u, pair[0]);
                game_memory_write_u32(m, 0x624f84u, pair[1]);
                game_application_log(app, "I-FORCE joystick detected!\n", "");
            } else {
                game_application_force_feedback_close(app, 0);
                game_memory_write_u32(m, 0x624f68u, 0u);
                game_application_log(app, "I-FORCE joystick was not detected!\n", "");
            }
        } else game_application_log(app, "iforce.dll not found!\n", "");
    }
    if (!game_application_options_initialize(app)) return application_failure(app,"options");
    if (!game_application_calendar(app, calendar)) return application_failure(app,"calendar");
    game_application_registry_prepare(app);
    if (!game_application_menu_loop(app, game_application_instance(app),
            level)) return application_failure(app,"menu/level loop");
    if (game_memory_read_u32(m, 0x624f68u))
        game_application_force_feedback_close(app, 1);
    game_application_services_release(app);
    return m->fault ? 1 : 0;
}
