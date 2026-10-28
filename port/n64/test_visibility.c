#include "visibility.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
 uint16_t order[8192],actual[8192];int count=0;uint8_t expected[8192],bits[2048];
 for(int t=0;t<texture_count;t++)for(int f=0;f<face_count;f++)if(faces[f].count && faces[f].texture==t)order[count++]=f;
 for(int leaf=0;leaf<leaf_count;leaf++){
  memset(bits,255,sizeof(bits));memset(expected,0,sizeof(expected));int offset=leaves[leaf].vis;
  if(offset>=0){int cursor=0;while(cursor<(world_leaf_count+7)/8){int byte=visibility[offset++];if(byte)bits[cursor++]=byte;else{int zeros=visibility[offset++];assert(zeros);while(zeros-- && cursor<(world_leaf_count+7)/8)bits[cursor++]=0;}}}
  for(int i=1;i<=world_leaf_count;i++)if(bits[(i-1)/8]&(1<<((i-1)%8)))for(int j=0;j<leaves[i].count;j++)expected[marks[leaves[i].first+j]]=1;
  for(int i=world_faces;i<face_count;i++)expected[i]=1;
  int n=world_candidates(leaf,order,count,actual),at=0;
  for(int i=0;i<count;i++)if(expected[order[i]]){assert(at<n && actual[at]==order[i]);at++;}
  assert(at==n);
 }
 Player p={0};p.pos=(Vec3){0,0,-28}; /* standing eye at the origin */
 for(int mode=0;mode<2;mode++)for(int i=0;i<100000;i++){
  float depth=4+(rand()%799600)/100.f,v=mode?.70020754f:.46630766f,h=v*320/(mode?240:120);
  Vec3 inside={depth,depth*h*((rand()%2001)-1000)/1000.f,depth*v*((rand()%2001)-1000)/1000.f};
  float radius=(rand()%40000)/100.f;Vec3 delta={(rand()%2001)-1000.f,(rand()%2001)-1000.f,(rand()%2001)-1000.f};
  float len=sqrtf(dot(delta,delta));if(len>0)delta=mul(delta,radius/len*.999f);
  assert(sphere_visible(&p,add(inside,delta),radius,mode));
 }
 puts("PASS: exact ordered BSP candidate lists for every leaf; 200000 conservative sphere/frustum probes");
}
