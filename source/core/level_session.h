#ifndef UPRISING_LEVEL_SESSION_H
#define UPRISING_LEVEL_SESSION_H

#include "level_manifest.h"
#include <stdint.h>

typedef enum LevelLoadStage {
    LEVEL_STAGE_TERRAIN,
    LEVEL_STAGE_PLAYERS,
    LEVEL_STAGE_ENTITIES,
    LEVEL_STAGE_RECORDS,
    LEVEL_STAGE_SCENE,
    LEVEL_STAGE_EFFECTS,
    LEVEL_STAGE_COUNT
} LevelLoadStage;

typedef enum LevelReleaseMode {
    LEVEL_RELEASE_NORMAL = 1,
    LEVEL_RELEASE_ROLLBACK = 2
} LevelReleaseMode;

typedef enum LevelReleaseAction {
    LEVEL_RELEASE_ENTITIES,
    LEVEL_RELEASE_ORPHANS,
    LEVEL_RELEASE_TELEPORTERS,
    LEVEL_RELEASE_SCENE_MEMBERS,
    LEVEL_RELEASE_PLAYER_SCENES,
    LEVEL_RELEASE_NAMES,
    LEVEL_RELEASE_NATIVE_STATE,
    LEVEL_RELEASE_ACTION_COUNT
} LevelReleaseAction;

typedef struct LevelSession {
    LevelManifest manifest;
    char path[512];
    uint32_t generation;
    uint32_t completed_stages;
    LevelLoadStage rollback_stage;
    int rollback_pending;
    int active;
    int loading;
    uint8_t release_mode, release_cursor;
} LevelSession;

typedef struct LevelSessionLoader {
    void *context;
    int (*run_stage)(void *, LevelLoadStage, const LevelManifest *);
    /* Unwind the attempted stage as well as completed stages. Return false if
     * external objects could not be destroyed; that session cannot reload. */
    int (*rollback)(void *, LevelLoadStage, uint32_t);
} LevelSessionLoader;

/* The session owns the load order and publishes an active level only after all
 * stages succeed. External entity constructors remain behind run_stage. */
int level_session_load(LevelSession *, const char *, const LevelManifest *,
    const LevelSessionLoader *);
/* Retry external destruction before a failed load can be replaced. */
int level_session_retry_rollback(LevelSession *, const LevelSessionLoader *);
void level_session_unload(LevelSession *);
/* Drives the dependency order for normal scene destruction and failed-load
 * rollback. Normal shutdown pauses after player scenes while platform services
 * flush, then resumes at names/native state. Failed callbacks retain cursor. */
int level_session_release_owned(LevelSession *, LevelReleaseMode,
    LevelReleaseAction through, void *,
    int (*release)(void *, LevelReleaseAction));

#endif
