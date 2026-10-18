#ifndef UPRISING_NATIVE_PRESENTATION_STATE_COPY_H
#define UPRISING_NATIVE_PRESENTATION_STATE_COPY_H

#include "game_memory.h"

typedef struct GamePresentationRecordCopy {
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
} GamePresentationRecordCopy;

GamePresentationRecordCopy game_presentation_record_copy(GameMemory *memory,
    GameAddress destination, GameAddress source);

typedef struct GamePresentationDefinition {
    GameAddress dimensions;
    uint32_t radius;
    uint16_t record_count;
    GameAddress records;
    uint32_t data_count;
    GameAddress data,positions,model;
    int32_t slot_count;
} GamePresentationDefinition;
typedef struct GamePresentationStorageServices {
    void *context;
    GameAddress (*allocate)(void *,size_t);
    int (*release)(void *,GameAddress);
    int (*clone_model)(void *,GameAddress,GameAddress *);
    int (*release_model)(void *,GameAddress);
} GamePresentationStorageServices;
/* Fresh records transfer the input arrays. Copies borrow source vertices and
 * own their output arrays/model clone. Their existing fields retain ownership. */
int game_presentation_construct(GameMemory *,GameAddress,const GamePresentationDefinition *,const GamePresentationStorageServices *);
int game_presentation_construct_values(GameMemory *,GameAddress,const float dimensions[3],const GamePresentationDefinition *,const GamePresentationStorageServices *);
int game_presentation_copy(GameMemory *,GameAddress,GameAddress,const GamePresentationStorageServices *);
int game_presentation_destroy(GameMemory *,GameAddress,const GamePresentationStorageServices *);
int game_presentation_base_copy(GameMemory *,GameAddress,GameAddress,const GamePresentationStorageServices *);
int game_presentation_base_destroy(GameMemory *,GameAddress,const GamePresentationStorageServices *);
int game_presentation_variant(GameMemory *,GameAddress,unsigned kind);
typedef struct GamePresentationBindingServices {
    void *context;
    int (*prepare)(void *,GameAddress,float position[3],uint16_t heading);
    int (*place)(void *,GameAddress,uint32_t,const float position[3],const uint16_t angles[3]);
} GamePresentationBindingServices;
int game_presentation_bind(GameMemory *,GameAddress,uint32_t,const float position[3],
    const uint16_t angles[3],int prepare,const GamePresentationBindingServices *);
typedef struct GamePresentationFactoryFamily {
    uint8_t kind;
    uint16_t slots;
    GameAddress publication,names,primary,secondary;
} GamePresentationFactoryFamily;
typedef struct GamePresentationFactoryServices {
    void *context;
    /* 1 loads owned inputs; 0 is an unsupported asset route; -1 fails. */
    int (*describe)(void *,const GamePresentationFactoryFamily *,uint16_t,
        float dimensions[3],GamePresentationDefinition *);
    const GamePresentationStorageServices *storage;
} GamePresentationFactoryServices;
const GamePresentationFactoryFamily *game_presentation_factory_family(unsigned kind);
GameAddress game_presentation_factory_template(GameMemory *,const GamePresentationFactoryFamily *,uint16_t,const GamePresentationFactoryServices *);
int game_presentation_factory_fill(GameMemory *,const GamePresentationFactoryFamily *,uint16_t,int32_t,const GamePresentationFactoryServices *);
GameAddress game_presentation_factory_take(GameMemory *,const GamePresentationFactoryFamily *,uint16_t,const GamePresentationFactoryServices *);

#endif
