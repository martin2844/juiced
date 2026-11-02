"""Fresh private Outback skirts candidate. Retain every stock receiver byte.

python THIS REPO BASELINE_RUNTIME STUDY FRESH_CANDIDATE
Kit3 cladding coating is a separate material-only adapter step.
"""
from pathlib import Path
import hashlib,json,shutil,struct,sys
import numpy as np
repo,base,study,out=map(Path,sys.argv[1:])
sys.path[:0]=[str(repo/'tools'),str(repo/'mods/street-cars/tools'),str(repo/'mods/ferrari/tools')]
from vinyl_pack import mesh
from gltf import Gltf
from import_ferrari import mesh_bytes,bounds
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report=json.loads((study/'safe-skirt-study.json').read_text())
for rel,h in report['source_sha256'].items():assert sha(repo/rel)==h,rel
cfg=next(p for p in json.loads((repo/'tools/street-cars-wave2/native_kit_profiles.json').read_text()) if p['model']=='outback_2002')
if out.exists():raise RuntimeError('Fresh candidate required')
shutil.copytree(base,out);results=[]
for level in [2,3]:
    export=study/('outback_2002-kit'+str(level)+'.glb');assert sha(export)==report['variants'][str(level)]['proof']['export_sha256']
    g=Gltf(export);parts={z:[] for z in ['sskirt_l','sskirt_r']}
    for p in g.parts():
        zone=next((z for z in parts if 'ADD_'+z.upper() in p['source'].upper()),None);assert zone is not None
        p['pos']*=100;p['mat']=g.j['materials'][p['mi']]['name'];assert p['mat']==('w261_m0' if level==2 else 'outback_2002_body')
        assert p['pos'][:,1].max()<cfg['vinyl_min_height_cm']-.03
        assert np.isfinite(p['pos']).all() and np.isfinite(p['norm']).all() and p['pos'][:,1].min()>9.49
        _,inverse=np.unique(np.round(p['pos'],4),axis=0,return_inverse=True);t=inverse[p['tri']]
        edges=np.sort(np.concatenate([t[:,[0,1]],t[:,[1,2]],t[:,[2,0]]]),axis=1);assert np.all(np.unique(edges,axis=0,return_counts=True)[1]==2)
        parts[zone].append(p)
    for zone,extra in parts.items():
        assert extra
        for suffix in ['','_ui']:
            folder=out/'assets/cars'/('outback_2002'+suffix)/'mesh';source=folder/('w261_'+zone+'.d8m');a=mesh.read(source.read_bytes());b=mesh.read(mesh_bytes(extra))
            indices=np.concatenate([a['indices'],b['indices']]).astype('<u2');payload=struct.pack('<I',len(indices))+indices.tobytes()+struct.pack('<I',len(a['parts'])+len(b['parts']))
            for p in a['parts']:payload+=bytes(p['header'])+bytes(p['verts'])
            for p in b['parts']:
                h=bytearray(p['header']);struct.pack_into('<I',h,32,p['first']+len(a['indices']));payload+=h+p['verts']
            payload+=bounds(np.concatenate([p['streams'][0,0] for p in a['parts']+b['parts']]))
            target=folder/('w261_kit'+str(level)+'_'+zone+'.d8m');assert target.exists();target.write_bytes(payload);new=mesh.read(payload)
            assert np.array_equal(new['indices'][:len(a['indices'])],a['indices'])
            for p,q in zip(a['parts'],new['parts']):assert p['header']==q['header'] and p['verts']==q['verts']
            results.append(dict(path=target.relative_to(out).as_posix(),sha256=sha(target),stock_source_sha256=sha(source),preserved_original_parts=len(a['parts']),added_triangles=sum(len(p['tri']) for p in extra)))
old={p.relative_to(base).as_posix():sha(p) for p in base.rglob('*') if p.is_file()};now={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file()}
assert old.keys()==now.keys() and {p for p in old if old[p]!=now[p]}=={p['path'] for p in results}
(out/'SHA256.json').write_text(json.dumps({p:h for p,h in now.items() if p!='SHA256.json'},indent=2)+'\n')
(study/'native-candidate-receipt.json').write_text(json.dumps(dict(status='PRIVATE; not visually approved or installed',baseline=str(base),candidate=str(out),all_original_skirt_vertex_bytes_retained=True,all_new_geometry_below_all31_decals=True,meshes=results),indent=2)+'\n')
print('Adapted8 skirt meshes; original receiver bytes intact; candidate remains private.')
