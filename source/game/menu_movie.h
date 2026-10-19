#ifndef UPRISING_MENU_MOVIE_H
#define UPRISING_MENU_MOVIE_H
#include "menu_widgets.h"
typedef struct GameMovieDecoder GameMovieDecoder;
/*Ordinary C Smacker decoding with borrowed frame pixels. Existing platform
 *sound services own PCM playback; no architecture-specific codec is required. */
GameMovieDecoder *game_movie_decoder_open(GameApplication *,const char *);
void game_movie_decoder_close(GameMovieDecoder *);
const AssetBitmap *game_movie_decoder_frame(GameMovieDecoder *);
int game_movie_decoder_ready(GameMovieDecoder *);
int game_movie_decoder_decode(GameMovieDecoder *,int *last,int *palette_changed);
void game_movie_decoder_volume(GameMovieDecoder *,uint32_t);
int game_menu_movie_path(GameApplication *,const char *,char *,size_t);
GameAddress game_menu_movie_create(GameApplication *,const char *,uint32_t,uint32_t);
int game_menu_movie_play(GameApplication *,GameAddress);
int game_menu_movie_volume(GameApplication *,GameAddress,uint32_t);
int game_menu_movie_tick(GameApplication *,GameAddress);
int game_menu_movies_playing(GameApplication *);
int game_menu_movie_draw(GameApplication *,GameAddress,const GameUiRect *);
int game_menu_movie_destroy(GameApplication *,GameAddress);
void game_menu_movies_dispose(GameApplication *);
int game_menu_video_state(GameApplication *,GameAddress,uint32_t,uint32_t);
int game_menu_video_action(GameApplication *,uint32_t,uint32_t);
#endif
