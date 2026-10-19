# Reliable ordered datagrams and portable socket primitives; no SDK/COM runtime.
FetchContent_Declare(enet
    URL https://codeload.github.com/lsalzman/enet/tar.gz/refs/tags/v1.3.18
    URL_HASH SHA256=28603c895f9ed24a846478180ee72c7376b39b4bb1287b73877e5eae7d96b0dd
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/enet-src"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/enet")
FetchContent_MakeAvailable(enet)
target_include_directories(enet PUBLIC "${enet_SOURCE_DIR}/include")
