#include "render_motion.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
enum {BUCKETS=8192,WAYS=4};
unsigned render_motion_camera(RenderMotionCamera *h,uint64_t frame,uint64_t time_ns,
        const float view[12],float max_translation){
    unsigned i,axis,reason=0;double trace=0,distance=0;
    for(i=0;i<12;i++)if(!isfinite(view[i]))goto invalid;
    if(!frame || !isfinite(max_translation) || max_translation<=0)goto invalid;
    if(!h->frame)reason=RENDER_CAMERA_INITIAL;
    else{
        if(frame<=h->frame || frame-h->frame!=1)reason|=RENDER_CAMERA_GAP;
        if(time_ns<h->time_ns || time_ns-h->time_ns>UINT64_C(250000000))reason|=RENDER_CAMERA_TIME;
        for(axis=0;axis<3;axis++){
            double current=0,previous=0;
            for(i=0;i<3;i++){
                current-=(double)view[i*4+axis]*view[i*4+3];
                previous-=(double)h->view[i*4+axis]*h->view[i*4+3];
                trace+=(double)view[i*4+axis]*h->view[i*4+axis];
            }
            distance+=(current-previous)*(current-previous);
        }
        /* trace(Rcurrent * Rprevious^T)=1+2*cos(angle). A jump over45deg
         * has little reusable coverage even if the camera object is unchanged. */
        if(trace<2.414213562373095 || distance>(double)max_translation*max_translation)reason|=RENDER_CAMERA_POSE;
    }
    h->frame=frame;h->time_ns=time_ns;memcpy(h->view,view,sizeof(h->view));return reason;
invalid:
    memset(h,0,sizeof(*h));return RENDER_CAMERA_INVALID;
}
int render_motion_plane(RenderMotionPlane *h,uint64_t frame,uint64_t identity,
        const float bounds[4],const float uv[4],float previous[4][4]){
    unsigned i,axis;int valid=frame && identity && h->identity==identity && h->frame &&
        h->frame<frame && frame-h->frame==1;
    memset(previous,0,4*4*sizeof(float));
    for(i=0;i<4;i++)if(!isfinite(bounds[i]) || !isfinite(uv[i])){memset(h,0,sizeof(*h));return 0;}
    if(bounds[2]<=bounds[0] || bounds[3]<=bounds[1] || uv[2]==uv[0] || uv[3]==uv[1]){
        memset(h,0,sizeof(*h));return 0;
    }
    if(valid)for(i=0;i<4;i++){
        for(axis=0;axis<2;axis++){
            unsigned edge=axis==0?(i==1 || i==2):(i>=2);
            float current=bounds[axis+edge*2];
            float mapped=h->bounds[axis]+(uv[axis+edge*2]-h->uv[axis])*
                (h->bounds[axis+2]-h->bounds[axis])/(h->uv[axis+2]-h->uv[axis]);
            if(!isfinite(mapped) || fabsf(mapped-current)>(bounds[axis+2]-bounds[axis])*0.5f)valid=0;
            previous[i][axis]=mapped;
        }
        previous[i][2]=previous[i][3]=1;
    }
    h->frame=frame;h->identity=identity;memcpy(h->bounds,bounds,sizeof(h->bounds));memcpy(h->uv,uv,sizeof(h->uv));
    if(!valid)memset(previous,0,4*4*sizeof(float));
    return valid;
}
int render_motion_jitter(uint64_t frame,uint32_t phases,float output[2]){
    unsigned axis;uint32_t index;
    output[0]=output[1]=0;if(!phases)return 0;
    index=(uint32_t)(frame%phases)+1;
    for(axis=0;axis<2;axis++){
        uint32_t n=index,base=axis?3:2;float value=0,weight=1;
        while(n){weight/=(float)base;value+=(float)(n%base)*weight;n/=base;}
        output[axis]=value-0.5f;
    }
    return 1;
}
void render_motion_release(RenderMotionHistory *h){free(h->entries);h->entries=NULL;}
void render_motion_reset(RenderMotionHistory *h){if(h->entries)memset(h->entries,0,BUCKETS*WAYS*sizeof(*h->entries));}
int render_motion_sample(RenderMotionHistory *h,uint64_t frame,uint64_t key,
        const float current[3],float previous[3]){
    uint64_t hash=key;RenderMotionEntry *slot=NULL,*replacement=NULL;unsigned i;
    if(!key || !frame || !isfinite(current[0]) || !isfinite(current[1]) ||
       !isfinite(current[2]) || current[2]<=0)return 0;
    if(!h->entries){h->entries=calloc(BUCKETS*WAYS,sizeof(*h->entries));if(!h->entries)return 0;}
    hash^=hash>>30;hash*=UINT64_C(0xbf58476d1ce4e5b9);hash^=hash>>27;
    hash*=UINT64_C(0x94d049bb133111eb);hash^=hash>>31;
    for(i=0;i<WAYS;i++){
        RenderMotionEntry *e=h->entries+(hash&(BUCKETS-1))*WAYS+i;
        if(e->key==key){slot=e;break;}
        if(!e->key || (e->frame<frame && frame-e->frame>1))replacement=e;
    }
    if(!slot){
        if(!replacement)return 0; /* Saturation rejects correspondence. */
        slot=replacement;memset(slot,0,sizeof(*slot));slot->key=key;
    }
    if(slot->frame==frame){
        if(memcmp(slot->current,current,sizeof(slot->current)))slot->ambiguous=1;
        if(slot->ambiguous)slot->valid=0;
    }else{
        slot->valid=!slot->ambiguous && slot->frame && slot->frame<frame && frame-slot->frame==1;
        slot->ambiguous=0;
        memcpy(slot->previous,slot->current,sizeof(slot->previous));
        memcpy(slot->current,current,sizeof(slot->current));slot->frame=frame;
    }
    if(slot->valid)memcpy(previous,slot->previous,sizeof(slot->previous));
    return slot->valid;
}
int render_motion_projective(float output[4],const float current[3],const float previous[3]){
    float ratio;unsigned i;memset(output,0,4*sizeof(*output));
    for(i=0;i<3;i++)if(!isfinite(current[i]) || !isfinite(previous[i]))return 0;
    if(current[2]<=0 || previous[2]<=0)return 0;
    ratio=current[2]/previous[2];
    if(!isfinite(ratio) || ratio<=0 || !isfinite(previous[0]*ratio) || !isfinite(previous[1]*ratio))return 0;
    output[0]=previous[0]*ratio;output[1]=previous[1]*ratio;output[2]=ratio;output[3]=1;return 1;
}
static int resolve(RenderMotionHistory *h,uint64_t frame,uint32_t reference,
        RenderMotionLookup lookup,void *context,float output[4],float *q,unsigned *budget,unsigned depth){
    RenderMotionSource source={0};float previous[3],a[4],b[4],qa,qb;unsigned i;
    memset(output,0,4*sizeof(*output));
    if(!reference || !*budget || depth>12)return 0;
    --*budget;
    if(!lookup(context,reference,&source) || !isfinite(source.current[2]) || source.current[2]<=0)return 0;
    *q=source.current[2];
    if(source.key)return render_motion_sample(h,frame,source.key,source.current,previous) &&
        render_motion_projective(output,source.current,previous);
    if(!isfinite(source.fraction) || source.fraction<0 || source.fraction>1 ||
       !resolve(h,frame,source.parents[0],lookup,context,a,&qa,budget,depth+1) ||
       !resolve(h,frame,source.parents[1],lookup,context,b,&qb,budget,depth+1))return 0;
    for(i=0;i<3;i++){
        /* Camera-space t is affine in world space; screen t is affine in the
         * q-weighted previous projection. They must not share the same lerp. */
        float x=source.screen_space?a[i]:a[i]/qa;
        float y=source.screen_space?b[i]:b[i]/qb;
        output[i]=(x+(y-x)*source.fraction)*(source.screen_space?1:*q);
        if(!isfinite(output[i])){memset(output,0,4*sizeof(*output));return 0;}
    }
    if(output[2]<=0){memset(output,0,4*sizeof(*output));return 0;}
    output[3]=1;return 1;
}
int render_motion_resolve(RenderMotionHistory *h,uint64_t frame,uint32_t reference,
        RenderMotionLookup lookup,void *context,float output[4]){
    unsigned budget=64;float q;
    return resolve(h,frame,reference,lookup,context,output,&q,&budget,0);
}
