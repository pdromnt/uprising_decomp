#ifndef UPRISING_APPLICATION_ENTRY_H
#define UPRISING_APPLICATION_ENTRY_H
#include "game_memory.h"
#include "resource_catalog.h"
#include "resource_file.h"
#include "resource_blob.h"
#include "level_session.h"
#include "level_terrain.h"

/* Host handles remain pointers. Record identities remain fixed-width IDs. */
typedef struct GameApplication {
    GameMemory memory;
    void *platform;
    ResourceCatalog *resources;
    ResourceFile *resource_index;
    struct MenuImage *menu_images;
    struct MenuWindow *menu_window;
    struct MenuSplash *menu_splash;
    struct GameInputDevices *input_devices;
    struct GameDisplay *display;
    struct GameMenuMovie *menu_movies;
    struct GameLevelRoutes *level_routes;
    struct GameModelAssets *model_assets;
    uint32_t selection_texture; /*Borrowed ID in model_assets; scene owns retirement. */
    struct GameNetworkTransport *network;
    LevelSession level_session;
    LevelTerrain *terrain;
    struct GameScriptState *scripts;
    GameAddress radar_reciprocals; /*Owned level table;596268 is its borrowed publication. */
    GameAddress camera_records[7]; /*Owned records;58f898..58f8b0 borrow their IDs. */
    GameAddress terrain_depth; /*One owned packed512x512 grid;625660 borrows block IDs. */
    struct GameVehicleFactory *vehicle_factory;
    struct GameResourceClusterOwner *resource_clusters;
    uint32_t level_name_mark;
    int level_names_active;
    uint32_t shutdown_methods[64];
    unsigned shutdown_count;
} GameApplication;

int game_application_main(GameApplication *, int argc, char **argv);
int game_application_load_blob(GameApplication *,const char *,ResourceBlob *);

/* Native application/platform contracts; no guest ABI or module handles. */
int game_application_platform_start(GameApplication *,const char *executable);
int game_application_platform_stop(GameApplication *);
int game_application_services_initialize(GameApplication *, unsigned, unsigned);
int game_application_options_initialize(GameApplication *);
int game_application_menu_loop(GameApplication *,const char *level);
void game_application_services_release(GameApplication *);
void game_application_services_dispose(GameApplication *);
void game_application_sleep(GameApplication *, unsigned milliseconds);
int game_application_directory(GameApplication *, const char *);
int game_application_output_directory(GameApplication *,const char *);
int game_application_error(GameApplication *, const char *, const char *);
int game_application_timed_message_bytes(GameApplication *,const uint8_t *,size_t,int32_t seconds);
void game_application_log(GameApplication *, const char *, const char *);
int game_application_calendar(GameApplication *, int32_t calendar[9]);
int game_application_startup(GameApplication *);
int game_application_initialize_globals(GameApplication *);
int game_application_shutdown_globals(GameApplication *);
#endif
