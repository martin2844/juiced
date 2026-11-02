"""Independent exported-mesh checks against the installed r18 baseline."""
from pathlib import Path
import argparse,hashlib,json,sys
import numpy as np
p=argparse.ArgumentParser();p.add_argument('baseline',type=Path);p.add_argument('candidate',type=Path);p.add_argument('receipt',type=Path);a=p.parse_args()
r=Path.cwd();sys.path.insert(0,str(r/'tools'));from vinyl_pack import mesh
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inv=lambda d:{p.relative_to(d).as_posix():sha(p) for p in d.rglob('*') if p.is_file()}
before,after=inv(a.baseline),inv(a.candidate)
changed=[k for k in before.keys()|after.keys() if before.get(k)!=after.get(k)]
assert all(k=='SHA256.json' or k.startswith(('assets/cars/peugeot_405_glx/mesh/','assets/cars/peugeot_405_glx_ui/mesh/')) for k in changed),changed
rows=[];f=a.candidate/'assets/cars/peugeot_405_glx/mesh'
retained_records={bytes(p['verts']) for zone in ['fbump','rbump','sskirt_l','sskirt_r'] for p in mesh.read((f/('w257_'+zone+'.d8m')).read_bytes())['parts']}
for name in sorted(k for k in changed if k.startswith('assets/cars/peugeot_405_glx/mesh/') and k.endswith('.d8m')):
 path=a.candidate/name;aa=mesh.read(path.read_bytes());tris=0;lo=[];hi=[]
 assert (a.candidate/name.replace('/peugeot_405_glx/','/peugeot_405_glx_ui/')).read_bytes()==path.read_bytes()
 for part in aa['parts']:
  pos=part['streams'][0,0];norm=part['streams'][3,0];t=mesh.triangles(aa,part)
  assert len(pos)<65536 and np.isfinite(pos).all() and np.isfinite(norm).all()
  assert t.min()>=0 and t.max()<len(pos)
  assert (f/'mats'/(part['material']+'.pmd')).is_file(),part['material']
  lengths=np.linalg.norm(norm,axis=1)
  if bytes(part['verts']) in retained_records or (path.name.endswith('bonnet.d8m') and part is aa['parts'][0]):
   # The source front body has deliberately untouched 0.815..1 normals.
   assert (lengths>.8).all() and (lengths<1.02).all()
  else:assert (lengths>.98).all() and (lengths<1.02).all(),(path,part['material'],lengths.min(),lengths.max())
  lo.append(pos.min(0));hi.append(pos.max(0));tris+=len(t)
 rows.append(dict(mesh=path.name,triangles=tris,bounds=[np.min(lo,axis=0).tolist(),np.max(hi,axis=0).tolist()]))
# Plain carbon must reproduce the factory pressing exactly, including normals;
# compare expanded triangle corners, independent of its export vertex packing.
stock=mesh.read((f/'w257_bonnet.d8m').read_bytes());s=stock['parts'][0];sidx=mesh.triangles(stock,s).ravel()
carbon=mesh.read((f/'w257_sport_bonnet.d8m').read_bytes());c=carbon['parts'][0];cidx=mesh.triangles(carbon,c).ravel()
assert np.array_equal(s['streams'][0,0][sidx],c['streams'][0,0][cidx])
assert np.array_equal(s['streams'][3,0][sidx],c['streams'][3,0][cidx])
seamchecks=[]
for name in ['w257_vented_paint_bonnet','w257_vented_carbon_bonnet']:
 aa=mesh.read((f/(name+'.d8m')).read_bytes());p=aa['parts'][0];pos=p['streams'][0,0];normal=p['streams'][3,0]
 # All four outer seams are source vertices, with source normals: clipping the
 # central inlet may not alter an exterior edge or its original crease.
 n=0
 for point,norm in zip(s['streams'][0,0],s['streams'][3,0]):
  if abs(point[0])<20 and 125<point[2]<170:continue
  ix=np.where(np.linalg.norm(pos-point,axis=1)<1e-5)[0]
  assert len(ix) and np.min(np.linalg.norm(normal[ix]-norm,axis=1))<2e-6,(name,point)
  n+=1
 seamchecks.append(dict(mesh=name,source_vertices_and_normals_preserved=n))
receipt=dict(changed=sorted(changed),catalogue_byte_identical=True,other_cars_and_physics_byte_identical=True,ui_race_meshes_identical=True,plain_carbon_factory_positions_exact=True,plain_carbon_factory_normals_exact=True,hood_seams=seamchecks,meshes=rows,limitations='Finite source normals and unit authored normals, native material/index integrity and preserved source seams; aesthetic acceptance remains with the user.')
a.receipt.write_text(json.dumps(receipt,indent=2)+'\n');print('Verified',len(rows),'meshes; stock hood positions/normals and all outer scoop-hood source seams preserved.')
