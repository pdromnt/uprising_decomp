#include "gameplay_input.h"
#include "gameplay_messages.h"
#include "gameplay_catalog.h"
#include "level_run.h"
#include "menu_sound.h"
#include <math.h>
#include <string.h>

static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t signed_word(uint32_t v){return v<=INT32_MAX?(int32_t)v:(int32_t)((int64_t)v-4294967296LL);}
static float real(GameApplication *a,GameAddress p){uint32_t word=get(a,p);float result;memcpy(&result,&word,4);return result;}
static uint32_t float_word(long double value){volatile float stored=(float)value;float copy=stored;uint32_t word;memcpy(&word,&copy,4);return word;}
static uint32_t truncate_word(long double value){return isfinite(value) && value>=-9223372036854775808.0L && value<9223372036854775808.0L?(uint32_t)(uint64_t)(int64_t)truncl(value):0x80000000u;}
static uint32_t magnitude_word(uint32_t value){return signed_word(value)>0?value:0u-value;}
static GameAddress root(GameApplication *a){return get(a,0x5f0208);}
static GameAddress controller(GameApplication *a){return get(a,root(a)+0x3bc);}
static void add_angle(GameApplication *a,GameAddress queue,unsigned offset,uint16_t delta){
    game_memory_write_u16(&a->memory,queue+offset,(uint16_t)(game_memory_read_u16(&a->memory,queue+offset)+delta));
}
static int button_state(GameApplication *a,unsigned binding,int previous){
    uint32_t button=get(a,0x604770+binding*4);if(button>=128){a->memory.fault=1;return 0;}
    return game_memory_read_u8(&a->memory,(previous?0x604568:0x604680)+button)!=0;
}
static int pressed(GameApplication *a,unsigned binding){return button_state(a,binding,0) && !button_state(a,binding,1);}
/*The current and previous records are input history, not competing owners.
 *Publish each SDK member explicitly; host structure padding is never copied. */
static int vehicle_joystick(GameApplication *);
static int overhead_joystick(GameApplication *);
static int publish_sample(GameApplication *a,const GameJoystickSample *sample){unsigned i;
    for(i=0;i<8;++i){put(a,0x604650+i*4,(uint32_t)sample->position[i]);put(a,0x604700+i*4,(uint32_t)sample->velocity[i]);
        put(a,0x604720+i*4,(uint32_t)sample->acceleration[i]);put(a,0x604740+i*4,(uint32_t)sample->force[i]);}
    for(i=0;i<4;++i)put(a,0x604670+i*4,sample->pov[i]);
    return game_memory_write(&a->memory,0x604680,sample->buttons,sizeof(sample->buttons)) && !a->memory.fault;
}

static int selector_panels(GameApplication *a,int down,int32_t x,int32_t y){
    GameAddress selected;
    return game_input_panel_pointer(a,get(a,0x60259c),down,x,y,&selected) &&
        (selected || game_input_panel_pointer(a,get(a,0x602678),down,x,y,&selected));
}
/*4d2780 retains its cursor and map/panel consumers. Joystick selection
 *does not use the keyboard modifier policy of mouse selection. */
