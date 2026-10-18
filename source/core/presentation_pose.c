#include "presentation_pose.h"
#include "gameplay_geometry.h"
#include "scene_instance_owner.h"
#include "effect_model.h"
#include <math.h>
#include <string.h>

int presentation_pose_interpolate(GameMemory *m,PresentationPose *p){
    uint32_t now,word;int32_t elapsed,remaining;float fraction,limit;long double duration;unsigned i;
    if(!m || !p || m->fault)return 0;
    now=game_memory_read_u32(m,0x5acbe8);word=now-p->local_phase;memcpy(&elapsed,&word,4);
    p->local_phase=now;
    word=game_memory_read_u32(m,0x5acc80)-now;memcpy(&remaining,&word,4);
    word=game_memory_read_u32(m,0x5acc38);memcpy(&fraction,&word,4);
    word=game_memory_read_u32(m,0x576594);memcpy(&limit,&word,4);
    if(m->fault)return 0;
    duration=(long double)remaining+fraction;
    /*Retail's unordered comparison also selects the snap branch. */
    if(elapsed>10 || !(duration>limit)){
        memcpy(p->local_position,p->position,sizeof(p->position));
        for(i=0;i<3;i++)p->local_angles[2-i]=p->angles[i];
        return 1;
    }
    fraction=(float)duration;fraction=(float)((long double)limit/fraction);
    for(i=0;i<3;i++){
        uint16_t wrapped=(uint16_t)((p->angles[i]-p->local_angles[2-i])&2047u);
        int32_t delta=wrapped>1024?(int32_t)wrapped-2048:wrapped;
        long double step=(long double)delta*fraction;int64_t n;
        p->local_position[i]=(float)(((long double)p->position[i]-p->local_position[i])*fraction+p->local_position[i]);
        n=!isfinite(step) || step< -0x1p63L || step>=0x1p63L?INT64_MIN:(int64_t)step;
        wrapped=(uint16_t)((p->local_angles[2-i]+(uint16_t)n)&2047u);
        p->local_angles[2-i]=(i && wrapped>1024)?(uint16_t)(wrapped-2048):wrapped;
    }
    return 1;
}

static void *segment(PresentationPose *pose, uint32_t offset,
        uint32_t *begin, size_t *length) {
#define FIELD(at, member) do { if (offset - (at) < sizeof(pose->member)) { \
    *begin = (at); *length = sizeof(pose->member); return &pose->member; } } while (0)
    switch (offset >> 2u) {
    case 4u: FIELD(0x10u, flags); break;
    case 5u: FIELD(0x14u, identity); break;
    case 6u: case 7u: case 8u: FIELD(0x18u, position); break;
    case 9u: case 10u: FIELD(0x24u, angles); FIELD(0x2au, angle_padding); break;
    case 11u: case 12u: case 13u: FIELD(0x2cu, dimensions); break;
    case 14u: FIELD(0x38u, radius); break;
    case 15u: FIELD(0x3cu, hull); break;
    case 16u: FIELD(0x40u, vertices); break;
    case 17u: FIELD(0x44u, vertex_count); FIELD(0x46u, count_padding); break;
    case 21u: FIELD(0x54u, model); break;
    case 22u: case 23u: case 24u: FIELD(0x58u, properties); break;
    case 25u: case 26u: case 27u: FIELD(0x64u, local_position); break;
    case 28u: FIELD(0x70u, controller); break;
    case 29u: case 30u: FIELD(0x74u, local_angles); FIELD(0x7au, local_angle_padding); break;
    case 31u: FIELD(0x7cu, local_phase); break;
    case 32u: case 33u: case 34u: case 35u: case 36u: case 37u:
    case 38u: case 39u: case 40u: case 41u: case 42u: case 43u:
        FIELD(0x80u, world_matrix); break;
    default: break;
    }
