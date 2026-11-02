"""Adapt a measured Blender study into a fresh, private Wave2 candidate.

python THIS REPO BASELINE_RUNTIME STUDY FRESH_CANDIDATE
Only mesh payloads for the study's existing kit indices change. No installation
or catalogue edits; previous native sources/receiver attributes stay protected.
"""
from pathlib import Path
import hashlib,json,shutil,struct,sys
import numpy as np

repo,baseline,study,out=map(Path,sys.argv[1:])
sys.path[:0]=[str(repo/'tools'),str(repo/'mods/street-cars/tools'),str(repo/'mods/ferrari/tools')]
from vinyl_pack import mesh
from gltf import Gltf
from import_ferrari import bounds,mesh_bytes
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report=json.loads((study/'native-vertex-study.json').read_text())
model,prefix=report['model'],report['prefix']
for rel,h in report['source_sha256'].items():assert sha(repo/rel)==h,rel
cfg=report['profile']
assert hashlib.sha256(json.dumps(cfg,sort_keys=True).encode()).hexdigest()==report['profile_sha256']
if out.exists():raise RuntimeError('Fresh candidate required')
shutil.copytree(baseline,out)
receipt=dict(status='PRIVATE; not installed or visually approved',study=str(study),baseline=str(baseline),meshes=[],exports={})

def append(base,extra):
    if not extra:return mesh.write(base)
    added=mesh.read(mesh_bytes(extra));indices=np.concatenate([base['indices'],added['indices']]).astype('<u2')
    data=struct.pack('<I',len(indices))+indices.tobytes()+struct.pack('<I',len(base['parts'])+len(added['parts']))
    for p in base['parts']:data+=bytes(p['header'])+bytes(p['verts'])
    for p in added['parts']:
        h=bytearray(p['header']);struct.pack_into('<I',h,32,p['first']+len(base['indices']));data+=h+p['verts']
    return data+bounds(np.concatenate([p['streams'][0,0] for p in base['parts']+added['parts']]))

for level,zones in report['variants'].items():
    export=study/(model+'-kit'+level+'.glb');receipt['exports'][export.name]=sha(export)
    g=Gltf(export);extras={z:[] for z in zones}
    for p in g.parts():
        name=p['source'].upper()
        if 'ADD_' not in name:continue
        zone=next((z for z in zones if 'ADD_'+z.upper() in name),None)
        assert zone is not None,name
        p['pos']*=100;p['mat']=g.j['materials'][p['mi']]['name']
        assert p['mat'] in [model+'_body',prefix+'_kit_trim',prefix+'_m0']
        assert np.isfinite(p['pos']).all() and np.isfinite(p['norm']).all()
        assert np.allclose(np.linalg.norm(p['norm'],axis=1),1,atol=1e-4)
        verts=p['pos'][p['tri']];assert np.all(np.linalg.norm(np.cross(verts[:,1]-verts[:,0],verts[:,2]-verts[:,0]),axis=1)>1e-8)
        _,inv=np.unique(np.round(p['pos'],4),axis=0,return_inverse=True);tri=inv[p['tri']]
        edges=np.sort(np.concatenate([tri[:,[0,1]],tri[:,[1,2]],tri[:,[2,0]]]),axis=1)
        assert np.all(np.unique(edges,axis=0,return_counts=True)[1]==2),name
        assert p['pos'][:,1].min()>=9.49,name
        assert p['pos'][:,1].max()<cfg['vinyl_min_height_cm'],('vinyl clearance',name)
        extras[zone].append(p)
    for zone,parts in zones.items():
        for suffix in ['','_ui']:
            folder=out/'assets/cars'/(model+suffix)/'mesh';source=folder/(prefix+'_'+zone+'.d8m')
            a=mesh.read(source.read_bytes());assert len(a['parts'])==len(parts)
            for p,v in zip(a['parts'],parts):
                assert p['material']==v['material']
                old=np.frombuffer(p['verts'],dtype=np.uint8).copy();mask=np.zeros(len(old),dtype=bool)
                original_pos=p['streams'][0,0].copy();original_norm=p['streams'][3,0].copy()
                for usage,values in [((0,0),v['positions']),((3,0),v['normals'])]:
                    arr=p['streams'][usage];offset=arr.ctypes.data-np.frombuffer(p['verts'],dtype=np.uint8).ctypes.data
                    for i in range(len(arr)):mask[offset+i*arr.strides[0]:offset+i*arr.strides[0]+12]=True
                    arr[:]=np.asarray(values,dtype='<f4')
                assert np.array_equal(old[~mask],np.frombuffer(p['verts'],dtype=np.uint8)[~mask])
                frozen=v['protected_vertices']
                assert np.array_equal(p['streams'][0,0][frozen],original_pos[frozen])
                assert np.array_equal(p['streams'][3,0][frozen],original_norm[frozen])
                p['header'][44:88]=bounds(p['streams'][0,0])
            a['bounds']=bounds(np.concatenate([p['streams'][0,0] for p in a['parts']]))
            data=append(a,extras[zone]);target=folder/(prefix+'_kit'+level+'_'+zone+'.d8m');assert target.exists()
            target.write_bytes(data);reread=mesh.read(data)
            assert np.array_equal(reread['indices'][:len(a['indices'])],a['indices'])
            for p,q in zip(a['parts'],reread['parts']):assert p['header']==q['header'] and p['verts']==q['verts']
            receipt['meshes'].append(dict(path=target.relative_to(out).as_posix(),sha256=sha(target),stock_sha256=sha(source),added_triangles=sum(len(p['tri']) for p in extras[zone]),changed_vertices=sum(v['changed_vertices'] for v in parts),protected_vertices=sum(len(v['protected_vertices']) for v in parts)))
old={p.relative_to(baseline).as_posix():sha(p) for p in baseline.rglob('*') if p.is_file()}
now={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file()}
changed={p for p in old if old[p]!=now.get(p)}
assert set(old)==set(now) and changed=={p['path'] for p in receipt['meshes']}
for suffix in ['','_ui']:
    folder=out/'assets/cars'/(model+suffix)/'mesh'
    for name,h in cfg['vinyl_source_sha256'].items():assert sha(folder/name)==h,name
receipt.update(changed_files=sorted(changed),all_other_runtime_files_unchanged=True,original_uvs_colours_unknown_vertex_data_preserved=True,protected_receiver_vertices_unchanged=True)
(out/'SHA256.json').write_text(json.dumps({p:h for p,h in now.items() if p!='SHA256.json'},indent=2)+'\n')
(study/'native-candidate-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(dict(candidate=str(out),changed_meshes=len(changed),installed=False)))