static int selector_joystick(GameApplication *a){
    unsigned i;uint32_t hat=get(a,0x604670),angle,delta,first,second;int hit,accepted;
    long double x,y,rate=real(a,0x594d68);
    if((get(a,0x604dd0)&16) && (hat&0xffff)!=0xffff){
        angle=truncate_word((long double)hat*real(a,0x5749bc)*real(a,0x5749c4));
        x=real(a,0x6298a8+(angle&2047)*4)*rate;y=real(a,0x6298a8+((angle+512)&2047)*4)*rate;
        put(a,0x5ac9c8,get(a,0x5ac9c8)+truncate_word(x*fabsl(x)*real(a,0x5749c8)));
        put(a,0x5ac9cc,get(a,0x5ac9cc)+truncate_word(-y*fabsl(y)*real(a,0x5749c8)));
    }
    for(i=0;i<2;++i){
        uint32_t axis_value=get(a,0x604650+i*4),value,limit=get(a,i?0x62792c:0x627928)-1;
        GameAddress cursor=i?0x5ac9cc:0x5ac9c8;
        uint32_t strength=float_word((long double)signed_word(axis_value)*rate*real(a,0x5749cc));
        float stored;memcpy(&stored,&strength,4);
        if(axis_value){delta=truncate_word((long double)stored*fabsl(stored)*real(a,0x5749d0));put(a,cursor,get(a,cursor)+delta);}
        value=get(a,cursor);if(signed_word(value)<0)value=0;else if(signed_word(value)>signed_word(limit))value=limit;
        put(a,cursor,value);
    }
    if(pressed(a,0)){
        int32_t px=signed_word(get(a,0x5ac9c8)),py=signed_word(get(a,0x5ac9cc));
        if(!game_input_selector_hit(a,px,py,&first,&second,&hit))return 0;
        if(hit){
            if(get(a,0x6025b4)){if(!game_input_selector_pair(a,first,second))return 0;}
            else if(!game_input_selector_choose(a,px,py,&accepted) || (accepted && !game_menu_sound(a,124)))return 0;
        }else if(!selector_panels(a,1,px,py))return 0;
    }else if(!button_state(a,0,0) && button_state(a,0,1)){
        if(!selector_panels(a,0,signed_word(get(a,0x5ac9c8)),signed_word(get(a,0x5ac9cc))))return 0;
        put(a,0x6027e4,get(a,0x6027e4)|1);
    }
    if(pressed(a,1) && !game_input_panel_action(a,0x4a21d0,3))return 0;
    if(pressed(a,7) && !game_input_panel_action(a,0x4a21d0,3))return 0;
    return !a->memory.fault;
}

static int orientation_joystick(GameApplication *a){
    unsigned i;GameAddress camera=get(a,0x58f890);GameRootEventServices events=game_input_event_services(a);
    for(i=0;i<2;++i){
        int32_t value=signed_word(get(a,0x604650+i*4));uint16_t orientation;
        if(!value)continue;
        if(value<-100)value=-100;else if(value>100)value=100;
        orientation=(uint16_t)(game_memory_read_u16(&a->memory,camera+6+i*2)-value);
        if(i){int32_t pitch=orientation<=INT16_MAX?orientation:(int32_t)orientation-65536;
            if(pitch<-398)orientation=(uint16_t)-398;else if(pitch>227)orientation=227;
        }else orientation&=2047;
        game_memory_write_u16(&a->memory,camera+6+i*2,orientation);
    }
    if(pressed(a,0)){
        uint32_t selected=get(a,0x60464c),count=get(a,0x604c00);uint8_t previous[128];
        if(selected<2){if(!game_root_scene_publish(&a->memory,&events,root(a),(uint8_t)(selected?40:39),game_memory_read_u16(&a->memory,camera+6)))return 0;}
        else if(selected==2 && !game_root_signal_publish(&a->memory,&events,root(a),38))return 0;
        if(count>sizeof(previous)){a->memory.fault=1;return 0;}
        memset(previous,255,count);if(!game_memory_write(&a->memory,0x604568,previous,count))return 0;
    }
    return !a->memory.fault;
}

