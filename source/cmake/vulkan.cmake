# Optional native Vulkan backend; no dependency in the stable OpenGL build.
FetchContent_Declare(VulkanHeaders
    URL https://codeload.github.com/KhronosGroup/Vulkan-Headers/tar.gz/refs/tags/v1.3.296
    URL_HASH SHA256=e204e0b3c19f622d197df945737f5db913d6621830999b8578d34e80a7c90585
    DOWNLOAD_DIR "${FETCHCONTENT_BASE_DIR}"
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/vulkan-headers-src"
    SUBBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/vulkan-headers-subbuild"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/vulkan-headers")
FetchContent_MakeAvailable(VulkanHeaders)
find_program(UPRISING_GLSLANG NAMES glslang glslangValidator REQUIRED)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(graphics_headers)
foreach(stage vert frag)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/platform/vulkan/projected.${stage}")
    set(binary "${CMAKE_CURRENT_BINARY_DIR}/generated/projected_${stage}.spv")
    set(header "${CMAKE_CURRENT_BINARY_DIR}/generated/projected_${stage}_spv.h")
    add_custom_command(OUTPUT "${header}"
        COMMAND "${UPRISING_GLSLANG}" -V --target-env vulkan1.2 "${source}" -o "${binary}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${binary}" "-DOUTPUT=${header}" "-DSYMBOL=projected_${stage}_spv" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        DEPENDS "${source}" cmake/embed_spirv.cmake VERBATIM)
    list(APPEND graphics_headers "${header}")
endforeach()
foreach(pass mask compose)
    set(temporal_source "${CMAKE_CURRENT_SOURCE_DIR}/platform/vulkan/temporal_${pass}.comp")
    set(temporal_binary "${CMAKE_CURRENT_BINARY_DIR}/generated/temporal_${pass}.spv")
    set(temporal_header "${CMAKE_CURRENT_BINARY_DIR}/generated/temporal_${pass}_spv.h")
    add_custom_command(OUTPUT "${temporal_header}"
        COMMAND "${UPRISING_GLSLANG}" -V --target-env vulkan1.2 "${temporal_source}" -o "${temporal_binary}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${temporal_binary}" "-DOUTPUT=${temporal_header}" "-DSYMBOL=temporal_${pass}_spv" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        DEPENDS "${temporal_source}" cmake/embed_spirv.cmake VERBATIM)
    list(APPEND graphics_headers "${temporal_header}")
endforeach()
add_library(uprising_vulkan STATIC platform/vulkan/vulkan_device.c platform/vulkan/vulkan_draw.c platform/vulkan/vulkan_present.c platform/vulkan/vulkan_menu.c platform/vulkan/vulkan_temporal.c ${graphics_headers})
target_include_directories(uprising_vulkan PUBLIC platform/vulkan PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(uprising_vulkan PUBLIC SDL3::SDL3-static Vulkan::Headers)
if(NOT MSVC)
    target_compile_options(uprising_vulkan PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    include("${CMAKE_CURRENT_LIST_DIR}/fsr3_upscaler.cmake")
    target_compile_definitions(uprising_vulkan PUBLIC UPRISING_FSR3_UPSCALER=1)
else()
    message(STATUS "32-bit target: native Vulkan only; FSR3 is a 64-bit feature")
endif()
