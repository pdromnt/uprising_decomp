#ifndef UPRISING_PRESENTATION_INSTANCE_H
#define UPRISING_PRESENTATION_INSTANCE_H

#include "game_memory.h"
#include "presentation_pose.h"
#include "presentation_lifecycle.h"
#include "player_events.h"
#include "player_rig.h"
#include "effect_animation.h"
#include "particle_effect.h"
#include "expansion_effect.h"
#include "projectile.h"
#include "tagged_scalar_stream.h"

typedef struct PresentationInstanceDefinition {
    uint8_t type;
    uint32_t flags;
    float dimensions[3];
    uint32_t radius, resource;
    int32_t count;
    GameAddress source, model;
    int copy_vertices;
} PresentationInstanceDefinition;

typedef struct PresentationInstance {
    GameAddress projection;
    PresentationInstanceDefinition definition;
    PresentationPose pose;
    PresentationLifecycle lifecycle;
    PlayerControls controls;
    PlayerRig rig;
    unsigned has_player_controls;
    EffectAnimation animation;
    unsigned has_animation, has_arrival;
    ParticleEffect particle;
    unsigned has_particle;
    ExpansionEffect expansion;
    unsigned has_expansion;
    Projectile projectile;
    unsigned has_projectile;
    /* Owned verified type5 record B4..E7. Native algorithms use bounded
     * endian access; no separate object or serialized state mirror. */
    uint8_t fragment_record[0x34];
    unsigned has_fragment;
    /* Authoritative class10 tail; configured hull geometry is borrowed. */
    uint8_t attachment_record[0x70];
    unsigned has_attachment;
    GameAddress vertices, controller;
    uint8_t release_phase;
    /* Authoritative class89 B4..187 tail, including its descriptor reference. */
    uint8_t spatial_record[0xd4];
    unsigned has_spatial_sample;
} PresentationInstance;

/* Resource/model payloads remain external ABI seams; the instance owns the
 * writable vertex/controller storage and its lifetime independently. */
int presentation_instance_create(GameMemory *memory, GameAddress object,
    const PresentationInstanceDefinition *definition, int runtime);
typedef struct PresentationConstructionServices {
    void *context;
    int (*clone_model)(void *, GameAddress, GameAddress *);
} PresentationConstructionServices;
int presentation_instance_create_copy(GameMemory *,GameAddress object,
    GameAddress source,int runtime,const PresentationConstructionServices *);
