#include "level_load.h"
#include "terrain_asset.h"
#include <string.h>

/*456c50: retire previous candidates, rebuild traversal masks from the existing
 *terrain owner, then build the coarse2x2 visibility mask in its existing record.
 *The route-table consumer uses the same authoritative traversal grid. */
int game_level_visibility_reset(GameApplication *a,GameAddress owner){
    uint8_t adjacency[8192],coarse[2048]={0};float diagonal,axial;uint32_t bits;
    const TerrainAsset *terrain=level_terrain_asset(a->terrain);unsigned x,z,dx,dz;
    GameMemory *m=&a->memory;
    if(!terrain || !terrain->cells || terrain->width!=256 || terrain->height!=256)return 0;
    if(!game_level_visibility_pool_reset(a,owner) || !game_level_work_grid_reset(a,0x60a4c8))return 0;
    bits=game_memory_read_u32(m,0x5e1204);memcpy(&diagonal,&bits,4);
    bits=game_memory_read_u32(m,0x5e1208);memcpy(&axial,&bits,4);
    terrain_build_adjacency(terrain->cells,diagonal,axial,adjacency);
    if(m->fault || !game_memory_write(m,0x610520,adjacency,sizeof(adjacency)))return 0;
    for(z=0;z<256;z+=2)for(x=0;x<256;x+=2){
        int blocked=0;
        for(dz=0;dz<2 && !blocked;++dz)for(dx=0;dx<2;++dx){
            unsigned tx=x+dx,tz=z+dz;
            if(tx>=256 || tz>=256 || (adjacency[tz*32+tx/8]&(1u<<(tx&7)))){blocked=1;break;}
        }
        if(blocked)coarse[(z/2)*16+x/16]|=(uint8_t)(1u<<((x/2)&7));
    }
    return game_memory_write(m,owner+0x11834,coarse,sizeof(coarse)) &&
        game_level_navigation_routes_build(a,0x604258) && !m->fault;
}
