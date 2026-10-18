#include "menu_choice_controls.h"
#include "scene_projection_storage.h"
typedef struct TechRecipe {
    GameAddress value,counter_label,choice_label,profile;
    uint32_t minimum,maximum;
    GameAddress previews[10];unsigned preview_count;
} TechRecipe;
/*Native metadata recovered with both constructor variants and their consumers.
 * Addresses refer to existing data/asset identities, never executable bodies. */
static const TechRecipe technology[]={
    {0x602834,0x599e7c,0x599e8c,0x5e40f8,0,2,{0x599e9c,0x599eac,0x599ebc},3},
    {0x602838,0x599ecc,0x599ed4,0x5e4bd0,0,2,{0x599edc,0x599ef0,0x599f04},3},
    {0x60283c,0x599f18,0x599f2c,0x5eb988,0,2,{0x599f40,0x599f50,0x599f60},3},
    {0x602848,0x599f70,0x599f80,0x5e5a10,UINT32_MAX,2,{0x599f90,0x599fa4,0x599fb8},3},
    {0x602858,0x599fcc,0x599fd8,0x5ef4f8,UINT32_MAX,2,{0x599fe4,0x599ff4,0x59a004},3},
    {0x602830,0x59a014,0x59a020,0x5ee608,UINT32_MAX,2,{0x59a02c,0x59a040,0x59a054},3},
    {0x602840,0x59a068,0x59a078,0x5e4a10,UINT32_MAX,3,{0x59a088,0x59a09c,0x59a0b0,0x59a0c4},4},
    {0x602844,0x59a0d8,0x59a0e4,0x5eb4c8,UINT32_MAX,2,{0x59a0f0,0x59a104,0x59a118},3},
    {0x60285c,0x59a12c,0x59a138,0x5ebe48,UINT32_MAX,1,{0x59a144},1},
    {0x60284c,0x59a154,0x59a164,0x5e17f8,UINT32_MAX,2,{0x59a174,0x59a188,0x59a19c},3},
    {0x602854,0x59a1b0,0x59a1bc,0x5ef9b8,UINT32_MAX,2,{0x59a1c8,0x59a1dc,0x59a1f0},3},
    {0x602850,0x59a204,0x59a210,0x5e48c0,UINT32_MAX,2,{0x59a218,0x59a22c,0x59a240},3},
    {0x602864,0x59a254,0x59a264,0x5ef488,UINT32_MAX,0,{0x59a274},1},
    {0x602868,0x59a288,0x59a298,0x5e2188,UINT32_MAX,0,{0x59a2a8},1},
    {0x602860,0x59a2bc,0x59a2d0,0x5e2568,UINT32_MAX,2,{0x59a2e4,0x59a2f4,0x59a304,0x59a314},4}
};
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int append(GameApplication *a,GameAddress parent,GameAddress item) {
    if(!item)return 0;
    /*The selector becomes the sole row owner before its layout callbacks. */
    return game_menu_selector_append(a,parent,item,1,get(a,item+0x28)-get(a,item+0x20));
}
static int build_technology(GameApplication *a,GameAddress parent,uint32_t mode,uint32_t offset) {
    GameAddress images[3]={get(a,0x627bb8),get(a,0x627bbc),get(a,0x627bc0)};
    unsigned i,j;int choice=mode==18 || mode==32;
    if(!choice && mode!=9 && mode!=11 && mode!=15 && mode!=22)return 0;
    for(i=0;i<sizeof(technology)/sizeof(technology[0]);++i) {
        const TechRecipe *r=&technology[i];GameAddress p;
        if(choice) {
            p=game_menu_choice_create(a,420+i,r->value+offset,r->choice_label,r->minimum,r->maximum,images);
            if(!p)return 0;
            put(a,p+(i?0xb8:0xb4),r->profile);
            for(j=0;j<r->preview_count;++j)if(!game_menu_choice_option(a,p,j,r->previews[j],0)) {
                (void)game_menu_widget_destroy(a,p);return 0;
            }
        } else {
            p=game_menu_value_create(a,420+i,r->value+offset,r->counter_label,1,r->minimum,r->maximum,1,0);
            if(!p)return 0;
            put(a,p+0x80,0x5acbec);
            put(a,p+0x110,get(a,0x628988+i*12));put(a,p+0x114,get(a,0x62898c+i*12));put(a,p+0x118,0);
            put(a,p+0xfc,0x524760);put(a,p+0x100,0x5248e0);
            put(a,get(a,p+0xcc)+0x98,0x5248f0);put(a,get(a,p+0xcc)+0x9c,0x524ab0);put(a,p+0x58,parent);put(a,p+0xf8,0x523680);
            put(a,p+(i?0x68:0x64),r->profile);
            if(!game_menu_value_enable(a,p,1)) {
                (void)game_menu_widget_destroy(a,p);return 0;
            }
        }
        if(a->memory.fault || !append(a,parent,p))return 0;
    }
    return !a->memory.fault;
}
static const TechRecipe units[]={
    {0x6027f0,0x59a324,0x59a32c,0x5ef648,0,4,{0},0},
    {0x6027f4,0x59a334,0x59a33c,0x5e8a80,0,4,{0x59a344,0x59a358,0x59a36c,0x59a380,0x59a394},5},
    {0x6027f8,0x59a3a8,0x59a3ac,0x5eaaf8,0,4,{0x59a3b0,0x59a3c0,0x59a3d0,0x59a3e0,0x59a3f0},5},
    {0x602804,0x59a400,0x59a408,0x5e1488,0,4,{0x59a410,0x59a420,0x59a430,0x59a440,0x59a450},5},
    {0x602820,0x59a460,0x59a46c,0x5e9a38,0,2,{0x59a478,0x59a48c,0x59a4a0},3},
    {0x602814,0x59a4b4,0x59a4bc,0x5e7440,0,2,{0x59a4c4,0x59a4d8,0x59a4ec},3},
    {0x602824,0x59a500,0x59a510,0x5edfd8,0,4,{0x59a520,0x59a534,0x59a548,0x59a55c,0x59a570},5},
    {0x60282c,0x59a584,0x59a590,0x5ee348,0,3,{0x59a59c,0x59a5b0,0x59a5c4,0x59a5d8},4},
    {0x602828,0x59a5ec,0x59a5f8,0x5e2fa8,0,2,{0x59a604,0x59a618,0x59a62c},3}
};
static int build_units(GameApplication *a,GameAddress parent,uint32_t mode,uint32_t offset) {
    GameAddress images[3]={get(a,0x6285a8),get(a,0x6285ac),get(a,0x6285b0)};
    unsigned i,j;int choice=mode==18 || mode==32;
    if(!choice && mode!=9 && mode!=11 && mode!=15 && mode!=22)return 0;
    for(i=0;i<sizeof(units)/sizeof(units[0]);++i) {
        const TechRecipe *r=&units[i];GameAddress p;
        if(choice) {
            p=game_menu_choice_create(a,420+i,r->value+offset,r->choice_label,0,r->maximum,images);
            if(!p)return 0;
            put(a,p+0xb4,r->profile);
            for(j=0;j<r->preview_count;++j)if(!game_menu_choice_option(a,p,j,r->previews[j],0)) {
                (void)game_menu_widget_destroy(a,p);return 0;
            }
        } else {
            p=game_menu_value_create(a,420+i,r->value+offset,r->counter_label,1,0,r->maximum,1,0);
            if(!p)return 0;
            put(a,p+0x80,0x5acbec);put(a,p+0x64,r->profile);
            put(a,p+0x110,get(a,0x627eb0+i*12));put(a,p+0x114,get(a,0x627eb4+i*12));
            put(a,p+0xfc,0x524760);put(a,p+0x100,0x5248e0);
            put(a,get(a,p+0xcc)+0x98,0x5248f0);put(a,get(a,p+0xcc)+0x9c,0x524ab0);
            put(a,p+0x58,parent);put(a,p+0xf8,0x523680);
            if(!game_menu_value_enable(a,p,1)) {
                (void)game_menu_widget_destroy(a,p);return 0;
            }
        }
        if(!append(a,parent,p))return 0;
    }
    return !a->memory.fault;
}
/*The parameter page intentionally uses different limits/prices for local and
 * network setup. These are gameplay values, not cosmetic widget metadata. */
