"""Isolated Outback kit 2/3 candidate, preserving all current stock mesh bytes.

Usage: python THIS REPO BASELINE_WAVE2 STUDY EMPTY_CANDIDATE
Does not install, publish, regenerate the roster or touch player profiles.
"""
from pathlib import Path
import hashlib,json,shutil,struct,sys
import numpy as np

repo,baseline,study,output=map(Path,sys.argv[1:5])
resume=sys.argv[5:]==['--resume']
sys.path[:0]=[str(repo/'tools'),str(repo/'mods/street-cars/tools'),str(repo/'mods/ferrari/tools')]
from vinyl_pack import mesh
from gltf import Gltf
from import_ferrari import mesh_bytes,bounds
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
if output.exists() and not resume:raise RuntimeError('Fresh candidate required')
if not output.exists():shutil.copytree(baseline,output)
model,prefix='outback_2002','w261'
allowed={f'assets/cars/{model}{suffix}/mesh/{prefix}_kit{level}_{zone}.d8m'
         for suffix in ['', '_ui'] for level in [2,3] for zone in ['fbump','rbump','sskirt_l','sskirt_r']}
if resume:
    for f in baseline.rglob('*'):
        if f.is_file() and f.relative_to(baseline).as_posix() not in allowed|{'SHA256.json'}:
            assert sha(f)==sha(output/f.relative_to(baseline)),f
report=json.loads((study/(model+'-kit-study.json')).read_text())
for rel,h in report['stock_sha256'].items():
    if sha(repo/rel)!=h:raise RuntimeError('Study source changed: '+rel)

def combine(original,parts):
    a,b=mesh.read(original),mesh.read(mesh_bytes(parts))
    indices=np.concatenate([a['indices'],b['indices']]).astype('<u2')
    data=struct.pack('<I',len(indices))+indices.tobytes()+struct.pack('<I',len(a['parts'])+len(b['parts']))
    for p in a['parts']:data+=bytes(p['header'])+bytes(p['verts'])
    for p in b['parts']:
        header=bytearray(p['header']);struct.pack_into('<I',header,32,p['first']+len(a['indices']))
        data+=header+p['verts']
    data+=bounds(np.concatenate([p['streams'][0,0] for p in a['parts']+b['parts']]))
    c=mesh.read(data)
    assert np.array_equal(c['indices'][:len(a['indices'])],a['indices'])
    assert all(x['header']==y['header'] and x['verts']==y['verts'] for x,y in zip(a['parts'],c['parts']))
    return data

receipt={'study':str(study),'baseline':str(baseline),'candidate':str(output),'status':'NOT INSTALLED; native visual review required','meshes':[]}
for level in [2,3]:
    source=study/(model+'-kit'+str(level)+'.glb');gltf=Gltf(source)
    zones={x:[] for x in ['FBUMP','RBUMP','SSKIRT_L','SSKIRT_R']}
    for p in gltf.parts():
        name=p['source'].upper()
        zone=('FBUMP' if 'FRONT_' in name else 'RBUMP' if 'REAR_' in name else
              'SSKIRT_L' if 'SKIRT_L_' in name else 'SSKIRT_R' if 'SKIRT_R_' in name else None)
        if zone is None:raise RuntimeError('Unclassified part: '+name)
        p['pos']*=100;p['mat']=gltf.j['materials'][p['mi']]['name']
        assert p['mat'] in [model+'_body',prefix+'_kit_trim']
        assert np.isfinite(p['pos']).all() and np.isfinite(p['norm']).all()
        assert np.allclose(np.linalg.norm(p['norm'],axis=1),1,atol=1e-4)
        assert p['pos'][:,1].min()>9
        a,b,c=p['pos'][p['tri']].transpose(1,0,2)
        assert np.all(np.linalg.norm(np.cross(b-a,c-a),axis=1)>1e-8)
        _,inv=np.unique(np.round(p['pos'],4),axis=0,return_inverse=True);tri=inv[p['tri']]
        edges=np.sort(np.concatenate([tri[:,[0,1]],tri[:,[1,2]],tri[:,[2,0]]]),axis=1)
        _,counts=np.unique(edges,axis=0,return_counts=True);assert np.all(counts==2),name
        zones[zone].append(p)
    for zone,parts in zones.items():
        assert parts
        for suffix in ['', '_ui']:
            folder=output/'assets/cars'/(model+suffix)/'mesh'
            original=folder/(prefix+'_'+zone.lower()+'.d8m')
            target=folder/(prefix+'_kit'+str(level)+'_'+zone.lower()+'.d8m')
            assert target.is_file()
            old=sha(baseline/target.relative_to(output));target.write_bytes(combine(original.read_bytes(),parts))
            receipt['meshes'].append({'path':str(target.relative_to(output)),'old_sha256':old,'sha256':sha(target),'preserved_stock_sha256':sha(original),'added_triangles':sum(len(p['tri']) for p in parts)})
for suffix in ['','_ui']:
    folder=output/'assets/cars'/(model+suffix)/'mesh/mats'
    (folder/(prefix+'_kit_trim.pmd')).write_text('Material Program = Constant Colour\nColour = 25 25 27\n')
old={p.relative_to(baseline).as_posix():sha(p) for p in baseline.rglob('*') if p.is_file()}
new={p.relative_to(output).as_posix():sha(p) for p in output.rglob('*') if p.is_file()}
changed=[p for p in old if old[p]!=new.get(p)];added=sorted(set(new)-set(old))
assert set(changed)=={x['path'] for x in receipt['meshes']}
assert len(added) in [0,2] and all(x.endswith('/mats/w261_kit_trim.pmd') for x in added)
receipt.update(changed=changed,added=added,all_other_runtime_files_unchanged=True,unchanged_car_part_and_save_ids=True)
manifest={p.relative_to(output).as_posix():sha(p) for p in sorted(output.rglob('*')) if p.is_file() and p.name!='SHA256.json'}
(output/'SHA256.json').write_text(json.dumps(manifest,indent=2)+'\n')
(study/'native-candidate-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'changed_meshes':len(changed),'added_materials':len(added),'candidate':str(output),'installed':False}))
