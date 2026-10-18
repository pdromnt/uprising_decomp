#ifndef UPRISING_VEHICLE_PRESENTATION_H
#define UPRISING_VEHICLE_PRESENTATION_H
#include "presentation_instance.h"
#include "player_feedback.h"
typedef struct VehiclePresentationDefinition {
    PresentationInstanceDefinition base;
    uint32_t groups[3];
    float anchors[3][3];
} VehiclePresentationDefinition;
/* 21 shared grouped object, 22 gameplay base, 23 deployment payload. */
int vehicle_presentation_construct(GameMemory *,GameAddress,const VehiclePresentationDefinition *,uint8_t);
int vehicle_presentation_finish(GameMemory *,GameAddress,const VehiclePresentationDefinition *,uint8_t);
int vehicle_presentation_copy_finish(GameMemory *,GameAddress,GameAddress,uint8_t);
int vehicle_presentation_copy(GameMemory *,GameAddress,GameAddress,uint8_t,const PresentationConstructionServices *);
typedef struct VehiclePresentationFactoryServices {
    void *context;
    int (*describe)(void *,uint16_t,VehiclePresentationDefinition *);
    GameAddress (*allocate)(void *,size_t);
    int (*release)(void *,GameAddress);
    PresentationConstructionServices construction;
    PresentationReleaseServices retirement;
    uint8_t type; /*Zero retains the existing type23 oracle entry point. */
} VehiclePresentationFactoryServices;
typedef struct VehiclePresentationFamily {
    uint8_t type,slots;uint16_t bytes;
    GameAddress templates,names,point_counts,points,part_counts,parts;
} VehiclePresentationFamily;
const VehiclePresentationFamily *vehicle_presentation_family(uint8_t);
GameAddress vehicle_presentation_template(GameMemory *,uint16_t,const VehiclePresentationFactoryServices *);
int vehicle_presentation_fill(GameMemory *,uint16_t,int32_t,const VehiclePresentationFactoryServices *);
GameAddress vehicle_presentation_take(GameMemory *,uint16_t,const VehiclePresentationFactoryServices *);
typedef struct VehicleRetirementServices {
    void *context;
    int (*return_event)(void *,GameAddress);
    int (*release_storage)(void *,GameAddress);
    int (*refresh_selection)(void *,GameAddress catalog);
    int (*notify_inactive)(void *,GameAddress object);
    int (*detach_controller)(void *,GameAddress controller,GameAddress object);
    int (*base_reset)(void *,GameAddress);
    int (*base_release)(void *,GameAddress);
} VehicleRetirementServices;
int vehicle_presentation_clear_events(GameMemory *,GameAddress,int,const VehicleRetirementServices *);
uint32_t vehicle_presentation_release_relation(GameMemory *,GameAddress,GameAddress);
int vehicle_presentation_auxiliary_reset(GameMemory *,GameAddress,const VehicleRetirementServices *);
int vehicle_presentation_base_reset(GameMemory *,GameAddress,const VehicleRetirementServices *);
int vehicle_presentation_reset(GameMemory *,GameAddress,const VehicleRetirementServices *);
int vehicle_presentation_base_destroy(GameMemory *,GameAddress,const VehicleRetirementServices *);
int vehicle_presentation_destroy(GameMemory *,GameAddress,int,const VehicleRetirementServices *);
int vehicle_presentation_world_frame(GameMemory *,GameAddress);
int vehicle_presentation_group_geometry(GameMemory *,GameAddress,unsigned operation);
int vehicle_presentation_local_pose(GameMemory *,GameAddress,int interpolate);
int vehicle_presentation_world_vertices(GameMemory *,GameAddress);
int vehicle_presentation_render_frame(GameMemory *,GameAddress);
int vehicle_presentation_render_vertices(GameMemory *,GameAddress);
typedef struct VehicleInputServices {
    void *context;
    int (*playing)(void *,int16_t,uint32_t,int *);
    int (*stop)(void *,int16_t,uint32_t);
    int (*play)(void *,int16_t,float,float,uint32_t);
    int (*volume)(void *,int16_t,uint32_t,float);
    int (*refresh)(void *,GameAddress,GameAddress method);
    int (*overlap)(void *,GameAddress,GameAddress method,const float position[3],float contact[3],int *);
} VehicleInputServices;
/*5521d0: input angles, spin mode, audio debounce and collision rollback.
 *Two saved matrices are local rollback data, not another state owner. */
int vehicle_presentation_input_tick(GameMemory *,GameAddress,const VehicleInputServices *);