static int build_parameters(GameApplication *a,GameAddress parent,uint32_t mode,uint32_t offset) {
    static const GameAddress labels[5][3]={
        {0x59a640,0x59a640,0x59a648},{0x59a65c,0x59a650,0x59a668},
        {0x59a67c,0x59a674,0x59a684},{0x59a694,0x59a68c,0x59a69c},
        {0x59a6ac,0x59a6a4,0x59a6b4}};
    static const uint32_t prices[5][3]={{16,16,8},{4,6,2},{8,12,4},{6,9,3},{16,24,8}};
    unsigned i;int choice=mode==18 || mode==32,network=mode==15 || mode==22;
    if(!choice && !network && mode!=9 && mode!=11)return 0;
    for(i=0;i<5;++i) {
        uint32_t minimum=i?0:1,maximum=i?(choice?19:network?9:19):9;
        GameAddress p;
        if(choice) {
            GameAddress images[3]={get(a,0x628898+i*12),get(a,0x62889c+i*12),get(a,0x6288a0+i*12)};
            p=game_menu_choice_create(a,420+i,0x602874+i*4+offset,labels[i][2],minimum,maximum,images);
            if(!p)return 0;
            put(a,p+0xa0,prices[i][2]);
        } else {
            p=game_menu_value_create(a,420+i,0x602874+i*4+offset,labels[i][network?1:0],1,minimum,maximum,0,0);
            if(!p)return 0;
            put(a,p+0xf8,0x523680);put(a,p+0x58,parent);
            put(a,p+0x110,get(a,0x628698+i*12));put(a,p+0x114,get(a,0x62869c+i*12));
            put(a,p+0x80,0x5acbec);put(a,p+0x84,prices[i][network?1:0]);
            if(!game_menu_value_enable(a,p,1)) {
                (void)game_menu_widget_destroy(a,p);return 0;
            }
        }
        if(!append(a,parent,p))return 0;
    }
    return !a->memory.fault;
}
static int build_miscellaneous(GameApplication *a,GameAddress parent,uint32_t mode,uint32_t offset) {
    static const GameAddress values[]={0x602f7c,0x602f78,0x602f88,0x602f8c};
    static const GameAddress labels[]={0x59a6bc,0x59a6c4,0x59a6d0,0x59a6d8};
    unsigned i;GameAddress p,names[4]={0x59a6e8,0,0,0};
    (void)offset;
    for(i=0;i<4;++i) {
        p=game_menu_value_create(a,420+i,values[i],labels[i],1,0,1,1,1);
        if(!p)return 0;
        put(a,p+0xf8,0x5236a0);put(a,p+0x58,parent);
        put(a,p+0x110,get(a,0x627ac8+i*12));put(a,p+0x114,get(a,0x627acc+i*12));
        if((mode!=15 && mode!=22) || get(a,0x5f0150)) {
            if(!game_menu_value_enable(a,p,1)) {
                (void)game_menu_widget_destroy(a,p);return 0;
            }
        }
        if(!append(a,parent,p))return 0;
    }
    /*The footer owns its asset, unlike the shared images on the choice rows. */
    p=game_menu_button_create(a,UINT32_MAX,names,UINT32_MAX,0,0,0);
    return p && append(a,parent,p);
}
/*5285a0: replace the complete mixed constructor family, keeping one selector
 * owner and restoring the user's horizontal position across a rebuild. */