/*Transfer custom fixed render buffers to the same instance retirement owner. */
int presentation_instance_take_render_buffers(GameMemory *,GameAddress,GameAddress vertices,GameAddress controller);
typedef struct PresentationArticulatedDefinition {
    PresentationInstanceDefinition base;
    uint32_t groups[4];
    float anchors[6][3], spans[2];
} PresentationArticulatedDefinition;
typedef struct PresentationActivationServices {
    void *context;
    int (*place)(void *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
    int (*catalog_value)(void *,GameAddress,uint8_t,uint32_t,uint32_t,uint32_t *);
} PresentationActivationServices;
int presentation_instance_bind_source(GameMemory *,GameAddress,uint32_t,GameAddress,
    const uint16_t[3],GameAddress,int,const PresentationActivationServices *);
int presentation_instance_articulated_activate(GameMemory *,GameAddress,uint32_t,
    GameAddress,const uint16_t[3],GameAddress,int,const PresentationActivationServices *,uint32_t *);
int presentation_instance_articulated_vertices(GameMemory *,GameAddress);
int presentation_instance_articulated_render(GameMemory *,GameAddress);
int presentation_instance_articulated_interpolate(GameMemory *,GameAddress);
int presentation_instance_articulated_render_vertices(GameMemory *,GameAddress);
typedef struct PresentationArticulatedServices {
    void *context;
    int (*health_limit)(void *,GameAddress,uint32_t *);
    int (*create_effect)(void *,GameAddress,const float[3],float,float,GameAddress *);
    int (*retire_effect)(void *,GameAddress,int);
    int (*surface_effect)(void *,GameAddress,GameAddress);
    int (*credit)(void *,GameAddress,uint32_t);
    int (*sound)(void *,GameAddress,int32_t);
    int (*gather)(void *,GameAddress);
    int (*command)(void *,GameAddress,GameAddress,const float[3]);
    int (*notify_command)(void *,GameAddress,GameAddress,const float[3]);
    int (*release_commands)(void *,GameAddress);
    int (*refresh_effect)(void *,GameAddress,const float[3]);
    int (*deactivate)(void *,GameAddress);
    int (*reset)(void *,GameAddress);
    int (*detach_source)(void *,GameAddress,GameAddress);
} PresentationArticulatedServices;
int presentation_instance_articulated_update(GameMemory *,GameAddress,const PresentationArticulatedServices *);
int presentation_instance_articulated_advance(GameMemory *,GameAddress,const PresentationArticulatedServices *);
int presentation_instance_source_reset(GameMemory *,GameAddress,int,const PresentationArticulatedServices *);
int presentation_instance_articulated_reset(GameMemory *,GameAddress,int,const PresentationArticulatedServices *);
struct AttachmentEmission;
typedef struct PresentationArticulatedDeathServices {
    void *context;
    int (*begin)(void *,GameAddress,GameAddress);
    int (*sound)(void *,const float[3],int32_t);
    int (*mesh_fragments)(void *,GameAddress,GameAddress,const float[12],const float[3]);
    int (*variant)(void *,GameAddress,uint32_t *);
    int (*anchors)(void *,GameAddress,int32_t,GameAddress,const float[3]);
    GameAddress (*acquire)(void *,uint16_t);
    int (*emit)(void *,GameAddress,GameAddress,struct AttachmentEmission *);
    int (*discard)(void *,GameAddress);
} PresentationArticulatedDeathServices;
int presentation_instance_articulated_death(GameMemory *,GameAddress,GameAddress,const PresentationArticulatedDeathServices *);
int presentation_instance_articulated_present(GameMemory *,GameAddress);
int presentation_instance_articulated_create(GameMemory *,GameAddress,const PresentationArticulatedDefinition *);
int presentation_instance_articulated_copy(GameMemory *,GameAddress,GameAddress,const PresentationConstructionServices *);
const PresentationInstance *presentation_instance_find(GameAddress object);
int presentation_instance_source_is_empty(GameAddress source);
PresentationPose *presentation_instance_pose(GameAddress object);
PresentationLifecycle *presentation_instance_lifecycle(GameAddress object);
PlayerControls *presentation_instance_player_controls(GameAddress object);
PlayerRig *presentation_instance_player_rig(GameAddress object);
int presentation_instance_enable_player(GameMemory *,GameAddress);
int presentation_instance_enable_spatial_sample(GameMemory *,GameAddress);
typedef struct PresentationArchiveServices {
    const TaggedRecordStreamServices *records;
    void *context;
    int (*frames)(void *,GameAddress,int rendered);
    int (*publish)(void *,GameAddress);
} PresentationArchiveServices;
/* Layers: 0 entity, 1 presentation, 2 source-backed, 3 class89. */
int presentation_instance_stream(GameMemory *,GameAddress,int reading,unsigned layer,uint32_t *marker,const PresentationArchiveServices *);
EffectAnimation *presentation_instance_animation(GameAddress);
int presentation_instance_enable_animation(GameMemory *,GameAddress,int arrival);
ParticleEffect *presentation_instance_particle(GameAddress);
int presentation_instance_enable_particle(GameMemory *,GameAddress);
ExpansionEffect *presentation_instance_expansion(GameAddress);
int presentation_instance_enable_expansion(GameMemory *,GameAddress);
Projectile *presentation_instance_projectile(GameAddress);
int presentation_instance_enable_projectile(GameMemory *,GameAddress);
int presentation_instance_enable_fragment(GameMemory *,GameAddress);
int presentation_instance_enable_attachment(GameMemory *,GameAddress);
int presentation_instance_reserve_attachment_buffers(GameAddress);
int presentation_instance_reset(GameMemory *,GameAddress,unsigned kind,const PresentationResetServices *);
int presentation_instance_projection_read(GameAddress, void *, size_t);
int presentation_instance_projection_write(GameAddress, const void *, size_t);
int presentation_instance_projection_bounds(GameAddress *, GameAddress *);
const uint16_t *presentation_instance_owner_pages(void);
typedef struct PresentationReleaseServices {
    void *context;
    int (*release_model)(void *, GameAddress model);
    int (*release_storage)(void *, GameAddress storage);
    int (*release_pool_member)(void *, GameAddress member);
    int (*detach_packet_chain)(void *, GameAddress chain);
} PresentationReleaseServices;
/* 1 released, 0 external object, -1 failed. Successful resource phases are
 * retained; the same owner serves player, world and terrain constructors. */
typedef struct PresentationArticulatedFactoryServices {
    void *context;
    int (*describe)(void *,uint16_t,PresentationArticulatedDefinition *);
    GameAddress (*allocate)(void *,size_t);
    int (*release)(void *,GameAddress);
    PresentationConstructionServices construction;
    PresentationReleaseServices retirement;
} PresentationArticulatedFactoryServices;
GameAddress presentation_articulated_template(GameMemory *,uint16_t,const PresentationArticulatedFactoryServices *);
int presentation_articulated_fill(GameMemory *,uint16_t,int32_t,const PresentationArticulatedFactoryServices *);
GameAddress presentation_articulated_take(GameMemory *,uint16_t,const PresentationArticulatedFactoryServices *);
int presentation_instance_release(GameMemory *memory, GameAddress object,
    const PresentationReleaseServices *services, int controller, int runtime);

#endif
