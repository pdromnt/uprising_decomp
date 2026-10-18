/* Recover the intentionally customized overlay layers, not the retail
 * TempSky substitute. The device owns these images, refreshed on level change.
 * BMP scanlines remain bottom-up, matching the preserved overlay mapping. */
static uint16_t gl1_le16(const uint8_t *p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static uint32_t gl1_le32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int gl1_sky_load(unsigned layer,const char *path){
    Gl1Device *g=gl1_current;FILE *file=fopen(path,"rb");
    uint8_t header[54],palette[1024]={0},row[256];uint32_t w,h,entries,dib,offset,y,x;
    uint64_t sums[3]={0};int ok=0;
    if(!file)return 0;
    if(fread(header,1,sizeof(header),file)!=sizeof(header) || header[0]!='B' || header[1]!='M')goto done;
    dib=gl1_le32(header+14);w=gl1_le32(header+18);h=gl1_le32(header+22);
    entries=gl1_le32(header+46);offset=gl1_le32(header+10);
    if(dib<40 || dib>4096 || !w || w>256 || !h || h>256 ||
       (layer<2 && (w!=256 || h!=256)) || gl1_le16(header+26)!=1 ||
       gl1_le16(header+28)!=8 || gl1_le32(header+30) || entries>256)goto done;
    if(!entries)entries=256;
    if(offset<14+dib+entries*4 || offset>INT32_MAX || fseek(file,(long)(14+dib),SEEK_SET) ||
       fread(palette,4,entries,file)!=entries || fseek(file,(long)offset,SEEK_SET))goto done;
    for(y=0;y<h;y++){
        if(fread(row,1,(w+3)&~3u,file)!=((w+3)&~3u))goto done;
        for(x=0;x<w;x++){
            uint8_t *out=g->rgba+((size_t)y*w+x)*4;const uint8_t *in=palette+row[x]*4u;
            out[0]=in[2];out[1]=in[1];out[2]=in[0];out[3]=255;
            if(layer==2){
                unsigned fade=h<16?h:16;
                if(in[2]>240 && in[0]>240 && in[1]<16)out[3]=0;
                else if(y<fade)out[3]=(uint8_t)(y*y*255/(fade*fade));
            }
            sums[0]+=out[0];sums[1]+=out[1];sums[2]+=out[2];
        }
    }
    glGenTextures(1,g->sky_layers+layer);glBindTexture(GL_TEXTURE_2D,g->sky_layers[layer]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,g->rgba);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,layer==2?GL_NEAREST:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,layer==2?GL_NEAREST:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,layer==2?GL_CLAMP:GL_REPEAT);
    if(!layer){for(x=0;x<3;x++)g->fog_color[x]=(float)sums[x]/(w*h*255.0f);g->fog_color[3]=1;}
    ok=1;
done:fclose(file);return ok;
}
static void gl1_sky_select(const char *name){
    Gl1Device *g=gl1_current;char path[256],base[128],message[256],*extension;unsigned i;
    if(!_stricmp(name,g->sky_name))return;
    glDeleteTextures(3,g->sky_layers);memset(g->sky_layers,0,sizeof(g->sky_layers));
    snprintf(g->sky_name,sizeof(g->sky_name),"%s",name);g->sky_loaded=0;g->fog_pending=0;
    if(!*name || !_stricmp(name,"nosky"))return;
    /* Level configuration supplies a basename, not an arbitrary path. */
    for(i=0;name[i];i++)if(name[i]=='/' || name[i]=='\\' || name[i]==':' || (name[i]=='.' && name[i+1]=='.'))return;
    snprintf(path,sizeof(path),"GRIDS/skies/%s%s",name,strrchr(name,'.')?"":".bmp");
    g->sky_loaded=gl1_sky_load(0,path);
    snprintf(base,sizeof(base),"%s",name);extension=strrchr(base,'.');if(extension)*extension=0;
    snprintf(path,sizeof(path),"GRIDS/skies/%s2.bmp",base);(void)gl1_sky_load(1,path);
    snprintf(path,sizeof(path),"GRIDS/skies/%st.bmp",base);(void)gl1_sky_load(2,path);
    g->sky_tick=(uint32_t)SDL_GetTicks();g->fog_pending=g->sky_loaded;
    snprintf(message,sizeof(message),"native OpenGL1 sky %s: base=%d detail=%d horizon=%d\n",name,g->sky_loaded,g->sky_layers[1]!=0,g->sky_layers[2]!=0);
    game_application_log(g->application,"%s",message);
}
static void gl1_level_fog(int enabled){
    Gl1Device *g=gl1_current;uint32_t config,range=0;float horizon=10000;
    if(!gl1_ready())return;
    if(!enabled || !g->sky_loaded){glDisable(GL_FOG);return;}
    config=game_memory_read_u32(&g->application->memory,0x5943c8);
    if(config)range=game_memory_read_u32(&g->application->memory,config+0x14);
    if(range>=500 && range<=20000)horizon=(float)range;
    glFogi(GL_FOG_COORDINATE_SOURCE,GL_FOG_COORDINATE);glFogi(GL_FOG_MODE,GL_LINEAR);
    glFogf(GL_FOG_START,horizon*0.67f);glFogf(GL_FOG_END,horizon);glFogfv(GL_FOG_COLOR,g->fog_color);glEnable(GL_FOG);
}
static int gl1_sky_begin(const GameSkyFrame *frame){
    Gl1Device *g=gl1_current;float horizontal,fov,pitch;GLint previous;
    unsigned i;
    if(!gl1_ready() || !frame || !frame->name || g->sky_active)return 0;
    if(!isfinite(frame->center[0]) || !isfinite(frame->center[1]) ||
       !isfinite(frame->scale) || !isfinite(frame->vertical))return 0;
    for(i=0;i<12;i++)if(!isfinite(frame->view[i]))return 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);gl1_sky_select(frame->name);glBindTexture(GL_TEXTURE_2D,(GLuint)previous);
    g->projection_center=frame->center[0];g->projection_scale=frame->scale>1?frame->scale:381.3607f;
    horizontal=hypotf(frame->view[8],frame->view[10]);if(!(horizontal>0.0001f))horizontal=1;
    pitch=atan2f(frame->view[9],horizontal);
    g->sky_yaw=atan2f(-frame->view[2],frame->view[0])/6.28318530718f;
    g->sky_horizon=frame->center[1]+g->projection_scale*frame->vertical*pitch;
    if(!isfinite(g->sky_horizon))return 0;
    fov=2*atanf(frame->center[0]/g->projection_scale);if(!(fov>0.01f))fov=6.28318530718f*0.22f;
    g->sky_yaw_scale=(g->width/256.0f)*6.28318530718f/fov;
    g->sky_offset=isfinite(frame->speed) && fabsf(frame->speed)<=1000?
        (float)(DWORD)((uint32_t)SDL_GetTicks()-g->sky_tick)*frame->speed/100000:0;
    glPushAttrib(GL_CURRENT_BIT|GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_TEXTURE_BIT|GL_SCISSOR_BIT);
    glDepthMask(GL_FALSE);glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);glDisable(GL_FOG);
    glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    g->sky_active=1;g->sky_triangles=0;return 1;
}
static float gl1_sky_z(const GameRenderVertex *v){
    int depth=gl1_w_depth(v->oow)+(int16_t)gl1_current->depth_bias;
    return 1-2*(float)(depth<0?0:depth>65535?65535:depth)/65535;
}
static void gl1_sky_triangle(const GameRenderVertex *a,const GameRenderVertex *b,const GameRenderVertex *c){
    Gl1Device *g=gl1_current;(void)b;(void)c;
    if(!g->sky_loaded)return;
    if(!g->sky_triangles)g->sky_depth=gl1_sky_z(a);
    ++g->sky_triangles;
}
static int gl1_sky_end(void){
    Gl1Device *g=gl1_current;float bottom,top,repeats,u;GLint previous;unsigned layer;
    if(!g || !g->sky_active)return 0;
    /* The retail cylinder's finite bottom curls up at wide/downward camera
     * angles. Fill the clipped background at its existing sky depth, then let
     * later terrain/world geometry occlude it, as with the flat horizon strip. */
    if(g->sky_triangles)for(layer=0;layer<2;layer++)if(g->sky_layers[layer]){
        float u0=-g->sky_yaw*g->sky_yaw_scale+(layer?g->sky_offset:0);
        float v0=-g->sky_horizon/256+0.5f+(layer?g->sky_offset*0.6f:0);
        glBindTexture(GL_TEXTURE_2D,g->sky_layers[layer]);
        if(layer){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}
        else glDisable(GL_BLEND);
        glColor4f(1,1,1,layer?0.5f:1);glBegin(GL_QUADS);
        glTexCoord2f(u0,v0);glVertex3f(0,0,g->sky_depth);
        glTexCoord2f(u0+g->width/256.0f,v0);glVertex3f(g->width,0,g->sky_depth);
        glTexCoord2f(u0+g->width/256.0f,v0+g->height/256.0f);glVertex3f(g->width,g->height,g->sky_depth);
        glTexCoord2f(u0,v0+g->height/256.0f);glVertex3f(0,g->height,g->sky_depth);glEnd();
    }
    if(g->sky_triangles && g->fog_pending){
        float sample=g->sky_horizon-142;int row;unsigned x;uint64_t sum[3]={0},count=0;
        if(sample<1)sample=1;if(sample>g->height-3)sample=(float)g->height-3;
        row=(int)floorf(sample);
        glGetIntegerv(GL_PACK_ALIGNMENT,&previous);glPixelStorei(GL_PACK_ALIGNMENT,1);
        glReadPixels(0,g->height-row-3,g->width,3,GL_RGB,GL_UNSIGNED_BYTE,g->rgba);glPixelStorei(GL_PACK_ALIGNMENT,previous);
        for(x=0;x<g->width*3;x++){
            uint8_t *p=g->rgba+x*3;
            if((p[0]==0x83 && p[1]==0xb9 && p[2]==0xbe) || (p[0]<80 && p[1]>220 && p[2]<80))continue;
            sum[0]+=p[0];sum[1]+=p[1];sum[2]+=p[2];count++;
        }
        if(count>=g->width){for(x=0;x<3;x++)g->fog_color[x]=(float)sum[x]/(count*255.0f);g->fog_pending=0;}
    }
    if(g->sky_triangles && g->sky_layers[2]){
        /* Requested presentation adjustment: lower the horizon artwork 35
         * pixels, keeping the camera and scrolling sky projection unchanged. */
        bottom=g->sky_horizon+19;top=bottom-128;
        repeats=2*atanf(g->projection_center/g->projection_scale)/6.28318530718f*16;u=-g->sky_yaw*16;
        glBindTexture(GL_TEXTURE_2D,g->sky_layers[2]);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColor4f(1,1,1,1);
        glBegin(GL_QUADS);
        glTexCoord2f(u-repeats*0.5f,1);glVertex3f(0,top,g->sky_depth);
        glTexCoord2f(u+repeats*0.5f,1);glVertex3f(g->width,top,g->sky_depth);
        glTexCoord2f(u+repeats*0.5f,0);glVertex3f(g->width,bottom,g->sky_depth);
        glTexCoord2f(u-repeats*0.5f,0);glVertex3f(0,bottom,g->sky_depth);glEnd();
    }
    glPopAttrib();if(g->bound)glBindTexture(GL_TEXTURE_2D,g->bound->name);
    g->sky_active=0;return !g->failed;
}

