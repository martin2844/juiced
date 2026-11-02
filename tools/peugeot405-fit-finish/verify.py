"""Independent read-back checks for the exported replacement, not its authoring objects.
Usage: python verify.py BASE CANDIDATE RECEIPT
"""
from pathlib import Path
import argparse,collections,hashlib,json,sys
import numpy as np

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('base',type=Path);p.add_argument('candidate',type=Path);p.add_argument('receipt',type=Path)
a=p.parse_args();r=Path.cwd();sys.path.insert(0,str(r/'tools'))
from vinyl_pack import mesh
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inventory=lambda d:{p.relative_to(d).as_posix():sha(p) for p in d.rglob('*') if p.is_file()}
old,new=inventory(a.base),inventory(a.candidate)
names=['chass','fbump','rbump']+['kit'+str(i)+'_'+z for i in [1,2,3] for z in ['fbump','rbump']]
allowed={'SHA256.json'}|{f'assets/cars/peugeot_405_glx{s}/mesh/mats/{n}.pmd' for s in ['', '_ui'] for n in ['p405_trim','p405_rubber']}|{f'assets/cars/peugeot_405_glx{s}/mesh/w257_{n}.d8m' for s in ['', '_ui'] for n in names}
changed=sorted(k for k in old.keys()|new.keys() if old.get(k)!=new.get(k))
assert set(changed)<=allowed,changed
def load(root,name,suffix=''):
 return mesh.read((root/f'assets/cars/peugeot_405_glx{suffix}/mesh/w257_{name}.d8m').read_bytes())
def areas(root):
 result=collections.defaultdict(float)
 for name in ['chass','fbump','rbump']:
  m=load(root,name)
  for part in m['parts']:
   v=part['streams'][0,0][mesh.triangles(m,part)].astype(float)
   result[part['material']]+=float(np.linalg.norm(np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]),axis=1).sum()/2)
 return dict(result)
before,after=areas(a.base),areas(a.candidate)
assert before.keys()==after.keys()
for mat in before:assert abs(before[mat]-after[mat])<max(.005,before[mat]*2e-6),(mat,before[mat],after[mat])
rows=[]
for name in names:
 path=f'assets/cars/peugeot_405_glx/mesh/w257_{name}.d8m'
 assert new[path]==new[path.replace('/peugeot_405_glx/','/peugeot_405_glx_ui/')]
 m=load(a.candidate,name);count=0;positions=[]
 if name.startswith('kit'):
  part=m['parts'][0];v=part['streams'][0,0][mesh.triangles(m,part)];c=v.mean(1)
  n=np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]);front=name.endswith('fbump');cut=54.5 if front else 60.
  mask=(c[:,1]>cut+.1)&(abs(c[:,0])<60)
  assert mask.sum()>100,(name,'mounting collar too sparse')
  assert not (n[mask,2]*(1 if front else -1)<-1e-7).any(),(name,'inward-facing mounting collar')
 for part in m['parts']:
  v,n,uv=(part['streams'][s,0] for s in [0,3,5]);t=mesh.triangles(m,part)
  assert np.isfinite(v).all() and np.isfinite(n).all() and np.isfinite(uv).all()
  assert t.min()>=0 and t.max()<len(v)<65536
  assert (np.linalg.norm(n[t],axis=2)>.5).all()
  count+=len(t);positions.extend(v[t].reshape(-1,3))
  if name=='chass' and not part['material'].startswith('p405_'):
   c=v[t].mean(1)
   assert not ((c[:,1]<58.25-1e-3)&(c[:,2]>161.45+1e-3)).any(),'Factory front shell still in fixed chassis'
   assert not ((c[:,1]<64.5-1e-3)&(c[:,2]<-161.45-1e-3)).any(),'Factory rear shell still in fixed chassis'
  if name.startswith('kit'):
   assert part['material'] not in {'w257_m0','w257_m4','w257_m5'},(name,part['material'])
   if part['material']=='w257_m6':assert name.endswith('rbump') and len(t)==2,'Only the original plate may remain'
 positions=np.asarray(positions);rows.append(dict(name=name,triangles=count,minimum=positions.min(0).tolist(),maximum=positions.max(0).tolist(),sha256=new[path]))
# Fixed front/rear lights survive removal of the bumper slots, with exact surface area.
fixed=load(a.candidate,'chass');mats={p['material'] for p in fixed['parts']}
assert {'w257_m3','w257_m4'}<=mats
for lamp in ['w257_m3','w257_m4']:
 fixed_area=0.
 for part in fixed['parts']:
  if part['material']!=lamp:continue
  v=part['streams'][0,0][mesh.triangles(fixed,part)].astype(float)
  fixed_area+=float(np.linalg.norm(np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]),axis=1).sum()/2)
 assert abs(fixed_area-before[lamp])<.005
result=dict(changed=changed,meshes=rows,factory_surface_area_by_material_before=before,factory_surface_area_by_material_after=after,fixed_body_contains_no_factory_bumper_shell=True,lights_remain_on_fixed_body=True,other_assets_and_tuning_byte_identical=True,only405_reveal_and_rubber_materials_changed=True,race_ui_identical=True)
a.receipt.write_text(json.dumps(result,indent=2)+'\n');print('PASS: 9 race/UI mesh pairs, factory area conserved per material; no stock shell under kits; other assets/tuning unchanged.')
