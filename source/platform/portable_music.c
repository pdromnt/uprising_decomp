#include "portable_sdl.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include "menu_music.h"
extern void *game_menu_window_handle(GameApplication *);
extern int game_application_audio_muted(GameApplication *);

/* MCI scalar device IDs are genuine native IDs. HWND, parameter pointers and
 * retry timer IDs stay full width and never enter legacy record fields. */
static struct {GameApplication *app;UINT_PTR retry;} music_retry;
/* Original 0063c750 GetTickCount import, shared by UI/game timing callers. */
uint32_t game_menu_milliseconds(GameApplication *a){(void)a;return (uint32_t)(uint32_t)SDL_GetTicks();}
uint32_t game_music_device_open(GameApplication *a,uint32_t *device,uint32_t *volume) {
    MCI_OPEN_PARMSA open;MCI_SET_PARMS set;MCI_GETDEVCAPS_PARMS caps;MCI_GENERIC_PARMS close;
    uint32_t code;memset(&open,0,sizeof(open));open.lpstrDeviceType="cdaudio";
    *device=0;*volume=0;code=mciSendCommandA(0,MCI_OPEN,MCI_OPEN_TYPE,(DWORD_PTR)&open);
    if(code)return code;
    memset(&set,0,sizeof(set));set.dwTimeFormat=MCI_FORMAT_TMSF;
    code=mciSendCommandA(open.wDeviceID,MCI_SET,MCI_SET_TIME_FORMAT,(DWORD_PTR)&set);
    if(!code) {
        memset(&caps,0,sizeof(caps));caps.dwItem=MCI_GETDEVCAPS_DEVICE_TYPE;
        code=mciSendCommandA(open.wDeviceID,MCI_GETDEVCAPS,MCI_GETDEVCAPS_ITEM|MCI_WAIT,(DWORD_PTR)&caps);
        if(!code && caps.dwReturn!=MCI_DEVTYPE_CD_AUDIO)code=MCIERR_DEVICE_TYPE_REQUIRED;
    }
    if(!code) {
        memset(&set,0,sizeof(set));
        code=mciSendCommandA(open.wDeviceID,MCI_SET,MCI_SET_DOOR_CLOSED|MCI_SET_AUDIO,(DWORD_PTR)&set);
    }
    if(!code && game_application_audio_muted(a)) {
        memset(&set,0,sizeof(set));set.dwAudio=MCI_SET_AUDIO_ALL;
        code=mciSendCommandA(open.wDeviceID,MCI_SET,MCI_SET_AUDIO|MCI_SET_OFF,(DWORD_PTR)&set);
    }
    if(code){
        memset(&close,0,sizeof(close));
        if(mciSendCommandA(open.wDeviceID,MCI_CLOSE,MCI_WAIT,(DWORD_PTR)&close))*device=open.wDeviceID;
        return code; /* A failed close still has an explicit owner to retry. */
    }
    memset(&caps,0,sizeof(caps));caps.dwItem=MCI_GETDEVCAPS_CAN_EJECT;
    if(!mciSendCommandA(open.wDeviceID,MCI_GETDEVCAPS,MCI_GETDEVCAPS_ITEM|MCI_WAIT,(DWORD_PTR)&caps))*volume=(uint32_t)caps.dwReturn;
    *device=open.wDeviceID;(void)a;return 0;
}
uint32_t game_music_device_status(GameApplication *a,uint32_t device,uint32_t item,uint32_t track,uint32_t *value) {
    MCI_STATUS_PARMS status;uint32_t code;memset(&status,0,sizeof(status));status.dwItem=item;status.dwTrack=track;
    code=mciSendCommandA(device,MCI_STATUS,MCI_STATUS_ITEM|MCI_WAIT|(track?MCI_TRACK:0),(DWORD_PTR)&status);
    *value=(uint32_t)status.dwReturn;(void)a;return code;
}
uint32_t game_music_device_play(GameApplication *a,uint32_t device,uint32_t first,uint32_t last,int has_last) {
    /* Some CD/MCI drivers accept SET_AUDIO OFF but still play audibly. Muted
     * validation must never issue PLAY; stop any previously started track too. */
    if(game_application_audio_muted(a)){
        static int reported;
        if(!reported){fputs("native CD music: playback suppressed (muted)\n",stderr);reported=1;}
        return game_music_device_stop(a,device);
    }
    MCI_PLAY_PARMS play;memset(&play,0,sizeof(play));play.dwCallback=(DWORD_PTR)game_menu_window_handle(a);
    play.dwFrom=first;play.dwTo=last;
    return mciSendCommandA(device,MCI_PLAY,MCI_NOTIFY|MCI_FROM|(has_last?MCI_TO:0),(DWORD_PTR)&play);
}
uint32_t game_music_device_stop(GameApplication *a,uint32_t device) {
    MCI_GENERIC_PARMS p;memset(&p,0,sizeof(p));(void)a;
    return mciSendCommandA(device,MCI_STOP,MCI_WAIT,(DWORD_PTR)&p);
}
uint32_t game_music_device_close(GameApplication *a,uint32_t device) {
    MCI_GENERIC_PARMS p;memset(&p,0,sizeof(p));(void)a;
    return mciSendCommandA(device,MCI_CLOSE,MCI_WAIT,(DWORD_PTR)&p);
}
void game_music_device_error(uint32_t code,char *text,size_t capacity) {
    if(!capacity)return;
    if(!mciGetErrorStringA(code,text,(UINT)capacity))lstrcpynA(text,"Unknown MCI error",(int)capacity);
}
/* The original worker retries at 500ms. Run its ordinary algorithm on the
 * window thread, so authoritative records need no racing state copies. */
static void CALLBACK retry_music(HWND window,UINT message,UINT_PTR timer,DWORD time) {
    (void)window;(void)message;(void)time;
    if(music_retry.app && music_retry.retry==timer && game_menu_music_refresh(music_retry.app)) {
        KillTimer(NULL,timer);music_retry.retry=0;
    }
}
int game_music_worker_start(GameApplication *a) {
    if(music_retry.retry)return music_retry.app==a;
    music_retry.app=a;music_retry.retry=SetTimer(NULL,0,500,retry_music);
    if(!music_retry.retry){music_retry.app=NULL;return 0;}return 1;
}
void game_music_worker_join(GameApplication *a) {
    if(music_retry.app!=a)return;
    if(music_retry.retry)KillTimer(NULL,music_retry.retry);
    music_retry.retry=0;music_retry.app=NULL;
}
