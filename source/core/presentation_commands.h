#ifndef UPRISING_PRESENTATION_COMMANDS_H
#define UPRISING_PRESENTATION_COMMANDS_H
#include "runtime_node_pool.h"
#include "presentation_lifecycle.h"
#include "presentation_record_state.h"

typedef enum PresentationCommandDisposition {
    PRESENTATION_COMMAND_IGNORE,
    PRESENTATION_COMMAND_REWRITE,
    PRESENTATION_COMMAND_PREPEND
} PresentationCommandDisposition;

typedef struct PresentationCommand {
    uint32_t priority,kind;
    GameAddress target;
    uint32_t target_identity,parameter;
    float position[3];
    uint8_t has_target,has_position,flagged,continue_chain;
} PresentationCommand;

/* Planning precedes decoding borrowed target/point arguments: ignored commands
 * must not touch those inputs. Kind 12 inherits the current command priority. */
PresentationCommandDisposition presentation_command_plan(const RuntimeNode *,
    uint32_t priority,uint32_t kind);
int presentation_command_build(GameMemory *,RuntimeNode *,
    const PresentationCommand *,RuntimeNode **result);
void presentation_command_seed_position(RuntimeNode *,uint32_t identity,const float position[3]);
int presentation_command_replace(GameMemory *,PresentationLifecycle *,const PresentationCommand *);
void presentation_command_publish(PresentationLifecycle *,const RuntimeNode *,
    const float target_position[3],uint32_t tick,GamePresentationRecordResult *);
int presentation_command_path(GameMemory *,PresentationLifecycle *,float (*points)[3],
    uint32_t count,unsigned mode);
int presentation_command_aircraft_path(GameMemory *,PresentationLifecycle *,float (*points)[3],
    uint32_t count);
#endif
