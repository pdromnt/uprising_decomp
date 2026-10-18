#include "recyclable_records.h"
#define WIN32_LEAN_AND_MEAN
#include "portable_hooks.h"
#include "portable_input_devices.h"
#include <windows.h>
#include "game_initial_data.h"
#include "application_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <io.h>
#include "application_entry.h"
#include "application_archives.h"
#include "menu_widgets.h"
#include "menu_movie.h"
#include "menu_settings.h"
#include "sound_platform.h"
#include "scene_projection_storage.h"
#include "presentation_instance.h"
#include "runtime_node_pool.h"
#include "application_name_registry.h"
#include "level_entity_store.h"
#include "gameplay_spatial.h"
#include "projection_owner_pages.h"
#include "render_arena.h"
#include "render_runtime_pools.h"
#include "glide_texture_cache.h"
#include "level_load.h"
#include "screen_primitives.h"
#include "ui_tiled_image.h"
#include "radar_compositor.h"
#include "model_assets.h"
#include "scene_surface.h"
#include "level_world_surface.h"
#include "level_world_render.h"
#include "level_scene_render.h"
#include "game_text_rasterizer.h"
#include "ui_geometry.h"
extern void *game_menu_window_handle(GameApplication *);

typedef struct NativeGlideTextureInfo {
    uint32_t small_lod,large_lod,aspect,format;
    void *pixels;
} NativeGlideTextureInfo;

typedef struct ArchiveFile {ResourceFile *file;GameAddress identity;struct ArchiveFile *next;} ArchiveFile;
/*Native SDK layout stays private here; pixel pointers are never legacy words. */
typedef struct NativeGlideLfbInfo {uint32_t size;void *pixels;uint32_t stride,write_mode,origin;} NativeGlideLfbInfo;
typedef int (__cdecl *StickBufferCall)(void *);
typedef int (__cdecl *StickQueryCall)(void);
typedef void (__cdecl *StickCommandCall)(uint32_t);
typedef void (__cdecl *StickCloseCall)(void);
typedef struct ForceFeedback {
    HMODULE library;
    FARPROC exports[29];
    StickBufferCall initialize,position;
    StickQueryCall enable;
    StickCommandCall port,report;
    StickCloseCall close;
    int initialized;
} ForceFeedback;
typedef struct PortablePlatform {
    int isolated_input;
    HINSTANCE instance;
    char *command;
    FILE *log;
    HHOOK message_hook, keyboard_hook;
    char *user_directory;
    GameConfig *configuration;
    char configuration_path[4128];
    int configuration_pending;
    int configuration_ready;
    HMODULE glide;
    void (WINAPI *glide_shutdown)(void);
    int32_t (WINAPI *window_open)(uintptr_t,uint32_t,uint32_t,uint32_t,uint32_t,int32_t,int32_t);
    void (WINAPI *window_close)(void);
    void (WINAPI *fog_generate)(uint8_t *,float);
    void (WINAPI *fog_color)(uint32_t), (WINAPI *fog_table)(const uint8_t *);
    void (WINAPI *texture_lod_bias)(uint32_t,float);
    void (WINAPI *texture_ncc_table)(uint32_t,uint32_t);
    void (WINAPI *chroma_mode)(uint32_t), (WINAPI *chroma_value)(uint32_t);
    void (WINAPI *alpha_reference)(uint32_t);
    int window_opened;
    void (WINAPI *buffer_clear)(uint32_t,uint32_t,uint32_t);
    void (WINAPI *buffer_swap)(int32_t);
    int32_t (WINAPI *lfb_lock)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,NativeGlideLfbInfo *);
    int32_t (WINAPI *lfb_unlock)(uint32_t,uint32_t);
    void (WINAPI *lfb_depth)(uint32_t);
    int text_glide_locked;
    int capture_attempted;
    int screen_mask_active;
    uint32_t (WINAPI *texture_min)(uint32_t), (WINAPI *texture_max)(uint32_t);
    void (WINAPI *texture_upload)(uint32_t,uint32_t,uint32_t,const NativeGlideTextureInfo *);
    uint32_t (WINAPI *texture_bytes)(uint32_t,const NativeGlideTextureInfo *);
    void (WINAPI *texture_statistics)(uint32_t *);
    void (WINAPI *texture_reset_statistics)(void);
    void (WINAPI *draw_line)(const GameRenderVertex *,const GameRenderVertex *);
    void (WINAPI *draw_triangle)(const GameRenderVertex *,const GameRenderVertex *,const GameRenderVertex *);
    void (WINAPI *draw_point)(const GameRenderVertex *);
    void (WINAPI *alpha_test)(uint32_t), (WINAPI *constant_color)(uint32_t);
    void (WINAPI *depth_bias)(int32_t);
    void (WINAPI *texture_filter)(uint32_t,uint32_t,uint32_t);
    void (WINAPI *palette_upload)(uint32_t,uint32_t,const uint32_t *);
    void (WINAPI *palette_range)(uint32_t,uint32_t,const uint32_t *,uint32_t,uint32_t);
    void (WINAPI *texture_source)(uint32_t,uint32_t,uint32_t,const NativeGlideTextureInfo *);
    void (WINAPI *texture_combine)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
    void (WINAPI *texture_clamp)(uint32_t,uint32_t,uint32_t);
    void (WINAPI *alpha_combine)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
    void (WINAPI *fog_mode)(uint32_t), (WINAPI *cull_mode)(uint32_t);
    void (WINAPI *dither)(uint32_t);
    void (WINAPI *depth_mode)(uint32_t);
    void (WINAPI *depth_function)(uint32_t);
    void (WINAPI *depth_mask)(uint32_t);
    void (WINAPI *alpha_blend)(uint32_t,uint32_t,uint32_t,uint32_t);
    void (WINAPI *color_combine)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
    int glide_initialized;
    ArchiveFile *archives;
    uint32_t archive_sequence;
    ForceFeedback *force_feedback;
    void *graphics_owner;
} PortablePlatform;

#ifdef UPRISING_OPENGL1
#include "portable_gl1.inc"
#endif

/*4f9bc0/4f9ee0: the native platform owns the entire SDK binding. Legacy
 * function-table words never contain DLL handles or native function pointers. */
int game_application_force_feedback_open(GameApplication *app) {
    static const char *names[]={"_InitStick","_GetStickStatus","_Echo","_IForceAuthenticate",
        "_GetStickError","_EnableForces","_DisableForces","_ClearForces","_CloseStick",
        "_SetJoystickPort","_Jolt","_ButtonReflexJolt","_ButtonReflexClear","_XVibration",
        "_XVibrationClear","_YVibration","_YVibrationClear","_Buffeting","_BuffetingClear",
        "_VectorForce","_VectorForceClear","_XYVectorForce","_EnablePositionReporting",
        "_DisablePositionReporting","_GetStickPosition","_XSpring","_XSpringClear","_YSpring","_YSpringClear"};
    PortablePlatform *p=app->platform;ForceFeedback *f;unsigned i;
    if(!p)return 0;if(p->force_feedback)return 1;
    f=calloc(1,sizeof(*f));if(!f)return 0;
    f->library=LoadLibraryA("iforce.dll");if(!f->library)goto failed;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        f->exports[i]=GetProcAddress(f->library,names[i]);if(!f->exports[i])goto failed;
    }
    /*These are native SDK C function signatures recovered at actual callers. */
    f->initialize=(StickBufferCall)(void (*)(void))f->exports[0];
    f->position=(StickBufferCall)(void (*)(void))f->exports[24];
    f->enable=(StickQueryCall)(void (*)(void))f->exports[5];
    f->port=(StickCommandCall)(void (*)(void))f->exports[9];
    f->report=(StickCommandCall)(void (*)(void))f->exports[22];
    f->close=(StickCloseCall)(void (*)(void))f->exports[8];
    p->force_feedback=f;return 1;
failed:
    if(f->library)FreeLibrary(f->library);free(f);return 0;
}
/*4fa450: try each real joystick port, enable forces, then position reporting.
 * SDK replies are fixed-width byte buffers, not host-layout game structures. */
int game_application_force_feedback_detect(GameApplication *app) {
    PortablePlatform *p=app->platform;ForceFeedback *f=p?p->force_feedback:NULL;uint32_t port;
    union {uint32_t words[15];long double alignment;} information;
    if(!f)return 0;
    for(port=1;port<=4;++port) {
        memset(&information,0,sizeof(information));f->port(port);
        if(f->initialize(information.words)) {f->initialized=1;break;}
        Sleep(100);f->close();
    }
    if(!f->initialized)return 0;
    while(!f->enable())Sleep(50);
    Sleep(200);f->report(10);return 1;
}
int game_application_force_feedback_poll(GameApplication *app,uint32_t sample[4]) {
    PortablePlatform *p=app->platform;ForceFeedback *f=p?p->force_feedback:NULL;uint32_t reply[4]={0,0,0,0};
    if(!f || !f->initialized || !sample || !f->position(reply))return 0;
    memcpy(sample,reply,sizeof(reply));return 1;
}
/*Recovered SDK signatures. The native binding owns full-width function
 *pointers; game records contain only effect parameters and success flags. */
uint32_t game_application_force_feedback_effect(GameApplication *app,GameForceFeedbackOperation operation,
    uint32_t first,uint32_t second,uint32_t third,uint32_t fourth,uint32_t fifth){
    typedef uint32_t (__cdecl *Call0)(void);
    typedef uint32_t (__cdecl *Call1)(uint32_t);
    typedef uint32_t (__cdecl *Call2)(uint32_t,uint32_t);
    typedef uint32_t (__cdecl *Call3)(uint32_t,uint32_t,uint32_t);
    typedef uint32_t (__cdecl *Call5)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
    PortablePlatform *p=app->platform;ForceFeedback *f=p?p->force_feedback:NULL;FARPROC function;
    if(!f || !f->initialized || (unsigned)operation>=29)return 0;
    function=f->exports[operation];if(!function)return 0;
    switch(operation){
    case GAME_FORCE_JOLT:case GAME_FORCE_X_VIBRATION:case GAME_FORCE_Y_VIBRATION:
    case GAME_FORCE_X_SPRING:case GAME_FORCE_Y_SPRING:
        return ((Call3)(void (*)(void))function)(first,second,third);
    case GAME_FORCE_REFLEX_JOLT:return ((Call5)(void (*)(void))function)(first,second,third,fourth,fifth);
    case GAME_FORCE_REFLEX_CLEAR:case GAME_FORCE_BUFFETING:return ((Call1)(void (*)(void))function)(first);
    case GAME_FORCE_VECTOR:case GAME_FORCE_XY_VECTOR:return ((Call2)(void (*)(void))function)(first,second);
    case GAME_FORCE_X_CLEAR:case GAME_FORCE_Y_CLEAR:case GAME_FORCE_BUFFETING_CLEAR:
    case GAME_FORCE_VECTOR_CLEAR:case GAME_FORCE_X_SPRING_CLEAR:case GAME_FORCE_Y_SPRING_CLEAR:
        return ((Call0)(void (*)(void))function)();
    default:return 0;
    }
}
void game_application_force_feedback_close(GameApplication *app,int finish) {
    PortablePlatform *p=app->platform;ForceFeedback *f=p?p->force_feedback:NULL;
    (void)finish;
    if(!f)return;
    if(f->initialized)f->close();
    FreeLibrary(f->library);free(f);p->force_feedback=NULL;
    game_memory_write_u32(&app->memory,0x624f68,0);
}

