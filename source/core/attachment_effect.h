#ifndef UPRISING_ATTACHMENT_EFFECT_H
#define UPRISING_ATTACHMENT_EFFECT_H
#include "presentation_instance.h"
/* Class10 uses the existing presentation owner and verified B4..123 record.
 * Bones are borrowed; vertices/controller and the mutable model are owned. */
typedef struct AttachmentResetServices {
    void *context;
    int (*stop_voice)(void *,uint32_t);
    int (*remove_child)(void *,GameAddress);
    const PresentationResetServices *membership;
} AttachmentResetServices;
int attachment_effect_initialize(GameMemory *,GameAddress);
int attachment_effect_configure(GameMemory *,GameAddress,GameAddress,GameAddress,uint16_t);
int attachment_effect_present(GameMemory *,GameAddress);
int attachment_effect_controller(GameMemory *,GameAddress);
int attachment_effect_reset(GameMemory *,GameAddress,const AttachmentResetServices *);
int attachment_mesh_clear(GameMemory *,GameAddress);
typedef struct AttachmentEmission {
    uint32_t identity;
    float position[3],velocity[3];
    uint16_t angles[3],mode,flags;
    uint32_t duration,source,deadline;
} AttachmentEmission;
typedef struct AttachmentEmissionServices {
    void *context;
    int (*place)(void *,const AttachmentEmission *,const uint16_t motion[3]);
    int (*height)(void *,float,float,long double *);
    int (*sound)(void *,const float position[3],int16_t,float pitch,uint32_t flags,int force,uint32_t *voice);
    int (*announcement)(void *,int16_t);
} AttachmentEmissionServices;
int attachment_effect_emit(GameMemory *,GameAddress,const AttachmentEmission *,const AttachmentEmissionServices *);
int attachment_effect_finish_placement(GameMemory *,GameAddress);
int attachment_effect_retain_anchors(GameMemory *,GameAddress,int32_t,GameAddress,uint16_t,const float[3]);
int attachment_effect_transfer_children(GameMemory *,GameAddress,GameAddress,GameAddress);
int attachment_effect_scale(GameMemory *,GameAddress,float);
int attachment_effect_accelerate(GameMemory *,GameAddress);
int attachment_effect_move(GameMemory *,GameAddress);
int attachment_effect_interpolate(GameMemory *,GameAddress);
uint32_t attachment_effect_detail(GameMemory *,GameAddress);
typedef struct AttachmentUpdateServices {
    void *context;
    int (*reset)(void *);
    int (*post)(void *,GameAddress);
    int (*spawn_animation)(void *,const float[3],uint8_t,float);
    int (*spawn_expansion)(void *,const float[3],float,float,GameAddress *);
    int (*surface)(void *,const float[3]);
    int (*wind)(void *,const float[3],uint32_t);
    int (*advance)(void *);
    int (*sound)(void *,const float[3],int16_t);
    int (*burst)(void *,uint16_t,int32_t,int32_t);
    int (*mesh_fragments)(void *,GameAddress,const float[12],const float[3]);
    int (*anchors)(void *,int32_t,GameAddress,const float[3]);
} AttachmentUpdateServices;
int attachment_effect_update(GameMemory *,GameAddress,const AttachmentUpdateServices *);
int attachment_effect_post(GameMemory *,GameAddress,GameAddress,const AttachmentUpdateServices *);
typedef struct AttachmentTerrainServices {
    void *context;
    int (*stop_voice)(void *,uint32_t);
    int (*impact)(void *,uint8_t);
    const AttachmentUpdateServices *update;
} AttachmentTerrainServices;
int attachment_effect_expansion(GameMemory *,GameAddress,const AttachmentUpdateServices *);
int attachment_effect_terrain_response(GameMemory *,GameAddress,const AttachmentTerrainServices *);
typedef struct AttachmentContactServices {
    void *context;
    int (*damage)(void *,GameAddress,int32_t *);
    const AttachmentUpdateServices *update;
} AttachmentContactServices;
int attachment_effect_contact(GameMemory *,GameAddress,GameAddress,const AttachmentContactServices *);
typedef struct AttachmentAdvanceServices {
    void *context;
    int (*gather)(void *,const float[3]);
    int (*peer_contact)(void *,GameAddress,const float[3],GameAddress,const float[3]);
    const AttachmentContactServices *contact;
    const AttachmentTerrainServices *terrain;
} AttachmentAdvanceServices;
int attachment_effect_advance(GameMemory *,GameAddress,const AttachmentAdvanceServices *);
float attachment_effect_pitch(GameMemory *,float,long double);
typedef struct AttachmentAnchorServices {
    void *context;
    GameAddress (*acquire)(void *,uint16_t);
    int (*emit)(void *,GameAddress,GameAddress,AttachmentEmission *);
    int (*discard)(void *,GameAddress);
} AttachmentAnchorServices;
int attachment_effect_anchors(GameMemory *,GameAddress,int32_t,GameAddress,const float[3],
    const AttachmentAnchorServices *);
int attachment_effect_legacy_anchors(GameMemory *,GameAddress,int32_t,GameAddress,const float[3],uint16_t,
    const AttachmentAnchorServices *);
#endif
