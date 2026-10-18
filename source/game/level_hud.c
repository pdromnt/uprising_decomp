#include "level_hud.h"
#include "level_run.h"
#include "model_assets.h"
#include "asset_bitmap.h"
#include "display_transition.h"
#include "player_feedback.h"
#include "scene_projection_storage.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Only ownership metadata is kept here. Sprite bytes, dimensions, coordinates
 *and resource IDs remain in the existing records; pixels have one page owner.
 *Uncached quad textures retire with the quad; cached IDs borrow the existing
 *application texture owner. Pixels are never duplicated in a legacy mirror. */
typedef struct HudAllocation {GameApplication *app;GameAddress record;unsigned kind;struct HudAllocation *next;} HudAllocation;
static HudAllocation *allocations;
int game_level_player_hit_marker(GameApplication *a,const float point[3],uint32_t kind,int32_t amount){
    HudAllocation *entry=allocations;
    for(;entry;entry=entry->next)if(entry->app==a && entry->record==0x60386c && entry->kind==2)
        return game_player_hit_marker(&a->memory,point,(uint8_t)kind,amount);
    return 0;
}
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static void real_put(GameApplication *a,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);put(a,p,bits);}
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int path(GameApplication *a,GameAddress address,char name[260]){
    unsigned i;for(i=0;i<260;++i){name[i]=(char)game_memory_read_u8(&a->memory,address+i);if(!name[i])return !a->memory.fault;}return 0;
}
static int own(GameApplication *a,GameAddress record,unsigned kind){
    HudAllocation *entry;
    for(entry=allocations;entry;entry=entry->next)if(entry->app==a && entry->record==record)return entry->kind==kind;
    entry=malloc(sizeof(*entry));if(!entry)return 0;
    entry->app=a;entry->record=record;entry->kind=kind;entry->next=allocations;allocations=entry;return 1;
}
int game_level_hud_quad_release(GameApplication *a,GameAddress record){
    static const GameAddress references[]={0x603928,0x6038dc,0x603a3c,0x602630};
    HudAllocation **link=&allocations;GameMemory cleanup=a->memory;unsigned i;
    if(!record)return 1;
    while(*link && ((*link)->app!=a || (*link)->record!=record))link=&(*link)->next;
    if(!*link || ((*link)->kind!=1 && (*link)->kind!=3))return 0;
    cleanup.fault=0;
    if((*link)->kind==3){
        uint32_t texture=game_memory_read_u32(&cleanup,record+0x94);
        if(cleanup.fault || !game_model_texture_release(a,texture))return 0;
        game_memory_write_u32(&cleanup,record+0x94,0);
    }
    for(i=0;i<sizeof(references)/sizeof(references[0]);++i)
        if(game_memory_read_u32(&cleanup,references[i])==record)game_memory_write_u32(&cleanup,references[i],0);
    if(cleanup.fault || !scene_projection_storage_free(record))return 0;
    {HudAllocation *entry=*link;*link=entry->next;free(entry);}
    return 1;
}
int game_level_hud_assets_release(GameApplication *a){
    HudAllocation **link=&allocations;GameMemory cleanup=a->memory;cleanup.fault=0;
    while(*link){HudAllocation *entry=*link;GameAddress pixels;
        if(entry->app!=a){link=&entry->next;continue;}
        if(entry->kind==0){
            pixels=game_memory_read_u32(&cleanup,entry->record+8);
            if(cleanup.fault || (pixels && !scene_projection_storage_free(pixels)))return 0;
            game_memory_write_u32(&cleanup,entry->record+8,0);
        }else if(entry->kind==1 || entry->kind==3){
            if(!game_level_hud_quad_release(a,entry->record))return 0;
            continue;
        }
        else{
            pixels=game_memory_read_u32(&cleanup,entry->record);
            if(cleanup.fault || (pixels && !scene_projection_storage_free(pixels)))return 0;
            game_memory_write_u32(&cleanup,entry->record,0);
        }
        *link=entry->next;free(entry);
    }
    return !cleanup.fault;
}
/*413190: BMP bfOffBits selects the source. HUD sprites pack each row without
 *padding and reverse the row order. Height1 skips copying in the original;
 *native zeroed storage makes its formerly uninitialized pixels deterministic. */
