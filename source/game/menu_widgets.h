#ifndef UPRISING_PORTABLE_MENU_WIDGETS_H
#define UPRISING_PORTABLE_MENU_WIDGETS_H
#include "application_entry.h"
#include "asset_bitmap.h"
#include "ui_tiled_image.h"
#include "ui_geometry.h"

typedef struct GameMenuTextStyle {
    uint32_t red,green,blue,font,width,height,x,y,fill;
} GameMenuTextStyle;
GameAddress game_menu_text_image_create(GameApplication *,GameAddress,const GameMenuTextStyle *);
GameAddress game_menu_text_image_create_bytes(GameApplication *,const char *,const GameMenuTextStyle *);
int game_menu_text_image_set(GameApplication *,GameAddress,GameAddress,uint32_t layout);
int game_menu_text_image_set_bytes(GameApplication *,GameAddress,const char *,uint32_t);
int game_menu_text_image_release(GameApplication *,GameAddress);
int game_menu_text_image_draw(GameApplication *,GameAddress,const GameUiRect *);
int game_menu_text_measure(GameApplication *,unsigned,const char *,int32_t *,int32_t *);
int game_menu_text_surface_create(GameApplication *,GameAddress,unsigned,unsigned,uint32_t);
int game_menu_text_surface_fill(GameApplication *,GameAddress,uint32_t);
int game_menu_text_surface_paint(GameApplication *,GameAddress,unsigned,const char *,uint32_t,int32_t,int32_t,uint32_t,int);
int game_menu_text_surface_draw(GameApplication *,GameAddress,const GameUiRect *,const GameUiRect *,int);
int game_menu_text_surface_key(GameApplication *,GameAddress,uint32_t);
int game_menu_text_surface_release(GameApplication *,GameAddress);
int game_menu_text_surface_restore(GameApplication *,GameAddress);
int game_menu_text_surface_white(GameApplication *,GameAddress);
int game_menu_frame_pixels_begin(GameApplication *,GameMenuPixelSpan *);
int game_menu_frame_pixels_end(GameApplication *);
int game_menu_surface_dimensions(GameApplication *,GameAddress,GameMenuPixelSpan *);
int game_menu_surface_pixels_begin(GameApplication *,GameAddress,GameMenuPixelSpan *);
int game_menu_surface_pixels_end(GameApplication *,GameAddress);
GameAddress game_menu_selector_create(GameApplication *,uint32_t,uint32_t,uint32_t,GameAddress);
int game_menu_selector_position(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t,int);
int game_menu_selector_resize(GameApplication *,GameAddress,uint32_t,uint32_t);
int game_menu_selector_append(GameApplication *,GameAddress,GameAddress,int,uint32_t);
int game_menu_selector_clear(GameApplication *,GameAddress);
int game_menu_selector_select(GameApplication *,GameAddress,GameAddress);
int game_menu_selector_release(GameApplication *,GameAddress);
int game_menu_selector_scroll_geometry(GameApplication *,GameAddress);
int game_menu_selector_reveal(GameApplication *,GameAddress,GameAddress);
int game_menu_selector_hover_move(GameApplication *,GameAddress,int32_t,int32_t);
int game_menu_selector_pointer(GameApplication *,GameAddress,uint32_t,int32_t,int32_t);
int game_menu_selector_hover_exit(GameApplication *,GameAddress,int32_t,int32_t);
int game_menu_widget_base_state(GameApplication *,GameAddress,uint32_t);
int game_menu_selector_page(GameApplication *,GameAddress,int32_t);
int game_menu_selector_capture_begin(GameApplication *,GameAddress,int32_t,int32_t,int);
int game_menu_selector_capture_move(GameApplication *,GameAddress,int32_t,int32_t,int);
int game_menu_selector_capture_end(GameApplication *,GameAddress);
int game_menu_selector_capture_tick(GameApplication *,GameAddress);
int game_menu_selector_replace(GameApplication *,GameAddress,GameAddress,GameAddress,GameAddress *);
int game_menu_binding_remap(GameApplication *,GameAddress,unsigned,uint32_t *);
int game_menu_bindings_build(GameApplication *,GameAddress);
int game_menu_keyboard_state(GameApplication *,GameAddress,uint32_t);
int game_menu_keyboard_option_action(GameApplication *,uint32_t,GameAddress,uint32_t);
int game_menu_focus_key(GameApplication *,GameAddress,uint32_t);
int game_menu_selector_enable(GameApplication *,GameAddress);
int game_menu_selector_draw(GameApplication *,GameAddress,const GameUiRect *);
GameAddress game_menu_toggle_create(GameApplication *,uint32_t,uint32_t,const GameAddress[4],uint32_t,uint32_t,uint32_t);
GameAddress game_menu_checkbox_create(GameApplication *,const GameAddress[4],uint32_t);
GameAddress game_menu_click_region_create(GameApplication *,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
GameAddress game_menu_edit_create(GameApplication *,uint32_t,uint32_t,uint32_t);
GameAddress game_menu_slider_create(GameApplication *,uint32_t,GameAddress,uint32_t,uint32_t,uint32_t,uint32_t,float,uint32_t,uint32_t,uint32_t);
int game_menu_slider_geometry(GameApplication *,GameAddress);
int game_menu_slider_draw(GameApplication *,GameAddress,const GameUiRect *);
int game_menu_slider_input(GameApplication *,GameAddress,uint32_t,int32_t,int32_t);
int game_menu_slider_cancel(GameApplication *);
int game_menu_edit_scroll_request(GameApplication *,GameAddress,uint32_t,int,uint32_t *);
int game_menu_edit_scroll_tick(GameApplication *,GameAddress,int);
GameAddress game_menu_image_create(GameApplication *, uint32_t identity, GameAddress name);
GameAddress game_menu_animation_create(GameApplication *,GameAddress name);
int game_menu_animation_advance(GameApplication *,GameAddress,uint32_t *frame);
int game_menu_image_mode(GameApplication *,GameAddress,uint32_t mode);
int game_menu_image_compose(GameApplication *,GameAddress,GameAddress,int32_t,int32_t,int,int);
int game_menu_image_load(GameApplication *, GameAddress, GameAddress name);
GameAddress game_menu_button_create(GameApplication *, uint32_t identity,
    const GameAddress names[4], uint32_t route, uint32_t recipient,
    uint32_t command, uint32_t callback);
GameAddress game_menu_combo_create(GameApplication *,uint32_t,const GameAddress[4],
    uint32_t,uint32_t,GameAddress,GameAddress,uint32_t,uint32_t,uint32_t);
int game_menu_combo_open(GameApplication *,GameAddress);
int game_menu_combo_close(GameApplication *,GameAddress,GameAddress);
GameAddress game_menu_text_entry_create(GameApplication *,uint32_t,uint32_t,uint32_t,uint32_t,GameAddress);
GameAddress game_menu_text_entry_create_bytes(GameApplication *,uint32_t,uint32_t,uint32_t,uint32_t,const char *);
int game_menu_text_entry_enable(GameApplication *,GameAddress,int);
int game_menu_text_entry_activate(GameApplication *,GameAddress);
int game_menu_text_entry_key(GameApplication *,GameAddress,uint32_t,uint32_t);
int game_menu_text_entry_tick(GameApplication *,GameAddress);
int game_menu_text_entry_set(GameApplication *,GameAddress,GameAddress);
int game_menu_text_entry_copy(GameApplication *,GameAddress,GameAddress,size_t);
int game_menu_text_entry_draw(GameApplication *,GameAddress,const GameUiRect *);
GameAddress game_menu_button_borrowed_create(GameApplication *,uint32_t,const GameAddress[4],uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
int game_menu_button_borrowed_initialize(GameApplication *,GameAddress,uint32_t,const GameAddress[4],uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
GameAddress game_menu_image_surface_create(GameApplication *,uint32_t,uint32_t,uint32_t);
int game_menu_image_surface_initialize(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
GameAddress game_menu_research_preview_create(GameApplication *,uint32_t,uint32_t);
int game_menu_research_preview_destroy(GameApplication *,GameAddress);
int game_menu_research_preview_draw(GameApplication *,GameAddress,const GameUiRect *);
int game_menu_research_preview_pointer(GameApplication *,GameAddress,uint32_t,int32_t,int32_t);
int game_menu_research_preview_tick(GameApplication *,GameAddress);
int game_menu_research_preview_stop(GameApplication *,GameAddress);
int game_menu_research_preview_load(GameApplication *,GameAddress,GameAddress);
int game_menu_preview_camera_transition(GameApplication *,GameAddress,const float[3],uint16_t,uint16_t,uint16_t,uint32_t);
int game_menu_preview_camera_command(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t *);
int game_menu_widget_base_position(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t,int);
GameAddress game_menu_value_create(GameApplication *,uint32_t,GameAddress,GameAddress,uint32_t,uint32_t,uint32_t,uint32_t,int);
int game_menu_value_position(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t,int);
int game_menu_value_enable(GameApplication *,GameAddress,int);
int game_menu_value_hover(GameApplication *,GameAddress,int);
int game_menu_value_release(GameApplication *,GameAddress);
int game_menu_value_draw(GameApplication *,GameAddress,const GameUiRect *);
int game_menu_value_change(GameApplication *,GameAddress,uint32_t,uint32_t *);
int game_menu_widget_position(GameApplication *, GameAddress, uint32_t x, uint32_t y,
    uint32_t z, int relative);
int game_menu_widget_visible(GameApplication *, GameAddress, int visible);
int game_menu_widget_enable(GameApplication *, GameAddress, int enable);
int game_menu_widget_destroy(GameApplication *, GameAddress);
int game_menu_widget_draw(GameApplication *, GameAddress, const GameUiRect *clip);
int game_menu_button_input(GameApplication *, GameAddress, uint32_t message, int32_t x, int32_t y);
int game_menu_toggle_state(GameApplication *,GameAddress,uint32_t);
int game_menu_toggle_group(GameApplication *,GameAddress,uint32_t);
void game_menu_images_dispose(GameApplication *);

/* Native platform services. No record value contains a host surface pointer. */
int game_menu_window_attach(GameApplication *, GameAddress);
int game_menu_window_detach(GameApplication *, GameAddress);
int game_menu_draw_bitmap(GameApplication *, const AssetBitmap *, const GameUiRect *source,
    const GameUiRect *destination, int transparent, uint32_t color_key);
int game_menu_sound(GameApplication *, uint32_t sound);
int game_menu_button_callback(GameApplication *, uint32_t method, GameAddress, uint32_t message, uint32_t argument);
int game_menu_research_unlock(GameApplication *);
int game_menu_resource_feedback(GameApplication *);
int game_menu_complete_research(GameApplication *);
int game_menu_cheat_action(GameApplication *);
int game_menu_quick_parameter_action(GameApplication *,uint32_t,uint32_t);
/*Unsupported IDs are explicit unfinished routes, never foreign execution. */
int game_menu_missing_route(GameApplication *,const char *,uint32_t);
#endif
