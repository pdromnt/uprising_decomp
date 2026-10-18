#ifndef UPRISING_PORTABLE_MODEL_ASSETS_H
#define UPRISING_PORTABLE_MODEL_ASSETS_H
#include "application_entry.h"
#include "scene_asset_loader.h"
#include "spatial_sample.h"
#include "texture_store.h"
GameAddress game_model_file_load(GameApplication *,const char *,uint32_t);
int game_model_graph_update(GameApplication *,GameAddress);
int game_model_bitmap_material_load(GameApplication *,const char *,int,uint32_t,MaterialValue *);
int game_model_cached_material_load(GameApplication *,const char *,int,uint32_t,MaterialValue *);
GameAddress game_model_mesh_project(GameApplication *,SceneMesh *,uint32_t,SceneMeshProjection *);
int game_application_scene_model_release(GameApplication *,GameAddress);
void game_model_assets_dispose(GameApplication *);
/*Factory templates/clones must be retired before their shared recipe pages. */
int game_model_factory_parts_release(GameApplication *);
/*Texture pixels belong to the application's existing native TextureStore;
 * backends borrow this record and use its fixed-width identity. */
const TextureRecord *game_model_texture(GameApplication *,uint32_t);
TextureRecord *game_model_texture_record(GameApplication *,uint32_t);
void game_model_texture_residency_clear(GameApplication *);
int game_model_texture_release(GameApplication *,uint32_t);
/*Transfers decoded pixels to the existing application texture owner. */
int game_model_texture_take(GameApplication *,AssetTexture *,uint32_t,MaterialValue *);
int game_model_material_load(GameApplication *,const char *,int,uint32_t,int,MaterialValue *);
GameAddress game_model_material_bind(GameApplication *,const char *,int,MaterialValue *);
int game_model_software_material_load(GameApplication *,const char *,MaterialValue *);
GameAddress game_model_file_three_load(GameApplication *,const ResourceBlob *,uint32_t);
/*Real binary definition loading and its shared native child cache. */
int game_model_asset_definition_load(GameApplication *,const SceneAssetRequest *);
typedef struct GameModelDefinition {
    AssetModelFormat format;
    float dimensions[3];uint32_t radius;
    uint32_t marker_count,point_count,secondary_count,part_count;
    GameAddress markers,points,secondary_points,parts,model;
} GameModelDefinition;
/*449290/44ab90: decoded inputs transfer into the existing presentation owner. */
int game_model_definition_read(GameApplication *,AssetModelFormat,uint16_t,GameAddress,int,int,int,GameModelDefinition *);
int game_model_definition_release(GameApplication *,GameModelDefinition *);
int game_level_spatial_describe(GameApplication *,uint16_t,SpatialSampleDefinition *);
int game_level_articulated_describe(GameApplication *,uint16_t,PresentationArticulatedDefinition *);
/*Actual SDF/ASC fallback transfers results into the same native owners. */
int game_model_text_definition_load(GameApplication *,AssetModelFormat,uint16_t,GameAddress,int,int,int,GameModelDefinition *);
int game_level_texture_profiles_initialize(GameApplication *); /*5566f0*/
int game_level_texture_profiles_release(GameApplication *); /*556430*/
#endif
