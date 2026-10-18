#include "scene_descriptor_clone.h"
#include "scene_projection_storage.h"
#include <stdlib.h>
#include <string.h>
typedef SceneDescriptorState SceneDescriptorView;
typedef char SceneDescriptorViewSize[(sizeof(SceneDescriptorView)==0x4cu)?1:-1];

static void fail(GameMemory *memory,uint32_t address,size_t size,int write) {
    memory->fault=1;
    memory->fault_address=address;
    memory->fault_size=size;
    memory->fault_write=write;
}

typedef struct DescriptorTransaction { uint32_t pages[16]; unsigned count; } DescriptorTransaction;

static uint32_t page(DescriptorTransaction *transaction, GameMemory *memory,uint64_t bytes,const char *purpose) {
    uint32_t address;
    if(!bytes || memory->fault)return 0;
    if(transaction->count == 16u){fail(memory,0,(size_t)bytes,1);return 0;}
    if(bytes>UINT32_MAX){fail(memory,0,(size_t)bytes,1);return 0;}
    address=scene_projection_storage_allocate((size_t)bytes,purpose);
    if(!address)fail(memory,0,(size_t)bytes,1);
    else transaction->pages[transaction->count++]=address;
    return address;
}

static SceneDescriptorView load_descriptor(GameMemory *memory,uint32_t source) {
    SceneDescriptorView view={0};uint32_t words[19];unsigned i;
    for(i=0;i<19 && !memory->fault;i++)
        words[i]=game_memory_read_u32(memory,source+i*4u);
    if(!memory->fault)memcpy(&view,words,sizeof(view));
    return view;
}

static void store_descriptor(GameMemory *memory,uint32_t address,
    const SceneDescriptorView *view) {
    uint32_t words[19];unsigned i;
    memcpy(words,view,sizeof(words));
    for(i=0;i<19 && !memory->fault;i++)
        game_memory_write_u32(memory,address+i*4u,words[i]);
}

static uint32_t copy_name(DescriptorTransaction *transaction,GameMemory *memory,uint32_t source) {
    char bytes[512];uint32_t size=0,address,i;
    do {
        bytes[size]=(char)game_memory_read_u8(memory,source+size);
        if(memory->fault)return 0;
        if(!bytes[size++])break;
    }while(size<sizeof(bytes));
    if(bytes[size-1]){fail(memory,source,size,0);return 0;}
    address=page(transaction,memory,size,"cloned descriptor name");
    for(i=0;i<size && !memory->fault;i++)
        game_memory_write_u8(memory,address+i,(uint8_t)bytes[i]);
    return memory->fault?0:address;
}

