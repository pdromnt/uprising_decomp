#include "application_service_roster.h"

#include <stdlib.h>

typedef struct ApplicationServiceRoster {
    uint32_t *handles;
    uint32_t count;
    uint32_t capacity;
    uint32_t release_cursor;
} ApplicationServiceRoster;

static ApplicationServiceRoster roster;
static ApplicationServiceRosterStats statistics;

void application_service_roster_reset(void) {
    free(roster.handles);
    roster.handles = NULL;
    roster.count = 0u;
    roster.capacity = 0u;
    roster.release_cursor = 0u;
}

uint32_t application_service_roster_count(void) {
    return roster.count;
}

void application_service_roster_stats(ApplicationServiceRosterStats *stats) {
    uint32_t i;
    if (!stats) return;
    *stats = statistics;
    stats->live_count = 0u;
    for (i = 0u; i < roster.count; ++i)
        if (roster.handles[i]) ++stats->live_count;
}

static int append_list(uint32_t head, ApplicationServiceNext next,
        void *context) {
    uint32_t handle = head;
    while (handle != 0u) {
        if (roster.count == roster.capacity) {
            uint32_t capacity = roster.capacity ? roster.capacity * 2u : 16u;
            uint32_t *handles;
            if (capacity <= roster.capacity || capacity > 65536u)
                return 0;
            handles = (uint32_t *)realloc(roster.handles,
                (size_t)capacity * sizeof(*handles));
            if (!handles) return 0;
            roster.handles = handles;
            roster.capacity = capacity;
        }
        roster.handles[roster.count++] = handle;
        handle = next(context, handle);
    }
    return 1;
}

int application_service_roster_build(uint32_t first, uint32_t second,
        ApplicationServiceNext next, void *context) {
    if (roster.count) return 0;
    application_service_roster_reset();
    if (!next || !append_list(first, next, context) ||
            !append_list(second, next, context)) {
        application_service_roster_reset();
        return 0;
    }
    ++statistics.builds;
    if (roster.count > statistics.peak_count)
        statistics.peak_count = roster.count;
    return 1;
}

int application_service_roster_tick(ApplicationServiceVisit visit,
        void *context) {
    uint32_t i;
    if (!visit || roster.release_cursor) return 0;
    for (i = 0; i < roster.count; ++i) {
        if (roster.handles[i]) {
            if (!visit(context, roster.handles[i])) return 0;
            ++statistics.tick_visits;
        }
    }
    return 1;
}

int application_service_roster_release(ApplicationServiceVisit destroy,
        void *context) {
    uint32_t i;
    if (!destroy) return 0;
    for (i = roster.release_cursor; i < roster.count; ++i) {
        if (roster.handles[i]) {
            if (!destroy(context, roster.handles[i])) return 0;
            ++statistics.destroyed;
            roster.handles[i] = 0u;
        }
        roster.release_cursor = i + 1u;
    }
    ++statistics.releases;
    application_service_roster_reset();
    return 1;
}