typedef struct VehicleUpdateServices {
    void *context;
    int (*property)(void *,GameAddress,GameAddress method,uint32_t *);
    int (*particles)(void *,GameAddress,const float position[3],const float velocity[3],float,GameAddress,uint16_t,uint32_t,uint32_t);
    int (*surface_particles)(void *,GameAddress,const float position[3],int32_t,GameAddress);
    GameAddress (*take_expansion)(void *);
    int (*start_expansion)(void *,GameAddress,uint32_t,const float[3],float,float,int32_t);
    int (*stop_expansion)(void *,GameAddress);
    int (*publish_expansion)(void *,GameAddress root,GameAddress);
    int (*next_identity)(void *,GameAddress root,uint32_t *);
    int (*expire_input)(void *,GameAddress);
    int (*command)(void *,GameAddress,GameAddress method,uint32_t);
    int (*refresh)(void *,GameAddress,GameAddress method);
    int (*move_expansion)(void *,GameAddress,const float[3]);
    int (*attachment)(void *,GameAddress,GameAddress method,const float matrix[12]);
} VehicleUpdateServices;
/*5527f0: deployment, damage effects, input expiry and attachment transform.
 *Existing record/instance owners remain authoritative. */
int vehicle_presentation_update(GameMemory *,GameAddress,const VehicleUpdateServices *);
typedef struct VehicleMotionServices {
    void *context;
    int (*height)(void *,GameAddress,float,float,long double *);
    int (*control)(void *,GameAddress,GameAddress method);
    int (*refresh)(void *,GameAddress,GameAddress method);
    int (*gather)(void *,GameAddress,GameAddress method,const float[3],int *);
    int (*contact)(void *,GameAddress,GameAddress other,GameAddress method,const uint32_t velocity[3],const float position[3]);
    int (*release_contacts)(void *,GameAddress);
    int (*spatial)(void *,GameAddress,int insert);
    int (*stop_voice)(void *,uint32_t voice);
} VehicleMotionServices;
int vehicle_presentation_motion(GameMemory *,GameAddress,const VehicleMotionServices *);
int vehicle_base_motion(GameMemory *,GameAddress,const VehicleMotionServices *); /*53b3e0*/
int vehicle_group_motion(GameMemory *,GameAddress,const VehicleMotionServices *); /*4035f0*/
int vehicle_heading_step(GameMemory *,GameAddress); /*405930*/
typedef struct VehicleNavigationServices {
    void *context;
    int (*speed)(void *,GameAddress,long double *);
    int (*height)(void *,GameAddress,float,float,long double *);
    int (*normal)(void *,float[3],float,float);
    int (*ascent_voice)(void *,GameAddress);
} VehicleNavigationServices;
int vehicle_destination_step(GameMemory *,GameAddress,const VehicleNavigationServices *); /*53a3e0*/
int vehicle_class29_destination(GameMemory *,GameAddress,const VehicleNavigationServices *); /*4511a0*/
int vehicle_class29_steer(GameMemory *,GameAddress); /*4516d0*/
int vehicle_presentation_class29_finish(GameMemory *,GameAddress,GameAddress); /*450c00/450af0*/
int vehicle_group_destination(GameMemory *,GameAddress,const VehicleNavigationServices *); /*4e8510*/
int vehicle_group_ascent(GameMemory *,GameAddress,const VehicleNavigationServices *); /*4062b0*/
int vehicle_presentation_remove_input(GameMemory *,GameAddress,GameAddress event);
int vehicle_presentation_command(GameMemory *,GameAddress,uint32_t,const GamePresentationFeedbackServices *);
typedef struct VehicleActivationServices {
    void *context;
    int (*reset_foreign)(void *,GameAddress object,GameAddress method);
    int (*prepare)(void *,GameAddress object,GameAddress point,uint16_t heading);
    int (*place)(void *,GameAddress object,uint32_t identity,GameAddress point,const uint16_t angles[3]);
    int (*catalog_value)(void *,GameAddress catalog,uint8_t type,uint32_t item,uint32_t alternate,uint32_t *);
    int (*component_enabled)(void *,GameAddress catalog,uint8_t type,uint32_t *);
    int (*auxiliary_foreign)(void *,GameAddress object,GameAddress catalog,GameAddress method);
    GameAddress (*allocate)(void *,size_t);
    int (*release)(void *,GameAddress);
} VehicleActivationServices;
int vehicle_presentation_build_auxiliary(GameMemory *,GameAddress,GameAddress,const VehicleActivationServices *);
int vehicle_presentation_mode_place(GameMemory *,GameAddress,uint32_t,GameAddress,const uint16_t[3],const VehicleActivationServices *);
int vehicle_presentation_gameplay_place(GameMemory *,GameAddress,uint32_t,GameAddress,const uint16_t[3],const VehicleActivationServices *);
int vehicle_presentation_activate(GameMemory *,GameAddress,uint32_t,GameAddress,const uint16_t[3],GameAddress,const VehicleActivationServices *);
#endif
