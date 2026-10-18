#include "level_load.h"
#include "level_run.h"
#include "presentation_instance.h"
#include "presentation_lifecycle.h"
#include "presentation_commands.h"
#include "level_player_scene.h"
#include "runtime_node_pool.h"
#include "runtime_utilities.h"
#include "scene_projection_storage.h"
#include "level_player_slots.h"
#include "game_state_ops.h"
#include "entity_queries.h"
#include "gameplay_geometry.h"
#include "gameplay_state.h"
#include <stdlib.h>
#include <string.h>
extern int game_level_scene_command_publish(GameApplication *,GameAddress,RuntimeNode *);

/*One authoritative bounded record per resource cluster. This list owns record
 *and presentation lifetime; it stores no second copy of the economic state. */
struct GameResourceClusterOwner {
    struct GameResourceClusterOwner *next;
    GameAddress object,registration;
    GameAddress borrower;uint32_t borrower_identity;
    int presentation;
};
typedef struct ClusterGroup {GameAddress representative,object;} ClusterGroup;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
/*4ca2d0 ->53c210: real point order, propagated along the linked unit group.
 *Use the existing owned command records, with explicit position/target values. */
int game_level_scene_point_order(GameApplication *a,GameAddress object,uint32_t priority,uint32_t kind,GameAddress target,const float position[3]){
    unsigned visits=0;
    if(!a || !position || a->memory.fault)return 0;
    while(object){
        RuntimeNode *node=runtime_node_pool_find(get(a,object+0xdc));PresentationCommand command={0};
        PresentationLifecycle *state=presentation_instance_lifecycle(object);PresentationPose *pose=presentation_instance_pose(object);
        GameAddress table=get(a,object);
        if(++visits>65536 || !node || !state || !pose || get(a,table+0x114)!=0x4ca2d0 || get(a,table+0x160)!=0x53c210)return 0;
        command.priority=priority&255u;command.kind=kind;command.target=target;command.has_target=target!=0;command.has_position=1;
        if(target)command.target_identity=get(a,target+0x14);
        memcpy(command.position,position,sizeof(command.position));
        if(!presentation_command_build(&a->memory,node,&command,&node))return 0;
        state->runtime.packet_chain=node->projection;put(a,object+0xdc,node->projection);
        if(!game_level_scene_command_publish(a,object,node))return 0;
        object=get(a,object+0x10c);
    }
    return !a->memory.fault;
}
static float real(GameApplication *a,GameAddress p){uint32_t b=get(a,p);float v;memcpy(&v,&b,4);return v;}
static int32_t signed_word(uint32_t b){int32_t v;memcpy(&v,&b,4);return v;}
/*4589b0's placement/arrival/command program is a real remaining dependency.
 *It must return whether production was accepted, not dispatch CPU execution. */
extern int game_level_ground_controller_deploy(GameApplication *,GameAddress,unsigned,uint8_t,GameAddress,float[3],int *);
/*458660: hostile target selection, explicit site fallback, worker census and
 *lane balancing. All economic/list fields remain in the owned cluster record. */
