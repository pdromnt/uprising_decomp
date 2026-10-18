#include "application_entry.h"
#include "runtime_utilities.h"
#include "scene_projection_storage.h"
#include "level_load.h"
#include <stdio.h>
/*5476f0/547730. Historical callers call this 'player UI', but the record is
 * the scoped allocation registry. Native objects retain their existing owners;
 * registry entries are bounded storage IDs, never truncated malloc pointers. */
int game_level_player_ui_initialize(GameApplication *a) {
    runtime_reset_registration_array(&a->memory,0);return !a->memory.fault;
}
int game_level_player_ui_release(GameApplication *a) {
    uint32_t count=game_memory_read_u32(&a->memory,0x639828),i;char message[48];
    if(a->memory.fault || count>8000)return 0;
    if(!game_level_resource_clusters_release(a))return 0;
    {GameAddress root=game_memory_read_u32(&a->memory,0x5f0220);
        if(root && !game_level_imperial_release(a,root))return 0;}
    game_memory_write_u32(&a->memory,0x59d934,0);
    for(i=0;i<count && !a->memory.fault;++i){GameAddress slot=0x631b28+i*4,p=game_memory_read_u32(&a->memory,slot);
        if(p && !scene_projection_storage_free(p))return 0;
        game_memory_write_u32(&a->memory,slot,0);}
    if(a->memory.fault)return 0;
    snprintf(message,sizeof(message),"%u scoped allocations released",count);
    game_application_log(a,"allocation registry: %s\n",message);return 1;
}
