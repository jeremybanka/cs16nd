"""Bake all viewmodel clips using the reference skeletal evaluator.

Adaptive samples keep the dense 1/8-source-frame reference within 1/32 unit
per coordinate. A separate 1/16-frame target-independent test checks the
runtime decoder. Asset-derived output stays in the ignored build directory.
"""
import array, hashlib, json, pathlib, subprocess
ROOT=pathlib.Path(__file__).resolve().parents[2]

def bake(output):
    inputs=[output/'model_data.c',ROOT/'port/n64/model.c',ROOT/'port/n64/model.h',ROOT/'port/n64/world.h',ROOT/'port/n64/perf.h',ROOT/'port/n64/game.h',pathlib.Path(__file__)]
    key=hashlib.sha256(b''.join(p.read_bytes() for p in inputs)).hexdigest()
    report=output/'bake-report.json'
    if report.exists() and json.loads(report.read_text()).get('key')==key and (output/'baked_data.c').exists():return
    probe=output/'bake-probe.c'
    probe.write_text(r'''
#include "model.h"
#include <stdio.h>
#include <math.h>
int main(void){
 const Model *m=&glock_view_model;BonePose pose[MODEL_MAX_BONES];BoneMatrix bones[MODEL_MAX_BONES];
 for(int s=0;s<m->sequences;s++)for(int tick=0;tick<=(m->sequence[s].frames-1)*8;tick++){
  float duration=model_duration(m,s),seconds=tick/(8*m->sequence[s].fps);
  if(seconds>=duration && duration>0)seconds=nextafterf(duration,-INFINITY);
  model_pose(m,s,seconds,.5f,.5f,pose);model_matrices(m,pose,bones,NULL);
  for(int v=0;v<m->vertices;v++){
   const ModelVertex *in=&m->vertex[v];const BoneMatrix *b=&bones[in->bone];float x[]={in->pos.x,in->pos.y,in->pos.z},p[3];
   for(int j=0;j<3;j++){p[j]=b->m[j][3];for(int k=0;k<3;k++)p[j]+=b->m[j][k]*x[k];}
   int16_t q[]={model_round_position(p[0]*32),model_round_position(p[2]*32),model_round_position(-p[1]*32)};
   fwrite(q,sizeof(q),1,stdout);
  }
 }
}
''')
    exe=output/'bake-probe'
    subprocess.run(['cc','-std=c17','-O2','-I'+str(ROOT/'port/n64'),str(probe),str(ROOT/'port/n64/model.c'),str(output/'model_data.c'),'-lm','-o',str(exe)],check=True)
    raw=array.array('h');raw.frombytes(subprocess.check_output([str(exe)]))
    source=json.loads((output/'model-report.json').read_text())['glock_view'];width=source['vertices']*3;at=0;entries=[];rows=['/* Generated private animated positions. */','#include "baked.h"'];reports=[]
    for si,s in enumerate(source['sequences']):
        count=(s['frames']-1)*8+1;frames=[raw[at+i*width:at+(i+1)*width] for i in range(count)];at+=count*width
        chosen={0,count-1}
        def refine(a,b):
            if b-a<=1:return
            maximum=0;worst=None
            for tick in range(a+1,b):
                fraction=((tick-a)*256)//(b-a)
                error=max(abs(int(x+(y-x)*fraction/256)-z) for x,y,z in zip(frames[a],frames[b],frames[tick]))
                # Decoder truncates the signed delta, not the final coordinate.
                error=max(error,max(abs(x+int((y-x)*fraction/256)-z) for x,y,z in zip(frames[a],frames[b],frames[tick])))
                if error>maximum:maximum=error;worst=tick
            if maximum>1:chosen.add(worst);refine(a,worst);refine(worst,b)
        # Retain every original frame boundary; subdivision never changes clip timing.
        for a in range(0,count-1,8):chosen.add(a);chosen.add(a+8);refine(a,a+8)
        chosen=sorted(chosen);name=f'baked_{si}'
        rows.append(f'static const uint16_t {name}_ticks[]={{'+','.join(map(str,chosen))+'};')
        rows.append(f'static const int16_t {name}_positions[][3]={{'+',\n'.join('{'+','.join(str(v) for v in frames[t][i:i+3])+'}' for t in chosen for i in range(0,width,3))+'};')
        entries.append('{%d,%d,%s_ticks,%s_positions}'%(len(chosen),source['vertices'],name,name))
        reports.append({'name':s['name'],'source_frames':s['frames'],'baked_frames':len(chosen),'bytes':len(chosen)*(width*2+2)})
    assert at==len(raw)
    rows.append('const BakedClip glock_baked[]={'+','.join(entries)+'};')
    (output/'baked_data.c').write_text('\n'.join(rows)+'\n')
    report.write_text(json.dumps({'key':key,'coordinate_scale':32,'source_frame_subdivisions':8,'reference_error_units':1/32,'bytes':sum(r['bytes'] for r in reports),'clips':reports},indent=2)+'\n')
    print('Baked viewmodel:',sum(r['bytes'] for r in reports),'bytes, all',len(reports),'clips')
