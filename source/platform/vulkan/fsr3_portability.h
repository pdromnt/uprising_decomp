#ifndef UPRISING_FSR3_PORTABILITY_H
#define UPRISING_FSR3_PORTABILITY_H
// The SDK's selected host closure uses Microsoft CRT helpers unconditionally.
// Keep their bounded behavior inside the adapter, without changing SDK bytes.
#ifndef _WIN32
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cstdarg>
#include <cerrno>
#include <locale>
#include <FidelityFX/host/ffx_types.h>
// Wide debug/binding names enlarge the private context on Unix. Reserve enough
// opaque backing storage in every SDK translation unit; retain its size assert.
static_assert(sizeof(wchar_t)==2 || sizeof(wchar_t)==4,"Unsupported SDK wide-character width");
#undef FFX_SDK_DEFAULT_CONTEXT_SIZE
#define FFX_SDK_DEFAULT_CONTEXT_SIZE (1024 * 128 * (sizeof(wchar_t) / 2))
template<typename T,size_t N> constexpr size_t _countof(T (&)[N]) noexcept {return N;}
inline int strcpy_s(char *out,size_t capacity,const char *in){
    if(!out || !capacity || !in)return EINVAL;
    size_t count=std::strlen(in);if(count>=capacity){out[0]=0;return ERANGE;}
    std::memcpy(out,in,count+1);return 0;
}
inline int wcscpy_s(wchar_t *out,size_t capacity,const wchar_t *in){
    if(!out || !capacity || !in)return EINVAL;
    size_t count=std::wcslen(in);if(count>=capacity){out[0]=0;return ERANGE;}
    std::wmemcpy(out,in,count+1);return 0;
}
template<size_t N> int strcpy_s(char (&out)[N],const char *in){return strcpy_s(out,N,in);}
template<size_t N> int wcscpy_s(wchar_t (&out)[N],const wchar_t *in){return wcscpy_s(out,N,in);}
inline int sprintf_s(char *out,size_t capacity,const char *format,...){
    if(!out || !capacity || !format)return 0;
    va_list args;va_start(args,format);int written=std::vsnprintf(out,capacity,format,args);va_end(args);
    // Breadcrumb diagnostics use this as a copy length: never exceed storage.
    return written<0?0:static_cast<size_t>(written)>=capacity?static_cast<int>(capacity-1):written;
}
#endif
#endif
