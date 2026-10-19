#include "portable_sdl.h"
#include "menu_network.h"
#include <enet/enet.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Reconstructed peers use versioned ENet sessions, not a DirectPlay wire format.
 * One SDL worker owns socket progress under the transport mutex. It copies only
 * bounded byte packets into the game-thread queue; it never touches GameMemory.
 * Full peer links survive host departure; normal traffic follows one host so
 * ordering does not change as mesh connections are established. */
#define NETWORK_QUEUE_LIMIT (8u*1024u*1024u)
#define NETWORK_PACKET_LIMIT (1024u*1024u)
#define NETWORK_SESSION_LIMIT 128u
#define NETWORK_PORT 2302u
#define DISCOVERY_PORT 2303u
#define JOIN_TOKEN 0x554e5032u
#define HEADER_SIZE 32u
#define PEER_SIZE 160u
#define SNAPSHOT_SIZE (144u+4u*PEER_SIZE)
/* Historical result values remain at the core clock boundary only. */
#define RESULT_OK 0u
#define RESULT_ARGUMENT 0x80070057u
#define RESULT_FAILED 0x80004005u
#define RESULT_SMALL 0x8877001eu
#define RESULT_OBJECT 0x88770082u
#define RESULT_PLAYER 0x88770096u
#define RESULT_EMPTY 0x887700beu
#define RESULT_BUSY 0x8877010eu

