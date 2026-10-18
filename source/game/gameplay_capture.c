#include "gameplay_input.h"
#include "menu_widgets.h"
#include "resource_file.h"
#include "application_archives.h"
#include <stdio.h>

extern int game_menu_palette_read_bytes(GameApplication *,uint8_t[1024]);
/*4d9290: the original fixed640x480, bottom-up24-bit TGA and RGB565 channel
 *expansion. The current indexed backend supplies its actual palette/pixels;
 *complete screenshot visual parity remains unverified. */
int game_input_screenshot(GameApplication *a,uint32_t number){
    static const uint8_t header[18]={0,0,2,0,0,0,0,0,0,0,0,0,0x80,2,0xe0,1,24,0};
    ResourceFile *file=NULL;GameMenuPixelSpan frame={0};uint8_t palette[1024],row[640*3];
    char name[80],path[4128];unsigned x,y;int locked=0,ok=0;int32_t index;
    index=number<=INT32_MAX?(int32_t)number:(int32_t)((int64_t)number-4294967296LL);
    if(snprintf(name,sizeof(name),"scrshts/scrsht%d.tga",(int)index)<0 || a->memory.fault ||
       !game_application_file_path(a,name,1,path,sizeof(path)) ||
       !game_menu_palette_read_bytes(a,palette) || !game_menu_frame_pixels_begin(a,&frame))return 0;
    locked=1;
    if(frame.width<640 || frame.height<480 || frame.stride<frame.width ||
       (uint64_t)(479)*(uint32_t)frame.stride+640>frame.bytes || !game_application_output_directory(a,"scrshts"))goto done;
    file=resource_file_open_mode(path,"wb");if(!file || resource_file_write(file,header,sizeof(header))!=sizeof(header))goto done;
    for(y=0;y<480;++y){const uint8_t *source=frame.pixels+(size_t)(479-y)*(uint32_t)frame.stride;
        for(x=0;x<640;++x){const uint8_t *color=palette+source[x]*4;
            row[x*3]=color[2]&0xf8;row[x*3+1]=color[1]&0xfc;row[x*3+2]=color[0]&0xf8;}
        if(resource_file_write(file,row,sizeof(row))!=sizeof(row))goto done;
    }
    ok=resource_file_flush(file);
done:if(file)resource_file_close(file);if(locked && !game_menu_frame_pixels_end(a))ok=0;return ok;
}
