#include "menu_level_catalog.h"
#include "menu_widgets.h"
#include <string.h>
/*00523870: populate the existing selector with its owned text/button children.
 * Category5 excludes campaign/quickstart files from multiplayer; category4
 * excludes campaign from quickstart. The cheat flag deliberately removes filters. */
int game_menu_quick_catalog(GameApplication *a,GameAddress selector,uint32_t group) {
    static const char *campaign[]={"CAMPAIGN"},*multiplayer[]={"MULTIPLAYER"},*quick[]={"QUICKSTART"};
    static const char *excluded[]={"CAMPAIGN","QUICKSTART"};
    GameLevelNames names,omit;const char *const *filters=0;unsigned count=0,i,j;int exclusions=0;
    GameAddress state=game_memory_read_u32(&a->memory,0x59d4c4);uint32_t next,width;
    if(!selector || !state || !game_menu_selector_clear(a,selector))return 0;
    game_memory_write_u32(&a->memory,0x598118,0);next=game_memory_read_u32(&a->memory,state+0x14);
    if(next==15){filters=multiplayer;count=1;}
    else {
        if(next!=9 && game_memory_read_u32(&a->memory,state+0x3c)!=9)return !a->memory.fault;
        if(!game_memory_read_u32(&a->memory,0x58ac08)) {
            if(group==6){filters=campaign;count=1;}
            else if(group==5){filters=multiplayer;count=1;exclusions=2;}
            else if(group==4){filters=quick;count=1;exclusions=1;}
            else return !a->memory.fault;
        }
    }
    if(!game_menu_level_files(a,&names,"grids\\*.lfl",".lfl",filters,count))return 0;
    memset(&omit,0,sizeof(omit));
    if(exclusions && !game_menu_level_files(a,&omit,"grids\\*.lfl",".lfl",excluded,(unsigned)exclusions))return 0;
    width=game_memory_read_u32(&a->memory,selector+0x28)-game_memory_read_u32(&a->memory,selector+0x20);
    if(width<=15)return 0;
    for(i=0;i<names.count;++i) {
        GameAddress row;for(j=0;j<omit.count;++j)if(!strcmp(names.names[i],omit.names[j]))break;
        if(j<omit.count)continue;
        row=game_menu_level_row_create(a,names.names[i],width-15,18,3,1,1,0x523df0);
        if(!row)return 0;
        if(!game_menu_selector_append(a,selector,row,0,0)){(void)game_menu_widget_destroy(a,row);return 0;}
    }
    return !a->memory.fault;
}
