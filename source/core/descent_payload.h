#ifndef UPRISING_DESCENT_PAYLOAD_H
#define UPRISING_DESCENT_PAYLOAD_H
#include "presentation_instance.h"
#include "presentation_state_copy.h"
/* Type0B's procedural model retains its bounded 54h/array records. */
int descent_model_construct(GameMemory *,GameAddress,const GamePresentationStorageServices *);
int descent_model_resize(GameMemory *,GameAddress,float,float);
GameAddress descent_model_clone(GameMemory *,GameAddress,const GamePresentationStorageServices *);
int descent_model_release(GameMemory *,GameAddress,int,const GamePresentationStorageServices *);
int descent_payload_construct(GameMemory *,GameAddress,const PresentationInstanceDefinition *);
int descent_payload_copy(GameMemory *,GameAddress,GameAddress,const PresentationConstructionServices *);
GameAddress descent_payload_template(GameMemory *,uint16_t,const GamePresentationStorageServices *);
int descent_payload_fill(GameMemory *,uint16_t,int32_t,const GamePresentationStorageServices *);
GameAddress descent_payload_take(GameMemory *,uint16_t,const GamePresentationStorageServices *);
typedef struct DescentUpdateServices {
    PresentationArticulatedServices contacts;
    int (*move)(void *,GameAddress,GameAddress);
    int (*phase)(void *,GameAddress,GameAddress);
} DescentUpdateServices;
int descent_payload_step(GameMemory *,GameAddress,const DescentUpdateServices *);
int descent_payload_advance(GameMemory *,GameAddress,const DescentUpdateServices *);
int descent_payload_reset(GameMemory *,GameAddress,const DescentUpdateServices *);
typedef struct DescentRenderServices {
    void *context;
    int (*surface)(void *,const float[3],float,float);
} DescentRenderServices;
int descent_model_present(GameMemory *,GameAddress,const DescentRenderServices *);
int descent_dirty_rectangle(GameMemory *,const uint16_t[4]);
int descent_enqueue_quad(GameMemory *,GameAddress);
int descent_surface_submit(GameMemory *,const float[3],float,float,int *);
#endif
