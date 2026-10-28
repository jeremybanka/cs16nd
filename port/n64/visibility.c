#include "visibility.h"
#include <math.h>
#include <string.h>
#include <assert.h>
int sphere_visible(const Player *camera,Vec3 center,float radius,int fullscreen){
    Vec3 diff=sub(center,player_eye(camera)),dir=player_direction(camera);
    Vec3 right={sinf(camera->yaw),-cosf(camera->yaw),0};
    Vec3 up={-cosf(camera->yaw)*sinf(camera->pitch),-sinf(camera->yaw)*sinf(camera->pitch),cosf(camera->pitch)};
    float depth=dot(diff,dir),v=fullscreen?.70020754f:.46630766f,h=fullscreen?v*(320.0f/240):v*(320.0f/120);
    return depth+radius>=4 && depth-radius<=8000 && fabsf(dot(diff,right))<=depth*h+radius*sqrtf(1+h*h) && fabsf(dot(diff,up))<=depth*v+radius*sqrtf(1+v*v);
}
int world_candidates(int leaf,const uint16_t *order,int count,uint16_t *output){
    enum {MAX_FACES=8192,MAX_PVS=2048};uint8_t visible[MAX_FACES]={0},pvs[MAX_PVS];
    assert(face_count<=MAX_FACES && (world_leaf_count+7)/8<=MAX_PVS);
    int offset=leaves[leaf].vis,bytes=(world_leaf_count+7)/8;memset(pvs,255,sizeof(pvs));
    if(offset>=0){int out=0;while(out<bytes){int b=visibility[offset++];if(b)pvs[out++]=b;else{int n=visibility[offset++];while(n-- && out<bytes)pvs[out++]=0;}}}
    for(int i=1;i<=world_leaf_count;i++)if((pvs[(i-1)/8]>>((i-1)%8))&1)
        for(int j=0;j<leaves[i].count;j++)visible[marks[leaves[i].first+j]]=1;
    for(int i=world_faces;i<face_count;i++)visible[i]=1;
    int used=0;for(int i=0;i<count;i++)if(visible[order[i]])output[used++]=order[i];
    return used;
}
