"""Read decoded, embedded glTF buffers preserving UVs, transforms and materials."""
import json,struct,sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'ferrari/tools'))
from import_ferrari import transform
class Gltf:
 def __init__(self,path):
  b=Path(path).read_bytes();n=struct.unpack_from('<I',b,12)[0];self.j=json.loads(b[20:20+n]);self.bin=b[28+n:]
 def accessor(self,index):
  a=self.j['accessors'][index];v=self.j['bufferViews'][a['bufferView']]
  typ={5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']];cols={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
  dtype=np.dtype(typ);start=v.get('byteOffset',0)+a.get('byteOffset',0)
  out=np.ndarray((a['count'],cols),dtype=dtype,buffer=self.bin,offset=start,strides=(v.get('byteStride',dtype.itemsize*cols),dtype.itemsize)).copy()
  if a.get('normalized'):out=out.astype(float)/np.iinfo(dtype).max
  return out
 def parts(self):
  result=[]
  def walk(i,parent,path):
   node=self.j['nodes'][i];m=parent@transform(node);path=path+'/'+node.get('name','')
   if 'mesh' in node:
    for p in self.j['meshes'][node['mesh']]['primitives']:
     attrs=p['attributes'];pos=self.accessor(attrs['POSITION']);pos=(np.c_[pos,np.ones(len(pos))]@m.T)[:,:3]
     norm=self.accessor(attrs['NORMAL'])@np.linalg.inv(m[:3,:3]);norm/=np.maximum(np.linalg.norm(norm,axis=1)[:,None],1e-9)
     uv=self.accessor(attrs['TEXCOORD_0']) if 'TEXCOORD_0' in attrs else np.zeros((len(pos),2))
     tri=self.accessor(p['indices']).reshape(-1,3)
     if np.linalg.det(m[:3,:3])<0:tri=tri[:,[0,2,1]]
     result.append(dict(source=path,mi=p.get('material',0),pos=pos,norm=norm,uv=uv,tri=tri))
   for c in node.get('children',[]):walk(c,m,path)
  for i in self.j['scenes'][self.j.get('scene',0)]['nodes']:walk(i,np.eye(4),'')
  return result
if __name__=='__main__':
 for path in sys.argv[1:]:
  g=Gltf(path);ps=g.parts();all=np.concatenate([p['pos'] for p in ps]);print(path,'bounds',all.min(0),all.max(0))
  for p in ps:
   print(p['source'],g.j['materials'][p['mi']].get('name'),np.round(p['pos'].min(0),3),np.round(p['pos'].max(0),3))
