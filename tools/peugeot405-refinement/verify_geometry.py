"""Independent native mesh and source-preservation checks for the405 refinement."""
from pathlib import Path
import argparse,sys,hashlib,json
import numpy as np
p=argparse.ArgumentParser();p.add_argument('base',type=Path);p.add_argument('candidate',type=Path);p.add_argument('receipt',type=Path);a=p.parse_args();sys.path.insert(0,str(Path.cwd()/'tools'));from vinyl_pack import mesh
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();inv=lambda d:{p.relative_to(d).as_posix():sha(p) for p in d.rglob('*') if p.is_file()}
before,after=inv(a.base),inv(a.candidate);changed=sorted(k for k in before.keys()|after.keys() if before.get(k)!=after.get(k))
names=['w257_chass']+['w257_kit'+str(i)+'_rbump' for i in [1,2,3]]+['w257_wing'+str(i) for i in [1,2,3]]
allowed={'SHA256.json','patches/scripts/dynamic_peugeot_405_glx.txt.patch'}|{f'assets/cars/peugeot_405_glx{s}/mesh/{n}.d8m' for s in ['', '_ui'] for n in names}
assert set(changed)<=allowed,changed
base=a.base/'assets/cars/peugeot_405_glx/mesh';out=a.candidate/'assets/cars/peugeot_405_glx/mesh';sourcechass=mesh.read((base/'w257_chass.d8m').read_bytes());newchass=mesh.read((out/'w257_chass.d8m').read_bytes())
assert len(newchass['parts'])>len(sourcechass['parts'])
for old,new in zip(sourcechass['parts'],newchass['parts']):
 assert old['material']==new['material'] and bytes(old['verts'])==bytes(new['verts'])
 assert np.array_equal(mesh.triangles(sourcechass,old),mesh.triangles(newchass,new))
retained={bytes(p['verts']) for zone in ['chass','rbump'] for p in mesh.read((base/('w257_'+zone+'.d8m')).read_bytes())['parts']}
rows=[]
for name in names:
 path=out/(name+'.d8m');data=path.read_bytes();aa=mesh.read(data);tris=0;lo=[];hi=[]
 assert (a.candidate/'assets/cars/peugeot_405_glx_ui/mesh'/path.name).read_bytes()==data
 for part in aa['parts']:
  pos=part['streams'][0,0];norm=part['streams'][3,0];t=mesh.triangles(aa,part)
  assert np.isfinite(pos).all() and np.isfinite(norm).all() and len(pos)<65536
  assert t.min()>=0 and t.max()<len(pos) and (out/'mats'/(part['material']+'.pmd')).is_file()
  if bytes(part['verts']) not in retained:
   length=np.linalg.norm(norm,axis=1);assert np.allclose(length,1,atol=2e-5),(name,part['material'])
  tris+=len(t);lo.append(pos.min(0));hi.append(pos.max(0))
 rows.append(dict(name=name,triangles=tris,bounds=[np.min(lo,axis=0).tolist(),np.max(hi,axis=0).tolist()]))
# Keep the actual original two-triangle number plate, including texture data.
old=mesh.read((base/'w257_rbump.d8m').read_bytes());plate=[]
for p in old['parts']:
 if p['material']!='w257_m6':continue
 for t in mesh.triangles(old,p):
  v=p['streams'][0,0][t]
  if abs(v[:,2]+227.13).max()<.02 and abs(v[:,0]).max()<28 and v[:,1].min()>41 and v[:,1].max()<55:plate.append(v)
assert len(plate)==2
for i in [1,2,3]:
 aa=mesh.read((out/('w257_kit'+str(i)+'_rbump.d8m')).read_bytes());triangles=[v for p in aa['parts'] if p['material']=='w257_m6' for v in p['streams'][0,0][mesh.triangles(aa,p)]]
 assert all(any(np.array_equal(v,t) for t in triangles) for v in plate)
receipt=dict(changed=changed,meshes=rows,all_original_chassis_vertices_normals_uvs_and_faces_unchanged=True,original_plate_retained_on_all_kits=True,ui_race_identity=True,other_cars_catalogue_fronts_hoods_skirts_byte_identical=True,scope='7 native mesh pairs and optional405-only dynamics curve; no personal profiles')
a.receipt.write_text(json.dumps(receipt,indent=2)+'\n');print('Verified7 native meshes, original chassis/plate preservation and unchanged other cars.')
