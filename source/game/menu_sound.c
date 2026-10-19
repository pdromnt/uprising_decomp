#include "menu_widgets.h"
#include "menu_choice_controls.h"
#include "menu_sound.h"
#include "menu_music.h"
#include "menu_network.h"
#include "sound_samples.h"
#include "sound_platform.h"
#include "scene_projection_storage.h"
#include "menu_runtime_tables.h"
#include <string.h>
#include <math.h>

extern int game_application_audio_muted(GameApplication *);

extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_strategic_modal_action(GameApplication *,uint32_t);

static int buffer_owned(GameApplication *a,GameAddress buffer) {
    if(sound_platform_owns(buffer))return 1;
    a->memory.fault=1;a->memory.fault_address=buffer;a->memory.fault_size=4u;
    return 0; /* Foreign device IDs cannot re-enter a CPU/import dispatcher. */
}
static int32_t status(void *context,GameAddress buffer,uint32_t *value) {
    return buffer_owned(context,buffer)?sound_platform_status(buffer,value):-1;
}
static int32_t position(void *context,GameAddress buffer,uint32_t *value) {
    return buffer_owned(context,buffer)?sound_platform_position(buffer,value):-1;
}
static int32_t command(void *context,GameAddress buffer,SoundBufferCommand cmd,uint32_t value) {
    return buffer_owned(context,buffer)?sound_platform_command(buffer,cmd,value):-1;
}
static int32_t format(void *context,GameAddress buffer,uint32_t *frequency,uint16_t *bits) {
    return buffer_owned(context,buffer)?sound_platform_format(buffer,frequency,bits):-1;
}
static GameAddress allocate(void *context,uint32_t size) {
    (void)context;return scene_projection_storage_allocate(size,"portable sound record");
}
static int release_record(void *context,GameAddress record) {
    (void)context;return scene_projection_storage_free(record);
}
static GameAddress create(void *context,GameAddress device,const SoundWave *wave) {
    return buffer_owned(context,device)?sound_platform_create_buffer(device,wave):0;
}
static GameAddress duplicate(void *context,GameAddress device,GameAddress buffer) {
    return buffer_owned(context,device) && buffer_owned(context,buffer)?sound_platform_duplicate(device,buffer):0;
}
static int release_buffer(void *context,GameAddress buffer) {
    return buffer_owned(context,buffer) && sound_platform_release_buffer(buffer);
}
static int release_device(void *context,GameAddress device) {
    return buffer_owned(context,device) && sound_platform_release_device(device);
}
static int32_t frequency(void *context,GameAddress buffer,uint32_t *value) {
    return buffer_owned(context,buffer)?sound_platform_frequency(buffer,value):-1;
}
static SoundSampleServices sample_services(GameApplication *a) {
    SoundSampleServices s={a,allocate,release_record,create,duplicate,release_buffer,release_device,frequency};
    return s;
}
static int append_name(GameMemory *m,GameAddress source,char path[128],unsigned *used) {
    unsigned i;
    for(i=0;i<128u;++i) {
        uint8_t ch=game_memory_read_u8(m,source+i);
        if(m->fault)return 0;
        if(!ch){path[*used]=0;return 1;}
        if(*used>=127u)return 0;
        path[(*used)++]=(char)ch;
    }
    return 0;
}
/* 0052e880: the resource is borrowed only during native buffer construction.
 * The sample owns its voice IDs; DirectSound owns the copied waveform bytes. */
int game_menu_sound_sample_begin(GameApplication *a,GameAddress sample,GameAddress bank,
        GameAddress name,uint32_t voices,float radius) {
    char path[128];unsigned used=0;ResourceBlob blob={0};int ok;
    SoundSampleServices s=sample_services(a);
    if(!append_name(&a->memory,0x59c690u,path,&used) ||
       !append_name(&a->memory,name,path,&used) ||
       !append_name(&a->memory,0x59c698u,path,&used) ||
       !game_application_load_blob(a,path,&blob))return 0;
    ok=sound_sample_initialize(&a->memory,sample,bank,voices,radius,&blob,&s);
    resource_blob_release(&blob);return ok && !a->memory.fault;
}
/* 0052dd60: load the real descriptor category, retaining the retail policy
 * that an unavailable individual sample leaves an empty slot for later retry. */