/* Native file operations share the application's isolated user-data owner.
 * Directory enumeration uses that same saves directory as archive loading. */
int game_application_file_path(GameApplication *app,const char *name,int write,char *path,size_t capacity) {
    PortablePlatform *p=app->platform;const char *segment;int size;
    if(!name || !*name || !path || !capacity || name[0]=='/' || name[0]=='\\' || strchr(name,':'))return 0;
    for(segment=name;*segment;) {
        const char *end=segment;while(*end && *end!='/' && *end!='\\')++end;
        if(end-segment==2 && segment[0]=='.' && segment[1]=='.')return 0;
        segment=*end?end+1:end;
    }
    if(write && (!p || !p->user_directory))return 0;
    if(p && p->user_directory && (write || !strncmp(name,"saves\\",6) || !strncmp(name,"saves/",6)))
        size=snprintf(path,capacity,"%s/%s",p->user_directory,name);
    else size=snprintf(path,capacity,"%s",name);
    return size>=0 && (size_t)size<capacity;
}
int game_application_file_delete(GameApplication *app,const char *name) {
    char path[4128];return game_application_file_path(app,name,1,path,sizeof(path)) && DeleteFileA(path);
}
/*00531980/CopyFileA: preserve overwrite and the caller's ignored failure.
 * All streams remain native full-width objects and close on every exit. */
int game_application_file_copy(GameApplication *app,const char *name,const char *destination) {
    GameAddress id=game_application_archive_open(app,name,0);ResourceFile *source,*target;char path[4128];uint8_t bytes[8192];size_t count;int ok=1;
    if(!id)return 0;
    source=game_application_archive_file(app,id);
    if(!game_application_file_path(app,destination,1,path,sizeof(path))){game_application_archive_close(app,id);return 0;}
    target=resource_file_open_mode(path,"wb");
    if(!target){game_application_archive_close(app,id);return 0;}
    while((count=resource_file_read(source,bytes,sizeof(bytes)))!=0)
        if(resource_file_write(target,bytes,count)!=count){ok=0;break;}
    if(!resource_file_flush(target))ok=0;
    resource_file_close(target);game_application_archive_close(app,id);return ok;
}
ResourceFile *game_application_archive_file(GameApplication *app,GameAddress id) {
    PortablePlatform *p=app->platform;ArchiveFile *entry;
    if(!p)return NULL;
    for(entry=p->archives;entry;entry=entry->next)if(entry->identity==id)return entry->file;
    return NULL;
}
int game_application_archive_close(GameApplication *app,GameAddress id) {
    PortablePlatform *p=app->platform;ArchiveFile **link,*entry;
    if(!p)return 0;
    for(link=&p->archives;*link;link=&(*link)->next)if((*link)->identity==id) {
        entry=*link;*link=entry->next;resource_file_close(entry->file);free(entry);return 1;
    }
    return 0;
}
GameAddress game_application_archive_open(GameApplication *app,const char *name,int write) {
    PortablePlatform *p=app->platform;ResourceFile *file=NULL;ArchiveFile *entry;char path[4128];const char *segment;
    if(!p || !name || !*name || p->archive_sequence>=0xffffffu)return 0;
    if(write) {
        if(!p->user_directory || name[0]=='/' || name[0]=='\\' || strchr(name,':'))return 0;
        for(segment=name;*segment;) {
            const char *end=segment;while(*end && *end!='/' && *end!='\\')++end;
            if(end-segment==2 && segment[0]=='.' && segment[1]=='.')return 0;
            segment=*end?end+1:end;
        }
    }
    if(p->user_directory) {
        int size=snprintf(path,sizeof(path),"%s/%s",p->user_directory,name);
        if(size<0 || (size_t)size>=sizeof(path))return 0;
        file=resource_file_open_mode(path,write?"wb":"rb");
    }
    if(!file && !write)file=resource_file_open(name);
    if(!file)return 0;
    entry=malloc(sizeof(*entry));if(!entry){resource_file_close(file);return 0;}
    entry->file=file;entry->identity=0xf7000000u+(++p->archive_sequence);
    entry->next=p->archives;p->archives=entry;return entry->identity;
}

/*004398e0/00439920: native SDK initialization/query/selection and shutdown.
 * This isolates the existing backend; no graphics instructions or CPU thunks
 * execute here. Missing native SDK/hardware retains the original false result. */