static int sprite(GameApplication *a,GameAddress record,const char *name){
    ResourceBlob blob={0};AssetBitmap image;GameAddress pixels=0,old;size_t bytes;uint32_t offset,row;int ok=0;
    if(!game_application_load_blob(a,name,&blob)) {
        game_application_log(a,"native HUD sprite load failed: %s\n",name);return 0;
    }
    if(!asset_bitmap_sprite_parse(&blob,&image))goto done;
    offset=word((const uint8_t *)blob.data+10);
    if(image.height>SIZE_MAX/image.width)goto done;
    bytes=(size_t)image.width*image.height;pixels=scene_projection_storage_allocate(bytes,"HUD sprite pixels");
    if(!pixels || !own(a,record,0))goto done;
    if(image.height>1)for(row=0;row<image.height;++row)if(!game_memory_write(&a->memory,pixels+row*image.width,
        (const uint8_t *)blob.data+offset+(image.height-1-row)*image.stride,image.width))goto done;
    old=get(a,record+8);if(a->memory.fault || (old && !scene_projection_storage_free(old)))goto done;
    game_memory_write_u16(&a->memory,record+4,(uint16_t)image.width);
    game_memory_write_u16(&a->memory,record+6,(uint16_t)image.height);put(a,record+8,pixels);
    pixels=0;ok=!a->memory.fault;
done:
    if(!ok)game_application_log(a,"native HUD sprite decode/publish failed: %s\n",name);
    if(pixels)(void)scene_projection_storage_free(pixels);resource_blob_release(&blob);return ok;
}
static int named_sprite(GameApplication *a,GameAddress record,GameAddress name){char text[260];return path(a,name,text) && sprite(a,record,text);}
/*412790: the quad's four vertex links point into its own owned record. */
GameAddress game_level_hud_quad_create(GameApplication *a){
    GameAddress p=scene_projection_storage_allocate(0xcc,"HUD textured quad");unsigned i;
    if(!p)return 0;
    if(!own(a,p,1)){scene_projection_storage_free(p);return 0;}
    for(i=0;i<4;++i)put(a,p+0x40+i*4,p+i*16);
    put(a,p+0xa0,0x11d);put(a,p+0xac,p+0x90);return a->memory.fault?0:p;
}
/*412800: recover dimensions and the texture's native ID from the shared
 *material owner, then preserve the original scalar stores and rounded sqrt. */
int game_level_hud_quad_image(GameApplication *a,GameAddress p,float scale,GameAddress name){
    MaterialValue material={0};char text[260];long double vertical,half_scale,height_half,sum;float rounded;
    int cached=get(a,0x5a0804)!=0 || get(a,0x5a0808)!=0;HudAllocation *entry;
    if(!p || !path(a,name,text))return 0;
    if(!game_model_material_load(a,text,-1,2,1,&material) || !material.width || !material.height) {
        game_application_log(a,"native HUD material failed: %s\n",text);return 0;
    }
    /*The existing material service creates an exclusive texture on its normal
     *path. Its alternate cache retains shared application ownership. */
    if(!cached)for(entry=allocations;entry;entry=entry->next)if(entry->app==a && entry->record==p){entry->kind=3;break;}
    put(a,p+0x94,material.backend_handle);
    vertical=(long double)material.height*((long double)real(a,0x571f70)/material.width)*scale;
    half_scale=(long double)scale*real(a,0x571f74);
    height_half=vertical*real(a,0x571f74);
    sum=half_scale*half_scale*real(a,0x571f78)+height_half*height_half;
    rounded=(float)sum;
    game_memory_write_u16(&a->memory,p+0x90,material.width);game_memory_write_u16(&a->memory,p+0x92,material.height);
    real_put(a,p+0xc0,scale);real_put(a,p+0xc4,(float)vertical);real_put(a,p+0xc8,(float)sqrtl(rounded));
    return !a->memory.fault;
}

