"""Transfer each retail decal's actual UV footprint onto a new side shell.

Intersection takes place in the side projection: generated vertices lie on the
receiver's triangles and retain donor barycentric UVs. No hood duplication,
rectangular UV stretching, or triangles spanning windows/wheel openings.
"""
import sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'tools/vinyl_pack'))
from mesh import read,triangles,boh

_cached_shell=None
_cached_receivers=None

def receivers_for(shell):
    global _cached_shell,_cached_receivers
    if shell is _cached_shell:return _cached_receivers
    receivers={-1:[],1:[]}
    for p in shell:
        v=p['pos'][p['tri']];n=p['norm'][p['tri']]
        face=np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0])
        length=np.linalg.norm(face,axis=1)
        for side in [-1,1]:
            mask=(v[:,:,0].mean(1)*side>0)&(face[:,0]*side>.35*length)&(abs(face[:,0])>1e-7)
            receivers[side].extend((a[:,[2,1]],a,b) for a,b in zip(v[mask],n[mask]))
    _cached_shell=shell;_cached_receivers=receivers
    return receivers


def cross(a,b):return a[0]*b[1]-a[1]*b[0]

def intersect(subject,clip):
    """Clip 2D convex polygon; preserve interpolated extra columns."""
    polygon=list(subject)
    sign=1 if cross(clip[1]-clip[0],clip[2]-clip[0])>0 else -1
    for a,b in zip(clip,np.roll(clip,-1,axis=0)):
        result=[]
        if not polygon:break
        for p,q in zip(polygon,polygon[1:]+polygon[:1]):
            dp=sign*cross(b-a,p[:2]-a);dq=sign*cross(b-a,q[:2]-a)
            if dp>=-1e-8:result.append(p)
            if (dp>=-1e-8)!=(dq>=-1e-8):result.append(p+(q-p)*dp/(dp-dq))
        polygon=result
    return polygon


def transfer(retail,style,shell,wheelbase,height,material):
    donor='focus_2'
    nodes=list(boh(retail.read(f'cars/{donor}_ui/models/{donor}_stock.boh')))
    wheels={n:m[:3,3] for n,_,m in nodes if n.startswith('WHEEL_')}
    zfront=wheels['WHEEL_FL'][2];zrear=wheels['WHEEL_RL'][2]
    scalez=wheelbase/(zfront-zrear);mid=(zfront+zrear)/2
    mesh=read(retail.read(f'cars/{donor}_ui/mesh/{donor}_{style.lower()}.d8m'))
    # The Focus side artwork fits below its 101cm belt line. Keep that relation
    # to the receiver height; side projection never selects the roof or bonnet.
    scaley=height/145.0
    receivers=receivers_for(shell)
    outputs=[]
    for side,rec in receivers.items():
        if not rec:continue
        rlo=np.array([t[0].min(0) for t in rec]);rhi=np.array([t[0].max(0) for t in rec]);points=[]
        for part in mesh['parts']:
            pos=part['streams'][0,0];uv=part['streams'][5,0]
            for tri in triangles(mesh,part):
                v=pos[tri]
                if np.any(v[:,0]*side<35):continue
                yz=np.c_[(v[:,2]-mid)*scalez,v[:,1]*scaley]
                if abs(cross(yz[1]-yz[0],yz[2]-yz[0]))<1e-7:continue
                eligible=np.flatnonzero(np.all(rhi>=yz.min(0)-1e-6,axis=1)&np.all(rlo<=yz.max(0)+1e-6,axis=1))
                for i in eligible:
                    ry,rv,rn=rec[i]
                    poly=intersect(np.c_[yz,uv[tri]],ry)
                    if len(poly)<3:continue
                    a=np.array(poly);matrix=np.c_[ry[1]-ry[0],ry[2]-ry[0]]
                    weights=np.linalg.solve(matrix,(a[:,:2]-ry[0]).T).T
                    weights=np.c_[1-weights.sum(1),weights]
                    p=weights@rv;n=weights@rn;n/=np.maximum(np.linalg.norm(n,axis=1)[:,None],1e-9)
                    a=np.c_[p+n*.08,n,a[:,2:]]
                    for j in range(1,len(a)-1):
                        t=a[[0,j,j+1]]
                        face=np.cross(t[1,:3]-t[0,:3],t[2,:3]-t[0,:3])
                        if np.linalg.norm(face)<1e-7:continue
                        if face@t[:,3:6].mean(0)<0:t=t[[0,2,1]]
                        points.extend(t)
        if points:
            a=np.array(points)
            for start in range(0,len(a),60000):
                chunk=a[start:start+60000]
                outputs.append(dict(mat=material,pos=chunk[:,:3],norm=chunk[:,3:6],uv=chunk[:,6:8],tri=np.arange(len(chunk)).reshape(-1,3)))
    if not outputs:raise ValueError('No decal intersection for '+style)
    return outputs
