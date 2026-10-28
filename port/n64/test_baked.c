#include "baked.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
 const Model *m=&glock_view_model;BonePose pose[MODEL_MAX_BONES];BoneMatrix bones[MODEL_MAX_BONES];
 VertexPair *reference=malloc(m->corners/2*sizeof(VertexPair)),*baked=malloc(m->corners/2*sizeof(VertexPair));
 model_vertices_init(m,reference);model_vertices_init(m,baked);int maximum=0;unsigned samples=0;
 for(int s=0;s<m->sequences;s++)for(int tick=0;tick<=(m->sequence[s].frames-1)*16;tick++){
  float seconds=tick/(16*m->sequence[s].fps);
  model_pose(m,s,seconds,.5f,.5f,pose);model_matrices(m,pose,bones,NULL);model_skin(m,bones,reference);
  model_baked(m,glock_baked,s,seconds,baked);
  for(int i=0;i<m->corners;i++){
   const int16_t *a=i&1?reference[i/2].posB:reference[i/2].posA,*b=i&1?baked[i/2].posB:baked[i/2].posA;
   for(int k=0;k<3;k++){
    int error=abs(a[k]-b[k]);if(error>maximum)maximum=error;
    if(error>3){fprintf(stderr,"bake error seq=%d tick=%d vertex=%d axis=%d expected=%d actual=%d\n",s,tick,i,k,a[k],b[k]);abort();}
   }
   assert(reference[i/2].rgbaA==baked[i/2].rgbaA && reference[i/2].stA[0]==baked[i/2].stA[0]);
  }
  samples++;
 }
 printf("PASS: %u independently evaluated baked poses, all 13 clips, maximum coordinate error %d/32 units\n",samples,maximum);
 free(reference);free(baked);
}
