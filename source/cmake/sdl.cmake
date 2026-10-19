# One downloaded source tree shared by the two local architecture caches.
# Static linking keeps architecture-specific SDL DLLs out of the asset folder.
set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/dependencies" CACHE PATH "Shared dependency sources" FORCE)
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
    URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/sdl3-src"
    SUBBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/sdl3-subbuild"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/sdl3")
FetchContent_MakeAvailable(SDL3)
# Apply on configure as well as fresh downloads: local x86/x64 caches share
# this source tree. The patch leaves an already-patched file untouched.
include("${CMAKE_CURRENT_LIST_DIR}/patch-sdl-xbox-ble.cmake")
uprising_patch_sdl_xbox_ble("${sdl3_SOURCE_DIR}")
