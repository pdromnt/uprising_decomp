#include "gameplay_input.h"
#include "application_archives.h"
#include <stdio.h>

extern int game_application_capture_png(GameApplication *,const char *);
/* Keep the established numbering and isolated screenshot directory. The
 * platform captures the rendered game front buffer, including 3D and HUD. */
int game_input_screenshot(GameApplication *a,uint32_t number){
    char name[80],path[4128];int32_t index;int length;
    index=number<=INT32_MAX?(int32_t)number:(int32_t)((int64_t)number-4294967296LL);
    length=snprintf(name,sizeof(name),"scrshts/scrsht%d.png",(int)index);
    return length>=0 && (size_t)length<sizeof(name) && !a->memory.fault &&
        game_application_file_path(a,name,1,path,sizeof(path)) &&
        game_application_output_directory(a,"scrshts") && game_application_capture_png(a,path);
}
