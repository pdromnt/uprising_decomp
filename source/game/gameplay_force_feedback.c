#include "gameplay_input.h"
#include "menu_sound.h"
#include <math.h>
#include <string.h>

static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t signed_word(uint32_t v){return v<=INT32_MAX?(int32_t)v:(int32_t)((int64_t)v-4294967296LL);}
static uint32_t magnitude(uint32_t v){return signed_word(v)>0?v:0u-v;}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static uint32_t float_word(long double value){volatile float stored=(float)value;float copy=stored;uint32_t bits;memcpy(&bits,&copy,4);return bits;}
static uint32_t truncate_word(long double value){return isfinite(value) && value>=-9223372036854775808.0L && value<9223372036854775808.0L?(uint32_t)(uint64_t)(int64_t)truncl(value):0x80000000u;}
static GameAddress root(GameApplication *a){return get(a,0x5f0208);}
static GameAddress queue(GameApplication *a){return get(a,root(a)+0x3bc)+0x13c;}
static void add_angle(GameApplication *a,GameAddress q,unsigned offset,uint16_t delta){
    game_memory_write_u16(&a->memory,q+offset,(uint16_t)(game_memory_read_u16(&a->memory,q+offset)+delta));
}
static long double shaping(GameApplication *a,uint32_t delta){
    long double value=(long double)signed_word(magnitude(delta))*real(a,0x5749bc),limit=real(a,0x5749b8);
    return value<limit?value:limit;
}
static int direction_axis(GameApplication *a,GameAddress q,uint32_t delta,unsigned positive,unsigned negative,uint32_t strength){
    if(signed_word(delta)>5)return game_input_direction(&a->memory,q,positive,1,strength);
    if(signed_word(0u-delta)>5)return game_input_direction(&a->memory,q,negative,1,strength);
    return game_input_direction(&a->memory,q,positive,0,0) && game_input_direction(&a->memory,q,negative,0,0);
}
/*4d0940: position axes and button-mask transitions feed the real vehicle queue. */
static int vehicle(GameApplication *a,uint32_t x,uint32_t y,uint32_t buttons,uint32_t previous){
    static const uint32_t masks[]={0x2200,0x8800,0x1100,0x4400};
    static const unsigned directions[]={2,3,0,1};unsigned i;GameAddress q=queue(a);
    if(!direction_axis(a,q,y,6,7,float_word(shaping(a,y)*real(a,0x5749c0))) ||
       !direction_axis(a,q,x,4,5,float_word(shaping(a,x)*real(a,0x5749c0))))return 0;
    for(i=0;i<4;++i){int current=!!(buttons&masks[i]),old=!!(previous&masks[i]);
        if(current!=old && !game_input_direction(&a->memory,q,directions[i],current,current?0x3f800000:0))return 0;}
    for(i=0;i<2;++i){uint32_t mask=i?6:0x21;int current=!!(buttons&mask),old=!!(previous&mask);
        if(current!=old && !game_input_queue_button(a,q,(uint8_t)(i?(current?3:5):(current?2:4))))return 0;}
    if((buttons&8) && !(previous&8) && !game_input_queue_button(a,q,10))return 0;
    return !((buttons&16) && !(previous&16)) || game_input_queue_button(a,q,7);
}
static int panels(GameApplication *a,int down,int32_t x,int32_t y){GameAddress selected;
    return game_input_panel_pointer(a,get(a,0x60259c),down,x,y,&selected) &&
        (selected || game_input_panel_pointer(a,get(a,0x602678),down,x,y,&selected));
}
/*4d0d40: nonlinear position input retains actual selector map and panel consumers. */
static int selector(GameApplication *a,uint32_t x,uint32_t y,uint32_t buttons,uint32_t previous){
    unsigned i;uint32_t first,second;int hit,accepted;
    for(i=0;i<2;++i){uint32_t delta=i?y:x;GameAddress cursor=i?0x5ac9cc:0x5ac9c8;
        if(signed_word(magnitude(delta))>5){uint32_t value=get(a,cursor)+truncate_word((long double)signed_word(delta)*((float)(shaping(a,delta))));
            uint32_t limit=get(a,i?0x62792c:0x627928);
            if(signed_word(value)<0)value=0;else if(signed_word(value)>=signed_word(limit))value=limit-1;put(a,cursor,value);}
    }
    if((buttons&1) && !(previous&1)){
        int32_t px=signed_word(get(a,0x5ac9c8)),py=signed_word(get(a,0x5ac9cc));
        if(!game_input_selector_hit(a,px,py,&first,&second,&hit))return 0;
        if(hit){if(buttons&2){if(!game_input_selector_pair(a,first,second))return 0;}
            else if(!game_input_selector_choose(a,px,py,&accepted) || (accepted && !game_menu_sound(a,124)))return 0;
        }else if(!panels(a,1,px,py))return 0;
    }else if(!(buttons&1) && (previous&1)){
        if(!panels(a,0,signed_word(get(a,0x5ac9c8)),signed_word(get(a,0x5ac9cc))))return 0;
        put(a,0x6027e4,get(a,0x6027e4)|1);
    }
    return !((buttons&4) && !(previous&4)) || game_input_panel_action(a,0x4a21d0,3);
}
static int orientation(GameApplication *a,uint32_t x,uint32_t y,uint32_t buttons){
    unsigned i;GameAddress camera=get(a,0x58f890);GameRootEventServices events=game_input_event_services(a);
    for(i=0;i<2;++i){uint32_t delta=i?y:x;int32_t value=signed_word(delta);uint16_t angle;
        if(signed_word(magnitude(delta))<=5)continue;
        if(value<-100)value=-100;else if(value>100)value=100;
        angle=(uint16_t)(game_memory_read_u16(&a->memory,camera+6+i*2)-value);
        if(i){int32_t pitch=angle<=INT16_MAX?angle:(int32_t)angle-65536;
            if(pitch<-398)angle=(uint16_t)-398;else if(pitch>227)angle=227;
        }else angle&=2047;
        game_memory_write_u16(&a->memory,camera+6+i*2,angle);
    }
    if(buttons){uint32_t selected=get(a,0x60464c);
        if(selected<2){if(!game_root_scene_publish(&a->memory,&events,root(a),(uint8_t)(selected?40:39),game_memory_read_u16(&a->memory,camera+6)))return 0;}
        else if(selected==2 && !game_root_signal_publish(&a->memory,&events,root(a),38))return 0;
        put(a,0x604764,0);
    }
    return !a->memory.fault;
}
/*4d05e0: each supported route samples once into the existing input record. */
static int input(GameApplication *a){
    uint32_t mode=get(a,0x604760),sample[4],previous=get(a,0x624f64),x,y;unsigned i;GameAddress q;
    if(mode!=1 && mode!=2 && mode!=3 && mode!=5 && mode!=7)return !a->memory.fault;
    put(a,0x604764,previous);
    if(!game_application_force_feedback_poll(a,sample))return !a->memory.fault;
    for(i=0;i<4;++i)put(a,0x624f58+i*4,sample[i]);
    x=sample[0]-get(a,0x624f80);y=sample[1]-get(a,0x624f84);
    if(mode==1)return vehicle(a,x,y,sample[3],previous);
    if(mode==3)return orientation(a,x,y,sample[3]);
    if(mode==7)return selector(a,x,y,sample[3],previous);
    if(mode==5){
        if(signed_word(magnitude(y))>5 && !game_input_weight_move(a,1,y))return 0;
        if(signed_word(magnitude(x))>5 && !game_input_weight_move(a,0,x))return 0;
        return !(sample[3]&1) || game_input_weight_publish(a);
    }
    q=get(a,root(a)+0x448)+0x264;
    if(signed_word(magnitude(y))>5){float stored=(float)shaping(a,y);
        add_angle(a,q,14,(uint16_t)(0u-truncate_word((long double)signed_word(y)*stored)));}
    if(signed_word(magnitude(x))>5){float stored=(float)shaping(a,x);
        add_angle(a,q,16,(uint16_t)(0u-truncate_word((long double)signed_word(x)*stored)));}
    if((sample[3]&0x27) && !(previous&0x27))return game_input_queue_button(a,q,21);
    if(!(sample[3]&0x27) && (previous&0x27))return game_input_queue_button(a,q,22);
    return !a->memory.fault;
}

