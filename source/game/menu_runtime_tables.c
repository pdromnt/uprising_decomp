#include "menu_runtime_tables.h"
#include "gameplay_state.h"
#include "game_initializers.h"
#include "portable_crt.h"
#include <string.h>
#include <math.h>
float game_menu_runtime_float(GameMemory *m,GameAddress address) {
    uint32_t bits=game_memory_read_u32(m,address);float value;
    memcpy(&value,&bits,4u);return value;
}
void game_menu_runtime_store_float(GameMemory *m,GameAddress address,float value) {
    uint32_t bits;memcpy(&bits,&value,4u);game_memory_write_u32(m,address,bits);
}
/* 00532970: the same global RNG stream is shared with subsequent game logic. */
void game_menu_character_tables(GameMemory *m) {
    unsigned i,row;
    game_initializer_character_alphabet(m);
    for(i=0;i<16u;++i)game_memory_write_u8(m,0x6291f8u+i,
        game_initializer_character_noise_value(portable_crt_rand_step(m),16u,0));
    for(row=0;row<5u;++row) {
        unsigned base=row*3u,upper=base+6u;
        if(upper>16u)upper=16u;
        for(i=0;i<256u;++i)game_memory_write_u8(m,0x629208u+row*256u+i,
            game_initializer_character_noise_value(portable_crt_rand_step(m),upper-base,(uint8_t)base));
    }
}
/* 0050bcd0: cubic interpolation weights. Keep extended intermediates and
 * round each original binary32 store, rather than emulate floating instructions. */
void game_menu_curve_tables(GameMemory *m) {
    unsigned i;long double step=game_menu_runtime_float(m,0x575284u);
    long double one=game_menu_runtime_float(m,0x575288u),three=game_menu_runtime_float(m,0x57528cu);
    for(i=0;i<9u;++i) {
        long double t=(long double)i*step,u=one-t;
        game_menu_runtime_store_float(m,0x6255c0u+i*16u,(float)((u*u)*u));
        game_menu_runtime_store_float(m,0x6255c4u+i*16u,(float)(((t*u)*u)*three));
        game_menu_runtime_store_float(m,0x6255c8u+i*16u,(float)(((t*t)*u)*three));
        game_menu_runtime_store_float(m,0x6255ccu+i*16u,(float)((t*t)*t));
    }
}
/* 004fadd0: eight rotations and the 8x8 view lookup, using original store order. */
void game_menu_view_tables(GameMemory *m) {
    float x=0.0f,z=1.0f,c=game_menu_runtime_float(m,0x629ca8u),s=game_menu_runtime_float(m,0x62a4a8u);
    unsigned i,j;
    for(i=0;i<8u;++i) {
        float next_z=(float)((long double)s*z+(long double)c*x);
        float next_x=(float)((long double)s*x+(long double)(-c)*z);
        game_menu_runtime_store_float(m,0x608268u+i*12u,x);
        game_menu_runtime_store_float(m,0x60826cu+i*12u,z);
        game_memory_write_u32(m,0x608270u+i*12u,0);
        x=next_x;z=next_z;
    }
    for(i=0;i<8u;++i) {
        float y=(float)((long double)i*game_menu_runtime_float(m,0x5750d4u)+game_menu_runtime_float(m,0x5750d8u));
        for(j=0;j<8u;++j) {
            game_menu_runtime_store_float(m,0x6082c8u+(i*8u+j)*8u,
                (float)((long double)j*game_menu_runtime_float(m,0x5750dcu)+game_menu_runtime_float(m,0x5750d8u)));
            game_menu_runtime_store_float(m,0x6082ccu+(i*8u+j)*8u,y);
        }
    }
}
int game_menu_reciprocal_table(GameMemory *m) {
    GameplayNumericTableSpec spec=gameplay_state_numeric_table_spec(1u);unsigned i;
    long double scale=game_menu_runtime_float(m,spec.scalar);
    game_memory_write_u32(m,0x622c50u,0);game_memory_write_u32(m,0x622c54u,0);
    for(i=1u;i<=642u;++i)game_menu_runtime_store_float(m,spec.output+(i-1u)*4u,(float)(scale/(long double)i));
    gameplay_state_fill_u32(m,0x624c58u,161u,UINT32_MAX,0);
    return !m->fault;
}
/* 00560db0's defined numeric conversion, including the indefinite low word.
 * This is arithmetic on C values, not a floating-point execution stack. */
static uint32_t table_integer(long double value) {
    if(!isfinite(value) || value < -9223372036854775808.0L || value >=9223372036854775808.0L)return 0;
    return (uint32_t)(uint64_t)(int64_t)truncl(value);
}
static uint16_t table_angle(GameMemory *m,long double radians) {
    uint32_t low=table_integer(radians*game_menu_runtime_float(m,0x576744u))&65535u;
    int32_t signed_angle=low<32768u?(int32_t)low:(int32_t)low-65536;
    long double adjusted=(long double)signed_angle;
    adjusted+=signed_angle>0?game_menu_runtime_float(m,0x576748u):-game_menu_runtime_float(m,0x576748u);
    return (uint16_t)table_integer(adjusted);
}
/* 00539450: shared trigonometric tables used by actual presentation/game math.
 * Keep the two stored sine anchors and original two-stage angle conversion. */
