"""Private candidate from exact Blender native vertex arrays; no installation."""
from pathlib import Path
import hashlib,json,shutil,sys
import numpy as np

repo,baseline,study,out=map(Path,sys.argv[1:])
sys.path[:0]=[str(repo/'tools'),str(repo/'mods/ferrari/tools')]
from vinyl_pack import mesh
from import_ferrari import bounds
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
if out.exists():raise RuntimeError('Fresh candidate required')
report=json.loads((study/'native-vertex-study.json').read_text())
for rel,h in report['source_sha256'].items():assert sha(repo/rel)==h,rel
shutil.copytree(baseline,out)
receipt=dict(status='NOT INSTALLED; native visual review required',study=str(study),baseline=str(baseline),candidate=str(out),meshes=[])
for level,zones in report['variants'].items():
    for zone,parts in zones.items():
        for suffix in ['','_ui']:
            folder=out/'assets/cars'/('outback_2002'+suffix)/'mesh'
            source=folder/('w261_'+zone+'.d8m');a=mesh.read(source.read_bytes());assert len(a['parts'])==len(parts)
            original=mesh.read(source.read_bytes())
            for p,v in zip(a['parts'],parts):
                assert p['material']==v['material']
                old=np.frombuffer(p['verts'],dtype=np.uint8).copy();mask=np.zeros(len(old),dtype=bool)
                for usage,values in [((0,0),v['positions']),((3,0),v['normals'])]:
                    array=p['streams'][usage];offset=array.ctypes.data-np.frombuffer(p['verts'],dtype=np.uint8).ctypes.data
                    for i in range(len(array)):mask[offset+i*array.strides[0]:offset+i*array.strides[0]+12]=True
                    array[:]=np.array(values,dtype='<f4')
                assert np.array_equal(old[~mask],np.frombuffer(p['verts'],dtype=np.uint8)[~mask])
                p['header'][44:88]=bounds(p['streams'][0,0])
            a['bounds']=bounds(np.concatenate([p['streams'][0,0] for p in a['parts']]))
            target=folder/('w261_kit'+level+'_'+zone+'.d8m');assert target.exists();target.write_bytes(mesh.write(a))
            reread=mesh.read(target.read_bytes());assert np.array_equal(reread['indices'],original['indices'])
            for x,y,v in zip(original['parts'],reread['parts'],parts):
                assert x['material']==y['material']
                assert x['header'][:44]==y['header'][:44] and x['header'][88:]==y['header'][88:]
                assert np.array_equal(y['streams'][0,0],np.array(v['positions'],dtype='<f4'))
                assert np.array_equal(y['streams'][3,0],np.array(v['normals'],dtype='<f4'))
            receipt['meshes'].append(dict(path=str(target.relative_to(out)),sha256=sha(target),stock_source_sha256=sha(source),changed_vertices=sum(v['changed_vertices'] for v in parts)))
old={p.relative_to(baseline).as_posix():sha(p) for p in baseline.rglob('*') if p.is_file()}
now={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file()}
changed=[p for p in old if now.get(p)!=old[p]]
assert set(old)==set(now) and set(changed)=={x['path'] for x in receipt['meshes']}
receipt.update(changed_files=changed,all_other_runtime_files_unchanged=True,topology_uvs_materials_and_unknown_vertex_data_preserved=True)
(out/'SHA256.json').write_text(json.dumps({p:h for p,h in now.items() if p!='SHA256.json'},indent=2)+'\n')
(study/'native-candidate-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'changed_meshes':len(changed),'candidate':str(out),'installed':False}))
