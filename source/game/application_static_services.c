#include "application_entry.h"
#include "game_initializers.h"
#include "gameplay_leaves.h"
#include "gameplay_spatial.h"
#include "runtime_utilities.h"
#include "scene_projection_storage.h"
#include "recyclable_records.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static float read_float(GameMemory *m, GameAddress address) {
    uint32_t bits = game_memory_read_u32(m, address);
    float value; memcpy(&value, &bits, sizeof(value)); return value;
}
static void write_float(GameMemory *m, GameAddress address, float value) {
    uint32_t bits; memcpy(&bits, &value, sizeof(bits)); game_memory_write_u32(m, address, bits);
}
static int register_shutdown(GameApplication *app, uint32_t method) {
    if (app->shutdown_count >= sizeof(app->shutdown_methods)/sizeof(app->shutdown_methods[0])) return 0;
    app->shutdown_methods[app->shutdown_count++] = method; return 1;
}

int game_application_static_constructor(GameApplication *app, uint32_t method) {
    GameMemory *m = &app->memory;
    switch (method) {
    case 0x5604e0u: /* Original CRT onexit storage is supplied by the host CRT.
                     Game callbacks use this application's owned shutdown stack. */
        return 1;
    case 0x567ea0u: /* Host CRT owns standard FILE handles; game I/O uses ResourceFile. */
        return 1;
    case 0x56b970u: /* Host CRT installs native exception/TLS handling. No guest SEH. */
        return 1;
    case 0x401000u:
        game_memory_write_u32(m, 0x5a387cu, 0u);
        game_memory_write_u32(m, 0x5a3878u, 0u);
        return !m->fault && register_shutdown(app, 0x401020u);
    case 0x43b320u:
        game_leaf_0048ef80(m, 0x5ad7d0u);
        return !m->fault && register_shutdown(app, 0x43b340u);
    case 0x452590u: case 0x4525c0u: case 0x4525f0u: {
        GameAddress object = method == 0x452590u ? 0x5cee40u :
            method == 0x4525c0u ? 0x5cef90u : 0x5ceee8u;
        game_initializer_0048f320(m, object);
        return !m->fault && register_shutdown(app, method + 0x20u);
    }
    case 0x452890u:
        game_initializer_type_004528_runtime(m);
        return !m->fault && register_shutdown(app, 0x452970u);
    case 0x456ac0u: {
        uint64_t bits = game_memory_read_u32(m, 0x573540u);
        double angle;
        bits |= (uint64_t)game_memory_read_u32(m, 0x573544u) << 32;
        memcpy(&angle, &bits, sizeof(angle));
        write_float(m, 0x5e1208u, (float)(tanl((long double)angle) * read_float(m, 0x573548u)));
        return !m->fault;
    }
    case 0x456ae0u:
        write_float(m, 0x5e1204u, (float)((long double)read_float(m, 0x5e1208u) * read_float(m, 0x573534u)));
        return !m->fault;
    case 0x471d80u:
        game_memory_write_u32(m, 0x5efefcu, 0u);
        return !m->fault && register_shutdown(app, 0x471da0u);
    case 0x499740u:
        return gameplay_spatial_begin(0x5f0800u) == 1 && register_shutdown(app, 0x499760u);
    case 0x4c81e0u:
        runtime_reset_presentation_pair(m);
        return !m->fault && register_shutdown(app, 0x4c8200u);
    case 0x4fd2c0u:
        game_initializer_00453eb0(m, 0x60c4f0u, 0u, 0u, 0u);
        game_initializer_type_004fd2_service(m);
        return !m->fault && register_shutdown(app, 0x4fd360u);
    case 0x532230u:
        write_float(m, 0x629080u, (float)((long double)read_float(m, 0x59d43cu) * read_float(m, 0x575c18u)));
        return !m->fault;
    case 0x540d60u:
        game_leaf_0048ef80(m, 0x631910u);
        return !m->fault && register_shutdown(app, 0x540d80u);
    /* The verified startup table is completely enumerated here and in the
     * paired-array family. An unknown ID is a hard failure, never CPU dispatch. */
    default: return 0;
    }
}