int game_level_ground_controller_worker(GameApplication *a,GameAddress controller,GameAddress producer,int32_t *result){
    GameAddress target,sites,root;int16_t site_count;int32_t selected=-1,requested,lane=0,count=0;
    uint32_t budget,cursor;unsigned i,j,visits=0,category;uint8_t kind;float nearest=0x1.fffffep127f,position[3];int accepted;
    if(!a || !result || !controller || a->memory.fault)return 0;*result=-1;
    if(!get(a,0x5853ec))return !a->memory.fault;
    target=native_entity_nearest_hostile(&a->memory,controller);put(a,controller+0xf0,target);
    if(a->memory.fault)return 0;
    if(!target){
        uint32_t mask=get(a,producer+0xac);uint16_t word=game_memory_read_u16(&a->memory,0x639a74);memcpy(&site_count,&word,2);
        if(!mask)return !a->memory.fault;
        sites=get(a,0x639a70);root=get(a,0x58973c);
        for(i=0;site_count>0 && i<(unsigned)site_count;i++){
            GameAddress site=get(a,sites+i*4),owner=get(a,site+0x54);float distance;
            if(!owner || !(mask&(1u<<(i&31))))continue;
            category=game_memory_read_u16(&a->memory,root+8)&63u;
            j=game_memory_read_u16(&a->memory,owner+8)&63u;
            if(get(a,0x5f0160u+category*24u+j*4u))continue;
            distance=gameplay_geometry_fast_distance(&a->memory,real(a,producer+24),real(a,producer+32),real(a,site+0x48),real(a,site+0x50));
            if(selected<0 || distance<nearest || distance!=distance || nearest!=nearest){selected=(int32_t)i;nearest=distance;}
        }
        if(selected<0)return !a->memory.fault;
        {GameAddress site=get(a,sites+(uint32_t)selected*4);int16_t choices;
            uint16_t word=game_memory_read_u16(&a->memory,site+0x26);memcpy(&choices,&word,2);
            cursor=get(a,0x6308c0);if(cursor==1024)cursor=0;if(cursor>=1024)return 0;put(a,0x6308c0,cursor+1);
            budget=presentation_lifecycle_phase(real(a,0x6308c8+cursor*4),(float)choices)+1u;
            for(i=0;signed_word(budget)>0;i++){
                int16_t rows;GameAddress descriptor;
                word=game_memory_read_u16(&a->memory,site+0x24);memcpy(&rows,&word,2);
                if(rows<=0 || i>=(unsigned)rows)break;
                descriptor=get(a,get(a,site+0x18)+i*4);if(!descriptor)continue;
                budget--;if(!native_entity_descriptor_target(&a->memory,descriptor,&target))return 0;
                put(a,controller+0xf0,target);
            }
        }
    }
    kind=game_memory_read_u8(&a->memory,producer+12);
    if(kind==0x60){category=0;kind=0x40;}else if(kind==0x62){category=2;kind=0x27;}
    else if(kind==0x63){category=1;kind=0x21;}else return 0; /*Retail default uses uninitialized locals. */
    for(i=0;i<5;i++){
        GameAddress worker=get(a,controller+0x13c+i*4);uint32_t n=0;
        put(a,controller+0x150+i*4,0);if(!worker)put(a,controller+0x164+i*4,0);
        while(worker){
            if(++visits>65536)return 0;n++;
            if(game_memory_read_u8(&a->memory,worker+12)==kind)count++;
            worker=get(a,worker+0x10c);
        }
        put(a,controller+0x150+i*4,n);
    }
    if(signed_word(get(a,controller+0x178+category*4))<=count)return !a->memory.fault;
    requested=signed_word(get(a,controller+0x184));if(requested<0 || requested>5)return 0;
    for(i=1;i<(unsigned)requested;i++)if(signed_word(get(a,controller+0x150+(unsigned)lane*4))>signed_word(get(a,controller+0x150+i*4)) && !get(a,controller+0x164+i*4))lane=(int32_t)i;
    if(signed_word(get(a,controller+0x150+(unsigned)lane*4))>=signed_word(get(a,controller+0x188))){
        lane=-1;for(i=(unsigned)requested;i<5;i++)if(get(a,controller+0x164+i*4)!=1){lane=(int32_t)i;break;}
        if(lane<0)return !a->memory.fault;
    }
    target=get(a,controller+0xf0);if(a->memory.fault)return 0;
    /* A claimed site's descriptors can temporarily have no active payload.
     * Leave result=-1 so production waits for a target instead of ending the
     * level. The next update repeats the real hostile/site selection. */
    if(!target)return 1;
    for(i=0;i<3;i++)position[i]=real(a,target+24+i*4);
    if(!game_level_ground_controller_deploy(a,controller,(unsigned)lane,kind,producer,position,&accepted))return 0;
    if(accepted)*result=lane;return !a->memory.fault;
}
static int phase(GameApplication *a,GameAddress scale,uint32_t *out){
    uint32_t index=get(a,0x6308c0);
    if(index==1024)index=0;
    if(index>1024)return 0;
    put(a,0x6308c0,index+1);*out=presentation_lifecycle_phase(real(a,0x6308c8+index*4),real(a,scale));
    return !a->memory.fault;
}
static int packet_allocate(void *c,GameAddress *out){*out=runtime_node_pool_acquire(&((GameApplication *)c)->memory);return *out!=0;}
static int packet_prepare(void *c,GameAddress p){return runtime_node_pool_prepare(&((GameApplication *)c)->memory,p)==1;}
static int runtime_phase(void *c,uint32_t *out){return phase(c,0x576918,out);}
static int runtime_refresh(void *c,GameAddress p){
    GameApplication *a=c;return get(a,get(a,p)+0x14c)==0x459e90 && !a->memory.fault; /*Actual empty override. */
}
static int return_packets(void *c,GameAddress p){return runtime_node_pool_return_chain(&((GameApplication *)c)->memory,p)==1;}
int game_level_resource_clusters_release(GameApplication *a){
    PresentationReleaseServices services={a,NULL,NULL,NULL,return_packets};
    if(!a || a->memory.fault)return 0;
    while(a->resource_clusters){
        struct GameResourceClusterOwner *owner=a->resource_clusters;
        if(owner->borrower && presentation_instance_find(owner->borrower) &&
           get(a,owner->borrower+20)==owner->borrower_identity && get(a,owner->borrower+0xa8)==owner->object)
            put(a,owner->borrower+0xa8,0);
        if(owner->presentation){
            if(presentation_instance_release(&a->memory,owner->object,&services,1,1)!=1)return 0;
            owner->presentation=0;
        }
        if(owner->registration)put(a,owner->registration,0);
        if(a->memory.fault || !scene_projection_storage_free(owner->object))return 0;
        a->resource_clusters=owner->next;free(owner);
    }
    return 1;
}
/*533640/4598e0: remove this site's quarter-share and retain a shared controller
 *until its last site leaves. Reuse the actual cluster owner and packet lifetime. */