#undef FIELD
    return NULL;
}
static int transfer(PresentationPose *pose, uint32_t offset, void *bytes,
        size_t size, int writing) {
    uint32_t check = offset;
    size_t remaining = size;
    uint8_t *cursor = bytes;
    if (!pose || !bytes || offset > 0xb0u || size > 0xb0u - offset) return 0;
    if (size) {
        uint32_t begin;
        size_t length;
        uint8_t *field = segment(pose, offset, &begin, &length);
        if (!field) return 0;
        if (size <= length - (offset - begin)) {
            if (writing) memcpy(field + offset - begin, bytes, size);
            else memcpy(bytes, field + offset - begin, size);
            return 1;
        }
    }
    /* Preflight prevents a mixed/unknown span from partially changing state. */
    while (remaining) {
        uint32_t begin;
        size_t length, step;
        if (!segment(pose, check, &begin, &length)) return 0;
        step = length - (check - begin); if (step > remaining) step = remaining;
        check += (uint32_t)step; remaining -= step;
    }
    while (size) {
        uint32_t begin;
        size_t length, step;
        uint8_t *field = segment(pose, offset, &begin, &length);
        step = length - (offset - begin); if (step > size) step = size;
        if (writing) memcpy(field + offset - begin, cursor, step);
        else memcpy(cursor, field + offset - begin, step);
        offset += (uint32_t)step; cursor += step; size -= step;
    }
    return 1;
}
int presentation_pose_projection_read(PresentationPose *pose, uint32_t offset,
        void *bytes, size_t size) { return transfer(pose, offset, bytes, size, 0); }
int presentation_pose_projection_write(PresentationPose *pose, uint32_t offset,
        const void *bytes, size_t size) {
    GameAddress previous=pose?pose->model:0u;
    int ok=transfer(pose,offset,(void *)bytes,size,1);
    if(ok && previous!=pose->model)pose->model_instance=scene_instance_owner_find(pose->model);
    return ok;
}

int presentation_pose_import_prefix(GameMemory *memory,GameAddress object,
        PresentationPose *pose,unsigned extent) {
    uint32_t offset;
    PresentationPose imported;
    if (!memory || !pose) return 0;
    memset(&imported, 0, sizeof(imported));
    if(!extent)extent=0xb0u;
    if(extent!=0x64u && extent!=0x80u && extent!=0xb0u)return 0;
    for (offset = 0u; offset < extent; offset += 4u) {
        uint32_t unused_begin, value;
        size_t unused_size;
        if (!segment(&imported, offset, &unused_begin, &unused_size)) continue;
        value = game_memory_read_u32(memory, object + offset);
        if (memory->fault || !presentation_pose_projection_write(&imported,
                offset, &value, 4u)) return 0;
    }
    *pose = imported;
    return 1;
}
int presentation_pose_import(GameMemory *memory,GameAddress object,PresentationPose *pose){
    return presentation_pose_import_prefix(memory,object,pose,0xb0u);
}
static float read_float(GameMemory *memory, GameAddress address) {
    uint32_t bits = game_memory_read_u32(memory, address);
    float value; memcpy(&value, &bits, 4u); return value;
}
/*483290 borrows482390's transformed points. Other target methods share the
 *ordinary entity-position query, including vehicle and parent/descriptor
 *overrides. Record/model owners retain allocation and retirement. */
