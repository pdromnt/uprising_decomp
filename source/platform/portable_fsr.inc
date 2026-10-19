/* AMD FSR1, full-precision EASU then RCAS. Presentation only; the native
 * renderer and its color/depth/CPU readback owners keep their original sizes. */
#include "fsr_sources.h"
#define DISPLAY_GL_SHADER_FUNCTIONS(X) \
 X(PFNGLCREATESHADERPROC,CreateShader) X(PFNGLSHADERSOURCEPROC,ShaderSource) \
 X(PFNGLCOMPILESHADERPROC,CompileShader) X(PFNGLGETSHADERIVPROC,GetShaderiv) \
 X(PFNGLGETSHADERINFOLOGPROC,GetShaderInfoLog) X(PFNGLDELETESHADERPROC,DeleteShader) \
 X(PFNGLCREATEPROGRAMPROC,CreateProgram) X(PFNGLATTACHSHADERPROC,AttachShader) \
 X(PFNGLLINKPROGRAMPROC,LinkProgram) X(PFNGLGETPROGRAMIVPROC,GetProgramiv) \
 X(PFNGLGETPROGRAMINFOLOGPROC,GetProgramInfoLog) X(PFNGLDELETEPROGRAMPROC,DeleteProgram) \
 X(PFNGLUSEPROGRAMPROC,UseProgram) X(PFNGLGETUNIFORMLOCATIONPROC,GetUniformLocation) \
 X(PFNGLUNIFORM1IPROC,Uniform1i) X(PFNGLUNIFORM2FPROC,Uniform2f)
#define DISPLAY_GL_TARGET_FUNCTIONS(X) \
 X(PFNGLGENFRAMEBUFFERSPROC,GenFramebuffers) X(PFNGLBINDFRAMEBUFFERPROC,BindFramebuffer) \
 X(PFNGLFRAMEBUFFERTEXTURE2DPROC,FramebufferTexture2D) \
 X(PFNGLCHECKFRAMEBUFFERSTATUSPROC,CheckFramebufferStatus) X(PFNGLDELETEFRAMEBUFFERSPROC,DeleteFramebuffers)
#define DISPLAY_GL_FUNCTIONS(X) DISPLAY_GL_SHADER_FUNCTIONS(X) DISPLAY_GL_TARGET_FUNCTIONS(X)
typedef struct DisplayFsr {
#define DECLARE(type,name) type name;
    DISPLAY_GL_FUNCTIONS(DECLARE)
#undef DECLARE
    GLuint easu,rcas,pixel_art,texture,framebuffer;
    int width,height,available;
} DisplayFsr;

