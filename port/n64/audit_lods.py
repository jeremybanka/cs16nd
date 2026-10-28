#!/usr/bin/env python3
"""Render private model LOD comparisons and check their animated displacement.

Standard-library orthographic software rasterization is a visual supplement;
actual Tiny3D clipping and RDP validation are checked separately in ares.
"""
import argparse,json,math,pathlib,struct,zlib
from studio import Studio
from model_lod import reduce_mesh,posed_positions
ROOT=pathlib.Path(__file__).resolve().parents[2]

def png(path,w,h,rgb):
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    scan=b''.join(b'\0'+rgb[y*w*3:(y+1)*w*3] for y in range(h))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b''))

def render(model,verts,tris,sequence,frame,angle,height):
    size=48;pixels=bytearray(bytes((28,32,38))*size*size);depth=[-1e30]*(size*size);mask=set();scale=height/72
    points=posed_positions(model,verts,sequence,frame)
    co,si=math.cos(angle),math.sin(angle)
    xy=[(24+(co*x-si*y)*scale,26-z*scale,si*x+co*y) for x,y,z in points]
    for material,face in tris:
        a,b,c=[xy[v] for v,s,t in face];tex=model.textures[material]
        area=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
        if abs(area)<1e-8:continue
        for y in range(max(0,math.floor(min(a[1],b[1],c[1]))),min(size,math.ceil(max(a[1],b[1],c[1])))):
            for x in range(max(0,math.floor(min(a[0],b[0],c[0]))),min(size,math.ceil(max(a[0],b[0],c[0])))):
                px,py=x+.5,y+.5
                w1=((px-a[0])*(c[1]-a[1])-(py-a[1])*(c[0]-a[0]))/area
                w2=((b[0]-a[0])*(py-a[1])-(b[1]-a[1])*(px-a[0]))/area;weights=(1-w1-w2,w1,w2)
                if min(weights)<-1e-6:continue
                z=sum(w*p[2] for w,p in zip(weights,(a,b,c)));i=y*size+x
                if z<depth[i]:continue
                depth[i]=z;mask.add(i)
                u=int(sum(w*p[1] for w,p in zip(weights,face)))%tex['w'];v=int(sum(w*p[2] for w,p in zip(weights,face)))%tex['h']
                color=tex['pixels'][v*tex['w']+u]*3;pixels[3*i:3*i+3]=tex['palette'][color:color+3]
    return pixels,mask

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--assets',type=pathlib.Path,default=ROOT/'cops-and-robbers');a=p.parse_args()
    model=Studio(a.assets/'cstrike/models/player/terror/terror.mdl');verts=[];tris=[]
    for part in model.parts:
        m=part['models'][0];start=len(verts);verts.extend(zip(m['verts'],m['bones']))
        tris.extend((t,[(v+start,s,u) for v,n,s,u in c]) for t,c in m['tris'])
    meshes=[(verts,tris,None)]+[reduce_mesh(model,verts,tris,target,error) for target,error in [(480,1.25),(260,3)]]
    rows=[];metrics=[];poses=[(19,0),(3,8),(2,0),(101,10)]
    for seq,frame in poses:
        frame=min(frame,model.sequences[seq]['frames']-1)
        for angle in (0,math.pi/2):
            images=[]
            for height in (24,10):
                rendered=[render(model,v,t,seq,frame,angle,height) for v,t,_ in meshes]
                for level,(pixels,mask) in enumerate(rendered):
                    images.append(pixels)
                    if level:
                        ref=rendered[0][1];metrics.append({'sequence':seq,'angle':angle,'pixels':height,'lod':level,'silhouette_iou':len(ref&mask)/max(1,len(ref|mask))})
            rows.append(images)
    w,h=6*48*2,len(rows)*48*2;atlas=bytearray(w*h*3)
    for row,images in enumerate(rows):
        for col,pixels in enumerate(images):
            for y in range(96):
                for x in range(96):
                    src=((y//2)*48+x//2)*3;dst=((row*96+y)*w+col*96+x)*3;atlas[dst:dst+3]=pixels[src:src+3]
    out=ROOT/'build/n64';png(out/'lod-audit.png',w,h,atlas)
    accepted=[m for m in metrics if (m['pixels']==24 and m['lod']==1) or (m['pixels']==10 and m['lod']==2)]
    report={'columns':'24px original/LOD1/LOD2, 10px original/LOD1/LOD2; 2x nearest display','rows':'idle,walk,crouch,death; front/side for each','mean_gate_iou':sum(m['silhouette_iou'] for m in accepted)/len(accepted),'minimum_gate_iou':min(m['silhouette_iou'] for m in accepted),'meshes':[{k:v for k,v in info.items() if k!='source_vertices'} for _,_,info in meshes[1:]],'views':metrics}
    (out/'lod-audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='views'},indent=2))
if __name__=='__main__':main()
