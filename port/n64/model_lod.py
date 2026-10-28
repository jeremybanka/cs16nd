"""Conservative endpoint edge collapse for rigid GoldSrc skin vertices.

Only vertices attached to the same bone can merge. Material/UV seams and open
boundaries stay fixed; each surviving endpoint is an original source vertex.
The displacement bound holds in every animated pose, since a bone is rigid.
"""
import math
from collections import defaultdict

def posed_positions(model,verts,sequence=0,frame=0):
    matrices=[]
    for b,pose in zip(model.bones,model.pose(sequence,frame)):
        p=pose[:3];sx,sy,sz=[math.sin(x/2) for x in pose[3:]];cx,cy,cz=[math.cos(x/2) for x in pose[3:]]
        x,y,z,w=sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz
        m=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),p[0]],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),p[1]],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),p[2]]]
        if b['parent']>=0:
            parent=matrices[b['parent']];m=[[sum(parent[i][k]*m[k][j] for k in range(3))+(parent[i][3] if j==3 else 0) for j in range(4)] for i in range(3)]
        matrices.append(m)
    return [tuple(sum(matrices[b][i][j]*p[j] for j in range(3))+matrices[b][i][3] for i in range(3)) for p,b in verts]

def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def normal(a,b,c):
    u,v=sub(b,a),sub(c,a);return (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])

def reduce_mesh(model,verts,source,target,max_error):
    points=posed_positions(model,verts);faces=[(t,list(c)) for t,c in source];members={i:{i} for i in range(len(verts))}
    uv=defaultdict(set)
    for t,c in faces:
        for v,s,u in c:uv[v].add((t,s,u))
    fixed={v for v,values in uv.items() if len(values)!=1}
    positions=defaultdict(list)
    for i,(p,b) in enumerate(verts):positions[(tuple(p),b)].append(i)
    fixed.update(i for values in positions.values() if len(values)>1 for i in values)
    maximum=0;collapses=0
    while len(faces)>target:
        edges=defaultdict(list);neighbors=defaultdict(set);incident=defaultdict(list)
        for fi,(t,c) in enumerate(faces):
            ids=[v[0] for v in c]
            for v in ids:incident[v].append(fi)
            for a,b in zip(ids,ids[1:]+ids[:1]):edges[tuple(sorted((a,b)))].append(fi);neighbors[a].add(b);neighbors[b].add(a)
        pinned=fixed|{v for e,fs in edges.items() if len(fs)!=2 for v in e}
        candidates=[]
        for (a,b),fs in edges.items():
            if len(fs)!=2 or verts[a][1]!=verts[b][1]:continue
            # The edge link condition preserves topology.
            opposite={v[0] for fi in fs for v in faces[fi][1]}-{a,b}
            if neighbors[a]&neighbors[b]!=opposite:continue
            for keep,remove in [(a,b),(b,a)]:
                if remove in pinned or len(uv[keep])!=1:continue
                distance=max(math.dist(verts[keep][0],verts[v][0]) for v in members[keep]|members[remove])
                if distance>max_error:continue
                cost=0;valid=True
                for fi in incident[remove]:
                    ids=[v[0] for v in faces[fi][1]]
                    if keep in ids:continue
                    old=[points[v] for v in ids];new=[points[keep if v==remove else v] for v in ids]
                    n0,n1=normal(*old),normal(*new);length=dot(n0,n0);length1=dot(n1,n1)
                    if length1<1e-12 or dot(n0,n1)<=.25*math.sqrt(length*length1):valid=False;break
                    cost+=dot(n0,sub(points[keep],points[remove]))**2/max(length,1e-12)
                if valid:candidates.append((cost+.01*distance*distance,keep,remove,distance))
        if not candidates:break
        _,keep,remove,distance=min(candidates);maximum=max(maximum,distance);members[keep]|=members.pop(remove);collapses+=1
        new=[];keep_uv=next(iter(uv[keep]))[1:]
        for t,c in faces:
            ids=[v[0] for v in c]
            if keep in ids and remove in ids:continue
            new.append((t,[(keep,*keep_uv) if v==remove else (v,s,u) for v,s,u in c]))
        faces=new
    selected=sorted({v for t,c in faces for v,s,u in c});indices={v:i for i,v in enumerate(selected)}
    reduced=[(t,[(indices[v],s,u) for v,s,u in c]) for t,c in faces]
    # Recompute the final bound, including accumulated collapse chains.
    bound=max((math.dist(verts[v][0],verts[src][0]) for v in selected for src in members[v]),default=0)
    assert bound<=max_error+1e-6
    return [verts[v] for v in selected],reduced,{'triangles':len(faces),'vertices':len(selected),'source_triangles':len(source),'target':target,'max_displacement':bound,'limit':max_error,'collapses':collapses,'source_vertices':selected}


def animation_bounds(model,verts,merged=None):
    """Pose-independent radius from translation extrema and rigid bone lengths."""
    extent=[[0.,0.,0.] for b in model.bones]
    for seq in model.sequences:
        for blend in range(seq['blends']):
            for bi,b in enumerate(model.bones):
                anim=seq['anim']+12*(blend*len(model.bones)+bi)
                for axis in range(3):
                    if bi==seq['motionbone'] and seq['motion']&(1<<axis):continue
                    offset=model.unpack('H',anim+2*axis)[0];values=[0]
                    if offset:
                        at=anim+offset;frames=0;values=[]
                        while frames<seq['frames']:
                            valid,total=model.data[at:at+2]
                            assert total and valid<=total
                            values.extend(model.unpack(str(valid)+'h',at+2));frames+=total;at+=2*(valid+1)
                    extent[bi][axis]=max(extent[bi][axis],max(abs(b['base'][axis]+v*b['scale'][axis]) for v in values))
    bounds=[]
    for bi,b in enumerate(model.bones):
        if merged and b['name'] in merged:bounds.append(merged[b['name']])
        else:bounds.append((bounds[b['parent']] if b['parent']>=0 else 0)+math.sqrt(sum(v*v for v in extent[bi])))
    return max(bounds[b]+math.sqrt(sum(v*v for v in p)) for p,b in verts),dict(zip((b['name'] for b in model.bones),bounds))
