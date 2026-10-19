#ifndef UPRISING_PORTABLE_SDL_H
#define UPRISING_PORTABLE_SDL_H
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include "application_entry.h"

/* Platform services borrow SDL's native window/context owners. */
SDL_Window *game_menu_sdl_window(GameApplication *application);
SDL_GLContext game_menu_gl_context(GameApplication *application);
int game_menu_game_palette(GameApplication *,const uint32_t[256]);
int game_menu_pump_events(GameApplication *application,int deliver_input);
int game_menu_event_forward(GameApplication *application,const SDL_Event *event);
int game_menu_pointer_from_client(GameApplication *application,int32_t point[2]);
int game_menu_capture_png(GameApplication *,const char *path);
struct VulkanMenu;
struct VulkanMenu *game_menu_vulkan(GameApplication *);
#endif