int presentation_target_position(GameMemory *m,GameAddress object,const float from[3],float out[3]){
    GameAddress method,points;uint32_t count,index=0,i;unsigned axis;
    float origin[3],best=0;long double delta[3],distance;
    if(!m || !object || !out || m->fault)return 0;
    method=game_memory_read_u32(m,game_memory_read_u32(m,object)+0x38u);
    if(method!=0x483290u)return gameplay_geometry_entity_position(m,object,out);
    if(!from)return 0;
    memcpy(origin,from,sizeof(origin));
    points=game_memory_read_u32(m,object+0x94u);count=game_memory_read_u32(m,object+0x98u);
    if(!points || !count || count>32767u || (uint64_t)points+(uint64_t)count*12u>UINT32_MAX)return 0;
    for(i=0;i<count;i++){
        for(axis=0;axis<3;axis++)delta[axis]=(long double)origin[axis]-read_float(m,points+i*12u+axis*4u);
        distance=(delta[1]*delta[1]+delta[2]*delta[2])+delta[0]*delta[0];
        /*Compare extended distance with the previous binary32 minimum. The
         *original unordered comparison also selects the new point. */
        if(!i || !(distance>=best)){index=i;best=(float)distance;}
    }
    for(axis=0;axis<3;axis++)out[axis]=read_float(m,points+index*12u+axis*4u);
    return !m->fault;
}
void presentation_pose_build_billboard(PresentationPose *pose){
    unsigned row;
    memset(pose->world_matrix,0,sizeof(pose->world_matrix));
    for(row=0;row<3u;++row){
        pose->world_matrix[row*4u+row]=1.0f;
        pose->world_matrix[row*4u+3u]=pose->position[row];
    }
}
int presentation_pose_build_billboard_controller(GameMemory *memory,PresentationPose *pose){
    float local[12]={0};unsigned row,i;
    EffectModel *model=effect_model_find(pose->model);
    for(row=0;row<3u;++row){local[row*4u+row]=1.0f;local[row*4u+3u]=pose->local_position[row];}
    if(model)memcpy(model->local,local,sizeof(local));
    else for(i=0;i<12u;++i){uint32_t bits;memcpy(&bits,&local[i],4u);game_memory_write_u32(memory,pose->model+4u+i*4u,bits);}
    return !memory->fault;
}
static void write_float(GameMemory *memory, GameAddress address, float value) {
    uint32_t bits; memcpy(&bits, &value, 4u); game_memory_write_u32(memory, address, bits);
}
int presentation_physical_drag(GameMemory *m,GameAddress object){
    float drag;unsigned i;
    if(!m || m->fault)return 0;
    drag=read_float(m,0x571b84);
    for(i=0;i<2;i++){
        GameAddress field=object+0xb4+i*8;uint32_t bits=game_memory_read_u32(m,field);float velocity;
        memcpy(&velocity,&bits,4);
        /*The original tests the signed bits, including both zero encodings.
         *It permits overshoot; clamping here changes the subsequent flight. */
        write_float(m,field,(float)((long double)velocity+(bits && bits<=INT32_MAX?-(long double)drag:(long double)drag)));
    }
    write_float(m,object+0xb8,(float)((long double)read_float(m,object+0xb8)-drag));
    return !m->fault;
}
static int pose_export_prefix(GameMemory *memory,GameAddress object,PresentationPose *pose,unsigned extent){
    uint32_t offset;
    if(!extent)extent=0xb0u;
    for (offset = 0u; offset < extent && !memory->fault; offset += 4u) {
        uint32_t value;
        if (presentation_pose_projection_read(pose, offset, &value, 4u))
            game_memory_write_u32(memory, object + offset, value);
    }
    return !memory->fault;
}
int presentation_pose_export(GameMemory *memory,GameAddress object,PresentationPose *pose){
    return pose_export_prefix(memory,object,pose,0xb0u);
}
int presentation_pose_build_world(GameMemory *memory, PresentationPose *pose) {
    if (!memory || !pose) return 0;
    gameplay_geometry_orientation_values(memory, pose->angles,
        pose->position, pose->world_matrix);
    return !memory->fault;
}
int presentation_pose_build_controller(GameMemory *memory, PresentationPose *pose) {
    float matrix[12];
    uint16_t angles[3];
    unsigned i;
    if (!memory || !pose) return 0;
    /* Controller storage reverses the world yaw/roll order. Placement's
     * local-pose copy performs the same reversal. */
    angles[0] = pose->local_angles[2]; angles[1] = pose->local_angles[1];
    angles[2] = pose->local_angles[0];
    gameplay_geometry_orientation_values(memory, angles,
        pose->local_position, matrix);
    if (memory->fault) return 0;
    if(pose->model_instance){
        memcpy(pose->model_instance->local,matrix,sizeof(matrix));
        return scene_projection_storage_project_holder(pose->model_instance);
    }
    for (i = 0u; i < 12u && !memory->fault; ++i)
        write_float(memory, pose->model + 4u + i * 4u, matrix[i]);
    return !memory->fault;
}
static int transform_vertices(GameMemory *memory,
        PresentationPose *pose, int controller,int laser) {
    uint32_t cached = controller ? 0x00400000u : 0x00800000u;
    uint32_t mark = controller ? 0x00040000u : 0x00080000u;
    int32_t count;
    float matrix[12];
    const ModelHullSphere *hull;
    unsigned vertex, row;
    if (!memory || !pose) return 0;
    if (!laser && (pose->flags & cached)) return 1;
    count = (int16_t)pose->vertex_count;
    if (count > 0) {
        hull=scene_projection_storage_hull(pose->hull,(size_t)count);
        if(controller && pose->model_instance)memcpy(matrix,pose->model_instance->local,sizeof(matrix));
        else if (controller)
            for (row = 0u; row < 12u; ++row)
                matrix[row] = read_float(memory, pose->model + 4u + row * 4u);
        else memcpy(matrix, pose->world_matrix, sizeof(matrix));
        for (vertex = 0u; vertex < (uint32_t)count && !memory->fault; ++vertex) {
            float point[3], output[3];
            if(hull)memcpy(point,hull[vertex].position,sizeof(point));
            else for (row = 0u; row < 3u; ++row)
                point[row] = read_float(memory, pose->hull + vertex * 28u + 12u + row * 4u);
            if (memory->fault) return 0;
            for (row = 0u; row < 3u; ++row) {
                unsigned base = row * 4u;
                long double x = (long double)point[0] * matrix[base];
                long double y = (long double)point[1] * matrix[base + 1u];
                long double z = (long double)point[2] * matrix[base + 2u];
                long double value = laser ? (x+y)+z : controller ? (z + y) + x :
                    row == 0u ? (x + z) + y : (x + y) + z;
                output[row] = (float)(value + matrix[base + 3u]);
            }
            for (row = 0u; row < 3u; ++row)
                write_float(memory, (controller ? pose->controller : pose->vertices) +
                    vertex * 12u + row * 4u, output[row]);
        }
    }
    if (memory->fault) return 0;
    if (!laser && (pose->flags & mark)) pose->flags |= cached;
    return 1;
}
int presentation_pose_transform_vertices(GameMemory *memory,PresentationPose *pose,int controller){
    return transform_vertices(memory,pose,controller,0);
}
int presentation_pose_transform_infantry_vertices(GameMemory *memory,GameAddress object,PresentationPose *pose){
    unsigned row,vertex;float point[3];
    if(!memory || !pose || memory->fault)return 0;
    if(pose->flags&0x800000)return 1;
    /*544260's vertex and probe sums use different evaluation orders. */
    for(vertex=0;vertex<(unsigned)((int16_t)pose->vertex_count>0?pose->vertex_count:0);vertex++){
        float v[3];for(row=0;row<3;row++)v[row]=read_float(memory,pose->hull+vertex*28+12+row*4);
        for(row=0;row<3;row++){const float *m=pose->world_matrix+row*4;
            float result=(float)(((long double)m[2]*v[2]+(long double)m[1]*v[1])+(long double)m[0]*v[0]+m[3]);
            write_float(memory,pose->vertices+vertex*12+row*4,result);
        }
    }
    for(row=0;row<3;row++)point[row]=read_float(memory,object+0x190+row*4);
    for(row=0;row<3;row++){const float *m=pose->world_matrix+row*4;
        long double x=(long double)m[0]*point[0],y=(long double)m[1]*point[1],z=(long double)m[2]*point[2];
        write_float(memory,object+0x184+row*4,(float)(((z+x)+y)+m[3]));
    }
    if(memory->fault)return 0;
    if(pose->flags&0x80000)pose->flags|=0x800000;
    return 1;
}
/*The two laser matrices preserve different binary32 intermediates. These
 *roundings are observable in the original world/controller consumers. */