static void display_quad(int top_down){
    float t=top_down?0.0f:1.0f,b=1.0f-t;
    glBegin(GL_QUADS);
    glTexCoord2f(0,t);glVertex2f(0,0);glTexCoord2f(1,t);glVertex2f(1,0);
    glTexCoord2f(1,b);glVertex2f(1,1);glTexCoord2f(0,b);glVertex2f(0,1);glEnd();
}
static GLuint display_shader(DisplayFsr *f,GameApplication *a,GLenum type,const char *const *sources,int count){
    GLuint shader=f->CreateShader(type);GLint ok;char error[2048];
    if(!shader)return 0;
    f->ShaderSource(shader,count,sources,NULL);f->CompileShader(shader);f->GetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(ok)return shader;
    f->GetShaderInfoLog(shader,sizeof(error),NULL,error);game_application_log(a,"FSR shader: %s\n",error);
    f->DeleteShader(shader);return 0;
}
static GLuint display_fsr_program(DisplayFsr *f,GameApplication *a,int rcas){
    const char *sources[64];unsigned n=0,i;GLuint vertex,fragment,program;GLint ok;char error[2048];
    const char *vertex_source="#version 430 compatibility\nvoid main(){gl_Position=ftransform();}\n";
    const char *easu_callbacks=
        "#define FSR_EASU_F 1\nuniform sampler2D image;uniform vec2 inputSize,outputSize;\n"
        "vec4 FsrEasuRF(vec2 p){return textureGather(image,p,0);}\n"
        "vec4 FsrEasuGF(vec2 p){return textureGather(image,p,1);}\n"
        "vec4 FsrEasuBF(vec2 p){return textureGather(image,p,2);}\n";
    const char *rcas_callbacks=
        "#define FSR_RCAS_F 1\n#define FSR_RCAS_DENOISE 1\n"
        "uniform sampler2D image;uniform vec2 outputSize,origin;uniform int flip;\n"
        "vec4 FsrRcasLoadF(ivec2 p){return texelFetch(image,clamp(p,ivec2(0),ivec2(outputSize)-1),0);}\n"
        "void FsrRcasInputF(inout float r,inout float g,inout float b){}\n";
    const char *easu_main=
        "out vec4 color;void main(){uvec4 c0,c1,c2,c3;"
        "FsrEasuCon(c0,c1,c2,c3,inputSize.x,inputSize.y,inputSize.x,inputSize.y,outputSize.x,outputSize.y);"
        "vec3 result;FsrEasuF(result,uvec2(gl_FragCoord.xy),c0,c1,c2,c3);color=vec4(result,1);}\n";
    const char *rcas_main=
        "out vec4 color;void main(){uvec4 con;FsrRcasCon(con,0.5);vec3 result;"
        "uvec2 p=uvec2(gl_FragCoord.xy-origin);if(flip!=0)p.y=uint(outputSize.y)-1u-p.y;"
        "FsrRcasF(result.r,result.g,result.b,p,con);color=vec4(result,1);}\n";
    sources[n++]="#version 430 compatibility\n#define A_GPU 1\n#define A_GLSL 1\n";
    for(i=0;fsr_portability[i];++i)sources[n++]=fsr_portability[i];
    sources[n++]=rcas?rcas_callbacks:easu_callbacks;
    for(i=0;fsr_algorithm[i];++i)sources[n++]=fsr_algorithm[i];
    sources[n++]=rcas?rcas_main:easu_main;
    vertex=display_shader(f,a,GL_VERTEX_SHADER,&vertex_source,1);
    fragment=display_shader(f,a,GL_FRAGMENT_SHADER,sources,(int)n);
    if(!vertex || !fragment){if(vertex)f->DeleteShader(vertex);if(fragment)f->DeleteShader(fragment);return 0;}
    program=f->CreateProgram();f->AttachShader(program,vertex);f->AttachShader(program,fragment);f->LinkProgram(program);
    f->DeleteShader(vertex);f->DeleteShader(fragment);f->GetProgramiv(program,GL_LINK_STATUS,&ok);
    if(ok)return program;
    f->GetProgramInfoLog(program,sizeof(error),NULL,error);game_application_log(a,"FSR program: %s\n",error);
    f->DeleteProgram(program);return 0;
}
static void display_fsr_begin(DisplayFsr *f,GameApplication *a){
    int major,minor;GLuint vertex,fragment;GLint ok;
    const char *vertex_source="#version 120\nvoid main(){gl_Position=ftransform();gl_TexCoord[0]=gl_MultiTexCoord0;}\n";
    /* TOU's integer-prescale coordinate filter: retain a flat pixel interior
     * and interpolate at its boundary instead of alternating nearest widths. */
    const char *fragment_source=
        "#version 120\nuniform sampler2D image;uniform vec2 inputSize,outputSize;"
        "void main(){vec2 scale=max(floor(outputSize/inputSize),vec2(1.0));"
        "vec2 p=gl_TexCoord[0].xy*inputSize;"
        "vec2 uv=(floor(p)+clamp((fract(p)-0.5)*scale+0.5,0.0,1.0))/inputSize;"
        "gl_FragColor=vec4(texture2D(image,uv).rgb,1.0);}\n";
    if(!SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,&major) ||
       !SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,&minor))return;
#define LOAD(type,name) f->name=(type)SDL_GL_GetProcAddress("gl" #name);if(!f->name)return;
    DISPLAY_GL_SHADER_FUNCTIONS(LOAD)
    vertex=display_shader(f,a,GL_VERTEX_SHADER,&vertex_source,1);
    fragment=display_shader(f,a,GL_FRAGMENT_SHADER,&fragment_source,1);
    if(vertex && fragment){
        f->pixel_art=f->CreateProgram();f->AttachShader(f->pixel_art,vertex);f->AttachShader(f->pixel_art,fragment);
        f->LinkProgram(f->pixel_art);f->GetProgramiv(f->pixel_art,GL_LINK_STATUS,&ok);
        if(!ok){f->DeleteProgram(f->pixel_art);f->pixel_art=0;}
    }
    if(vertex)f->DeleteShader(vertex);
    if(fragment)f->DeleteShader(fragment);
    if(major<4 || (major==4 && minor<3))return;
    DISPLAY_GL_TARGET_FUNCTIONS(LOAD)
