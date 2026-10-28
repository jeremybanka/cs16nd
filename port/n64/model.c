#include "model.h"
#include "perf.h"
#include <math.h>
#include <string.h>
#include <assert.h>
static float clampf(float x,float lo,float hi){return fmaxf(lo,fminf(hi,x));}
static unsigned u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int channel(const uint8_t *p,int axis,int frame){
    unsigned off=u16(p+axis*2);if(!off)return 0;p+=off;
    while(frame>=p[1]){assert(p[1]);frame-=p[1];p+=2*(p[0]+1);}
    unsigned k=frame<p[0]?(unsigned)frame+1:p[0];return (int16_t)u16(p+2*k);
}
static void quaternion(const float *a,float *q){
    float sx=sinf(a[0]*.5f),cx=cosf(a[0]*.5f),sy=sinf(a[1]*.5f),cy=cosf(a[1]*.5f),sz=sinf(a[2]*.5f),cz=cosf(a[2]*.5f);
    q[0]=sx*cy*cz-cx*sy*sz;q[1]=cx*sy*cz+sx*cy*sz;q[2]=cx*cy*sz-sx*sy*cz;q[3]=cx*cy*cz+sx*sy*sz;
}
void model_mix(BonePose *a,const BonePose *b,float t){
    if(t<=.00001f)return;
    if(t>=.99999f){*a=*b;return;}
    float dotq=0;for(int k=0;k<4;k++)dotq+=a->q[k]*b->q[k];
    float sign=dotq<0?-1:1;dotq=fabsf(dotq);float wa=1-t,wb=t;
    if(dotq<.9995f){float angle=acosf(clampf(dotq,-1,1)),inv=1/sinf(angle);wa=sinf((1-t)*angle)*inv;wb=sinf(t*angle)*inv;}
    float len=0;for(int k=0;k<4;k++){a->q[k]=a->q[k]*wa+b->q[k]*wb*sign;len+=a->q[k]*a->q[k];}
    len=1/sqrtf(len);for(int k=0;k<4;k++)a->q[k]*=len;
    for(int k=0;k<3;k++)a->p[k]+=(b->p[k]-a->p[k])*t;
}
static void sample(const Model *m,const ModelSequence *s,int b,int blend,int frame,BonePose *p){
    const ModelBone *bone=&m->bone[b];const uint8_t *anim=m->animation+s->offset+12*(blend*m->bones+b);
    float a[6];for(int k=0;k<6;k++)a[k]=bone->base[k]+channel(anim,k,frame)*bone->scale[k];
    for(int k=0;k<3;k++)p->p[k]=a[k];
    quaternion(a+3,p->q);
    if(b==s->motionbone)for(int k=0;k<3;k++)if(s->motion&(1<<k))p->p[k]=0;
}
/* Shared LRU of decoded keyframes: source RLE and Euler trig run once per key,
   not once per viewport or interpolated sample. About 58 KiB on N64. */
