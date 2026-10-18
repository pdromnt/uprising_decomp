#include "asset_sdf.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define NONE UINT32_MAX
typedef struct SdfTextureArray {char name[256];char **files;uint32_t count;} SdfTextureArray;
typedef struct SdfReader {
    const uint8_t *bytes;size_t size,at;char line[257];float bias;
    AssetSdf result;SdfTextureArray textures[100];uint32_t texture_count;
} SdfReader;
typedef struct SdfPoint {uint16_t vertex;AssetMeshUV uv;} SdfPoint;
static uint32_t word(float value){uint32_t bits;memcpy(&bits,&value,4);return bits;}
static float value(uint32_t bits){float real;memcpy(&real,&bits,4);return real;}
static char *copy(const char *text){size_t size=strlen(text)+1;char *p=malloc(size);if(p)memcpy(p,text,size);return p;}
static void *append(void *array,size_t size,uint32_t count,const void *entry){
    void *next;if(count>=65535 || size>SIZE_MAX/(count+1u))return NULL;
    next=realloc(array,size*(count+1u));if(!next)return NULL;
    memcpy((uint8_t *)next+size*count,entry,size);return next;
}
/*The original fgets-like reader returns at most255 chars, trims leading space,
 * treats CR/LF/form-feed as line ends, and retains the final EOF fragment. */
