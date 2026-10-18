#include "level_session.h"
#include <string.h>

int level_session_load(LevelSession *session, const char *path,
    const LevelManifest *manifest,
    const LevelSessionLoader *loader) {
    LevelSession next;
    unsigned stage;
    if (!session || !path || !*path || strlen(path) >= sizeof(session->path) ||
        !manifest || !loader || !loader->run_stage || !loader->rollback ||
        session->loading ||
        session->active || session->completed_stages ||
        session->rollback_pending) return 0;
    session->loading = 1;
    next = *session;
    next.manifest = *manifest;
    strcpy(next.path, path);
    next.completed_stages = 0;
    next.loading = 1;
    for (stage = 0; stage < LEVEL_STAGE_COUNT; ++stage) {
        if (!loader->run_stage(loader->context, (LevelLoadStage)stage,
                manifest)) {
            int recovered = loader->rollback(loader->context,
                (LevelLoadStage)stage, next.completed_stages);
            if (recovered) {
                session->completed_stages = 0;
                session->path[0] = '\0';
                session->rollback_pending = 0;
            } else {
                session->manifest = next.manifest;
                strcpy(session->path, next.path);
                session->completed_stages = next.completed_stages |
                    (1u << stage);
                session->rollback_stage = (LevelLoadStage)stage;
                session->rollback_pending = 1;
            }
            session->active = 0;
            session->loading = 0;
            return 0;
        }
        next.completed_stages |= 1u << stage;
    }
    next.generation++;
    next.active = 1;
    next.loading = 0;
    *session = next;
    return 1;
}

int level_session_retry_rollback(LevelSession *session,
    const LevelSessionLoader *loader) {
    if (!session || !loader || !loader->rollback || session->loading ||
        session->active) return 0;
    if (!session->rollback_pending) return 1;
    if (!loader->rollback(loader->context, session->rollback_stage,
            session->completed_stages & ~(1u << session->rollback_stage)))
        return 0;
    session->rollback_pending = 0;
    session->completed_stages = 0;
    session->path[0] = '\0';
    return 1;
}

void level_session_unload(LevelSession *session) {
    if (!session) return;
    /* Keep a failed rollback locked until its external objects are destroyed. */
    if (!session->active && session->rollback_pending) return;
    session->active = 0;
    session->loading = 0;
    session->completed_stages = 0;
    session->rollback_pending = 0;
    session->path[0] = '\0';
    session->release_mode = session->release_cursor = 0u;
}

int level_session_release_owned(LevelSession *session, LevelReleaseMode mode,
    LevelReleaseAction through, void *context,
    int (*release)(void *, LevelReleaseAction)) {
    unsigned action;
    if (!session || !release || through >= LEVEL_RELEASE_ACTION_COUNT ||
        (mode != LEVEL_RELEASE_NORMAL && mode != LEVEL_RELEASE_ROLLBACK) ||
        (session->release_mode && session->release_mode != (uint8_t)mode))
        return 0;
    if (!session->release_mode) session->release_mode = (uint8_t)mode;
    for (action = session->release_cursor;
            action <= (unsigned)through; ++action) {
        if (mode == LEVEL_RELEASE_NORMAL &&
            action == LEVEL_RELEASE_ORPHANS) {
            session->release_cursor = (uint8_t)(action + 1u);
            continue;
        }
        if (!release(context, (LevelReleaseAction)action)) return 0;
        session->release_cursor = (uint8_t)(action + 1u);
    }
    if (session->release_cursor == LEVEL_RELEASE_ACTION_COUNT)
        session->release_mode = session->release_cursor = 0u;
    return 1;
}