int game_level_ground_controller_release(GameApplication *a,GameAddress controller,uint8_t kind,int detach_workers){
    struct GameResourceClusterOwner **link=&a->resource_clusters,*owner;unsigned lane;
    uint32_t value,remaining;int32_t signed_value;
    PresentationReleaseServices services={a,NULL,NULL,NULL,return_packets};
    if(a->memory.fault)return 0;
    /*Retail's64 branch reads an uninitialized stack index. Do not invent a
     *successful release for that uncovered route or a foreign allocation. */
    if(kind==0x60)lane=0;else if(kind==0x62)lane=2;else if(kind==0x63)lane=1;else return 0;
    while(*link && (*link)->object!=controller)link=&(*link)->next;
    if(!(owner=*link))return 0;
    value=get(a,controller+0x178+lane*4);memcpy(&signed_value,&value,4);
    remaining=get(a,controller+0x190)-1u;
    put(a,controller+0x178+lane*4,value-(uint32_t)(signed_value/4));put(a,controller+0x190,remaining);
    if(a->memory.fault || remaining)return !a->memory.fault;
    if(detach_workers && !gameplay_state_peer_method(&a->memory,controller,0,get(a,get(a,controller)+0x100),NULL))return 0;
    if(owner->borrower && presentation_instance_find(owner->borrower) &&
       get(a,owner->borrower+20)==owner->borrower_identity && get(a,owner->borrower+0xa8)==controller)
        put(a,owner->borrower+0xa8,0);
    if(owner->presentation){
        if(presentation_instance_release(&a->memory,controller,&services,1,1)!=1)return 0;
        owner->presentation=0;
    }
    if(owner->registration)put(a,owner->registration,0);
    if(a->memory.fault || !scene_projection_storage_free(controller))return 0;
    *link=owner->next;free(owner);return 1;
}
/*Inline classFE construction in4599d0, including the shared539810 lifetime,
 *its separate randomized cluster phase and the real root identity sequence. */
static GameAddress create_controller(GameApplication *a,GameAddress position,int position_dimensions,struct GameResourceClusterOwner **output){
    struct GameResourceClusterOwner *owner;PresentationInstanceDefinition definition={0};
    PresentationInitializeServices initialize={a,packet_allocate,packet_prepare,runtime_phase,runtime_refresh};
    GameAddress object;uint32_t random;unsigned i;
    if(get(a,0x639828)>=8000)return 0;
    owner=calloc(1,sizeof(*owner));if(!owner)return 0;
    object=scene_projection_storage_allocate(0x19c,"level resource cluster");
    if(!object){free(owner);return 0;}
    owner->object=object;owner->next=a->resource_clusters;a->resource_clusters=owner;
    definition.type=0xfe;
    if(position_dimensions)for(i=0;i<3;i++)definition.dimensions[i]=real(a,position+i*4);
    if(!presentation_instance_create(&a->memory,object,&definition,1)){
        owner->presentation=presentation_instance_find(object)!=NULL;return 0;
    }
    owner->presentation=1;put(a,object,0x573598);
    for(i=0;i<3;i++)put(a,object+0x18+i*4,get(a,position+i*4));
    if(!presentation_lifecycle_initialize(&a->memory,object,presentation_instance_lifecycle(object),&initialize))return 0;
    put(a,object+0x10,0x3860);put(a,object+0x120,0x558c);
    for(i=0;i<5;i++){put(a,object+0x13c+i*4,0);put(a,object+0x150+i*4,0);put(a,object+0x164+i*4,0);}
    for(i=0;i<3;i++)put(a,object+0x178+i*4,0);
    for(i=0;i<3;i++)put(a,object+0x184+i*4,0);
    put(a,object+0x190,0);put(a,object+0x194,0);
    if(!phase(a,0x573568,&random))return 0;
    put(a,object+0x198,random);
    *output=owner;return a->memory.fault?0:object;
}
static GameAddress create_cluster(GameApplication *a,GameAddress site,GameAddress root){
    struct GameResourceClusterOwner *owner;uint32_t sequence,index;
    GameAddress object=create_controller(a,site+0x18,0,&owner);if(!object)return 0;
    sequence=get(a,root+0x18);put(a,root+0x18,sequence+1);
    put(a,object+0x14,(sequence<<6)|game_memory_read_u16(&a->memory,root+8));
    index=runtime_register_pointer(&a->memory,object);owner->registration=0x631b24+index*4;
    return a->memory.fault?0:object;
}
/*5339f0 constructs this private458280 delegate before459ff0 restores it.
 *The existing cluster owner also retires archive-created delegates. */
