"""Read back the final replacement meshes and guard the exact scope against r22."""
from pathlib import Path
import argparse,hashlib,json,struct,sys
import numpy as np
p=argparse.ArgumentParser();p.add_argument('base',type=Path);p.add_argument('candidate',type=Path);p.add_argument('receipt',type=Path);a=p.parse_args()
r=Path.cwd();sys.path.insert(0,str(r/'tools'));from vinyl_pack import mesh
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inv=lambda d:{p.relative_to(d).as_posix():sha(p) for p in d.rglob('*') if p.is_file()}
before,after=inv(a.base),inv(a.candidate)
allowed={'SHA256.json'}|{f'assets/cars/peugeot_405_glx{s}/mesh/w257_kit{i}_{z}.d8m' for s in ['', '_ui'] for i in [1,2,3] for z in ['fbump','rbump']}|{f'assets/cars/peugeot_405_glx{s}/{p}' for s in ['', '_ui'] for p in ['mesh/mats/p405_inner_surface.pmd','textures/p405_inner_surface.ddt']}
changed=sorted(k for k in before.keys()|after.keys() if before.get(k)!=after.get(k));assert set(changed)<=allowed,changed
rows=[]
for zone,sgn,cut in [('fbump',1,57.65),('rbump',-1,63.9)]:
 for kit in [1,2,3]:
  name=f'w257_kit{kit}_{zone}';path=f'assets/cars/peugeot_405_glx/mesh/{name}.d8m'
  assert after[path]==after[path.replace('/peugeot_405_glx/','/peugeot_405_glx_ui/')]
  m=mesh.read((a.candidate/path).read_bytes());count=0;inner=False
  for part in m['parts']:
   v,n,uv=(part['streams'][s,0] for s in [0,3,5]);t=mesh.triangles(m,part)
   assert np.isfinite(v).all() and np.isfinite(n).all() and np.isfinite(uv).all(),name
   assert len(v)<65536 and t.min()>=0 and t.max()<len(v),name
   assert (np.linalg.norm(n[t],axis=2)>.45).all(),name
   count+=len(t);inner|=part['material']=='p405_inner_surface'
  assert inner,name
  part=m['parts'][0];v=part['streams'][0,0][mesh.triangles(m,part)];c=v.mean(1);n=np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]);mask=(abs(c[:,0])<50)&(np.linalg.norm(n,axis=1)>.0001)
  assert not (n[mask,2]*sgn<-.0001).any(),(name,'inward front-facing fascia')
  rows.append(dict(name=name,triangles=count,parts=len(m['parts']),sha256=after[path]))
for suffix in ['', '_ui']:
 f=a.candidate/f'assets/cars/peugeot_405_glx{suffix}'
 tex=(f/'textures/p405_inner_surface.ddt').read_bytes();assert struct.unpack_from('<3I',tex)==(21,1,1) and tex[47]==255
 assert 'Material Program = Specular Opaque' in (f/'mesh/mats/p405_inner_surface.pmd').read_text()
result=dict(changed=changed,meshes=rows,stock_chassis_and_bumpers_byte_identical=True,other405_assets_and_other_cars_byte_identical=True,physics_and_catalogue_byte_identical=True,race_ui_identical=True,opaque_inner_material_verified=True)
a.receipt.write_text(json.dumps(result,indent=2)+'\n');print('PASS: six bumper pairs, finite attributes/outward fascia, opaque lining; fixed body, stock, other assets and tuning unchanged.')
