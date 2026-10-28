#!/usr/bin/env python3
"""Export source meshes, all animation clips/blends and original skeletons."""
import hashlib,json,math,wave
from pathlib import Path
from studio import Studio
from model_lod import reduce_mesh,animation_bounds

def export_models(assets,output):
    output.mkdir(parents=True,exist_ok=True)
    files={'terror':'player/terror/terror.mdl','glock_view':'v_glock18.mdl','glock_world':'p_glock18.mdl'}
    models={name:Studio(assets/'cstrike/models'/file) for name,file in files.items()}
    target_names=[b['name'] for b in models['terror'].bones]
    text=['/* Generated from local owned MDL files. */','#include "model.h"'];report={};actor_radius=0;body_bounds=None
    def f(v):
        if not math.isfinite(v):raise ValueError('Nonfinite model coordinate')
        return format(v,'.9g')+('f' if '.' in format(v,'.9g') or 'e' in format(v,'.9g') else '.0f')
    def row(v):return '{'+','.join(map(str,v))+'}'
    def arr(t,n,rows):text.append('static const '+t+' '+n+'[]={\n'+',\n'.join(rows)+'\n};')
    def mesh_data(prefix,verts,triangles,m):
        arr('ModelVertex',prefix+'vertices',['{'+row(map(f,v))+','+str(b)+'}' for v,b in verts])
        triangles.sort(key=lambda x:x[0]);corners=[];batches=[];indices=[];used=sorted({t for t,_ in triangles})
        for ti in used:
            texture=m.textures[ti];lookup={};pending=[]
            def flush():
                if not pending:return
                batches.append((len(corners),len(lookup),used.index(ti),len(indices),len(pending)))
                corners.extend(lookup);indices.extend(pending)
                if len(corners)%2:corners.append(corners[-1])
                lookup.clear();pending.clear()
            for t,tri in triangles:
                if t!=ti:continue
                keys=[(v,round(s*64/texture['w']*32),round(tcoord*32/texture['h']*32),255) for v,s,tcoord in tri]
                if len(set(lookup)|set(keys))>64:flush()
                pending.append(tuple(lookup.setdefault(key,len(lookup)) for key in keys))
            flush()
        arr('ModelCorner',prefix+'corners',[row(c) for c in corners])
        arr('ModelBatch',prefix+'batches',[row(b) for b in batches])
        arr('ModelTriangle',prefix+'triangles',['{'+row(t)+'}' for t in indices])
        return corners,batches,used
    for name,m in models.items():
        prefix=name+'_';lower=True;bones=[]
        for b in m.bones:
            if b['name']=='Bip01 Spine':lower=False
            elif b['parent']>=0 and m.bones[b['parent']]['name']=='Bip01 Pelvis':lower=True
            merge=target_names.index(b['name']) if name=='glock_world' and b['name'] in target_names else -1
            bones.append('{%d,%d,%d,%s,%s}'%(b['parent'],merge,int(lower),row(map(f,b['base'])),row(map(f,b['scale']))))
        arr('ModelBone',prefix+'bones',bones)
        # Compact range holding every compressed channel used by every sequence/blend.
        base=min(s['anim'] for s in m.sequences);end=base
        for s in m.sequences:
            for blend in range(s['blends']):
                for b in range(len(m.bones)):
                    a=s['anim']+12*(blend*len(m.bones)+b);end=max(end,a+12)
                    for k in range(6):
                        offset=m.unpack('H',a+k*2)[0]
                        if not offset:continue
                        at=a+offset;frames=0
                        while frames<s['frames']:
                            valid,total=m.data[at:at+2]
                            if not total or valid>total:raise ValueError('Invalid RLE')
                            at+=2*(valid+1);frames+=total
                        end=max(end,at)
        blob=m.data[base:end]
        arr('uint8_t',prefix+'animation',[','.join(map(str,blob[i:i+32])) for i in range(0,len(blob),32)])
        arr('ModelSequence',prefix+'sequences',['{"%s",%s,%d,%d,%d,%d,%d,%d,%s}'%(s['name'],f(s['fps']),s['anim']-base,s['frames'],s['blends'],s['motion'],s['motionbone'],s['flags'],f(math.sqrt(sum(v*v for v in s['movement'])))) for s in m.sequences])
        verts=[];triangles=[]
        for part in m.parts:
            model=part['models'][0] # source default bodygroup, no bomb backpack
            start=len(verts)
            verts.extend((v,b) for v,b in zip(model['verts'],model['bones']))
            for tex,corners in model['tris']:
                triangles.append((tex,[(start+v,s,t) for v,n,s,t in corners]))
        if name in ('terror','glock_world'):
            radius,bounds=animation_bounds(m,verts,body_bounds if name=='glock_world' else None)
            actor_radius=max(actor_radius,radius)
            if name=='terror':body_bounds=bounds
        corners,batches,used=mesh_data(prefix,verts,triangles,m)
        textures=[]
        for ti in used:
            t=m.textures[ti];w,h=t['w'],t['h'];rgb=t['palette'];pix=t['pixels'];pixels=[]
            for y in range(32):
                for x in range(64):
                    ids=[pix[v*w+u] for v in range(y*h//32,max(y*h//32+1,(y+1)*h//32)) for u in range(x*w//64,max(x*w//64+1,(x+1)*w//64))]
                    c=[sum(rgb[i*3+k] for i in ids)//len(ids) for k in range(3)]
                    pixels.append((c[0]>>3)<<11|(c[1]>>3)<<6|(c[2]>>3)<<1|1)
            textures.append(row(pixels))
        text.append('static const uint16_t '+prefix+'textures[][2048] __attribute__((aligned(16)))={'+',\n'.join(textures)+'};')
        text.append('const Model '+name+'_model={'+','.join(map(str,[len(bones),len(m.sequences),len(verts),len(corners),len(batches),len(used)]))+','+','.join(prefix+s for s in ['bones','sequences','vertices','corners','batches','triangles','textures','animation'])+'};')
        report[name]={'source':str(m.path),'sha256':hashlib.sha256(m.data).hexdigest(),'bones':len(bones),'vertices':len(verts),'triangles':len(triangles),'animation_bytes':len(blob),'sequences':m.sequences,'texture_pages':len(used),'all_source_sequences_and_blends_preserved':True}
        if name=='terror':
            report[name]['lods']=[]
            for level,target,error in [(1,480,1.25),(2,260,3.0)]:
                lv,lt,info=reduce_mesh(m,verts,triangles,target,error)
                lp=f'terror_lod{level}_';lc,lb,lu=mesh_data(lp,lv,lt,m)
                assert lu==used,'LOD removed a material'
                text.append('const Model terror_lod%d_model={'%level+','.join(map(str,[len(bones),len(m.sequences),len(lv),len(lc),len(lb),len(used)]))+','+','.join([prefix+'bones',prefix+'sequences',lp+'vertices',lp+'corners',lp+'batches',lp+'triangles',prefix+'textures',prefix+'animation'])+'};')
                report[name]['lods'].append(info)

    text.append('const float actor_bound_radius='+f(actor_radius+.125)+';')
    report['terror']['actor_bound_radius']=actor_radius+.125
    (output/'model_data.c').write_text('\n'.join(text)+'\n')
    (output/'model-report.json').write_text(json.dumps(report,indent=2)+'\n')
    sound_text=['/* Local source PCM, signed mono 22050 Hz. */','#include "sound.h"'];sounds=[];sound_report={}
    for i,file in enumerate(['glock18-1.wav','clipout1.wav','clipin1.wav','sliderelease1.wav']):
        path=assets/'cstrike/sound/weapons'/file
        with wave.open(str(path),'rb') as wav:
            if (wav.getnchannels(),wav.getsampwidth(),wav.getframerate())!=(1,1,22050):raise ValueError(f'Unexpected sound format: {path}')
            samples=wav.readframes(wav.getnframes())
        sound_text.append(f'static const int8_t sound_{i}[]={{'+','.join(str(v-128) for v in samples)+'};')
        sounds.append('{sound_%d,%d}'%(i,len(samples)))
        sound_report[file]={'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'samples':len(samples)}
    sound_text.append('const Sound gun_sounds[4]={'+','.join(sounds)+'};')
    (output/'sound_data.c').write_text('\n'.join(sound_text)+'\n')
    (output/'sound-report.json').write_text(json.dumps(sound_report,indent=2)+'\n')
    print('Models:',{k:{field:v[field] for field in ['bones','triangles','animation_bytes']} for k,v in report.items()})
    return report