GameAddress game_level_ground_controller_restore_create(GameApplication *a,GameAddress target){
    struct GameResourceClusterOwner *owner;GameAddress object;
    if(!a || !target || a->memory.fault || get(a,target+0xa8))return 0;
    object=create_controller(a,target+0x18,1,&owner);if(!object)return 0;
    owner->borrower=target;owner->borrower_identity=get(a,target+20);
    put(a,target+0xa8,object);return a->memory.fault?0:object;
}
/*458440: increment aggregate production using the original low-word multiply,
 *signed division and ten-site saturation; retain the existing three lanes. */
int game_level_resource_cluster_accumulate(GameApplication *a,GameAddress p,uint8_t kind,int32_t v0,int32_t v1,int32_t v2,uint32_t v3){
    unsigned lane;int32_t count;uint32_t factor;unsigned i;int32_t values[3]={v0,v1,v2};
    if(!a || a->memory.fault)return 0;
    if(kind==0x60)lane=0;else if(kind==0x63)lane=1;else if(kind==0x62)lane=2;else return 0;
    if(v0==-1){for(i=0;i<3;i++)put(a,p+0x184+i*4,0);put(a,p+0x178+lane*4,0);return !a->memory.fault;}
    count=signed_word(get(a,p+0x190));factor=10u-(uint32_t)(count<10?count:9);
    for(i=0;i<3;i++){
        uint32_t product=(uint32_t)values[i]*(uint32_t)factor;
        int32_t contribution=signed_word(product)/9;
        put(a,p+0x184+i*4,get(a,p+0x184+i*4)+(uint32_t)contribution);
    }
    put(a,p+0x178+lane*4,get(a,p+0x178+lane*4)+v3);return !a->memory.fault;
}
static int add_site(GameApplication *a,GameAddress cluster,uint8_t kind){
    GameAddress fields[4];uint32_t count=get(a,cluster+0x190)+1;
    put(a,cluster+0x190,count);put(a,cluster+0x194,count);
    if(kind!=0x60 && kind!=0x62 && kind!=0x63)return !a->memory.fault; /*459940's actual default RET. */
    if(kind==0x60){fields[0]=0x5e1210;fields[1]=0x5e1224;fields[2]=0x5e1214;fields[3]=0x5e122c;}
    else if(kind==0x62){fields[0]=0x5e1228;fields[1]=0x5e121c;fields[2]=0x5e1238;fields[3]=0x5e123c;}
    else{fields[0]=0x5e1230;fields[1]=0x5e120c;fields[2]=0x5e1220;fields[3]=0x5e1234;}
    return game_level_resource_cluster_accumulate(a,cluster,kind,signed_word(get(a,fields[0])),signed_word(get(a,fields[1])),signed_word(get(a,fields[2])),get(a,fields[3]));
}
/*440d80's458280 ->4598c0/459940 delegate. Reuse the existing controller
 *owner/runtime and its economic binding; the unit only borrows its identity.
 *The private delegate is not registered as a separate world/render actor. */
