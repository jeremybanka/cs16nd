#include "baked.h"
#include <math.h>
#include <assert.h>
void model_baked(const Model *m,const BakedClip *clips,int sequence,float seconds,VertexPair *out){
    const ModelSequence *s=&m->sequence[sequence];const BakedClip *clip=&clips[sequence];
    float frame=seconds*s->fps;
    if(s->flags&1){frame=fmodf(frame,fmaxf(1,s->frames-1));if(frame<0)frame+=s->frames-1;}
    else frame=fmaxf(0,fminf(frame,s->frames-1));
    float tick=frame*8;unsigned lo=0,hi=clip->frames-1;
    while(lo+1<hi){unsigned mid=(lo+hi)/2;if(clip->ticks[mid]<=tick)lo=mid;else hi=mid;}
    int span=clip->ticks[hi]-clip->ticks[lo];
    int fraction=span?(int)((tick-clip->ticks[lo])*256/span):0;
    if(fraction>256)fraction=256;
    const int16_t (*a)[3]=clip->positions+lo*clip->vertices,(*b)[3]=clip->positions+hi*clip->vertices;
    ModelPosition positions[MODEL_MAX_VERTICES];assert(clip->vertices==m->vertices);
    for(int i=0;i<m->vertices;i++){
        positions[i].xyz[0]=a[i][0]+((b[i][0]-a[i][0])*fraction)/256;
        positions[i].xyz[1]=a[i][1]+((b[i][1]-a[i][1])*fraction)/256;
        positions[i].xyz[2]=a[i][2]+((b[i][2]-a[i][2])*fraction)/256;
        positions[i].xyz[3]=0;
    }
    model_scatter(m,positions,out);
}