static int laser_matrix(GameMemory *memory,const uint16_t angles[3],const float position[3],float out[12],int controller){
    uint32_t a=angles[0]&2047u,b=angles[1]&2047u,c=angles[2]&2047u;
    float sa=read_float(memory,0x6298a8u+a*4u),sb=read_float(memory,0x6298a8u+b*4u);
    float sc=read_float(memory,0x6298a8u+c*4u),ca=read_float(memory,0x6298a8u+((a+512u)&2047u)*4u);
    float cb=read_float(memory,0x6298a8u+((b+512u)&2047u)*4u),cc=read_float(memory,0x6298a8u+((c+512u)&2047u)*4u);
    long double sc_sb=(long double)sc*sb;
    float saved=controller?(float)sc_sb:(float)((long double)sc*ca);
    out[0]=(float)((long double)cc*ca+(controller?(long double)saved:sc_sb)*sa);
    out[1]=(float)(((long double)sb*sa)*cc-(controller?(long double)sc*ca:saved));
    out[2]=(float)((long double)sa*cb);
    out[4]=(float)((long double)sc*cb);out[5]=(float)((long double)cc*cb);out[6]=-sb;
    out[8]=(float)((long double)saved*(controller?ca:sb)-(long double)sa*cc);
    out[9]=(float)((long double)sa*sc+((long double)sb*cc)*ca);
    out[10]=(float)((long double)cb*ca);
    out[3]=position[0];out[7]=position[1];out[11]=position[2];return !memory->fault;
}
/*430b10 caches rotation, but refreshes translation on every placement/update. */
int presentation_pose_build_laser_world(GameMemory *memory,PresentationPose *pose){
    unsigned i;
    if(!(pose->flags&0x80000u)){
        if(!laser_matrix(memory,pose->angles,pose->position,pose->world_matrix,0))return 0;
        pose->flags|=0x80000u;
    }
    for(i=0;i<3u;i++)pose->world_matrix[i*4u+3u]=pose->position[i];
    return !memory->fault;
}
/*430c90 adds a 128-unit clock phase to the local roll. The existing model
 *owner remains authoritative; no second matrix or execution context is kept. */
