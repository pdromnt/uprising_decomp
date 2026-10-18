#include "menu_widgets.h"
#include "menu_choice_controls.h"
#include "menu_movie.h"
#include "scene_projection_storage.h"
#include "state_helper_plans.h"
#include "gameplay_state.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int game_menu_missing_route(GameApplication *a,const char *kind,uint32_t id){
    char message[96];snprintf(message,sizeof(message),"%s ID 0x%08lx requires native reconstruction",kind,(unsigned long)id);
    game_application_log(a,"unfinished menu route",message);return 0;
}

extern int game_menu_register_region(GameApplication *, GameAddress, GameAddress, GameAddress, uint32_t, uint32_t, uint32_t);
extern int game_menu_register_tick(GameApplication *, GameAddress, GameAddress, uint32_t, uint32_t);
extern int game_menu_register_delivery(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister(GameApplication *, GameAddress, GameAddress, int);
extern int game_menu_request(GameApplication *, GameAddress, uint32_t, uint32_t, uint32_t);
extern int game_menu_broadcast(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_focus_add(GameApplication *, GameAddress);
extern int game_menu_focus_remove(GameApplication *, GameAddress);
extern GameAddress game_menu_controller_construct(GameApplication *);
extern void game_menu_controller_destroy(GameApplication *,GameAddress);

/* The immutable file buffer is the image owner. The parsed view borrows it;
 * records hold only the image's fixed-width identity, never a host pointer. */
typedef struct MenuImage {
    GameAddress identity;
    ResourceBlob blob;
    AssetBitmap bitmap;
    struct MenuImage *next;
} MenuImage;

static uint32_t get(GameApplication *a, GameAddress p, unsigned o) {
    return game_memory_read_u32(&a->memory, p + o);
}
static void put(GameApplication *a, GameAddress p, unsigned o, uint32_t v) {
    game_memory_write_u32(&a->memory, p + o, v);
}
static MenuImage *image_find(GameApplication *a, GameAddress identity) {
    MenuImage *p = a->menu_images;
    while (p && p->identity != identity) p = p->next;
    return p;
}
static void image_release(GameApplication *a, GameAddress identity) {
    MenuImage **link = &a->menu_images;
    while (*link) {
        MenuImage *p = *link;
        if (p->identity == identity) {
            *link = p->next; resource_blob_release(&p->blob); free(p); return;
        }
        link = &p->next;
    }
}
void game_menu_images_dispose(GameApplication *a) {
    while (a->menu_images) image_release(a, ((MenuImage *)a->menu_images)->identity);
}
static int name_read(GameApplication *a, GameAddress name, char output[132]) {
    unsigned i;
    for (i = 0; i < 132u; ++i) {
        output[i] = (char)game_memory_read_u8(&a->memory, name + i);
        if (!output[i]) return !a->memory.fault;
    }
    return 0;
}
static void copy_rect(GameApplication *a, GameAddress p, unsigned to, unsigned from) {
    unsigned i;
    for (i = 0; i < 16u; i += 4u) put(a, p, to + i, get(a, p, from + i));
}
static void dirty(GameApplication *a, GameAddress p) {
    if (!get(a, p, 0x1c)) copy_rect(a, p, 0x30, 0x20);
    put(a, p, 0x1c, 1u);
}
static MenuImage *image_load_bytes(GameApplication *a,GameAddress identity,const char *filename) {
    MenuImage *image=calloc(1,sizeof(*image));
    if(!image)return NULL;
    if(!game_application_load_blob(a,filename,&image->blob) || !asset_bitmap_menu_parse(&image->blob,&image->bitmap)) {
        fprintf(stderr,"native menu bitmap load failed: %s; bytes=%lu bpp=%u compression=%u\n",filename,
            (unsigned long)image->blob.size,image->blob.size>30?image->blob.data[28]:0,
            image->blob.size>34?image->blob.data[30]:0);
        resource_blob_release(&image->blob);free(image);return NULL;
    }
    image_release(a,identity);image->identity=identity;
    image->next=a->menu_images;a->menu_images=image;return image;
}
/* 0040b140/004daac0: load the actual loose/catalog bitmap and derive bounds.
 * Indexed and 24-bit BI_RGB are implemented. Other encodings remain explicit failure
 * until their real conversion path is reconstructed. */
int game_menu_image_load(GameApplication *a, GameAddress p, GameAddress name) {
    char filename[132];
    MenuImage *image;
    unsigned i;
    if (!name_read(a, name, filename)) return 0;
    image=image_load_bytes(a,p,filename);
    if(!image)return 0;
    for (i = 0; i <= strlen(filename); ++i)
        game_memory_write_u8(&a->memory, p + 0xb8u + i, (uint8_t)filename[i]);
    put(a, p, 0xb4, p);
    put(a, p, 0x28, get(a, p, 0x20) + image->bitmap.width);
    put(a, p, 0x2c, get(a, p, 0x24) + image->bitmap.height);
    dirty(a, p); copy_rect(a, p, 0x144, 0x20);
    return !a->memory.fault;
}
GameAddress game_menu_image_create(GameApplication *a, uint32_t identity, GameAddress name) {
    GameAddress p = scene_projection_storage_allocate(0x16cu, "portable menu image");
    if (!p) return 0;
    game_memory_fill_u8(&a->memory, p, 0x16cu, 0, 0);
    state_helper_construct_packet(&a->memory, p, identity);
    put(a, p, 0, 0x571c58u); put(a, p, 0xb0, 100u);
    if (a->memory.fault || (name && !game_menu_image_load(a, p, name))) {
        image_release(a, p); scene_projection_storage_free(p); return 0;
    }
    return p;
}
/* 0040c0f0/0040c280/0040c510: the shared menu indicator's real frame assets.
 * Each frame slot supplies a stable ID owned by this one legacy record; the
 * immutable file owner is shared with the ordinary image presenter. */
GameAddress game_menu_animation_create(GameApplication *a,GameAddress name) {
    GameAddress p=scene_projection_storage_allocate(0x30cu,"portable menu animation");
    char prefix[132],filename[144];unsigned i;
    MenuImage *first=NULL;
    if(!p)return 0;
    game_memory_fill_u8(&a->memory,p,0x30cu,0,0);
    state_helper_construct_packet(&a->memory,p,0);
    put(a,p,0,0x571ca0u);put(a,p,0xb0,100);put(a,p,0x304,1);
    if(!name_read(a,name,prefix))goto fail;
    for(i=0;i<100u;++i) {
        MenuImage *image;ResourceId id;
        ResourcePathBytes path;
        int length=snprintf(filename,sizeof(filename),"%s%02u.bmp",prefix,i);
        if(length<0 || (size_t)length>=sizeof(filename))goto fail;
        path.data=(const uint8_t *)filename;path.readable_size=(size_t)length+1u;
        /* The original counts frames via its shared resource catalog. */
        if(!a->resources || resource_catalog_find(a->resources,path,&id)!=RESOURCE_TREE_OK)break;
        image=image_load_bytes(a,p+0x16cu+i*4u,filename);
        if(!image)goto fail;
        if(!first)first=image;
        put(a,p,0x16c+i*4u,image->identity);put(a,p,0x2fc,i+1u);
    }
    if(i==100u) {
        ResourceId id;
        ResourcePathBytes path;
        int length=snprintf(filename,sizeof(filename),"%s100.bmp",prefix);
        if(length<0 || (size_t)length>=sizeof(filename))goto fail;
        path.data=(const uint8_t *)filename;path.readable_size=(size_t)length+1u;
        if(resource_catalog_find(a->resources,path,&id)==RESOURCE_TREE_OK)goto fail;
    }
    if(!first)goto fail;
    for(i=0;i<=strlen(prefix);++i)game_memory_write_u8(&a->memory,p+0xb8u+i,(uint8_t)prefix[i]);
    put(a,p,0x140,1);put(a,p,0xb4,get(a,p,0x16c));
    put(a,p,0x28,first->bitmap.width);put(a,p,0x2c,first->bitmap.height);
    put(a,p,0x14c,first->bitmap.width-1u);put(a,p,0x150,first->bitmap.height-1u);
    dirty(a,p);return a->memory.fault?0:p;
fail:
    (void)game_menu_widget_destroy(a,p);return 0;
}
int game_menu_animation_advance(GameApplication *a,GameAddress p,uint32_t *frame) {
    uint32_t old=get(a,p,0x300),step=get(a,p,0x304),index=old+step,count=get(a,p,0x2fc);
    if(count>100u)return 0;
    put(a,p,0x300,index);
    if((int32_t)index<0 || (int32_t)index>=(int32_t)count) {
        if(get(a,p,0x308)) {put(a,p,0x300,old);*frame=UINT32_MAX;return !a->memory.fault;}
        if(step==1u) {index=0;put(a,p,0x300,index);}
        else if(step==UINT32_MAX)put(a,p,0x304,count-1u); /* Retail writes step here. */
    }
    if(index>=100u && index!=UINT32_MAX)return 0;
    put(a,p,0xb4,get(a,p,0x16cu+index*4u));dirty(a,p);put(a,p,0x18,1);
    *frame=get(a,p,0x300);return !a->memory.fault;
}
int game_menu_image_mode(GameApplication *a,GameAddress p,uint32_t mode) {
    uint32_t methods=get(a,p,0);
    if(methods!=0x571c58u && methods!=0x571ca0u && methods!=0x571c80u)return 0;
    put(a,p,0xb0,mode);
    /*40b0e0 publishes the mode, then applies the image's explicit source key
     *or samples its first pixel via4db630. File images are drawn directly with
     *that key; composed image/text surfaces must configure their actual owner. */
    if(mode==101u && get(a,p,0xb4) && !image_find(a,get(a,p,0xb4)))
        return !a->memory.fault && game_menu_text_surface_key(a,get(a,p,0xb4),get(a,p,0x13c));
    return !a->memory.fault;
}
static GameAddress button_create(GameApplication *a, size_t size,uint32_t identity,
    const GameAddress names[4], uint32_t route, uint32_t recipient,
    uint32_t command, uint32_t callback) {
    static const unsigned slots[] = {0x58,0x5c,0x60,0x8c};
    GameAddress p = scene_projection_storage_allocate(size, "portable menu button");
    unsigned i;
    if (!p) return 0;
    game_memory_fill_u8(&a->memory, p, size, 0, 0);
    state_helper_construct_packet(&a->memory, p, identity);
    gameplay_state_initialize_presentation_packet(&a->memory, p);
    for (i = 0; i < 4u; ++i) {
        GameAddress child = names[i] ? game_menu_image_create(a, UINT32_MAX, names[i]) : 0;
        if (names[i] && !child) { game_menu_widget_destroy(a, p); return 0; }
        put(a, p, slots[i], child);
    }
    put(a, p, 0x90, get(a, p, 0x58));
    if (get(a, p, 0x90)) {
        MenuImage *image = image_find(a, get(a, p, 0x90));
        if (!image) { game_menu_widget_destroy(a, p); return 0; }
        put(a, p, 0x28, image->bitmap.width); put(a, p, 0x2c, image->bitmap.height);
    }
    copy_rect(a, p, 0x30, 0x20);
    put(a, p, 0x7c, route); put(a, p, 0x80, recipient); put(a, p, 0x64, command);
    put(a, p, 0x6c, callback);
    return a->memory.fault ? 0 : p;
}
GameAddress game_menu_button_create(GameApplication *a,uint32_t identity,const GameAddress names[4],
    uint32_t route,uint32_t recipient,uint32_t command,uint32_t callback) {
    return button_create(a,0x94,identity,names,route,recipient,command,callback);
}
/* 00520ab0: the inherited button, selector and dismiss region have one owner.
 * 98 borrows a row owned by 94; a0 borrows the parent controller. ac, unused
 * by retail, records the owned capture identity so repeated close cannot free
 * the parent. Text coordinates deliberately remain independent of button x/y. */
GameAddress game_menu_combo_create(GameApplication *a,uint32_t id,const GameAddress names[4],
    uint32_t text_x,uint32_t text_y,GameAddress scroll_name,GameAddress background_name,
    uint32_t width,uint32_t height,uint32_t callback) {
    GameAddress p=button_create(a,0xc4,id,names,UINT32_MAX,UINT32_MAX,UINT32_MAX,0),child;
    if(!p)return 0;
    put(a,p,0,0x575ac0);put(a,p,0xa4,text_x);put(a,p,0xa8,text_y);
    put(a,p,0xb4,24);put(a,p,0xb8,0u-4u);put(a,p,0xbc,22);
    child=game_menu_selector_create(a,id+1u,15,callback,scroll_name);
    if(!child)goto fail;
    put(a,p,0x94,child);
    if(!game_menu_selector_resize(a,child,width,height))goto fail;
    child=game_menu_click_region_create(a,id+2u,id,1009,0,0);
    if(!child)goto fail;
    put(a,p,0x9c,child);put(a,child,0x14,640);put(a,child,0x18,480);
    child=background_name?game_menu_image_create(a,UINT32_MAX,background_name):
        game_menu_image_surface_create(a,width,height,0);
    if(!child)goto fail;
    put(a,p,0xc0,child);if(a->memory.fault)goto fail;return p;
fail:
    (void)game_menu_widget_destroy(a,p);return 0;
}
/* 00520ec0/00521040: capture executes ordinary controller callbacks. A nested
 * scrollbar gesture is ended before retiring this capture, also on destruction. */
int game_menu_combo_close(GameApplication *a,GameAddress p,GameAddress chosen) {
    GameAddress capture=get(a,p,0xac),parent=get(a,p,0xa0),selector=get(a,p,0x94);
    if(chosen)put(a,p,0x98,chosen);
    if(!capture)return !a->memory.fault;
    if(game_memory_read_u32(&a->memory,0x627ab4)==capture &&
       !game_menu_selector_capture_end(a,selector))return 0;
    if(game_memory_read_u32(&a->memory,0x59d4c0)!=capture)return 0;
    if(!game_menu_widget_enable(a,p,0) || !game_menu_widget_enable(a,selector,0) ||
       !game_menu_widget_enable(a,get(a,p,0x9c),0))return 0;
    game_menu_controller_destroy(a,capture);put(a,p,0xac,0);
    game_memory_write_u32(&a->memory,0x59d4c0,parent);
    if(!game_menu_widget_visible(a,get(a,p,0xc0),0) ||
       !game_menu_widget_visible(a,selector,0))return 0;
    game_memory_write_u32(&a->memory,0x58fd3c,1);put(a,p,0x48,1);dirty(a,p);
    return !a->memory.fault;
}
int game_menu_combo_open(GameApplication *a,GameAddress p) {
    GameAddress parent,capture;uint32_t recipient,callback;
    if(get(a,p,0xac))return !a->memory.fault;
    if(get(a,p,0x44)!=3u) {
        if(!game_menu_widget_base_state(a,p,2))return 0;
        recipient=get(a,p,0x84);
        if(recipient!=UINT32_MAX) {
            if(!game_menu_request(a,game_memory_read_u32(&a->memory,0x59d4c0),get(a,p,0x7c),recipient,get(a,p,0x88)))return 0;
        } else {
            callback=get(a,p,0x70);
            if(callback && !game_menu_button_callback(a,callback,p,0x201,get(a,p,0x88)))return 0;
        }
    }
    parent=game_memory_read_u32(&a->memory,0x59d4c0);
    if(!parent)return 0;
    capture=game_menu_controller_construct(a);if(!capture)return 0;
    put(a,p,0xa0,parent);put(a,p,0xac,capture);
    game_memory_write_u32(&a->memory,0x59d4c0,capture);
    if(!game_menu_widget_visible(a,get(a,p,0xc0),1) ||
       !game_menu_widget_visible(a,get(a,p,0x94),1) ||
       !game_menu_register_tick(a,capture,p,0x520eb0,0) ||
       !game_menu_widget_enable(a,get(a,p,0x9c),1) ||
       !game_menu_widget_enable(a,get(a,p,0x94),1) ||
       !game_menu_register_delivery(a,capture,p,1009,0x521040)) {
        (void)game_menu_combo_close(a,p,0);return 0;
    }
    game_memory_write_u32(&a->memory,0x58fd3c,1);return !a->memory.fault;
}
/* 0051ded0 differs from the radio group: selected clicks can turn it off. */
GameAddress game_menu_checkbox_create(GameApplication *a,const GameAddress names[4],uint32_t callback) {
    GameAddress p=button_create(a,0x98,UINT32_MAX,names,0,UINT32_MAX,0,callback);
    if(p){put(a,p,0,0x575998);put(a,p,0x94,0);}return p;
}
/* 0051c7c0/0051cc20: four borrowed images, one owned control record. */
int game_menu_button_borrowed_initialize(GameApplication *a,GameAddress p,uint32_t identity,const GameAddress images[4],
    uint32_t route,uint32_t recipient,uint32_t command,uint32_t press,uint32_t press_argument) {
    static const unsigned slots[]={0x58,0x5c,0x60,0x8c};unsigned i;
    if(!p)return 0;
    game_memory_fill_u8(&a->memory,p,0xa0,0,0);state_helper_construct_packet(&a->memory,p,identity);
    gameplay_state_initialize_presentation_packet(&a->memory,p);
    for(i=0;i<4;++i)put(a,p,slots[i],images[i]);
    put(a,p,0x90,images[0]);put(a,p,0x68,1);
    if(images[0]) {
        put(a,p,0x28,get(a,images[0],0x28)-get(a,images[0],0x20));
        put(a,p,0x2c,get(a,images[0],0x2c)-get(a,images[0],0x24));
    }
    copy_rect(a,p,0x30,0x20);put(a,p,0x7c,route);put(a,p,0x80,recipient);
    put(a,p,0x64,command);put(a,p,0x84,press);put(a,p,0x88,press_argument);
    return !a->memory.fault;
}
GameAddress game_menu_button_borrowed_create(GameApplication *a,uint32_t identity,const GameAddress images[4],
    uint32_t route,uint32_t recipient,uint32_t command,uint32_t press,uint32_t press_argument) {
    GameAddress p=scene_projection_storage_allocate(0xa0,"portable borrowed menu button");
    if(!p)return 0;
    if(!game_menu_button_borrowed_initialize(a,p,identity,images,route,recipient,command,press,press_argument)) {
        scene_projection_storage_free(p);return 0;
    }
    return p;
}
/* 0040ad90: a mutable offscreen image owned by the native surface service. */
int game_menu_image_surface_initialize(GameApplication *a,GameAddress p,uint32_t width,uint32_t height,uint32_t flags) {
    game_memory_fill_u8(&a->memory,p,0x16c,0,0);
    state_helper_construct_packet(&a->memory,p,UINT32_MAX);
    put(a,p,0,0x571c58);put(a,p,0xb0,100);
    if(!game_menu_text_surface_create(a,p,width,height,0)) {
        (void)game_menu_text_surface_release(a,p);return 0;
    }
    put(a,p,0xb4,p);put(a,p,0x168,flags);put(a,p,0x28,width);put(a,p,0x2c,height);
    copy_rect(a,p,0x30,0x20);copy_rect(a,p,0x144,0x20);
    if(a->memory.fault){game_menu_text_surface_release(a,p);return 0;}return 1;
}
GameAddress game_menu_image_surface_create(GameApplication *a,uint32_t width,uint32_t height,uint32_t flags) {
    GameAddress p=scene_projection_storage_allocate(0x16c,"portable menu image");
    if(!p)return 0;
    if(!game_menu_image_surface_initialize(a,p,width,height,flags)){scene_projection_storage_free(p);return 0;}
    return p;
}
int game_menu_widget_base_position(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z,int relative) {
    uint32_t width=get(a,p,0x28)-get(a,p,0x20),height=get(a,p,0x2c)-get(a,p,0x24);
    dirty(a,p);
    if(relative){x+=get(a,p,0x20);y+=get(a,p,0x24);z+=get(a,p,0x40);}
    put(a,p,0x20,x);put(a,p,0x24,y);put(a,p,0x28,x+width);put(a,p,0x2c,y+height);put(a,p,0x40,z);
    return !a->memory.fault;
}
/* 0051e630: the radio/toggle specialization shares the actual image owner and
 * base behavior; its two extra words remain in the same allocation. */
GameAddress game_menu_toggle_create(GameApplication *a,uint32_t identity,uint32_t group,
    const GameAddress names[4],uint32_t callback,uint32_t press,uint32_t timer) {
    GameAddress p=button_create(a,0xa0,identity,names,UINT32_MAX,UINT32_MAX,UINT32_MAX,callback);
    if(!p)return 0;
    put(a,p,0,0x5759e0);put(a,p,0x70,press);put(a,p,0x74,timer);
    put(a,p,0x88,UINT32_MAX);put(a,p,0x94,0);put(a,p,0x98,group);return p;
}
/*0051bec0/0051bf40: one click-region owner shared by page construction. */
GameAddress game_menu_click_region_create(GameApplication *a,uint32_t id,uint32_t route,uint32_t recipient,uint32_t command,uint32_t callback) {
    GameAddress p=scene_projection_storage_allocate(0x2c,"portable menu click region");
    if(!p)return 0;
    game_memory_fill_u8(&a->memory,p,0x2c,0,0);
    put(a,p,0,0x5758a0);put(a,p,4,id);put(a,p,0x1c,route);put(a,p,0x20,recipient);put(a,p,0x24,command);put(a,p,0x28,callback);
    return p;
}
/* 00559150/005591c0 and derived 0051d790/0051d6f0. Unsigned arithmetic
 * preserves wrapped 32-bit coordinates without signed-overflow UB. */
int game_menu_widget_position(GameApplication *a, GameAddress p,
    uint32_t x, uint32_t y, uint32_t z, int relative) {
    uint32_t methods = get(a, p, 0);
    if(methods==0x5731c8u || methods==0x5731e8u)return game_menu_widget_base_position(a,p,x,y,z,relative);
    if(methods==0x571c40u)return game_menu_widget_base_position(a,p,x,y,z,relative);
    if(methods==0x575b60u)return game_menu_widget_base_position(a,p,x,y,z,relative);
    static const unsigned absolute_slots[] = {0x60,0x58,0x5c,0x60,0x8c};
    static const unsigned relative_slots[] = {0x90,0x58,0x5c,0x60,0x8c};
    unsigned i;
    if(methods==0x577450u || methods==0x5774e8u)return game_menu_value_position(a,p,x,y,z,relative);
    if(methods==0x575a70u || methods==0x575b08u || methods==0x577400u)return game_menu_selector_position(a,p,x,y,z,relative);
    if (methods != 0x571c58u && methods != 0x571c80u && methods != 0x571ca0u && methods != 0x575908u && methods != 0x575ac0u && methods != 0x575950u && methods != 0x5759e0u && methods!=0x575998u && methods!=0x575ba8u && methods != 0x577a58u && methods!=0x5774a0u && methods!=0x577580u)
        return game_menu_missing_route(a,"widget position",methods);
    if(!game_menu_widget_base_position(a,p,x,y,z,relative))return 0;
    if (methods == 0x575908u || methods==0x575ac0u || methods==0x575950u || methods==0x5759e0u || methods==0x575998u || methods==0x5774a0u || methods==0x577580u) for (i = 0; i < 5u; ++i) {
        GameAddress child = get(a, p, relative ? relative_slots[i] : absolute_slots[i]);
        if (child && !game_menu_widget_position(a, child, get(a,p,0x20), get(a,p,0x24), get(a,p,0x40), 0)) return 0;
    }
    if(methods==0x575ac0u)return
        game_menu_widget_position(a,get(a,p,0xc0),get(a,p,0xa4)+get(a,p,0xb8),get(a,p,0xa8)+get(a,p,0xbc),get(a,p,0x40)+1u,0) &&
        game_menu_selector_position(a,get(a,p,0x94),get(a,p,0xa4)+get(a,p,0xb0),get(a,p,0xa8)+get(a,p,0xb4),get(a,p,0x40)+2u,0);
    if(methods==0x575ba8u && !relative)return game_menu_slider_geometry(a,p);
    return !a->memory.fault;
}
int game_menu_widget_visible(GameApplication *a, GameAddress p, int visible) {
    if (!!get(a, p, 0x14) != !!visible) {
        put(a, p, 0x14, !!visible);
        if (visible ? !game_menu_window_attach(a, p) : !game_menu_window_detach(a, p)) return 0;
    }
    return !a->memory.fault;
}
int game_menu_widget_enable(GameApplication *a, GameAddress p, int enable) {
    if(get(a,p,0)==0x575b60u)return game_menu_text_entry_enable(a,p,enable);
    if(get(a,p,0)==0x5758a0u) {
        GameAddress controller=game_memory_read_u32(&a->memory,0x59d4c0);
        if(!!get(a,p,8)==!!enable)return !a->memory.fault;
        if(!enable){if(!game_menu_unregister(a,controller,p,0))return 0;put(a,p,8,0);return !a->memory.fault;}
        if(!game_menu_register_region(a,controller,p,p+0xc,0x202,0x51c0f0,0) ||
           !game_menu_register_region(a,controller,p,p+0xc,0x201,0x5232d0,0))return 0;
        put(a,p,8,1);return !a->memory.fault;
    }
    GameAddress controller = game_memory_read_u32(&a->memory, 0x59d4c0u);
    if(get(a,p,0)==0x577450u || get(a,p,0)==0x5774e8u)return game_menu_value_enable(a,p,enable);
    if (!!get(a,p,0x48) == !!enable) return !a->memory.fault;
    if (!enable) {
        if (!game_menu_unregister(a, controller, p, 0) || !game_menu_focus_remove(a, p)) return 0;
        put(a,p,0x48,0); return !a->memory.fault;
    }
    if(get(a,p,0)==0x575a70u || get(a,p,0)==0x575b08u || get(a,p,0)==0x577400u) {
        if(!game_menu_selector_enable(a,p))return 0;
        if(get(a,p,0)==0x577400u)return game_menu_register_delivery(a,controller,p,1006,0x54bd30);
        if(get(a,p,0)==0x575b08u)return game_menu_register_delivery(a,controller,p,1011,0x521320) &&
            game_menu_register_delivery(a,controller,p,1013,0x521540);
        return 1;
    }
    if(get(a,p,0)==0x575ba8u) {
        put(a,p,0x48,1);return game_menu_register_region(a,controller,p,p+0x78,0x201,0x522890,0);
    }
    if (!game_menu_focus_add(a,p)) return 0;
    put(a,p,0x48,1);
    if (!game_menu_register_region(a,controller,p,p+0x20u,0x202u,0x5232b0u,0) ||
        !game_menu_register_region(a,controller,p,p+0x20u,0x201u,0x5232c0u,0)) return 0;
    if (get(a,p,0x74) && !game_menu_register_tick(a,controller,p,0x51d6d0u,0)) return 0;
    if (get(a,p,0x5c) &&
        (!game_menu_register_region(a,controller,p,p+0x20u,0x20au,0x523290u,0) ||
         !game_menu_register_region(a,controller,p,p+0x20u,0x20bu,0x5232a0u,0))) return 0;
    if(get(a,p,0)==0x5759e0u && !game_menu_register_delivery(a,controller,p,1002,0x51ea30))return 0;
    return !a->memory.fault;
}
int game_menu_widget_destroy(GameApplication *a, GameAddress p) {
    static const unsigned slots[] = {0x58,0x5c,0x60,0x8c};
    uint32_t methods;
    unsigned i;
    if (!p) return 1;
    methods = get(a,p,0);
    /*44e0a0 owns only the allocation.559240 first detaches the shared base
     *widget from its actual window; it owns no image/controller children. */
    if(methods==0x573180u)return !a->memory.fault && scene_projection_storage_free(p);
    if(methods==0x577a58u){
        if(!game_menu_widget_visible(a,p,0))return 0;
        put(a,p,0,0x573180u);return !a->memory.fault && scene_projection_storage_free(p);
    }
    if(methods==0x575b60u) {
        if(!game_menu_text_entry_enable(a,p,0) || !game_menu_widget_destroy(a,get(a,p,0x74)))return 0;
        put(a,p,0x74,0);put(a,p,0,0x5758b0);
        if(!game_menu_widget_visible(a,p,0))return 0;
        return !a->memory.fault && scene_projection_storage_free(p);
    }
    if(methods==0x575ac0u) {
        static const unsigned owned[]={0xc0,0x94,0x9c};
        if(!game_menu_combo_close(a,p,0))return 0;
        put(a,p,0x98,0);
        for(i=0;i<3u;++i) {
            if(!game_menu_widget_destroy(a,get(a,p,owned[i])))return 0;
            put(a,p,owned[i],0);
        }
        put(a,p,0,0x575908u);methods=0x575908u;
    }
    if(methods==0x5731e8u)return game_menu_research_preview_destroy(a,p);
    if(methods==0x5731c8u){extern int game_campaign_preview_destroy(GameApplication *,GameAddress);return game_campaign_preview_destroy(a,p);}
    if(methods==0x577580u)return game_menu_choice_release(a,p);
    if(methods==0x571c40u)return game_menu_movie_destroy(a,p);
    if(methods==0x5758a0u) {
        if(get(a,p,8) && !game_menu_unregister(a,game_memory_read_u32(&a->memory,0x59d4c0),p,0))return 0;
        put(a,p,8,0);put(a,p,0,0x573180);return !a->memory.fault && scene_projection_storage_free(p);
    } else if(methods==0x575ba8u) {
        if(!game_menu_widget_destroy(a,get(a,p,0x88)))return 0;
        put(a,p,0x88,0);if(!game_menu_widget_enable(a,p,0))return 0;
    } else if(methods==0x575a70u || methods==0x575b08u || methods==0x577400u) {
        if(!game_menu_selector_release(a,p))return 0;
    } else if (methods==0x577450u || methods==0x5774e8u) {
        if(!game_menu_value_release(a,p))return 0;
    } else if (methods == 0x575908u || methods==0x575950u || methods==0x5759e0u || methods==0x575998u || methods==0x5774a0u) {
        if (!get(a,p,0x68)) for (i=0;i<4u;++i) {
            if (!game_menu_widget_destroy(a,get(a,p,slots[i]))) return 0;
            put(a,p,slots[i],0);
        }
        if (!game_menu_widget_enable(a,p,0)) return 0;
    } else if (methods == 0x571c80u) {
        if(!game_menu_text_image_release(a,p))return 0;
    } else if (methods == 0x571ca0u) {
        uint32_t count=get(a,p,0x2fc);
        if(count>100u)return 0;
        for(i=0;i<count;++i) {
            image_release(a,get(a,p,0x16c+i*4u));put(a,p,0x16c+i*4u,0);
        }
        put(a,p,0xb4,0);
    } else if (methods == 0x571c58u) {
        if (!get(a,p,0x140)) {
            if(get(a,p,0xb4) && !image_find(a,p) && !game_menu_text_surface_release(a,p))return 0;
            image_release(a,p);put(a,p,0xb4,0);
        }
    } else return game_menu_missing_route(a,"widget retirement",methods);
    if (!game_menu_widget_visible(a,p,0)) return 0;
    put(a,p,0,0x573180u);
    return !a->memory.fault && scene_projection_storage_free(p);
}
static GameUiRect rect_read(GameApplication *a, GameAddress p, unsigned offset) {
    GameUiRect r;
    r.left=(int32_t)get(a,p,offset); r.top=(int32_t)get(a,p,offset+4);
    r.right=(int32_t)get(a,p,offset+8); r.bottom=(int32_t)get(a,p,offset+12); return r;
}
static int intersect(GameUiRect *r, const GameUiRect *clip) {
    if (r->left<clip->left) r->left=clip->left;
    if (r->top<clip->top) r->top=clip->top;
    if (r->right>clip->right) r->right=clip->right;
    if (r->bottom>clip->bottom) r->bottom=clip->bottom;
    return r->left<r->right && r->top<r->bottom;
}
/* Native owner lookup replaces image/surface virtual dispatch during composition. */
int game_menu_image_compose(GameApplication *a,GameAddress destination,GameAddress source,int32_t x,int32_t y,int transparent,int mirror) {
    extern int game_menu_draw_bitmap_on_surface(GameApplication *,GameAddress,const AssetBitmap *,const GameUiRect *,const GameUiRect *,int,uint32_t,int);
    extern int game_menu_surface_compose_copy(GameApplication *,GameAddress,GameAddress,const GameUiRect *,int32_t,int32_t,int,int);
    MenuImage *image=image_find(a,get(a,source,0xb4));GameUiRect src=rect_read(a,source,0x144),dst;
    int64_t right=(int64_t)x+src.right-src.left,bottom=(int64_t)y+src.bottom-src.top;
    if(right>INT32_MAX || right<INT32_MIN || bottom>INT32_MAX || bottom<INT32_MIN)return 0;
    dst.left=x;dst.top=y;dst.right=(int32_t)right;dst.bottom=(int32_t)bottom;
    if(image) {
        uint32_t key=get(a,source,0x13c);
        if(!key)key=asset_bitmap_pixel(&image->bitmap,0u,0u);
        return game_menu_draw_bitmap_on_surface(a,destination,&image->bitmap,&src,&dst,transparent,key,mirror);
    }
    return game_menu_surface_compose_copy(a,destination,get(a,source,0xb4),&src,x,y,transparent,mirror);
}
int game_menu_widget_draw(GameApplication *a, GameAddress p, const GameUiRect *clip) {
    uint32_t methods=get(a,p,0);
    if(methods==0x577a58u)return !a->memory.fault; /*559230 is the verified empty base draw. */
    if(methods==0x5731e8u)return game_menu_research_preview_draw(a,p,clip);
    if(methods==0x5731c8u){extern int game_campaign_preview_draw(GameApplication *,GameAddress,const GameUiRect *);return game_campaign_preview_draw(a,p,clip);}
    if(methods==0x575b60u)return game_menu_text_entry_draw(a,p,clip);
    if(methods==0x575ac0u) {
        static const unsigned frames[]={0x58,0x5c,0x60,0x8c};
        uint32_t state=get(a,p,0x44),x,y,z;int ok,restored;
        GameAddress frame=state<4u?get(a,p,frames[state]):0,row=get(a,p,0x98);
        if(!frame)frame=get(a,p,0x58);
        if(frame && !game_menu_widget_draw(a,frame,clip))return 0;
        if(!row)return !a->memory.fault;
        state=get(a,row,0x44);x=get(a,row,0x20);y=get(a,row,0x24);z=get(a,row,0x40);
        ok=game_menu_widget_position(a,row,get(a,p,0xa4),get(a,p,0xa8),z,0) && game_menu_widget_base_state(a,row,1);
        if(ok){GameUiRect bounds=rect_read(a,row,0x20);ok=game_menu_widget_draw(a,row,&bounds);}
        restored=game_menu_widget_position(a,row,x,y,z,0);
        if(!game_menu_widget_base_state(a,row,state))restored=0;
        return ok && restored && !a->memory.fault;
    }
    if(methods==0x571c40u)return game_menu_movie_draw(a,p,clip);
    GameUiRect destination;
    if (methods==0x571c80u)return game_menu_text_image_draw(a,p,clip);
    if(methods==0x575a70u || methods==0x575b08u || methods==0x577400u)return game_menu_selector_draw(a,p,clip);
    if(methods==0x575ba8u)return game_menu_slider_draw(a,p,clip);
    if(methods==0x577450u || methods==0x5774e8u)return game_menu_value_draw(a,p,clip);
    if (methods==0x575908u || methods==0x575950u || methods==0x5759e0u || methods==0x575998u || methods==0x5774a0u || methods==0x577580u) {
        static const unsigned slots[]={0x58,0x5c,0x60,0x8c};
        uint32_t state=get(a,p,0x44);
        /* 0051eaa0 repositions shared frames before each toggle presentation.
         * Its selected-border methods 004abd30/40/50 are actual retail no-ops. */
        if(methods==0x5759e0u && !game_menu_widget_position(a,p,get(a,p,0x20),get(a,p,0x24),get(a,p,0x40),0))return 0;
        GameAddress child=state<4u?get(a,p,slots[state]):0;
        if (!child) child=get(a,p,0x58);
        if (!child) return !a->memory.fault;
        destination=rect_read(a,child,0x20);
        if (clip && !intersect(&destination,clip)) return 1;
        return game_menu_widget_draw(a,child,&destination);
    }
    if (methods==0x571c58u || methods==0x571ca0u) {
        MenuImage *image=image_find(a,get(a,p,0xb4));
        GameUiRect source;
        destination=rect_read(a,p,0x20);
        if (clip && !intersect(&destination,clip)) return 1;
        if (get(a,p,0x164)) {
            GameUiRect own_clip=rect_read(a,p,0x154);
            if (!intersect(&destination,&own_clip)) return 1;
        }
        source.left=(int32_t)((uint32_t)destination.left-get(a,p,0x20));
        source.top=(int32_t)((uint32_t)destination.top-get(a,p,0x24));
        source.right=(int32_t)((uint32_t)destination.right-get(a,p,0x20));
        source.bottom=(int32_t)((uint32_t)destination.bottom-get(a,p,0x24));
        if (get(a,p,0x10)) source=rect_read(a,p,0x144);
        if (get(a,p,0xb0)!=100u && get(a,p,0xb0)!=101u) return 0;
        if(!image)return game_menu_text_surface_draw(a,get(a,p,0xb4),&source,&destination,get(a,p,0xb0)==101u);
        {
            uint32_t key=get(a,p,0x13c);
            if(!key)key=asset_bitmap_pixel(&image->bitmap,0u,0u);
            return game_menu_draw_bitmap(a,&image->bitmap,&source,&destination,get(a,p,0xb0)==101u,key);
        }
    }
    return 0; /* Unreconstructed presentation family, never a successful no-op. */
}
int game_menu_widget_base_state(GameApplication *a,GameAddress p,uint32_t state) {
    put(a,p,0x44,state); dirty(a,p); return !a->memory.fault;
}
/* 0051ea50/0051ea30: selection excludes siblings through the real controller.
 * Temporarily hide this group while delivering, exactly as the original does. */
int game_menu_toggle_state(GameApplication *a,GameAddress p,uint32_t state) {
    uint32_t group;int ok;
    if(!game_menu_widget_base_state(a,p,state))return 0;
    if(state!=2u)return 1;
    group=get(a,p,0x98);put(a,p,0x98,UINT32_MAX);
    ok=game_menu_broadcast(a,game_memory_read_u32(&a->memory,0x59d4c0),1002,group);
    put(a,p,0x98,group);return ok && !a->memory.fault;
}
int game_menu_toggle_group(GameApplication *a,GameAddress p,uint32_t group) {
    return get(a,p,0x98)!=group ? !a->memory.fault : game_menu_toggle_state(a,p,0);
}
static int toggle_input(GameApplication *a,GameAddress p,uint32_t message) {
    uint32_t state=get(a,p,0x44),sound,recipient,callback;
    /* Press is the original empty 0051e390. Release's 0051ea00 wrapper
     * leaves an already selected radio alone; 0051e2f0 handles selection. */
    if(message==0x201u)return !a->memory.fault;
    if(message==0x20au) {
        if(state==2u || state==3u)return !a->memory.fault;
        if(game_memory_read_u32(&a->memory,0x59d4cc) && game_memory_read_u32(&a->memory,0x627ab8)!=p)return !a->memory.fault;
        if(!game_menu_widget_base_state(a,p,game_memory_read_u32(&a->memory,0x59d4cc)?2u:1u))return 0;
        game_memory_write_u32(&a->memory,0x627ab8,0);
        sound=get(a,p,0x50);return sound==UINT32_MAX || game_menu_sound(a,sound);
    }
    if(message==0x20bu)return state==2u || state==3u ? !a->memory.fault : game_menu_widget_base_state(a,p,0);
    if(message!=0x202u)return 0;
    if(state==2u)return !a->memory.fault;
    if(state==0u || state==1u) {
        if(!game_menu_toggle_state(a,p,2))return 0;
        put(a,p,0x94,1);
    }
    dirty(a,p);sound=get(a,p,0x54);
    if(sound!=UINT32_MAX && !game_menu_sound(a,sound))return 0;
    recipient=get(a,p,0x80);
    if(recipient!=UINT32_MAX)return game_menu_request(a,game_memory_read_u32(&a->memory,0x59d4c0),get(a,p,0x7c),recipient,get(a,p,0x64));
    callback=get(a,p,0x6c);
    return !callback || game_menu_button_callback(a,callback,p,0x202,get(a,p,0x64));
}
/* Real press/release/hover consumers and command dispatch (0051d5b0/0051d670).
 * Retail hit testing is strict at all four edges. */
static int button_input(GameApplication *a,GameAddress p,uint32_t message,int32_t x,int32_t y) {
    uint32_t state=get(a,p,0x44), result=1, id, callback, argument;
    int inside=x>(int32_t)get(a,p,0x20) && x<(int32_t)get(a,p,0x28) &&
        y>(int32_t)get(a,p,0x24) && y<(int32_t)get(a,p,0x2c);
    if (state==3u) return !a->memory.fault;
    if (message==0x20au) {
        GameAddress captured=game_memory_read_u32(&a->memory,0x627ab8u);
        if (game_memory_read_u32(&a->memory,0x59d4ccu) && captured!=p) return !a->memory.fault;
        game_menu_widget_base_state(a,p,game_memory_read_u32(&a->memory,0x59d4ccu) && captured==p?2u:1u);
        game_memory_write_u32(&a->memory,0x627ab8u,0);
        id=get(a,p,0x50); return id==UINT32_MAX || game_menu_sound(a,id);
    }
    if (message==0x20bu) {
        if (state==2u) game_memory_write_u32(&a->memory,0x627ab8u,p);
        return game_menu_widget_base_state(a,p,0);
    }
    if (message==0x202u && state!=2u) return !a->memory.fault;
    if (message!=0x201u && message!=0x202u) return 0;
    game_menu_widget_base_state(a,p,message==0x201u?2u:(inside?1u:0u));
    if (message==0x202u) {
        id=get(a,p,0x54);
        if (id!=UINT32_MAX && !game_menu_sound(a,id)) return 0;
    }
    id=get(a,p,message==0x201u?0x84:0x80);
    argument=get(a,p,message==0x201u?0x88:0x64);
    if (id!=UINT32_MAX) return game_menu_request(a,game_memory_read_u32(&a->memory,0x59d4c0u),get(a,p,0x7c),id,argument);
    callback=get(a,p,message==0x201u?0x70:0x6c);
    if (callback) result=game_menu_button_callback(a,callback,p,message,argument);
    return result && !a->memory.fault;
}
int game_menu_button_input(GameApplication *a,GameAddress p,uint32_t message,int32_t x,int32_t y) {
    uint32_t methods=get(a,p,0),callback;GameAddress parent,table;uint32_t kind=0;int result;
    if(methods==0x575b60u) {
        uint32_t state=get(a,p,0x44),sound;
        if(message==0x201u)return game_menu_text_entry_activate(a,p);
        if(state==3u)return !a->memory.fault;
        if(message==0x20au) {
            if(game_memory_read_u32(&a->memory,0x59d4cc) && game_memory_read_u32(&a->memory,0x627ab8)!=p)return !a->memory.fault;
            if(!game_menu_widget_base_state(a,p,game_memory_read_u32(&a->memory,0x59d4cc)?2:1))return 0;
            game_memory_write_u32(&a->memory,0x627ab8,0);sound=get(a,p,0x50);
            return sound==UINT32_MAX || game_menu_sound(a,sound);
        }
        if(message==0x20bu){if(state==2)game_memory_write_u32(&a->memory,0x627ab8,p);return game_menu_widget_base_state(a,p,0);}
        if(message!=0x202u)return 0;
        if(state!=2u)return !a->memory.fault;
        if(!game_menu_widget_base_state(a,p,x>(int32_t)get(a,p,0x20) && x<(int32_t)get(a,p,0x28) && y>(int32_t)get(a,p,0x24) && y<(int32_t)get(a,p,0x2c)?1:0))return 0;
        sound=get(a,p,0x54);return sound==UINT32_MAX || game_menu_sound(a,sound);
    }
    if(methods==0x575ac0u && message==0x202u)return game_menu_combo_open(a,p);
    if(methods==0x5759e0u)return toggle_input(a,p,message);
    if(methods==0x575998u) {
        if(message!=0x202)return toggle_input(a,p,message);
        if(get(a,p,0x44)==2){game_menu_widget_base_state(a,p,0);put(a,p,0x94,0);}
        else if(get(a,p,0x44)<=1){game_menu_widget_base_state(a,p,2);put(a,p,0x94,1);}
        dirty(a,p);
        callback=get(a,p,0x54);if(callback!=UINT32_MAX && !game_menu_sound(a,callback))return 0;
        callback=get(a,p,0x80);
        if(callback!=UINT32_MAX)return game_menu_request(a,game_memory_read_u32(&a->memory,0x59d4c0),get(a,p,0x7c),callback,get(a,p,0x64));
        callback=get(a,p,0x6c);
        return !callback || game_menu_button_callback(a,callback,p,0x202,get(a,p,0x64));
    }
    if((methods==0x577450u || methods==0x5774e8u) && (message==0x20a || message==0x20b))return game_menu_value_hover(a,p,message==0x20a);
    if(methods==0x575a70u || methods==0x575b08u || methods==0x577400u) {
        if(message==0x20a)return button_input(a,p,message,x,y);
        if(message==0x20b)return game_menu_selector_hover_exit(a,p,x,y);
        return game_menu_selector_pointer(a,p,message,x,y);
    }
    if(methods!=0x575908u && methods!=0x575ac0u && methods!=0x575950u && methods!=0x5774a0u && methods!=0x577580u)return 0;
    result=button_input(a,p,message,x,y);
    if(!result || methods!=0x5774a0u || (message!=0x20au && message!=0x20bu))return result;
    callback=get(a,p,message==0x20au?0x98:0x9c);if(!callback)return !a->memory.fault;
    parent=get(a,p,0x94);table=get(a,parent,0x64);
    if(!table){table=get(a,parent,0x68);kind=1;}
    return !table || game_menu_button_callback(a,callback,parent,kind,table);
}
