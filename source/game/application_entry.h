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

/* Unreconstructed connected services are deliberately unresolved at link time.
 * These are native contracts, never guest-ABI calls or successful stubs. */
/* Full-width platform module owner; windowed validation verifies its local
 * wrapper configuration before any graphics entry point can run. */
int game_application_platform_start(GameApplication *, void *instance,
    const char *command_line, int show);
int game_application_platform_stop(GameApplication *);
int game_application_services_initialize(GameApplication *, unsigned, unsigned);
int game_application_force_feedback_open(GameApplication *);
int game_application_force_feedback_detect(GameApplication *);
int game_application_force_feedback_poll(GameApplication *, uint32_t sample[4]);
typedef enum GameForceFeedbackOperation {
    GAME_FORCE_JOLT=10,GAME_FORCE_REFLEX_JOLT,GAME_FORCE_REFLEX_CLEAR,
    GAME_FORCE_X_VIBRATION,GAME_FORCE_X_CLEAR,GAME_FORCE_Y_VIBRATION,GAME_FORCE_Y_CLEAR,
    GAME_FORCE_BUFFETING,GAME_FORCE_BUFFETING_CLEAR,GAME_FORCE_VECTOR,GAME_FORCE_VECTOR_CLEAR,
    GAME_FORCE_XY_VECTOR,GAME_FORCE_X_SPRING=25,GAME_FORCE_X_SPRING_CLEAR,
    GAME_FORCE_Y_SPRING,GAME_FORCE_Y_SPRING_CLEAR
} GameForceFeedbackOperation;
uint32_t game_application_force_feedback_effect(GameApplication *,GameForceFeedbackOperation,
    uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void game_application_force_feedback_close(GameApplication *, int finish);
int game_application_options_initialize(GameApplication *);
int game_application_menu_loop(GameApplication *, void *instance,
    const char *level);
void game_application_services_release(GameApplication *);
void game_application_services_dispose(GameApplication *);
void *game_application_instance(GameApplication *);
const char *game_application_command_line(GameApplication *);
void game_application_sleep(GameApplication *, unsigned milliseconds);
int game_application_directory(GameApplication *, const char *);
int game_application_output_directory(GameApplication *,const char *);
int game_application_error(GameApplication *, const char *, const char *);
int game_application_timed_message_bytes(GameApplication *,const uint8_t *,size_t,int32_t seconds);
void game_application_log(GameApplication *, const char *, const char *);
int game_application_calendar(GameApplication *, int32_t calendar[9]);
void game_application_registry_prepare(GameApplication *);
int game_application_startup(GameApplication *, int show);
int game_application_initialize_globals(GameApplication *);
int game_application_shutdown_globals(GameApplication *);
#endif
