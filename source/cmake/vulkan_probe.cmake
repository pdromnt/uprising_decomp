# Isolated dependency proof. It neither links nor replaces the playable game.
FetchContent_Declare(VulkanHeaders
    URL https://codeload.github.com/KhronosGroup/Vulkan-Headers/tar.gz/refs/tags/v1.3.296
    URL_HASH SHA256=e204e0b3c19f622d197df945737f5db913d6621830999b8578d34e80a7c90585
    DOWNLOAD_DIR "${FETCHCONTENT_BASE_DIR}"
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/vulkan-headers-src"
    SUBBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/vulkan-headers-subbuild"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/vulkan-headers")
FetchContent_MakeAvailable(VulkanHeaders)
find_program(UPRISING_GLSLANG NAMES glslang glslangValidator REQUIRED)
include("${CMAKE_CURRENT_LIST_DIR}/fsr3_probe_sources.cmake")
set(fsr3_source "${FETCHCONTENT_BASE_DIR}/fsr3-probe-src")
foreach(entry IN LISTS FSR3_PROBE_SOURCES)
    string(REPLACE "|" ";" pair "${entry}")
    list(GET pair 0 path)
    list(GET pair 1 hash)
    set(file "${fsr3_source}/${path}")
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
        file(DOWNLOAD "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/${FSR3_PROBE_REVISION}/${path}"
            "${file}" EXPECTED_HASH "SHA256=${hash}" TLS_VERIFY ON)
    endif()
    list(APPEND fsr3_dependencies "${file}")
endforeach()
set(shader "${fsr3_source}/sdk/src/backends/vk/shaders/fsr3upscaler/ffx_fsr3upscaler_autogen_reactive_pass.glsl")
set(spirv "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr3_reactive.spv")
set(embedded "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr3_reactive_spv.h")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(OUTPUT "${embedded}"
    COMMAND "${UPRISING_GLSLANG}" -V --target-env vulkan1.2 -S comp -DFFX_GPU=1 -DFFX_GLSL=1 -DFFX_HALF=0
        "-I${fsr3_source}/sdk/include/FidelityFX/gpu" "${shader}" -o "${spirv}"
    COMMAND "${CMAKE_COMMAND}" "-DINPUT=${spirv}" "-DOUTPUT=${embedded}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
    DEPENDS ${fsr3_dependencies} cmake/embed_spirv.cmake VERBATIM)
add_executable(uprising_vulkan_probe tests/vulkan_probe.c platform/vulkan/vulkan_device.c "${embedded}")
target_include_directories(uprising_vulkan_probe PRIVATE platform/vulkan "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(uprising_vulkan_probe PRIVATE SDL3::SDL3-static Vulkan::Headers)
if(NOT MSVC)
    target_compile_options(uprising_vulkan_probe PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
    target_link_libraries(uprising_vulkan_probe PRIVATE m)
endif()
# Keep every probe artifact in its build cache, away from released executables.
set_target_properties(uprising_vulkan_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
foreach(configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(uprising_vulkan_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${configuration} "${CMAKE_CURRENT_BINARY_DIR}/bin")
endforeach()