static const struct {GameAddress record,name;} hud_sprites[]={
        {0x00603348,0x0058ff0c},
        {0x0060335c,0x0058ff20},
        {0x00603370,0x0058ff34},
        {0x00603384,0x0058ff44},
        {0x00603398,0x0058ff58},
        {0x006033ac,0x0058ff6c},
        {0x006033c0,0x0058ff80},
        {0x006033d4,0x0058ff94},
        {0x006033e8,0x0058ffa4},
        {0x006033fc,0x0058ffb8},
        {0x00603410,0x0058ffcc},
        {0x00603424,0x0058ffe0},
        {0x00603438,0x0058fff0},
        {0x0060344c,0x00590000},
        {0x00603460,0x00590010},
        {0x006034d8,0x00590024},
        {0x006034ec,0x00590038},
        {0x00603500,0x0059004c},
        {0x00603514,0x00590060},
        {0x00603528,0x00590074},
        {0x0060353c,0x00590088},
        {0x00603550,0x0059009c},
        {0x00603564,0x005900b0},
        {0x00603578,0x005900c4},
        {0x0060358c,0x005900d8},
        {0x006035a0,0x005900ec},
        {0x006035b4,0x00590100},
        {0x006035c8,0x00590114},
        {0x006035dc,0x00590128},
        {0x006035f0,0x0059013c},
        {0x00603e40,0x00590150},
        {0x00603e54,0x00590164},
        {0x00603e68,0x00590174},
        {0x00603e7c,0x00590188},
        {0x00603e90,0x00590198},
        {0x00603ea4,0x005901ac},
        {0x00603eb8,0x005901c0},
        {0x00603ecc,0x005901d0},
        {0x00603ee0,0x005901e0},
        {0x00603ef4,0x005901f0},
        {0x00603f08,0x00590204},
        {0x00603f1c,0x00590218},
        {0x00603f44,0x00590228},
        {0x00603f30,0x0059023c},
        {0x00603d18,0x0059024c},
        {0x00603d2c,0x00590260},
        {0x00603d40,0x00590274},
        {0x00603d54,0x00590288},
        {0x00603d68,0x00590298},
        {0x00603d7c,0x005902ac},
        {0x00603d90,0x005902c0},
        {0x00603da4,0x005902d4},
        {0x00603db8,0x005902e8},
        {0x00603dcc,0x005902fc},
        {0x00603de0,0x00590310},
        {0x00603df4,0x00590324},
        {0x00603e1c,0x00590334},
        {0x00603e08,0x00590348},
        {0x00603ba0,0x00590358},
        {0x00603bb4,0x00590368},
        {0x00603bc8,0x00590378},
        {0x00603bdc,0x00590388},
        {0x00603bf0,0x0059039c},
        {0x00603c04,0x005903ac},
        {0x00603c18,0x005903c0},
        {0x00603c2c,0x005903d0},
        {0x00603c40,0x005903e4},
        {0x00603c54,0x005903f4},
        {0x00603c68,0x00590404},
        {0x00603c7c,0x00590418},
        {0x00603c90,0x0059042c},
        {0x00603ca4,0x0059043c},
        {0x00603cb8,0x0059044c},
        {0x00603ccc,0x0059045c},
        {0x00603ce0,0x0059046c},
        {0x00603a48,0x00590480},
        {0x00603a5c,0x00590490},
        {0x00603a70,0x005904a4},
        {0x00603a84,0x005904b8},
        {0x00603a98,0x005904cc},
        {0x00603aac,0x005904e0},
        {0x00603ac0,0x005904f4},
        {0x00603ad4,0x00590508},
        {0x00603ae8,0x0059051c},
        {0x00603afc,0x00590530},
        {0x00603b10,0x00590544},
        {0x00603b24,0x00590558},
        {0x00603b38,0x0059056c},
        {0x00603b4c,0x00590580},
        {0x00603b60,0x00590590},
        {0x00603b74,0x005905a0},
        {0x00603b88,0x005905b4},
};

static int assets_load(GameApplication *a){
    static const struct {GameAddress record,name;} frames[]={
        {0x603930,0x5905c8},{0x603768,0x5905dc},{0x6039b0,0x5905f0},{0x603690,0x590604}};
    unsigned i,j;char format[260],name[260];int written;
    for(i=0;i<sizeof(hud_sprites)/sizeof(hud_sprites[0]);++i)
        if(!named_sprite(a,hud_sprites[i].record,hud_sprites[i].name))return 0;
    for(i=0;i<6;++i)for(j=0;j<4;++j){
        if(!path(a,frames[j].name,format))return 0;
        written=snprintf(name,sizeof(name),format,(int)i);
        if(written<0 || (size_t)written>=sizeof(name) || !sprite(a,frames[j].record+i*20,name))return 0;
    }
    put(a,0x6040a0,0);return !a->memory.fault;
}
static int clear_records(GameApplication *a,GameAddress first,unsigned count){return game_memory_fill_u32(&a->memory,first,count*5,0,0);}
static uint32_t signed_half(uint32_t v){return (v>>1)|((v&0x80000000u)?0x80000000u:0);}
extern int game_level_display_mask_resize(GameApplication *,uint32_t);
extern void game_level_display_geometry_constants(GameApplication *);
/*4ac830: recover the complete native asset/layout construction, including both
 *quad owners, actual radar mask generation and the16-entry pointer history.
 *The frame/render consumers borrow the same records and fixed-width IDs. */