extern int game_application_options_load(GameApplication *,uint32_t);
static int weight_joystick(GameApplication *a){
    uint32_t hat=get(a,0x604670),angle,word;unsigned i;long double x,y,rate=real(a,0x594d68);
    GameKeyboardServices services=game_input_keyboard_services(a);
    if((get(a,0x604dd0)&16) && (hat&0xffff)!=0xffff){
        angle=truncate_word((long double)hat*real(a,0x5749bc)*real(a,0x5749c4));
        x=real(a,0x6298a8+(angle&2047)*4)*rate;y=real(a,0x6298a8+((angle+512)&2047)*4)*rate;
        if(!game_input_weight_move(a,0,truncate_word(x*fabsl(x)*real(a,0x5749c8))))return 0;
        /*The negated cosine is stored as float before its square. */
        word=float_word(-y);
        {float stored;memcpy(&stored,&word,4);if(!game_input_weight_move(a,1,truncate_word((long double)stored*fabsl(stored)*real(a,0x5749c8))))return 0;}
    }
    for(i=0;i<2;++i)if(get(a,0x604650+i*4)){
        float stored;word=float_word((long double)signed_word(get(a,0x604650+i*4))*rate*real(a,0x5749cc));memcpy(&stored,&word,4);
        if(!game_input_weight_move(a,i,truncate_word((long double)stored*fabsl(stored)*real(a,0x5749d0))))return 0;
    }
    if(button_state(a,0,0) && !game_input_weight_publish(a))return 0;
    if(!button_state(a,1,0) && button_state(a,1,1))
        return services.queue_float_pair(a,controller(a)+0x13c,get(a,0x604114),get(a,0x6040a4)) && game_application_options_load(a,get(a,0x604490));
    return !a->memory.fault;
}

/*4d0f80: native SDK polling, bounded sample history, mode algorithms and
 *lost-device reacquisition. Snapshot precedes GetState even on its failure. */
int game_level_joystick_update(GameApplication *a){
    GameJoystickSample sample;uint8_t previous[272];int32_t status=game_input_device_joystick_poll(a);
    if(status){game_application_log(a,"joystick poll", "native SDK error");(void)game_menu_input_acquire(a,2);return !a->memory.fault;}
    if(!game_memory_read(&a->memory,0x604650,previous,sizeof(previous)) || !game_memory_write(&a->memory,0x604538,previous,sizeof(previous)))return 0;
    status=game_input_device_joystick_read(a,&sample);
    if(status){game_application_log(a,"joystick state", "native SDK error");(void)game_menu_input_acquire(a,2);return !a->memory.fault;}
    if(!publish_sample(a,&sample))return 0;
    if((get(a,0x604dd0)&12)==12 && signed_word(magnitude_word(get(a,0x604668)))>=455 && signed_word(magnitude_word(get(a,0x60466c)))>=227){
        put(a,0x604650,0);put(a,0x604654,0);put(a,0x604658,0);put(a,0x604664,0);
        put(a,0x604668,get(a,0x604d5c));put(a,0x60466c,get(a,0x604d60));
    }
    switch(get(a,0x604760)){
    case 1:return vehicle_joystick(a);
    case 2:return overhead_joystick(a);
    case 3:return orientation_joystick(a);
    case 5:return weight_joystick(a);
    case 7:return selector_joystick(a);
    default:return !a->memory.fault;
    }
}
static int axis(GameApplication *a,GameAddress queue,unsigned channel,unsigned positive,unsigned negative,uint32_t strength){
    uint32_t value=get(a,0x604650+channel*4),deadzone=get(a,0x6044d0+channel*4);
    if(signed_word(value)>signed_word(deadzone))return game_input_direction(&a->memory,queue,positive,1,strength);
    if(signed_word(value)<signed_word(0u-deadzone))return game_input_direction(&a->memory,queue,negative,1,strength^0x80000000u);
    if(signed_word(magnitude_word(get(a,0x604538+channel*4)))>signed_word(deadzone))
        return game_input_direction(&a->memory,queue,positive,0,0) && game_input_direction(&a->memory,queue,negative,0,0);
    return !a->memory.fault;
}
static int choose_collection(GameApplication *a,const GameKeyboardServices *s,uint8_t unit){
    GameAddress chosen=gameplay_player_choose_collection(&a->memory,&s->units,unit);
    if(a->memory.fault)return 0;
    if(unit==5)return signed_word(get(a,root(a)+0x2d4))>=0 || s->feedback(a,123,0x596098);
    if(!chosen)return s->feedback(a,123,0x5960b8);
    return s->feedback(a,121,0) && game_root_command_publish(&a->memory,&s->events,root(a),26,unit,game_memory_read_u8(&a->memory,chosen)) &&
        s->feedback(a,0,gameplay_collection_message(unit));
}
static int fire(GameApplication *a,const GameKeyboardServices *s,unsigned binding,unsigned weapon,uint8_t down,uint8_t up){
    int current=button_state(a,binding,0),previous=button_state(a,binding,1);GameAddress profile;uint32_t period;int32_t status;
    if(current==previous)return !a->memory.fault;
    if(!s->queue_button(a,controller(a)+0x13c,current?down:up))return 0;
    if(!get(a,0x604c30))return !a->memory.fault;
    if(!current){(void)game_input_device_pulse(a,GAME_INPUT_PULSE_STOP,0,0);return !a->memory.fault;}
    profile=get(a,get(a,controller(a)+0x1cc+weapon*4)+0x14);if(!profile || a->memory.fault)return 0;
    period=truncate_word((long double)signed_word(get(a,profile+0x20))*real(a,0x5749d4));
    status=game_input_device_pulse(a,GAME_INPUT_PULSE_CONFIGURE,get(a,profile+0x14)*20,period);
    if(status)game_application_log(a,"joystick periodic parameters", "native SDK error");
    (void)game_input_device_pulse(a,GAME_INPUT_PULSE_START,0,0);return !a->memory.fault;
}
/*4d13b0: vehicle axes/hat/sliders feed existing controller queues; button
 *edges and deployment requests consume the existing input history. */