void *game_application_backend_library(GameApplication *a,const char *name){
#ifdef UPRISING_OPENGL1
    char path[MAX_PATH];UINT length;
    if(strcmp(name,"ddraw.dll") || !GetEnvironmentVariableA("UPRISING_RECOMP_WINDOWED",NULL,0) ||
       !GetEnvironmentVariableA("UPRISING_RECOMP_NATIVE_WINDOWED_DDRAW",NULL,0))return NULL;
    length=GetSystemDirectoryA(path,sizeof(path));
    if(!length || length+sizeof("\\ddraw.dll")>sizeof(path))return NULL;
    memcpy(path+length,"\\ddraw.dll",sizeof("\\ddraw.dll"));
    (void)a;return LoadLibraryExA(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    char directory[1024],config[1080],path[1080],value[32];DWORD length;
    int native_draw=GetEnvironmentVariableA("UPRISING_RECOMP_NATIVE_WINDOWED_DDRAW",NULL,0)!=0;
    static const struct {const char *section,*key,*value;} required[]={
        {"General","FullScreenMode","false"},{"General","CaptureMouse","false"},
        {"GeneralExt","FreeMouse","true"},{"DirectX","AppControlledScreenMode","false"},
        {"DirectX","DisableAndPassThru","false"}
    };unsigned i;
    if(!GetEnvironmentVariableA("UPRISING_RECOMP_WINDOWED",NULL,0))return LoadLibraryA(name);
    length=GetEnvironmentVariableA("UPRISING_RECOMP_BACKEND_DIRECTORY",directory,sizeof(directory));
    if(!length || length>=sizeof(directory) || !((directory[0] && directory[1]==':' &&
       (directory[2]=='\\' || directory[2]=='/')) || (directory[0]=='\\' && directory[1]=='\\')) ||
       (strcmp(name,"glide2x.dll") && strcmp(name,"ddraw.dll")))goto unavailable;
    if(snprintf(config,sizeof(config),"%s\\dgVoodoo.conf",directory)>=(int)sizeof(config))goto unavailable;
    for(i=0;i<sizeof(required)/sizeof(required[0]);i++){
        if(!GetPrivateProfileStringA(required[i].section,required[i].key,"",value,sizeof(value),config) ||
           lstrcmpiA(value,required[i].value))goto unavailable;
    }
    /*Glide always uses the pinned windowed module. Explicit native DirectDraw
     *uses only normal cooperative/offscreen surfaces; it is not a fallback
     *from a failed wrapper or permission to request a display mode. */
    for(i=0;i<2;i++){
        if(i && native_draw)continue;
        if(snprintf(path,sizeof(path),"%s\\%s",directory,i?"ddraw.dll":"glide2x.dll")>=(int)sizeof(path) ||
           GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES)goto unavailable;
    }
    if(native_draw && !strcmp(name,"ddraw.dll")){
        length=GetSystemDirectoryA(directory,sizeof(directory));
        if(!length || length>=sizeof(directory) ||
           snprintf(path,sizeof(path),"%s\\ddraw.dll",directory)>=(int)sizeof(path))goto unavailable;
        return LoadLibraryExA(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    if(snprintf(path,sizeof(path),"%s\\%s",directory,name)>=(int)sizeof(path))goto unavailable;
    return LoadLibraryExA(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
unavailable:
    game_application_log(a,"windowed backend rejected: %s\n","explicit local DLLs and forced windowed/free-mouse configuration required");return NULL;
#endif
}
int game_application_ui_begin(GameApplication *app) {
    PortablePlatform *p=app->platform;
#ifndef UPRISING_OPENGL1
    void (WINAPI *initialize)(void);void (WINAPI *select)(int32_t);
    int32_t (WINAPI *query)(void *);FARPROC entry;uint32_t hardware[37]={0};
#endif
    if(!p || p->glide_initialized)return 0;
    /*The installed wrapper can change the desktop even for a hidden HWND.
     *Headless validation must fail before entering it, never simulate success. */
    if(GetEnvironmentVariableA("UPRISING_RECOMP_HEADLESS",NULL,0)){
        game_application_log(app,"%s\n","HEADLESS_GLIDE_DISABLED: external Glide presentation is unsafe for unattended tests");
        return 0;
    }
#ifdef UPRISING_OPENGL1
    if(!GetEnvironmentVariableA("UPRISING_RECOMP_WINDOWED",NULL,0))return 0;
    return gl1_install(app,p);
#else
    if(!p->glide)p->glide=game_application_backend_library(app,"glide2x.dll");
    if(!p->glide){game_application_log(app,"native backend: %s\n","Glide SDK unavailable");return 0;}
#define LOAD_GLIDE(function,name) do { \
    typedef char pointer_width[sizeof(function)==sizeof(entry)?1:-1]; \
    (void)sizeof(pointer_width);entry=GetProcAddress(p->glide,name); \
    if(!entry)goto unavailable;memcpy(&function,&entry,sizeof(function)); \
} while(0)
    LOAD_GLIDE(initialize,"_grGlideInit@0");
    LOAD_GLIDE(p->glide_shutdown,"_grGlideShutdown@0");
    LOAD_GLIDE(p->window_open,"_grSstWinOpen@28");
    LOAD_GLIDE(p->window_close,"_grSstWinClose@0");
    LOAD_GLIDE(p->fog_generate,"_guFogGenerateExp2@8");
    LOAD_GLIDE(p->fog_color,"_grFogColorValue@4");
    LOAD_GLIDE(p->fog_table,"_grFogTable@4");
    LOAD_GLIDE(p->texture_lod_bias,"_grTexLodBiasValue@8");
    LOAD_GLIDE(p->texture_ncc_table,"_grTexNCCTable@8");
    LOAD_GLIDE(p->chroma_mode,"_grChromakeyMode@4");
    LOAD_GLIDE(p->chroma_value,"_grChromakeyValue@4");
    LOAD_GLIDE(p->alpha_reference,"_grAlphaTestReferenceValue@4");
    LOAD_GLIDE(p->buffer_clear,"_grBufferClear@12");
    LOAD_GLIDE(p->buffer_swap,"_grBufferSwap@4");
    LOAD_GLIDE(p->lfb_lock,"_grLfbLock@24");
    LOAD_GLIDE(p->lfb_unlock,"_grLfbUnlock@8");
    LOAD_GLIDE(p->lfb_depth,"_grLfbConstantDepth@4");
    LOAD_GLIDE(query,"_grSstQueryHardware@4");
    LOAD_GLIDE(select,"_grSstSelect@4");
    LOAD_GLIDE(p->texture_min,"_grTexMinAddress@4");
    LOAD_GLIDE(p->texture_max,"_grTexMaxAddress@4");
    LOAD_GLIDE(p->texture_upload,"_grTexDownloadMipMap@16");
    LOAD_GLIDE(p->texture_bytes,"_grTexTextureMemRequired@8");
    LOAD_GLIDE(p->texture_statistics,"_grSstPerfStats@4");
    LOAD_GLIDE(p->texture_reset_statistics,"_grSstResetPerfStats@0");
    LOAD_GLIDE(p->draw_line,"_grDrawLine@8");
    LOAD_GLIDE(p->draw_triangle,"_grDrawTriangle@12");
    LOAD_GLIDE(p->draw_point,"_grDrawPoint@4");
    LOAD_GLIDE(p->alpha_test,"_grAlphaTestFunction@4");
    LOAD_GLIDE(p->constant_color,"_grConstantColorValue@4");
    LOAD_GLIDE(p->depth_bias,"_grDepthBiasLevel@4");
    LOAD_GLIDE(p->texture_filter,"_grTexFilterMode@12");
    LOAD_GLIDE(p->palette_upload,"_grTexDownloadTable@12");
    LOAD_GLIDE(p->palette_range,"_grTexDownloadTablePartial@20");
    LOAD_GLIDE(p->texture_source,"_grTexSource@16");
    LOAD_GLIDE(p->texture_combine,"_grTexCombine@28");
    LOAD_GLIDE(p->texture_clamp,"_grTexClampMode@12");
    LOAD_GLIDE(p->alpha_combine,"_grAlphaCombine@20");
    LOAD_GLIDE(p->fog_mode,"_grFogMode@4");
    LOAD_GLIDE(p->cull_mode,"_grCullMode@4");
    LOAD_GLIDE(p->dither,"_grDitherMode@4");
    LOAD_GLIDE(p->depth_mode,"_grDepthBufferMode@4");
    LOAD_GLIDE(p->depth_function,"_grDepthBufferFunction@4");
    LOAD_GLIDE(p->depth_mask,"_grDepthMask@4");
    LOAD_GLIDE(p->alpha_blend,"_grAlphaBlendFunction@16");
    LOAD_GLIDE(p->color_combine,"_grColorCombine@20");
#undef LOAD_GLIDE
    /* Retail imports keep Glide mapped for the process lifetime. Some SDK
     * wrappers leave a queued WinMM callback after grGlideShutdown returns;
     * unloading its code races that callback. Pin this native SDK module,
     * while still closing contexts and releasing our LoadLibrary reference. */
    {HMODULE pinned;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                (LPCSTR)(uintptr_t)initialize,&pinned))goto unavailable;
    }
    initialize();p->glide_initialized=1;
    if(!query(hardware))return 0;
    select(0);return 1;
unavailable:
    p->draw_point=NULL;p->alpha_test=NULL;p->constant_color=NULL;p->depth_bias=NULL;
    p->texture_filter=NULL;p->palette_upload=NULL;p->palette_range=NULL;
    p->draw_triangle=NULL;p->texture_source=NULL;p->texture_combine=NULL;p->texture_clamp=NULL;
    p->alpha_combine=NULL;p->fog_mode=NULL;p->cull_mode=NULL;
    p->draw_line=NULL;p->dither=NULL;p->depth_mode=NULL;p->depth_function=NULL;
    p->depth_mask=NULL;p->alpha_blend=NULL;p->color_combine=NULL;
    FreeLibrary(p->glide);p->glide=NULL;p->glide_shutdown=NULL;
    p->texture_min=NULL;p->texture_max=NULL;p->texture_upload=NULL;
    p->texture_statistics=NULL;p->texture_reset_statistics=NULL;p->texture_bytes=NULL;return 0;
#endif
}

/*439930: actual SDK context/fog/texture setup. Locals are native SDK values;
 * the legacy fog bytes are a bounded published table, not a CPU stack image. */
int game_application_level_graphics_begin(GameApplication *a) {
    PortablePlatform *p=a->platform;uint8_t fog[64];uint32_t palette[256];
    float density;uint32_t bits=0x37fba882;unsigned i;char diagnostic[160];
    if(GetEnvironmentVariableA("UPRISING_RECOMP_HEADLESS",NULL,0)){
        game_application_log(a,"%s\n","HEADLESS_GLIDE_DISABLED: external Glide presentation is unsafe for unattended tests");
        return 0;
    }
    if(!p || !p->glide_initialized || !p->window_open || !p->fog_generate ||
        !p->palette_upload || a->memory.fault || p->window_opened){
        snprintf(diagnostic,sizeof(diagnostic),"native graphics initialization rejected: initialized=%d opened=%d fault=%d",p?p->glide_initialized:0,p?p->window_opened:0,a->memory.fault);
        game_application_log(a,"%s\n",diagnostic);return 0;
    }
    /* A hidden window is never GetActiveWindow(). Give the SDK the actual
     * native owner instead of relying on its foreground-window discovery. */
    if(!p->window_open((uintptr_t)game_menu_window_handle(a),7,0,1,0,2,1)){
        snprintf(diagnostic,sizeof(diagnostic),"native Glide window open failed: owner=%p",game_menu_window_handle(a));game_application_log(a,"%s\n",diagnostic);return 0;
    }
    p->window_opened=1;p->capture_attempted=0;memcpy(&density,&bits,sizeof(density));
    p->fog_generate(fog,density);
    if(!game_memory_write(&a->memory,0x5ad348,fog,sizeof(fog)))goto fail;
    p->fog_color(0x83b9be);p->fog_table(fog);
    p->texture_filter(0,1,1);p->texture_lod_bias(0,0.5f);
    p->texture_ncc_table(0,0);p->chroma_mode(1);p->chroma_value(0xff00ff);p->alpha_reference(255);
    /*4176e0 immediately uploads the existing P8 palette after opening. */
    for(i=0;i<256;++i)palette[i]=game_memory_read_u32(&a->memory,0x5ad3a8+i*4);
    if(a->memory.fault)goto fail;
    p->palette_upload(0,2,palette);return 1;
fail:p->window_close();p->window_opened=0;return 0;
}
/*4399b0, also used when the platform owner itself is retired. */
int game_application_level_graphics_end(GameApplication *a) {
    PortablePlatform *p=a->platform;if(!p)return 1;
    if(p->text_glide_locked){p->lfb_unlock(1,1);p->text_glide_locked=0;}
    if(p->window_opened){p->window_close();p->window_opened=0;}
    return 1;
}
int game_level_screen_clear(GameApplication *a,int enabled) {
    PortablePlatform *p=a->platform;
    if(enabled || !p || !p->window_opened || !p->buffer_clear)return 0;
    p->buffer_clear(0x83b9be,0,0xffff);return 1;
}
int game_level_screen_present(GameApplication *a,uint32_t interval) {
    PortablePlatform *p=a->platform;int32_t signed_interval;
    /*Do not enter a backend's nested message pump after this frame requested
     *reload/termination. The level loop retires the existing display owner. */
    if(game_memory_read_u32(&a->memory,0x5ac970) || game_memory_read_u32(&a->memory,0x58360c) ||
       game_memory_read_u32(&a->memory,0x59d4c8))return !a->memory.fault;
    if(!p || !p->window_opened || !p->buffer_swap)return 0;
    memcpy(&signed_interval,&interval,4);p->buffer_swap(signed_interval);
#ifdef UPRISING_OPENGL1
    if(!gl1_current || gl1_current->failed)return 0;
#endif
    return 1;
}
/*4d7680 SDK state is isolated here; the portable caller owns depth geometry. */
int game_application_frame_depth_state(GameApplication *a,int enabled){
    PortablePlatform *p=a->platform;
    if(!p || !p->window_opened || !p->depth_mode || !p->depth_mask)return 0;
    if(!enabled){
        if(!p->buffer_clear)return 0;
        p->depth_mode(2);p->depth_mask(1);p->buffer_clear(0x83b9be,0,0xffff);return 1;
    }
    if(!p->alpha_blend || !p->alpha_combine || !p->texture_combine || !p->fog_mode ||
       !p->cull_mode || !p->depth_function || !p->depth_bias || !p->dither || !p->color_combine || !p->constant_color)return 0;
    p->alpha_blend(4,0,4,0);p->alpha_combine(0,0,1,2,0);
    p->texture_combine(0,0,0,0,0,0,0);p->fog_mode(0);p->cull_mode(0);
    /* The SDK's compare-to-bias path stores the comparison bias as depth,
     * leaving the whole viewport at near depth. Clear the original viewport
     * quad through ordinary W writes; world_begin restores normal testing.
     * Exact preservation of preexisting zero-depth pixels remains unproved. */
    p->depth_bias(0);p->screen_mask_active=1;
    p->depth_mode(2);p->depth_function(7);p->dither(0);p->depth_mask(1);
    p->color_combine(1,0,1,2,0);p->constant_color(0x83b9be);return 1;
}
extern int game_menu_level_surface_flip(GameApplication *);
/* Opt-in evidence from the actual SDK front buffer. No new presentation owner
 * or CPU capture bridge; the LFB pointer is borrowed until its matching unlock. */
static void capture_game_frame(GameApplication *a,unsigned buffer){
    PortablePlatform *p=a->platform;char directory[MAX_PATH],path[MAX_PATH+32];DWORD length;
    NativeGlideLfbInfo info={0};BITMAPFILEHEADER header={0};BITMAPINFOHEADER bitmap={0};
    uint32_t width,height,row_bytes,x,y;uint8_t *row=NULL;FILE *file=NULL;int ok=0,locked=0;
    if(p->capture_attempted)return;
    p->capture_attempted=1;
    length=GetEnvironmentVariableA("UPRISING_RECOMP_CAPTURE_DIR",directory,sizeof(directory));
    if(!length || length>=sizeof(directory))return;
    width=game_memory_read_u32(&a->memory,0x627928);height=game_memory_read_u32(&a->memory,0x62792c);
    if(a->memory.fault || !width || !height || width>16384 || height>16384)goto done;
    info.size=sizeof(info);
    if(!p->lfb_lock(0,buffer,0,0,0,&info))goto done;
    locked=1;
    if(!info.pixels || info.stride<width*2 || info.write_mode || info.origin)goto done;
    row_bytes=(width*3+3)&~3u;row=calloc(row_bytes,1);if(!row)goto done;
    snprintf(path,sizeof(path),"%s/native-%s-frame.bmp",directory,buffer?"before-text":"game");file=fopen(path,"wb");if(!file)goto done;
    header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(bitmap);header.bfSize=header.bfOffBits+row_bytes*height;
    bitmap.biSize=sizeof(bitmap);bitmap.biWidth=(LONG)width;bitmap.biHeight=(LONG)height;bitmap.biPlanes=1;bitmap.biBitCount=24;
    if(fwrite(&header,1,sizeof(header),file)!=sizeof(header) || fwrite(&bitmap,1,sizeof(bitmap),file)!=sizeof(bitmap))goto done;
    for(y=height;y>0;y--){const uint8_t *pixels=(const uint8_t *)info.pixels+(size_t)(y-1)*info.stride;
        for(x=0;x<width;x++){unsigned color=pixels[x*2]|((unsigned)pixels[x*2+1]<<8);
            row[x*3]=(uint8_t)((color&31)*255/31);row[x*3+1]=(uint8_t)(((color>>5)&63)*255/63);row[x*3+2]=(uint8_t)((color>>11)*255/31);}
        if(fwrite(row,1,row_bytes,file)!=row_bytes)goto done;
    }ok=1;
done:
    if(file && fclose(file))ok=0;
    free(row);if(locked && !p->lfb_unlock(0,buffer))ok=0;
    game_application_log(a,"native frame capture: %s\n",ok?"saved":"FAILED");
}
/*5189a0: actual swap then optional owned DirectDraw surface flip. The native
 * caller consumes success and samples its frame timer immediately afterward. */
int game_application_frame_present(GameApplication *a){
    if(!game_level_screen_present(a,1) || !game_menu_level_surface_flip(a))return 0;
    capture_game_frame(a,0);return 1;
}
extern int game_menu_accelerated_text_begin(GameApplication *,GameAddress,const GameUiRect *,const GameUiRect *,GameTextTarget *,int32_t *);
extern int game_menu_accelerated_text_end(GameApplication *);
/*532bf0: borrow the actual backend pixels for exactly one glyph operation. */
int game_application_glyph_begin(GameApplication *a,GameTextTarget *target,int32_t *fallback){
    PortablePlatform *p=a->platform;GameMemory *m=&a->memory;NativeGlideLfbInfo info={0};
    uint32_t height=game_memory_read_u32(m,0x62792c);
    if(p && GetEnvironmentVariableA("UPRISING_RECOMP_CAPTURE_BEFORE_TEXT",NULL,0))capture_game_frame(a,1);
    if(!game_memory_read_u32(m,0x5840f0)){
        GameUiRect clip;uint32_t v;int32_t *fields[4]={&clip.left,&clip.top,&clip.right,&clip.bottom};
        static const GameAddress addresses[4]={0x59d450,0x59d458,0x59d454,0x59d45c};unsigned i;
        for(i=0;i<4;i++){v=game_memory_read_u32(m,addresses[i]);memcpy(fields[i],&v,4);}
        return !m->fault && game_menu_accelerated_text_begin(a,0,&clip,&clip,target,fallback);
    }
    if(m->fault || !target || !fallback || !p || !p->window_opened || p->text_glide_locked ||
       !p->lfb_lock || !p->lfb_unlock || !p->lfb_depth || !p->depth_mode || !p->depth_mask || !p->depth_function){
        fprintf(stderr,"native text backend unavailable: fault=%u window=%d locked=%d lfb=%d/%d/%d depth=%d/%d/%d\n",
            m->fault,p?p->window_opened:0,p?p->text_glide_locked:0,p&&p->lfb_lock,p&&p->lfb_unlock,p&&p->lfb_depth,
            p&&p->depth_mode,p&&p->depth_mask,p&&p->depth_function);return 0;
    }
    p->depth_mode(0);p->depth_mask(0);p->lfb_depth(0xffff);info.size=sizeof(info);
    if(!p->lfb_lock(1,1,0,0,0,&info)){fprintf(stderr,"native text backend: LFB lock rejected\n");goto fail;}
    p->text_glide_locked=1;
    if(!info.pixels || !info.stride || info.stride>INT32_MAX || (info.stride&1) || !height ||
       (uint64_t)info.stride*height>SIZE_MAX){fprintf(stderr,"native text backend: invalid LFB pixels=%p stride=%u height=%u\n",info.pixels,info.stride,height);goto fail;}
    memset(target,0,sizeof(*target));target->pixels=info.pixels;target->pixel_bytes=(size_t)info.stride*height;
    target->stride=(int32_t)(info.stride/2);target->rgb565=1;*fallback=20;return 1;
fail:
    if(p->text_glide_locked){p->lfb_unlock(1,1);p->text_glide_locked=0;}
    p->depth_mode(2);p->depth_function(3);p->depth_mask(1);return 0;
}
int game_application_glyph_end(GameApplication *a){
    PortablePlatform *p=a->platform;int ok;
    if(p && p->text_glide_locked){
        ok=p->lfb_unlock(1,1)!=0;p->text_glide_locked=0;
        p->depth_mode(2);p->depth_function(3);p->depth_mask(1);return ok;
    }
    return !game_memory_read_u32(&a->memory,0x5840f0) && game_menu_accelerated_text_end(a);
}
/*439e00/439e30: the palette frame's actual colored quad and SDK state. */
int game_application_palette_quad(GameApplication *a,int32_t left,int32_t top,int32_t right,int32_t bottom,uint8_t color){
    PortablePlatform *p=a->platform;GameRenderVertex vertices[4],inherited[4]={{0}};uint8_t colors[4][3];unsigned i,k;
    if(!p || !p->window_opened || !p->fog_mode || !p->alpha_blend || !p->color_combine ||
       !p->depth_mode || !p->depth_function || !p->depth_mask || !p->cull_mode || !p->draw_triangle)return 0;
    for(i=0;i<4;i++)for(k=0;k<3;k++)colors[i][k]=game_memory_read_u8(&a->memory,0x5ad3a8+(uint32_t)color*4+2-k);
    if(a->memory.fault)return 0;
    p->fog_mode(0);p->alpha_blend(4,0,4,0);p->color_combine(3,8,1,0,0);
    p->depth_mode(2);p->depth_function(3);p->depth_mask(1);p->cull_mode(0);
    game_screen_build_quad(vertices,inherited,left,top,right,bottom,colors,0x3f000000);
    p->draw_triangle(vertices,vertices+1,vertices+2);p->draw_triangle(vertices,vertices+2,vertices+3);return 1;
}
int game_level_screen_palette(GameApplication *a,GameAddress address) {
    PortablePlatform *p=a->platform;uint32_t colors[256];unsigned i;
    if(!p || !p->window_opened || !p->palette_upload)return 0;
    for(i=0;i<256;++i)colors[i]=game_memory_read_u32(&a->memory,address+i*4);
    if(a->memory.fault)return 0;
    p->palette_upload(0,2,colors);return 1;
}
/*4d9570: the final overlay queue shares the native world renderer. Restore
 *chromakey even when a submission fails, so failure cleanup preserves SDK state. */
int game_level_frame_004d9570(GameApplication *a){
    PortablePlatform *p=a->platform;
    GameAddress head=game_memory_read_u32(&a->memory,0x604f08);int ok;
    if(a->memory.fault)return 0;
    if(!head)return 1;
    if(!p || !p->window_opened || !p->chroma_mode)return 0;
    p->chroma_mode(0);ok=game_level_world_render(a,head);p->chroma_mode(1);
    return ok;
}
typedef struct LevelImageDraw {GameApplication *app;TextureRecord *texture;int started;} LevelImageDraw;
static int level_image_begin(void *context) {(void)context;return 1;}
static int level_image_tile(void *context,const uint8_t pixels[1024],const GameRenderVertex vertices[4]) {
    LevelImageDraw *draw=context;GameApplication *a=draw->app;PortablePlatform *p=a->platform;
    TextureRecord *record=draw->texture;NativeGlideTextureInfo info;
    if(texture_store_write_indexed(record,pixels,1024,0,0,1)<0)return 0;
    if(!(record->flags&0x80000000u)) {
        if(!game_level_texture_use(a,record->id))return 0;
    } else {
        info.small_lod=info.large_lod=record->image.lod;info.aspect=record->image.aspect;
        info.format=record->image.format;info.pixels=record->image.pixels;
        p->texture_upload(0,record->address,3,&info);
    }
    if(!draw->started) {
        p->texture_combine(0,1,0,1,0,0,0);p->color_combine(3,8,1,1,0);p->alpha_blend(4,0,4,0);
        p->depth_function(7);p->depth_mode(2);p->depth_mask(1);p->texture_filter(0,0,0);
        p->dither(0);p->fog_mode(0);draw->started=1;
    }
    return game_application_surface_texture(a,record) && game_application_surface_polygon(a,vertices,4);
}
static int level_image_finish(void *context) {
    LevelImageDraw *draw=context;PortablePlatform *p=draw->app->platform;
    p->texture_filter(0,1,1);return 1;
}
extern TextureRecord *game_model_ui_tile_texture(GameApplication *);
static int frame_image_tile(void *context,const uint8_t pixels[1024],const GameRenderVertex vertices[4]){
    LevelImageDraw *draw=context;PortablePlatform *p=draw->app->platform;TextureRecord *record=draw->texture;
    NativeGlideTextureInfo info;
    if(texture_store_write_indexed(record,pixels,1024,0,0,1)<0 || !game_level_texture_use(draw->app,record->id))return 0;
    info.small_lod=info.large_lod=record->image.lod;info.aspect=record->image.aspect;
    info.format=record->image.format;info.pixels=record->image.pixels;
    p->texture_upload(0,record->address,3,&info);
    if(!game_application_surface_texture(draw->app,record))return 0;
    p->draw_triangle(vertices,vertices+1,vertices+2);p->draw_triangle(vertices,vertices+2,vertices+3);return 1;
}
/*4d8b50: the existing tiled-image algorithm consumes bounded owned bitmap
 * pixels directly. The source/destination may alias; neither is rewritten. */
int game_level_frame_004d8b50(GameApplication *a,const uint32_t source[4],uint32_t destination[4],const uint32_t descriptor[2],float oow){
    PortablePlatform *p=a->platform;GameUiIndexedImage image;GameUiRect src,dst;LevelImageDraw draw;
    GameUiTileOps ops={&draw,level_image_begin,frame_image_tile,level_image_finish};int32_t c[8];unsigned i;
    if(!source || !destination || !descriptor || !p || !p->window_opened || !p->texture_upload || !p->draw_triangle ||
       !p->texture_combine || !p->alpha_blend || !p->color_combine || !p->depth_function || !p->depth_mode ||
       !p->texture_filter || !p->dither || !p->fog_mode)return 0;
    image.width=image.stride=descriptor[0]&65535;image.height=descriptor[0]>>16;
    if(!image.width || image.width>32767 || !image.height || (uint64_t)image.stride*image.height>SIZE_MAX)return 0;
    image.size=(size_t)image.stride*image.height;
    image.pixels=scene_projection_storage_borrow_bytes(descriptor[1],image.size);
    if(!image.pixels)image.pixels=game_radar_map_borrow(descriptor[1],image.size);
    if(!image.pixels)return 0;
    for(i=0;i<4;i++){memcpy(c+i,source+i,4);memcpy(c+4+i,destination+i,4);}
    src.left=c[0];src.top=c[1];src.right=c[2];src.bottom=c[3];
    dst.left=c[4];dst.top=c[5];dst.right=c[6];dst.bottom=c[7];
    draw.app=a;draw.texture=game_model_ui_tile_texture(a);draw.started=0;if(!draw.texture)return 0;
    p->texture_combine(0,1,0,1,0,0,0);p->alpha_blend(4,0,4,0);p->color_combine(3,8,1,1,0);
    p->depth_function(3);p->depth_mode(2);p->texture_filter(0,0,0);p->dither(0);p->fog_mode(0);
    {uint32_t bits=game_memory_read_u32(&a->memory,0x574a18);float epsilon;int ok;memcpy(&epsilon,&bits,4);
     ok=!a->memory.fault && game_ui_draw_tiled_image(&image,&src,&dst,oow,epsilon,&ops)==GAME_UI_TILED_IMAGE_OK;
     if(!ok)p->texture_filter(0,1,1);return ok;}
}
/*43b0b0: the established native tile algorithm and existing TextureStore own
 * all working pixels. The source snapshot is transient and the scratch texture
 * is retired after submission, including failed submissions. */
int game_level_screen_image(GameApplication *a,GameAddress source,uint32_t stride) {
    PortablePlatform *p=a->platform;uint8_t *pixels;unsigned row;int ok;
    AssetTexture scratch={0};MaterialValue material={0};LevelImageDraw draw;
    GameUiIndexedImage image;GameUiTileOps ops={&draw,level_image_begin,level_image_tile,level_image_finish};
    if(!p || !p->window_opened || !p->texture_upload || stride<640 ||
        (uint64_t)source+479u*(uint64_t)stride+640u>UINT32_MAX)return 0;
    pixels=malloc(640u*480u);if(!pixels)return 0;
    for(row=0;row<480;++row)if(!game_memory_read(&a->memory,source+row*stride,pixels+row*640u,640)){free(pixels);return 0;}
    scratch.width=scratch.height=scratch.render_width=scratch.render_height=32;
    scratch.lod=3;scratch.aspect=3;scratch.format=5;scratch.size=1024;scratch.pixels=calloc(1,1024);
    if(!scratch.pixels || !game_model_texture_take(a,&scratch,0,&material)) {
        asset_texture_release(&scratch);free(pixels);return 0;
    }
    draw.app=a;draw.texture=game_model_texture_record(a,material.backend_handle);draw.started=0;
    image.pixels=pixels;image.size=640u*480u;image.width=image.stride=640;image.height=480;
    ok=draw.texture && game_ui_draw_fullscreen_image(&image,&ops)==GAME_UI_TILED_IMAGE_OK;
    free(pixels);if(!game_model_texture_release(a,material.backend_handle))ok=0;
    return ok && !a->memory.fault;
}
void game_application_ui_shutdown(GameApplication *app) {
    PortablePlatform *p=app->platform;
    if(!p)return;
    game_application_level_graphics_end(app);
    p->draw_line=NULL;p->dither=NULL;p->depth_mode=NULL;p->depth_function=NULL;
    p->depth_mask=NULL;p->alpha_blend=NULL;p->color_combine=NULL;
    if(p->glide_initialized && p->glide_shutdown)p->glide_shutdown();
    p->graphics_owner=NULL;
    p->glide_initialized=0;
    if(p->glide)FreeLibrary(p->glide);
    p->glide=NULL;p->glide_shutdown=NULL;
    p->texture_min=NULL;p->texture_max=NULL;p->texture_upload=NULL;
    p->texture_statistics=NULL;p->texture_reset_statistics=NULL;p->texture_bytes=NULL;
}
/*439ce0: ordinary SDK vertices and actual palette bytes. Native DLL pointers
 * stay with the platform owner; no address-ID calling convention adapter. */
int game_application_glide_line(GameApplication *a,const int32_t coordinates[4],uint8_t color){
    PortablePlatform *p=a->platform;GameRenderVertex inherited[2]={{0}},vertices[2];
    GameAddress palette=0x5ad3a8u+(uint32_t)color*4u;uint8_t r,g,b;
    if(!p || !p->glide_initialized || !p->draw_line || !p->dither || !p->depth_mode ||
       !p->depth_function || !p->depth_mask || !p->alpha_blend || !p->color_combine)return 0;
    b=game_memory_read_u8(&a->memory,palette);g=game_memory_read_u8(&a->memory,palette+1);r=game_memory_read_u8(&a->memory,palette+2);
    if(a->memory.fault)return 0;
    game_screen_build_segment(vertices,inherited,coordinates,r,g,b);
    p->dither(0);p->alpha_blend(4,0,4,0);p->color_combine(3,8,1,0,0);
    p->depth_mode(2);p->depth_function(3);p->depth_mask(1);
    p->draw_line(&vertices[0],&vertices[1]);return 1;
}
int game_application_palette_point(GameApplication *a,int32_t x,int32_t y,uint8_t color){
    PortablePlatform *p=a->platform;GameRenderVertex inherited={0},vertex;
    GameAddress palette=0x5ad3a8+(uint32_t)color*4;
    if(!p || !p->glide_initialized || !p->draw_point || !p->fog_mode || !p->alpha_blend ||
       !p->color_combine || !p->depth_mode || !p->depth_function || !p->depth_mask)return 0;
    game_screen_build_point(&vertex,&inherited,x,y,game_memory_read_u8(&a->memory,palette+2),
        game_memory_read_u8(&a->memory,palette+1),game_memory_read_u8(&a->memory,palette));
    if(a->memory.fault)return 0;
    p->fog_mode(0);p->alpha_blend(4,0,4,0);p->color_combine(3,8,1,0,0);
    p->depth_mode(2);p->depth_function(3);p->depth_mask(1);p->draw_point(&vertex);return 1;
}
/*Circle's shader is established once, even when all segments are clipped. */
int game_application_palette_lines(GameApplication *a,const int32_t lines[][4],unsigned count,uint8_t color){
    PortablePlatform *p=a->platform;GameAddress palette=0x5ad3a8+(uint32_t)color*4;
    GameRenderVertex inherited[2]={{0}},vertices[2];uint8_t r,g,b;unsigned i;
    if(!p || !p->glide_initialized || !p->draw_line || !p->fog_mode || !p->alpha_blend ||
       !p->color_combine || !p->depth_mode || !p->depth_function || !p->depth_mask || count>32 || (count && !lines))return 0;
    r=game_memory_read_u8(&a->memory,palette+2);g=game_memory_read_u8(&a->memory,palette+1);b=game_memory_read_u8(&a->memory,palette);
    if(a->memory.fault)return 0;
    p->fog_mode(0);p->alpha_blend(4,0,4,0);p->color_combine(3,8,1,0,0);
    p->depth_mode(2);p->depth_function(3);p->depth_mask(1);
    for(i=0;i<count;i++){game_screen_build_segment(vertices,inherited,lines[i],r,g,b);p->draw_line(vertices,vertices+1);}
    return 1;
}
/*The native SDK ABI is isolated here. Device addresses are numbers; the
 *texture-info data member is a full-width native pointer, never a guest word. */
static int texture_range(void *context,uint32_t *first,uint32_t *last){
    PortablePlatform *p=((GameApplication *)context)->platform;
    if(!p || !p->glide_initialized || !p->texture_min || !p->texture_max)return 0;
    *first=p->texture_min(0);*last=p->texture_max(0);return 1;
}
static int texture_statistics(void *context,uint32_t output[5]){
    PortablePlatform *p=((GameApplication *)context)->platform;
    if(!p || !p->glide_initialized || !p->texture_statistics || !p->texture_reset_statistics)return 0;
    p->texture_statistics(output);p->texture_reset_statistics();return 1;
}
static int texture_upload(void *context,uint32_t address,const uint8_t format[4],GameAddress data,uint32_t bytes){
    GameApplication *a=context;PortablePlatform *p=a->platform;NativeGlideTextureInfo info;uint8_t *pixels;
    if(!p || !p->glide_initialized || !p->texture_upload || !p->texture_bytes)return 0;
    pixels=scene_projection_storage_borrow_bytes(data,bytes);
    if(!pixels)return 0;
    info.small_lod=format[0];info.large_lod=format[1];info.aspect=format[2];info.format=format[3];info.pixels=pixels;
    {uint32_t required=p->texture_bytes(3,&info);if(!required || required>bytes)return 0;}
    p->texture_upload(0,address,3,&info);return 1;
}
static TextureRecord *texture_record(void *context,uint32_t id){return game_model_texture_record(context,id);}
static int texture_upload_owned(void *context,uint32_t address,const AssetTexture *image){
    PortablePlatform *p=((GameApplication *)context)->platform;NativeGlideTextureInfo info;uint8_t minimum[8]={0};
    if(!p || !p->glide_initialized || !p->texture_upload || !p->texture_bytes || !image || !image->pixels)return 0;
    info.small_lod=image->lod;info.large_lod=image->lod;info.aspect=image->aspect;info.format=image->format;info.pixels=image->pixels;
    {uint32_t required=p->texture_bytes(3,&info);
     if(required==sizeof(minimum) && image->size<sizeof(minimum)){
        /*The synchronous SDK upload reads its minimum8-byte block. Supply
         *bounded zero padding, without changing the owned logical image. */
        memcpy(minimum,image->pixels,image->size);info.pixels=minimum;
     }else if(!required || required>image->size){
        fprintf(stderr,"native texture upload size failed: width=%u height=%u lod=%u aspect=%u format=%u bytes=%u required=%u\n",
            image->width,image->height,image->lod,image->aspect,image->format,(unsigned)image->size,required);return 0;}}
    p->texture_upload(0,address,3,&info);return 1;
}
static GameGlideTextureServices texture_services(GameApplication *a){
    GameGlideTextureServices s={a,texture_range,texture_statistics,texture_upload,texture_record,texture_upload_owned};return s;
}
int game_level_interface_prepare(GameApplication *a){
    GameGlideTextureServices s=texture_services(a);return game_glide_texture_cache_begin(&a->memory,&s);
}
int game_level_interface_end(GameApplication *a){
    if(!game_level_projected_texture_release(a))return 0;
    if(!game_glide_texture_cache_end(&a->memory))return 0;
    game_model_texture_residency_clear(a);return !a->memory.fault;
}
int game_level_texture_frame(GameApplication *a){
    GameGlideTextureServices s=texture_services(a);return game_glide_texture_cache_frame(&a->memory,&s);
}
int game_level_texture_use(GameApplication *a,GameAddress texture){
    GameGlideTextureServices s=texture_services(a);return game_glide_texture_use(&a->memory,texture,&s);
}
/*The actual4d7bb0 shader state and triangle algorithm are exposed as native
 *platform services. SDK pointers stay inside this full-width platform owner. */
int game_application_surface_begin(GameApplication *a){
    PortablePlatform *p=a->platform;
    if(!p || !p->glide_initialized || !p->texture_combine || !p->color_combine || !p->alpha_blend ||
       !p->alpha_combine || !p->depth_mode || !p->depth_function || !p->depth_mask || !p->dither || !p->fog_mode || !p->cull_mode)return 0;
    p->texture_combine(0,1,0,1,0,0,0);p->color_combine(3,8,1,1,0);p->alpha_blend(4,0,4,0);
    p->alpha_combine(0,0,1,2,0);p->depth_mode(2);p->depth_function(1);p->depth_mask(0);
    p->dither(2);p->fog_mode(0);p->cull_mode(0);return 1;
}
int game_application_sky_begin(GameApplication *a,const GameSkyFrame *frame){
#ifdef UPRISING_OPENGL1
    if(!a || !a->platform)return 0;
    return gl1_sky_begin(frame);
#else
    (void)a;(void)frame;return 1;
#endif
}
int game_application_sky_end(GameApplication *a){
#ifdef UPRISING_OPENGL1
    if(!a || !a->platform)return 0;
    return gl1_sky_end();
#else
    (void)a;return 1;
#endif
}
int game_application_remote_sky(GameApplication *a,const GameRemoteSkyFrame *frame){
#ifdef UPRISING_OPENGL1
    if(!a || !a->platform)return 0;
    return gl1_remote_sky(frame);
#else
    (void)a;(void)frame;return 1;
#endif
}
int game_application_surface_clamp(GameApplication *a,int enabled){
    PortablePlatform *p=a->platform;if(!p || !p->glide_initialized || !p->texture_clamp)return 0;
    p->texture_clamp(0,(uint32_t)enabled,(uint32_t)enabled);return 1;
}
int game_application_surface_texture(GameApplication *a,const TextureRecord *texture){
    PortablePlatform *p=a->platform;NativeGlideTextureInfo info;
    if(!p || !p->glide_initialized || !p->texture_source || !texture || !texture->image.pixels)return 0;
    info.small_lod=texture->image.lod;info.large_lod=texture->image.lod;info.aspect=texture->image.aspect;
    info.format=texture->image.format;info.pixels=texture->image.pixels;
    p->texture_source(0,texture->address,3,&info);return 1;
}
int game_application_surface_polygon(GameApplication *a,const GameRenderVertex *vertices,unsigned count){
    PortablePlatform *p=a->platform;GameRenderVertex mask[32];float y[32];uint8_t triangles[30][3];unsigned i,n;
    if(!p || !p->glide_initialized || !p->draw_triangle || !vertices || !count || count>32)return 0;
    if(p->screen_mask_active){
        memcpy(mask,vertices,count*sizeof(*vertices));
        /* 65536 wraps to zero in the original FBI W representation. */
        for(i=0;i<count;i++)if(mask[i].oow==65536.0f)mask[i].oow=0;
        vertices=mask;
    }
    for(i=0;i<count;++i)y[i]=vertices[i].y;n=game_screen_polygon_triangles(y,count,triangles);
    for(i=0;i<n;++i)p->draw_triangle(vertices+triangles[i][0],vertices+triangles[i][1],vertices+triangles[i][2]);
    return 1;
}
int game_application_texture_update(GameApplication *a,const TextureRecord *texture){
    PortablePlatform *p=a->platform;NativeGlideTextureInfo info;
    if(!p || !p->glide_initialized || !p->texture_upload || !texture ||
       !texture->image.pixels || !(texture->flags&0x80000000u))return 0;
    info.small_lod=info.large_lod=texture->image.lod;info.aspect=texture->image.aspect;
    info.format=texture->image.format;info.pixels=texture->image.pixels;
    p->texture_upload(0,texture->address,3,&info);return 1;
}
int game_application_scene_polygon_begin(GameApplication *a,int textured){
    PortablePlatform *p=a->platform;
    if(!p || !p->glide_initialized || !p->color_combine || !p->alpha_blend ||
       !p->depth_mode || !p->depth_function || !p->fog_mode)return 0;
    if(textured){
        if(!p->texture_combine || !p->dither)return 0;
        p->texture_combine(0,1,0,1,0,0,0);p->color_combine(3,8,1,1,0);p->dither(0);
    }else{
        if(!p->cull_mode || !p->constant_color || !p->alpha_combine)return 0;
        p->constant_color(0x7f000000);p->alpha_combine(3,8,1,2,0);
        p->color_combine(3,8,1,0,0);p->cull_mode(0);
    }
    p->fog_mode(0);p->alpha_blend(4,0,4,0);p->depth_mode(2);p->depth_function(3);return 1;
}
int game_application_scene_translucency(GameApplication *a,int enabled){
    PortablePlatform *p=a->platform;
    if(!p || !p->glide_initialized || !p->alpha_blend || !p->depth_mask)return 0;
    p->alpha_blend(enabled?1:4,enabled?5:0,4,0);p->depth_mask(enabled?0:1);return 1;
}
int game_application_world_begin(GameApplication *a){
    PortablePlatform *p=a->platform;
    if(!p || !p->glide_initialized || !p->texture_combine || !p->color_combine || !p->alpha_combine ||
       !p->alpha_blend || !p->depth_mode || !p->depth_function || !p->depth_mask || !p->dither || !p->cull_mode)return 0;
    p->screen_mask_active=0;
    p->texture_combine(0,1,0,1,0,0,0);p->color_combine(3,9,0,1,0);p->alpha_combine(3,8,1,1,0);
    p->alpha_blend(1,5,4,0);p->depth_mode(2);p->depth_function(1);p->depth_mask(1);p->dither(2);p->cull_mode(0);
#ifdef UPRISING_OPENGL1
    gl1_level_fog(1);
#endif
    return 1;
}
int game_application_world_state(GameApplication *a,WorldRenderState kind,int value){
    PortablePlatform *p=a->platform;if(!p || !p->glide_initialized)return 0;
    switch(kind){
    case WORLD_ALPHA_FADE:
        if(!p->alpha_combine || !p->alpha_test)return 0;
        p->alpha_combine(3,8,1,value?0:1,0);p->alpha_test(value?2:7);return 1;
    case WORLD_ALPHA_TEST:if(!p->alpha_test)return 0;p->alpha_test((uint32_t)value);return 1;
    case WORLD_DEPTH_BIAS:if(!p->depth_bias)return 0;p->depth_bias(value);return 1;
    case WORLD_TEXTURE_FILTER:if(!p->texture_filter)return 0;p->texture_filter(0,value?1:0,value?1:0);return 1;
    case WORLD_DEPTH_WRITE:if(!p->depth_mask)return 0;p->depth_mask(value?1:0);return 1;
    }return 0;
}
int game_application_world_palette(GameApplication *a,const uint32_t *colors,unsigned first,unsigned last,int full){
    PortablePlatform *p=a->platform;
    if(!p || !p->glide_initialized || !colors || first>last || last>255)return 0;
    if(full){if(!p->palette_upload)return 0;p->palette_upload(0,2,colors);}
    else{if(!p->palette_range)return 0;p->palette_range(0,2,colors,first,last);}return 1;
}
int game_application_world_ncc(GameApplication *a,const uint8_t table[112]){
    PortablePlatform *p=a->platform;uint32_t aligned[28];
    if(!p || !p->glide_initialized || !p->palette_upload || !table)return 0;
    /*This Windows SDK accepts its112-byte table ABI. Copy to aligned storage;
     *portable code owns only the defined bytes and never aliases a host struct. */
    memcpy(aligned,table,112);p->palette_upload(0,0,aligned);return 1;
}
int game_application_world_point(GameApplication *a,const GameRenderVertex *vertex){
    PortablePlatform *p=a->platform;if(!p || !p->glide_initialized || !p->draw_point || !vertex)return 0;
#ifdef UPRISING_OPENGL1
    return gl1_world_point(vertex);
#else
    p->draw_point(vertex);return 1;
#endif
}
int game_application_render_texture(GameApplication *a,GameAddress id){
    const TextureRecord *record=game_model_texture(a,id);PortablePlatform *p=a->platform;
    NativeGlideTextureInfo info;uint8_t format[4];uint32_t size;GameAddress pixels;
    if(record)return game_application_surface_texture(a,record);
    if(!p || !p->glide_initialized || !p->texture_source)return 0;
    if(!game_memory_read(&a->memory,id+16,format,4))return 0;
    size=game_memory_read_u32(&a->memory,id+12);pixels=game_memory_read_u32(&a->memory,id+20);
    info.small_lod=format[0];info.large_lod=format[1];info.aspect=format[2];info.format=format[3];
    info.pixels=scene_projection_storage_borrow_bytes(pixels,size);
    if(a->memory.fault || !size || !info.pixels)return 0;
    p->texture_source(0,game_memory_read_u32(&a->memory,id+8),3,&info);return !a->memory.fault;
}
int game_application_projected_begin(GameApplication *a,ProjectedQueueKind kind){
    PortablePlatform *p=a->platform;
    if((unsigned)kind>=3 || !p || !p->glide_initialized || !p->fog_mode || !p->alpha_blend ||
       !p->constant_color || !p->alpha_combine || !p->color_combine || !p->depth_mode || !p->depth_function ||
       !p->depth_mask || !p->cull_mode || !p->depth_bias || (kind==PROJECTED_TEXTURED && !p->texture_filter))return 0;
    p->fog_mode(0);p->alpha_blend(1,5,4,0);
    if(kind!=PROJECTED_TEXTURED)p->constant_color(kind==PROJECTED_ALPHA?UINT32_MAX:0);
    p->alpha_combine(3,8,1,0,0);p->color_combine(3,8,1,kind==PROJECTED_TEXTURED?1:2,0);
    p->depth_mode(2);p->depth_function(3);p->depth_mask(0);p->cull_mode(0);
    if(kind==PROJECTED_TEXTURED)p->texture_filter(0,0,0);p->depth_bias(-100);return 1;
}
int game_application_projected_end(GameApplication *a,ProjectedQueueKind kind){
    PortablePlatform *p=a->platform;
    if((unsigned)kind>=3 || !p || !p->glide_initialized || !p->depth_bias ||
       (kind==PROJECTED_TEXTURED && !p->texture_filter))return 0;
    if(kind==PROJECTED_TEXTURED)p->texture_filter(0,1,1);p->depth_bias(0);return 1;
}

/* Actual 00506210/00506c30 callbacks and 00506640 resource/bootstrap chain.
 * Definitions must be ordinary native code before this target can link. */


extern int game_application_asset_bootstrap(GameApplication *, const char *);
extern void game_application_asset_shutdown(GameApplication *);
extern void game_application_window_shutdown(GameApplication *);
extern int game_level_display_masks_release(GameApplication *);
extern int game_level_progress_end(GameApplication *);

static int owned_read(void *context, GameAddress address, void *output, size_t size) {
    if(presentation_instance_projection_read(address,output,size) ||
       runtime_node_pool_projection_read(address,output,size))return 1;
    GameApplication *app = context;
    GameplaySpatialGrid *grid = gameplay_spatial_grid(0x5f0800u);
    if(address>=SCENE_SURFACE_BASE && address<SCENE_SURFACE_BASE+0x5000u){
        if(!scene_surface_read(app->memory.surface,address,output,size))goto fail;return 1;
    }
    if(address>=GAME_TERRAIN_CELLS && address<GAME_TERRAIN_MATERIALS+0x80000u){
        if(!game_level_terrain_read(app,address,output,size))goto fail;
        return 1;
    }
    if(address>=RENDER_ARENA_BASE && address<RENDER_ARENA_END) {
        if(!render_arena_read(address,output,size))goto fail;
        return 1;
    }
    if(address>=RADAR_MAP_BASE && address<RADAR_MAP_END) {
        if(!game_radar_map_read(address,output,size))goto fail;
        return 1;
    }
    if(address>=0xfb000000u && address<0xfb070000u) {
        unsigned pool=(address-0xfb000000u)/0x10000u,unit=pool==6u?2u:4u;
        size_t offset=(address-0xfb000000u)%0x10000u,i,capacity=render_runtime_pool_size(pool);
        if(offset>capacity || size>capacity-offset)goto fail;
        for(i=0;i<size;++i) {
            size_t at=offset+i;uint32_t bits=0;
            if(unit==2u) {
                uint16_t half;
                if(!render_runtime_pool_read_native(pool,at&~(size_t)1u,&half,2u))goto fail;
                bits=half;
            } else if(!render_runtime_pool_read_native(pool,at&~(size_t)3u,&bits,4u))goto fail;
            ((uint8_t *)output)[i]=(uint8_t)(bits>>((at%unit)*8u));
        }
        return 1;
    }
    if (grid && address >= grid->identity && address - grid->identity < 0xf00cu) {
        size_t index;
        if (size > 0xf00cu - (address - grid->identity)) goto fail;
        for (index = 0; index < size; ++index) {
            uint32_t word;
            GameAddress cursor = address + (GameAddress)index;
            if (!gameplay_spatial_projection_read(cursor & ~3u, &word, 4u)) goto fail;
            ((unsigned char *)output)[index] = (unsigned char)(word >> ((cursor & 3u) * 8u));
        }
        return 1;
    }
    if(address>=LEVEL_ENTITY_VIEW_BASE && address<LEVEL_ENTITY_VIEW_END)
        return level_entity_store_read(address,output,size);
    if(address>=APPLICATION_NAME_VIEW_BASE && address<APPLICATION_NAME_VIEW_END)
        return application_name_registry_read(address,output,size);
    return scene_projection_storage_read(address, output, size);
fail:
    app->memory.fault = 1; app->memory.fault_address = address;
    app->memory.fault_size = size; app->memory.fault_write = 0; return 0;
}
static int owned_write(void *context, GameAddress address, const void *input, size_t size) {
    if(presentation_instance_projection_write(address,input,size) ||
       runtime_node_pool_projection_write(address,input,size))return 1;
    GameApplication *app = context;
    GameplaySpatialGrid *grid = gameplay_spatial_grid(0x5f0800u);
    if(address>=SCENE_SURFACE_BASE && address<SCENE_SURFACE_BASE+0x5000u){
        if(!scene_surface_write_pipeline(app->memory.surface,address,input,size))goto fail;return 1;
    }
    if(address>=GAME_TERRAIN_CELLS && address<GAME_TERRAIN_MATERIALS+0x80000u){
        if(!game_level_terrain_write(app,address,input,size))goto fail;
        return 1;
    }
    if(address>=RENDER_ARENA_BASE && address<RENDER_ARENA_END) {
        if(!render_arena_write(address,input,size))goto fail;
        return 1;
    }
    if(address>=RADAR_MAP_BASE && address<RADAR_MAP_END) {
        if(!game_radar_map_write(address,input,size))goto fail;
        return 1;
    }
    if(address>=0xfb000000u && address<0xfb070000u) {
        unsigned pool=(address-0xfb000000u)/0x10000u,unit=pool==6u?2u:4u;
        size_t offset=(address-0xfb000000u)%0x10000u,i,capacity=render_runtime_pool_size(pool);
        if(offset>capacity || size>capacity-offset)goto fail;
        for(i=0;i<size;++i) {
            size_t at=offset+i,aligned=at&~(size_t)(unit-1u);uint32_t bits=0,shift=(unsigned)(at%unit)*8u;
            uint16_t half;
            if(unit==2u) {
                if(!render_runtime_pool_read_native(pool,aligned,&half,2u))goto fail;
                bits=half;
            } else if(!render_runtime_pool_read_native(pool,aligned,&bits,4u))goto fail;
            bits=(bits&~(255u<<shift))|((uint32_t)((const uint8_t *)input)[i]<<shift);
            if(unit==2u) {
                half=(uint16_t)bits;
                if(!render_runtime_pool_write_native(pool,aligned,&half,2u))goto fail;
            } else if(!render_runtime_pool_write_native(pool,aligned,&bits,4u))goto fail;
        }
        return 1;
    }
    if (grid && address >= grid->identity && address - grid->identity < 0xf00cu) {
        size_t index;
        if (size > 0xf00cu - (address - grid->identity)) goto fail;
        for (index = 0; index < size; ++index) {
            uint32_t word, shift;
            GameAddress cursor = address + (GameAddress)index;
            shift = (cursor & 3u) * 8u;
            if (!gameplay_spatial_projection_read(cursor & ~3u, &word, 4u)) goto fail;
            word = (word & ~(255u << shift)) | (uint32_t)((const unsigned char *)input)[index] << shift;
            if (!gameplay_spatial_projection_write(cursor & ~3u, &word, 4u)) goto fail;
        }
        return 1;
    }
    if(address>=LEVEL_ENTITY_VIEW_BASE && address<LEVEL_ENTITY_VIEW_END)
        return level_entity_store_write(address,input,size);
    if(address>=APPLICATION_NAME_VIEW_BASE && address<APPLICATION_NAME_VIEW_END)
        return application_name_registry_write(address,input,size);
    return scene_projection_storage_write(address, input, size);
fail:
    app->memory.fault = 1; app->memory.fault_address = address;
    app->memory.fault_size = size; app->memory.fault_write = 1; return 0;
}

GameConfig *game_application_config(GameApplication *app){PortablePlatform *p=app->platform;return p?p->configuration:NULL;}
int game_application_config_flush(GameApplication *app){
    PortablePlatform *p=app->platform;char temporary[4140];FILE *file;int ok;
    if(!p || !p->configuration || !p->configuration_ready || !*p->configuration_path)return 1;
    if(!p->configuration_pending && !game_config_dirty(p->configuration))return 1;
    if(snprintf(temporary,sizeof(temporary),"%s.tmp",p->configuration_path)>=(int)sizeof(temporary))return 0;
    file=fopen(temporary,"wb");if(!file)return 0;
    ok=game_config_write(p->configuration,file);
    if(ok && _commit(_fileno(file)))ok=0;
    if(fclose(file))ok=0;
    if(!ok || !MoveFileExA(temporary,p->configuration_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        DeleteFileA(temporary);return 0;
    }
    game_config_saved(p->configuration);p->configuration_pending=0;return 1;
}
static int configuration_begin(GameApplication *app){
    PortablePlatform *p=app->platform;char directory[4128];FILE *file;int loaded=0;
    p->configuration=game_config_create();if(!p->configuration)return 0;
    if(snprintf(directory,sizeof(directory),"%s/CONFIG",p->user_directory)>=(int)sizeof(directory))return 0;
    if(!CreateDirectoryA(directory,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    if(snprintf(p->configuration_path,sizeof(p->configuration_path),"%s/settings.ini",directory)>=(int)sizeof(p->configuration_path))return 0;
    file=fopen(p->configuration_path,"rb");
    if(file){loaded=game_config_read(p->configuration,file);fclose(file);if(!loaded)return 0;}
    else {
        if(errno!=ENOENT)return 0;
        /* Isolated runs borrow the user's initial preferences once, then own
         * their independent CONFIG. There is no per-frame file syncing. */
        p->configuration_pending=1;file=fopen("CONFIG/settings.ini","rb");
        if(file){loaded=game_config_read(p->configuration,file);fclose(file);if(!loaded)return 0;}
    }
    if(!loaded){
        char line[128];unsigned lines=0;int value;
        file=fopen("uprising-recomp-settings.ini","rb");
        if(file){while(++lines<=128 && fgets(line,sizeof(line),file)){
            if(sscanf(line,"music_mode=%d",&value)==1 && value>=0 && value<=2)
                game_config_set_number(p->configuration,"music","mode",(uint32_t)value);
            else if(sscanf(line,"music_track=%d",&value)==1 && value>=1 && value<=99)
                game_config_set_number(p->configuration,"music","track",(uint32_t)value);
        }fclose(file);}
    }
    p->configuration_ready=game_config_set_number(p->configuration,"configuration","version",1);
    return p->configuration_ready;
}
int game_application_platform_start(GameApplication *app, void *instance,
    const char *command, int show) {
    const char *stage = "platform allocation";
    PortablePlatform *platform = calloc(1, sizeof(*platform));
    if (!platform) return 0;
    app->platform = platform;
    {const char *isolated=getenv("UPRISING_RECOMP_ISOLATED_INPUT");
     platform->isolated_input=isolated && !strcmp(isolated,"1");}
    platform->instance = instance;
    platform->command = malloc(strlen(command) + 1u);
    if (!platform->command) goto fail;
    strcpy(platform->command, command);
    {
        DWORD size=GetEnvironmentVariableA("UPRISING_USER_DIRECTORY",NULL,0);
        if(size) {
            if(size>4096u) goto fail;
            platform->user_directory=malloc(size);
            if(!platform->user_directory || !GetEnvironmentVariableA("UPRISING_USER_DIRECTORY",platform->user_directory,size)) goto fail;
        }
    }
    if(!platform->user_directory){
        char executable[4096],*slash;DWORD n=GetModuleFileNameA(instance,executable,sizeof(executable));
        if(!n || n>=sizeof(executable) || !(slash=strrchr(executable,'\\')))goto fail;
        *slash=0;
        if(!SetCurrentDirectoryA(executable))goto fail;
        platform->user_directory=malloc(strlen(executable)+1);
        if(!platform->user_directory)goto fail;
        strcpy(platform->user_directory,executable);
    }
    /* A normal double-click is interactive and windowed. Validation overrides
     * remain explicit; no flag or launcher is needed for safe desktop use. */
    if(!GetEnvironmentVariableA("UPRISING_RECOMP_HEADLESS",NULL,0) &&
       !GetEnvironmentVariableA("UPRISING_RECOMP_WINDOWED",NULL,0))
        SetEnvironmentVariableA("UPRISING_RECOMP_WINDOWED","1");
    if(GetEnvironmentVariableA("UPRISING_RECOMP_WINDOWED",NULL,0))
        SetEnvironmentVariableA("UPRISING_RECOMP_NATIVE_WINDOWED_DDRAW","1");
    if(!configuration_begin(app))goto fail;
    if(platform->user_directory){
        char log_path[4128];
        if(game_application_file_path(app,"uprising-portable.log",1,log_path,sizeof(log_path)))platform->log=fopen(log_path,"w");
    }else platform->log=fopen("uprising-portable.log","w");
    if(platform->log)setvbuf(platform->log,NULL,_IOLBF,0);
    if(platform->isolated_input)
        game_application_log(app,"native automated input isolation: %s\n","physical devices and desktop cursor excluded");
    stage = "source-defined initial records";
    if (!platform->log || !game_initial_data_create(&app->memory)) goto fail;
    app->memory.read_fallback = owned_read;
    app->memory.write_fallback = owned_write;
    app->memory.fallback_context = app;
    app->memory.owned_pages = projection_owner_pages();
    stage = "global constructors";
    if (!game_application_initialize_globals(app)) goto fail;
    stage = "platform startup";
    if (!game_application_startup(app, show)) goto fail;
    return 1;
fail:
    fprintf(stderr,"native startup failed: %s; memory fault=%d at %08lx size=%lu\n",
        stage, app->memory.fault, (unsigned long)app->memory.fault_address,
        (unsigned long)app->memory.fault_size);
    fflush(stderr);
    game_application_platform_stop(app);
    return 0;
}
int game_application_isolated_input(GameApplication *app) {
    PortablePlatform *platform=app->platform;return platform && platform->isolated_input;
}
int game_application_platform_stop(GameApplication *app) {
    PortablePlatform *platform = app->platform;
    int settings_ok=1;
    if(app->memory.data && !app->memory.fault)
        settings_ok=game_menu_settings_checkpoint(app);
    game_application_force_feedback_close(app,1);
    game_menu_movies_dispose(app);
    if(app->memory.data && !game_level_world_surface_release(app))game_application_log(app,"world surface cleanup","retirement failed");
    if(app->memory.data)(void)game_level_progress_end(app);
    if(app->memory.data)(void)game_level_interface_end(app);
    int ok = game_application_shutdown_globals(app) && settings_ok;
    {extern int game_level_routes_release(GameApplication *);
        if(!game_level_routes_release(app))ok=0;}
    game_application_ui_shutdown(app);
    game_menu_images_dispose(app);
    if(app->memory.data)(void)game_level_display_masks_release(app);
    game_input_devices_end(app);
    sound_platform_shutdown();
    render_runtime_pools_release();
    render_arena_release();
    game_application_window_shutdown(app);
    game_application_asset_shutdown(app);
    game_application_services_dispose(app);
    recyclable_record_shutdown();
    scene_projection_storage_release_all();
    if (platform) {
        if(!game_application_config_flush(app))ok=0;
        game_config_free(platform->configuration);
        while(platform->archives)(void)game_application_archive_close(app,platform->archives->identity);
        if (platform->message_hook) UnhookWindowsHookEx(platform->message_hook);
        if (platform->keyboard_hook) UnhookWindowsHookEx(platform->keyboard_hook);
        game_application_hooks_bind(NULL);
        if (platform->log) fclose(platform->log);
        free(platform->command);
        free(platform->user_directory);
        free(platform);
    }
    free(app->memory.data);
    memset(app, 0, sizeof(*app));
    return ok;
}

ResourceFile *game_application_settings_file(GameApplication *app,int write) {
    (void)app;if(write)return NULL;
    /* Read-only, one-time legacy migration. All new writes use CONFIG. */
    return resource_file_open("FONTS/settings.cfg");
}
ResourceFile *game_application_bindings_file(GameApplication *app,int write) {
    (void)app;if(write)return NULL;
    return resource_file_open("FONTS/keymap.cfg");
}

int game_application_startup(GameApplication *app, int show) {
    PortablePlatform *platform = app->platform;
    char executable[260];
    DWORD length, version = GetVersion();
    GameMemory *m = &app->memory;
    (void)show;
    game_memory_write_u32(m, 0x5a1c68u, version >> 16);
    game_memory_write_u32(m, 0x5a1c5cu, VER_PLATFORM_WIN32_NT);
    game_memory_write_u32(m, 0x5a1c74u, (version >> 8) & 255u);
    game_memory_write_u32(m, 0x5a1c70u, version & 255u);
    game_memory_write_u32(m, 0x5a1c6cu,
        ((version & 255u) << 8) + ((version >> 8) & 255u));
    if (!game_application_directory(app, "saves") &&
        game_memory_read_u32(m, 0x5a1c5cu) == 2u) return 0;
    if (!GetSystemMetrics(SM_MOUSEPRESENT)) {
        game_application_error(app, "A mouse is required to play Uprising.", "Uprising");
        return 0;
    }
    game_memory_write_u32(m, 0x597084u, 0u);
    /* Retail 00505685 jumps unconditionally over the old CD retry loop. */
    length = GetModuleFileNameA(platform->instance, executable, sizeof(executable));
    if (!length || length >= sizeof(executable)) return 0;
    if (!game_memory_write(m, 0x5eff38u, executable, length + 1u) ||
        !game_application_asset_bootstrap(app, platform->command)) return 0;
    /*00540590 reserved the optional guest heap used by53ff70/540170. Native
     *records, images and models already allocate through their existing owned
     *stores. Shipping has no guest allocator or arena consumers; reserving a
     *second15MiB heap would create an unused competing allocation owner. */
    game_application_hooks_bind(app);
    platform->message_hook = SetWindowsHookExA(WH_CBT,
        game_application_message_hook, NULL, GetCurrentThreadId());
    platform->keyboard_hook = SetWindowsHookExA(WH_GETMESSAGE,
        game_application_keyboard_hook, NULL, GetCurrentThreadId());
    return !m->fault;
}
void *game_application_instance(GameApplication *app) {
    return ((PortablePlatform *)app->platform)->instance;
}
int game_application_audio_muted(GameApplication *app) {
    (void)app;
    return GetEnvironmentVariableA("UPRISING_RECOMP_HEADLESS",NULL,0)!=0 ||
        GetEnvironmentVariableA("UPRISING_RECOMP_MUTE",NULL,0)!=0;
}
const char *game_application_command_line(GameApplication *app) {
    return ((PortablePlatform *)app->platform)->command;
}
void game_application_sleep(GameApplication *app, unsigned milliseconds) {
    (void)app; Sleep(milliseconds);
}
int game_application_directory(GameApplication *app, const char *path) {
    DWORD attributes;
    PortablePlatform *platform=app?app->platform:NULL;
    char resolved[4128];
    /*Startup's saves/scrshts directories must match the actual output-file
     *owner. Creating them in the asset working directory strands isolated
     *campaign saves even though ordinary root-level save files succeed. */
    if(platform && platform->user_directory){
        if(!game_application_file_path(app,path,1,resolved,sizeof(resolved)))return 0;
        path=resolved;
    }
    if (CreateDirectoryA(path, NULL)) return 1;
    if (GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
int game_application_error(GameApplication *app, const char *message, const char *title) {
    if(GetEnvironmentVariableA("UPRISING_RECOMP_HEADLESS",NULL,0)){
        fprintf(stderr,"%s: %s\n",title?title:"native game",message?message:"");fflush(stderr);
        if(app && app->platform)game_application_log(app,"%s\n",message?message:"");
        return 0;
    }
    MessageBoxA(NULL, message, title, 0);return 0;
}
int game_application_output_directory(GameApplication *app,const char *path){
    char resolved[4128];
    return game_application_file_path(app,path,1,resolved,sizeof(resolved)) &&
        (CreateDirectoryA(resolved,NULL)!=0 || GetLastError()==ERROR_ALREADY_EXISTS);
}
void game_application_log(GameApplication *app, const char *format, const char *argument) {
    FILE *log = ((PortablePlatform *)app->platform)->log;
    if (log) {fprintf(log, format, argument);fflush(log);}
}
int game_application_calendar(GameApplication *app, int32_t calendar[9]) {
    time_t now = time(NULL);
    struct tm *date = localtime(&now);
    (void)app;
    if (!date) return 0;
    calendar[0] = date->tm_sec; calendar[1] = date->tm_min;
    calendar[2] = date->tm_hour; calendar[3] = date->tm_mday;
    calendar[4] = date->tm_mon + 1; calendar[5] = date->tm_year;
    calendar[6] = date->tm_wday; calendar[7] = date->tm_yday;
    calendar[8] = date->tm_isdst;
    return 1;
}
void game_application_registry_prepare(GameApplication *app) {
    static const char *names[] = {"Software", "Microsoft", "Windows", "CurrentVersion"};
    HKEY key = HKEY_LOCAL_MACHINE, next = NULL;
    unsigned index;
    (void)app;
    for (index = 0; index < sizeof(names)/sizeof(names[0]); ++index) {
        if (RegOpenKeyExA(key, names[index], 0, KEY_ALL_ACCESS, &next) != ERROR_SUCCESS) break;
        if (key != HKEY_LOCAL_MACHINE) RegCloseKey(key);
        key = next;
    }
    if (key != HKEY_LOCAL_MACHINE) RegCloseKey(key);
}
