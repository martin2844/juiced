"""Body-colour fascia option for kit3 of405/240; no geometry changes.

python THIS BASELINE_RUNTIME FRESH_CANDIDATE RECEIPT
Source black bumper materials stay intact on stock/kit1/kit2. Existing chrome,
plates, grille textures, lights and every triangle/vertex attribute are retained.
"""
from pathlib import Path
import hashlib,json,shutil,struct,sys
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from vinyl_pack import mesh
base,out,receipt=map(Path,sys.argv[1:]);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
if out.exists():raise RuntimeError('Fresh candidate required')
shutil.copytree(base,out);results=[]
def components(pos,tri):
    # Weld only for connectivity; actual exported vertex data is never changed.
    _,inverse=np.unique(np.round(pos,3),axis=0,return_inverse=True);parent=np.arange(inverse.max()+1)
    def root(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    for t in inverse[tri]:
        r=root(t[0]);parent[root(t[1])]=r;parent[root(t[2])]=r
    groups={}
    for i,t in enumerate(inverse[tri]):groups.setdefault(root(t[0]),[]).append(i)
    return list(groups.values())
for model,prefix in [('peugeot_405_glx','w257'),('volvo_240_turbo','w262')]:
    for suffix in ['','_ui']:
        folder=out/'assets/cars'/(model+suffix)/'mesh'
        for zone in ['fbump','rbump']:
            path=folder/(prefix+'_kit3_'+zone+'.d8m');a=mesh.read(path.read_bytes());chunks=[];indices=[];first=0;count=0;selected=[]
            for index,p in enumerate(a['parts']):
                tri=mesh.triangles(a,p);keep=np.zeros(len(tri),dtype=bool)
                if p['material']==prefix+'_m0':
                    if model=='peugeot_405_glx':keep[:]=True # Verified black-only atlas; grille/plates/chrome are other materials.
                    else:
                        pos=p['streams'][0,0]
                        # Native welded components match source Cube.027(front,
                        #1290 faces) / Cube.034(rear,940). Exclude the separate
                        # grille and inner chassis, rather than selecting facets
                        # by normal angle or painting everything below a height.
                        found=[]
                        for ids in components(pos,tri):
                            pts=pos[tri[ids]].reshape(-1,3);lo,hi=pts.min(0),pts.max(0)
                            if hi[1]<53 and lo[1]>30 and hi[0]-lo[0]>160:found.append(ids)
                        assert len(found)==1,(zone,[len(x) for x in found])
                        assert len(found[0])==(1290 if zone=='fbump' else 940),zone
                        keep[found[0]]=True
                for paint in [False,True]:
                    faces=tri[keep if paint else ~keep]
                    if not len(faces):continue
                    header=bytearray(p['header']);name=model+'_body' if paint else p['material'];header[:32]=name.encode().ljust(32,b'\0');struct.pack_into('<3I',header,32,first,faces.size,1)
                    chunks.append(bytes(header)+bytes(p['verts']));indices.extend(faces.ravel().tolist());first+=faces.size
                    if paint:count+=len(faces);selected.append(dict(original_part=index,triangle_indices=np.flatnonzero(keep).tolist()))
            assert count>10,(model,zone,count)
            payload=struct.pack('<I',len(indices))+np.array(indices,dtype='<u2').tobytes()+struct.pack('<I',len(chunks))+b''.join(chunks)+a['bounds'];path.write_bytes(payload)
            new=mesh.read(payload);assert sum(len(mesh.triangles(new,p)) for p in new['parts'])==sum(len(mesh.triangles(a,p)) for p in a['parts'])
            results.append(dict(path=path.relative_to(out).as_posix(),sha256=sha(path),painted_faces=count,selected_faces=selected))
old={p.relative_to(base).as_posix():sha(p) for p in base.rglob('*') if p.is_file()};now={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file()}
assert old.keys()==now.keys() and {p for p in old if old[p]!=now[p]}=={p['path'] for p in results}
(out/'SHA256.json').write_text(json.dumps({p:h for p,h in now.items() if p!='SHA256.json'},indent=2)+'\n')
receipt.write_text(json.dumps(dict(status='PRIVATE; not visually approved or installed',baseline=str(base),candidate=str(out),author_sha256=sha(Path(__file__)),method='Material-only kit3 fascia; existing native player body colour; no new texture, geometry or catalogue',meshes=results),indent=2)+'\n')
print([(p['path'],p['painted_faces']) for p in results])