static int vehicle_joystick(GameApplication *a){
    GameAddress queue=controller(a)+0x13c;GameKeyboardServices s=game_input_keyboard_services(a);uint32_t x,y,hat,flags=get(a,0x604dd0),value;
    unsigned i;long double rate=real(a,0x594d68),scale=real(a,0x5749cc);uint16_t horizontal,vertical;
    x=float_word((long double)signed_word(get(a,0x604650))*rate*scale);y=float_word((long double)signed_word(get(a,0x604654))*rate*scale);
    if(get(a,0x594d30)){if(!axis(a,queue,0,4,5,x) || !axis(a,queue,1,6,7,y))return 0;}
    else if(!axis(a,queue,0,2,3,x) || !axis(a,queue,1,1,0,y))return 0;
    if(flags&16){hat=get(a,0x604670);
        if((hat&0xffff)!=0xffff){value=truncate_word((long double)hat*real(a,0x5749bc)*real(a,0x5749c4));
            x=float_word((long double)real(a,0x6298a8+(value&2047)*4)*rate);
            y=float_word((long double)real(a,0x6298a8+((value+512)&2047)*4)*rate);
            if(signed_word(x)>0){if(!game_input_direction(&a->memory,queue,2,1,x))return 0;}
            else if(x>0x80000000u){if(!game_input_direction(&a->memory,queue,3,1,x^0x80000000u))return 0;}
            else if(!game_input_direction(&a->memory,queue,2,0,0) || !game_input_direction(&a->memory,queue,3,0,0))return 0;
            if(signed_word(y)>0){if(!game_input_direction(&a->memory,queue,0,1,y))return 0;}
            else if(y>0x80000000u){if(!game_input_direction(&a->memory,queue,1,1,y^0x80000000u))return 0;}
            else if(!game_input_direction(&a->memory,queue,0,0,0) || !game_input_direction(&a->memory,queue,1,0,0))return 0;
        }else if((get(a,0x604558)&0xffff)!=0xffff){for(i=0;i<4;++i)if(!game_input_direction(&a->memory,queue,i,0,0))return 0;}
    }
    if((flags&12)==12){horizontal=(uint16_t)(0u-(get(a,0x604668)-get(a,0x604d5c)));vertical=(uint16_t)(get(a,0x60466c)-get(a,0x604d60));
        if(!get(a,0x594d44))vertical=(uint16_t)(0u-vertical);
        if(get(a,controller(a)+0x2f4)&4){add_angle(a,queue,12,horizontal);add_angle(a,queue,14,vertical);}
        else{add_angle(a,queue,14,vertical);add_angle(a,queue,16,horizontal);}
    }
    if(!fire(a,&s,0,0,2,4) || !fire(a,&s,1,1,3,5))return 0;
    {static const unsigned bindings[]={2,4,3,5};static const uint8_t codes[]={10,7,11,8};
        for(i=0;i<4;++i)if(pressed(a,bindings[i]) && !s.queue_button(a,queue,codes[i]))return 0;}
    if(pressed(a,6)){uint16_t coordinates[4];int found;
        if(!game_input_target_coordinates(a,coordinates,&found) || (found && !game_root_coordinates_publish(&a->memory,&s.events,root(a),32,coordinates)))return 0;}
    if(pressed(a,7) && !game_root_signal_publish(&a->memory,&s.events,root(a),49))return 0;
    for(i=0;i<6;++i)if(pressed(a,8+i)){
        uint8_t unit=i==4?10:i==5?5:(uint8_t)((game_memory_read_u8(&a->memory,0x594d5c)?6:1)+i);
        if(!choose_collection(a,&s,unit))return 0;
    }
    return !a->memory.fault;
}

