"""Correct only the constant UV of the previously appended grey skirt lips.

python THIS REPO BASELINE_RUNTIME FRESH_CANDIDATE RECEIPT
Blender's glTF exporter flips V. The native grey atlas texel is (.05,.05),
not (.05,.95). Original stock surfaces and every other vertex field stay exact.
"""
from pathlib import Path
import hashlib,json,shutil,sys
import numpy as np

repo,baseline,out,receipt=map(Path,sys.argv[1:])
sys.path.insert(0,str(repo/'tools'))
from vinyl_pack import mesh
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inventory=lambda p:{f.relative_to(p).as_posix():sha(f) for f in p.rglob('*') if f.is_file()}
if out.exists():raise RuntimeError('Fresh candidate required')
before=inventory(baseline)
shutil.copytree(baseline,out)
rows=[]
for suffix in ['', '_ui']:
    folder=out/'assets/cars'/('outback_2002'+suffix)/'mesh'
    for side in ['l','r']:
        target=folder/('w261_kit2_sskirt_'+side+'.d8m')
        original=target.read_bytes();a=mesh.read(original)
        stock=mesh.read((folder/('w261_sskirt_'+side+'.d8m')).read_bytes())
        assert len(a['parts'])==len(stock['parts'])+1
        added=a['parts'][-1];assert added['material']=='w261_m0'
        uv=added['streams'][5,0]
        assert np.allclose(uv, [.05,.95],atol=1e-6)
        raw=np.frombuffer(added['verts'],dtype=np.uint8)
        prior=raw.copy();mask=np.zeros(len(raw),dtype=bool)
        offset=uv.ctypes.data-raw.ctypes.data
        for i in range(len(uv)):mask[offset+i*uv.strides[0]+4:offset+i*uv.strides[0]+8]=True
        uv[:,1]=.05
        assert np.array_equal(prior[~mask],raw[~mask])
        data=mesh.write(a);b=mesh.read(data);old=mesh.read(original)
        assert a['prefix']==old['prefix'] and a['bounds']==old['bounds']
        for p,q in zip(b['parts'][:-1],old['parts'][:-1]):
            assert p['header']==q['header'] and p['verts']==q['verts']
        assert added['header']==old['parts'][-1]['header']
        target.write_bytes(data)
        rows.append(dict(path=target.relative_to(out).as_posix(),before_sha256=hashlib.sha256(original).hexdigest(),after_sha256=sha(target),vertices=len(uv),old_native_uv=[.05,.95],new_native_uv=[.05,.05]))
after=inventory(out)
assert set(before)==set(after)
assert {p for p in before if before[p]!=after[p]}=={r['path'] for r in rows}
(out/'SHA256.json').write_text(json.dumps({p:h for p,h in after.items() if p!='SHA256.json'},indent=2)+'\n')
result=dict(baseline=str(baseline),candidate=str(out),meshes=rows,only_appended_vertex_V_changed=True,all_original_stock_bytes_preserved=True,installed=False)
receipt.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(changed_meshes=len(rows),candidate=str(out),installed=False)))