/* Derive the irregular monitor opening from its actual freshly drawn artwork,
 * as the preserved overlay does. Cache only the immutable mask, not a second
 * scene or UI framebuffer. Queue/coverage storage is bounded by the device. */
static int gl1_remote_mask(const int32_t bounds[4]){
    Gl1Device *g=gl1_current;int width=bounds[2]-bounds[0],height=bounds[3]-bounds[1];
    size_t count=(size_t)width*height,head=0,tail=0,index;uint8_t *inside=NULL,color[3];
    uint32_t *queue=NULL;GLint packing;int ok=0;
    if(g->remote_mask && !memcmp(g->remote_bounds,bounds,sizeof(g->remote_bounds)))return 1;
    inside=calloc(count,1);queue=malloc(count*sizeof(*queue));if(!inside || !queue)goto done;
    glGetIntegerv(GL_PACK_ALIGNMENT,&packing);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(bounds[0],g->height-bounds[3],width,height,GL_RGB,GL_UNSIGNED_BYTE,g->rgba);
    glPixelStorei(GL_PACK_ALIGNMENT,packing);
    index=(size_t)(height/2)*width+width/2;memcpy(color,g->rgba+index*3,3);
    inside[index]=1;queue[tail++]=(uint32_t)index;
    while(head<tail){
        uint32_t current=queue[head++];int x=(int)(current%(unsigned)width),y=(int)(current/(unsigned)width);
        const int dx[4]={-1,1,0,0},dy[4]={0,0,-1,1};unsigned direction;
        for(direction=0;direction<4;direction++){
            int nx=x+dx[direction],ny=y+dy[direction];size_t neighbor;const uint8_t *pixel;
            if(nx<0 || ny<0 || nx>=width || ny>=height)continue;
            neighbor=(size_t)ny*width+nx;if(inside[neighbor])continue;
            pixel=g->rgba+neighbor*3;
            if(abs((int)pixel[0]-color[0])>5 || abs((int)pixel[1]-color[1])>5 || abs((int)pixel[2]-color[2])>5)continue;
            inside[neighbor]=1;queue[tail++]=(uint32_t)neighbor;
        }
    }
    /* The native tiled artwork leaves the glass at the explicit clear color;
     * its clip bounds can lie wholly inside the bezel (Erudara:31950/31950).
     * That is an owned transparent opening, not the old ambiguous screenshot
     * case. Retain the original coverage guard for any other center color. */
    if(tail<count/2 || (tail>count*97/100 &&
       !(color[0]==0x83 && color[1]==0xb9 && color[2]==0xbe)))goto done;
    for(index=0;index<count;index++){
        uint8_t *p=g->rgba+index*4;p[0]=p[1]=p[2]=255;p[3]=inside[index]?255:0;
    }
    if(!g->remote_mask)glGenTextures(1,&g->remote_mask);
    glBindTexture(GL_TEXTURE_2D,g->remote_mask);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,g->rgba);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    memcpy(g->remote_bounds,bounds,sizeof(g->remote_bounds));
    game_application_log(g->application,"%s","native OpenGL1 remote glass mask ready\n");ok=1;
