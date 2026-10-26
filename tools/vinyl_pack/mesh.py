"""Small lossless reader/writer for the native Juiced D8M vertex streams."""
import struct
import numpy as np

def read(data):
    u=lambda off:struct.unpack_from('<I',data,off)[0]
    n=u(0);indices=np.frombuffer(data,dtype='<u2',count=n,offset=4).copy()
    at=4+2*n;count=u(at);at+=4;parts=[]
    for _ in range(count):
        start=at;material=data[at:at+32].split(b'\0')[0].decode();at+=32
        first,num,primitive=struct.unpack_from('<3I',data,at);at+=12+44+12
        ndecl=u(at);at+=4
        decl=[struct.unpack_from('<HHBBBB',data,at+8*i) for i in range(ndecl)];at+=8*ndecl
        stride,nverts=struct.unpack_from('<2I',data,at);at+=8
        verts=bytearray(data[at:at+stride*nverts]);at+=len(verts)
        streams={}
        for stream,offset,kind,method,usage,idx in decl:
            if stream!=0:continue
            if usage in (0,3,5) and kind in (1,2,3):
                streams[(usage,idx)]=np.ndarray((nverts,kind+1),dtype='<f4',buffer=verts,offset=offset,strides=(stride,4))
        parts.append(dict(material=material,header=bytearray(data[start:at-len(verts)]),
                          verts=verts,streams=streams,first=first,count=num,primitive=primitive))
    assert at+44==len(data),(at,len(data))
    return dict(prefix=data[:4+2*n+4],indices=indices,parts=parts,bounds=data[at:])

def write(mesh):
    return mesh['prefix']+b''.join(bytes(p['header'])+bytes(p['verts']) for p in mesh['parts'])+mesh['bounds']

def boh(data):
    at=0
    def node(parent):
        nonlocal at
        name=data[at:at+32].split(b'\0')[0].decode();at+=32
        mat=np.array(struct.unpack_from('<16f',data,at)).reshape(4,4);at+=64
        attach=struct.unpack_from('<I',data,at)[0];at+=4
        mesh=None
        if attach==0:mesh=data[at:at+32].split(b'\0')[0].decode();at+=32
        elif attach==2:n=struct.unpack_from('<I',data,at)[0];at+=4+n
        else:raise ValueError(attach)
        n=struct.unpack_from('<I',data,at)[0];at+=4
        world=mat if parent is None else parent@mat
        yield name,mesh,world
        for _ in range(n):yield from node(world)
    while at<len(data):yield from node(None)

def triangles(mesh,p):
    idx=mesh['indices'][p['first']:p['first']+p['count']]
    if p['primitive']==1:return idx.reshape(-1,3).astype(int)
    out=[];run=[]
    for n in idx:
        if n==65535:run=[];continue
        run.append(int(n))
        if len(run)>=3:
            a,b,c=run[-3:]
            if len(run)%2==0:a,b=b,a
            if a!=b and b!=c and a!=c:out.append((a,b,c))
    return np.array(out,dtype=int).reshape(-1,3)

def encode(pos,norm,uv,material):
    # Independent triangle vertices keep UV zone seams from interpolating across the atlas.
    assert len(pos)<65535
    tri=np.arange(len(pos),dtype='<u2')
    lo=pos.min(0);hi=pos.max(0);centre=(lo+hi)/2
    bounds=struct.pack('<11f',*centre,1,float(np.linalg.norm(pos-centre,axis=1).max()),*lo,*hi)
    out=struct.pack('<I',len(tri))+tri.tobytes()+struct.pack('<I',1)
    out+=material.encode().ljust(32,b'\0')+struct.pack('<III',0,len(tri),1)+bounds
    out+=struct.pack('<ffII',0,1e12,2,5)
    for off,typ,usage in [(0,2,0),(12,2,3),(24,4,10),(28,1,5)]:
        out+=struct.pack('<HHBBBB',0,off,typ,0,usage,0)
    out+=struct.pack('<HHBBBB',255,0,17,0,0,0)+struct.pack('<II',36,len(pos))
    verts=np.zeros(len(pos),dtype=[('p','<f4',3),('n','<f4',3),('c','<u4'),('uv','<f4',2)])
    verts['p']=pos;verts['n']=norm;verts['c']=0x00ffffff;verts['uv']=uv
    return out+verts.tobytes()+bounds
