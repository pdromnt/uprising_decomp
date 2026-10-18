#ifndef UPRISING_PROJECTILE_FACTORY_H
#define UPRISING_PROJECTILE_FACTORY_H
#include "presentation_instance.h"

typedef struct ProjectileFamily {
    uint8_t kind;
    uint16_t slots;
    GameAddress publication,names,methods;
    uint32_t size;
} ProjectileFamily;
typedef struct ProjectileFactoryServices {
    void *context;
    int (*describe)(void *,const ProjectileFamily *,uint16_t,PresentationInstanceDefinition *);
    GameAddress (*allocate)(void *,uint32_t);
    int (*destroy)(void *,GameAddress);
    int (*clone_model)(void *,GameAddress,GameAddress *);
} ProjectileFactoryServices;
const ProjectileFamily *projectile_factory_family(unsigned);
GameAddress projectile_factory_template(GameMemory *,const ProjectileFamily *,uint16_t,
    const ProjectileFactoryServices *);
int projectile_factory_fill(GameMemory *,const ProjectileFamily *,uint16_t,int32_t,
    const ProjectileFactoryServices *);
GameAddress projectile_factory_take(GameMemory *,const ProjectileFamily *,uint16_t,
    const ProjectileFactoryServices *);
typedef struct ProjectileSplitServices {
    ProjectileFactoryServices factory;
    int (*sequence)(void *,GameAddress,uint32_t *);
    int (*launch)(void *,GameAddress,uint32_t,const PresentationPose *,GameAddress,GameAddress);
    int (*publish)(void *,GameAddress,GameAddress);
} ProjectileSplitServices;
int projectile_split(GameMemory *,GameAddress,Projectile *,PresentationPose *,
    const ProjectileSplitServices *);
#endif
