cmake_minimum_required(VERSION 3.24)
project(Uprising VERSION 0.1.0 LANGUAGES C RC)

if(NOT WIN32 OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "The current validated backend needs Windows and MinGW-w64 GCC. Other platforms/backends are future work.")
endif()
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(UPRISING_ASSET_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Output folder containing the user's game assets")
get_filename_component(UPRISING_ASSET_DIRECTORY "${UPRISING_ASSET_DIRECTORY}" ABSOLUTE)
include(cmake/sources.cmake)
include(cmake/sdl.cmake)
include(cmake/enet.cmake)
include(cmake/freetype.cmake)

foreach(path IN LISTS CORE_SOURCES GAME_SOURCES PLATFORM_SOURCES)
    if(path MATCHES "(^|/)(cpu|generated|overlay)/")
        message(FATAL_ERROR "Forbidden execution source: ${path}")
    endif()
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${path}" source_text)
    if(source_text MATCHES "#[ \t]*include[ \t]*[\"<](cpu|x87)\\.h")
        message(FATAL_ERROR "CPU dependency in shipping source: ${path}")
    endif()
endforeach()

add_library(uprising_core OBJECT ${CORE_SOURCES})
target_include_directories(uprising_core PUBLIC core platform)
target_compile_options(uprising_core PRIVATE -O2 -frounding-math -ffunction-sections -fdata-sections -Wall -Wextra)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(resource_target pe-x86-64)
    set(default_executable_name uprising64)
else()
    set(resource_target pe-i386)
    set(default_executable_name uprising)
endif()
set(UPRISING_EXECUTABLE_NAME "${default_executable_name}" CACHE STRING "Output basename; CI packages use uprising for either architecture")
if(NOT UPRISING_EXECUTABLE_NAME MATCHES "^uprising(64)?$")
    message(FATAL_ERROR "Unsupported executable name")
endif()
configure_file(resources/version.h.in generated/version.h @ONLY)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/uprising.res" "${CMAKE_CURRENT_BINARY_DIR}/generated/uprising_resources.o"
    COMMAND "${CMAKE_RC_COMPILER}" --input-format=rc --output-format=res
        "-I${CMAKE_CURRENT_SOURCE_DIR}/resources" "-I${CMAKE_CURRENT_BINARY_DIR}/generated"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/uprising.rc" "${CMAKE_CURRENT_BINARY_DIR}/generated/uprising.res"
    COMMAND "${CMAKE_RC_COMPILER}" --input-format=res --output-format=coff --target=${resource_target}
        "${CMAKE_CURRENT_BINARY_DIR}/generated/uprising.res" "${CMAKE_CURRENT_BINARY_DIR}/generated/uprising_resources.o"
    DEPENDS resources/uprising.rc resources/uprising.ico resources/version.h.in
        "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h"
    VERBATIM)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/generated/uprising_resources.o" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
add_executable(uprising WIN32 ${GAME_SOURCES} ${PLATFORM_SOURCES}
    third_party/libsmacker/smacker.c "${CMAKE_CURRENT_BINARY_DIR}/generated/uprising_resources.o")
set_target_properties(uprising PROPERTIES OUTPUT_NAME "${UPRISING_EXECUTABLE_NAME}")
target_include_directories(uprising PRIVATE core game platform)

target_compile_options(uprising PRIVATE -O2 -frounding-math -ffunction-sections -fdata-sections -Wall -Wextra)
target_link_options(uprising PRIVATE -Wl,--gc-sections)
target_link_libraries(uprising PRIVATE uprising_core SDL3::SDL3-static enet freetype m user32 gdi32 advapi32 opengl32)
set_target_properties(uprising PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${UPRISING_ASSET_DIRECTORY}")
foreach(configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(uprising PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${configuration} "${UPRISING_ASSET_DIRECTORY}")
endforeach()

option(BUILD_TESTING "Build asset-free native checks" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_executable(core_smoke tests/core_smoke.c)
    target_link_libraries(core_smoke PRIVATE uprising_core m)
    add_test(NAME native_core COMMAND core_smoke)
    add_executable(config_test tests/config.c core/game_config.c)
    target_include_directories(config_test PRIVATE core)
    add_test(NAME configuration COMMAND config_test)
    add_executable(audio_test tests/audio.c)
    target_include_directories(audio_test PRIVATE core game platform)
    target_link_libraries(audio_test PRIVATE SDL3::SDL3-static m)
    target_compile_options(audio_test PRIVATE -UNDEBUG)
    add_test(NAME audio_mixer COMMAND audio_test)
endif()
