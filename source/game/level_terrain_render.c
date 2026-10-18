#include "level_scene_render.h"
#include "gameplay_geometry.h"
#include "render_arena.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
/*49c670 ->49b550/49bc10: consume the actual five-point frustum through the
 * existing native edge/row/clip algorithms. Terrain and scene scratch pools
 * have distinct owners; only the final painter queue joins their packet IDs. */
int game_level_terrain_present(GameApplication *a,GameAddress viewport,GameAddress matrix){
    GameplayTerrainPoint points[5];GameplayTerrainEdgeStream stream;
    GameplayTerrainFrame frame={0};unsigned i;
    render_arena_begin_frame();
    for(i=0;i<4;i++)put(a,0x597720+i*8,get(a,0x597724+i*8));
    for(i=0;i<5;i++){
        points[i].x=real(a,viewport+i*12);points[i].y=real(a,viewport+i*12+4);
        points[i].z=real(a,viewport+i*12+8);
        if(!isfinite(points[i].x) || !isfinite(points[i].y) || !isfinite(points[i].z) ||
           points[i].x/512.0L<=INT32_MIN || points[i].x/512.0L>=INT32_MAX ||
           points[i].z/512.0L<=INT32_MIN || points[i].z/512.0L>=INT32_MAX){
            GameAddress camera=get(a,0x58f890);
            char diagnostic[256];
            snprintf(diagnostic,sizeof(diagnostic),
                "terrain invalid frustum: point=%u xyz=%.9g/%.9g/%.9g camera=%08x method=%08x mode=%u far=%u fov=%.9g\n",
                i,(double)points[i].x,(double)points[i].y,(double)points[i].z,
                camera,get(a,camera),get(a,0x604760),get(a,0x597f7c),(double)real(a,0x59d43c));
            game_application_log(a,"%s",diagnostic);return 0;
        }
    }
    if(a->memory.fault || !gameplay_terrain_edge_stream_build(points,&stream))return 0;
    memcpy(frame.edges,stream.edges,sizeof(frame.edges));
    frame.clip.vertex=get(a,0x597720);frame.vertex_begin=get(a,0x597724);
    frame.clip.packet=get(a,0x597728);frame.clip.uv=get(a,0x597730);frame.node_cursor=get(a,0x597738);
    frame.render_head=get(a,0x6252f8);frame.previous_head=get(a,0x604484);
    {uint32_t bits=get(a,0x597f7c);memcpy(&frame.far_distance,&bits,4);}
    frame.center_x=real(a,0x6290c8);frame.center_y=real(a,0x6290cc);
    frame.scale=real(a,0x6290f4);frame.vertical=real(a,0x6290f0);
    frame.numerator=real(a,0x574cb0);frame.backface_threshold=real(a,0x574cb4);
    frame.bounds[0]=real(a,0x6290d4);frame.bounds[1]=real(a,0x6290d0);
    frame.bounds[2]=real(a,0x6290dc);frame.bounds[3]=real(a,0x6290d8);
    if(a->memory.fault || !gameplay_geometry_render_terrain_frame(&a->memory,matrix,
            (int32_t)stream.count,points[0].x,points[0].z,&frame))return 0;
    put(a,0x597720,frame.clip.vertex);put(a,0x597728,frame.clip.packet);
    put(a,0x597730,frame.clip.uv);put(a,0x597738,frame.node_cursor);put(a,0x6252f8,frame.render_head);
    return !a->memory.fault;
}
