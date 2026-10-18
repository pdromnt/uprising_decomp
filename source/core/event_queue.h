#ifndef UPRISING_EVENT_QUEUE_H
#define UPRISING_EVENT_QUEUE_H

#include "game_memory.h"
#include "gameplay_catalog.h"

typedef struct GameEventQueueCallbacks {
    void *context;
    int (*remove)(void *context, GameMemory *memory, GameAddress queue,
        GameAddress node);
    int (*release)(void *context, GameAddress node);
} GameEventQueueCallbacks;

/* Retail event order: signed deadline, then unsigned code/route and payload.
 * Equal records compare true; packed input payload words compare in reverse. */
int game_event_record_compare(GameMemory *,GameAddress,GameAddress,int *);
int game_event_queue_insert(GameMemory *,GameAddress,GameAddress);
int game_event_queue_insert_at(GameMemory *,GameAddress head,GameAddress tail,GameAddress event);
/*47c7f0: transfer a decoded event to its root/controller, or defer it until
 *the matching entity exists. The receiving queue becomes its sole owner. */
int game_root_event_receive(GameMemory *,GameAddress root,GameAddress event);
typedef struct GameEventPacket {
    uint32_t deadline,identity,words[2];
    uint16_t values[8];
    uint8_t code,mask,count,compact;
} GameEventPacket;
/* Existing event pools own constructed packets and their queued lifetime. */
int game_event_packet_publish(GameMemory *,GameAddress queue,const GameEventPacket *,GameAddress *);

typedef struct GameRootEventServices {
    void *context;
    int (*clock_now)(void *, uint32_t *);
    int (*controller_delta)(void *, GameAddress queue, uint32_t tick, uint32_t *changed);
    int (*network_notify)(void *, GameAddress event);
} GameRootEventServices;
/* Selection, controller deltas and reset/heartbeat production (47dde0).
 * Pooled records remain owned by the existing root queue. */
int game_root_events_produce(GameMemory *,const GameRootEventServices *,GameAddress root,uint32_t tick);
int game_root_command_publish(GameMemory *,const GameRootEventServices *,GameAddress root,
    uint8_t code,uint8_t unit,uint8_t selection);
int game_root_signal_publish(GameMemory *,const GameRootEventServices *,GameAddress,uint8_t);
int game_root_count_publish(GameMemory *,const GameRootEventServices *,GameAddress,uint8_t,uint8_t);
int game_root_coordinates_publish(GameMemory *,const GameRootEventServices *,GameAddress,uint8_t,const uint16_t[4]);
int game_root_scene_publish(GameMemory *,const GameRootEventServices *,GameAddress,uint8_t,uint16_t);
int game_root_pair_publish(GameMemory *,const GameRootEventServices *,GameAddress,uint8_t,uint32_t,uint32_t);
int game_root_word_publish(GameMemory *,const GameRootEventServices *,GameAddress,uint8_t,uint32_t);
int game_input_button_publish(GameMemory *,const GameRootEventServices *,GameAddress queue,
    uint8_t code,uint32_t identity);
/* Shared directional methods and keyboard producer operate on the existing
 * controller records. Directions pair 0/1, 2/3, 4/5, 6/7 and overhead 8/9. */
int game_input_direction(GameMemory *,GameAddress queue,unsigned direction,int pressed,uint32_t strength);
typedef struct GameKeyboardServices {
    void *context;
    GameRootEventServices events;
    GameplayUnitServices units;
    int (*feedback)(void *,uint32_t sound,GameAddress message);
    int (*queue_button)(void *,GameAddress queue,uint8_t code);
    int (*palette_changed)(void *);
    int (*queue_float_pair)(void *,GameAddress queue,uint32_t first,uint32_t second);
} GameKeyboardServices;
int game_selector_request(GameMemory *,const GameRootEventServices *,uint8_t code);
int game_panel_press_clear(GameMemory *,GameAddress);
int game_selector_unit_request(GameMemory *,const GameRootEventServices *,const GameplayFeedbackServices *,uint32_t unit);
int game_player_keyboard(GameMemory *,const GameKeyboardServices *,uint32_t key,uint32_t flags);
typedef int (*GameRootEntityResolve)(void *,GameAddress owner,uint32_t identity,GameAddress *result);
/* Handles active-scene reset/camera binding, selection clear/set and out-of-range no-ops. */
int game_root_selection_event(GameMemory *,GameAddress root,GameAddress event,
    GameRootEntityResolve,void *context);
typedef enum GameRootControlAction {
    GAME_ROOT_OPEN_SOUND, GAME_ROOT_LOOP_SOUND, GAME_ROOT_CONFIRM_SOUND,
    GAME_ROOT_STOP_LOOP_SOUND,
    GAME_ROOT_REFRESH_SELECTOR, GAME_ROOT_SET_APPLICATION_MODE,
    GAME_ROOT_SET_DISPLAY_CONTROL
} GameRootControlAction;
typedef struct GameRootControlServices {
    void *context;
    int (*perform)(void *,GameRootControlAction,uint32_t value);
    int (*loop_sound_playing)(void *,uint32_t *playing);
    int (*foreign_queue)(void *,GameAddress queue,int begin);
    int (*foreign_widget_kind)(void *,GameAddress widget,uint32_t *kind);
} GameRootControlServices;
int game_root_selector_open(GameMemory *,const GameRootControlServices *);
int game_root_selector_close(GameMemory *,const GameRootControlServices *);
int game_root_control_event(GameMemory *,GameAddress root,GameAddress event,const GameRootControlServices *);
/* Begin/end share this native reset for base input queues. Unknown classes
 * return 0 so their distinct behavior remains an explicit external boundary. */
int game_input_queue_reset(GameMemory *,GameAddress queue,int begin);
void game_input_queue_clear(GameMemory *,GameAddress queue);

int game_timed_event_queue_expire(GameMemory *memory,
    const GameEventQueueCallbacks *callbacks, GameAddress queue);
/* Drain every queued event during player/runtime teardown. The release
 * callback owns returning each node to its event pool. */
int game_timed_event_queue_clear(GameMemory *memory,
    const GameEventQueueCallbacks *callbacks, GameAddress queue);
int game_event_list_clear_head(GameMemory *memory,
    const GameEventQueueCallbacks *callbacks, GameAddress head_address);

#endif