typedef struct { const Model *model; int seq,blend,frame; unsigned used; BonePose pose[MODEL_MAX_BONES]; } Cache;
static Cache cache[32];static unsigned stamp;
static const BonePose *keyframe(const Model *m,int seq,int blend,int frame){
    Cache *old=&cache[0];
    for(int i=0;i<32;i++){
        Cache *c=&cache[i];
        if(c->model==m && c->seq==seq && c->blend==blend && c->frame==frame){PERF_COUNTER(PC_CACHE_HIT,1);c->used=++stamp;return c->pose;}
        if(c->used<old->used)old=c;
    }
    old->model=m;old->seq=seq;old->blend=blend;old->frame=frame;old->used=++stamp;
    PERF_COUNTER(PC_CACHE_MISS,1);
    for(int b=0;b<m->bones;b++)sample(m,&m->sequence[seq],b,blend,frame,&old->pose[b]);
    return old->pose;
}
static void blended_frame(const Model *m,int seq,int blend,float frame,BonePose *pose){
    int first=(int)frame,next=first+1;
    if(next>=m->sequence[seq].frames)next=first;
    const BonePose *a=keyframe(m,seq,blend,first),*b=keyframe(m,seq,blend,next);
    for(int i=0;i<m->bones;i++){pose[i]=a[i];model_mix(&pose[i],&b[i],frame-first);}
}
int model_sequence(const Model *m,const char *name){for(int i=0;i<m->sequences;i++)if(!strcmp(m->sequence[i].name,name))return i;return -1;}
float model_duration(const Model *m,int seq){assert(seq>=0 && seq<m->sequences);return (m->sequence[seq].frames-1)/m->sequence[seq].fps;}
void model_pose(const Model *m,int seq,float seconds,float yaw_blend,float pitch_blend,BonePose *pose){
    assert(seq>=0 && seq<m->sequences && m->bones<=MODEL_MAX_BONES);
    const ModelSequence *s=&m->sequence[seq];float frame=seconds*s->fps;
    if(s->flags&1){frame=fmodf(frame,fmaxf(1,s->frames-1));if(frame<0)frame+=s->frames-1;}else frame=clampf(frame,0,s->frames-1);
    float bx=clampf(yaw_blend,0,1)*2,by=clampf(pitch_blend,0,1)*2;
    int x=bx>=1?1:0,y=by>=1?1:0;bx-=x;by-=y;
    if(s->blends!=9){blended_frame(m,seq,0,frame,pose);return;}
    BonePose right[MODEL_MAX_BONES],bottom[MODEL_MAX_BONES];
    blended_frame(m,seq,y*3+x,frame,pose);
    if(bx>.00001f){
        blended_frame(m,seq,y*3+x+1,frame,right);
        for(int b=0;b<m->bones;b++)model_mix(&pose[b],&right[b],bx);
    }
    if(by>.00001f){
        blended_frame(m,seq,(y+1)*3+x,frame,bottom);
        if(bx>.00001f){
            blended_frame(m,seq,(y+1)*3+x+1,frame,right);
            for(int b=0;b<m->bones;b++)model_mix(&bottom[b],&right[b],bx);
        }
        for(int b=0;b<m->bones;b++)model_mix(&pose[b],&bottom[b],by);
    }
}
void model_matrices(const Model *m,const BonePose *pose,BoneMatrix *out,const BoneMatrix *merge){
    for(int b=0;b<m->bones;b++){
        if(merge && m->bone[b].merge>=0){out[b]=merge[m->bone[b].merge];continue;}
        const float *q=pose[b].q;float x=q[0],y=q[1],z=q[2],w=q[3];
        BoneMatrix local={.m={{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),pose[b].p[0]},
            {2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),pose[b].p[1]},
            {2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),pose[b].p[2]}}};
        int parent=m->bone[b].parent;
        if(parent<0){out[b]=local;continue;}
        for(int i=0;i<3;i++)for(int j=0;j<4;j++){
            float v=j==3?out[parent].m[i][3]:0;
            for(int k=0;k<3;k++)v+=out[parent].m[i][k]*local.m[k][j];
            out[b].m[i][j]=v;
        }
    }
}
void model_skin(const Model *m,const BoneMatrix *bones,VertexPair *out){
    PERF_START(transform_time);
    Vec3 positions[MODEL_MAX_VERTICES];assert(m->vertices<=MODEL_MAX_VERTICES);
    for(int i=0;i<m->vertices;i++){
        const ModelVertex *v=&m->vertex[i];const BoneMatrix *b=&bones[v->bone];float in[3]={v->pos.x,v->pos.y,v->pos.z},p[3];
        for(int j=0;j<3;j++){p[j]=b->m[j][3];for(int k=0;k<3;k++)p[j]+=b->m[j][k]*in[k];}
        positions[i]=(Vec3){p[0],p[2],-p[1]};
    }
    PERF_DETAIL(PD_SKIN_TRANSFORM,transform_time);
    PERF_START(pack_time);
    for(int i=0;i<m->corners;i++){
        const ModelCorner *v=&m->corner[i];Vec3 p=positions[v->vertex];VertexPair *pair=&out[i/2];
        int16_t *pos=i&1?pair->posB:pair->posA,*uv=i&1?pair->stB:pair->stA;
        pos[0]=lroundf(p.x*32);pos[1]=lroundf(p.y*32);pos[2]=lroundf(p.z*32);uv[0]=v->s;uv[1]=v->t;
        if(i&1){pair->normB=0;pair->rgbaB=0xffffffff;}else{pair->normA=0;pair->rgbaA=0xffffffff;}
    }
    PERF_DETAIL(PD_SKIN_PACK,pack_time);
}