int game_menu_research_catalog_build(GameApplication *a,GameAddress parent,uint32_t reset) {
    GameAddress first=get(a,parent+0x5c),controller,root,item;
    uint32_t delta=(!reset && first)?get(a,first+0x20)-get(a,parent+0x20):0;
    uint32_t mode,category,slot,offset;unsigned visited=0;int ok=1;
    if(!game_menu_selector_clear(a,parent))return 0;
    controller=get(a,0x59d4c4);mode=get(a,controller+0x3c);
    if(mode==18 || mode==32)put(a,0x5977c0,0);
    category=get(a,0x58ac04);offset=0;
    if(category<3) {
        root=get(a,0x5f0208);if(!root)return 0;
        slot=game_memory_read_u16(&a->memory,root+8);if(slot>=6)return 0;offset=slot*240;
    }
    switch(category) {
    case 0:ok=build_technology(a,parent,mode,offset);break;
    case 1:ok=build_units(a,parent,mode,offset);break;
    case 2:ok=build_parameters(a,parent,mode,offset);break;
    case 3:ok=build_miscellaneous(a,parent,mode,offset);break;
    }
    if(!ok)return 0; /*Partially built rows remain owned for ordinary teardown. */
    if(delta) {
        put(a,parent+0x18,1);
        for(item=get(a,parent+0x5c);item;item=get(a,item+0x4c)) {
            if(++visited>65536 || !game_menu_widget_position(a,item,delta,0,0,1))return 0;
        }
        if(!get(a,parent+0x1c)) {
            unsigned o;for(o=0;o<16;o+=4)put(a,parent+0x30+o,get(a,parent+0x20+o));
        }
        put(a,parent+0x1c,1);
    }
    if((mode==18 || mode==32) && get(a,parent+0x5c)) {
        int32_t x=(int32_t)(get(a,parent+0x20)+10),y=(int32_t)(get(a,parent+0x24)+10);
        if(!game_menu_selector_pointer(a,parent,0x202,x,y) ||
           !game_menu_selector_pointer(a,parent,0x201,x,y))return 0;
    }
    return !a->memory.fault;
}
