#ifndef UPRISING_LEVEL_PLAYER_ROSTER_H
#define UPRISING_LEVEL_PLAYER_ROSTER_H

#include <stdint.h>

enum { LEVEL_PLAYER_ROSTER_SLOTS = 6 };

typedef struct LevelPlayerRosterSlot {
    uint32_t player;
    uint32_t active_flags;
    uint32_t next;
} LevelPlayerRosterSlot;

typedef struct LevelPlayerRosterStats {
    uint32_t activations, releases, live, linked, generation;
    uint32_t external_cursors;
    uint32_t native_ticks, external_ticks;
} LevelPlayerRosterStats;

/* The roster owns the active player order for one loaded level. Player and
 * scene values are opaque handles until their constructors are migrated. */
int level_player_roster_begin(const uint32_t players[LEVEL_PLAYER_ROSTER_SLOTS],
    const uint32_t active_flags[LEVEL_PLAYER_ROSTER_SLOTS]);
void level_player_roster_clear(void);
int level_player_roster_slot(unsigned index, LevelPlayerRosterSlot *);
int level_player_roster_next(uint32_t player, uint32_t *next);
int level_player_roster_set_current(uint32_t player);
uint32_t level_player_roster_head(void);
uint32_t level_player_roster_current(void);
int level_player_roster_live(void);
void level_player_roster_note_external_cursor(void);
void level_player_roster_note_tick(int native);
void level_player_roster_stats(LevelPlayerRosterStats *);

#endif
