set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_ENABLE_ERROR_STRINGS ON CACHE BOOL "" FORCE)
FetchContent_Declare(freetype
    URL
        https://download-mirror.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz
        https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz
        https://downloads.sourceforge.net/project/freetype/freetype2/2.14.3/freetype-2.14.3.tar.xz
    URL_HASH SHA256=36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f
    DOWNLOAD_DIR "${FETCHCONTENT_BASE_DIR}"
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/freetype-src"
    SUBBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/freetype-subbuild"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/freetype")
FetchContent_MakeAvailable(freetype)
