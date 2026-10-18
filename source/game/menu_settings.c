#include "menu_settings.h"
#include "application_config.h"
#include "scene_projection_storage.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const GameAddress option_globals[12]={
    0x583608u,0,0x583e20u,0x629080u,0x597f7cu,0x58fdecu,
    0x597f84u,0x59700cu,0x597008u,0x6297b8u,0x5ad8acu,0x596fb0u};
static const unsigned option_offsets[12]={8,0x34,0xc,0x10,0x14,0x18,0x1c,0x20,0x24,0x28,0x2c,0x30};
static const unsigned tail_offsets[]={0x30,0x34,0x38,0x3c,0x40,0x44,0x4c,0x48,0x50,0x54,0x58};
static const uint32_t tail_defaults[]={1,100,3,0,0x36b0,0,0x3e800000,0x3f000000,1,UINT32_MAX,UINT32_MAX};

static uint32_t read(GameApplication *a,GameAddress p) {return game_memory_read_u32(&a->memory,p);}
static void write(GameApplication *a,GameAddress p,uint32_t value) {game_memory_write_u32(&a->memory,p,value);}
static float float_read(GameApplication *a,GameAddress p) {
    uint32_t bits=read(a,p);float value;memcpy(&value,&bits,4u);return value;
}
static void settings_float_write(GameApplication *a,GameAddress p,float value) {
    uint32_t bits;memcpy(&bits,&value,4u);write(a,p,bits);
}
/* Original 00560db0 truncates to signed 64-bit then returns its low word.
 * Invalid/out-of-range conversion yields the low word of integer-indefinite. */