/*4d20e0 uses the active overhead queue. Neutral axes clear every sample;
 *vehicle axes above clear only when crossing back into the dead zone. */
static int overhead_joystick(GameApplication *a){
    GameAddress queue=get(a,root(a)+0x448)+0x264;GameKeyboardServices s=game_input_keyboard_services(a);
    unsigned i;uint16_t horizontal,vertical;int current0,current1,previous0,previous1;
    for(i=0;i<2;++i){
        uint32_t value=get(a,0x604650+i*4),deadzone=get(a,0x6044d0+i*4);
        uint32_t strength=float_word(fabsl((long double)signed_word(value)*real(a,0x594d68)*real(a,0x5749cc)));
        unsigned positive=i?6:8,negative=i?7:9;
        if(signed_word(value)>signed_word(deadzone)){if(!game_input_direction(&a->memory,queue,positive,1,strength))return 0;}
        else if(signed_word(value)<signed_word(0u-deadzone)){if(!game_input_direction(&a->memory,queue,negative,1,strength))return 0;}
        else if(!game_input_direction(&a->memory,queue,positive,0,0) || !game_input_direction(&a->memory,queue,negative,0,0))return 0;
    }
    if((get(a,0x604dd0)&12)==12){
        horizontal=(uint16_t)(0u-(get(a,0x604668)-get(a,0x604d5c)));
        vertical=(uint16_t)(get(a,0x60466c)-get(a,0x604d60));
        if(!get(a,0x594d44))vertical=(uint16_t)(0u-vertical);
        add_angle(a,queue,14,vertical);add_angle(a,queue,16,horizontal);
    }
    current0=button_state(a,0,0);current1=button_state(a,1,0);
    previous0=button_state(a,0,1);previous1=button_state(a,1,1);
    if((current0||current1) && !previous0 && !previous1){if(!s.queue_button(a,queue,21))return 0;}
    else if(!current0 && !current1 && (previous0 || previous1)){if(!s.queue_button(a,queue,22))return 0;}
    if(pressed(a,6)){uint16_t coordinates[4];int found;
        if(!game_input_target_coordinates(a,coordinates,&found) || (found && !game_root_coordinates_publish(&a->memory,&s.events,root(a),32,coordinates)))return 0;}
    if(pressed(a,7) && !game_root_signal_publish(&a->memory,&s.events,root(a),49))return 0;
    for(i=0;i<6;++i)if(pressed(a,8+i)){
        uint8_t unit=i==4?10:i==5?5:(uint8_t)((game_memory_read_u8(&a->memory,0x594d5c)?6:1)+i);
        if(!choose_collection(a,&s,unit))return 0;
    }
    return !a->memory.fault;
}