int game_level_arrival_delegate_create(GameApplication *a,GameAddress root,GameAddress target){
    struct GameResourceClusterOwner *owner;GameAddress object;uint32_t identity,next;
    if(!a || !root || !target || a->memory.fault)return 0;
    object=create_controller(a,target+0x18,1,&owner);if(!object)return 0;
    if(level_player_slots_owns_player(root)){
        if(!level_player_slots_next_owner_sequence(&a->memory,root,&identity,&next))return 0;
        put(a,root+0x18,next);
    }else identity=game_state_next_owner_sequence(&a->memory,root);
    put(a,object+0x14,identity);
    if(!add_site(a,object,game_memory_read_u8(&a->memory,target+12)))return 0;
    owner->borrower=target;owner->borrower_identity=get(a,target+20);
    put(a,target+0xa8,object);return !a->memory.fault;
}
/*459770: census worker chains, then issue the actual group point command once
 *a lane has sufficient workers and is not already assigned to the target. */
int game_level_resource_cluster_update(GameApplication *a,GameAddress object){
    uint32_t tick,total=0;int32_t requested;GameAddress target;unsigned i;
    if(!a || a->memory.fault)return 0;
    tick=get(a,0x5ac6ac);
    if(signed_word(get(a,object+0x198)+32)>signed_word(tick))return !a->memory.fault;
    put(a,object+0x198,tick);
    for(i=0;i<5;i++){
        GameAddress worker=get(a,object+0x13c+i*4);uint32_t count=0;
        if(!worker)put(a,object+0x164+i*4,0);
        while(worker){if(++count>65536)return 0;worker=get(a,worker+0x10c);}
        put(a,object+0x150+i*4,count);
    }
    requested=signed_word(get(a,object+0x184));
    /*The original reads a record prefix before checking its five-worker cap.
     *Keep those valid reads; reject malformed counts outside the owned tail. */
    if(requested>19)return 0;
    for(i=0;(int32_t)i<requested;i++)total+=get(a,object+0x150+i*4);
    target=get(a,object+0xf0);
    if(!target || signed_word(get(a,object+0x188)*(uint32_t)requested)>signed_word(total) || requested>=5)return !a->memory.fault;
    if(requested<0)return 0;
    for(i=(unsigned)requested;i<5;i++){
        GameAddress worker=get(a,object+0x13c+i*4);float position[3];unsigned j;
        if(!worker || signed_word(get(a,object+0x150+i*4))<signed_word(get(a,object+0x18c)) || get(a,object+0x164+i*4))continue;
        for(j=0;j<3;j++)position[j]=real(a,target+0x18+j*4);
        if(!game_level_scene_point_order(a,worker,1,5,target,position))return 0;
        put(a,object+0x164+i*4,1);
    }
    return !a->memory.fault;
}
/*4107a0: max(dx,dz)+min(dx,dz)*factor, with its two float subtraction
 *stores and final float store; this is deliberately not Euclidean distance. */
static float site_distance(GameApplication *a,GameAddress first,GameAddress second){
    long double dx=(long double)real(a,first+0x18)-real(a,second+0x18),dz=(long double)real(a,first+0x20)-real(a,second+0x20);
    float x=(float)dx,z=(float)dz;float zero=real(a,0x571f14),factor=real(a,0x571f18);
    if(!(dx>=zero))x=-x;
    if(!(dz>=zero))z=-z;
    return !(z>=x)?(float)((long double)z*factor+x):(float)((long double)x*factor+z);
}
/*4599d0: preserve traversal and first matching representative, not transitive
 *clustering. Native temporary metadata replaces the original disposable list. */
int game_level_visibility_finish(GameApplication *a){
    GameAddress root,site;ClusterGroup *groups=NULL;size_t count=0,capacity=0;unsigned visits=0;int ok=0;
    if(!a || a->memory.fault)return 0;
    root=get(a,0x5f0224);
    for(site=get(a,root+0x12c);site;site=get(a,site+0x48)){
        uint8_t kind=game_memory_read_u8(&a->memory,site+0xc);size_t i;GameAddress cluster;
        if(++visits>65536 || a->memory.fault)goto done;
        if(kind!=0x60 && kind!=0x62 && kind!=0x63)continue;
        for(i=0;i<count;i++)if(!(site_distance(a,groups[i].representative,site)>=real(a,0x573584)))break;
        if(i==count){
            if(count==capacity){size_t next=capacity?capacity*2:16;ClusterGroup *grown=realloc(groups,next*sizeof(*groups));if(!grown)goto done;groups=grown;capacity=next;}
            cluster=create_cluster(a,site,root);if(!cluster)goto done;
            groups[count].representative=site;groups[count++].object=cluster;
        }else cluster=groups[i].object;
        if(!add_site(a,cluster,kind))goto done;
        put(a,site+0xa8,cluster);
    }
    ok=!a->memory.fault;
done:free(groups);return ok;
}
