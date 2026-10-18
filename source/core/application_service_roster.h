#ifndef APPLICATION_SERVICE_ROSTER_H
#define APPLICATION_SERVICE_ROSTER_H

#include <stdint.h>

/* The roster owns its storage. Handles still refer to services owned by the
   application until those services have native implementations. */
typedef uint32_t (*ApplicationServiceNext)(void *context, uint32_t handle);
typedef int (*ApplicationServiceVisit)(void *context, uint32_t handle);

typedef struct ApplicationServiceRosterStats {
    uint32_t builds;
    uint32_t releases;
    uint32_t peak_count;
    uint32_t live_count;
    uint32_t tick_visits;
    uint32_t destroyed;
} ApplicationServiceRosterStats;

int application_service_roster_build(uint32_t first, uint32_t second,
    ApplicationServiceNext next, void *context);
int application_service_roster_tick(ApplicationServiceVisit visit, void *context);
int application_service_roster_release(ApplicationServiceVisit destroy,
    void *context);
void application_service_roster_reset(void);
uint32_t application_service_roster_count(void);
void application_service_roster_stats(ApplicationServiceRosterStats *stats);

#endif