int game_menu_trig_tables(GameMemory *m) {
    unsigned i;long double step=game_menu_runtime_float(m,0x57672cu);
    for(i=0;i<2048u;++i) {
        long double angle=(long double)i*step;
        game_menu_runtime_store_float(m,0x6298a8u+i*4u,(float)sinl(angle));
        game_menu_runtime_store_float(m,0x62c8b0u+i*4u,(float)tanl(angle));
    }
    game_memory_write_u32(m,0x62a0a8u,0x3f800000u);game_memory_write_u32(m,0x62b0a8u,0xbf800000u);
    game_memory_write_u32(m,0x6298a8u,0);game_memory_write_u32(m,0x62a8a8u,0);
    for(i=0;i<=2048u;++i) {
        long double centered=((long double)i-game_menu_runtime_float(m,0x57673cu))*game_menu_runtime_float(m,0x576740u);
        long double endpoint=(long double)i*game_menu_runtime_float(m,0x57674cu);
        endpoint*=game_menu_runtime_float(m,0x576750u);endpoint+=game_menu_runtime_float(m,0x576754u);
        if(centered < -1.0L || centered > 1.0L || endpoint < -1.0L || endpoint > 1.0L)return 0;
        game_memory_write_u16(m,0x62f8b8u+i*2u,table_angle(m,asinl(centered)));
        game_memory_write_u16(m,0x62b8a8u+i*2u,table_angle(m,asinl(endpoint)));
        game_memory_write_u16(m,0x62e8b0u+i*2u,table_angle(m,atan2l(centered,1.0L)));
    }
    return !m->fault;
}
/* 0045ac20/0045ac50: build first quadrant, mirror it, then negate the half.
 * The original increment is stored as binary64 before extended accumulation. */
int game_menu_scalar_tables(GameMemory *m) {
    uint64_t bits=game_memory_read_u32(m,0x573760u);double quarter_angle;unsigned which,i;
    bits|=(uint64_t)game_memory_read_u32(m,0x573764u)<<32u;
    memcpy(&quarter_angle,&bits,8u);
    for(which=0;which<2u;++which) {
        GameplayScalarTableSpec spec=gameplay_state_scalar_table_spec(which);
        float amplitude;uint32_t quarter=spec.count/4u,half=spec.count/2u;
        long double angle=0;double increment;
        if(spec.count<4u || spec.count%4u || spec.count>65536u)return 0;
        memcpy(&amplitude,&spec.scalar_bits,4u);increment=(double)((long double)quarter_angle/quarter);
        game_memory_write_u16(m,spec.destination,0);
        for(i=1;i<=quarter;++i) {
            angle+=(long double)increment;
            game_memory_write_u16(m,spec.destination+i*2u,(uint16_t)table_integer(sinl(angle)*amplitude));
        }
        for(i=quarter+1u;i<half;++i)game_memory_write_u16(m,spec.destination+i*2u,
            game_memory_read_u16(m,spec.destination+(half-i)*2u));
        for(i=half;i<spec.count;++i)game_memory_write_u16(m,spec.destination+i*2u,
            (uint16_t)(0u-game_memory_read_u16(m,spec.destination+(i-half)*2u)));
    }
    return !m->fault;
}
/* 00457fd0 and the same inlined 0049a560 lookup. Callers supply the original
 * scale constant; table addresses remain stable IDs for bounded C data access. */
int game_menu_pair_heading(GameMemory *m,float x,float z,float scale,uint16_t *out) {
    uint32_t index,bits;uint16_t angle;
    if(!out)return 0;
    if(!(fabsl(x)>=fabsl(z))) {
        index=table_integer((long double)x*scale/z+scale);
        if(index>2048u)return 0;
        angle=game_memory_read_u16(m,0x62e8b0u+index*2u);
        memcpy(&bits,&z,4u);
        angle=bits && bits<0x80000000u?(uint16_t)(angle&2047u):(uint16_t)(angle+1024u);
    } else {
        index=table_integer((long double)z*scale/x+scale);
        if(index>2048u)return 0;
        memcpy(&bits,&x,4u);
        angle=(uint16_t)((bits && bits<0x80000000u?512u:1536u)-
            game_memory_read_u16(m,0x62e8b0u+index*2u));
    }
    *out=angle;return !m->fault;
}
/* 00499fe0/0049a560: 32 heading wedges over the 9x9 neighborhood. Preserve
 * x/z traversal and the first-six/last-three sampling order and radius inputs. */
int game_menu_neighborhood_tables(GameMemory *m) {
    unsigned sector;int x,z,ox,oz;
    float inline_radius=game_menu_runtime_float(m,0x5741d4u);
    float shared_radius=(float)(4.0L+game_menu_runtime_float(m,0x5741dcu));
    float inline_scale=game_menu_runtime_float(m,0x573538u),shared_scale=game_menu_runtime_float(m,0x5741d8u);
    for(sector=0;sector<32u;++sector) {
        uint16_t lower=(uint16_t)((sector*64u-256u)&2047u),upper=(uint16_t)((sector*64u+256u)&2047u);
        unsigned count=0;
        for(x=-4;x<=4;++x)for(z=-4;z<=4;++z) {
            int accepted=0;
            for(ox=-1;ox<=1 && !accepted;++ox)for(oz=-1;oz<=1 && !accepted;++oz) {
                int px=x+ox,pz=z+oz;long double distance=sqrtl((long double)(px*px+pz*pz));uint16_t angle;
                if(ox==1)distance=(float)distance;
                if(distance>(ox==1?shared_radius:inline_radius))continue;
                if(!px && !pz){accepted=1;continue;}
                if(!game_menu_pair_heading(m,(float)px,(float)pz,ox==1?shared_scale:inline_scale,&angle))return 0;
                angle&=2047u;
                accepted=lower<=upper?(angle>=lower && angle<=upper):(angle>=lower || angle<=upper);
            }
            if(accepted) {
                GameAddress at=0x5ff850u+(sector*81u+count)*4u;
                game_memory_write_u16(m,at,(uint16_t)x);game_memory_write_u16(m,at+2u,(uint16_t)z);++count;
            }
        }
        game_memory_write_u16(m,0x5ff810u+sector*2u,(uint16_t)count);
    }
    return !m->fault;
}
