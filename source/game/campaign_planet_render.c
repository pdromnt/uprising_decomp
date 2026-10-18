#include "campaign_planet.h"
#include "model_assets.h"
#include <stdint.h>
extern int game_model_graph_update(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
/*45bf00: animate the exclusive catalog texture in place every seventh draw.
 * Source and flag bytes are borrowed from their existing full-width texture
 * owners. No byte mirror, pointer-to-word conversion or CPU callback exists. */
int game_campaign_planet_texture_advance(GameApplication *a,GameAddress planet) {
    uint32_t tick=get(a,planet+0x580)+1,kind,frame,completed;GameAddress descriptor,material;
    const TextureRecord *source,*flag,*destination;int32_t width,height;uint64_t size;size_t i;
    if((int32_t)tick>=7)tick=0;game_memory_write_u32(&a->memory,planet+0x580,tick);if(tick)return !a->memory.fault;
    kind=get(a,planet+0x564);frame=get(a,planet+0x584);completed=get(a,planet+0x2c);
    if(kind>=get(a,0x5a052c) || frame>=8)return 0;
    material=get(a,get(a,0x5a0524)+kind*4);source=game_model_texture(a,get(a,material+4));
    descriptor=get(a,get(a,planet)+0x50);material=get(a,get(a,descriptor+0x18)+0x14);
    width=(int16_t)game_memory_read_u16(&a->memory,material);height=(int16_t)game_memory_read_u16(&a->memory,material+2);
    destination=game_model_texture(a,get(a,material+4));material=get(a,0x5e1450+frame*4);flag=game_model_texture(a,get(a,material+4));
    if(width>0 && height>0){size=(uint64_t)(uint32_t)width*(uint32_t)height;
        if(!source || !destination || source==destination || source->image.format!=5 || destination->image.format!=5 ||
           size>source->image.size || size>destination->image.size || (!completed && (!flag || flag->image.format!=5 || size>flag->image.size)))return 0;
        /*Retail indexes these software rows by width, including its behavior
         * on padded input. Keep that arithmetic bounded by the actual owners. */
        for(i=0;i<(size_t)size;++i)destination->image.pixels[i]=!completed && flag->image.pixels[i]!=254?flag->image.pixels[i]:source->image.pixels[i];
    }
    game_memory_write_u32(&a->memory,planet+0x584,frame==7?0:frame+1);return !a->memory.fault;
}
/*45bbe0: the same main model is submitted for ordinary and mission planets. */
int game_campaign_planet_present(GameApplication *a,GameAddress planet) {
    if(get(a,planet+0x24) && !game_campaign_planet_texture_advance(a,planet))return 0;
    return !a->memory.fault && game_model_graph_update(a,get(a,planet));
}
