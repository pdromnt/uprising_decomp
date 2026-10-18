#include "menu_widgets.h"

/* 0054bf70/0054c210. These are existing menu-image owners, including slots
 * populated by later consumers. No separate pixel/object mirrors are retained. */
int game_menu_text_cache_release(GameApplication *a) {
    unsigned i;
    for(i=0;i<10;++i) {
        GameAddress slot=0x59dbc0u+i*4u,object=game_memory_read_u32(&a->memory,slot);
        if(object && !game_menu_widget_destroy(a,object))return 0;
        game_memory_write_u32(&a->memory,slot,0);
    }
    return !a->memory.fault;
}
int game_menu_text_cache_initialize(GameApplication *a) {
    static const struct {GameAddress slot,name;int transparent;} images[]={
        {0x59dbc0,0x59dd3c,1},{0x59dbc4,0x59dd58,1},{0x59dbcc,0x59dd74,1},
        {0x59dbd0,0x59dd90,1},{0x59dbd8,0x59ddac,0},{0x59dbdc,0x59ddcc,0},
        {0x59dbe0,0x59ddec,0},{0x59dbe4,0x59de0c,0}};
    unsigned i;
    /*Shared value controls borrow this family. Reject a competing cache rather
     *than overwrite live owning slots and orphan their allocations. */
    for(i=0;i<10;++i)if(game_memory_read_u32(&a->memory,0x59dbc0u+i*4u))return 0;
    for(i=0;i<sizeof(images)/sizeof(images[0]);++i) {
        GameAddress object=game_menu_image_create(a,UINT32_MAX,images[i].name);
        if(!object){(void)game_menu_text_cache_release(a);return 0;}
        game_memory_write_u32(&a->memory,images[i].slot,object);
        if(images[i].transparent && !game_menu_image_mode(a,object,101)) {
            (void)game_menu_text_cache_release(a);return 0;
        }
    }
    return !a->memory.fault;
}

typedef struct PopupImageFamily {
    GameAddress count,table;
    unsigned capacity,owners;
    int reset,borrowed_selection;
} PopupImageFamily;
/* Tables retain their verified twelve-byte rows. A selection can borrow an
 * image from the first slot; it is not another allocation or owning pointer. */
static const PopupImageFamily popup_families[]={
    {0x62878c,0x628988,22,2,1,0},{0x627ac0,0x627bb8,63,3,0,0},
    {0x628888,0x627eb0,148,2,1,0},{0x627ac4,0x6285a8,20,3,0,0},
    {0x62888c,0x628698,20,1,1,1},{0x627ea8,0x628898,20,3,0,0},
    {0x628788,0x627ac8,20,1,1,1},{0x628890,0x628798,20,1,1,1}};
typedef struct PopupImageSpec {GameAddress name;unsigned char family,column,finish,alias;} PopupImageSpec;
static const PopupImageSpec popup_images[]={
#include "menu_popup_images.inc"
};
static int popup_row_release(GameApplication *a,const PopupImageFamily *family,unsigned row) {
    GameAddress base=family->table+row*12u;unsigned i;
    for(i=0;i<family->owners;++i) {
        GameAddress object=game_memory_read_u32(&a->memory,base+i*4u);
        if(object && !game_menu_widget_destroy(a,object))return 0;
        game_memory_write_u32(&a->memory,base+i*4u,0);
    }
    if(family->borrowed_selection)game_memory_write_u32(&a->memory,base+4u,0);
    return !a->memory.fault;
}
/* 00527eb0. Preserve original family order, and retire every owner once. */
int game_menu_popup_release(GameApplication *a) {
    static const unsigned order[]={0,2,4,6,7,1,3,5};unsigned i,row,count;
    for(i=0;i<8;++i) {
        const PopupImageFamily *family=&popup_families[order[i]];
        count=game_memory_read_u32(&a->memory,family->count);
        if(count>family->capacity)return 0;
        for(row=0;row<count;++row)if(!popup_row_release(a,family,row))return 0;
        game_memory_write_u32(&a->memory,family->count,0);
    }
    return !a->memory.fault;
}
/* 00524af0. Load real image assets in reference order, atomically publishing
 * each complete row. Local pending IDs own partial rows only during startup;
 * no second persistent list or per-frame synchronization is introduced. */
int game_menu_popup_initialize(GameApplication *a) {
    GameAddress pending[8][3]={{0}};unsigned baseline[8],i,j,k,count;
    for(i=0;i<8;++i) {
        baseline[i]=game_memory_read_u32(&a->memory,popup_families[i].count);
        if(baseline[i]>popup_families[i].capacity || (popup_families[i].reset && baseline[i]))return 0;
    }
    for(i=0;i<sizeof(popup_images)/sizeof(popup_images[0]);++i) {
        const PopupImageSpec *spec=&popup_images[i];const PopupImageFamily *family=&popup_families[spec->family];
        count=game_memory_read_u32(&a->memory,family->count);
        if(count>=family->capacity || pending[spec->family][spec->column])goto fail;
        pending[spec->family][spec->column]=game_menu_image_create(a,UINT32_MAX,spec->name);
        if(!pending[spec->family][spec->column])goto fail;
        if(spec->finish) {
            GameAddress base=family->table+count*12u;
            for(j=0;j<family->owners;++j)if(!pending[spec->family][j])goto fail;
            for(j=0;j<family->owners;++j)game_memory_write_u32(&a->memory,base+j*4u,pending[spec->family][j]);
            if(spec->alias)game_memory_write_u32(&a->memory,base+4u,pending[spec->family][0]);
            for(j=0;j<3;++j)pending[spec->family][j]=0;
            game_memory_write_u32(&a->memory,family->count,count+1);
        }
        if(a->memory.fault)goto fail;
    }
    return !a->memory.fault;
fail:
    for(i=0;i<8;++i) {
        const PopupImageFamily *family=&popup_families[i];
        for(j=0;j<3;++j)if(pending[i][j])(void)game_menu_widget_destroy(a,pending[i][j]);
        count=game_memory_read_u32(&a->memory,family->count);
        for(k=baseline[i];k<count && k<family->capacity;++k)(void)popup_row_release(a,family,k);
        game_memory_write_u32(&a->memory,family->count,baseline[i]);
    }
    return 0;
}