int game_level_window_initialize(GameApplication *a){
    static const GameAddress singles[]={0x604010,0x603490,0x6034a4,0x6038c0,0x603830,0x603f60,
        0x603620,0x603ff8,0x6032e0,0x603308,0x6032c0,0x603818,0x603f90,0x6034b8,0x603a28,
        0x603478,0x603678,0x603718,0x603330,0x603608,0x603730,0x604088,0x604068,0x603cf8,
        0x6040e8,0x604100,0x6040c0,0x603888};
    static const struct {GameAddress record,name;} extra[]={
        {0x603830,0x58fe18},{0x603620,0x58fe2c},{0x6032e0,0x58fe3c},{0x6032c0,0x58fe4c},
        {0x603818,0x58fe60},{0x604068,0x58fe88},{0x603cf8,0x58fe9c},{0x6040e8,0x58feb0},
        {0x604100,0x58fec4},{0x6040c0,0x58fed4},{0x603888,0x58fee4}};
    GameMemory *m=&a->memory;GameAddress first,second,history;unsigned i;uint32_t mode=get(a,0x597f80);
    if(m->fault || !game_level_hud_assets_release(a))return 0;
    if(!clear_records(a,0x603ba0,17) || !clear_records(a,0x603a48,17) ||
       !clear_records(a,0x603348,15) || !clear_records(a,0x6034d8,15) ||
       !clear_records(a,0x603e40,14) || !clear_records(a,0x603d18,14) ||
       !clear_records(a,0x603930,6) || !clear_records(a,0x603768,6) ||
       !clear_records(a,0x6039b0,6) || !clear_records(a,0x603690,6))return 0;
    for(i=0;i<sizeof(singles)/sizeof(singles[0]);++i)if(!clear_records(a,singles[i],1))return 0;
    if(!game_memory_fill_u32(m,0x604028,15,0,0) || !game_memory_fill_u32(m,0x603638,15,0,0))return 0;
    put(a,0x603320,0);put(a,0x603324,0);put(a,0x6038a0,0);
    if(!assets_load(a))goto failed;
    game_memory_write_u8(m,0x59d448,246);
    for(i=0;i<5;++i)if(!named_sprite(a,extra[i].record,extra[i].name))goto failed;
    first=game_level_hud_quad_create(a);put(a,0x603928,first);
    if(!first || !game_level_hud_quad_image(a,first,132.0f,0x58fe74))goto failed;
    for(i=5;i<sizeof(extra)/sizeof(extra[0]);++i)if(!named_sprite(a,extra[i].record,extra[i].name))goto failed;
    second=game_level_hud_quad_create(a);put(a,0x6038dc,second);
    if(!second || !game_level_hud_quad_image(a,second,66.0f,0x58fef8))goto failed;
    put(a,0x6034cc,mode==1?0x604118:0x6040e0);put(a,0x603a3c,mode==1?first:second);
    put(a,0x604064,UINT32_MAX);put(a,0x6037e8,mode?30:19);put(a,0x6034d0,mode?128:56);
    if(!game_level_display_mask_resize(a,mode?128:56) || !game_display_layout(m,0,0,0))goto failed;
    game_level_display_geometry_constants(a);
    history=scene_projection_storage_allocate(256,"HUD pointer history");
    if(!history || !own(a,0x60386c,2)){if(history)scene_projection_storage_free(history);goto failed;}
    put(a,0x60386c,history);
    for(i=0;i<16;++i){
        put(a,history+i*16,signed_half(get(a,0x627928)));put(a,history+i*16+4,signed_half(get(a,0x62792c)));
        put(a,history+i*16+8,get(a,0x5ac6ac));
    }
    put(a,0x6039a8,0);put(a,0x603874,0);
    if(!game_display_layout(m,0,0,0))goto failed;
    put(a,0x603878,10);put(a,0x603870,5);return !m->fault;
failed:
    (void)game_level_hud_assets_release(a);return 0;
}
