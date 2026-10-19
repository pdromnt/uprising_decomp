// Runtime metadata comes from the exact compiled SPIR-V, never handwritten
// binding tables. This profile is LDR, inverted depth, low-resolution vectors,
// FP32 and native subgroup width; unsupported permutations fail explicitly.
#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <ffx_shader_blobs.h>
#include <spirv_reflect.h>
#include <new>
#include <mutex>
#include <cstdio>
#include "fsr3_prepare_inputs_spv.h"
#include "fsr3_luma_pyramid_spv.h"
#include "fsr3_shading_change_pyramid_spv.h"
#include "fsr3_shading_change_spv.h"
#include "fsr3_prepare_reactivity_spv.h"
#include "fsr3_luma_instability_spv.h"
#include "fsr3_accumulate_spv.h"
#include "fsr3_accumulate_sharpen_spv.h"
#include "fsr3_rcas_spv.h"
#include "fsr3_debug_view_spv.h"
#include "fsr3_autogen_reactive_spv.h"
namespace {
struct Bindings {uint32_t size=0,slots[64]{},counts[64]{},spaces[64]{};const char *names[64]{};};
struct Shader {
    const uint32_t *data;uint32_t bytes;SpvReflectShaderModule module{};
    Bindings groups[4];std::once_flag once;bool valid=false;
    Shader(const uint32_t *p,uint32_t n):data(p),bytes(n){}
    ~Shader(){if(module._internal)spvReflectDestroyShaderModule(&module);}
    void reflect(){
        if(spvReflectCreateShaderModule(bytes,data,&module)!=SPV_REFLECT_RESULT_SUCCESS)return;
        for(uint32_t i=0;i<module.descriptor_binding_count;i++){
            const auto &b=module.descriptor_bindings[i];unsigned group;
            switch(b.descriptor_type){
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:group=0;break;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:group=1;break;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:group=2;break;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:group=3;break;
            default:std::fprintf(stderr,"FSR3 unsupported descriptor %u\n",b.descriptor_type);return;
            }
            auto &g=groups[group];if(g.size==64 || b.set || !b.count || !b.name || !*b.name)return;
            unsigned n=g.size++;g.names[n]=b.name;g.slots[n]=b.binding;g.counts[n]=b.count;g.spaces[n]=b.set;
        }
        valid=true;
    }
};
#define SHADER(name) {fsr3_##name##_spv,sizeof(fsr3_##name##_spv)}
Shader shaders[]={SHADER(prepare_inputs),SHADER(luma_pyramid),SHADER(shading_change_pyramid),
    SHADER(shading_change),SHADER(prepare_reactivity),SHADER(luma_instability),SHADER(accumulate),
    SHADER(accumulate_sharpen),SHADER(rcas),SHADER(debug_view),SHADER(autogen_reactive)};
}
extern "C" FfxErrorCode ffxGetPermutationBlobByIndex(FfxEffect effect,FfxPass pass,FfxBindStage stage,uint32_t options,FfxShaderBlob *out){
    if(!out || effect!=FFX_EFFECT_FSR3UPSCALER || stage!=FFX_BIND_COMPUTE_SHADER_STAGE ||
       pass>=sizeof(shaders)/sizeof(shaders[0]) || options!=(20u|(pass==FFX_FSR3UPSCALER_PASS_ACCUMULATE_SHARPEN?32u:0u))){
        std::fprintf(stderr,"FSR3 unsupported shader effect=%u pass=%u options=%u\n",effect,pass,options);return FFX_ERROR_INVALID_ARGUMENT;
    }
    Shader &s=shaders[pass];std::call_once(s.once,[&s]{s.reflect();});if(!s.valid)return FFX_ERROR_BACKEND_API_ERROR;
    auto &c=s.groups[0],&r=s.groups[1],&w=s.groups[2],&a=s.groups[3];
    new(out) FfxShaderBlob{reinterpret_cast<const uint8_t*>(s.data),s.bytes,c.size,r.size,w.size,0,0,a.size,0,
        c.names,c.slots,c.counts,c.spaces,r.names,r.slots,r.counts,r.spaces,w.names,w.slots,w.counts,w.spaces,
        nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,a.names,a.slots,a.counts,a.spaces,
        nullptr,nullptr,nullptr,nullptr};
    return FFX_OK;
}
extern "C" FfxErrorCode ffxIsWave64(FfxEffect effect,uint32_t options,bool &wave64){
    if(effect!=FFX_EFFECT_FSR3UPSCALER)return FFX_ERROR_INVALID_ARGUMENT;
    wave64=(options&64u)!=0;return FFX_OK;
}
