#include "level_settings.h"
#include "application_archives.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
/*4a6ea0 ->4a6d90: publish the selected configuration into the existing four
 *player managers, then apply the shared/faction rows. -69 means inherit. */
int game_level_player_configuration_publish(GameApplication *a){
    static const unsigned fields[]={0x22c,0x588,0x4b4,0x4b8,0x4bc,0x4c0};
    static const GameAddress defaults[]={0x5acc8c,0x5ac9c0,0x5ac900,0x5acbf0,0x5acc84,0x5ac904};
    unsigned player,i;uint32_t value,total;GameAddress row,manager,weights,base;
    if(!a || a->memory.fault)return 0;
    for(player=0;player<4;player++){
        base=player*240;manager=get(a,0x5f0210+player*4);
        row=base+((get(a,0x5eff04) && !get(a,0x5f0150))?0x602bb8:0x6027f0);
        if(!manager)return 0;
        for(i=0;i<32;i++){value=get(a,row+i*4);if(value!=0xffffffbbu)put(a,manager+0x29c+i*4,value);}
        value=get(a,0x5ac9d8);put(a,manager+0x2b8,(int32_t)value<0?0:value>=4?4:value);
        /*47f2c0 is the scene player's real3c setter. */
        if(get(a,get(a,manager)+0x3c)!=0x47f2c0)return 0;
        for(i=0;i<6;i++){
            value=get(a,row+0x80+i*4);if(value==0xffffffbbu)value=get(a,defaults[i]);
            put(a,manager+fields[i],i==1?value-1:value);
        }
        weights=base+0x602888;if(get(a,weights)==0xffffffbbu)weights=0x5ac978;
        total=0;for(i=0;i<9;i++){
            value=get(a,weights+i*8);put(a,manager+0x5ac+i*8,value);total+=value;
            put(a,manager+0x5b0+i*8,get(a,weights+i*8+4));
        }
        put(a,manager+0x5a8,total);
        for(i=0;i<2;i++){
            GameAddress source=base+0x6028d0+i*8,target=manager+0x5f4+i*8;
            if(get(a,source)==0xffffffbbu){
                put(a,target,get(a,i?0x5ac6a8:0x5ac974));put(a,target+4,get(a,i?0x5accd8:0x5ac914));
            }else{put(a,target,get(a,source));put(a,target+4,get(a,source+4));}
        }
    }
    for(i=0;i<10;i++){value=get(a,0x602f78+i*4);if(value!=0xffffffbbu)put(a,0x5ac678+i*4,value);}
    for(player=0;player<2;player++){
        manager=get(a,player?0x5f0220:0x5f0224);if(!manager)return 0;
        for(i=0;i<(player?17u:13u);i++){
            value=get(a,(player?0x602fd4:0x602fa0)+i*4);
            if(value!=0xffffffbbu)put(a,manager+(player?0x29c:0x1fc)+i*4,value);
        }
    }
    return !a->memory.fault;
}
/*476590: the slot number and both current/default color words belong to the
 * existing player manager. No per-level player copy is constructed. */
static int assign_player_slot(GameApplication *a,GameAddress player,unsigned slot){
    uint32_t color;
    if(!player || slot>=6 || get(a,get(a,player)+0x10)!=0x476590 || a->memory.fault)return 0;
    color=get(a,0x589748+slot*4);
    game_memory_write_u16(&a->memory,player+10,(uint16_t)slot);
    put(a,player+12,color);put(a,player+16,color);return !a->memory.fault;
}
/*418da0: assign the level's six provider/player bindings before any slot,
 * object, script or color consumer runs. This must execute on every load. */
int game_level_player_slot_bindings(GameApplication *a){
    uint32_t capacity,index,slot,visits;unsigned player;
    if(!game_network_level_capacity(a,0x5acc90,&capacity) || !capacity || capacity>4)return 0;
    put(a,0x5ac8b8,capacity);
    if((int32_t)get(a,0x5ac6b4)>(int32_t)capacity)return 0;
    if(!game_memory_fill_u32(&a->memory,0x5f01f0,6,UINT32_MAX,0))return 0;
    for(player=4;player<6;player++)if(get(a,player==4?0x5ac678:0x5ac67c)){
        put(a,0x5f01f0+player*4,player);
        if(!assign_player_slot(a,get(a,0x5f0210+player*4),player))return 0;
    }
    if(!get(a,0x5eff04)){
        put(a,0x5f01f0,0);return assign_player_slot(a,get(a,0x5f0208),0);
    }
    for(player=0;player<4;player++){
        GameAddress manager=get(a,0x5f0210+player*4);
        if(!get(a,manager+20))continue;
        slot=player;
        if(get(a,0x5ac684)){
            uint32_t bits;float sample;long double value;
            index=get(a,0x6308c0);if(index==1024)index=0;
            if(index>1023 || a->memory.fault)return 0;
            bits=get(a,0x6308c8+index*4);memcpy(&sample,&bits,4);
            put(a,0x6308c0,index+1);value=(long double)(int32_t)capacity*sample;
            if(!isfinite(value) || value<0 || value>=capacity)return 0;
            slot=(uint32_t)value;visits=0;
            while(get(a,0x5f01f0+slot*4)!=UINT32_MAX){
                if(++visits>=capacity)return 0;
                slot=(slot+1)%capacity;
            }
        }
        put(a,0x5f01f0+slot*4,player);
        if(!assign_player_slot(a,manager,slot))return 0;
    }
    return !a->memory.fault;
}
/*0045a630/00538420/00538820 restore the selected filename from an already-open
 * archive: tag, length, tag, then the original string bytes. The application
 * owns a full-width ResourceFile; the record holds only its stable identity. */
