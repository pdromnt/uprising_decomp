#ifndef UPRISING_SCALED_EFFECT_MODEL_H
#define UPRISING_SCALED_EFFECT_MODEL_H
#include "presentation_state_copy.h"
/* 4faeb0/4fb310/4fb710/4fcce0: the record owns five bounded arrays.
 * Materials and the shared direction/UV tables are borrowed. No state mirror. */
int scaled_effect_model_construct(GameMemory *,GameAddress,uint32_t,int,
    const GamePresentationStorageServices *);
GameAddress scaled_effect_model_clone(GameMemory *,GameAddress,
    const GamePresentationStorageServices *);
int scaled_effect_model_release(GameMemory *,GameAddress,int,
    const GamePresentationStorageServices *);
int scaled_effect_model_present(GameMemory *,GameAddress,const float[3],const float[3],float);
#endif