static int line(SdfReader *r){
    size_t n=0,start;unsigned c;if(r->at==r->size)return 0;
    while(r->at<r->size && n<255){c=r->bytes[r->at++];if(c=='\r' || c=='\n' || c=='\f'){
        if(c=='\r' && r->at<r->size && r->bytes[r->at]=='\n')++r->at;
        break;}r->line[n++]=(char)c;}
    r->line[n]=0;for(start=0;start<n && isspace((unsigned char)r->line[start]);++start){}
    if(start)memmove(r->line,r->line+start,n-start+1);
    return 1;
}
static int tag(SdfReader *r,const char *key){return !strncmp(r->line,key,strlen(key));}
static int skip_block(SdfReader *r){unsigned depth=strchr(r->line,'{')?1:0;while(line(r)){
    const char *p=r->line;for(;*p;++p){if(*p=='{')++depth;else if(*p=='}'){
        if(!depth || --depth==0)return 1;}}}
    return 0;
}
static int texture_array(SdfReader *r,const char *name){
    SdfTextureArray *t;char path[256];void *next;if(r->texture_count>=100)return 0;t=&r->textures[r->texture_count++];strcpy(t->name,name);
    while(line(r)){if(tag(r,"}"))return 1;if(tag(r,"fileName")){
        char *owned;if(t->count>=512 || sscanf(r->line,"fileName %255s",path)!=1 || !(owned=copy(path)))return 0;
        next=append(t->files,sizeof(*t->files),t->count,&owned);if(!next){free(owned);return 0;}t->files=next;++t->count;
    }else if(tag(r,"# TexControl")){/*4ecc70 returns the last filename in this controller. */
        char *owned;int found=0,closed=0;
        while(line(r)){if(tag(r,"}")){closed=1;break;}if(tag(r,"fileName")){
            if(sscanf(r->line,"fileName %255s",path)!=1)return 0;
            found=1;}}
        if(!found || !closed || t->count>=512 || !(owned=copy(path)))return 0;
        next=append(t->files,sizeof(*t->files),t->count,&owned);if(!next){free(owned);return 0;}t->files=next;++t->count;
    }}return 0;
}
static int mesh_materials(SdfReader *r,AssetMesh *mesh,const char *name){
    uint32_t i,j;if(mesh->material_count)return 0;
    for(i=0;i<r->texture_count;++i)if(!strcmp(name,r->textures[i].name)){
        SdfTextureArray *t=&r->textures[i];mesh->materials=calloc(t->count,sizeof(*mesh->materials));if(t->count && !mesh->materials)return 0;
        mesh->material_count=t->count;for(j=0;j<t->count;++j)if(!(mesh->materials[j]=copy(t->files[j])))return 0;return 1;
    }return 0;
}
static int point(AssetMesh *m,float xyz[3],float uv[2],SdfPoint *out){
    AssetMeshVertex vertex={word(-xyz[0]),word(xyz[1]),word(xyz[2])};uint32_t i;void *next;
    for(i=0;i<m->vertex_count;++i){AssetMeshVertex v=m->vertices[i];
        if(value(v.x_bits)==-xyz[0] && value(v.y_bits)==xyz[1] && value(v.z_bits)==xyz[2])break;}
    if(i==m->vertex_count){next=append(m->vertices,sizeof(vertex),i,&vertex);if(!next)return 0;m->vertices=next;++m->vertex_count;}
    out->vertex=(uint16_t)i;out->uv.u_bits=word(uv[0]);out->uv.v_bits=word(uv[1]);return 1;
}
static int triangle(AssetMesh *m,const SdfPoint *points,uint32_t count,int a,int b,int c,int texture){
    AssetMeshTriangle t={0};int indices[3]={a,b,c};unsigned i;void *next;
    /*Collision/point inputs may have no texture. Preserve that identity;
     * rendered material-less surfaces still need their color consumer. */
    if(texture>=0 && (uint32_t)texture>=m->material_count)return 0;
    t.material=texture<0?UINT16_MAX:(uint16_t)texture;
    for(i=0;i<3;++i){if(indices[i]<0 || (uint32_t)indices[i]>=count)return 0;
        t.vertices[i]=points[indices[i]].vertex;t.uv[i]=points[indices[i]].uv;}
    next=append(m->triangles,sizeof(t),m->triangle_count,&t);if(!next)return 0;
    m->triangles=next;++m->triangle_count;return 1;
}
static int vertex_list(SdfReader *r,AssetMesh *m,int texture,int indexed,SdfPoint **points,uint32_t *count){
    float xyz[3],uv[2]={0};SdfPoint next;uint32_t beginning=*count;void *grown;
    if(!line(r) || !tag(r,"format") || !line(r) || !tag(r,"vertices"))return 0;
    while(line(r) && !tag(r,"}")){
        int n=sscanf(r->line,"{ %f %f %f %f %f",&xyz[0],&xyz[1],&xyz[2],&uv[0],&uv[1]);
        if(n!=(texture<0?3:5))return 0;
        uv[1]=(float)((long double)r->bias-uv[1]);
        if(!point(m,xyz,uv,&next))return 0;
        grown=append(*points,sizeof(next),*count,&next);if(!grown)return 0;*points=grown;++*count;
        if(!indexed && (*count-beginning)%3==0 && !triangle(m,*points,*count,(int)*count-3,(int)*count-2,(int)*count-1,texture))return 0;
    }
    return tag(r,"}") && line(r) && tag(r,"}") && (indexed || (*count-beginning)%3==0);
}
static int indices(SdfReader *r,AssetMesh *m,int texture,const SdfPoint *points,uint32_t count){
    int a,b,c;while(line(r)){if(tag(r,"}"))return 1;
        if(sscanf(r->line,"%d %d %d",&a,&b,&c)!=3 || !triangle(m,points,count,a,b,c,texture))return 0;}return 0;
}
static int model(SdfReader *r,const char *name,uint32_t *index){
    AssetSdfMesh owned={0};SdfPoint *points=NULL;uint32_t count=0;int texture=-1,depth=1,ok=0;char array[256];void *next;
    if(r->result.mesh_count>=256 || !(owned.name=copy(name)))return 0;
    while(line(r)){
        if(tag(r,"}")){if(--depth==0){ok=1;break;}continue;}
        if(tag(r,"Use TexArray")){if(sscanf(r->line,"Use TexArray %255s",array)!=1 || !mesh_materials(r,&owned.mesh,array))break;}
        else if(tag(r,"TexIndex")){if(sscanf(r->line,"TexIndex %d",&texture)!=1)break;}
        else if(tag(r,"vertexList")){free(points);points=NULL;count=0;if(!vertex_list(r,&owned.mesh,texture,1,&points,&count))break;}
        else if(tag(r,"vertexIndices")){if(!indices(r,&owned.mesh,texture,points,count))break;}
        else if(tag(r,"Triangles")){free(points);points=NULL;count=0;
            while(line(r) && !tag(r,"}")){if(tag(r,"vertexList") && !vertex_list(r,&owned.mesh,texture,0,&points,&count))goto done;}
            if(!tag(r,"}"))break;
        }else if(strchr(r->line,'{'))++depth;
    }
done:free(points);if(ok){*index=r->result.mesh_count;
        next=append(r->result.meshes,sizeof(owned),r->result.mesh_count,&owned);
        if(!next)ok=0;else {r->result.meshes=next;++r->result.mesh_count;}}
    if(!ok){free(owned.name);asset_mesh_release(&owned.mesh);}return ok;
}
static int group(SdfReader *r,uint32_t parent,unsigned depth){
    AssetSdfNode n={NONE,parent,NONE,NONE,{1,0,0,0,0,1,0,0,0,0,1,0}};uint32_t id=r->result.node_count,mesh,i;char name[256];void *next;
    if(depth>128 || id>=4096)return 0;
    n.previous_child=r->result.nodes[parent].last_child;
    next=append(r->result.nodes,sizeof(n),id,&n);if(!next)return 0;
    r->result.nodes=next;++r->result.node_count;r->result.nodes[parent].last_child=id;
    while(line(r)){
        if(tag(r,"}"))return 1;
        if(tag(r,"Define Group")){if(!group(r,id,depth+1))return 0;}
        else if(tag(r,"Define Model")){if(sscanf(r->line,"Define Model %255s",name)!=1 || !model(r,name,&mesh))return 0;r->result.nodes[id].mesh=mesh;}
        else if(tag(r,"Use Model")){if(sscanf(r->line,"Use Model %255s",name)!=1)return 0;
            for(mesh=0;mesh<r->result.mesh_count;++mesh)if(!strcmp(name,r->result.meshes[mesh].name))break;
            if(mesh==r->result.mesh_count)return 0;
            r->result.nodes[id].mesh=mesh;
        }else if(tag(r,"Transform")){float row[4];unsigned j;
            for(i=0;i<4;++i){if(!line(r) || sscanf(r->line,"%f %f %f %f",&row[0],&row[1],&row[2],&row[3])!=4)return 0;
                for(j=0;j<3;++j)r->result.nodes[id].local[j*4+i]=row[j];}
            if(!line(r) || !tag(r,"}"))return 0;
        }
    }return 0;
}
void asset_sdf_release(AssetSdf *s){uint32_t i;if(!s)return;
    for(i=0;i<s->mesh_count;++i){free(s->meshes[i].name);asset_mesh_release(&s->meshes[i].mesh);}free(s->meshes);free(s->nodes);memset(s,0,sizeof(*s));
}
int asset_sdf_read(const ResourceBlob *blob,float bias,AssetSdf *out){
    SdfReader *r;AssetSdfNode root={NONE,NONE,NONE,NONE,{1,0,0,0,0,1,0,0,0,0,1,0}};char name[256];uint32_t index,i,j;int ok=0;
    if(!blob || !out || out->meshes || out->nodes || !blob->data || !blob->size)return 0;
    r=calloc(1,sizeof(*r));if(!r)return 0;r->bytes=blob->data;r->size=blob->size;r->bias=bias;
    if(!line(r) || !tag(r,"SDFVersion"))goto done;
    r->result.nodes=append(NULL,sizeof(root),0,&root);if(!r->result.nodes)goto done;
    r->result.node_count=1;
    while(line(r)){
        if(tag(r,"Define MatArray")){if(!skip_block(r))goto done;}
        else if(tag(r,"Define TexArray")){if(sscanf(r->line,"Define TexArray %255s",name)!=1 || !texture_array(r,name))goto done;}
        else if(tag(r,"Define Model")){if(sscanf(r->line,"Define Model %255s",name)!=1 || !model(r,name,&index))goto done;}
        else if(tag(r,"Define Group")){if(!group(r,0,0))goto done;}
    }
    if(!r->result.mesh_count)goto done;
    *out=r->result;memset(&r->result,0,sizeof(r->result));ok=1;
done:asset_sdf_release(&r->result);for(i=0;i<r->texture_count;++i){for(j=0;j<r->textures[i].count;++j)free(r->textures[i].files[j]);free(r->textures[i].files);}free(r);return ok;
}