typedef struct EffectRoute {
    GameAddress desired,active;GameForceFeedbackOperation begin,end;
    GameAddress parameters[5];
} EffectRoute;
/*4fa190: desired/running flags retain their authoritative records. These
 *routes select native SDK functions, with ordinary parameters and returns. */
static int effects(GameApplication *a){
    static const EffectRoute routes[]={
        {0x608258,0x6081e4,GAME_FORCE_REFLEX_JOLT,GAME_FORCE_REFLEX_CLEAR,{0x6081e0,0x60814c,0x608180,0x608168,0x608178}},
        {0x60825c,0x60820c,GAME_FORCE_X_VIBRATION,GAME_FORCE_X_CLEAR,{0x608160,0x608218,0x6081b0,0,0}},
        {0x6081cc,0x608224,GAME_FORCE_Y_VIBRATION,GAME_FORCE_Y_CLEAR,{0x608154,0x60817c,0x608244,0,0}},
        {0x608248,0x60821c,GAME_FORCE_BUFFETING,GAME_FORCE_BUFFETING_CLEAR,{0x6081ac,0,0,0,0}},
        {0x6081a8,0x60823c,GAME_FORCE_VECTOR,GAME_FORCE_VECTOR_CLEAR,{0x608254,0x6081fc,0,0,0}},
        {0x60818c,0x60816c,GAME_FORCE_X_SPRING,GAME_FORCE_X_SPRING_CLEAR,{0x608204,0x608188,0x6081f8,0,0}},
        {0x608238,0x6081b8,GAME_FORCE_Y_SPRING,GAME_FORCE_Y_SPRING_CLEAR,{0x6081dc,0x608208,0x608150,0,0}}
    };
    unsigned i,j;
    if(!get(a,0x608194) && get(a,0x6081c0))
        put(a,0x608194,game_application_force_feedback_effect(a,GAME_FORCE_JOLT,get(a,0x608220),get(a,0x608214),get(a,0x608210),0,0));
    for(i=0;i<sizeof(routes)/sizeof(routes[0]);++i){
        const EffectRoute *route=&routes[i];uint32_t desired=get(a,route->desired),active=get(a,route->active),parameters[5]={0};
        if(desired && !active){
            GameForceFeedbackOperation operation=route->begin;
            for(j=0;j<5;++j)if(route->parameters[j])parameters[j]=get(a,route->parameters[j]);
            if(operation==GAME_FORCE_VECTOR && get(a,0x608250)!=UINT32_MAX){operation=GAME_FORCE_XY_VECTOR;parameters[1]=get(a,0x608250);}
            active=game_application_force_feedback_effect(a,operation,parameters[0],parameters[1],parameters[2],parameters[3],parameters[4]);
            put(a,route->active,active);
        }
        if(!desired && active){
            uint32_t result=game_application_force_feedback_effect(a,route->end,route->end==GAME_FORCE_REFLEX_CLEAR?get(a,0x6081e0):0,0,0,0,0);
            put(a,route->active,result==0);
        }
    }
    return !a->memory.fault;
}
/*417590 calls input then effect publication even when input had no new sample. */
int game_level_force_feedback_update(GameApplication *a){return input(a) && effects(a);}
