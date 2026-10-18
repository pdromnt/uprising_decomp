#include "asset_collision.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VERTICES 300u
#define PLANES 100u
typedef struct AscInput {
    const ResourceBlob *blob;size_t position;char line[256];
    float vertices[VERTICES][3];uint32_t count,beginning;
    int32_t *third;uint8_t *visible;
} AscInput;
static uint32_t bits(float value){uint32_t result;memcpy(&result,&value,4);return result;}
static float real(uint32_t value){float result;memcpy(&result,&value,4);return result;}
static AssetMeshVertex vector(const float v[3]){AssetMeshVertex out={bits(v[0]),bits(v[1]),bits(v[2])};return out;}
static void unpack(const AssetMeshVertex *v,float out[3]){out[0]=real(v->x_bits);out[1]=real(v->y_bits);out[2]=real(v->z_bits);}
static int line(AscInput *in){
    size_t count=0,first;unsigned c;if(in->position==in->blob->size)return 0;
    while(in->position<in->blob->size && count<255){
        c=in->blob->data[in->position++];if(c=='\r' || c=='\n' || c=='\f'){
            if(c=='\r' && in->position<in->blob->size && in->blob->data[in->position]=='\n')++in->position;
            break;
        }in->line[count++]=(char)c;
    }
    in->line[count]=0;for(first=0;first<count && isspace((unsigned char)in->line[first]);++first){}
    if(first)memmove(in->line,in->line+first,count-first+1);
    return 1;
}
static int read(AscInput *in){
    int index,a,b,c,ab,bc,ca;float x,y,z;unsigned i;
    while(line(in)){
        if(!strncmp(in->line,"Vertex list",11)){in->beginning=in->count;continue;}
        if(!strncmp(in->line,"Vertex ",7)){
            if(in->count==VERTICES || sscanf(in->line,"Vertex %d: X:%f Y:%f Z:%f",&index,&x,&y,&z)!=4 || !isfinite(x) || !isfinite(y) || !isfinite(z))return 0;
            /*ASC uses exporter Z as game height; SDF is already in game axes. */
            in->vertices[in->count][0]=-x;in->vertices[in->count][1]=z;in->vertices[in->count++][2]=y;
        }else if(!strncmp(in->line,"Face list:",10))continue;
        else if(!strncmp(in->line,"Face ",5)){
            if(sscanf(in->line,"Face %d: A:%d B:%d C:%d AB:%d BC:%d CA:%d",&index,&a,&b,&c,&ab,&bc,&ca)!=7 || a<0 || b<0 || c<0)return 0;
            if((unsigned)a>=in->count-in->beginning || (unsigned)b>=in->count-in->beginning || (unsigned)c>=in->count-in->beginning)return 0;
            a+=(int)in->beginning;b+=(int)in->beginning;c+=(int)in->beginning;
            in->third[a*VERTICES+b]=c;in->third[b*VERTICES+c]=a;in->third[c*VERTICES+a]=b;
            in->visible[a*VERTICES+b]=ab!=0;in->visible[b*VERTICES+c]=bc!=0;in->visible[c*VERTICES+a]=ca!=0;
        }
    }
    for(i=0;i<in->count*VERTICES;++i){
        unsigned from=i/VERTICES,to=i%VERTICES;
        if(in->visible[i] && in->third[to*VERTICES+from]!=-1 && !in->visible[to*VERTICES+from])
            in->visible[to*VERTICES+from]=1;
    }
    return in->count!=0;
}
static float magnitude(const float v[3]){
    /*The original rounds the squared sum before calling its float sqrt. */
    volatile float squared=(float)((long double)v[0]*v[0]+(long double)v[1]*v[1]+(long double)v[2]*v[2]);
    volatile float length=sqrtf(squared);
    return length;
}
static void cross(const float a[3],const float b[3],float out[3]){
    out[0]=(float)((long double)a[1]*b[2]-(long double)a[2]*b[1]);
    out[1]=(float)((long double)a[2]*b[0]-(long double)a[0]*b[2]);
    out[2]=(float)((long double)a[0]*b[1]-(long double)a[1]*b[0]);
}
static int normalize(float v[3],float unit){volatile float length=magnitude(v);long double scale;unsigned i;
    if(!isfinite(length) || length==0)return 0;
    if(length==unit)return 1;
    scale=(long double)unit/length;for(i=0;i<3;++i)v[i]=(float)((long double)v[i]*scale);return 1;
}
static int equal(const float a[3],const float b[3]){return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];}
static int rounded_equal(const AssetMeshVertex *a,const AssetMeshVertex *b){
    /*562dd0 installs the verified173f downward-rounding control word. */
    return floorf(real(a->x_bits))==floorf(real(b->x_bits)) &&
        floorf(real(a->y_bits))==floorf(real(b->y_bits)) && floorf(real(a->z_bits))==floorf(real(b->z_bits));
}
int asset_collision_read_asc(const ResourceBlob *blob,float half,float unit,AssetModel *out){
    AscInput *in=NULL;AssetModel model={0};uint16_t contour[VERTICES];float low[3],high[3],center[3],extent[3],radius=0;
    uint32_t from,to,current,next,third,count,i,j,k,p,q,e,other;unsigned axis,visits;int ok=0;
    if(!blob || !blob->data || !blob->size || !out || out->planes || out->mesh.vertices || out->markers || !isfinite(half) || !isfinite(unit))return 0;
    in=calloc(1,sizeof(*in));if(!in)goto done;in->blob=blob;
    in->third=malloc(VERTICES*VERTICES*sizeof(*in->third));in->visible=calloc(VERTICES*VERTICES,1);
    if(!in->third || !in->visible)goto done;
    for(i=0;i<VERTICES*VERTICES;++i)in->third[i]=-1;
    if(!read(in))goto done;
    for(axis=0;axis<3;++axis)low[axis]=high[axis]=in->vertices[0][axis];
    for(i=1;i<in->count;++i)for(axis=0;axis<3;++axis){float value=in->vertices[i][axis];if(value<low[axis])low[axis]=value;if(value>high[axis])high[axis]=value;}
    for(axis=0;axis<3;++axis){center[axis]=(float)(((long double)low[axis]+high[axis])*half);extent[axis]=(float)((long double)high[axis]-center[axis]);}
    for(i=0;i<in->count;++i){float distance;
        for(axis=0;axis<3;++axis)in->vertices[i][axis]=(float)((long double)in->vertices[i][axis]-center[axis]);
        distance=magnitude(in->vertices[i]);if(distance>radius)radius=distance;
    }
    model.planes=calloc(PLANES,sizeof(*model.planes));if(!model.planes)goto done;
    for(from=0;from<in->count;++from)for(to=0;to<in->count;++to){AssetCollisionPlane *plane;
        if(!in->visible[from*VERTICES+to])continue;
        current=from;next=to;count=0;
        do{
            if(count==VERTICES)goto done;
            contour[count++]=(uint16_t)current;
            third=(uint32_t)in->third[current*VERTICES+next];if(third>=in->count)goto done;
            visits=0;
            while(!in->visible[next*VERTICES+third]){
                third=(uint32_t)in->third[third*VERTICES+next];
                if(third>=in->count || third==current || ++visits>VERTICES)goto done;
            }
            current=next;next=third;
        }while(current!=from);
        in->visible[contour[count-1]*VERTICES+contour[0]]=0;
        for(i=0;i+1<count;++i)in->visible[contour[i]*VERTICES+contour[i+1]]=0;
        /*4ef775 removes coincident vertices while retaining first-seen order. */
        for(i=1,j=1;i<count;++i){for(k=0;k<j;++k)if(equal(in->vertices[contour[k]],in->vertices[contour[i]]))break;
            if(k==j)contour[j++]=contour[i];}
        count=j;if(count<3)continue;
        if(model.plane_count==PLANES)goto done;
        plane=model.planes+model.plane_count++;plane->vertex_count=count;
        plane->vertices=malloc(count*sizeof(*plane->vertices));plane->secondary_vectors=calloc(count,sizeof(*plane->secondary_vectors));
        if(!plane->vertices || !plane->secondary_vectors)goto done;
        for(i=0;i<count;++i)plane->vertices[i]=vector(in->vertices[contour[i]]);
        {float first[3],second[3],normal[3];for(axis=0;axis<3;++axis){first[axis]=(float)((long double)in->vertices[contour[2]][axis]-in->vertices[contour[1]][axis]);
            second[axis]=(float)((long double)in->vertices[contour[0]][axis]-in->vertices[contour[1]][axis]);}
            cross(first,second,normal);if(!normalize(normal,unit))goto done;plane->normal=vector(normal);}
    }
    /*4efaa8 matches reverse edges using rounded coordinates. The edge plane
     *uses the mean of the adjacent normals, or its own normal on a boundary. */
    for(p=0;p<model.plane_count;++p){AssetCollisionPlane *plane=model.planes+p;
        for(e=0;e<plane->vertex_count;++e){float normal[3],edge[3],side[3],a[3],b[3],adjacent[3];
            unpack(&plane->normal,normal);other=UINT32_MAX;
            for(q=0;q<model.plane_count && other==UINT32_MAX;++q)if(q!=p){AssetCollisionPlane *neighbor=model.planes+q;
                for(j=0;j<neighbor->vertex_count;++j)
                    if(rounded_equal(plane->vertices+e,neighbor->vertices+(j+1)%neighbor->vertex_count) &&
                       rounded_equal(plane->vertices+(e+1)%plane->vertex_count,neighbor->vertices+j)){other=q;break;}
            }
            if(other!=UINT32_MAX){unpack(&model.planes[other].normal,adjacent);
                for(axis=0;axis<3;++axis)normal[axis]=(float)(((long double)normal[axis]+adjacent[axis])*half);}
            unpack(plane->vertices+e,a);unpack(plane->vertices+(e+1)%plane->vertex_count,b);
            for(axis=0;axis<3;++axis)edge[axis]=(float)((long double)b[axis]-a[axis]);
            cross(normal,edge,side);if(!normalize(side,unit))goto done;plane->secondary_vectors[e]=vector(side);
        }
    }
    model.format=ASSET_MODEL_COLLISION;model.center=vector(extent);model.radius_bits=bits(radius);
    *out=model;memset(&model,0,sizeof(model));ok=1;
done:asset_model_release(&model);if(in){free(in->third);free(in->visible);free(in);}return ok;
}
