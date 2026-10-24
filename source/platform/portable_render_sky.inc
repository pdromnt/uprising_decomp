/* Recover the intentionally customized overlay layers, not the retail
 * TempSky substitute. The device owns these images, refreshed on level change.
 * BMP scanlines remain bottom-up, matching the preserved overlay mapping. */
static uint16_t render_le16(const uint8_t *p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static uint32_t render_le32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int render_sky_load(unsigned layer,const char *path){
    RenderDevice *g=render_current;FILE *file=host_file_open(path,"rb");
    uint8_t header[54],palette[1024]={0},row[256];uint32_t w,h,entries,dib,offset,y,x;
    uint64_t sums[3]={0};int ok=0;
    if(!file)return 0;
    if(fread(header,1,sizeof(header),file)!=sizeof(header) || header[0]!='B' || header[1]!='M')goto done;
    dib=render_le32(header+14);w=render_le32(header+18);h=render_le32(header+22);
    entries=render_le32(header+46);offset=render_le32(header+10);
    if(dib<40 || dib>4096 || !w || w>256 || !h || h>256 ||
       (layer<2 && (w!=256 || h!=256)) || render_le16(header+26)!=1 ||
       render_le16(header+28)!=8 || render_le32(header+30) || entries>256)goto done;
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
    if(!vk1_sky_upload(layer,w,h))goto done;
    if(!layer){for(x=0;x<3;x++)g->fog_color[x]=(float)sums[x]/(w*h*255.0f);g->fog_color[3]=1;}
    ok=1;
done:fclose(file);return ok;
}
static void render_sky_select(const char *name){
    RenderDevice *g=render_current;char path[256],base[128],message[256],*extension;unsigned i;
    if(!SDL_strcasecmp(name,g->sky_name))return;
    vk1_sky_release();
    memset(g->sky_layers,0,sizeof(g->sky_layers));
    snprintf(g->sky_name,sizeof(g->sky_name),"%s",name);g->sky_loaded=0;g->fog_pending=0;
    if(!*name || !SDL_strcasecmp(name,"nosky"))return;
    /* Level configuration supplies a basename, not an arbitrary path. */
    for(i=0;name[i];i++)if(name[i]=='/' || name[i]=='\\' || name[i]==':' || (name[i]=='.' && name[i+1]=='.'))return;
    snprintf(path,sizeof(path),"GRIDS/skies/%s%s",name,strrchr(name,'.')?"":".bmp");
    g->sky_loaded=render_sky_load(0,path);
    snprintf(base,sizeof(base),"%s",name);extension=strrchr(base,'.');if(extension)*extension=0;
    /* Some original training names have no loose layered replacement. Keep
     * the established TempSky fallback instead of suppressing sky and fog. */
    if(!g->sky_loaded){
        g->sky_loaded=render_sky_load(0,"GRIDS/skies/tempsky.bmp");
        if(g->sky_loaded)snprintf(base,sizeof(base),"tempsky");
    }
    snprintf(path,sizeof(path),"GRIDS/skies/%s2.bmp",base);(void)render_sky_load(1,path);
    snprintf(path,sizeof(path),"GRIDS/skies/%st.bmp",base);(void)render_sky_load(2,path);
    g->sky_tick=(uint32_t)SDL_GetTicks();g->fog_pending=g->sky_loaded;
    snprintf(message,sizeof(message),"native Vulkan sky %s [%s]: base=%d detail=%d horizon=%d\n",name,base,g->sky_loaded,g->sky_layers[1]!=0,g->sky_layers[2]!=0);
    game_application_log(g->application,"%s",message);
}
static void render_level_fog(int enabled){
    RenderDevice *g=render_current;uint32_t config,range=0;float horizon=10000;
    if(!render_ready())return;
        g->native_fog=enabled && g->sky_loaded?1:0;
        if(g->native_fog){
            config=game_memory_read_u32(&g->application->memory,0x5943c8);if(config)range=game_memory_read_u32(&g->application->memory,config+0x14);
            if(range>=500 && range<=20000)horizon=(float)range;
            g->native_fog_start=horizon*0.67f;g->native_fog_end=horizon;
        }

}

static int render_sky_begin(const GameSkyFrame *frame){
    RenderDevice *g=render_current;float horizontal,fov,pitch;
    unsigned i;
    if(!render_ready() || !frame || !frame->name || g->sky_active)return 0;
    if(!isfinite(frame->center[0]) || !isfinite(frame->center[1]) ||
       !isfinite(frame->scale) || !isfinite(frame->vertical))return 0;
    for(i=0;i<12;i++)if(!isfinite(frame->view[i]))return 0;
    render_sky_select(frame->name);
    g->projection_center=frame->center[0];g->projection_scale=frame->scale>1?frame->scale:381.3607f;
    horizontal=hypotf(frame->view[8],frame->view[10]);if(!(horizontal>0.0001f))horizontal=1;
    pitch=atan2f(frame->view[9],horizontal);
    g->sky_yaw=atan2f(-frame->view[2],frame->view[0])/6.28318530718f;
    g->sky_horizon=frame->center[1]+g->projection_scale*frame->vertical*pitch;
    if(!isfinite(g->sky_horizon))return 0;
    fov=2*atanf(frame->center[0]/g->projection_scale);if(!(fov>0.01f))fov=6.28318530718f*0.22f;
    g->sky_yaw_scale=(g->width/256.0f)*6.28318530718f/fov;
    g->sky_offset=isfinite(frame->speed) && fabsf(frame->speed)<=1000?
        (float)(uint32_t)((uint32_t)SDL_GetTicks()-g->sky_tick)*frame->speed/100000:0;
    g->sky_active=1;g->sky_triangles=0;return 1;
}
static int render_remote_mask(const int32_t bounds[4]){
    RenderDevice *g=render_current;int width=bounds[2]-bounds[0],height=bounds[3]-bounds[1];
    size_t count=(size_t)width*height,head=0,tail=0,index;uint8_t *inside=NULL,color[3];
    uint32_t *queue=NULL;int ok=0;
    if(g->remote_mask && !memcmp(g->remote_bounds,bounds,sizeof(g->remote_bounds)))return 1;
    inside=calloc(count,1);queue=malloc(count*sizeof(*queue));if(!inside || !queue)goto done;
    if(!vk1_remote_read(bounds))goto done;
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
    if(!vk1_remote_upload((unsigned)width,(unsigned)height))goto done;
    memcpy(g->remote_bounds,bounds,sizeof(g->remote_bounds));
    game_application_log(g->application,"%s","native Vulkan remote glass mask ready\n");ok=1;
done:free(queue);free(inside);return ok;
}

static float render_sky_z(const GameRenderVertex *v){
    int depth=render_w_depth(v->oow)+(int16_t)render_current->depth_bias;
    return 1-2*(float)(depth<0?0:depth>65535?65535:depth)/65535;
}
static void render_sky_triangle(const GameRenderVertex *a,const GameRenderVertex *b,const GameRenderVertex *c){
    RenderDevice *g=render_current;(void)b;(void)c;
    if(!g->sky_loaded)return;
    if(!g->sky_triangles)g->sky_depth=render_sky_z(a);
    ++g->sky_triangles;
}