static int release_chain(GameMemory *m, GameAddress head_slot, uint32_t next_offset) {
    unsigned visits = 0;
    GameAddress node = game_memory_read_u32(m, head_slot);
    while (node && !m->fault) {
        GameAddress next = game_memory_read_u32(m, node + next_offset);
        if (++visits > 65536u || !scene_projection_storage_free(node)) return 0;
        game_memory_write_u32(m, head_slot, next); node = next;
    }
    return !m->fault;
}
/* Shared 00453ee0 destructor: retire the intrusive outer-node chain through
 * the existing record owner; retained pooled storage dies with that owner. */
static int drain_container(GameMemory *m,GameAddress object) {
    GameAddress node;unsigned visits=0;
    game_memory_write_u32(m,object,0x573558u);
    while((node=game_memory_read_u32(m,object+4u))!=0) {
        GameAddress next=game_memory_read_u32(m,node+4u);
        if(m->fault || ++visits>65536u || !recyclable_outer_retire(m,node,NULL))return 0;
        game_memory_write_u32(m,object+4u,next);
    }
    return !m->fault;
}
static int destroy_visibility(GameMemory *m) {
    uint32_t count=game_memory_read_u32(m,0x5e106cu);
    game_memory_write_u32(m,0x585370u,0);
    while(count<0x80000000u) {
        if(count>=100u || !recyclable_outer_retire(m,game_memory_read_u32(m,0x5e1074u+count*4u),NULL))return 0;
        --count;game_memory_write_u32(m,0x5e106cu,count);
    }
    game_memory_fill_u32(m,0x5e1074u,100u,0,0);
    game_memory_write_u32(m,0x5e106cu,UINT32_MAX);
    game_memory_write_u32(m,0x5d0848u,0x573550u);
    return drain_container(m,0x5d0848u);
}
static int destroy_presentation_pair(GameMemory *m) {
    GameAddress array=game_memory_read_u32(m,0x604258u);
    uint32_t count=game_memory_read_u32(m,0x60425cu),i;
    if(!array)return !m->fault;
    if(count<0x80000000u) {
        if(count>65536u)return 0;
        for(i=0;i<count;++i) {
            GameAddress allocation=game_memory_read_u32(m,array+i*4u);
            if(m->fault || (allocation && !scene_projection_storage_free(allocation)))return 0;
            game_memory_write_u32(m,array+i*4u,0);
        }
    }
    if(!scene_projection_storage_free(array))return 0;
    game_memory_write_u32(m,0x604258u,0);return !m->fault;
}
int game_application_shutdown_globals(GameApplication *app) {
    GameMemory *m = &app->memory;
    while (app->shutdown_count) {
        uint32_t method = app->shutdown_methods[app->shutdown_count - 1u];
        int ok;
        switch (method) {
        case 0x43b340u: case 0x4525b0u: case 0x4525e0u: case 0x452610u: case 0x540d80u:
            /* Actual 0048f0f0 is RET; the original base has no shutdown work. */
            ok = 1; break;
        case 0x401020u:
            if (!(game_memory_read_u8(m, 0x5a3884u) & 1u)) {
                game_memory_write_u8(m, 0x5a3884u, game_memory_read_u8(m, 0x5a3884u) | 1u);
                ok = release_chain(m, 0x5a3878u, 4u);
            } else ok = !m->fault;
            break;
        case 0x471da0u: ok = release_chain(m, 0x5efefcu, 0x84u); break;
        case 0x499760u: ok = gameplay_spatial_end(); break;
        case 0x452970u: ok=destroy_visibility(m); break;
        case 0x4c8200u: ok=destroy_presentation_pair(m); break;
        case 0x4fd360u:
            game_memory_write_u32(m,0x60c4f0u,0x575210u);
            ok=drain_container(m,0x60c4f0u); break;
        default: ok=0; break;
        }
        if (!ok) {
            char detail[80];
            snprintf(detail,sizeof(detail),"method %08x fault %d",method,m->fault);
            game_application_log(app,"global shutdown failed: %s\n",detail);
            return 0;
        }
        --app->shutdown_count;
    }
    return !m->fault;
}
