#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <dplay.h> /*Only fixed legacy result codes and provider IDs.*/
#include <dplay8.h>
#include <dpaddr.h>
#include <dplobby8.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "menu_network.h"

/*The platform service owns all native SDK objects and copied callback data.
 * SDK workers never enter GameApplication/GameMemory. Game roster/clock code
 * drains the bounded queue on its own thread, with no SDK pointers in packets.
 * DP8 sessions connect reconstructed peers, not the retail DP3 wire protocol.*/
#define NETWORK_QUEUE_LIMIT (8u*1024u*1024u)
#define NETWORK_PACKET_LIMIT (1024u*1024u)
#define NETWORK_SESSION_LIMIT 128u
#define NETWORK_PORT 2302u
static const uint8_t open_policy[8]={'U','N','P','1',1,0,0,0};
typedef struct NetworkPacket {
    struct NetworkPacket *next;
    uint32_t sender,size;
    uint8_t data[];
} NetworkPacket;
typedef struct NetworkSession {
    struct NetworkSession *next;
    GUID guid;
    char name[128];
    IDirectPlay8Address *host,*device;
} NetworkSession;
typedef struct NetworkPeer {DPNID id;int published;} NetworkPeer;
struct GameNetworkTransport {
    IDirectPlay8Peer *play;
    IDirectPlay8LobbiedApplication *lobby;
    DPNHANDLE lobby_connection;
    CRITICAL_SECTION lock;
    GUID provider;
    NetworkPacket *head,*tail;
    NetworkSession *sessions;
    NetworkPeer peers[4];
    uint32_t queued,session_count;
    DPNID local,host;
    int com_owned,initialized,opened,accepting,failed,enumerating;
};
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put_word(uint8_t *p,uint32_t v){unsigned i;for(i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
static int wide(const char *in,WCHAR *out,int capacity){return in && MultiByteToWideChar(CP_ACP,0,in,-1,out,capacity)>0;}
static int narrow(const WCHAR *in,char *out,int capacity){return in && WideCharToMultiByte(CP_ACP,0,in,-1,out,capacity,NULL,NULL)>0;}
static void queue_locked(struct GameNetworkTransport *n,uint32_t sender,const void *data,uint32_t size){
    NetworkPacket *p;
    if(n->failed)return;
    if(size>NETWORK_PACKET_LIMIT || n->queued>NETWORK_QUEUE_LIMIT-size-sizeof(*p) || !(p=malloc(sizeof(*p)+size))){n->failed=1;return;}
    p->next=NULL;p->sender=sender;p->size=size;if(size)memcpy(p->data,data,size);
    if(n->tail)n->tail->next=p;else n->head=p;n->tail=p;n->queued+=(uint32_t)sizeof(*p)+size;
}
static void system_locked(struct GameNetworkTransport *n,uint32_t type,DPNID id,const void *data,const char *name){
    uint8_t event[16+16+128]={0};uint32_t size=16;
    put_word(event,type);put_word(event+4,id);put_word(event+8,1);
    if(type==3){size_t length=strlen(name)+1;if(length>128){n->failed=1;return;}
        put_word(event+12,16);memcpy(event+16,data,16);memcpy(event+32,name,length);size=32+(uint32_t)length;}
    queue_locked(n,0,event,size);
}
static void peer_info(struct GameNetworkTransport *n,DPNID id){
    DWORD size=0;DPN_PLAYER_INFO *info;HRESULT hr;unsigned i;char name[128];
    hr=IDirectPlay8Peer_GetPeerInfo(n->play,id,NULL,&size,0);
    if(hr!=DPNERR_BUFFERTOOSMALL || size<sizeof(*info) || size>65536)return;
    info=calloc(1,size);if(!info){EnterCriticalSection(&n->lock);n->failed=1;LeaveCriticalSection(&n->lock);return;}
    info->dwSize=sizeof(*info);hr=IDirectPlay8Peer_GetPeerInfo(n->play,id,info,&size,0);
    if(SUCCEEDED(hr)){
        EnterCriticalSection(&n->lock);
        for(i=0;i<4 && n->peers[i].id && n->peers[i].id!=id;++i){}
        if(i==4)n->failed=1;
        else{
            n->peers[i].id=id;
            if(info->dwPlayerFlags&DPNPLAYER_LOCAL)n->local=id;
            if(info->dwPlayerFlags&DPNPLAYER_HOST)n->host=id;
            /*Host/Connect create the peer before the game's CreatePlayer call.
             * Publish the remote roster event only once its real16-byte game
             * metadata arrives through SetPeerInfo. Never register self twice.*/
            if(!(info->dwPlayerFlags&DPNPLAYER_LOCAL) && !n->peers[i].published && info->dwDataSize==16 && info->pvData && narrow(info->pwszName,name,sizeof(name))){
                system_locked(n,3,id,info->pvData,name);n->peers[i].published=1;
            }
        }
        LeaveCriticalSection(&n->lock);
    }
    free(info);
}
static DPN_APPLICATION_DESC *application_description(struct GameNetworkTransport *n){
    DPN_APPLICATION_DESC *d;DWORD size=0;HRESULT hr;
    hr=IDirectPlay8Peer_GetApplicationDesc(n->play,NULL,&size,0);
    if(hr!=DPNERR_BUFFERTOOSMALL || size<sizeof(*d) || size>65536)return NULL;
    d=calloc(1,size);if(!d)return NULL;d->dwSize=sizeof(*d);
    if(FAILED(IDirectPlay8Peer_GetApplicationDesc(n->play,d,&size,0))){free(d);return NULL;}
    return d;
}
static void application_policy(struct GameNetworkTransport *n){
    DPN_APPLICATION_DESC *d;
    /*Serialize the query with its cache publication. Otherwise an older
     * callback can read "open", pause, and overwrite a later "closed" value.*/
    EnterCriticalSection(&n->lock);
    d=application_description(n);
    if(d && !d->dwApplicationReservedDataSize)n->accepting=1;
    else if(d && d->dwApplicationReservedDataSize==8 && d->pvApplicationReservedData &&
            !memcmp(d->pvApplicationReservedData,open_policy,4) && word((const uint8_t *)d->pvApplicationReservedData+4)<=1)
        n->accepting=(int)word((const uint8_t *)d->pvApplicationReservedData+4);
    else{n->accepting=0;n->failed=1;}
    LeaveCriticalSection(&n->lock);free(d);
}
static HRESULT WINAPI network_callback(void *context,DWORD kind,void *message){
    struct GameNetworkTransport *n=context;
    if(kind==DPN_MSGID_CREATE_PLAYER){DPNMSG_CREATE_PLAYER *p=message;peer_info(n,p->dpnidPlayer);}
    else if(kind==DPN_MSGID_PEER_INFO){DPNMSG_PEER_INFO *p=message;peer_info(n,p->dpnidPeer);}
    else if(kind==DPN_MSGID_RECEIVE){DPNMSG_RECEIVE *p=message;
        EnterCriticalSection(&n->lock);queue_locked(n,p->dpnidSender,p->pReceiveData,p->dwReceiveDataSize);LeaveCriticalSection(&n->lock);
    }else if(kind==DPN_MSGID_DESTROY_PLAYER){DPNMSG_DESTROY_PLAYER *p=message;unsigned i;
        EnterCriticalSection(&n->lock);
        for(i=0;i<4;++i)if(n->peers[i].id==p->dpnidPlayer){
            if(n->peers[i].published)system_locked(n,5,p->dpnidPlayer,NULL,NULL);
            memset(&n->peers[i],0,sizeof(n->peers[i]));break;
        }
        LeaveCriticalSection(&n->lock);
    }else if(kind==DPN_MSGID_HOST_MIGRATE){DPNMSG_HOST_MIGRATE *p=message;
        /*The shared SDK description, not each peer's old local toggle, owns
         * admission policy. Recover it before the new host can accept joins.*/
        application_policy(n);
        EnterCriticalSection(&n->lock);n->host=p->dpnidNewHost;
        /*Retail DPSYS_HOST is delivered only to the newly selected host.*/
        if(n->host==n->local)system_locked(n,257,n->host,NULL,NULL);
        LeaveCriticalSection(&n->lock);
    }else if(kind==DPN_MSGID_APPLICATION_DESC){
        application_policy(n);
    }else if(kind==DPN_MSGID_INDICATE_CONNECT){int accepting;
        EnterCriticalSection(&n->lock);accepting=n->accepting;LeaveCriticalSection(&n->lock);
        if(!accepting)return DPNERR_HOSTREJECTEDCONNECTION;
    }else if(kind==DPN_MSGID_ENUM_HOSTS_RESPONSE){DPNMSG_ENUM_HOSTS_RESPONSE *p=message;NetworkSession *s,**at;
        const DPN_APPLICATION_DESC *d=p->pApplicationDescription;
        if(!d || !p->pAddressSender || !p->pAddressDevice)return S_OK;
        /*DP8 session flags cannot be changed after Host. Its supported mutable
         * application data carries admission to discovery and future hosts.*/
        if(d->dwApplicationReservedDataSize==8 && d->pvApplicationReservedData &&
           !memcmp(d->pvApplicationReservedData,open_policy,4) && !word((const uint8_t *)d->pvApplicationReservedData+4))return S_OK;
        EnterCriticalSection(&n->lock);
        if(!n->enumerating){LeaveCriticalSection(&n->lock);return S_OK;}
        for(s=n->sessions;s;s=s->next)if(!memcmp(&s->guid,&d->guidInstance,16))break;
        if(!s){
            s=calloc(1,sizeof(*s));
            if(!s || n->session_count>=NETWORK_SESSION_LIMIT || !narrow(d->pwszSessionName,s->name,sizeof(s->name))){free(s);n->failed=1;}
            else if(FAILED(IDirectPlay8Address_Duplicate(p->pAddressSender,&s->host)) || FAILED(IDirectPlay8Address_Duplicate(p->pAddressDevice,&s->device))){
                if(s->host)IDirectPlay8Address_Release(s->host);free(s);n->failed=1;
            }else{memcpy(&s->guid,&d->guidInstance,16);
                for(at=&n->sessions;*at && strcmp(s->name,(*at)->name)>=0;at=&(*at)->next){}
                s->next=*at;*at=s;++n->session_count;}
        }
        LeaveCriticalSection(&n->lock);
    }
    return S_OK;
}
static HRESULT WINAPI lobby_callback(void *context,DWORD kind,void *message){
    (void)context;(void)kind;(void)message;return S_OK;
}
static void sessions_clear(struct GameNetworkTransport *n){NetworkSession *s;
    while((s=n->sessions)!=NULL){n->sessions=s->next;IDirectPlay8Address_Release(s->host);IDirectPlay8Address_Release(s->device);free(s);}n->session_count=0;
}
static void packets_clear(struct GameNetworkTransport *n){NetworkPacket *p;
    while((p=n->head)!=NULL){n->head=p->next;free(p);}n->tail=NULL;n->queued=0;
}
static void release(struct GameNetworkTransport *n){
    if(!n)return;
    /*Close waits for SDK callbacks before their queue/lock/context are freed.*/
    if(n->play){if(n->initialized)IDirectPlay8Peer_Close(n->play,0);IDirectPlay8Peer_Release(n->play);}
    if(n->lobby){IDirectPlay8LobbiedApplication_Close(n->lobby,0);IDirectPlay8LobbiedApplication_Release(n->lobby);}
    sessions_clear(n);packets_clear(n);DeleteCriticalSection(&n->lock);
    if(n->com_owned)CoUninitialize();
    free(n);
}
static struct GameNetworkTransport *create(void){
    struct GameNetworkTransport *n=calloc(1,sizeof(*n));HRESULT hr;
    if(!n)return NULL;
    InitializeCriticalSection(&n->lock);n->accepting=1;
    hr=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);n->com_owned=SUCCEEDED(hr);
    if(FAILED(hr) && hr!=RPC_E_CHANGED_MODE)goto fail;
    hr=CoCreateInstance(&CLSID_DirectPlay8Peer,NULL,CLSCTX_INPROC_SERVER,&IID_IDirectPlay8Peer,(void **)&n->play);
    if(FAILED(hr))goto fail;
    hr=IDirectPlay8Peer_Initialize(n->play,n,network_callback,0);if(FAILED(hr))goto fail;
    n->initialized=1;return n;
fail:fprintf(stderr,"native DP8 initialization failed: %08lx\n",(unsigned long)hr);release(n);return NULL;
}
static int reinitialize(struct GameNetworkTransport *n){
    if(n->initialized)return 1;
    if(FAILED(IDirectPlay8Peer_Initialize(n->play,n,network_callback,0)))return 0;
    n->initialized=1;return 1;
}
static IDirectPlay8Address *address(struct GameNetworkTransport *n,int remote,int hosting){
    IDirectPlay8Address *p=NULL;char endpoint[256];DWORD length,port=NETWORK_PORT;WCHAR host[256];
    if(FAILED(CoCreateInstance(&CLSID_DirectPlay8Address,NULL,CLSCTX_INPROC_SERVER,&IID_IDirectPlay8Address,(void **)&p)))return NULL;
    if(FAILED(IDirectPlay8Address_SetSP(p,&n->provider)))goto fail;
    if(!memcmp(&n->provider,&CLSID_DP8SP_TCPIP,16)){
        if((remote || hosting) && FAILED(IDirectPlay8Address_AddComponent(p,DPNA_KEY_PORT,&port,sizeof(port),DPNA_DATATYPE_DWORD)))goto fail;
        if(remote){length=GetEnvironmentVariableA("UPRISING_RECOMP_NETWORK_ADDRESS",endpoint,sizeof(endpoint));
            if(length>=sizeof(endpoint))goto fail;
            if(length && (!wide(endpoint,host,256) || FAILED(IDirectPlay8Address_AddComponent(p,DPNA_KEY_HOSTNAME,host,((DWORD)lstrlenW(host)+1)*sizeof(WCHAR),DPNA_DATATYPE_STRING))))goto fail;
        }
    }
    return p;
fail:IDirectPlay8Address_Release(p);return NULL;
}
void game_network_dispose(GameApplication *a){release(a->network);a->network=NULL;}
int game_network_providers(GameApplication *a,GameAddress selector){
    struct GameNetworkTransport *n=create();DPN_SERVICE_PROVIDER_INFO *providers=NULL;DWORD size=0,count=0,i;int ok=0;HRESULT hr;
    if(!n)return 0;
    hr=IDirectPlay8Peer_EnumServiceProviders(n->play,NULL,NULL,NULL,&size,&count,0);
    if(hr!=DPNERR_BUFFERTOOSMALL || !size || size>65536)goto done;
    providers=malloc(size);if(!providers)goto done;
    if(FAILED(IDirectPlay8Peer_EnumServiceProviders(n->play,NULL,NULL,providers,&size,&count,0)) || count>size/sizeof(*providers))goto done;
    ok=1;for(i=0;i<count && ok;++i){uint8_t guid[16];char name[128];
        if(!narrow(providers[i].pwszName,name,sizeof(name))){ok=0;break;}
        /*Retain the existing TCP menu row's stable provider ID/localized label.*/
        memcpy(guid,!memcmp(&providers[i].guid,&CLSID_DP8SP_TCPIP,16)?&DPSPGUID_TCPIP:&providers[i].guid,16);
        ok=game_menu_network_provider_row(a,selector,guid,name);
    }
done:free(providers);release(n);return ok && !a->memory.fault;
}
int game_network_provider_open(GameApplication *a,const uint8_t bytes[16]){
    struct GameNetworkTransport *n;if(!bytes || !(n=create()))return 0;
    memcpy(&n->provider,bytes,16);if(!memcmp(bytes,&DPSPGUID_TCPIP,16))n->provider=CLSID_DP8SP_TCPIP;
    game_network_dispose(a);a->network=n;return 1;
}
int game_network_session_open(GameApplication *a,int hosting,const char *name,const uint8_t instance[16]){
    struct GameNetworkTransport *n=a->network;DPN_APPLICATION_DESC d={0};IDirectPlay8Address *device=NULL;NetworkSession *s;
    WCHAR title[128];HRESULT hr=E_INVALIDARG;
    if(!n || !reinitialize(n) || !game_memory_read(&a->memory,0x573bf8,&d.guidApplication,16))return 0;
    d.dwSize=sizeof(d);
    if(hosting){
        if(!wide(name,title,128) || !(device=address(n,0,1)))return 0;
        d.dwFlags=DPNSESSION_MIGRATE_HOST|DPNSESSION_NODPNSVR;d.dwMaxPlayers=4;d.pwszSessionName=title;
        d.pvApplicationReservedData=(void *)open_policy;d.dwApplicationReservedDataSize=sizeof(open_policy);
        hr=IDirectPlay8Peer_Host(n->play,&d,&device,1,NULL,NULL,NULL,0);IDirectPlay8Address_Release(device);
    }else if(instance){
        for(s=n->sessions;s && memcmp(&s->guid,instance,16);s=s->next){}
        if(s){memcpy(&d.guidInstance,instance,16);hr=IDirectPlay8Peer_Connect(n->play,&d,s->host,s->device,NULL,NULL,NULL,0,NULL,NULL,NULL,DPNCONNECT_SYNC);}
    }
    if(FAILED(hr)){fprintf(stderr,"native DP8 %s failed: %08lx\n",hosting?"host":"join",(unsigned long)hr);return 0;}
    n->opened=1;return 1;
}
int game_network_sessions(GameApplication *a,GameAddress selector,const uint8_t *selected){
    struct GameNetworkTransport *n=a->network;DPN_APPLICATION_DESC d={0};IDirectPlay8Address *host=NULL,*device=NULL;NetworkSession *s;HRESULT hr;int ok=0;
    if(!n || !reinitialize(n) || !game_memory_read(&a->memory,0x573bf8,&d.guidApplication,16))return 0;
    d.dwSize=sizeof(d);
    host=address(n,1,0);device=address(n,0,0);if(!host || !device)goto done;
    EnterCriticalSection(&n->lock);sessions_clear(n);n->enumerating=1;LeaveCriticalSection(&n->lock);
    hr=IDirectPlay8Peer_EnumHosts(n->play,&d,host,device,NULL,0,2,250,750,NULL,NULL,DPNENUMHOSTS_SYNC);
    EnterCriticalSection(&n->lock);n->enumerating=0;LeaveCriticalSection(&n->lock);
    if(FAILED(hr) || n->failed)goto done;
    ok=1;for(s=n->sessions;s && ok;s=s->next)ok=game_menu_network_session_row(a,selector,(const uint8_t *)&s->guid,s->name,selected);
done:if(host)IDirectPlay8Address_Release(host);if(device)IDirectPlay8Address_Release(device);return ok && !a->memory.fault;
}
/*External lobby launches use the native DP8 lobby contract. No installed lobby
 * connection is an ordinary unavailable route, not a fabricated session.*/
int game_network_lobby_open(GameApplication *a){
    struct GameNetworkTransport *n;DPL_CONNECTION_SETTINGS *settings=NULL;DWORD size=0,i;HRESULT hr;int ok=0,settings_owned=0;
    if(a->network)return 1;
    n=create();if(!n)return 0;
    hr=CoCreateInstance(&CLSID_DirectPlay8LobbiedApplication,NULL,CLSCTX_INPROC_SERVER,&IID_IDirectPlay8LobbiedApplication,(void **)&n->lobby);if(FAILED(hr))goto done;
    hr=IDirectPlay8LobbiedApplication_Initialize(n->lobby,n,lobby_callback,&n->lobby_connection,0);
    if(FAILED(hr) || !n->lobby_connection)goto done;
    hr=IDirectPlay8LobbiedApplication_GetConnectionSettings(n->lobby,n->lobby_connection,NULL,&size,0);
    if(hr!=DPNERR_BUFFERTOOSMALL || size<sizeof(*settings) || size>65536)goto done;
    settings=calloc(1,size);if(!settings)goto done;settings->dwSize=sizeof(*settings);
    if(FAILED(IDirectPlay8LobbiedApplication_GetConnectionSettings(n->lobby,n->lobby_connection,settings,&size,0)))goto done;
    settings_owned=1; /*The SDK adds a reference to each returned address.*/
    if(!settings->cNumDeviceAddresses || !settings->ppdp8DeviceAddresses)goto done;
    if(FAILED(IDirectPlay8Address_GetSP(settings->ppdp8DeviceAddresses[0],&n->provider)))goto done;
    if(FAILED(IDirectPlay8Peer_RegisterLobby(n->play,n->lobby_connection,n->lobby,DPNLOBBY_REGISTER)))goto done;
    settings->dpnAppDesc.dwMaxPlayers=4;settings->dpnAppDesc.dwFlags|=DPNSESSION_MIGRATE_HOST|DPNSESSION_NODPNSVR;
    settings->dpnAppDesc.pvApplicationReservedData=(void *)open_policy;settings->dpnAppDesc.dwApplicationReservedDataSize=sizeof(open_policy);
    if(settings->dwFlags&DPLCONNECTSETTINGS_HOST)hr=IDirectPlay8Peer_Host(n->play,&settings->dpnAppDesc,settings->ppdp8DeviceAddresses,settings->cNumDeviceAddresses,NULL,NULL,NULL,0);
    else hr=IDirectPlay8Peer_Connect(n->play,&settings->dpnAppDesc,settings->pdp8HostAddress,settings->ppdp8DeviceAddresses[0],NULL,NULL,NULL,0,NULL,NULL,NULL,DPNCONNECT_SYNC);
    if(FAILED(hr))goto done;
    n->opened=1;a->network=n;n=NULL;ok=1;
done:
    if(settings_owned){
        if(settings->pdp8HostAddress)IDirectPlay8Address_Release(settings->pdp8HostAddress);
        if(settings->ppdp8DeviceAddresses)for(i=0;i<settings->cNumDeviceAddresses;++i)
            if(settings->ppdp8DeviceAddresses[i])IDirectPlay8Address_Release(settings->ppdp8DeviceAddresses[i]);
    }
    free(settings);release(n);return ok;
}
int game_network_capabilities(GameApplication *a,GameNetworkCapabilities *out){
    struct GameNetworkTransport *n=a->network;DPN_SP_CAPS caps={0};DPN_CONNECTION_INFO info={0};DPNID local,host;
    if(!n || !out)return 0;
    caps.dwSize=sizeof(caps);if(FAILED(IDirectPlay8Peer_GetSPCaps(n->play,&n->provider,&caps,0)))return 0;
    EnterCriticalSection(&n->lock);local=n->local;host=n->host;LeaveCriticalSection(&n->lock);
    memset(out,0,sizeof(*out));out->flags=local && local==host?2:0;
    /*DP8 has no advertised link baud rate. Zero stays unknown; the core's
     * existing100ms fallback remains until real connection measurements exist.*/
    if(host && host!=local){info.dwSize=sizeof(info);if(SUCCEEDED(IDirectPlay8Peer_GetConnectionInfo(n->play,host,&info,0))){
        out->latency=info.dwRoundTripLatencyMS;out->hundred_baud=info.dwThroughputBPS/100u*8u;
    }}return 1;
}
int game_network_player_create(GameApplication *a,const char *name,const uint8_t data[16],uint32_t *id){
    struct GameNetworkTransport *n=a->network;DPN_PLAYER_INFO p={0};WCHAR title[128];
    if(!n || !n->opened || !data || !id || !wide(name,title,128))return 0;
    p.dwSize=sizeof(p);p.dwInfoFlags=DPNINFO_NAME|DPNINFO_DATA;p.pwszName=title;p.pvData=(void *)data;p.dwDataSize=16;
    if(FAILED(IDirectPlay8Peer_SetPeerInfo(n->play,&p,NULL,NULL,DPNSETPEERINFO_SYNC)))return 0;
    EnterCriticalSection(&n->lock);*id=n->local;LeaveCriticalSection(&n->lock);return *id!=0;
}
uint32_t game_network_receive_result(GameApplication *a,uint32_t *sender,uint32_t *receiver,uint8_t *data,uint32_t *size){
    struct GameNetworkTransport *n=a->network;NetworkPacket *p;uint32_t result=DP_OK;
    if(!n || !sender || !receiver || !data || !size)return (uint32_t)DPERR_INVALIDPARAMS;
    EnterCriticalSection(&n->lock);p=n->head;
    if(n->failed)result=(uint32_t)DPERR_GENERIC;
    else if(!p)result=(uint32_t)DPERR_NOMESSAGES;
    else{*sender=p->sender;*receiver=n->local;if(*size<p->size)result=(uint32_t)DPERR_BUFFERTOOSMALL;
        else{if(p->size)memcpy(data,p->data,p->size);n->head=p->next;if(!n->head)n->tail=NULL;n->queued-=(uint32_t)sizeof(*p)+p->size;}
        *size=p->size;if(!result)free(p);
    }
    LeaveCriticalSection(&n->lock);return result;
}
int game_network_receive(GameApplication *a,uint32_t *sender,uint32_t *receiver,uint8_t *data,uint32_t *size){
    uint32_t result=game_network_receive_result(a,sender,receiver,data,size);return result==(uint32_t)DPERR_NOMESSAGES?0:result==DP_OK?1:-1;
}
int game_network_transport_close(GameApplication *a){
    struct GameNetworkTransport *n=a->network;HRESULT hr;
    if(!n)return 0;
    if(!n->initialized)return 1;
    hr=IDirectPlay8Peer_Close(n->play,0);if(FAILED(hr))return 0;
    n->initialized=0;n->opened=0;n->local=n->host=0;n->failed=0;n->accepting=1;
    memset(n->peers,0,sizeof(n->peers));packets_clear(n);sessions_clear(n);return 1;
}
uint32_t game_network_milliseconds(GameApplication *a){(void)a;return (uint32_t)GetTickCount();}
static uint32_t result_code(HRESULT hr){
    if(SUCCEEDED(hr))return DP_OK;
    if(hr==DPNERR_INVALIDPLAYER || hr==DPNERR_PLAYERLOST || hr==DPNERR_PLAYERNOTREACHABLE)return (uint32_t)DPERR_INVALIDPLAYER;
    if(hr==DPNERR_INVALIDOBJECT || hr==DPNERR_UNINITIALIZED || hr==DPNERR_NOCONNECTION)return (uint32_t)DPERR_INVALIDOBJECT;
    if(hr==DPNERR_NOTREADY || hr==DPNERR_CONNECTING)return (uint32_t)DPERR_BUSY;
    return (uint32_t)hr;
}
uint32_t game_network_send_result(GameApplication *a,uint32_t from,uint32_t to,const uint8_t *data,uint32_t size){
    struct GameNetworkTransport *n=a->network;DPN_BUFFER_DESC p;DPNID local;
    if(!n || (!data && size) || size>NETWORK_PACKET_LIMIT)return (uint32_t)DPERR_INVALIDPARAMS;
    EnterCriticalSection(&n->lock);local=n->local;LeaveCriticalSection(&n->lock);
    if(from!=local || !local)return (uint32_t)DPERR_INVALIDPLAYER;
    p.dwBufferSize=size;p.pBufferData=(BYTE *)data;
    return result_code(IDirectPlay8Peer_SendTo(n->play,to,&p,1,0,NULL,NULL,DPNSEND_SYNC|DPNSEND_NOLOOPBACK));
}
int game_network_send(GameApplication *a,uint32_t from,uint32_t to,const uint8_t *data,uint32_t size){return game_network_send_result(a,from,to,data,size)==DP_OK;}
int game_network_player_destroy(GameApplication *a,uint32_t id){
    struct GameNetworkTransport *n=a->network;DPNID local;
    if(!n)return 0;
    EnterCriticalSection(&n->lock);local=n->local;LeaveCriticalSection(&n->lock);
    if(id==local)return game_network_transport_close(a);
    /*The roster also requests destruction after a real SDK departure.*/
    {HRESULT hr=IDirectPlay8Peer_DestroyPeer(n->play,id,NULL,0,0);
     return SUCCEEDED(hr) || hr==DPNERR_INVALIDPLAYER || hr==DPNERR_PLAYERLOST;}
}
int game_network_enable_players(GameApplication *a,int enabled){
    struct GameNetworkTransport *n=a->network;DPN_APPLICATION_DESC *d;HRESULT hr;uint8_t policy[8];
    if(!n)return 0;
    d=application_description(n);
    if(!d){DWORD bytes=0;char diagnostic[128];hr=IDirectPlay8Peer_GetApplicationDesc(n->play,NULL,&bytes,0);
        snprintf(diagnostic,sizeof(diagnostic),"native DP8 description unavailable: result=%08lx size=%lu",(unsigned long)hr,(unsigned long)bytes);
        game_application_log(a,"%s\n",diagnostic);return 0;}
    /*Only supported mutable SDK application data carries this8-byte service
     * record. The SDK owns its copy across migration; gameplay packets stay
     * unchanged. Local accepting is the callback cache of that shared policy.*/
    memcpy(policy,open_policy,sizeof(policy));put_word(policy+4,!!enabled);
    d->pvApplicationReservedData=policy;d->dwApplicationReservedDataSize=sizeof(policy);
    hr=IDirectPlay8Peer_SetApplicationDesc(n->play,d,0);free(d);
    if(FAILED(hr)){char diagnostic[128];snprintf(diagnostic,sizeof(diagnostic),"native DP8 admission failed: enabled=%d result=%08lx",!!enabled,(unsigned long)hr);game_application_log(a,"%s\n",diagnostic);return 0;}
    application_policy(n);
    EnterCriticalSection(&n->lock);enabled=!n->failed;LeaveCriticalSection(&n->lock);return enabled;
}
int game_network_decode_system(GameApplication *a,const uint8_t *p,uint32_t size){
    uint32_t type;if(!size)return !a->memory.fault;if(!p || size<16)return 0;type=word(p);
    if(type==3){if(size<33 || word(p+12)!=16 || !memchr(p+32,0,size-32))return 0;
        return game_network_system_player(a,3,word(p+4),p+16,16,(const char *)p+32);}
    if(type==5)return game_network_system_player(a,5,word(p+4),NULL,0,NULL);
    return !a->memory.fault;
}
int game_network_decode_clock_system(const uint8_t *p,uint32_t size,GameNetworkSystemEvent *out){
    if(!p || !out || size<16)return 0;
    memset(out,0,sizeof(*out));
    if(word(p)==5){out->type=5;out->player_type=word(p+8);out->player_id=word(p+4);}
    else if(word(p)==257)out->type=257;
    return 1;
}
