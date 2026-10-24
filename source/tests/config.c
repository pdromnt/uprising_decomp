#include "game_config.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"configuration check failed at %d\n",__LINE__);return 1;}}while(0)
int main(void){
    GameConfig *a=game_config_create(),*b=game_config_create();FILE *file=fopen("config-roundtrip.tmp","w+b");unsigned i;char key[32];
    CHECK(a && b && file);
    CHECK(game_config_set(a,"profile","name"," Player\\name\n"));
    CHECK(game_config_set_number(a,"music","mode",2));
    CHECK(game_config_set_number(a,"legacy_options","field_54",UINT32_MAX));
    CHECK(game_config_set_number(a,"display","fsr1",1) && game_config_set_number(a,"display","msaa_samples",4));
    CHECK(game_config_remove(a,"display","fsr1") && game_config_remove(a,"display","msaa_samples"));
    for(i=0;i<287;i++){snprintf(key,sizeof(key),"key_%03u",i);CHECK(game_config_set_number(a,"bindings",key,i));}
    CHECK(game_config_write(a,file));rewind(file);CHECK(game_config_read(b,file));
    CHECK(!strcmp(game_config_get(b,"profile","name")," Player\\name\n"));
    CHECK(!game_config_get(b,"display","fsr1") && !game_config_get(b,"display","msaa_samples"));
    CHECK(game_config_remove(b,"display","absent") && !game_config_dirty(b));
    CHECK(game_config_number(b,"music","mode",0)==2);
    CHECK(game_config_number(b,"legacy_options","field_54",0)==UINT32_MAX);
    CHECK(!game_config_dirty(b));CHECK(game_config_set_number(b,"music","mode",2));CHECK(!game_config_dirty(b));
    CHECK(game_config_set_number(b,"music","mode",1));CHECK(game_config_dirty(b));
    CHECK(game_config_set(b,"music","bad","-1"));CHECK(game_config_number(b,"music","bad",7)==7);
    CHECK(!game_config_set(b,"bad section","key","value"));
    for(i=0;i<287;i++){snprintf(key,sizeof(key),"key_%03u",i);CHECK(game_config_number(b,"bindings",key,999)==i);}
    fclose(file);remove("config-roundtrip.tmp");game_config_free(a);game_config_free(b);
    puts("configuration roundtrip PASS: bindings, numeric bounds, escaped strings and unchanged-write detection");return 0;
}
