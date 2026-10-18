#ifndef UPRISING_CURVE_EFFECT_MODEL_H
#define UPRISING_CURVE_EFFECT_MODEL_H
#include "presentation_state_copy.h"
/*5752d0: one record owns five arrays; material and curve coefficients borrow
 *their existing owners. The tube has nine rings and three points per ring. */
int curve_effect_model_construct(GameMemory *,GameAddress,GameAddress,const GamePresentationStorageServices *);
GameAddress curve_effect_model_clone(GameMemory *,GameAddress,const GamePresentationStorageServices *);
int curve_effect_model_release(GameMemory *,GameAddress,int,const GamePresentationStorageServices *);
int curve_effect_model_present(GameMemory *,GameAddress,const float[4][3],const float[3][3]);
#endif
