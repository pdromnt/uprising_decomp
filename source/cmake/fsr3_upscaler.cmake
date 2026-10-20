enable_language(CXX)
include("${CMAKE_CURRENT_LIST_DIR}/fsr3_sdk.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/spirv_reflect_sources.cmake")
set(reflect_source "${FETCHCONTENT_BASE_DIR}/spirv-reflect-src")
foreach(entry IN LISTS SPIRV_REFLECT_SOURCES)
    string(REPLACE "|" ";" pair "${entry}")
    list(GET pair 0 path)
    list(GET pair 1 hash)
    set(file "${reflect_source}/${path}")
    set(download TRUE)
    if(EXISTS "${file}")
        file(SHA256 "${file}" actual)
        if(actual STREQUAL hash)
            set(download FALSE)
        endif()
    endif()
    if(download)
        get_filename_component(directory "${file}" DIRECTORY)
        file(MAKE_DIRECTORY "${directory}")
        file(DOWNLOAD "https://raw.githubusercontent.com/KhronosGroup/SPIRV-Reflect/vulkan-sdk-1.3.296.0/${path}"
            "${file}" EXPECTED_HASH "SHA256=${hash}" TLS_VERIFY ON)
    endif()
endforeach()
set(fsr3_sdk "${fsr3_source}/sdk")
# Replace only the SDK's discrete-only memory selector in a derived build file.
# Original downloaded bytes and their checksum remain untouched.
file(READ "${fsr3_sdk}/src/backends/vk/ffx_vk.cpp" backend)
string(FIND "${backend}" "uint32_t findMemoryTypeIndex(" memory_begin)
string(FIND "${backend}" "VkBufferUsageFlags ffxGetVKBufferUsageFlagsFromResourceUsage(" memory_end)
if(memory_begin LESS 0 OR memory_end LESS memory_begin)
    message(FATAL_ERROR "Pinned SDK memory selector changed")
endif()
math(EXPR memory_length "${memory_end}-${memory_begin}")
string(SUBSTRING "${backend}" ${memory_begin} ${memory_length} original_memory)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/platform/vulkan/fsr3_memory.inc" native_memory)
string(REPLACE "${original_memory}" "${native_memory}\n" backend "${backend}")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr3_backend_native.inc" CONTENT "${backend}" @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/platform/vulkan/fsr3_memory.inc")
set(fsr3_headers)
set(fsr3_passes prepare_inputs luma_pyramid shading_change_pyramid shading_change
    prepare_reactivity luma_instability accumulate accumulate_sharpen rcas debug_view autogen_reactive)
foreach(pass IN LISTS fsr3_passes)
    set(shader_pass "${pass}")
    set(sharpen 0)
    if(pass STREQUAL "accumulate_sharpen")
        set(shader_pass accumulate)
        set(sharpen 1)
    endif()
    set(shader "${fsr3_sdk}/src/backends/vk/shaders/fsr3upscaler/ffx_fsr3upscaler_${shader_pass}_pass.glsl")
    set(binary "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr3_${pass}.spv")
    set(header "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr3_${pass}_spv.h")
    add_custom_command(OUTPUT "${header}"
        COMMAND "${UPRISING_GLSLANG}" -V -Os --target-env vulkan1.2 -S comp -DFFX_GPU=1 -DFFX_GLSL=1 -DFFX_HALF=0
            -DFFX_FSR3UPSCALER_OPTION_LOW_RESOLUTION_MOTION_VECTORS=1 -DFFX_FSR3UPSCALER_OPTION_INVERTED_DEPTH=1
            -DFFX_FSR3UPSCALER_OPTION_HDR_COLOR_INPUT=0 -DFFX_FSR3UPSCALER_OPTION_JITTERED_MOTION_VECTORS=0
            -DFFX_FSR3UPSCALER_OPTION_REPROJECT_USE_LANCZOS_TYPE=0 "-DFFX_FSR3UPSCALER_OPTION_APPLY_SHARPENING=${sharpen}"
            "-I${fsr3_sdk}/include/FidelityFX/gpu" "${shader}" -o "${binary}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${binary}" "-DOUTPUT=${header}" "-DSYMBOL=fsr3_${pass}_spv"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        DEPENDS ${fsr3_dependencies} cmake/embed_spirv.cmake VERBATIM)
    list(APPEND fsr3_headers "${header}")
endforeach()
add_library(uprising_fsr3 STATIC platform/vulkan/fsr3_backend.cpp platform/vulkan/fsr3_upscaler.cpp
    platform/vulkan/fsr3_shaders.cpp "${reflect_source}/spirv_reflect.c"
    "${fsr3_sdk}/src/components/fsr3upscaler/ffx_fsr3upscaler.cpp"
    "${fsr3_sdk}/src/shared/ffx_assert.cpp" "${fsr3_sdk}/src/shared/ffx_object_management.cpp"
    "${fsr3_sdk}/src/shared/ffx_message.cpp"
    "${fsr3_sdk}/src/shared/ffx_breadcrumbs_list.cpp" ${fsr3_headers})
target_compile_features(uprising_fsr3 PRIVATE cxx_std_17)
if(NOT WIN32)
    target_compile_options(uprising_fsr3 PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-include>"
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/platform/vulkan/fsr3_portability.h>"
        "$<$<COMPILE_LANGUAGE:CXX>:-g1>")
endif()
target_include_directories(uprising_fsr3 PRIVATE "${fsr3_sdk}/include" "${fsr3_sdk}/src/shared"
    "${fsr3_sdk}/src/backends/shared" "${fsr3_sdk}/src/backends/vk" "${reflect_source}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated" "${CMAKE_CURRENT_SOURCE_DIR}/platform/vulkan")
target_link_libraries(uprising_fsr3 PUBLIC Vulkan::Headers SDL3::SDL3-static)
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(uprising_fsr3 INTERFACE -static-libgcc -static-libstdc++)
endif()
target_link_libraries(uprising_vulkan PUBLIC uprising_fsr3)