static int archive_word(GameApplication *a,ResourceFile *file,uint32_t *word) {
    uint8_t bytes[4];if(resource_file_read(file,bytes,4)!=4)return 0;
    *word=(uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24);
    (void)a;return 1;
}
static int restore_filename(GameApplication *a,GameAddress id) {
    ResourceFile *file=game_application_archive_file(a,id);uint32_t tag,length;char name[60];
    if(!file || !archive_word(a,file,&tag))return 0;
    put(a,0x59d4b8,1);
    if(!archive_word(a,file,&length) || length==0 || length>sizeof(name) || !archive_word(a,file,&tag))return 0;
    put(a,0x59d4b8,1);
    if(resource_file_read(file,name,length)!=length || !memchr(name,0,length))return 0;
    return game_memory_write(&a->memory,0x5efe80,name,length);
}
static int text_read(GameApplication *a,GameAddress p,char *text,size_t capacity) {
    size_t i;for(i=0;i<capacity;++i){text[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!text[i])return !a->memory.fault;}return 0;
}
static int capacity_line(ResourceFile *file,char line[512]) {
    unsigned visits=0;
    while(++visits<65536u && resource_file_line(file,line,512)) {
        const unsigned char *p=(const unsigned char *)line;
        while(*p && isspace(*p))++p;
        if(*p && *p!='#')return 1;
    }
    return 0;
}
/*004aa1c0: inspect the actual SLK header and its slots section without loading
 * a competing world or changing the selected level. One slot is reserved for
 * the imperial faction. Malformed input is a failure, never fake capacity. */
int game_network_level_capacity(GameApplication *a,GameAddress name,uint32_t *capacity) {
    ResourceFile *file;char filename[60],path[128],line[512];float slick;
    int terrain,slots,section_count,result=0;
    if(!capacity || !text_read(a,name,filename,sizeof(filename)))return 0;
    if(snprintf(path,sizeof(path),"grids\\%s",filename)>=(int)sizeof(path))return 0;
    file=resource_file_open(path);if(!file)return 0;
    if(!capacity_line(file,line) || sscanf(line,"SLICK %f",&slick)!=1 ||
       !capacity_line(file,line) || sscanf(line,"terrain %d",&terrain)!=1 ||
       !capacity_line(file,line) || sscanf(line,"slots %d",&slots)!=1 ||
       !resource_file_seek(file,slots,0) || !capacity_line(file,line) || sscanf(line,"slots %d",&section_count)!=1)goto done;
    if(section_count<2)*capacity=1;
    else if(section_count<=5)*capacity=(uint32_t)section_count-1u;
    else goto done;
    result=!a->memory.fault;
done:resource_file_close(file);return result;
}
static int manager_reset(GameApplication *a,GameAddress p) {
    static const unsigned absent[]={0x2fc,0x2dc,0x304,0x308,0x300,0x30c,0x2f4,0x2f8,0x2f0,0x310,0x314,0x318,0x2ec};
    uint32_t type=get(a,p);unsigned i;
    if(type==0x573e78) {
        game_memory_fill_u32(&a->memory,p+0x4b4,4,0,0);
        game_memory_fill_u32(&a->memory,p+0x29c,32,0,0);
        for(i=0;i<sizeof(absent)/sizeof(absent[0]);++i)put(a,p+absent[i],UINT32_MAX);
    } else if(type==0x573da8)game_memory_fill_u32(&a->memory,p+0x29c,17,0,0);
    else if(type==0x573c98)game_memory_fill_u32(&a->memory,p+0x1fc,13,0,0);
    else return 0;
    return !a->memory.fault;
}
/*00419070: the optional map-text file owns no retained allocation. Preserve
 * description fallback, ranking budgets and the original substring markers. */
