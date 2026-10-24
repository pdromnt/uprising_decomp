# Isolated dependency proof; uses the same backend as the optional game target.
if(NOT TARGET uprising_vulkan)
    include("${CMAKE_CURRENT_LIST_DIR}/vulkan.cmake")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/fsr3_sdk.cmake")
set(shader "${fsr3_source}/sdk/src/backends/vk/shaders/fsr3upscaler/ffx_fsr3upscaler_autogen_reactive_pass.glsl")
set(spirv "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr3_reactive.spv")
set(embedded "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr3_reactive_spv.h")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(OUTPUT "${embedded}"
    COMMAND "${UPRISING_GLSLANG}" -V --target-env vulkan1.2 -S comp -DFFX_GPU=1 -DFFX_GLSL=1 -DFFX_HALF=0
        "-I${fsr3_source}/sdk/include/FidelityFX/gpu" "${shader}" -o "${spirv}"
    COMMAND "${CMAKE_COMMAND}" "-DINPUT=${spirv}" "-DOUTPUT=${embedded}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
    DEPENDS ${fsr3_dependencies} cmake/embed_spirv.cmake VERBATIM)

add_executable(uprising_vulkan_probe tests/vulkan_probe.c "${embedded}")
target_include_directories(uprising_vulkan_probe PRIVATE platform/vulkan "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(uprising_vulkan_probe PRIVATE uprising_vulkan)
if(NOT MSVC)
    target_compile_options(uprising_vulkan_probe PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
    target_link_libraries(uprising_vulkan_probe PRIVATE m)
endif()
# Keep every probe artifact in its build cache, away from released executables.
set_target_properties(uprising_vulkan_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
foreach(configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(uprising_vulkan_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${configuration} "${CMAKE_CURRENT_BINARY_DIR}/bin")
endforeach()
