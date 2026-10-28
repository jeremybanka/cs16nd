#!/usr/bin/env python3
"""Compare runtime poses against the original MDL, including all nine-way blends."""
import argparse,math,pathlib,struct,subprocess,tempfile
from studio import Studio
ROOT=pathlib.Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--assets',type=pathlib.Path,default=ROOT/'cops-and-robbers')
assets=p.parse_args().assets/'cstrike/models'
models=[Studio(assets/p) for p in ['player/terror/terror.mdl','v_glock18.mdl','p_glock18.mdl']]
source=r'''
#include "model.h"
#include <stdio.h>
int main(void){
 const Model *models[]={&terror_model,&glock_view_model,&glock_world_model};
 BonePose pose[MODEL_MAX_BONES];
 for(int m=0;m<3;m++)for(int s=0;s<models[m]->sequences;s++){
  const ModelSequence *q=&models[m]->sequence[s];
  int frames[]={0,(q->frames-1)/2,q->frames>1?q->frames-2:0};
  for(int b=0;b<q->blends;b++)for(int f=0;f<3;f++){
   float x=q->blends==9?(b%3)*.5f:.5f,y=q->blends==9?(b/3)*.5f:.5f;
   model_pose(models[m],s,frames[f]/q->fps,x,y,pose);
   fwrite(pose,sizeof(BonePose),models[m]->bones,stdout);
  }
 }
}
'''
with tempfile.TemporaryDirectory(prefix='cs64-pose-') as directory:
    path=pathlib.Path(directory);(path/'probe.c').write_text(source)
    subprocess.run(['cc','-std=c17','-O1','-g','-fsanitize=address,undefined','-I'+str(ROOT/'port/n64'),str(path/'probe.c'),str(ROOT/'port/n64/model.c'),str(ROOT/'build/n64/model_data.c'),'-lm','-o',str(path/'probe')],check=True)
    raw=subprocess.check_output([str(path/'probe')])
at=0;count=0;max_position=0;max_rotation=0
for m in models:
    for si,s in enumerate(m.sequences):
        for blend in range(s['blends']):
            for frame in [0,(s['frames']-1)//2,max(0,s['frames']-2)]:
                for expected in m.pose(si,frame,blend):
                    actual=struct.unpack_from('=7f',raw,at);at+=28;count+=1
                    sx,sy,sz=[math.sin(x/2) for x in expected[3:]]
                    cx,cy,cz=[math.cos(x/2) for x in expected[3:]]
                    quaternion=[sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz]
                    position=max(abs(a-b) for a,b in zip(actual[:3],expected[:3]))
                    rotation=abs(1-abs(sum(a*b for a,b in zip(actual[3:],quaternion))))
                    assert position<.003,(m.path,s['name'],frame,blend,position)
                    assert rotation<.00001,(m.path,s['name'],frame,blend,rotation)
                    max_position=max(position,max_position);max_rotation=max(rotation,max_rotation)
assert at==len(raw)
print(f'PASS: {count} source bone poses; all clips and blend endpoints; max position error {max_position:.6f}, quaternion error {max_rotation:.8f}')
