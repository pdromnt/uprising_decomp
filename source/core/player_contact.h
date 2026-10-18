#ifndef UPRISING_PLAYER_CONTACT_H
#define UPRISING_PLAYER_CONTACT_H
#include "player_rig.h"

typedef enum PlayerContactMessage {
    CONTACT_CREDITS,CONTACT_HEALTH,CONTACT_SCORE,CONTACT_REFILL,
    CONTACT_DEFAULT_AMMO,CONTACT_AMMO,CONTACT_REWARD5,CONTACT_REWARD6,
    CONTACT_REWARD7,CONTACT_REWARD8,CONTACT_UNKNOWN_PICKUP
} PlayerContactMessage;
typedef struct PlayerContactEvent {
    GameAddress entity,owner;
    uint32_t flags,kind,effect_type;
    uint8_t type;
    int32_t amount;
    int relocate;
    float destination[2],velocity[3],position[3];
} PlayerContactEvent;
typedef struct PlayerContactEnvironment {
    int local,dead,health_cheat,damage_immune,controlled,primary;
    GameAddress owner;
    unsigned owner_index;
    int16_t impulse;
    float knockback_scale,thresholds[3];
} PlayerContactEnvironment;
/* External families: world resources, inventory descriptors, effects/audio,
 * catalog data/concrete object methods and relocation. Response arithmetic is C. */
typedef struct PlayerContactServices {
    void *context;
    int (*accept)(void *,const PlayerContactEvent *,int *);
    int (*damage)(void *,const PlayerContactEvent *,int32_t *);
    int (*maximum_health)(void *,int owner,int32_t *);
    int (*hit_timer)(void *);
    int (*sound)(void *,unsigned);
    int (*effect)(void *,const PlayerContactEvent *,int32_t);
    int (*remove_player)(void *,const PlayerContactEvent *);
    int (*acknowledge)(void *);
    int (*reward)(void *,unsigned kind,uint32_t amount);
    int (*inventory)(void *,unsigned slot,GameAddress *,int32_t *count,int32_t *maximum);
    int (*set_inventory)(void *,GameAddress,int32_t);
    int (*random)(void *,unsigned scale,int32_t *);
    int (*ammo_scale)(void *,unsigned slot,uint32_t *);
    int (*message)(void *,PlayerContactMessage,int32_t,unsigned slot);
    int (*relocate)(void *,const float destination[2]);
    int (*publish_impulse)(void *,int16_t);
} PlayerContactServices;
int player_apply_contact_damage(PresentationPose *,PlayerControls *,uint32_t,
    const PlayerContactEnvironment *,const PlayerContactServices *);
int player_handle_contact(PresentationPose *,PlayerControls *,PlayerRig *,
    PlayerContactEvent *,PlayerContactEnvironment *,const PlayerContactServices *);
int32_t player_contact_scaled_damage(int32_t base,long double coefficient);
int32_t player_contact_resisted_damage(int32_t damage,int32_t armor,uint8_t source_type,uint32_t source_flags);
typedef struct GameContactDamageServices {
    void *context;
    int (*response)(void *,GameAddress,GameAddress method,uint32_t *);
    int (*catalog_record)(void *,GameAddress,uint8_t,GameAddress *);
    int (*impact)(void *,GameAddress,GameAddress source,GameAddress method,uint32_t *);
} GameContactDamageServices;
/*42dd70 with53fcf0/42ff70/42ffb0/54b3e0 and42ec70 collision arithmetic. */
int game_contact_damage(GameMemory *,GameAddress,GameAddress,const GameContactDamageServices *,int32_t *);
#endif
