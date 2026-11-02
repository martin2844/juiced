"""Private first-wave Sport1 replacement from reviewed stock + fitted additions.

python THIS REPO BASELINE_RUNTIME STUDY FRESH_CANDIDATE
Existing part/roster indices, stock and kit2/3 remain unchanged.
"""
from pathlib import Path
import hashlib,json,shutil,struct,sys
import numpy as np
repo,base,study,out=map(Path,sys.argv[1:]);sys.path[:0]=[str(repo/'tools'),str(repo/'mods/street-cars/tools'),str(repo/'mods/ferrari/tools')]
from vinyl_pack import mesh
from gltf import Gltf
from import_ferrari import mesh_bytes,bounds
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
if out.exists():raise RuntimeError('Fresh candidate required')
shutil.copytree(base,out);records=[]
for model,prefix in [('integra_db8','db8'),('bmw_e36','e36'),('levin_ae92','ae92')]:
 report=json.loads((study/(model+'-kit-study.json')).read_text())
 for rel,h in report['stock_sha256'].items():assert sha(repo/rel)==h,rel
 g=Gltf(study/(model+'-kit1.glb'));zones={z:[] for z in ['fbump','rbump','sskirt_l','sskirt_r']}
 notch=next(p['exhaust_clearance_x_m'] for p in report['parts'] if p['level']==1 and p['part']=='REAR')
 for p in g.parts():
  name=p['source'].upper();zone='fbump' if 'FRONT_' in name else 'rbump' if 'REAR_' in name else 'sskirt_l' if 'SKIRT_L_' in name else 'sskirt_r' if 'SKIRT_R_' in name else None;assert zone is not None,name
  p['pos']*=100;p['mat']=g.j['materials'][p['mi']]['name'];assert p['mat']==model+'_body'
  assert np.isfinite(p['pos']).all() and np.isfinite(p['norm']).all() and np.allclose(np.linalg.norm(p['norm'],axis=1),1,atol=1e-4)
  assert p['pos'][:,1].min()>9
  v=p['pos'][p['tri']];assert np.all(np.linalg.norm(np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]),axis=1)>1e-8)
  _,inverse=np.unique(np.round(p['pos'],4),axis=0,return_inverse=True);t=inverse[p['tri']];edges=np.sort(np.concatenate([t[:,[0,1]],t[:,[1,2]],t[:,[2,0]]]),axis=1);assert np.all(np.unique(edges,axis=0,return_counts=True)[1]==2)
  if zone=='rbump':assert p['pos'][:,0].max()<notch[0]*100+.01 or p['pos'][:,0].min()>notch[1]*100-.01,('blocked exhaust',name)
  zones[zone].append(p)
 for zone,extra in zones.items():
  assert extra,(model,zone)
  for suffix in ['','_ui']:
   folder=out/'assets/cars'/(model+suffix)/'mesh';stock=folder/(prefix+'_'+zone+'.d8m');target=folder/(prefix+'_sport_'+zone+'.d8m');assert target.exists();a=mesh.read(stock.read_bytes());b=mesh.read(mesh_bytes(extra));indices=np.concatenate([a['indices'],b['indices']]).astype('<u2');payload=struct.pack('<I',len(indices))+indices.tobytes()+struct.pack('<I',len(a['parts'])+len(b['parts']))
   for p in a['parts']:payload+=bytes(p['header'])+bytes(p['verts'])
   for p in b['parts']:
    header=bytearray(p['header']);struct.pack_into('<I',header,32,p['first']+len(a['indices']));payload+=header+p['verts']
   payload+=bounds(np.concatenate([p['streams'][0,0] for p in a['parts']+b['parts']]));target.write_bytes(payload);new=mesh.read(payload)
   assert np.array_equal(new['indices'][:len(a['indices'])],a['indices'])
   for p,q in zip(a['parts'],new['parts']):assert p['header']==q['header'] and p['verts']==q['verts']
   records.append({'path':target.relative_to(out).as_posix(),'sha256':sha(target),'stock_sha256':sha(stock),'added_triangles':sum(len(p['tri']) for p in extra),'all_stock_parts_retained_byte_for_byte':True})
old={p.relative_to(base).as_posix():sha(p) for p in base.rglob('*') if p.is_file()};new={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file()};assert old.keys()==new.keys();assert {p for p in old if old[p]!=new[p]}=={p['path'] for p in records}
(out/'SHA256.json').write_text(json.dumps({p:h for p,h in new.items() if p!='SHA256.json'},indent=2)+'\n')
(study/'sport1-native-candidate-receipt.json').write_text(json.dumps({'status':'PRIVATE; geometry/graphics review required','baseline':str(base),'candidate':str(out),'meshes':records,'other_files_preserved':True},indent=2)+'\n');print('Replaced24 existing Sport1 meshes with stock-preserving fitted geometry; candidate only.')