uint32_t scene_descriptor_clone_native(GameMemory *memory,uint32_t source_address) {
    uint32_t owned_output=0;
    int owned_result;
    if(!memory || memory->fault)return 0;
    owned_result=scene_projection_storage_clone_descriptor(source_address,
        &owned_output);
    if(owned_result>0)return owned_output;
    if(owned_result<0){fail(memory,source_address,0x4cu,0);return 0;}
    SceneDescriptorView source=load_descriptor(memory,source_address),clone={0};
    DescriptorTransaction transaction={{0},0u};
    MaterialValue *materials=NULL;
    uint32_t output,i,j,typed_materials=0;
    if(memory->fault)goto failed;
    if(source.vertex_count>0x100000u || source.polygon_count>0x100000u){
        fail(memory,source_address,0x4c,0);return 0;
    }
    output=page(&transaction,memory,0x4c,"cloned mesh descriptor");
    if(memory->fault)goto failed;
    clone.source_vertices=source.source_vertices;
    clone.vertex_count=source.vertex_count;
    clone.polygon_count=source.polygon_count;
    clone.name=copy_name(&transaction,memory,source.name);
    clone.transformed_vertices=page(&transaction,memory,(uint64_t)source.vertex_count*16u,
        "cloned mesh transformed");
    clone.links=page(&transaction,memory,(uint64_t)source.polygon_count*12u,"cloned mesh links");
    clone.uv=page(&transaction,memory,(uint64_t)source.polygon_count*24u,"cloned mesh uv");
    clone.uv_work=page(&transaction,memory,(uint64_t)source.polygon_count*24u,"cloned mesh uv work");
    clone.polygons=page(&transaction,memory,(uint64_t)source.polygon_count*40u,"cloned mesh polygons");
    if(memory->fault)goto failed;
    if(source.polygon_count){
        materials=calloc(source.polygon_count,sizeof(*materials));
        if(!materials){fail(memory,0,(size_t)source.polygon_count*sizeof(*materials),1);goto failed;}
    }
    clone.radius_bits=source.radius_bits;
    for(i=0;i<3;i++)clone.center[i]=source.center[i];
    for(i=0;i<3;i++)clone.auxiliary[i]=source.auxiliary[i];
    if(source.optional_shorts){
        clone.optional_shorts=page(&transaction,memory,(uint64_t)source.polygon_count*2u,
            "cloned mesh short flags");
        for(i=0;i<source.polygon_count && !memory->fault;i++)
            game_memory_write_u16(memory,clone.optional_shorts+i*2u,
                game_memory_read_u16(memory,source.optional_shorts+i*2u));
    }
    if(source.optional_bytes){
        clone.optional_bytes=page(&transaction,memory,source.polygon_count,
            "cloned mesh byte flags");
        for(i=0;i<source.polygon_count && !memory->fault;i++)
            game_memory_write_u8(memory,clone.optional_bytes+i,
                game_memory_read_u8(memory,source.optional_bytes+i));
    }
    if(memory->fault)goto failed;
    store_descriptor(memory,output,&clone);
    for(i=0;i<source.polygon_count && !memory->fault;i++) {
        uint32_t source_polygon=source.polygons+i*40u;
        uint32_t polygon=clone.polygons+i*40u;
        game_memory_write_u32(memory,polygon,3u);
        game_memory_write_u32(memory,polygon+4u,clone.links+i*12u);
        game_memory_write_u32(memory,polygon+8u,
            game_memory_read_u32(memory,source_polygon+8u));
        game_memory_write_u32(memory,polygon+16u,clone.uv_work+i*24u);
        game_memory_write_u32(memory,polygon+20u,
            game_memory_read_u32(memory,source_polygon+20u));
        if(scene_projection_storage_material(source_polygon,&materials[i]))
            ++typed_materials;
        for(j=0;j<3u && !memory->fault;j++) {
            uint32_t corner=i*3u+j;
            uint32_t old_vertex=game_memory_read_u32(memory,source.links+corner*4u);
            uint32_t relative=(old_vertex-source.transformed_vertices)&~15u;
            uint32_t u=game_memory_read_u32(memory,source.uv+corner*8u);
            uint32_t v=game_memory_read_u32(memory,source.uv+corner*8u+4u);
            game_memory_write_u32(memory,clone.links+corner*4u,
                clone.transformed_vertices+relative);
            game_memory_write_u32(memory,clone.uv+corner*8u,u);
            game_memory_write_u32(memory,clone.uv+corner*8u+4u,v);
        }
    }
    if(!memory->fault && typed_materials==source.polygon_count && typed_materials &&
        !scene_projection_storage_attach_materials(clone.polygons,materials,typed_materials))
        fail(memory,clone.polygons,(size_t)typed_materials*sizeof(*materials),1);
    if(!memory->fault && source.polygon_count &&
       !scene_projection_storage_bind_links(clone.links))
        fail(memory,clone.links,(size_t)source.polygon_count*12u,0);
    if(!memory->fault && source.source_vertices>=SCENE_PROJECTION_BASE &&
       source.source_vertices<SCENE_PROJECTION_END &&
       !scene_projection_storage_attach_shared_source(output,
            source.source_vertices))
        fail(memory,source.source_vertices,12u,0);
    if(memory->fault)goto failed;
    free(materials);
    return output;
failed:
    free(materials);
    while(transaction.count)scene_projection_storage_free(transaction.pages[--transaction.count]);
    return 0;
}


int scene_descriptor_can_release_native(GameMemory *memory,uint32_t descriptor) {
    return memory && !memory->fault &&
        scene_projection_storage_can_release_descriptor(descriptor);
}

int scene_descriptor_release_native(GameMemory *memory,uint32_t descriptor) {
    int released;
    if(!memory || memory->fault)return 0;
    released=scene_projection_storage_release_descriptor(descriptor);
    if(released<0)fail(memory,descriptor,0x4cu,1);
    return released!=0;
}

int scene_descriptor_discard_clone_native(uint32_t descriptor) {
    SceneDescriptorState view;
    uint32_t pages[8]; unsigned i;
    int result=scene_projection_storage_release_descriptor(descriptor);
    if(result<0)return 0;
    if(!result){
        if(!scene_projection_storage_descriptor(descriptor,&view))return 0;
        pages[0]=view.name;pages[1]=view.transformed_vertices;pages[2]=view.links;
        pages[3]=view.uv;pages[4]=view.uv_work;pages[5]=view.polygons;
        pages[6]=view.optional_shorts;pages[7]=view.optional_bytes;
        for(i=0u;i<8u;++i)if(pages[i] && !scene_projection_storage_free(pages[i]))return 0;
    }
    return scene_projection_storage_free(descriptor);
}