#undef LOAD
    f->easu=display_fsr_program(f,a,0);f->rcas=display_fsr_program(f,a,1);
    f->available=f->easu && f->rcas;
}
static void display_fsr_end(DisplayFsr *f){
    if(f->framebuffer)f->DeleteFramebuffers(1,&f->framebuffer);
    if(f->texture)glDeleteTextures(1,&f->texture);
    if(f->easu)f->DeleteProgram(f->easu);
    if(f->rcas)f->DeleteProgram(f->rcas);
    if(f->pixel_art)f->DeleteProgram(f->pixel_art);
    memset(f,0,sizeof(*f));
}
static void display_pixel_art_draw(DisplayFsr *f,GLuint input,int sw,int sh,const int viewport[4],int top_down){
    GLint program=0,min_filter,mag_filter;
    glBindTexture(GL_TEXTURE_2D,input);
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&min_filter);
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&mag_filter);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    if(f->pixel_art){
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);f->UseProgram(f->pixel_art);
        f->Uniform1i(f->GetUniformLocation(f->pixel_art,"image"),0);
        f->Uniform2f(f->GetUniformLocation(f->pixel_art,"inputSize"),(float)sw,(float)sh);
        f->Uniform2f(f->GetUniformLocation(f->pixel_art,"outputSize"),(float)viewport[2],(float)viewport[3]);
    }else if(viewport[2]%sw==0 && viewport[3]%sh==0){
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
    display_quad(top_down);
    if(f->pixel_art)f->UseProgram((GLuint)program);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,min_filter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,mag_filter);
}
static int display_fsr_draw(DisplayFsr *f,GLuint input,int sw,int sh,const int viewport[4],int top_down){
    GLint program,framebuffer;int width=viewport[2],height=viewport[3],ok=1;
    if(!f->available)return 0;
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_FRAMEBUFFER_BINDING,&framebuffer);
    if(width!=f->width || height!=f->height){
        if(!f->texture)glGenTextures(1,&f->texture);
        if(!f->framebuffer)f->GenFramebuffers(1,&f->framebuffer);
        glBindTexture(GL_TEXTURE_2D,f->texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
        f->BindFramebuffer(GL_FRAMEBUFFER,f->framebuffer);
        f->FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,f->texture,0);
        if(f->CheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE || glGetError()!=GL_NO_ERROR)ok=0;
        if(ok){f->width=width;f->height=height;}
    }
    if(ok){
        f->BindFramebuffer(GL_FRAMEBUFFER,f->framebuffer);glDrawBuffer(GL_COLOR_ATTACHMENT0);glViewport(0,0,width,height);
        f->UseProgram(f->easu);glBindTexture(GL_TEXTURE_2D,input);
        f->Uniform1i(f->GetUniformLocation(f->easu,"image"),0);
        f->Uniform2f(f->GetUniformLocation(f->easu,"inputSize"),(float)sw,(float)sh);
        f->Uniform2f(f->GetUniformLocation(f->easu,"outputSize"),(float)width,(float)height);display_quad(0);
        f->BindFramebuffer(GL_FRAMEBUFFER,(GLuint)framebuffer);glDrawBuffer(framebuffer?GL_COLOR_ATTACHMENT0:GL_BACK);
        glViewport(viewport[0],viewport[1],width,height);f->UseProgram(f->rcas);glBindTexture(GL_TEXTURE_2D,f->texture);
        f->Uniform1i(f->GetUniformLocation(f->rcas,"image"),0);
        f->Uniform1i(f->GetUniformLocation(f->rcas,"flip"),top_down);
        f->Uniform2f(f->GetUniformLocation(f->rcas,"outputSize"),(float)width,(float)height);
        f->Uniform2f(f->GetUniformLocation(f->rcas,"origin"),(float)viewport[0],(float)viewport[1]);display_quad(0);
        ok=glGetError()==GL_NO_ERROR;
    }
    f->UseProgram((GLuint)program);f->BindFramebuffer(GL_FRAMEBUFFER,(GLuint)framebuffer);return ok;
}