enum {WIRE_WELCOME=1,WIRE_SNAPSHOT,WIRE_METADATA,WIRE_GAME,WIRE_POLICY,WIRE_LEAVE};
typedef struct NetworkPacket {
    struct NetworkPacket *next;
    uint32_t sender,size;
    uint8_t data[];
} NetworkPacket;
typedef struct NetworkSession {
    struct NetworkSession *next;
    uint8_t guid[16];
    char name[128];
    ENetAddress address;
} NetworkSession;
typedef struct NetworkPeer {
    uint32_t id;
    int ready,published;
    ENetAddress address;
    ENetPeer *link;
    uint8_t metadata[16];
    char name[128];
} NetworkPeer;
struct GameNetworkTransport {
    SDL_Mutex *lock;
    SDL_Thread *worker;
    ENetHost *play;
    ENetPeer *joining;
    ENetSocket discovery;
    NetworkPacket *head,*tail;
    NetworkSession *sessions;
    NetworkPeer peers[4];
    uint8_t application[16],instance[16];
    char name[128];
    uint32_t local,host,next_id,queued,outgoing,session_count;
    int enet_owned,opened,accepting,failed,enumerating,stop,discovery_host;
};
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put_word(uint8_t *p,uint32_t v){unsigned i;for(i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
static NetworkPeer *peer_id(struct GameNetworkTransport *n,uint32_t id){unsigned i;for(i=0;i<4;++i)if(id && n->peers[i].id==id)return &n->peers[i];return NULL;}
static NetworkPeer *peer_link(struct GameNetworkTransport *n,ENetPeer *link){unsigned i;for(i=0;i<4;++i)if(n->peers[i].link==link && n->peers[i].id)return &n->peers[i];return NULL;}
static void sessions_clear(struct GameNetworkTransport *n){NetworkSession *s;while((s=n->sessions)!=NULL){n->sessions=s->next;free(s);}n->session_count=0;}
static void packets_clear(struct GameNetworkTransport *n){NetworkPacket *p;while((p=n->head)!=NULL){n->head=p->next;free(p);}n->tail=NULL;n->queued=0;}
static void queue_locked(struct GameNetworkTransport *n,uint32_t sender,const void *data,uint32_t size){
    NetworkPacket *p;
    if(n->failed)return;
    if(size>NETWORK_PACKET_LIMIT || n->queued>NETWORK_QUEUE_LIMIT-size-sizeof(*p) || !(p=malloc(sizeof(*p)+size))){n->failed=1;return;}
    p->next=NULL;p->sender=sender;p->size=size;if(size)memcpy(p->data,data,size);
    if(n->tail)n->tail->next=p;else n->head=p;n->tail=p;n->queued+=(uint32_t)sizeof(*p)+size;
}
static void system_locked(struct GameNetworkTransport *n,uint32_t type,uint32_t id,const void *data,const char *name){
    uint8_t event[160]={0};uint32_t size=16;
    put_word(event,type);put_word(event+4,id);put_word(event+8,1);
    if(type==3){size_t length=strlen(name)+1;if(length>128){n->failed=1;return;}
        put_word(event+12,16);memcpy(event+16,data,16);memcpy(event+32,name,length);size=32+(uint32_t)length;}
    queue_locked(n,0,event,size);
}
static void publish_peer(struct GameNetworkTransport *n,NetworkPeer *p){
    if(p->id && p->id!=n->local && p->ready && !p->published){system_locked(n,3,p->id,p->metadata,p->name);p->published=1;}
}
static void ENET_CALLBACK sent_packet_free(ENetPacket *packet){
    struct GameNetworkTransport *n=packet->userData;n->outgoing-=(uint32_t)packet->dataLength;
}
static int send_wire(struct GameNetworkTransport *n,ENetPeer *link,uint32_t kind,uint32_t sender,uint32_t to,const void *data,uint32_t size){
    ENetPacket *packet;uint8_t *p;
    if(!link || link->state!=ENET_PEER_STATE_CONNECTED || size>NETWORK_PACKET_LIMIT || n->outgoing>NETWORK_QUEUE_LIMIT-size-HEADER_SIZE)return 0;
    packet=enet_packet_create(NULL,HEADER_SIZE+size,ENET_PACKET_FLAG_RELIABLE);if(!packet)return 0;
    packet->userData=n;packet->freeCallback=sent_packet_free;n->outgoing+=(uint32_t)packet->dataLength;
    p=packet->data;memcpy(p,"UNP2",4);put_word(p+4,kind);put_word(p+8,sender);put_word(p+12,to);memcpy(p+16,n->instance,16);
    if(size)memcpy(p+HEADER_SIZE,data,size);
    if(enet_peer_send(link,0,packet)<0){enet_packet_destroy(packet);return 0;}return 1;
}
static void snapshot(struct GameNetworkTransport *n,ENetPeer *link,uint32_t local,int welcome){
    uint8_t data[SNAPSHOT_SIZE]={0};unsigned i;
    put_word(data,local);put_word(data+4,n->host);put_word(data+8,n->next_id);put_word(data+12,n->accepting);memcpy(data+16,n->name,128);
    for(i=0;i<4;++i){NetworkPeer *p=&n->peers[i];uint8_t *q=data+144+i*PEER_SIZE;
        put_word(q,p->id);memcpy(q+4,&p->address.host,4);put_word(q+8,p->address.port);put_word(q+12,p->ready);
        memcpy(q+16,p->metadata,16);memcpy(q+32,p->name,128);}
    if(!send_wire(n,link,welcome?WIRE_WELCOME:WIRE_SNAPSHOT,n->local,local,data,sizeof(data)))n->failed=1;
}
static void snapshots(struct GameNetworkTransport *n){unsigned i;for(i=0;i<4;++i)if(n->peers[i].id && n->peers[i].id!=n->local)snapshot(n,n->peers[i].link,n->peers[i].id,0);}
static int discovery_open(struct GameNetworkTransport *n,int hosting){
    ENetAddress bind_address={ENET_HOST_ANY,hosting?DISCOVERY_PORT:0};ENetSocket socket;
    if(n->discovery!=ENET_SOCKET_NULL && n->discovery_host==hosting)return 1;
    if(n->discovery!=ENET_SOCKET_NULL)enet_socket_destroy(n->discovery);
    n->discovery=ENET_SOCKET_NULL;
    socket=enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);if(socket==ENET_SOCKET_NULL)return 0;
    if(enet_socket_set_option(socket,ENET_SOCKOPT_NONBLOCK,1)<0 || enet_socket_set_option(socket,ENET_SOCKOPT_BROADCAST,1)<0 || enet_socket_bind(socket,&bind_address)<0){enet_socket_destroy(socket);return 0;}
    n->discovery=socket;n->discovery_host=hosting;return 1;
}
static void peer_departed(struct GameNetworkTransport *n,NetworkPeer *p){
    uint32_t id=p->id,previous=n->host;unsigned i;
    if(p->published)system_locked(n,5,id,NULL,NULL);
    memset(p,0,sizeof(*p));
    if(id==previous){
        n->host=0;for(i=0;i<4;++i)if(n->peers[i].id && (!n->host || n->peers[i].id<n->host))n->host=n->peers[i].id;
        if(n->host==n->local){
            if(!discovery_open(n,1))n->failed=1;
            system_locked(n,257,n->host,NULL,NULL);
        }
    }
    if(n->host==n->local && n->opened)snapshots(n);
}
static int read_snapshot(struct GameNetworkTransport *n,ENetPeer *link,const uint8_t *data,uint32_t size,int welcome){
    NetworkPeer next[4]={0};uint32_t local=welcome?word(data):n->local,host;unsigned i,j;
    if(size!=SNAPSHOT_SIZE || !local || word(data+12)>1 || !memchr(data+16,0,128))return 0;
    host=word(data+4);if(!host || !word(data+8))return 0;
    for(i=0;i<4;++i){const uint8_t *q=data+144+i*PEER_SIZE;NetworkPeer *p=&next[i],*old;
        p->id=word(q);if(!p->id)continue;
        for(j=0;j<i;++j)if(next[j].id==p->id)return 0;
        if(word(q+8)>65535 || word(q+12)>1 || !memchr(q+32,0,128))return 0;
        memcpy(&p->address.host,q+4,4);p->address.port=(enet_uint16)word(q+8);p->ready=(int)word(q+12);
        memcpy(p->metadata,q+16,16);memcpy(p->name,q+32,128);
        old=peer_id(n,p->id);if(old){p->link=old->link;p->published=old->published;
            /* An earlier host snapshot may cross local metadata publication. */
            if(p->id==local && old->ready){p->ready=1;memcpy(p->metadata,old->metadata,16);memcpy(p->name,old->name,128);}}
        if(p->id==host){p->link=link;if(!p->address.host)p->address.host=link->address.host;}
    }
    for(i=0;i<4 && next[i].id!=local;++i){}if(i==4)return 0;
    for(i=0;i<4 && next[i].id!=host;++i){}if(i==4)return 0;
    for(i=0;i<4;++i)if(n->peers[i].id && n->peers[i].id!=local){
        for(j=0;j<4 && next[j].id!=n->peers[i].id;++j){}
        if(j==4){if(n->peers[i].published)system_locked(n,5,n->peers[i].id,NULL,NULL);if(n->peers[i].link)enet_peer_reset(n->peers[i].link);}
    }
    memcpy(n->peers,next,sizeof(next));n->local=local;n->host=host;n->next_id=word(data+8);n->accepting=(int)word(data+12);memcpy(n->name,data+16,128);
    n->opened=1;n->joining=NULL;for(i=0;i<4;++i)publish_peer(n,&n->peers[i]);return 1;
}
static void route_game(struct GameNetworkTransport *n,uint32_t sender,uint32_t to,const uint8_t *data,uint32_t size){
    unsigned i;
    if(!to || to==n->local){if(sender!=n->local)queue_locked(n,sender,data,size);}
    if(n->local==n->host)for(i=0;i<4;++i){NetworkPeer *p=&n->peers[i];
        if(p->id && p->id!=sender && p->id!=n->local && (!to || to==p->id))
            if(!send_wire(n,p->link,WIRE_GAME,sender,to,data,size))n->failed=1;
    }
}
static void received(struct GameNetworkTransport *n,ENetPeer *link,const uint8_t *packet,size_t length){
    uint32_t kind,sender,to,size;const uint8_t *data;NetworkPeer *p=peer_link(n,link);
    if(length<HEADER_SIZE || length>NETWORK_PACKET_LIMIT+HEADER_SIZE || memcmp(packet,"UNP2",4))return;
    kind=word(packet+4);sender=word(packet+8);to=word(packet+12);size=(uint32_t)length-HEADER_SIZE;data=packet+HEADER_SIZE;
    if(kind==WIRE_WELCOME){
        if(link!=n->joining || size!=SNAPSHOT_SIZE)return;
        memcpy(n->instance,packet+16,16);if(!read_snapshot(n,link,data,size,1))n->failed=1;return;
    }
    if(memcmp(packet+16,n->instance,16) || !p)return;
    if(kind==WIRE_SNAPSHOT){
        if(sender==n->host && p->id==n->host && !read_snapshot(n,link,data,size,0))n->failed=1;
    }else if(kind==WIRE_METADATA){
        if(n->local!=n->host || sender!=p->id || size!=144 || !memchr(data+16,0,128))return;
        memcpy(p->metadata,data,16);memcpy(p->name,data+16,128);p->ready=1;publish_peer(n,p);snapshots(n);
    }else if(kind==WIRE_GAME){
        if(!peer_id(n,sender) || (p->id!=sender && p->id!=n->host) || (to && !peer_id(n,to)))return;
        route_game(n,sender,to,data,size);
    }else if(kind==WIRE_POLICY){
        if(p->id==n->host && size==4 && word(data)<=1)n->accepting=(int)word(data);
    }else if(kind==WIRE_LEAVE && p->id==n->host && size==4){
        NetworkPeer *departing=peer_id(n,word(data));if(departing && departing->id!=n->local){if(departing->link)enet_peer_reset(departing->link);peer_departed(n,departing);}
    }
}
static void connected(struct GameNetworkTransport *n,ENetEvent *event){
    NetworkPeer *p;unsigned i;
    enet_peer_timeout(event->peer,8,2000,5000);
    if(event->peer==n->joining || peer_link(n,event->peer))return;
    if(event->data==JOIN_TOKEN && n->opened && n->local==n->host && n->accepting){
        for(i=0;i<4 && n->peers[i].id;++i){}if(i==4 || !n->next_id){enet_peer_disconnect_now(event->peer,0);return;}
        p=&n->peers[i];p->id=n->next_id++;p->address=event->peer->address;p->link=event->peer;
        snapshot(n,event->peer,p->id,1);snapshots(n);return;
    }
    p=peer_id(n,event->data);
    if(p && p->id>n->local && !p->link && p->address.host==event->peer->address.host)p->link=event->peer;
    else enet_peer_disconnect_now(event->peer,0);
}
static void discovery_pump(struct GameNetworkTransport *n){
    uint8_t data[184];ENetBuffer buffer;ENetAddress from;int length;unsigned visits=0;
    if(n->discovery==ENET_SOCKET_NULL)return;
    buffer.data=data;buffer.dataLength=sizeof(data);
    while(visits++<32 && (length=enet_socket_receive(n->discovery,&from,&buffer,1))>0){
        if(length==20 && !memcmp(data,"UNQ2",4) && !memcmp(data+4,n->application,16)){
            if(n->opened && n->host==n->local && n->accepting){
                uint8_t reply[184]={0};ENetBuffer out;out.data=reply;out.dataLength=sizeof(reply);
                memcpy(reply,"UNS2",4);memcpy(reply+4,n->application,16);memcpy(reply+20,n->instance,16);
                put_word(reply+36,n->play->address.port);memcpy(reply+40,n->name,128);put_word(reply+168,n->host);
                (void)enet_socket_send(n->discovery,&from,&out,1);
            }
        }else if(length==184 && n->enumerating && !memcmp(data,"UNS2",4) && !memcmp(data+4,n->application,16) && word(data+36)>0 && word(data+36)<=65535 && memchr(data+40,0,128)){
            NetworkSession *s,**at;for(s=n->sessions;s && memcmp(s->guid,data+20,16);s=s->next){}
            if(!s && n->session_count<NETWORK_SESSION_LIMIT){s=calloc(1,sizeof(*s));if(!s){n->failed=1;return;}
                memcpy(s->guid,data+20,16);memcpy(s->name,data+40,128);s->address.host=from.host;s->address.port=(enet_uint16)word(data+36);
                for(at=&n->sessions;*at && strcmp(s->name,(*at)->name)>=0;at=&(*at)->next){}s->next=*at;*at=s;++n->session_count;
            }
        }
        buffer.dataLength=sizeof(data);
    }
}
static int worker(void *context){
    struct GameNetworkTransport *n=context;ENetEvent event;int stop;
    for(;;){unsigned i,events=0;SDL_LockMutex(n->lock);stop=n->stop;
        if(!stop){
            while(events++<256 && enet_host_service(n->play,&event,0)>0){
                if(event.type==ENET_EVENT_TYPE_CONNECT)connected(n,&event);
                else if(event.type==ENET_EVENT_TYPE_RECEIVE){received(n,event.peer,event.packet->data,event.packet->dataLength);enet_packet_destroy(event.packet);}
                else if(event.type==ENET_EVENT_TYPE_DISCONNECT){NetworkPeer *p=peer_link(n,event.peer);if(p)peer_departed(n,p);else if(event.peer==n->joining){n->joining=NULL;n->failed=1;}}
            }
            if(n->opened)for(i=0;i<4;++i){NetworkPeer *p=&n->peers[i];
                if(p->id && p->id<n->local && p->id!=n->host && !p->link){p->link=enet_host_connect(n->play,&p->address,1,n->local);if(!p->link)n->failed=1;}
            }
            discovery_pump(n);enet_host_flush(n->play);
        }
        SDL_UnlockMutex(n->lock);if(stop)break;SDL_Delay(2);
    }return 0;
}
static int begin(struct GameNetworkTransport *n,int hosting){
    ENetAddress address={ENET_HOST_ANY,hosting?NETWORK_PORT:0};
    if(n->play)return n->worker!=NULL;
    n->play=enet_host_create(&address,8,1,0,0);if(!n->play)return 0;
    n->play->maximumPacketSize=NETWORK_PACKET_LIMIT+HEADER_SIZE;n->play->maximumWaitingData=NETWORK_QUEUE_LIMIT;
    if(!discovery_open(n,hosting))goto failed;
    n->stop=0;n->worker=SDL_CreateThread(worker,"Uprising network",n);if(n->worker)return 1;
failed:
    enet_host_destroy(n->play);n->play=NULL;
    if(n->discovery!=ENET_SOCKET_NULL)enet_socket_destroy(n->discovery);
    n->discovery=ENET_SOCKET_NULL;return 0;
}
int game_network_transport_close(GameApplication *a){
    struct GameNetworkTransport *n=a->network;unsigned i;uint64_t until;
    if(!n)return 0;
    if(n->play){
        SDL_LockMutex(n->lock);n->opened=0;
        /* Release the well-known discovery port before peers elect a successor. */
        if(n->discovery!=ENET_SOCKET_NULL)enet_socket_destroy(n->discovery);
        n->discovery=ENET_SOCKET_NULL;
        for(i=0;i<n->play->peerCount;++i)if(n->play->peers[i].state==ENET_PEER_STATE_CONNECTED)enet_peer_disconnect_later(&n->play->peers[i],0);
        enet_host_flush(n->play);SDL_UnlockMutex(n->lock);
        until=SDL_GetTicks()+300;
        while(n->worker && SDL_GetTicks()<until){int active=0;SDL_LockMutex(n->lock);for(i=0;i<n->play->peerCount;++i)if(n->play->peers[i].state!=ENET_PEER_STATE_DISCONNECTED)active=1;SDL_UnlockMutex(n->lock);if(!active)break;SDL_Delay(2);}
        SDL_LockMutex(n->lock);n->stop=1;SDL_UnlockMutex(n->lock);if(n->worker)SDL_WaitThread(n->worker,NULL);n->worker=NULL;
        /* Do not make survivors wait for a timeout if the graceful drain was
         * bounded by a paused/debugged peer or by a congested link. */
        for(i=0;i<n->play->peerCount;++i)if(n->play->peers[i].state!=ENET_PEER_STATE_DISCONNECTED)enet_peer_disconnect_now(&n->play->peers[i],0);
        enet_host_destroy(n->play);n->play=NULL;
    }
    if(n->discovery!=ENET_SOCKET_NULL)enet_socket_destroy(n->discovery);
    n->discovery=ENET_SOCKET_NULL;
    n->joining=NULL;n->opened=0;n->local=n->host=0;n->failed=0;n->accepting=1;
    memset(n->peers,0,sizeof(n->peers));packets_clear(n);sessions_clear(n);return 1;
}
void game_network_dispose(GameApplication *a){struct GameNetworkTransport *n=a->network;if(!n)return;
    (void)game_network_transport_close(a);SDL_DestroyMutex(n->lock);if(n->enet_owned)enet_deinitialize();free(n);a->network=NULL;
}
static struct GameNetworkTransport *create(GameApplication *a){
    struct GameNetworkTransport *n=calloc(1,sizeof(*n));if(!n)return NULL;
    n->discovery=ENET_SOCKET_NULL;n->accepting=1;n->next_id=2;
    n->lock=SDL_CreateMutex();if(!n->lock || enet_initialize()<0){if(n->lock)SDL_DestroyMutex(n->lock);free(n);return NULL;}
    n->enet_owned=1;if(!game_memory_read(&a->memory,0x573bf8,n->application,16)){enet_deinitialize();SDL_DestroyMutex(n->lock);free(n);return NULL;}return n;
}
int game_network_providers(GameApplication *a,GameAddress selector){uint8_t guid[16];
    return game_memory_read(&a->memory,0x577ce8,guid,16) && game_menu_network_provider_row(a,selector,guid,"LAN / Internet (ENet)");
}
int game_network_provider_open(GameApplication *a,const uint8_t bytes[16]){uint8_t known[16];struct GameNetworkTransport *n;
    if(!bytes || !game_memory_read(&a->memory,0x577ce8,known,16) || memcmp(bytes,known,16) || !(n=create(a)))return 0;
    game_network_dispose(a);a->network=n;return 1;
}
int game_network_session_open(GameApplication *a,int hosting,const char *name,const uint8_t instance[16]){
    struct GameNetworkTransport *n=a->network;int ok=0;uint64_t until;
    if(!n)return 0;
    if(hosting){
        uint64_t seed=SDL_GetPerformanceCounter();unsigned i;if(!name || strlen(name)>=128)return 0;
        if(n->play && !game_network_transport_close(a))return 0;
        if(!begin(n,1))return 0;
        SDL_LockMutex(n->lock);for(i=0;i<4;++i)put_word(n->instance+i*4,SDL_rand_bits_r(&seed));
        strcpy(n->name,name);n->local=n->host=1;n->next_id=2;n->peers[0].id=1;n->peers[0].address=n->play->address;n->opened=1;SDL_UnlockMutex(n->lock);return 1;
    }
    if(!instance || !begin(n,0))return 0;
    SDL_LockMutex(n->lock);{NetworkSession *s;for(s=n->sessions;s && memcmp(s->guid,instance,16);s=s->next){}
        if(s){memcpy(n->instance,instance,16);n->joining=enet_host_connect(n->play,&s->address,1,JOIN_TOKEN);}}
    ok=n->joining!=NULL;SDL_UnlockMutex(n->lock);if(!ok)return 0;
    until=SDL_GetTicks()+5000;do{SDL_LockMutex(n->lock);ok=n->opened;int failed=n->failed;SDL_UnlockMutex(n->lock);if(ok || failed)break;SDL_Delay(2);}while(SDL_GetTicks()<until);
    return ok;
}
int game_network_sessions(GameApplication *a,GameAddress selector,const uint8_t *selected){
    struct GameNetworkTransport *n=a->network;NetworkSession *s;ENetAddress remote={ENET_HOST_BROADCAST,DISCOVERY_PORT};
    const char *endpoint=SDL_getenv("UPRISING_RECOMP_NETWORK_ADDRESS");uint8_t query[20];ENetBuffer buffer;unsigned i;int ok=1;
    if(!n || !begin(n,0) || (endpoint && *endpoint && enet_address_set_host(&remote,endpoint)<0))return 0;
    buffer.data=query;buffer.dataLength=sizeof(query);memcpy(query,"UNQ2",4);memcpy(query+4,n->application,16);
    SDL_LockMutex(n->lock);sessions_clear(n);n->enumerating=1;SDL_UnlockMutex(n->lock);
    for(i=0;i<3;++i){SDL_LockMutex(n->lock);(void)enet_socket_send(n->discovery,&remote,&buffer,1);SDL_UnlockMutex(n->lock);SDL_Delay(250);}
    SDL_LockMutex(n->lock);n->enumerating=0;
    for(s=n->sessions;s && ok;s=s->next)ok=game_menu_network_session_row(a,selector,s->guid,s->name,selected);
    ok=ok && !n->failed;SDL_UnlockMutex(n->lock);return ok && !a->memory.fault;
}
/* DirectPlay's external lobby ABI is retired. Explicit portable launch intent
 * can still host or discover/join through these same real session functions. */
int game_network_lobby_open(GameApplication *a){
    const char *intent=SDL_getenv("UPRISING_NETWORK_LOBBY");uint8_t guid[16];
    if(a->network)return 1;
    if(!intent || strncmp(intent,"host:",5))return 0;
    if(!game_memory_read(&a->memory,0x577ce8,guid,16) || !game_network_provider_open(a,guid))return 0;
    return game_network_session_open(a,1,intent+5,NULL);
}
int game_network_capabilities(GameApplication *a,GameNetworkCapabilities *out){
    struct GameNetworkTransport *n=a->network;NetworkPeer *host;if(!n || !out)return 0;
    SDL_LockMutex(n->lock);memset(out,0,sizeof(*out));out->flags=n->local && n->local==n->host?2:0;
    host=peer_id(n,n->host);if(host && host->link)out->latency=host->link->roundTripTime;SDL_UnlockMutex(n->lock);return 1;
}
int game_network_player_create(GameApplication *a,const char *name,const uint8_t data[16],uint32_t *id){
    struct GameNetworkTransport *n=a->network;NetworkPeer *local,*host;int ok=1;
    if(!n || !name || strlen(name)>=128 || !data || !id)return 0;
    SDL_LockMutex(n->lock);local=peer_id(n,n->local);host=peer_id(n,n->host);
    if(!n->opened || !local || !host)ok=0;
    else{memcpy(local->metadata,data,16);strcpy(local->name,name);local->ready=1;*id=n->local;
        if(n->local==n->host)snapshots(n);
        else{uint8_t metadata[144]={0};memcpy(metadata,data,16);strcpy((char *)metadata+16,name);ok=send_wire(n,host->link,WIRE_METADATA,n->local,n->host,metadata,sizeof(metadata));}}
    SDL_UnlockMutex(n->lock);return ok;
}
uint32_t game_network_receive_result(GameApplication *a,uint32_t *sender,uint32_t *receiver,uint8_t *data,uint32_t *size){
    struct GameNetworkTransport *n=a->network;NetworkPacket *p;uint32_t result=RESULT_OK;
    if(!n || !sender || !receiver || !data || !size)return RESULT_ARGUMENT;
    SDL_LockMutex(n->lock);p=n->head;
    if(n->failed)result=RESULT_FAILED;else if(!p)result=RESULT_EMPTY;
    else{*sender=p->sender;*receiver=n->local;if(*size<p->size)result=RESULT_SMALL;
        else{if(p->size)memcpy(data,p->data,p->size);n->head=p->next;if(!n->head)n->tail=NULL;n->queued-=(uint32_t)sizeof(*p)+p->size;}
        *size=p->size;if(!result)free(p);}
    SDL_UnlockMutex(n->lock);return result;
}
int game_network_receive(GameApplication *a,uint32_t *sender,uint32_t *receiver,uint8_t *data,uint32_t *size){uint32_t result=game_network_receive_result(a,sender,receiver,data,size);return result==RESULT_EMPTY?0:result==RESULT_OK?1:-1;}
uint32_t game_network_milliseconds(GameApplication *a){(void)a;return (uint32_t)SDL_GetTicks();}
uint32_t game_network_send_result(GameApplication *a,uint32_t from,uint32_t to,const uint8_t *data,uint32_t size){
    struct GameNetworkTransport *n=a->network;NetworkPeer *host;uint32_t result=RESULT_OK;
    if(!n || (!data && size) || size>NETWORK_PACKET_LIMIT)return RESULT_ARGUMENT;
    SDL_LockMutex(n->lock);host=peer_id(n,n->host);
    if(!n->opened || n->failed)result=RESULT_OBJECT;
    else if(!from || from!=n->local || (to && !peer_id(n,to)))result=RESULT_PLAYER;
    else if(n->local==n->host){route_game(n,from,to,data,size);if(n->failed)result=RESULT_FAILED;}
    else if(!host || !send_wire(n,host->link,WIRE_GAME,from,to,data,size))result=RESULT_BUSY;
    SDL_UnlockMutex(n->lock);return result;
}
int game_network_send(GameApplication *a,uint32_t from,uint32_t to,const uint8_t *data,uint32_t size){return game_network_send_result(a,from,to,data,size)==RESULT_OK;}
int game_network_player_destroy(GameApplication *a,uint32_t id){struct GameNetworkTransport *n=a->network;NetworkPeer *p;unsigned i;
    if(!n)return 0;
    SDL_LockMutex(n->lock);
    if(id==n->local){SDL_UnlockMutex(n->lock);return game_network_transport_close(a);}
    p=peer_id(n,id);
    if(p && n->local==n->host){uint8_t data[4];put_word(data,id);for(i=0;i<4;++i)if(n->peers[i].id && n->peers[i].id!=n->local && n->peers[i].id!=id)(void)send_wire(n,n->peers[i].link,WIRE_LEAVE,n->local,0,data,4);
        if(p->link)enet_peer_disconnect_now(p->link,0);
        peer_departed(n,p);}
    SDL_UnlockMutex(n->lock);return 1;
}
int game_network_enable_players(GameApplication *a,int enabled){struct GameNetworkTransport *n=a->network;
    if(!n)return 0;
    SDL_LockMutex(n->lock);if(n->local!=n->host){SDL_UnlockMutex(n->lock);return 0;}
    n->accepting=!!enabled;snapshots(n);int ok=!n->failed;SDL_UnlockMutex(n->lock);return ok;
}
int game_network_decode_system(GameApplication *a,const uint8_t *p,uint32_t size){
    uint32_t type;if(!size)return !a->memory.fault;if(!p || size<16)return 0;type=word(p);
    if(type==3){if(size<33 || word(p+12)!=16 || !memchr(p+32,0,size-32))return 0;return game_network_system_player(a,3,word(p+4),p+16,16,(const char *)p+32);}
    if(type==5)return game_network_system_player(a,5,word(p+4),NULL,0,NULL);
    return !a->memory.fault;
}
int game_network_decode_clock_system(const uint8_t *p,uint32_t size,GameNetworkSystemEvent *out){
    if(!p || !out || size<16)return 0;
    memset(out,0,sizeof(*out));
    if(word(p)==5){out->type=5;out->player_type=word(p+8);out->player_id=word(p+4);}else if(word(p)==257)out->type=257;return 1;
}
