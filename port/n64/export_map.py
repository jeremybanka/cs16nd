#!/usr/bin/env python3
"""Convert locally owned GoldSrc BSP30/WAD3 data into compact N64 C arrays."""
import argparse, collections, hashlib, json, math, pathlib, re, struct

def records(data, fmt):
    return list(struct.iter_unpack('<'+fmt, data))

def load_bsp(path):
    data = path.read_bytes()
    if struct.unpack_from('<i',data)[0] != 30:
        raise ValueError('Expected GoldSrc BSP version 30')
    lumps=[]
    for off,size in records(data[4:124], 'ii'):
        if off<0 or size<0 or off+size>len(data): raise ValueError('Invalid lump bounds')
        lumps.append(data[off:off+size])
    return data,lumps

def export(bsp, wad_dirs, output):
    raw,l=load_bsp(bsp)
    entities=[dict(re.findall(r'"([^"]*)"\s*"([^"]*)"',s)) for s in re.findall(r'\{([^}]*)\}',l[0].decode('latin1'))]
    wad_textures={}
    wad_hashes={}
    for directory in wad_dirs:
        for path in sorted(directory.glob('*.wad')):
            data=path.read_bytes()
            if data[:4]!=b'WAD3': continue
            num,off=struct.unpack_from('<ii',data,4)
            for pos,size,_,typ,compression,_,name in struct.iter_unpack('<iiiBBH16s', data[off:off+num*32]):
                if typ==67 and not compression:
                    wad_textures.setdefault(name.split(b'\0')[0].decode('latin1').lower(),(data[pos:pos+size],path))
    tex=[]
    num=struct.unpack_from('<i',l[2])[0]
    for off in struct.unpack_from('<'+'i'*num,l[2],4):
        if off<0: raise ValueError('Missing texture entry')
        name,w,h,*mips=struct.unpack_from('<16s6I',l[2],off)
        name=name.split(b'\0')[0].decode('latin1')
        data=l[2][off:]
        if not mips[0]:
            if name.lower() not in wad_textures: raise ValueError('Missing WAD texture '+name)
            data,path=wad_textures[name.lower()]
            wad_hashes[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
            _,w,h,*mips=struct.unpack_from('<16s6I',data)
        paloff=mips[3]+(w//8)*(h//8)+2
        pixels=data[mips[0]:mips[0]+w*h]
        palette=data[paloff:paloff+768]
        if len(pixels)!=w*h or len(palette)!=768: raise ValueError('Truncated texture '+name)
        # Area average keeps small brick/grain detail stable at 320x240.
        colors=[]
        for y in range(32):
            for x in range(32):
                samples=[pixels[sy*w+sx] for sy in range(y*h//32,max(y*h//32+1,(y+1)*h//32)) for sx in range(x*w//32,max(x*w//32+1,(x+1)*w//32))]
                rgb=[sum(palette[s*3+c] for s in samples)//len(samples) for c in range(3)]
                alpha=0 if name.startswith('{') and samples.count(255)>len(samples)/2 else 1
                colors.append(((rgb[0]>>3)<<11)|((rgb[1]>>3)<<6)|((rgb[2]>>3)<<1)|alpha)
        tex.append((name,w,h,colors))
    points=records(l[3],'3f'); edge=records(l[12],'HH'); se=[x[0] for x in records(l[13],'i')]
    ti=records(l[6],'8fii'); fs=records(l[7],'Hhihh4Bi'); models=records(l[14],'9f7i')
    planes=records(l[1],'4fi'); nodes=records(l[5],'i2h6h2H'); clips=records(l[9],'i2h'); leaves=records(l[10],'ii6h2H4B')
    output.mkdir(parents=True,exist_ok=True)
    dest=output/'map_data.c'
    out=['/* Generated from local game data. Do not distribute. */','#include "world.h"']
    def arr(ctype,name,rows):out.append('const '+ctype+' '+name+'[]={\n'+',\n'.join(rows)+'\n};')
    def fl(v): return format(v,'.9g')+('f' if '.' in format(v,'.9g') or 'e' in format(v,'.9g') else '.0f')
    def vec(v):return '{'+','.join(fl(x) for x in v)+'}'
    arr('Plane','planes',['{'+vec(p[:3])+','+fl(p[3])+'}' for p in planes])
    arr('Node','nodes',['{%d,{%d,%d}}'%p[:3] for p in nodes])
    arr('Node','clips',['{%d,{%d,%d}}'%p for p in clips])
    arr('Node','point_nodes',['{%d,{%d,%d}}'%(p[0],*(leaves[-1-c][0] if c<0 else c for c in p[1:3])) for p in nodes])
    arr('Leaf','leaves',['{%d,%d,%d}'%(p[1],p[8],p[9]) for p in leaves])
    arr('uint16_t','marks',[str(x[0]) for x in records(l[11],'H')])
    arr('uint8_t','visibility',[','.join(map(str,l[4][i:i+32])) for i in range(0,len(l[4]),32)])
    brush_entities={int(e['model'][1:]):e for e in entities if e.get('model','').startswith('*')}
    solid_classes={'func_wall','func_breakable','func_door','func_door_rotating'}
    hulls=[models[0][10]]
    crouch_hulls=[models[0][12]];point_hulls=[models[0][9]]
    render_models={0}
    for i,m in enumerate(models[1:],1):
        e=brush_entities.get(i,{})
        if e.get('classname') in solid_classes:
            if e.get('origin','0 0 0')!='0 0 0': raise ValueError('Translated solid brush needs local-space tracing')
            hulls.append(m[10])
            crouch_hulls.append(m[12]);point_hulls.append(m[9])
        if e.get('classname') in solid_classes|{'func_illusionary'}:render_models.add(i)
    included=set()
    for i in render_models:included.update(range(models[i][14],models[i][14]+models[i][15]))
    vrows=[];frows=[];triangles=0;usedtex={};max_vertices=0
    for fi,(plane,side,first,n,info,*lighting) in enumerate(fs):
        t=ti[info];name,w,h,colors=tex[t[8]]
        if fi not in included or name.lower() in ('sky','clip','origin','aaatrigger','null'):
            frows.append('{0,0,0,0,0,{0,0,0},0}');continue
        if n>64:raise ValueError('Face exceeds Tiny3D vertex cache')
        material=usedtex.setdefault(t[8],len(usedtex))
        verts=[points[edge[abs(se[j])][0 if se[j]>=0 else 1]] for j in range(first,first+n)]
        uvs=[(sum(p[k]*t[k] for k in range(3))+t[3],sum(p[k]*t[k+4] for k in range(3))+t[7]) for p in verts]
        mins=[math.floor(min(uv[a] for uv in uvs)/16) for a in range(2)]
        dims=[math.ceil(max(uv[a] for uv in uvs)/16)-mins[a]+1 for a in range(2)]
        uvbase=[math.floor(uvs[0][0]/w)*32,math.floor(uvs[0][1]/h)*32]
        packed=[]
        lightofs=lighting[-1]
        for p,uv in zip(verts,uvs):
            color=[210]*3
            if lightofs>=0:
                s=max(0,min(dims[0]-1,round(uv[0]/16-mins[0])))
                v=max(0,min(dims[1]-1,round(uv[1]/16-mins[1])))
                at=lightofs+3*(v*dims[0]+s)
                color=[min(255,round(255*(l[8][at+c]/255)**0.65)) for c in range(3)]
            rgba=(color[0]<<24)|(color[1]<<16)|(color[2]<<8)|255
            st=[round((uv[a]*32/(w if a==0 else h)-uvbase[a])*32) for a in range(2)]
            if any(abs(v)>32767 for v in st):raise ValueError('UV overflow')
            packed.append(([round(p[0]),round(p[2]),round(-p[1])],rgba,st))
        start=len(vrows)
        if len(packed)%2:packed.append(packed[-1])
        for a,b in zip(packed[::2],packed[1::2]):
            ints=lambda v:'{'+','.join(map(str,v))+'}'
            vrows.append('{%s,0,%s,0,0x%08x,0x%08x,%s,%s}'%(ints(a[0]),ints(b[0]),a[1],b[1],ints(a[2]),ints(b[2])))
        center=[sum(p[k] for p in verts)/n for k in range(3)]
        radius=max(math.dist(center,p) for p in verts)
        frows.append('{%d,%d,%d,%d,%d,%s,%s}'%(start,n,material,plane,side,vec(center),fl(radius)))
        triangles+=n-2;max_vertices=max(max_vertices,n)
    arr('VertexPair __attribute__((aligned(16)))','vertices',vrows)
    arr('Face','faces',frows)
    out.append('const uint16_t textures[][1024] __attribute__((aligned(16)))={\n'+',\n'.join('{'+','.join(map(str,tex[i][3]))+'}' for i in usedtex)+'\n};')
    arr('int','hulls',list(map(str,hulls)))
    arr('int','crouch_hulls',list(map(str,crouch_hulls)))
    arr('int','point_hulls',list(map(str,point_hulls)))
    starts=[e for e in entities if e.get('classname') in ('info_player_deathmatch','info_player_start')]
    arr('Spawn','spawns',['{'+vec(map(float,e['origin'].split()))+','+fl(float(e.get('angles','0 '+e.get('angle','0')+' 0').split()[1])*math.pi/180)+'}' for e in starts])
    counts={'face_count':len(fs),'world_leaf_count':models[0][13],'world_faces':models[0][15],'leaf_count':len(leaves),'texture_count':len(usedtex),'hull_count':len(hulls),'spawn_count':len(starts),'vertex_pairs':len(vrows)}
    out.extend('const int %s=%d;'%(k,v) for k,v in counts.items())
    dest.write_text('\n'.join(out)+'\n')
    report=dict(counts,triangles=triangles,planes=len(planes),clipnodes=len(clips),nodes=len(nodes),max_face_vertices=max_vertices,vertex_bytes=len(vrows)*32,texture_bytes=len(usedtex)*2048,bsp_sha256=hashlib.sha256(raw).hexdigest(),bsp=str(bsp),wads=wad_hashes,texture_names=[tex[i][0] for i in usedtex])
    (output/'map-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('texture_names','wads')},indent=2))
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('bsp',type=pathlib.Path);p.add_argument('--wad-dir',type=pathlib.Path,action='append',default=[]);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('build/n64'))
    a=p.parse_args();export(a.bsp,a.wad_dir,a.output)