static uint32_t truncate_word(long double value) {
    if (!isfinite(value) || value>=9223372036854775808.0L || value< -9223372036854775808.0L) return 0;
    return (uint32_t)(uint64_t)(int64_t)truncl(value);
}
void game_menu_options_default(GameApplication *a) {
    static const GameAddress zeros[]={0x583608u,0x597008u,0x58fdecu};
    static const GameAddress ones[]={0x583e20u,0x59700cu,0x597f84u,0x6297b8u,0x5ad8acu,0x596fb0u};
    unsigned i;
    for(i=0;i<3u;++i)write(a,zeros[i],0);
    for(i=0;i<6u;++i)write(a,ones[i],1);
    write(a,0x59d440u,0x3f800000u);write(a,0x59d444u,0x3f800000u);
    write(a,0x597f7cu,16000u);write(a,0x629080u,0x42700000u);write(a,0x59d43cu,0x41f00000u);
}
/* 0051a110/0051a250: options and their shared gameplay/render consumers. */
int game_menu_option_set(GameApplication *a,uint32_t option,uint32_t value) {
    if(option>11u)return !a->memory.fault;
    if(option==1u) {
        long double scaled=(long double)(int32_t)value*float_read(a,0x575588u);
        uint32_t bits;
        float stored=(float)scaled;
        settings_float_write(a,0x59d440u,stored);
        settings_float_write(a,0x59d444u,(float)((long double)float_read(a,0x57554cu)/scaled));
        bits=read(a,0x59d440u);
        if((int32_t)bits<(int32_t)0x3dcccccdu)write(a,0x59d440u,0x3dcccccdu);
        if((int32_t)read(a,0x59d440u)>(int32_t)0x3f800000u)write(a,0x59d440u,0x3f800000u);
        return !a->memory.fault && game_menu_render_quality(a,read(a,0x597f88u));
    }
    if(option==3u) {
        long double numeric=(long double)(int32_t)value;
        settings_float_write(a,0x629080u,(float)numeric);
        settings_float_write(a,0x59d43cu,(float)(numeric*float_read(a,0x575590u)));
    } else write(a,option_globals[option],value);
    return !a->memory.fault;
}
uint32_t game_menu_option_get(GameApplication *a,uint32_t option) {
    if(option>11u)return 0;
    if(option==1u)return truncate_word((long double)float_read(a,0x59d440u)*float_read(a,0x575594u));
    if(option==3u)return truncate_word(float_read(a,0x629080u));
    return read(a,option_globals[option]);
}
static int word_read(ResourceFile *file,uint32_t *value) {
    uint8_t bytes[4];
    if(resource_file_read(file,bytes,4u)!=4u)return 0;
    *value=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8|(uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;
    return 1;
}
static int word_write(ResourceFile *file,uint32_t value) {
    uint8_t bytes[4]={(uint8_t)value,(uint8_t)(value>>8),(uint8_t)(value>>16),(uint8_t)(value>>24)};
    return resource_file_write(file,bytes,4u)==4u;
}
int game_menu_settings_string(GameApplication *a,GameAddress slot,const uint8_t *bytes,size_t length) {
    GameAddress old=read(a,slot),replacement;
    if(length>4096u)return 0;
    replacement=scene_projection_storage_allocate(length+1u,"portable settings string");
    if(!replacement)return 0;
    if((length && !game_memory_write(&a->memory,replacement,bytes,length)) ||
        !game_memory_fill_u8(&a->memory,replacement+(uint32_t)length,1u,0,0)) {
        scene_projection_storage_free(replacement);return 0;
    }
    if(old && !scene_projection_storage_free(old)) {scene_projection_storage_free(replacement);return 0;}
    write(a,slot,replacement);return !a->memory.fault;
}
static int string_load(GameApplication *a,ResourceFile *file,GameAddress slot) {
    uint32_t length;uint8_t bytes[4096];
    if(!word_read(file,&length) || length>sizeof(bytes) || resource_file_read(file,bytes,length)!=length)return 0;
    return game_menu_settings_string(a,slot,bytes,length);
}
static int string_save(GameApplication *a,ResourceFile *file,GameAddress slot) {
    GameAddress string=read(a,slot);uint8_t bytes[4097];unsigned length=0;
    if(string) for(;;) {
        if(length==sizeof(bytes))return 0;
        bytes[length]=game_memory_read_u8(&a->memory,string+length);
        if(!bytes[length])break;
        ++length;
    }
    return !a->memory.fault && word_write(file,length) && resource_file_write(file,bytes,length)==length;
}
/* 004c99a0/004c9670: the legacy file has ten words, three sized strings,
 * then eleven optional tail words. Pointers are never serialized. */
int game_menu_settings_load(GameApplication *a,GameAddress object,ResourceFile *file) {
    unsigned i;uint32_t value;
    if(!file) {
        static const uint8_t player[]="PLAYER";
        if(!game_menu_settings_string(a,object,player,sizeof(player)-1u))return 0;
        game_menu_options_default(a);
        for(i=0;i<12u;++i)write(a,object+option_offsets[i],game_menu_option_get(a,i));
        for(i=2;i<11u;++i)write(a,object+tail_offsets[i],tail_defaults[i]);
        return !a->memory.fault;
    }
    for(i=8;i<=0x2cu;i+=4u) {
        if(!word_read(file,&value))return 0;
        write(a,object+i,value);
    }
    if(!string_load(a,file,object) || !string_load(a,file,object+4u) || !string_load(a,file,object+0x5cu))return 0;
    for(i=0;i<11u;++i) {
        if(!word_read(file,&value))value=tail_defaults[i];
        write(a,object+tail_offsets[i],value);
    }
    /* The original clamps only the renderer's published quality to <=1,
     * retaining the saved record value for later UI consumers. */
    value=read(a,object+0x38u);
    write(a,0x597f88u,(int32_t)value>1?1u:value);
    for(i=0;i<12u;++i)if(i!=1u && !game_menu_option_set(a,i,read(a,object+option_offsets[i])))return 0;
    return game_menu_option_set(a,1u,read(a,object+0x34u));
}
int game_menu_settings_save(GameApplication *a,GameAddress object,ResourceFile *file) {
    unsigned i;
    if(!file)return 0;
    for(i=8;i<=0x2cu;i+=4u)if(!word_write(file,read(a,object+i)))return 0;
    if(!string_save(a,file,object) || !string_save(a,file,object+4u) || !string_save(a,file,object+0x5cu))return 0;
    for(i=0;i<11u;++i)if(!word_write(file,read(a,object+tail_offsets[i])))return 0;
    return !a->memory.fault && resource_file_flush(file);
}
static int configuration_load(GameApplication *a,GameAddress object){
    GameConfig *c=game_application_config(a);unsigned i;char key[32];
    static const char *names[]={"player_name","text_04","text_5c"};
    static const unsigned slots[]={0,4,0x5c};
    if(!game_menu_settings_load(a,object,NULL))return 0;
    for(i=0;i<3;i++){const char *value=game_config_get(c,"profile",names[i]);if(!value)value=i?"":"PLAYER";
        if(!game_menu_settings_string(a,object+slots[i],(const uint8_t *)value,strlen(value)))return 0;}
    for(i=0;i<12;i++){snprintf(key,sizeof(key),"option_%u",i);write(a,object+option_offsets[i],game_config_number(c,"game_options",key,read(a,object+option_offsets[i])));}
    for(i=0;i<11;i++)if(tail_offsets[i]!=0x34){snprintf(key,sizeof(key),"field_%02x",tail_offsets[i]);write(a,object+tail_offsets[i],game_config_number(c,"legacy_options",key,read(a,object+tail_offsets[i])));}
    write(a,0x597f88,(int32_t)read(a,object+0x38)>1?1:read(a,object+0x38));
    for(i=0;i<12;i++)if(i!=1 && !game_menu_option_set(a,i,read(a,object+option_offsets[i])))return 0;
    return game_menu_option_set(a,1,read(a,object+0x34));
}
static int configuration_save(GameApplication *a,GameAddress object){
    GameConfig *c=game_application_config(a);unsigned i,j;char key[32],value[4097];
    static const char *names[]={"player_name","text_04","text_5c"};
    static const unsigned slots[]={0,4,0x5c};
    for(i=0;i<3;i++){GameAddress address=read(a,object+slots[i]);
        for(j=0;j<sizeof(value);j++){value[j]=address?(char)game_memory_read_u8(&a->memory,address+j):0;if(!value[j])break;}
        if(j==sizeof(value) || !game_config_set(c,"profile",names[i],value))return 0;}
    for(i=0;i<12;i++){snprintf(key,sizeof(key),"option_%u",i);if(!game_config_set_number(c,"game_options",key,read(a,object+option_offsets[i])))return 0;}
    for(i=0;i<11;i++)if(tail_offsets[i]!=0x34){snprintf(key,sizeof(key),"field_%02x",tail_offsets[i]);if(!game_config_set_number(c,"legacy_options",key,read(a,object+tail_offsets[i])))return 0;}
    return !a->memory.fault;
}
int game_menu_settings_begin(GameApplication *a) {
    GameAddress object=read(a,0x5943c8u),player;ResourceFile *file;unsigned i;
    uint8_t name[60]; /* Next independently owned global is 005efefc. */
    if(!object) {
        object=scene_projection_storage_allocate(100u,"portable menu settings");
        if(!object)return 0;
        game_memory_fill_u8(&a->memory,object,100u,0,0);
        write(a,object+0x4cu,0x3e800000u);write(a,object+0x48u,0x3f000000u);
        write(a,object+0x50u,1);write(a,object+0x54u,UINT32_MAX);write(a,object+0x58u,UINT32_MAX);
        write(a,0x5943c8u,object);
    }
    if(game_config_get(game_application_config(a),"profile","player_name"))i=configuration_load(a,object);
    else {
        file=game_application_settings_file(a,0);
        i=game_menu_settings_load(a,object,file);
        if(file)resource_file_close(file);
    }
    if(!i)return 0;
    player=read(a,object);
    for(i=0;i<sizeof(name);++i) {
        name[i]=game_memory_read_u8(&a->memory,player+i);
        if(!name[i])return game_memory_write(&a->memory,0x5efec0u,name,i+1u) && configuration_save(a,object);
    }
    return 0;
}
int game_menu_settings_checkpoint(GameApplication *a) {
    GameAddress object=read(a,0x5943c8u);uint8_t name[60];unsigned i;
    if(!object)return 1;
    for(i=0;i<sizeof(name);++i) {
        name[i]=game_memory_read_u8(&a->memory,0x5efec0u+i);
        if(!name[i])break;
    }
    if(i==sizeof(name) || !game_menu_settings_string(a,object,name,i))return 0;
    return configuration_save(a,object);
}
int game_menu_settings_end(GameApplication *a) {
    static const unsigned strings[]={0,4,0x5c,0x60};
    GameAddress object=read(a,0x5943c8u);unsigned i;int ok;
    if(!object)return 0;
    ok=game_menu_settings_checkpoint(a);
    for(i=0;i<4u;++i) {
        GameAddress string=read(a,object+strings[i]);
        if(string && !scene_projection_storage_free(string))ok=0;
        write(a,object+strings[i],0);
    }
    if(!scene_projection_storage_free(object))ok=0;
    write(a,0x5943c8u,0);return ok && !a->memory.fault;
}
