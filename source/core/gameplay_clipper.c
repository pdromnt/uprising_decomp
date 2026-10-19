#include "render_arena.h"
#include "render_packet_view.h"
#include "gameplay_clipper.h"
#include "scene_surface.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define CLIP_MAX_VERTICES GAMEPLAY_CLIP_MAX_VERTICES

typedef struct ClipVertex {
    float x;
    float y;
    float z;
} ClipVertex;

static void clip_guard_log(const char *reason, uint32_t polygon,
    int32_t count, uint32_t pointer) {
    static unsigned reports;
    if (reports++ >= 32u) return;
    fprintf(stderr, "CLIP_GUARD reason=%s polygon=%08x count=%d pointer=%08x\n",
        reason, polygon, count, pointer);
}

static uint32_t clip_read_index(GameMemory *memory,GameAddress address){
    uint32_t *index=render_arena_indices(address,1);
    const uint32_t *owned=index?index:scene_projection_storage_links(address,1u);
    uint32_t value;
    if(!owned && memory->surface)
        owned=scene_surface_link(memory->surface,address);
    if(owned)return *owned;
    if(memory->surface && render_arena_read(address,&value,sizeof(value)))
        return value;
    if(memory->index_view){
        uint32_t *native=memory->index_view(memory->fallback_context,address);
        if(native)return *native;
    }
    return game_memory_read_u32(memory,address);
}
static RenderArenaPacket *clip_packet(GameMemory *memory,GameAddress address){
    RenderArenaPacket *packet=render_packet_view(address);
    if(packet)return packet;
    packet=scene_surface_packet(memory->surface,address);
    return packet?packet:memory->packet_view?
        memory->packet_view(memory->fallback_context,address):NULL;
}
static GameAddress clip_packet_next(GameMemory *memory,GameAddress address,
    RenderArenaPacket *packet){
    RenderArenaPacket *next_owned=NULL;
    GameAddress next;
    if(packet && render_packet_link(address,&next,&next_owned))return next;
    return packet?packet->next:game_memory_read_u32(memory,address+12u);
}
static void clip_write_index(GameMemory *memory,GameAddress address,uint32_t value){
    uint32_t *index=render_arena_indices(address,1);
    if(index)*index=value;
    else if(memory->index_view &&
        (index=memory->index_view(memory->fallback_context,address))){
        *index=value;
        if(memory->note_typed_write)
            memory->note_typed_write(memory->fallback_context,address);
    }
    else if(!memory->surface ||
            !render_arena_write(address,&value,sizeof(value)))
        game_memory_write_u32(memory,address,value);
}
static float clip_read_f32(GameMemory *memory, uint32_t address) {
    float value;
    float *component=render_arena_float_component(address);if(component)return *component;
    if(!(address&3u)){
        uint32_t *bits=render_arena_indices(address,1u);
        float value;
        if(bits){memcpy(&value,bits,4u);return value;}
    }
    if(!(address&3u) && (address&15u)<12u){
        RenderArenaVertex *vertex=scene_projection_storage_transformed(
            address&~15u,1u);
        if(vertex)return vertex->position[(address&15u)/4u];
    }
    if(memory->surface){
        float *surface=scene_surface_transformed_component(memory->surface,address);
        if(surface)return *surface;
    }
    if(!(address&3u)){
        const RenderArenaUv *uv=scene_projection_storage_work_uv(
            address&~7u,1u);
        if(uv)return (address&4u)?uv->v:uv->u;
        if(scene_surface_uv_component(memory->surface,address,&value))return value;
    }
    if(memory->float_view){
        float *native=memory->float_view(memory->fallback_context,address);
        if(native)return *native;
    }
    uint32_t bits;
    if(!memory->surface || !render_arena_read(address,&bits,sizeof(bits)))
        bits=game_memory_read_u32(memory,address);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void clip_write_f32(GameMemory *memory, uint32_t address, float value) {
    float *component=render_arena_float_component(address);if(component){*component=value;return;}
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    if(!(address&3u)){
        uint32_t *scalar=render_arena_indices(address,1u);
        if(scalar){*scalar=bits;return;}
    }
    if(memory->float_view){
        float *native=memory->float_view(memory->fallback_context,address);
        if(native){*native=value;
            if(memory->note_typed_write)
                memory->note_typed_write(memory->fallback_context,address);
            return;}
    }
    if(!memory->surface || !render_arena_write(address,&bits,sizeof(bits)))
        game_memory_write_u32(memory, address, bits);
}

static ClipVertex clip_read_vertex(GameMemory *memory, uint32_t address) {
    ClipVertex vertex;
    vertex.x = clip_read_f32(memory, address);
    vertex.y = clip_read_f32(memory, address + 4u);
    vertex.z = clip_read_f32(memory, address + 8u);
    return vertex;
}

static float clip_lerp(float previous, float current, float fraction) {
    return (float)(((long double)current - (long double)previous) * fraction +
        (long double)previous);
}

static uint32_t clip_intersection(GameMemory *memory, uint32_t cursor,
    ClipVertex previous, ClipVertex current, float near_z,
    float *fraction_out) {
    float fraction = (float)(
        ((long double)near_z - (long double)previous.z) /
        ((long double)current.z - (long double)previous.z));
    clip_write_f32(memory, cursor, clip_lerp(previous.x, current.x, fraction));
    clip_write_f32(memory, cursor + 4u,
        clip_lerp(previous.y, current.y, fraction));
    clip_write_f32(memory, cursor + 8u, near_z);
    *fraction_out = fraction;
    return cursor;
}


static uint32_t clip_input_vertex(GameMemory *memory,
        const GameplayClipPolygonInput *input, uint32_t indices,
        uint32_t index) {
    return input ? input->vertices[index] :
        game_memory_read_u32(memory, indices + index * 4u);
}

static GameAddress clip_cursor_get(GameMemory *memory,GameAddress address,
    GameAddress *native){return native?*native:game_memory_read_u32(memory,address);}
static void clip_cursor_set(GameMemory *memory,GameAddress address,
    GameAddress *native,GameAddress value){
    if(native)*native=value;else game_memory_write_u32(memory,address,value);
}

static float clip_input_uv(GameMemory *memory,
        const GameplayClipPolygonInput *input, uint32_t attributes,
        uint32_t index, uint32_t component) {
    return input ? input->uv[index][component] :
        clip_read_f32(memory, attributes + index * 8u + component * 4u);
}

static float clip_input_scalar(GameMemory *memory,
        const GameplayClipPolygonInput *input, uint32_t attributes,
        uint32_t index) {
    return input ? input->scalars[index] :
        clip_read_f32(memory, attributes + index * 4u);
}

static GameAddress clip_polygon_inputs(GameMemory *memory,
        GameAddress polygons, int32_t polygon_count,
        const GameplayClipPolygonInput *inputs, float near_z, float far_z,
        GameAddress vertex_cursor_pointer, GameAddress index_cursor_pointer,
        GameAddress uv_cursor_pointer, GameAddress scalar_cursor_pointer,
        GameplayClipCursors *cursors) {
    GameAddress vertex_cursor = 0u;
    GameAddress index_cursor = 0u;
    GameAddress uv_cursor = 0u;
    GameAddress scalar_cursor = 0u;
    int vertex_cursor_loaded = 0;
    int index_cursor_loaded = 0;
    int uv_cursor_loaded = 0;
    int scalar_cursor_loaded = 0;
    GameAddress list_head = 0u;
    int32_t polygon_index;
    if (memory->fault) return 0u;
    /* Empty batches are common while menus assemble optional geometry.
       Retail returns before dereferencing any of the arena-pointer arguments;
       some callers intentionally leave those arguments null in this path. */
    if (polygon_count <= 0) {
        return 0u;
    }
    if (polygons < 0x10000u) {
        clip_guard_log("polygon-array", polygons, polygon_count, polygons);
        goto clipper_return;
    }
    for (polygon_index = 0; polygon_index < polygon_count; polygon_index++) {
        uint32_t polygon = polygons + (uint32_t)polygon_index * 40u;
        RenderArenaPacket *packet=clip_packet(memory,polygon);
        const GameplayClipPolygonInput *input = inputs ? &inputs[polygon_index] : NULL;
        int32_t input_count = input ? input->count :
            (packet?(int32_t)packet->count:(int32_t)game_memory_read_u32(memory, polygon));
        uint32_t input_indices = input ? 0u :
            (packet?packet->vertices:game_memory_read_u32(memory, polygon + 4u));
        uint32_t flags = input ? input->flags :
            (packet?packet->flags:game_memory_read_u32(memory, polygon + 8u));
        uint32_t input_attributes = input ? 0u :
            (packet?packet->attributes:game_memory_read_u32(memory, polygon + 16u));
        uint32_t output_indices[CLIP_MAX_VERTICES];
        float output_uvs[CLIP_MAX_VERTICES][2];
        float output_scalars[CLIP_MAX_VERTICES];
        unsigned output_count = 0u;
        int changed = 0;
        int32_t current_index;

        if (memory->fault) return list_head;
        if (input && input_count > (int32_t)CLIP_MAX_VERTICES) {
            memory->fault = 1;
            return list_head;
        }
        if (input_count > 0) {
            if (!input && input_indices < 0x10000u) {
                clip_guard_log("index-array", polygon, input_count,
                    input_indices);
                goto clipper_return;
            }
            uint32_t previous_address = clip_input_vertex(memory, input,
                input_indices, 0u);
            if (memory->fault) return list_head;
            if (previous_address < 0x10000u) {
                clip_guard_log("first-vertex", polygon, input_count,
                    previous_address);
                goto clipper_return;
            }
            ClipVertex previous = clip_read_vertex(memory, previous_address);
            int previous_inside = near_z <= previous.z;

            if (previous_inside) {
                output_indices[output_count] = previous_address;
                if ((flags & 1u) != 0u) {
                    if (!input && input_attributes < 0x10000u) {
                        clip_guard_log("uv-input", polygon, input_count,
                            input_attributes);
                        goto clipper_return;
                    }
                    output_uvs[output_count][0] = clip_input_uv(memory, input,
                        input_attributes, 0u, 0u);
                    output_uvs[output_count][1] = clip_input_uv(memory, input,
                        input_attributes, 0u, 1u);
                } else if ((flags & 2u) != 0u) {
                    if (!input && input_attributes < 0x10000u) {
                        clip_guard_log("scalar-input", polygon, input_count,
                            input_attributes);
                        goto clipper_return;
                    }
                    output_scalars[output_count] = clip_input_scalar(memory,
                        input, input_attributes, 0u);
                }
                output_count++;
            }

            /* Retail emits the first surviving source vertex first, clips
               edges 0->1 through n-2->n-1, then appends the closing n-1->0
               intersection last.  That ordering is observable by packet
               construction even though a rotated polygon is equivalent. */
            for (current_index = 1; current_index <= input_count;
                    current_index++) {
                int closing_edge = current_index == input_count;
                int32_t source_index = closing_edge ? 0 : current_index;
                uint32_t current_address = clip_input_vertex(memory, input,
                    input_indices, (uint32_t)source_index);
                if (memory->fault) return list_head;
                if (current_address < 0x10000u) {
                    clip_guard_log("vertex", polygon, input_count,
                        current_address);
                    goto clipper_return;
                }
                ClipVertex current = clip_read_vertex(memory, current_address);
                int current_inside = near_z <= current.z;

                if (previous_inside != current_inside) {
                    float fraction;
                    uint32_t intersection;
                    if (output_count >= CLIP_MAX_VERTICES) {
                        memory->fault = 1;
                        return list_head;
                    }
                    if (!vertex_cursor_loaded) {
                        if (!cursors && vertex_cursor_pointer < 0x10000u) {
                            clip_guard_log("vertex-cursor", polygon,
                                input_count, vertex_cursor_pointer);
                            goto clipper_return;
                        }
                        vertex_cursor = clip_cursor_get(memory,vertex_cursor_pointer,
                            cursors?&cursors->vertex:NULL);
                        vertex_cursor_loaded = 1;
                        if (memory->fault) return list_head;
                    }
                    intersection = clip_intersection(memory, vertex_cursor,
                        previous, current, near_z, &fraction);
                    render_arena_record_motion_clip(intersection,previous_address,current_address,fraction,0);
                    output_indices[output_count] = intersection;
                    if ((flags & 1u) != 0u) {
                        if (!input && input_attributes < 0x10000u) {
                            clip_guard_log("uv-input", polygon, input_count,
                                input_attributes);
                            goto clipper_return;
                        }
                        float previous_u = clip_input_uv(memory, input,
                            input_attributes, (uint32_t)(current_index - 1), 0u);
                        float previous_v = clip_input_uv(memory, input,
                            input_attributes, (uint32_t)(current_index - 1), 1u);
                        float current_u = clip_input_uv(memory, input,
                            input_attributes, (uint32_t)source_index, 0u);
                        float current_v = clip_input_uv(memory, input,
                            input_attributes, (uint32_t)source_index, 1u);
                        output_uvs[output_count][0] = clip_lerp(previous_u,
                            current_u, fraction);
                        output_uvs[output_count][1] = clip_lerp(previous_v,
                            current_v, fraction);
                    } else if ((flags & 2u) != 0u) {
                        if (!input && input_attributes < 0x10000u) {
                            clip_guard_log("scalar-input", polygon,
                                input_count, input_attributes);
                            goto clipper_return;
                        }
                        float previous_scalar = clip_input_scalar(memory, input,
                            input_attributes, (uint32_t)(current_index - 1));
                        float current_scalar = clip_input_scalar(memory, input,
                            input_attributes, (uint32_t)source_index);
                        output_scalars[output_count] = clip_lerp(
                            previous_scalar, current_scalar, fraction);
                    }
                    output_count++;
                    vertex_cursor += 16u;
                    clip_cursor_set(memory,vertex_cursor_pointer,
                        cursors?&cursors->vertex:NULL,vertex_cursor);
                    changed = 1;
                }

                if (!closing_edge && current_inside) {
                    if (output_count >= CLIP_MAX_VERTICES) {
                        memory->fault = 1;
                        return list_head;
                    }
                    output_indices[output_count] = current_address;
                    if ((flags & 1u) != 0u) {
                        if (!input && input_attributes < 0x10000u) {
                            clip_guard_log("uv-input", polygon, input_count,
                                input_attributes);
                            goto clipper_return;
                        }
                        output_uvs[output_count][0] = clip_input_uv(memory,
                            input, input_attributes, (uint32_t)source_index, 0u);
                        output_uvs[output_count][1] = clip_input_uv(memory,
                            input, input_attributes, (uint32_t)source_index, 1u);
                    } else if ((flags & 2u) != 0u) {
                        if (!input && input_attributes < 0x10000u) {
                            clip_guard_log("scalar-input", polygon,
                                input_count, input_attributes);
                            goto clipper_return;
                        }
                        output_scalars[output_count] = clip_input_scalar(memory,
                            input, input_attributes, (uint32_t)source_index);
                    }
                    output_count++;
                }
                previous_address = current_address;
                previous = current;
                previous_inside = current_inside;
            }
        }

        if (output_count != 0u) {
            unsigned index;
            if (changed) {
                if (!index_cursor_loaded) {
                    if (!cursors && index_cursor_pointer < 0x10000u) {
                        clip_guard_log("index-cursor", polygon, input_count,
                            index_cursor_pointer);
                        goto clipper_return;
                    }
                    index_cursor = clip_cursor_get(memory,index_cursor_pointer,
                        cursors?&cursors->packet:NULL);
                    index_cursor_loaded = 1;
                    if (memory->fault) return list_head;
                }
                if(packet){packet->count=output_count;packet->vertices=index_cursor;}
                else {game_memory_write_u32(memory, polygon, output_count);
                    game_memory_write_u32(memory, polygon + 4u, index_cursor);}
                for (index = 0; index < output_count; index++)
                    clip_write_index(memory,index_cursor+index*4u,
                        output_indices[index]);
                index_cursor += output_count * 4u;
                clip_cursor_set(memory,index_cursor_pointer,
                    cursors?&cursors->packet:NULL,index_cursor);

                if ((flags & 1u) != 0u) {
                    if (!uv_cursor_loaded) {
                        if (!cursors && uv_cursor_pointer < 0x10000u) {
                            clip_guard_log("uv-cursor", polygon, input_count,
                                uv_cursor_pointer);
                            goto clipper_return;
                        }
                        uv_cursor = clip_cursor_get(memory,uv_cursor_pointer,
                            cursors?&cursors->uv:NULL);
                        uv_cursor_loaded = 1;
                        if (memory->fault) return list_head;
                    }
                    if(packet)packet->attributes=uv_cursor;
                    else game_memory_write_u32(memory, polygon + 16u, uv_cursor);
                    for (index = 0; index < output_count; index++) {
                        clip_write_f32(memory, uv_cursor + index * 8u,
                            output_uvs[index][0]);
                        clip_write_f32(memory, uv_cursor + index * 8u + 4u,
                            output_uvs[index][1]);
                    }
                    uv_cursor += output_count * 8u;
                    clip_cursor_set(memory,uv_cursor_pointer,
                        cursors?&cursors->uv:NULL,uv_cursor);
                }
                if ((flags & 2u) != 0u) {
                    if (!scalar_cursor_loaded) {
                        if (!cursors && scalar_cursor_pointer < 0x10000u) {
                            clip_guard_log("scalar-cursor", polygon,
                                input_count, scalar_cursor_pointer);
                            goto clipper_return;
                        }
                        scalar_cursor = clip_cursor_get(memory,scalar_cursor_pointer,
                            cursors?&cursors->scalar:NULL);
                        scalar_cursor_loaded = 1;
                        if (memory->fault) return list_head;
                    }
                    if(packet)packet->attributes=scalar_cursor;
                    else game_memory_write_u32(memory, polygon + 16u, scalar_cursor);
                    for (index = 0; index < output_count; index++)
                        clip_write_f32(memory, scalar_cursor + index * 4u,
                            output_scalars[index]);
                    scalar_cursor += output_count * 4u;
                    clip_cursor_set(memory,scalar_cursor_pointer,
                        cursors?&cursors->scalar:NULL,scalar_cursor);
                }
            }

            if(packet){if(!render_packet_set_next(polygon,list_head))packet->next=list_head;}
            else game_memory_write_u32(memory, polygon + 12u, list_head);
            list_head = polygon;
            for (index = 0; index < output_count; index++) {
                float z = clip_read_f32(memory, output_indices[index] + 8u);
                if (far_z < z) flags |= 0x400u;
            }
            if(packet && scene_projection_storage_packet(polygon)){
                if(!scene_projection_storage_set_packet_flags(polygon,flags))
                    memory->fault=1;
            }else if(packet)packet->flags=flags;
            else game_memory_write_u32(memory, polygon + 8u, flags);
        }
        if (memory->fault) return list_head;
    }

clipper_return:
    return list_head;
}

GameAddress gameplay_clipper_clip_polygon_inputs(GameMemory *memory,
        GameAddress polygons,int32_t count,const GameplayClipPolygonInput *inputs,
        float near_z,float far_z,GameAddress vertex,GameAddress packet,
        GameAddress uv,GameAddress scalar){
    return clip_polygon_inputs(memory,polygons,count,inputs,near_z,far_z,
        vertex,packet,uv,scalar,NULL);
}

GameAddress gameplay_clipper_clip_polygon_inputs_state(GameMemory *memory,
        GameAddress polygons,int32_t count,const GameplayClipPolygonInput *inputs,
        float near_z,float far_z,GameplayClipCursors *cursors){
    if(!cursors)return 0;
    return clip_polygon_inputs(memory,polygons,count,inputs,near_z,far_z,
        0,0,0,0,cursors);
}

GameAddress gameplay_clipper_clip_polygons(GameMemory *memory,
        GameAddress polygons, int32_t polygon_count, float near_z, float far_z,
        GameAddress vertex_cursor_pointer, GameAddress index_cursor_pointer,
        GameAddress uv_cursor_pointer, GameAddress scalar_cursor_pointer) {
    return gameplay_clipper_clip_polygon_inputs(memory, polygons, polygon_count,
        NULL, near_z, far_z, vertex_cursor_pointer, index_cursor_pointer,
        uv_cursor_pointer, scalar_cursor_pointer);
}

static float list_clip_read_f32(GameMemory *memory, uint32_t address) {
    return clip_read_f32(memory,address);
}

static void list_clip_write_f32(GameMemory *memory, uint32_t address, float value) {
    clip_write_f32(memory,address,value);
}

static float list_clip_lerp(float previous, float current, long double ratio) {
    long double result = ((long double)current - (long double)previous) *
        ratio + (long double)previous;
    volatile float rounded = (float)result;
    return rounded;
}

static void list_clip_copy_attributes(GameMemory *memory, uint32_t attributes,
    uint32_t flags, int source_index, int output_index, float *uv,
    float *scalar) {
    if (flags & 1u) {
        uv[output_index * 2] = list_clip_read_f32(memory,
            attributes + (uint32_t)source_index * 8u);
        uv[output_index * 2 + 1] = list_clip_read_f32(memory,
            attributes + (uint32_t)source_index * 8u + 4u);
    } else if (flags & 2u) {
        scalar[output_index] = list_clip_read_f32(memory,
            attributes + (uint32_t)source_index * 4u);
    }
}

static void list_clip_emit_intersection(GameMemory *memory, uint32_t vertex_array,
    uint32_t attributes, uint32_t flags, int previous_index,
    int current_index, float near_z, uint32_t *vertex_cursor,
    uint32_t *vertices, float *uv, float *scalar, int output_index) {
    uint32_t previous = clip_read_index(memory,
        vertex_array + (uint32_t)previous_index * 4u);
    uint32_t current = clip_read_index(memory,
        vertex_array + (uint32_t)current_index * 4u);
    float previous_z = list_clip_read_f32(memory, previous + 8u);
    float current_z = list_clip_read_f32(memory, current + 8u);
    volatile float rounded_ratio = (float)(
        ((long double)near_z - (long double)previous_z) /
        ((long double)current_z - (long double)previous_z));
    long double ratio = rounded_ratio;
    uint32_t output = *vertex_cursor;
    float previous_x = list_clip_read_f32(memory, previous);
    float current_x = list_clip_read_f32(memory, current);
    float previous_y = list_clip_read_f32(memory, previous + 4u);
    float current_y = list_clip_read_f32(memory, current + 4u);

    list_clip_write_f32(memory, output, list_clip_lerp(previous_x, current_x, ratio));
    list_clip_write_f32(memory, output + 4u,
        list_clip_lerp(previous_y, current_y, ratio));
    list_clip_write_f32(memory, output + 8u, near_z);
    render_arena_record_motion_clip(output,previous,current,rounded_ratio,0);
    vertices[output_index] = output;
    *vertex_cursor = output + 16u;

    if (flags & 1u) {
        float previous_s = list_clip_read_f32(memory,
            attributes + (uint32_t)previous_index * 8u);
        float current_s = list_clip_read_f32(memory,
            attributes + (uint32_t)current_index * 8u);
        float previous_t = list_clip_read_f32(memory,
            attributes + (uint32_t)previous_index * 8u + 4u);
        float current_t = list_clip_read_f32(memory,
            attributes + (uint32_t)current_index * 8u + 4u);
        uv[output_index * 2] = list_clip_lerp(previous_s, current_s, ratio);
        uv[output_index * 2 + 1] = list_clip_lerp(previous_t, current_t, ratio);
    } else if (flags & 2u) {
        float previous_value = list_clip_read_f32(memory,
            attributes + (uint32_t)previous_index * 4u);
        float current_value = list_clip_read_f32(memory,
            attributes + (uint32_t)current_index * 4u);
        scalar[output_index] = list_clip_lerp(previous_value, current_value, ratio);
    }
}

static GameAddress near_list(GameMemory *memory,GameAddress node,float near_z,float far_z,
    GameAddress vertex_cursor_address,GameAddress pointer_cursor_address,GameAddress uv_cursor_address,
    GameAddress scalar_cursor_address,GameplayClipCursors *state,GameplayClipStatus *status) {
    memset(status,0,sizeof(*status));
    uint32_t vertex_cursor = 0u;
    uint32_t pointer_cursor = 0u;
    uint32_t uv_cursor = 0u;
    uint32_t scalar_cursor = 0u;
    int vertex_cursor_loaded = 0;
    int pointer_cursor_loaded = 0;
    int uv_cursor_loaded = 0;
    int scalar_cursor_loaded = 0;
    uint32_t result = 0u;
    int had_intersection = 0;
    int had_far_compare = 0;
    float last_far_z = 0.0f;
    int had_near_compare = 0;
    float last_near_z = 0.0f;

    if (memory->fault) return 0;
    if(!node)return 0;
    while (node) {
        RenderArenaPacket *packet=clip_packet(memory,node);
        uint32_t next=clip_packet_next(memory,node,packet);
        int count=packet?(int32_t)packet->count:(int32_t)game_memory_read_u32(memory,node);
        uint32_t vertex_array=packet?packet->vertices:game_memory_read_u32(memory,node+4u);
        uint32_t flags=packet?packet->flags:game_memory_read_u32(memory,node+8u);
        uint32_t attributes=packet?packet->attributes:game_memory_read_u32(memory,node+16u);
        uint32_t vertices[CLIP_MAX_VERTICES * 2];
        float uv[CLIP_MAX_VERTICES * 4];
        float scalar[CLIP_MAX_VERTICES * 2];
        int output_count = 0;
        int crossed = 0;
        int first_outside;
        int previous_outside;
        int index;

        if (memory->fault) return 0;
        if (count <= 0 || count > (int)CLIP_MAX_VERTICES) {
            node = next;
            continue;
        }

        {
            uint32_t first = clip_read_index(memory, vertex_array);
            last_near_z = list_clip_read_f32(memory, first + 8u);
            had_near_compare = 1;
            first_outside = last_near_z < near_z;
            previous_outside = first_outside;
            if (!first_outside) {
                vertices[output_count] = first;
                list_clip_copy_attributes(memory, attributes, flags, 0,
                    output_count, uv, scalar);
                output_count++;
            }
        }

        for (index = 1; index < count; index++) {
            uint32_t current = clip_read_index(memory,
                vertex_array + (uint32_t)index * 4u);
            int current_outside;
            last_near_z = list_clip_read_f32(memory, current + 8u);
            had_near_compare = 1;
            current_outside = last_near_z < near_z;
            if (current_outside != previous_outside) {
                if (!vertex_cursor_loaded) {
                    vertex_cursor = clip_cursor_get(memory,vertex_cursor_address,
                        state?&state->vertex:NULL);
                    vertex_cursor_loaded = 1;
                }
                list_clip_emit_intersection(memory, vertex_array, attributes, flags,
                    index - 1, index, near_z, &vertex_cursor, vertices, uv,
                    scalar, output_count++);
                crossed = 1;
            }
            if (!current_outside) {
                vertices[output_count] = current;
                list_clip_copy_attributes(memory, attributes, flags, index,
                    output_count, uv, scalar);
                output_count++;
            }
            previous_outside = current_outside;
        }
        if (previous_outside != first_outside) {
            if (!vertex_cursor_loaded) {
                vertex_cursor = clip_cursor_get(memory,vertex_cursor_address,
                    state?&state->vertex:NULL);
                vertex_cursor_loaded = 1;
            }
            list_clip_emit_intersection(memory, vertex_array, attributes, flags,
                count - 1, 0, near_z, &vertex_cursor, vertices, uv, scalar,
                output_count++);
            crossed = 1;
        }
        if (memory->fault) return 0;

        if (output_count > 0) {
            if (crossed) {
                had_intersection = 1;
                if (!pointer_cursor_loaded) {
                    pointer_cursor = clip_cursor_get(memory,pointer_cursor_address,
                        state?&state->packet:NULL);
                    pointer_cursor_loaded = 1;
                }
                if(packet)packet->count=(uint32_t)output_count;else game_memory_write_u32(memory,node,(uint32_t)output_count);
                if(packet)packet->vertices=pointer_cursor;else game_memory_write_u32(memory,node+4u,pointer_cursor);
                for (index = 0; index < output_count; index++)
                    clip_write_index(memory, pointer_cursor + (uint32_t)index * 4u,
                        vertices[index]);
                pointer_cursor += (uint32_t)output_count * 4u;
                if (flags & 1u) {
                    if (!uv_cursor_loaded) {
                        uv_cursor = clip_cursor_get(memory,uv_cursor_address,
                            state?&state->uv:NULL);
                        uv_cursor_loaded = 1;
                    }
                    if(packet)packet->attributes=uv_cursor;else game_memory_write_u32(memory,node+16u,uv_cursor);
                    for (index = 0; index < output_count * 2; index++)
                        list_clip_write_f32(memory, uv_cursor + (uint32_t)index * 4u,
                            uv[index]);
                    uv_cursor += (uint32_t)output_count * 8u;
                }
                if (flags & 2u) {
                    if (!scalar_cursor_loaded) {
                        scalar_cursor = clip_cursor_get(memory,scalar_cursor_address,
                            state?&state->scalar:NULL);
                        scalar_cursor_loaded = 1;
                    }
                    if(packet)packet->attributes=scalar_cursor;else game_memory_write_u32(memory,node+16u,scalar_cursor);
                    for (index = 0; index < output_count; index++)
                        list_clip_write_f32(memory,
                            scalar_cursor + (uint32_t)index * 4u,
                            scalar[index]);
                    scalar_cursor += (uint32_t)output_count * 4u;
                }
            }
            if(packet){if(!render_packet_set_next(node,result))packet->next=result;}
            else game_memory_write_u32(memory,node+12u,result);
            result = node;
            for (index = 0; index < output_count; index++) {
                float vertex_z = list_clip_read_f32(memory, vertices[index] + 8u);
                had_far_compare = 1;
                last_far_z = vertex_z;
                if (far_z < vertex_z) {
                    flags |= 0x400u;
                    if(packet && scene_projection_storage_packet(node)){
                        if(!scene_projection_storage_set_packet_flags(node,flags))
                            memory->fault=1;
                    }else if(packet)packet->flags=flags;
                    else game_memory_write_u32(memory,node+8u,flags);
                }
            }
        }
        if (memory->fault) return 0;
        node = next;
    }

    if (vertex_cursor_loaded)
        clip_cursor_set(memory,vertex_cursor_address,state?&state->vertex:NULL,vertex_cursor);
    if (pointer_cursor_loaded)
        clip_cursor_set(memory,pointer_cursor_address,state?&state->packet:NULL,pointer_cursor);
    if (uv_cursor_loaded)
        clip_cursor_set(memory,uv_cursor_address,state?&state->uv:NULL,uv_cursor);
    if (scalar_cursor_loaded)
        clip_cursor_set(memory,scalar_cursor_address,state?&state->scalar:NULL,scalar_cursor);
    if (memory->fault) return 0;
    status->had_compare=had_far_compare || had_near_compare;
    status->last_coordinate=had_far_compare?last_far_z:last_near_z;
    status->last_boundary=had_far_compare?far_z:near_z;
    status->had_intersection=had_intersection;
    return result;
}

GameAddress gameplay_clipper_near_list(GameMemory *memory,GameAddress head,
    float near_z,float far_z,GameAddress vertex,GameAddress packet,
    GameAddress uv,GameAddress scalar,GameplayClipStatus *status){
    return near_list(memory,head,near_z,far_z,vertex,packet,uv,scalar,NULL,status);
}

GameAddress gameplay_clipper_near_list_state(GameMemory *memory,GameAddress head,
    float near_z,float far_z,GameplayClipCursors *state,GameplayClipStatus *status){
    if(!state)return 0;
    return near_list(memory,head,near_z,far_z,0,0,0,0,state,status);
}

#define SCREEN_CLIP_MAX_VERTICES 25

typedef struct ScreenClipCursors {
    uint32_t vertex_address, pointer_address, uv_address, scalar_address;
    uint32_t vertex, pointer, uv, scalar;
    int vertex_loaded, pointer_loaded, uv_loaded, scalar_loaded;
    GameplayClipCursors *state;
} ScreenClipCursors;

static float screen_read_f32(GameMemory *memory, uint32_t address) {
    return clip_read_f32(memory,address);
}

static void screen_write_f32(GameMemory *memory, uint32_t address, float value) {
    clip_write_f32(memory,address,value);
}

static float screen_lerp(float previous, float current, long double ratio) {
    long double result = ((long double)current - (long double)previous) *
        ratio + (long double)previous;
    volatile float rounded = (float)result;
    return rounded;
}

static int screen_inside(float coordinate, float boundary, unsigned plane) {
    return plane < 2u ? boundary <= coordinate : coordinate <= boundary;
}

static float screen_coordinate(GameMemory *memory, uint32_t vertex,
    unsigned plane) {
    return screen_read_f32(memory, vertex + ((plane & 1u) ? 4u : 0u));
}

static void screen_copy_attributes(GameMemory *memory, uint32_t attributes,
    uint32_t flags, int source_index, int output_index, float *uv,
    float *scalar) {
    if (flags & 1u) {
        uv[output_index * 2] = screen_read_f32(memory,
            attributes + (uint32_t)source_index * 8u);
        uv[output_index * 2 + 1] = screen_read_f32(memory,
            attributes + (uint32_t)source_index * 8u + 4u);
    } else if (flags & 2u) {
        scalar[output_index] = screen_read_f32(memory,
            attributes + (uint32_t)source_index * 4u);
    }
}

static void screen_emit_intersection(GameMemory *memory, uint32_t vertex_array,
    uint32_t attributes, uint32_t flags, int previous_index,
    int current_index, float boundary, unsigned plane,
    uint32_t *vertex_cursor, uint32_t *vertices, float *uv, float *scalar,
    int output_index) {
    uint32_t previous = clip_read_index(memory,
        vertex_array + (uint32_t)previous_index * 4u);
    uint32_t current = clip_read_index(memory,
        vertex_array + (uint32_t)current_index * 4u);
    float previous_coordinate = screen_coordinate(memory, previous, plane);
    float current_coordinate = screen_coordinate(memory, current, plane);
    volatile float rounded_ratio = (float)(
        ((long double)boundary - (long double)previous_coordinate) /
        ((long double)current_coordinate - (long double)previous_coordinate));
    long double ratio = rounded_ratio;
    uint32_t output = *vertex_cursor;
    unsigned component;

    for (component = 0; component < 4u; component++) {
        if (component == (plane & 1u))
            screen_write_f32(memory, output + component * 4u, boundary);
        else
            screen_write_f32(memory, output + component * 4u,
                screen_lerp(screen_read_f32(memory, previous + component * 4u),
                    screen_read_f32(memory, current + component * 4u), ratio));
    }
    render_arena_record_motion_clip(output,previous,current,rounded_ratio,1);
    vertices[output_index] = output;
    *vertex_cursor = output + 16u;

    if (flags & 1u) {
        float previous_s = screen_read_f32(memory,
            attributes + (uint32_t)previous_index * 8u);
        float current_s = screen_read_f32(memory,
            attributes + (uint32_t)current_index * 8u);
        float previous_t = screen_read_f32(memory,
            attributes + (uint32_t)previous_index * 8u + 4u);
        float current_t = screen_read_f32(memory,
            attributes + (uint32_t)current_index * 8u + 4u);
        uv[output_index * 2] = screen_lerp(previous_s, current_s, ratio);
        uv[output_index * 2 + 1] = screen_lerp(previous_t, current_t, ratio);
    } else if (flags & 2u) {
        float previous_value = screen_read_f32(memory,
            attributes + (uint32_t)previous_index * 4u);
        float current_value = screen_read_f32(memory,
            attributes + (uint32_t)current_index * 4u);
        scalar[output_index] = screen_lerp(previous_value, current_value,
            ratio);
    }
}

static int screen_clip_plane(GameMemory *memory, uint32_t node,
    float boundary, unsigned plane, ScreenClipCursors *cursors,
    GameplayClipStatus *status) {
    RenderArenaPacket *packet=clip_packet(memory,node);
    int count=packet?(int32_t)packet->count:(int32_t)game_memory_read_u32(memory,node);
    uint32_t vertex_array=packet?packet->vertices:game_memory_read_u32(memory,node+4u);
    uint32_t flags=packet?packet->flags:game_memory_read_u32(memory,node+8u);
    uint32_t attributes=packet?packet->attributes:game_memory_read_u32(memory,node+16u);
    uint32_t vertices[SCREEN_CLIP_MAX_VERTICES * 2];
    float uv[SCREEN_CLIP_MAX_VERTICES * 4];
    float scalar[SCREEN_CLIP_MAX_VERTICES * 2];
    int output_count = 0;
    int crossed = 0;
    int first_outside;
    int previous_outside;
    int index;

    if (memory->fault || count <= 0 || count > SCREEN_CLIP_MAX_VERTICES)
        return 0;
    {
        uint32_t first = clip_read_index(memory, vertex_array);
        float coordinate = screen_coordinate(memory, first, plane);
        status->last_coordinate = coordinate;
        status->last_boundary = boundary;
        status->had_compare = 1;
        first_outside = !screen_inside(coordinate, boundary, plane);
        previous_outside = first_outside;
        if (!first_outside) {
            vertices[output_count] = first;
            screen_copy_attributes(memory, attributes, flags, 0,
                output_count, uv, scalar);
            output_count++;
        }
    }
    for (index = 1; index < count; index++) {
        uint32_t current = clip_read_index(memory,
            vertex_array + (uint32_t)index * 4u);
        float coordinate = screen_coordinate(memory, current, plane);
        int current_outside;
        status->last_coordinate = coordinate;
        status->last_boundary = boundary;
        status->had_compare = 1;
        current_outside = !screen_inside(coordinate, boundary, plane);
        if (current_outside != previous_outside) {
            if (!cursors->vertex_loaded) {
                cursors->vertex = clip_cursor_get(memory,cursors->vertex_address,
                    cursors->state?&cursors->state->vertex:NULL);
                cursors->vertex_loaded = 1;
            }
            screen_emit_intersection(memory, vertex_array, attributes, flags,
                index - 1, index, boundary, plane, &cursors->vertex, vertices,
                uv, scalar, output_count++);
            crossed = 1;
        }
        if (!current_outside) {
            vertices[output_count] = current;
            screen_copy_attributes(memory, attributes, flags, index,
                output_count, uv, scalar);
            output_count++;
        }
        previous_outside = current_outside;
    }
    if (previous_outside != first_outside) {
        if (!cursors->vertex_loaded) {
            cursors->vertex = clip_cursor_get(memory,cursors->vertex_address,
                cursors->state?&cursors->state->vertex:NULL);
            cursors->vertex_loaded = 1;
        }
        screen_emit_intersection(memory, vertex_array, attributes, flags,
            count - 1, 0, boundary, plane, &cursors->vertex, vertices, uv,
            scalar, output_count++);
        crossed = 1;
    }
    if (memory->fault || output_count == 0) return 0;
    if (crossed) {
        status->had_intersection = 1;
        if (!cursors->pointer_loaded) {
            cursors->pointer = clip_cursor_get(memory,cursors->pointer_address,
                cursors->state?&cursors->state->packet:NULL);
            cursors->pointer_loaded = 1;
        }
        if(packet)packet->count=(uint32_t)output_count;else game_memory_write_u32(memory,node,(uint32_t)output_count);
        if(packet)packet->vertices=cursors->pointer;else game_memory_write_u32(memory,node+4u,cursors->pointer);
        for (index = 0; index < output_count; index++)
            clip_write_index(memory,
                cursors->pointer + (uint32_t)index * 4u, vertices[index]);
        cursors->pointer += (uint32_t)output_count * 4u;
        if (flags & 1u) {
            if (!cursors->uv_loaded) {
                cursors->uv = clip_cursor_get(memory,cursors->uv_address,
                    cursors->state?&cursors->state->uv:NULL);
                cursors->uv_loaded = 1;
            }
            if(packet)packet->attributes=cursors->uv;else game_memory_write_u32(memory,node+16u,cursors->uv);
            for (index = 0; index < output_count * 2; index++)
                screen_write_f32(memory, cursors->uv + (uint32_t)index * 4u,
                    uv[index]);
            cursors->uv += (uint32_t)output_count * 8u;
        } else if (flags & 2u) {
            if (!cursors->scalar_loaded) {
                cursors->scalar = clip_cursor_get(memory,cursors->scalar_address,
                    cursors->state?&cursors->state->scalar:NULL);
                cursors->scalar_loaded = 1;
            }
            if(packet)packet->attributes=cursors->scalar;else game_memory_write_u32(memory,node+16u,cursors->scalar);
            for (index = 0; index < output_count; index++)
                screen_write_f32(memory,
                    cursors->scalar + (uint32_t)index * 4u, scalar[index]);
            cursors->scalar += (uint32_t)output_count * 4u;
        }
    }
    return !memory->fault;
}

static GameAddress screen_chain(GameMemory *memory,GameAddress node,GameAddress *tail,
    const float boundaries[4],GameAddress vertex_address,GameAddress pointer_address,
    GameAddress uv_address,GameAddress scalar_address,GameplayClipCursors *state,
    GameplayClipStatus *output_status) {
    ScreenClipCursors cursors;
    GameplayClipStatus status;
    uint32_t result_head = 0u;
    uint32_t result_tail = 0u;
    unsigned plane;

    memset(&cursors, 0, sizeof(cursors));
    memset(&status, 0, sizeof(status));
    cursors.vertex_address=vertex_address;cursors.pointer_address=pointer_address;
    cursors.uv_address=uv_address;cursors.scalar_address=scalar_address;
    cursors.state=state;
    if (memory->fault) return 0;
    while (node) {
        RenderArenaPacket *packet=clip_packet(memory,node);
        uint32_t next=clip_packet_next(memory,node,packet);
        int visible = 1;
        for (plane = 0; plane < 4u; plane++) {
            if (!screen_clip_plane(memory, node, boundaries[plane], plane,
                    &cursors, &status)) {
                visible = 0;
                break;
            }
        }
        if (memory->fault) return 0;
        if (visible) {
            if(packet){if(!render_packet_set_next(node,result_head))packet->next=result_head;}
            else game_memory_write_u32(memory,node+12u,result_head);
            result_head = node;
            if (!result_tail) result_tail = node;
        }
        node = next;
    }
    *tail=result_tail;
    if (cursors.vertex_loaded)
        clip_cursor_set(memory,cursors.vertex_address,
            state?&state->vertex:NULL,cursors.vertex);
    if (cursors.pointer_loaded)
        clip_cursor_set(memory,cursors.pointer_address,
            state?&state->packet:NULL,cursors.pointer);
    if (cursors.uv_loaded)
        clip_cursor_set(memory,cursors.uv_address,state?&state->uv:NULL,cursors.uv);
    if (cursors.scalar_loaded)
        clip_cursor_set(memory,cursors.scalar_address,
            state?&state->scalar:NULL,cursors.scalar);
    if (memory->fault) return 0;
    *output_status=status;
    return result_head;
}

GameAddress gameplay_clipper_screen_chain(GameMemory *memory,GameAddress node,GameAddress *tail,
    const float bounds[4],GameAddress vertex,GameAddress packet,GameAddress uv,
    GameAddress scalar,GameplayClipStatus *status){
    return screen_chain(memory,node,tail,bounds,vertex,packet,uv,scalar,NULL,status);
}

GameAddress gameplay_clipper_screen_chain_state(GameMemory *memory,GameAddress node,
    GameAddress *tail,const float bounds[4],GameplayClipCursors *state,
    GameplayClipStatus *status){
    if(!state)return 0;
    return screen_chain(memory,node,tail,bounds,0,0,0,0,state,status);
}

void gameplay_clipper_screen_list(GameMemory *memory,GameAddress head_address,GameAddress tail_address,
    const float boundaries[4],GameAddress vertex_address,GameAddress pointer_address,
    GameAddress uv_address,GameAddress scalar_address,GameplayClipStatus *status) {
    GameAddress tail=0,head=gameplay_clipper_screen_chain(memory,game_memory_read_u32(memory,head_address),&tail,
        boundaries,vertex_address,pointer_address,uv_address,scalar_address,status);
    if(memory->fault)return;
    game_memory_write_u32(memory,head_address,head);game_memory_write_u32(memory,tail_address,tail);
}