int game_menu_sound_bank_load(GameApplication *a,uint32_t category) {
    GameMemory *m=&a->memory;GameAddress bank=game_memory_read_u32(m,0x628a90u);unsigned i;
    if(!bank)return 0;
    if(!game_memory_read_u32(m,bank+0x468u))return !m->fault;
    for(i=0;i<276u && !m->fault;++i) {
        GameAddress entry=0x59ac58u+i*16u,slot=bank+12u+i*4u,sample;
        uint32_t bits;float radius;
        if(game_memory_read_u32(m,slot) || game_memory_read_u8(m,entry+12u)!=category)continue;
        sample=allocate(a,28u);
        if(sample) {
            bits=game_memory_read_u32(m,entry+8u);memcpy(&radius,&bits,4u);
            if(!game_menu_sound_sample_begin(a,sample,bank,game_memory_read_u32(m,entry),
                    game_memory_read_u8(m,entry+4u),radius)) {
                release_record(a,sample);sample=0;
            }
        }
        game_memory_write_u32(m,slot,sample);
    }
    return !m->fault;
}
/* 0052dbf0/0052ded0: one authoritative bank, optional native device and owned
 * auxiliary asset. A disabled device is the original supported silent route;
 * missing mandatory asset data remains a startup failure. */
int game_menu_sound_bank_begin(GameApplication *a) {
    GameMemory *m=&a->memory;ResourceBlob blob={0};char path[128];unsigned used=0;
    GameAddress bank,device,auxiliary=0;
    if(game_memory_read_u32(m,0x628a90u))return 0;
    bank=allocate(a,0x480u);if(!bank)return 0;
    device=sound_platform_device(game_application_audio_muted(a));
    sound_bank_initialize(m,bank,device);
    game_memory_write_u32(m,bank+8u,0x3f800000u);
    if(!m->fault && append_name(m,0x59c678u,path,&used) &&
        game_application_load_blob(a,path,&blob) && blob.size<=UINT32_MAX) {
        auxiliary=allocate(a,(uint32_t)blob.size);
        if(auxiliary && !scene_projection_storage_write(auxiliary,blob.data,blob.size)) {
            release_record(a,auxiliary);auxiliary=0;
        }
    }
    if(!auxiliary || m->fault) {
        resource_blob_release(&blob);
        if(auxiliary)release_record(a,auxiliary);
        if(device)sound_platform_release_device(device);
        release_record(a,bank);return 0;
    }
    game_memory_write_u32(m,0x628a90u,bank);
    game_memory_write_u32(m,0x59ac54u,auxiliary);
    game_memory_write_u32(m,0x628a8cu,(uint32_t)blob.size);
    resource_blob_release(&blob);return !m->fault;
}
/* 0052dc90: retire buffers before their device, then owned auxiliary bytes. */
int game_menu_sound_bank_end(GameApplication *a) {
    GameMemory *m=&a->memory;SoundSampleServices s=sample_services(a);
    GameAddress bank=game_memory_read_u32(m,0x628a90u),auxiliary;
    if(bank && (!sound_bank_unload(m,bank,-1,&s) || !release_record(a,bank)))return 0;
    game_memory_write_u32(m,0x628a90u,0);
    auxiliary=game_memory_read_u32(m,0x59ac54u);
    if(auxiliary && !release_record(a,auxiliary))return 0;
    game_memory_write_u32(m,0x59ac54u,0);game_memory_write_u32(m,0x628a8cu,0);
    return !m->fault;
}
int game_menu_sound_play(GameApplication *a,const SoundPlaybackRequest *request) {
    SoundPlaybackServices services={a,status,position,command,format};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90u);
    return bank && request && sound_playback_play(&a->memory,bank,request,&services);
}
int game_menu_sound_stop(GameApplication *a,int16_t sound,uint32_t voice){
    SoundPlaybackServices services={a,status,position,command,format};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90u);
    return bank && sound_playback_stop(&a->memory,bank,sound,voice,&services);
}
int game_menu_sound_stop_all(GameApplication *a){
    SoundPlaybackServices services={a,status,position,command,format};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90);
    return bank && sound_playback_stop_all(&a->memory,bank,&services);
}
int game_menu_sound_update(GameApplication *a){
    SoundPlaybackServices services={a,status,position,command,format};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90u);
    return bank && sound_playback_queue_update(&a->memory,bank,&services);
}
int game_menu_sound_frequency(GameApplication *a,GameAddress bank,int16_t sound,uint32_t voice,float pitch){
    SoundPlaybackServices services={a,status,position,command,format};
    return bank && sound_playback_frequency(&a->memory,bank,sound,voice,pitch,&services);
}
int game_menu_sound_playing(GameApplication *a,int16_t sound,uint32_t voice,int *playing){
    SoundPlaybackServices services={a,status,position,command,format};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90);int value;
    if(!bank || !playing || a->memory.fault)return 0;
    value=sound_playback_playing(&a->memory,bank,sound,voice,&services);
    if(value<0 || a->memory.fault)return 0;
    *playing=value;return 1;
}
int game_menu_sound_volume(GameApplication *a,int16_t sound,uint32_t voice,float value){
    SoundPlaybackServices services={a,status,position,command,format};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90);
    return bank && sound_playback_volume(&a->memory,bank,sound,voice,value,&services);
}
int game_menu_sound(GameApplication *a,uint32_t sound) {
    SoundPlaybackRequest request={(int16_t)sound,0.0f,1.0f,0.0f,0,0};
    return game_menu_sound_play(a,&request);
}
/*52f170 then52ef30: the existing sound bank owns the queued utterance. */
int game_menu_sound_queue_clear(GameApplication *a,GameAddress bank){
    SoundPlaybackServices services={a,status,position,command,format};
    return bank && sound_playback_queue_clear(&a->memory,bank,&services);
}
int game_menu_sound_queue_submit(GameApplication *a,GameAddress bank,const int16_t sounds[8],int *started){
    SoundPlaybackServices services={a,status,position,command,format};
    *started=0;if(!bank)return 0;
    *started=sound_playback_queue_submit(&a->memory,bank,sounds,&services);return !a->memory.fault;
}
int game_menu_sound_sequence(GameApplication *a,const int16_t sounds[8],int *started){
    SoundPlaybackServices services={a,status,position,command,format};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90);
    *started=0;
    if(!bank || !sound_playback_queue_clear(&a->memory,bank,&services))return 0;
    *started=sound_playback_queue_submit(&a->memory,bank,sounds,&services);
    return !a->memory.fault;
}
/*00417a7c: retain the startup loop's actual gain and voice policy. */
int game_menu_game_start_sound(GameApplication *a) {
    SoundPlaybackServices services={a,status,position,command,format};
    SoundPlaybackRequest request={9,0.0f,0.88f,0.0f,1,0};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90u);
    return bank && sound_playback_play(&a->memory,bank,&request,&services);
}
int game_menu_sound_mode(GameApplication *a,uint32_t category) {
    SoundSampleServices services={a,allocate,release_record,create,duplicate,release_buffer,release_device,frequency};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90u);
    if(!bank)return 0;
    if(category>255u || !game_memory_read_u32(&a->memory,bank+0x468u))return !a->memory.fault;
    return sound_bank_unload(&a->memory,bank,(int)category,&services);
}
int game_menu_sound_loop_begin(GameApplication *a,uint32_t sound,GameAddress publication) {
    SoundPlaybackServices services={a,status,position,command,format};
    SoundPlaybackRequest request={(int16_t)sound,0.0f,1.0f,0.0f,1,0};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90);
    if(!bank || !sound_playback_play(&a->memory,bank,&request,&services))return 0;
    game_memory_write_u32(&a->memory,publication,game_memory_read_u32(&a->memory,bank));
    return !a->memory.fault;
}
int game_menu_sound_loop_end(GameApplication *a,uint32_t sound,uint32_t voice) {
    SoundPlaybackServices services={a,status,position,command,format};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90);
    return bank && sound_playback_stop(&a->memory,bank,(int16_t)sound,voice,&services);
}
int game_menu_sound_drag_begin(GameApplication *a,uint32_t sound,float pitch) {
    SoundPlaybackServices services={a,status,position,command,format};
    SoundPlaybackRequest request={(int16_t)sound,0.0f,0.9f,pitch,1,1};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90);
    if(!bank || !sound_playback_play(&a->memory,bank,&request,&services))return 0;
    game_memory_write_u32(&a->memory,0x627aa8,game_memory_read_u32(&a->memory,bank));return !a->memory.fault;
}
int game_menu_sound_drag_pitch(GameApplication *a,uint32_t sound,float pitch) {
    SoundPlaybackServices services={a,status,position,command,format};
    GameAddress bank=game_memory_read_u32(&a->memory,0x628a90);
    return bank && sound_playback_frequency(&a->memory,bank,(int16_t)sound,game_memory_read_u32(&a->memory,0x627aa8),pitch,&services);
}
/* 0052cb00: real main-menu start callback and recipient command. */
int game_menu_button_callback(GameApplication *a,uint32_t method,GameAddress receiver,uint32_t message,uint32_t argument) {
    if(method==0x523fe0){
        extern int game_menu_level_launch_selected(GameApplication *);
        return game_menu_level_launch_selected(a);
    }
    if(method==0x4c80f0){
        extern int game_menu_ingame_information_back(GameApplication *);
        return game_menu_ingame_information_back(a);
    }
    if(method==0x4c8030 || method==0x4c80a0 || method==0x4c8060 || method==0x4c80d0){
        extern int game_menu_ingame_players_action(GameApplication *,uint32_t,GameAddress,uint32_t);
        return game_menu_ingame_players_action(a,method,receiver,argument);
    }
    if(method==0x4c4dd0 || method==0x4c5490){
        extern int game_menu_ingame_audio_back(GameApplication *,uint32_t);
        return game_menu_ingame_audio_back(a,method);
    }
    if(method==0x4c7710 || method==0x4c7740 || method==0x4c7b90){
        extern int game_menu_ingame_graphics_action(GameApplication *,uint32_t,GameAddress);
        return game_menu_ingame_graphics_action(a,method,receiver);
    }
    if(method==0x4c3890 || method==0x4c38c0 || method==0x4c38f0 || method==0x4c3920){
        extern int game_menu_ingame_options_action(GameApplication *,uint32_t);
        return game_menu_ingame_options_action(a,method);
    }
    if(method==0x4c6f70 || method==0x4c6fb0 || method==0x4c6ff0 || method==0x4c7030 || method==0x4c7070 ||
       method==0x4c70b0 || method==0x4c70f0 || method==0x4c71a0){
        extern int game_menu_pause_action(GameApplication *,uint32_t);
        return game_menu_pause_action(a,method);
    }
    if(method==0x4c7270 || method==0x4c7310 || method==0x4c7500 || method==0x4c7550 || method==0x4c7570 ||
       method==0x4c7590 || method==0x4c76a0 || method==0x4c76f0){
        extern int game_menu_strategic_file_action(GameApplication *,uint32_t,GameAddress);
        return game_menu_strategic_file_action(a,method,receiver);
    }
    if(method==0x52fe60 || method==0x52ff50 || method==0x531460 || method==0x531650){
        extern int game_campaign_purchase_action(GameApplication *,uint32_t);
        return game_campaign_purchase_action(a,method);
    }
    if(method==0x52fd00){
        extern int game_menu_campaign_launch(GameApplication *);
        return game_menu_campaign_launch(a);
    }
    if(method==0x532190 || method==0x5321c0 || method==0x5321f0 || method==0x532200)
        return game_menu_strategic_modal_action(a,method);
    if(method==0x512060 || method==0x512070 || method==0x52fce0) {
        extern int game_menu_research_popup_action(GameApplication *,uint32_t,uint32_t);
        return game_menu_research_popup_action(a,method,argument);
    }
    GameMemory *m=&a->memory;
    if(method==0x524760 || method==0x5248e0 || method==0x5248f0 || method==0x524ab0)
        return game_menu_research_hover(a,method,receiver,message,argument);
    if(method==0x54d3a0) {
        game_memory_write_u32(m,0x5977c0,receiver);
        return !m->fault && game_menu_research_popup_refresh(a);
    }
    if(method==0x5120d0) {
        GameAddress preview=game_memory_read_u32(m,0x5977c4);
        uint32_t enabled=game_memory_read_u32(m,receiver+0x94)!=0;
        if(!preview)return 0;
        game_memory_write_u32(m,0x5977d8,enabled);game_memory_write_u32(m,preview+0x170,enabled);
        return !m->fault && game_menu_research_popup_refresh(a);
    }
    if(method==0x512720)return game_menu_research_purchase(a);
    if(method==0x5285a0)return game_menu_research_catalog_build(a,receiver,argument);
    if(method==0x52bf80 || method==0x52bfb0 || method==0x52bfe0 || method==0x52c010) {
        uint32_t category=method==0x52bf80?0:method==0x52bfb0?1:method==0x52bfe0?2:3;
        game_memory_write_u32(m,0x58ac04,category);
        return !m->fault && game_menu_request(a,game_memory_read_u32(m,0x59d4c0),530,1001,1);
    }
    if(method==0x52c040)return game_menu_request(a,game_memory_read_u32(m,0x59d4c0),69,1000,11);
    if(method==0x52c060) {
        game_memory_write_u32(m,0x602bb0,1);
        return !m->fault && game_menu_request(a,game_memory_read_u32(m,0x59d4c0),69,1000,game_memory_read_u32(m,0x5eff04)?15:9);
    }
    if(method==0x52c100) {
        extern int game_menu_quick_apply(GameApplication *,GameAddress);
        return game_menu_quick_apply(a,0x5acbec) &&
            game_menu_request(a,game_memory_read_u32(m,0x59d4c0),530,1001,0) && game_menu_quick_parameter_action(a,0x523680,0);
    }
    if(method==0x52c0b0) {
        uint32_t network=game_memory_read_u32(m,0x5eff04);
        if(!network) {
            extern int game_level_selection_presets(GameApplication *);
            game_memory_write_u32(m,0x5acbec,game_memory_read_u32(m,0x5ac910));
            if(!game_level_selection_presets(a))return 0;
        }
        return game_menu_request(a,game_memory_read_u32(m,0x59d4c0),69,1000,network?15:9);
    }
    if(method==0x523680 || method==0x5236a0 || method==0x5236c0)
        return game_menu_quick_parameter_action(a,method,argument);
    if(method==0x52c140) {
        extern int game_menu_training_choose(GameApplication *,uint32_t);
        return game_menu_training_choose(a,argument);
    }
    if(method==0x524090 || method==0x524180 || method==0x524270 || method==0x5242b0 || method==0x524480 || method==0x5246b0) {
        extern int game_menu_save_action(GameApplication *,uint32_t,GameAddress,uint32_t);
        return game_menu_save_action(a,method,receiver,0);
    }
    if(method==0x523df0 || method==0x523ee0) {
        extern int game_menu_level_action(GameApplication *,uint32_t,GameAddress);
        return game_menu_level_action(a,method,receiver);
    }
    if(method==0x52c1f0) {
        extern int game_menu_results_toggle(GameApplication *,GameAddress);
        return game_menu_results_toggle(a,receiver);
    }
    if(method==0x473f20 || method==0x4740d0 || method==0x474280 || method==0x474310 || method==0x4743e0 ||
       method==0x474920 || method==0x474ab0 || method==0x474ac0)
        return game_menu_network_connection_action(a,method,receiver,argument);
    if(method==0x475480 || method==0x4754b0 || method==0x4754f0 || method==0x475530 || method==0x4756f0 || method==0x475710 ||
       method==0x475aa0 || method==0x475ae0 || method==0x475b60 || method==0x475bc0 || method==0x475c30)
        return game_menu_network_page_action(a,method,receiver,argument);
    if(method==0x44c350u) {
        extern int game_campaign_preview_select(GameApplication *,GameAddress,uint32_t);
        game_memory_write_u32(m,0x58fd3c,1);
        return !m->fault && game_campaign_preview_select(a,game_memory_read_u32(m,0x59c6c8),argument);
    }
    if(method==0x54be80u)return !m->fault; /* Actual original callback returns1. */
    if(method==0x523740 || method==0x523770 || method==0x523790 || method==0x5237d0 ||
       method==0x523800 || method==0x523830 || method==0x523850)
        return game_menu_keyboard_option_action(a,method,receiver,argument);
    /* 0052ca10/60 consume a borrowed address of the slider's owned float.
     * Volume is the low sixteen bits of original int64 truncation. */
    if(method==0x52ca10u || method==0x52ca60u) {
        long double value=game_menu_runtime_float(m,argument),scaled;
        uint32_t volume=0;GameAddress settings=game_memory_read_u32(m,0x5943c8);
        if(message!=6u)value=(long double)game_menu_runtime_float(m,0x575bf0)-value;
        scaled=value*game_menu_runtime_float(m,0x575bf4);
        if(isfinite(scaled) && scaled>=-9223372036854775808.0L && scaled<9223372036854775808.0L)
            volume=(uint32_t)(uint64_t)(int64_t)truncl(scaled)&0xffffu;
        if(method==0x52ca10u) {
            game_music_volume(volume);
            game_memory_write_u32(m,settings+0x54,volume);
        } else {
            sound_platform_master_volume(volume);
            game_memory_write_u32(m,settings+0x58,volume);
        }
        return !m->fault;
    }
    if(method==0x52cab0u || method==0x52cae0u) {
        SoundPlaybackServices services={a,status,position,command,format};
        GameAddress bank=game_memory_read_u32(m,0x628a90);
        if(!bank)return 0;
        if(method==0x52cab0u) {
            SoundPlaybackRequest request={56,0.0f,1.0f,0.0f,1,0};
            if(!sound_playback_play(m,bank,&request,&services))return 0;
            game_memory_write_u32(m,0x627fa0,game_memory_read_u32(m,bank));
            return !m->fault;
        }
        return sound_playback_stop(m,bank,56,game_memory_read_u32(m,0x627fa0),&services);
    }
    if(method==0x52c680 || method==0x52c720 || method==0x52c810 || method==0x52c8b0)
        return game_menu_music_action(a,method,argument);
    if(method==0x52c660)return game_menu_request(a,game_memory_read_u32(m,0x59d4c0),69,1000,4);
    if(method!=0x52cb00u)return game_menu_missing_route(a,"button callback",method);
    game_memory_write_u32(m,0x597084u,0);
    return !m->fault && game_menu_sound(a,0x91u) &&
        game_menu_request(a,game_memory_read_u32(m,0x59d4c0u),0x45u,1000u,3u);
}
