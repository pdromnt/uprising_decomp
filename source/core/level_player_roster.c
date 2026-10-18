#include "level_player_roster.h"

#include <string.h>

typedef struct LevelPlayerRoster {
    LevelPlayerRosterSlot slots[LEVEL_PLAYER_ROSTER_SLOTS];
    uint32_t head;
    uint32_t current;
    LevelPlayerRosterStats stats;
} LevelPlayerRoster;

static LevelPlayerRoster roster;

int level_player_roster_begin(const uint32_t players[LEVEL_PLAYER_ROSTER_SLOTS],
    const uint32_t active_flags[LEVEL_PLAYER_ROSTER_SLOTS]) {
    unsigned i;
    if (!players || !active_flags || roster.stats.live) return 0;
    roster.head = roster.current = 0;
    roster.stats.linked = 0;
    for (i = 0; i < LEVEL_PLAYER_ROSTER_SLOTS; ++i) {
        LevelPlayerRosterSlot *slot = &roster.slots[i];
        slot->player = players[i];
        slot->active_flags = active_flags[i];
        slot->next = 0;
        if (!slot->player || !slot->active_flags) continue;
        slot->next = roster.head;
        roster.head = slot->player;
        roster.stats.linked++;
    }
    roster.stats.activations++;
    roster.stats.generation++;
    roster.stats.live = 1;
    return 1;
}

void level_player_roster_clear(void) {
    if (!roster.stats.live) return;
    memset(roster.slots, 0, sizeof(roster.slots));
    roster.head = roster.current = 0;
    roster.stats.live = roster.stats.linked = 0;
    roster.stats.releases++;
}

int level_player_roster_slot(unsigned index, LevelPlayerRosterSlot *output) {
    if (!output || !roster.stats.live || index >= LEVEL_PLAYER_ROSTER_SLOTS)
        return 0;
    *output = roster.slots[index];
    return 1;
}

int level_player_roster_next(uint32_t player, uint32_t *next) {
    unsigned i;
    if (!next || !player || !roster.stats.live) return 0;
    for (i = 0; i < LEVEL_PLAYER_ROSTER_SLOTS; ++i)
        if (roster.slots[i].player == player && roster.slots[i].active_flags) {
            *next = roster.slots[i].next;
            return 1;
        }
    return 0;
}

int level_player_roster_set_current(uint32_t player) {
    uint32_t unused;
    if (!roster.stats.live) return 0;
    if (player && !level_player_roster_next(player, &unused)) return 0;
    roster.current = player;
    return 1;
}

uint32_t level_player_roster_head(void) { return roster.head; }
uint32_t level_player_roster_current(void) { return roster.current; }
int level_player_roster_live(void) { return !!roster.stats.live; }
void level_player_roster_note_external_cursor(void) {
    roster.stats.external_cursors++;
}
void level_player_roster_note_tick(int native) {
    if (native) roster.stats.native_ticks++;
    else roster.stats.external_ticks++;
}
void level_player_roster_stats(LevelPlayerRosterStats *output) {
    if (output) *output = roster.stats;
}
