#ifndef UPRISING_PLAYER_ATTACHMENT_H
#define UPRISING_PLAYER_ATTACHMENT_H

#include "game_memory.h"

typedef enum PlayerSceneChildKind {
    PLAYER_SCENE_SPATIAL, PLAYER_SCENE_DERIVED_SPATIAL,
    PLAYER_SCENE_COMMAND, PLAYER_SCENE_POSITION, PLAYER_SCENE_MOTION_POSITION
} PlayerSceneChildKind;

typedef struct PlayerAttachmentState {
    PlayerSceneChildKind kind;
    GameAddress parent, motion;
    uint32_t quantity, last_tick, heading_word;
    uint32_t stream_cookie, stream_options, kind_flags;
    float position[3], secondary_position[3];
    float local_orientation[12], reverse_orientation[12];
    uint32_t oriented, mode, alternate, repeating;
    uint32_t repeat_total, repeat_remaining, repeat_delay, last_repeat;
    /* Only the compatibility decoder exposes this dispatch identity. */
    uint32_t projection_method_table;
} PlayerAttachmentState;

typedef struct PlayerAttachmentFrame {
    uint32_t tick, parent_flags, motion_delay;
    uint16_t angles[3];
    float world_matrix[12];
} PlayerAttachmentFrame;

typedef struct PlayerAttachmentEmission {
    PlayerSceneChildKind kind;
    GameAddress parent, motion;
    float position[3];
    uint16_t angles[3];
    float strength;
} PlayerAttachmentEmission;

typedef struct PlayerAttachmentEnvironment {
    uint32_t tick, previous_notice;
    unsigned feedback_enabled, parent_key, linked_world_matches;
    unsigned current_world_is_local;
} PlayerAttachmentEnvironment;

typedef struct PlayerAttachmentServices {
    void *context;
    int (*available)(void *, PlayerSceneChildKind, int *);
    int (*emit)(void *, const PlayerAttachmentEmission *);
    int (*notify)(void *, GameAddress parent, uint16_t heading);
    int (*environment)(void *, GameAddress parent, PlayerSceneChildKind,
        PlayerAttachmentEnvironment *);
    int (*feedback)(void *);
    int (*last_round)(void *, PlayerSceneChildKind, uint32_t tick);
} PlayerAttachmentServices;

typedef struct PlayerAttachmentStats {
    uint32_t updates, submissions, emitted, consumed;
    uint32_t waiting, unavailable, paired, alternating;
} PlayerAttachmentStats;
void player_attachment_stats(PlayerAttachmentStats *);
typedef struct PlayerAttachmentArchiveServices {
    void *context;
    int (*write_word)(void *, uint32_t);
    int (*read_word)(void *, uint32_t *);
} PlayerAttachmentArchiveServices;
int player_attachment_save(const PlayerAttachmentState *,
    const PlayerAttachmentArchiveServices *);
int player_attachment_load(PlayerAttachmentState *,
    const PlayerAttachmentArchiveServices *);

/* Legacy layout is confined to these compatibility views. Native consumers
 * use named state, and legacy writes update that same state until retired. */
int player_attachment_import(GameMemory *, GameAddress, PlayerSceneChildKind,
    PlayerAttachmentState *);
int player_attachment_projection_read(void *, uint32_t, void *, size_t);
int player_attachment_projection_write(void *, uint32_t, const void *, size_t);
int player_attachment_submit(PlayerAttachmentState *, const float position[3],
    const PlayerAttachmentFrame *, const PlayerAttachmentServices *);
/* Retail results: 0 inactive, 1 waiting, 2 unavailable, 3 emitted; -1 failure. */
int player_attachment_update(GameMemory *, PlayerAttachmentState *,
    const PlayerAttachmentFrame *, const PlayerAttachmentServices *);
/* Standalone vehicle attachments retain their existing owned 0xbc record. */
int player_attachment_record_submit(GameMemory *, GameAddress,
    const float position[3], const PlayerAttachmentFrame *,
    const PlayerAttachmentServices *);
int player_attachment_record_update(GameMemory *, GameAddress,
    const PlayerAttachmentFrame *, const PlayerAttachmentServices *);

#endif
