"""Read GoldSrc studio MDL v10 assets without modifying the source files."""
import math, struct
from pathlib import Path

class Studio:
    def __init__(self,path):
        self.path=Path(path);self.data=self.path.read_bytes();d=self.data
        if d[:4]!=b'IDST' or self.i(4)!=10 or self.i(72)!=len(d):raise ValueError('Expected intact MDL v10: '+str(path))
        self.bones=[]
        for n in range(self.i(140)):
            at=self.i(144)+112*n
            self.bones.append(dict(name=self.text(at,32),parent=self.i(at+32),controller=self.unpack('6i',at+40),base=self.unpack('6f',at+64),scale=self.unpack('6f',at+88)))
        self.sequences=[]
        for n in range(self.i(164)):
            at=self.i(168)+176*n
            self.sequences.append(dict(name=self.text(at,32),fps=self.f(at+32),flags=self.i(at+36),activity=self.i(at+40),frames=self.i(at+56),motion=self.i(at+68),motionbone=self.i(at+72),movement=self.unpack('3f',at+76),blends=self.i(at+120),anim=self.i(at+124),blendtype=self.unpack('2i',at+128),blendstart=self.unpack('2f',at+136),blendend=self.unpack('2f',at+144),group=self.i(at+156)))
        if any(s['group'] for s in self.sequences):raise ValueError('External sequence groups need explicit import')
        self.textures=[]
        for n in range(self.i(180)):
            at=self.i(184)+80*n
            name=self.text(at,64);flags,w,h,off=self.unpack('4i',at+64)
            self.textures.append(dict(name=name,flags=flags,w=w,h=h,pixels=d[off:off+w*h],palette=d[off+w*h:off+w*h+768]))
        self.skin=self.unpack(str(self.i(192))+'h',self.i(200))
        self.parts=[]
        for n in range(self.i(204)):
            at=self.i(208)+76*n;name=self.text(at,64);count,base,off=self.unpack('3i',at+64)
            models=[]
            for m in range(count):
                a=off+m*112; nm,mi,nv,vi,vp,nn,ni,np=self.unpack('8i',a+72)
                verts=[self.unpack('3f',vp+v*12) for v in range(nv)];bones=list(d[vi:vi+nv])
                normals=[self.unpack('3f',np+v*12) for v in range(nn)]
                tris=[]
                for j in range(nm):
                    _,cmd,skin,_,_=self.unpack('5i',mi+j*20)
                    while True:
                        k=self.unpack('h',cmd)[0];cmd+=2
                        if not k:break
                        corners=[self.unpack('4h',cmd+x*8) for x in range(abs(k))];cmd+=abs(k)*8
                        for v in range(2,len(corners)):
                            ids=(0,v-1,v) if k<0 else ((v-2,v-1,v) if v%2==0 else (v-1,v-2,v))
                            tris.append((self.skin[skin],tuple(corners[q] for q in ids)))
                models.append(dict(name=self.text(a,64),verts=verts,bones=bones,normals=normals,tris=tris))
            self.parts.append(dict(name=name,base=base,models=models))
    def unpack(self,fmt,at):return struct.unpack_from('<'+fmt,self.data,at)
    def i(self,at):return self.unpack('i',at)[0]
    def f(self,at):return self.unpack('f',at)[0]
    def text(self,at,n):return self.data[at:at+n].split(b'\0')[0].decode('latin1')
    def channel(self,anim,axis,frame):
        off=self.unpack('H',anim+axis*2)[0]
        if not off:return 0
        at=anim+off
        while True:
            valid,total=self.data[at:at+2]
            if total==0 or valid>total:raise ValueError('Invalid animation run')
            if frame<total:return self.unpack('h',at+2*(min(frame+1,valid)))[0]
            frame-=total;at+=2*(valid+1)
    def pose(self,seq,frame,blend=0):
        s=self.sequences[seq];result=[]
        for n,b in enumerate(self.bones):
            a=s['anim']+12*(blend*len(self.bones)+n)
            values=[b['base'][k]+self.channel(a,k,frame)*b['scale'][k] for k in range(6)]
            if n==s['motionbone']:
                for k in range(3):
                    if s['motion']&(1<<k):values[k]=0
            result.append(values)
        return result