int game_level_description_load(GameApplication *a) {
    static const uint32_t budgets[]={0,20,120,320,570,680,992};
    char name[128],path[256],line[128],text[512]="No description available ....";ResourceFile *file;
    uint32_t rank=1,budget;int found=0;unsigned count=0;
    if(!text_read(a,0x639a78,name,sizeof(name)))return 0;
    if(snprintf(path,sizeof(path),"grids\\%s",name)>=(int)sizeof(path))return 0;
    put(a,0x5accd0,1);put(a,0x5ac910,0);put(a,0x5acbec,0);file=resource_file_open(path);
    if(file) {
        while(resource_file_line(file,line,sizeof(line)))if(strstr(line,"CAMPAIGN_RANKING")){found=1;break;}
        if(!found){resource_file_close(file);return game_memory_write(&a->memory,0x5ac9e0,text,strlen(text)+1);}
        /*The original fscanf consumes the ranking and its trailing whitespace. */
        while(resource_file_line(file,line,sizeof(line))) {
            int value;const char *start=line;while(*start==' ' || *start=='\t' || *start=='\r' || *start=='\n')++start;
            if(!*start)continue;
            if(sscanf(start,"%d",&value)==1)rank=(uint32_t)value;
            break;
        }
        found=0;while(resource_file_line(file,line,sizeof(line)))if(strstr(line,"OBJECTIVE_BEGIN")){found=1;break;}
        if(!found){resource_file_close(file);put(a,0x5accd0,rank);return game_memory_write(&a->memory,0x5ac9e0,text,strlen(text)+1);}
        text[0]=0;
        while(count<511) {
            uint8_t byte;if(resource_file_read(file,&byte,1)!=1 || byte==255 || strstr(text,"OBJECTIVE_END"))break;
            if(byte!='\n'){text[count++]=(char)byte;text[count]=0;}
        }
        {char *end=strstr(text,"OBJECTIVE_END");if(end)*end=0;}
        resource_file_close(file);
    }
    budget=rank<7?budgets[rank]:320;put(a,0x5accd0,rank);put(a,0x5ac910,budget);put(a,0x5acbec,budget);
    return game_memory_write(&a->memory,0x5ac9e0,text,strlen(text)+1);
}
/*00417af0: reuse six already-owned startup managers, parse actual selected
 * level data, then reset the original per-manager roster/command globals. */
int game_level_selection_load(GameApplication *a) {
    char name[60],path[128];unsigned i;GameAddress previous=get(a,0x5acc88);
    for(i=0;i<6;++i){GameAddress manager=get(a,0x5f0210+i*4);if(!manager || !manager_reset(a,manager))return 0;}
    if(previous && !restore_filename(a,previous))return 0;
    if(!game_level_settings_defaults(a) || !text_read(a,0x5efe80,name,sizeof(name)))return 0;
    if(snprintf(path,sizeof(path),"grids\\%s",name)>=(int)sizeof(path) || !game_level_settings_read(a,path) || !game_level_description_load(a))return 0;
    for(i=0;i<6;++i) {
        GameAddress manager=get(a,0x5f0210+i*4);
        put(a,manager+0x148,0);put(a,manager+0x14c,1);game_memory_fill_u32(&a->memory,manager+0x154,18,0,0);
        game_memory_fill_u32(&a->memory,0x5f0160+i*24,6,0,0);put(a,0x5f0160+i*28,1);
    }
    put(a,0x5f01d4,1);put(a,0x5f01e8,1);return !a->memory.fault;
}
/*004a77e0/004a78d0/004a79f0: copy level manager presets, or construct the
 * original campaign faction defaults in place. These are ordinary table data. */
int game_level_selection_default_row(GameApplication *a,uint32_t index) {
    static const GameAddress absent[]={0x602850,0x602830,0x602858,0x60285c,0x602854,0x602860,0x602848,0x60284c,0x602844,0x602864,0x602868,0x60286c,0x602840};
    GameAddress offset;unsigned j;if(index>=4)return 0;offset=index*240;
    game_memory_fill_u32(&a->memory,0x6027f0+offset,32,0,0);
    for(j=0;j<sizeof(absent)/sizeof(absent[0]);++j)put(a,absent[j]+offset,UINT32_MAX);
    put(a,0x602870+offset,5000);put(a,0x602874+offset,5);
    game_memory_fill_u32(&a->memory,0x602878+offset,4,0,0);game_memory_fill_u32(&a->memory,0x602888+offset,22,0xffffffbb,0);
    return !a->memory.fault;
}
int game_level_selection_presets(GameApplication *a) {
    static const GameAddress globals[]={0x5acc8c,0x5ac9c0,0x5ac900,0x5acbf0,0x5acc84,0x5ac904};
    unsigned i,j;int campaign=get(a,0x5eff04)!=0;
    for(i=0;i<4;++i) {
        GameAddress offset=i*240,manager=get(a,0x5f0210+i*4);
        if(campaign) {
            if(!game_level_selection_default_row(a,i))return 0;
        } else {
            if(!manager)return 0;
            for(j=0;j<32;++j)put(a,0x6027f0+offset+j*4,get(a,manager+0x29c+j*4));
            for(j=0;j<6;++j)put(a,0x602870+offset+j*4,get(a,globals[j]));
        }
    }
    for(j=0;j<10;++j)put(a,0x602f78+j*4,get(a,0x5ac678+j*4));
    if(!campaign) {
        GameAddress imperial=get(a,0x5f0220),rabble=get(a,0x5f0224);if(!imperial || !rabble)return 0;
        for(j=0;j<17;++j)put(a,0x602fd4+j*4,get(a,imperial+0x29c+j*4));
        for(j=0;j<13;++j)put(a,0x602fa0+j*4,get(a,rabble+0x1fc+j*4));
    }
    return !a->memory.fault;
}
