#define COBJMACROS
#include <windows.h>
#include <dsound.h>
#include <stdlib.h>
#include <string.h>
#include "sound_platform.h"

typedef struct SoundSlot {void *object;unsigned kind;int silent;} SoundSlot;
static SoundPlatformStats stats;
static SoundSlot *slots;
static uint32_t count,capacity;
static HMODULE module;
static CRITICAL_SECTION sound_lock;
static volatile LONG lock_state;
static void lock_sound(void){
    if(InterlockedCompareExchange(&lock_state,1,0)==0){InitializeCriticalSection(&sound_lock);InterlockedExchange(&lock_state,2);}
    else while(InterlockedCompareExchange(&lock_state,2,2)!=2)SwitchToThread();
    EnterCriticalSection(&sound_lock);
}
static void unlock_sound(void){LeaveCriticalSection(&sound_lock);}
static SoundSlot *find_sound(GameAddress id,unsigned kind){
    uint32_t index=(id&0xffffffu)-1u;
    if((id&0xff000000u)!=0xf9000000u || index>=count || !slots[index].object || slots[index].kind!=kind)return NULL;
    return slots+index;
}
static GameAddress own_sound(void *object,unsigned kind,int silent){
    if(count==capacity){
        uint32_t next=capacity?capacity*2u:256u;SoundSlot *storage;
        if(next>0xfffffeu)return 0u;
        storage=realloc(slots,(size_t)next*sizeof(*slots));if(!storage)return 0u;
        slots=storage;capacity=next;
    }
    slots[count].object=object;slots[count].kind=kind;slots[count].silent=silent;
    if(kind==1u){stats.devices++;stats.live_devices++;}else{stats.buffers++;stats.live_buffers++;}
    return 0xf9000000u|++count;
}
int sound_platform_owns(GameAddress id){
    int result;lock_sound();result=find_sound(id,1u)!=NULL || find_sound(id,2u)!=NULL;unlock_sound();return result;
}
void *sound_platform_device_acquire(GameAddress id) {
    SoundSlot *slot;void *object=NULL;
    lock_sound();slot=find_sound(id,1u);
    if(slot && !slot->silent){object=slot->object;IDirectSound_AddRef((IDirectSound *)object);}
    unlock_sound();return object;
}
void sound_platform_device_release_native(void *object) {
    if(object)IDirectSound_Release((IDirectSound *)object);
}
GameAddress sound_platform_device(void *window,const uint8_t *guid_bytes,int muted){
    typedef HRESULT (WINAPI *CreateSound)(LPCGUID,LPDIRECTSOUND *,LPUNKNOWN);
    CreateSound create;IDirectSound *device=NULL;GUID guid;GameAddress id=0u;char quiet[8]={0};int silent;
    /*Mute playback, not the bank: scripts need the real sample lengths even
     *when nothing is audible. A missing device still follows normal failure. */
    silent=muted || (GetEnvironmentVariableA("UPRISING_RECOMP_SILENT_NATIVE_SOUND",quiet,sizeof(quiet)) && quiet[0]=='1');
    lock_sound();if(!module)module=LoadLibraryA("dsound.dll");
    create=module?(CreateSound)(uintptr_t)GetProcAddress(module,"DirectSoundCreate"):NULL;
    if(guid_bytes){
        guid.Data1=(uint32_t)guid_bytes[0]|(uint32_t)guid_bytes[1]<<8|(uint32_t)guid_bytes[2]<<16|(uint32_t)guid_bytes[3]<<24;
        guid.Data2=(uint16_t)((uint16_t)guid_bytes[4]|(uint16_t)guid_bytes[5]<<8);
        guid.Data3=(uint16_t)((uint16_t)guid_bytes[6]|(uint16_t)guid_bytes[7]<<8);memcpy(guid.Data4,guid_bytes+8u,8u);
    }
    if(create && SUCCEEDED(create(guid_bytes?&guid:NULL,&device,NULL))){
        if(SUCCEEDED(IDirectSound_SetCooperativeLevel(device,(HWND)window,DSSCL_NORMAL)))id=own_sound(device,1u,silent);
        if(!id)IDirectSound_Release(device);
    }
    unlock_sound();return id;
}
GameAddress sound_platform_create_buffer(GameAddress device_id,const SoundWave *wave){
    SoundSlot *device;IDirectSoundBuffer *buffer=NULL;WAVEFORMATEX *format;DSBUFFERDESC description;
    GameAddress id=0u;DWORD bytes1=0u,bytes2=0u;void *data1=NULL,*data2=NULL;size_t extra=0u;
    if(!wave || !wave->pcm || !wave->pcm_bytes || !wave->frequency || !wave->channels || !wave->block_alignment)return 0u;
    if(wave->format_bytes>=18u)extra=(size_t)wave->format_data[16]|(size_t)wave->format_data[17]<<8;
    if(extra && (wave->format_bytes<18u || extra>wave->format_bytes-18u))return 0u;
    format=calloc(1u,sizeof(*format)+extra);if(!format)return 0u;
    format->wFormatTag=wave->format;format->nChannels=wave->channels;format->nSamplesPerSec=wave->frequency;
    format->nAvgBytesPerSec=wave->bytes_per_second;format->nBlockAlign=wave->block_alignment;
    format->wBitsPerSample=wave->bits_per_sample;format->cbSize=(WORD)extra;
    if(extra)memcpy((uint8_t *)format+18u,wave->format_data+18u,extra);
    memset(&description,0,sizeof(description));description.dwSize=sizeof(description);
    description.dwFlags=0xe2u;description.dwBufferBytes=wave->pcm_bytes;description.lpwfxFormat=format;
    lock_sound();device=find_sound(device_id,1u);
    if(device && SUCCEEDED(IDirectSound_CreateSoundBuffer((IDirectSound *)device->object,&description,&buffer,NULL))){
        if(SUCCEEDED(IDirectSoundBuffer_Lock(buffer,0u,wave->pcm_bytes,&data1,&bytes1,&data2,&bytes2,0u))){
            if((uint64_t)bytes1+bytes2==wave->pcm_bytes && data1 && (!bytes2 || data2)){
                memcpy(data1,wave->pcm,bytes1);if(bytes2)memcpy(data2,wave->pcm+bytes1,bytes2);
                if(SUCCEEDED(IDirectSoundBuffer_Unlock(buffer,data1,bytes1,data2,bytes2)))id=own_sound(buffer,2u,device->silent);
            }else (void)IDirectSoundBuffer_Unlock(buffer,data1,bytes1,data2,bytes2);
        }
        if(!id)IDirectSoundBuffer_Release(buffer);
    }
    unlock_sound();free(format);return id;
}
GameAddress sound_platform_duplicate(GameAddress device_id,GameAddress buffer_id){
    SoundSlot *device,*source;IDirectSoundBuffer *buffer=NULL;GameAddress id=0u;
    lock_sound();device=find_sound(device_id,1u);source=find_sound(buffer_id,2u);
    if(device && source && SUCCEEDED(IDirectSound_DuplicateSoundBuffer((IDirectSound *)device->object,(IDirectSoundBuffer *)source->object,&buffer))){
        id=own_sound(buffer,2u,source->silent);if(!id)IDirectSoundBuffer_Release(buffer);
    }
    unlock_sound();return id;
}
int sound_platform_release_buffer(GameAddress id){
    SoundSlot *slot;int result=0;lock_sound();slot=find_sound(id,2u);
    if(slot){IDirectSoundBuffer_Release((IDirectSoundBuffer *)slot->object);slot->object=NULL;stats.buffer_releases++;stats.live_buffers--;result=1;}
    unlock_sound();return result;
}
int sound_platform_release_device(GameAddress id){
    SoundSlot *slot;int result=0;lock_sound();slot=find_sound(id,1u);
    if(slot){IDirectSound_Release((IDirectSound *)slot->object);slot->object=NULL;stats.device_releases++;stats.live_devices--;result=1;}
    unlock_sound();return result;
}
int32_t sound_platform_status(GameAddress id,uint32_t *status){
    SoundSlot *slot;DWORD value=0u;HRESULT result=DSERR_INVALIDPARAM;lock_sound();slot=find_sound(id,2u);
    if(slot)result=IDirectSoundBuffer_GetStatus((IDirectSoundBuffer *)slot->object,&value);
    *status=value;unlock_sound();return (int32_t)result;
}
int32_t sound_platform_position(GameAddress id,uint32_t *position){
    SoundSlot *slot;DWORD value=0u;HRESULT result=DSERR_INVALIDPARAM;lock_sound();slot=find_sound(id,2u);
    if(slot)result=IDirectSoundBuffer_GetCurrentPosition((IDirectSoundBuffer *)slot->object,&value,NULL);
    *position=value;unlock_sound();return (int32_t)result;
}
int32_t sound_platform_format(GameAddress id,uint32_t *frequency,uint16_t *bits){
    SoundSlot *slot;WAVEFORMATEX format;HRESULT result=DSERR_INVALIDPARAM;memset(&format,0,sizeof(format));
    lock_sound();slot=find_sound(id,2u);
    if(slot)result=IDirectSoundBuffer_GetFormat((IDirectSoundBuffer *)slot->object,&format,18u,NULL);
    *frequency=format.nSamplesPerSec;*bits=format.wBitsPerSample;unlock_sound();return (int32_t)result;
}
int32_t sound_platform_frequency(GameAddress id,uint32_t *frequency){
    SoundSlot *slot;DWORD value=0u;HRESULT result=DSERR_INVALIDPARAM;lock_sound();slot=find_sound(id,2u);
    if(slot)result=IDirectSoundBuffer_GetFrequency((IDirectSoundBuffer *)slot->object,&value);
    *frequency=value;unlock_sound();return (int32_t)result;
}
int32_t sound_platform_command(GameAddress id,SoundBufferCommand command,uint32_t value){
    SoundSlot *slot;HRESULT result=DSERR_INVALIDPARAM;lock_sound();slot=find_sound(id,2u);
    if(slot){IDirectSoundBuffer *buffer=slot->object;
        switch(command){
        case SOUND_BUFFER_VOLUME:{int32_t volume=value<=INT32_MAX?(int32_t)value:(int32_t)((int64_t)value-4294967296LL);result=IDirectSoundBuffer_SetVolume(buffer,slot->silent?DSBVOLUME_MIN:volume);break;}
        case SOUND_BUFFER_FREQUENCY:result=IDirectSoundBuffer_SetFrequency(buffer,value);break;
        case SOUND_BUFFER_PLAY:
            if(slot->silent && FAILED(result=IDirectSoundBuffer_SetVolume(buffer,DSBVOLUME_MIN)))break;
            result=IDirectSoundBuffer_Play(buffer,0u,0u,value);break;
        case SOUND_BUFFER_STOP:result=IDirectSoundBuffer_Stop(buffer);break;
        case SOUND_BUFFER_SEEK:result=IDirectSoundBuffer_SetCurrentPosition(buffer,value);break;
        }
    }
    unlock_sound();return (int32_t)result;
}
void sound_platform_shutdown(void){
    uint32_t i;lock_sound();
    for(i=0;i<count;i++)if(slots[i].object && slots[i].kind==2u){IDirectSoundBuffer_Release((IDirectSoundBuffer *)slots[i].object);slots[i].object=NULL;stats.buffer_releases++;stats.live_buffers--;}
    for(i=0;i<count;i++)if(slots[i].object){IDirectSound_Release((IDirectSound *)slots[i].object);slots[i].object=NULL;stats.device_releases++;stats.live_devices--;}
    free(slots);slots=NULL;count=capacity=0u;
    if(module){FreeLibrary(module);module=NULL;}unlock_sound();
}
void sound_platform_stats(SoundPlatformStats *output){lock_sound();*output=stats;unlock_sound();}
