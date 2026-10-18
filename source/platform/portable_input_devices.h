#ifndef UPRISING_PORTABLE_INPUT_DEVICES_H
#define UPRISING_PORTABLE_INPUT_DEVICES_H
#include "application_entry.h"
typedef struct GameInputEvent {uint32_t offset,value,time,sequence;} GameInputEvent;
/* Guarded automation owns input; ordinary interactive play retains host devices. */
int game_application_isolated_input(GameApplication *);
typedef struct GameJoystickSample {
    int32_t position[8];uint32_t pov[4];uint8_t buttons[128];
    int32_t velocity[8],acceleration[8],force[8];
} GameJoystickSample;
int game_input_devices_begin(GameApplication *);
void game_input_devices_end(GameApplication *);
int game_menu_input_acquire(GameApplication *,unsigned);
int game_menu_input_unacquire(GameApplication *);
/* Native reads return HRESULT as signed32. Counts include buffered overflow
 * samples; callers retain the actual original lost-device/reacquire policy. */
int32_t game_input_device_events(GameApplication *,unsigned,GameInputEvent *,unsigned *count);
int32_t game_input_device_joystick(GameApplication *,GameJoystickSample *);
int32_t game_input_device_joystick_poll(GameApplication *);
int32_t game_input_device_joystick_read(GameApplication *,GameJoystickSample *);
typedef enum GameInputPulseAction {GAME_INPUT_PULSE_CONFIGURE,GAME_INPUT_PULSE_START,GAME_INPUT_PULSE_STOP} GameInputPulseAction;
int32_t game_input_device_pulse(GameApplication *,GameInputPulseAction,uint32_t magnitude,uint32_t period);
/*Wait on native event HANDLEs owned by the input service plus host messages.
 * Result0/1 selects input,2 messages,3 timeout; -1 is a host failure. */
int game_input_device_wait(GameApplication *,uint32_t milliseconds);
#endif