int presentation_pose_build_laser_controller(GameMemory *memory,PresentationPose *pose){
    uint16_t angles[3]={pose->local_angles[2],pose->local_angles[1],
        (uint16_t)(pose->local_angles[0]+(game_memory_read_u32(memory,0x5acbe8u)<<7u))};
    float matrix[12];unsigned i;
    if(!laser_matrix(memory,angles,pose->local_position,matrix,1))return 0;
    if(pose->model_instance){
        memcpy(pose->model_instance->local,matrix,sizeof(matrix));
        return scene_projection_storage_project_holder(pose->model_instance);
    }
    for(i=0;i<12u;i++)write_float(memory,pose->model+4u+i*4u,matrix[i]);
    return !memory->fault;
}
/*419ee0 rounds sc*sb before the first row;41a050 retains the wide product
 *there and rounds the third row, like the shared controller. A falling pickup
 *invalidates its caches until the grounded pose stabilizes. */
int presentation_pose_build_spawn_world(GameMemory *m,PresentationPose *p,int falling){
    if(!m || !p || m->fault)return 0;
    if(p->flags&0x80000u)return 1;
    if(!laser_matrix(m,p->angles,p->position,p->world_matrix,1))return 0;
    if(!falling)p->flags|=0x80000u;
    return 1;
}
int presentation_pose_build_spawn_controller(GameMemory *m,PresentationPose *p,int falling){
    uint16_t angles[3];float matrix[12];unsigned i;int same=1;
    if(!m || !p || m->fault)return 0;
    if(p->flags&0x40000u)return 1;
    for(i=0;i<3;i++)angles[i]=p->local_angles[2-i];
    gameplay_geometry_orientation_values(m,angles,p->local_position,matrix);
    if(m->fault)return 0;
    if(p->model_instance){memcpy(p->model_instance->local,matrix,sizeof(matrix));
        if(!scene_projection_storage_project_holder(p->model_instance))return 0;
    }else for(i=0;i<12;i++)write_float(m,p->model+4+i*4,matrix[i]);
    /*Original equality tests accept unordered floats as equal (C2 and C3).
     *Finite mismatches, or any different angle, keep the controller dirty. */
    for(i=0;i<3;i++)if((!isnan(p->position[i]) && !isnan(p->local_position[i]) &&
        p->position[i]!=p->local_position[i]) || p->angles[i]!=angles[i])same=0;
    if(!falling && same)p->flags|=0x40000u;
    return !m->fault;
}
int presentation_pose_transform_laser_vertices(GameMemory *memory,PresentationPose *pose){
    return transform_vertices(memory,pose,0,1);
}
static int placement_refresh(GameMemory *memory, GameAddress object,
        PresentationPose *pose, const PresentationPlacementServices *services,
        unsigned operation) {
    int ok;
    if (services->override_mask & (1u << operation)) {
        if (!pose_export_prefix(memory, object, pose, services->projection_extent) ||
            !services->refresh_override ||
            !services->refresh_override(services->context, operation)) return 0;
        /* Overrides may change pose/cache state through their ABI views. */
        return presentation_pose_import_prefix(memory, object, pose, services->projection_extent);
    }
    ok = operation == 0u ? presentation_pose_build_world(memory, pose) :
        operation == 1u ? presentation_pose_build_controller(memory, pose) :
        presentation_pose_transform_vertices(memory, pose, operation == 3u);
    return ok && presentation_pose_export(memory, object, pose);
}
int presentation_pose_place(GameMemory *memory, GameAddress object,
        PresentationPose *pose, const PresentationPlacement *request,
        const PresentationPlacementServices *services) {
    unsigned i;
    if (!memory || !pose || !request || !services || !services->property ||
        !services->register_spatial) return 0;
    pose->identity = request->identity;
    game_memory_write_u32(memory, object + 0x14u, pose->identity);
    game_memory_write_u32(memory, pose->model + 0x34u, request->descriptor);
    game_memory_write_u32(memory, pose->model + 0x38u, request->descriptor);
    if (memory->fault) return 0;
    memcpy(pose->position, request->position, sizeof(pose->position));
    for (i = 0u; i < 3u; ++i) pose->angles[i] = request->angles[i] & 2047u;
    if (!pose_export_prefix(memory, object, pose, services->projection_extent) ||
        !placement_refresh(memory, object, pose, services, 0u) ||
        !placement_refresh(memory, object, pose, services, 2u)) return 0;
    for (i = 0u; i < 3u; ++i) {
        uint32_t value;
        if (!services->property(services->context, i, &value)) return 0;
        if (!presentation_pose_import_prefix(memory, object, pose, services->projection_extent)) return 0;
        pose->properties[i] = value;
        game_memory_write_u32(memory, object + 0x58u + i * 4u, value);
        if (memory->fault) return 0;
    }
    if (!services->register_spatial(services->context)) return 0;
    if (!presentation_pose_import_prefix(memory, object, pose, services->projection_extent)) return 0;
    if (request->controller) {
        memcpy(pose->local_position, pose->position, sizeof(pose->position));
        for (i = 0u; i < 3u; ++i) pose->local_angles[i] = pose->angles[2u - i];
        pose->local_phase = 0u;
        if (!pose_export_prefix(memory, object, pose, services->projection_extent) ||
            !placement_refresh(memory, object, pose, services, 1u) ||
            !placement_refresh(memory, object, pose, services, 3u)) return 0;
        game_memory_write_u32(memory, object + 0xb0u, 0u);
    }
    return !memory->fault;
}
