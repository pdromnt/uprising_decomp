#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "application_entry.h"

/* 00543180/00543280/00543340/005432e0: device indices and volume values
 * are integers, not narrowed native handles. All calls stay at this platform edge. */
int game_menu_audio_initialize(GameApplication *a) {
    GameMemory *m=&a->memory;UINT count=auxGetNumDevs(),i;DWORD volume=0;
    AUXCAPSA aux;WAVEOUTCAPSA wave;
    game_memory_write_u32(m,0x6319a0u,0);game_memory_write_u32(m,0x6319b0u,0);
    for(i=0;i<count;++i) {
        /* Retail probes device zero on each iteration; preserve that behavior. */
        if(auxGetDevCapsA(0,&aux,sizeof(aux))==MMSYSERR_NOERROR &&
            (aux.dwSupport&AUXCAPS_VOLUME) && aux.wTechnology==AUXCAPS_CDAUDIO) {
            game_memory_write_u32(m,0x631ac4u,i);game_memory_write_u32(m,0x6319a0u,1);
            if(auxGetVolume(i,&volume)==MMSYSERR_NOERROR)game_memory_write_u32(m,0x6319a8u,volume);
            break;
        }
    }
    if(!waveOutGetNumDevs())return 0;
    if(waveOutGetDevCapsA(0,&wave,sizeof(wave))==MMSYSERR_NOERROR && (wave.dwSupport&WAVECAPS_VOLUME)) {
        if(waveOutGetVolume(NULL,&volume)==MMSYSERR_NOERROR) {
            game_memory_write_u32(m,0x6319a4u,volume);
            volume&=0xffffu;waveOutSetVolume(NULL,volume|(volume<<16));
        }
    }
    return !m->fault && (game_memory_read_u32(m,0x6319b0u) || game_memory_read_u32(m,0x6319a0u));
}
int game_menu_aux_volume(GameApplication *a,uint32_t volume) {
    if(game_memory_read_u32(&a->memory,0x6319a0u)) {
        volume&=0xffffu;
        auxSetVolume(game_memory_read_u32(&a->memory,0x631ac4u),volume|(volume<<16));
    }
    return !a->memory.fault;
}
int game_menu_wave_volume(GameApplication *a,uint32_t volume) {
    volume&=0xffffu;waveOutSetVolume(NULL,volume|(volume<<16));return !a->memory.fault;
}
uint32_t game_menu_aux_volume_read(GameApplication *a) {
    DWORD volume=0;
    if(game_memory_read_u32(&a->memory,0x6319a0u))
        (void)auxGetVolume(game_memory_read_u32(&a->memory,0x631ac4u),&volume);
    return volume&0xffffu;
}
uint32_t game_menu_wave_volume_read(GameApplication *a) {
    DWORD volume=0;(void)a;
    if(waveOutGetVolume(NULL,&volume)!=MMSYSERR_NOERROR)return 0;
    return volume&0xffffu;
}
int game_menu_audio_restore(GameApplication *a) {
    GameMemory *m=&a->memory;
    if(game_memory_read_u32(m,0x6319a0u))auxSetVolume(game_memory_read_u32(m,0x631ac4u),game_memory_read_u32(m,0x6319a8u));
    waveOutSetVolume(NULL,game_memory_read_u32(m,0x6319a4u));return !m->fault;
}
