#ifndef UPRISING_SPATIAL_SAMPLE_H
#define UPRISING_SPATIAL_SAMPLE_H
#include "presentation_instance.h"
/* Class89 retains its verified record and existing presentation/model owners. */
typedef struct SpatialSampleDefinition {
    PresentationInstanceDefinition base;
    uint32_t groups[3];
    float anchors[3][3];
} SpatialSampleDefinition;
typedef struct SpatialSampleServices {
    PresentationArticulatedServices effects;
    int (*gather)(void *,GameAddress,uint32_t *);
    int (*probe)(void *,GameAddress);
    int (*unit)(void *,GameAddress catalog,uint8_t kind,int enabled);
    int (*base_reset)(void *,GameAddress,int notify);
    int (*catalog_value)(void *,GameAddress,uint8_t,unsigned,uint32_t *);
} SpatialSampleServices;
int spatial_sample_construct(GameMemory *,GameAddress,const SpatialSampleDefinition *);
int spatial_sample_copy(GameMemory *,GameAddress,GameAddress,const PresentationConstructionServices *);
int spatial_sample_activate(GameMemory *,GameAddress,uint32_t,GameAddress,const uint16_t[3],GameAddress,int,const PresentationActivationServices *,const SpatialSampleServices *);
int spatial_sample_frame(GameMemory *,GameAddress,int rendered);
int spatial_sample_vertices(GameMemory *,GameAddress,int rendered);
int spatial_sample_interpolate(GameMemory *,GameAddress);
int spatial_sample_present(GameMemory *,GameAddress,void *,int (*submit)(void *,GameAddress));
int spatial_sample_death(GameMemory *,GameAddress,GameAddress,const PresentationArticulatedDeathServices *);
int spatial_sample_advance(GameMemory *,GameAddress,const SpatialSampleServices *);
int spatial_sample_step(GameMemory *,GameAddress,const SpatialSampleServices *);
int spatial_sample_reset(GameMemory *,GameAddress,int notify,const SpatialSampleServices *);
typedef struct SpatialUnitRequestServices {
    void *context;
    int (*create)(void *,uint8_t,uint16_t,GameAddress *);
    int (*sequence)(void *,GameAddress,uint32_t *);
    int (*place)(void *,GameAddress,uint32_t,const float[3]);
    int (*publish)(void *,GameAddress,GameAddress);
    int (*sound)(void *,const float[3]);
    int (*remove)(void *,GameAddress,uint8_t,int);
    int (*retire)(void *,GameAddress,int);
} SpatialUnitRequestServices;
int spatial_sample_request_unit(GameMemory *,GameAddress,uint32_t,uint32_t,const SpatialUnitRequestServices *,uint32_t *);
typedef struct SpatialSampleFactoryServices {
    void *context;
    int (*describe)(void *,uint16_t,SpatialSampleDefinition *);
    GameAddress (*allocate)(void *,size_t);
    int (*release)(void *,GameAddress);
    PresentationConstructionServices construction;
    PresentationReleaseServices retirement;
} SpatialSampleFactoryServices;
GameAddress spatial_sample_template(GameMemory *,uint16_t,const SpatialSampleFactoryServices *);
int spatial_sample_fill(GameMemory *,uint16_t,int32_t,const SpatialSampleFactoryServices *);
GameAddress spatial_sample_take(GameMemory *,uint16_t,const SpatialSampleFactoryServices *);
#endif