done:free(queue);free(inside);return ok;
}
static void gl1_remote_quad(float l,float t,float r,float b,float u0,float v0,float u1,float v1){
    glBegin(GL_QUADS);
    glTexCoord2f(u0,v0);glVertex2f(l,t);glTexCoord2f(u1,v0);glVertex2f(r,t);
    glTexCoord2f(u1,v1);glVertex2f(r,b);glTexCoord2f(u0,v1);glVertex2f(l,b);glEnd();
}
static int gl1_remote_sky(const GameRemoteSkyFrame *frame){
    Gl1Device *g=gl1_current;int32_t bounds[4];GLint previous;float width,height,scale,fov,u0,u1,offset=0,bottom;
    if(!gl1_ready() || !frame || !frame->name || g->sky_active || !isfinite(frame->yaw))return 0;
    memcpy(bounds,frame->bounds,sizeof(bounds));
    if(bounds[0]<0)bounds[0]=0;if(bounds[1]<0)bounds[1]=0;
    if(bounds[2]>(int)g->width)bounds[2]=(int)g->width;if(bounds[3]>(int)g->height)bounds[3]=(int)g->height;
    if(bounds[2]<=bounds[0] || bounds[3]<=bounds[1])return 1;
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);
    glPushAttrib(GL_CURRENT_BIT|GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT|GL_TEXTURE_BIT|GL_SCISSOR_BIT|GL_PIXEL_MODE_BIT);
    gl1_sky_select(frame->name);if(!g->sky_loaded)goto done;
    if(!gl1_remote_mask(bounds))goto done;
    width=(float)(bounds[2]-bounds[0]);height=(float)(bounds[3]-bounds[1]);
    scale=isfinite(frame->scale) && frame->scale>1 && frame->scale<=100000?frame->scale:width*0.596f;
    fov=2*atanf(width*0.5f/scale);if(!(fov>0.01f))fov=6.28318530718f*0.22f;
    u0=-frame->yaw*(width/256)*6.28318530718f/fov;u1=u0+width/256;
    if(isfinite(frame->speed) && fabsf(frame->speed)<=1000)offset=(float)(DWORD)((uint32_t)SDL_GetTicks()-g->sky_tick)*frame->speed/100000;
    g->projection_center=(bounds[0]+bounds[2])*0.5f;g->projection_scale=scale;
    glEnable(GL_SCISSOR_TEST);glScissor(bounds[0],g->height-bounds[3],(int)width,(int)height);
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glDisable(GL_FOG);glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glStencilMask(0xff);glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS,1,0xff);glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.5f);
    glBindTexture(GL_TEXTURE_2D,g->remote_mask);glColor4f(1,1,1,1);
    gl1_remote_quad(bounds[0],bounds[1],bounds[2],bounds[3],0,1,1,0);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDisable(GL_ALPHA_TEST);
    glStencilMask(0);glStencilFunc(GL_EQUAL,1,0xff);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
    glBindTexture(GL_TEXTURE_2D,g->sky_layers[0]);
    gl1_remote_quad(bounds[0],bounds[1],bounds[2],bounds[3],u0,0,u1,height/256);
    if(g->sky_layers[1]){
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColor4f(1,1,1,0.5f);
        glBindTexture(GL_TEXTURE_2D,g->sky_layers[1]);
        gl1_remote_quad(bounds[0],bounds[1],bounds[2],bounds[3],u0+offset,offset*0.6f,u1+offset,height/256+offset*0.6f);
    }
    if(g->sky_layers[2]){
        bottom=bounds[1]+height*0.58f;
        glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);glColor4fv(g->fog_color);
        gl1_remote_quad(bounds[0],bottom,bounds[2],bounds[3],0,0,1,1);
        glEnable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0);glColor4f(1,1,1,1);glBindTexture(GL_TEXTURE_2D,g->sky_layers[2]);
        gl1_remote_quad(bounds[0],bottom-height*0.42f,bounds[2],bottom,-frame->yaw*16,1,-frame->yaw*16+4,0);
    }
    glStencilMask(0xff);glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);
done:glPopAttrib();glBindTexture(GL_TEXTURE_2D,(GLuint)previous);return !g->failed;
}
