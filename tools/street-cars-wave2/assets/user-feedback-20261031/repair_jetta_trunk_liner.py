"""Remove the simplified, intrusive trunk-liner shell by exact source faces.

python THIS REPO SOURCE_GLB BASELINE_PACKAGE FRESH_CANDIDATE RECEIPT
The floor, parcel shelf, exterior panels and normal cabin trim are retained.
Only the named source object's triangles are removed from existing stock and
kit surfaces; material, position, normal and UV bytes of retained vertices stay.
"""
from pathlib import Path
import hashlib,json,re,shutil,struct,sys
import numpy as np
r,source,base,out,receipt=map(Path,sys.argv[1:]);assert not out.exists()
sys.path[:0]=[str(r/'tools'),str(r/'mods/street-cars/tools'),str(r/'mods/ferrari/tools')]
from vinyl_pack import mesh
from gltf import Gltf
from import_ferrari import bounds
g=Gltf(source);parts=g.parts()
def wheel(p):return bool(re.search(r'/wheel(?:fl|fr|rl|rr|_[rl][fr])(?:/|_|$)',p['source'].lower()))
tyres=[p for p in parts if wheel(p) and (any(k in p['source'].lower() for k in ['_tire','_tyre','tire_','tyre_']) or g.j['materials'][p['mi']].get('name','').lower() in ['rubber_oj_rubber','oj_rubber'])]
assert tyres;v=np.concatenate([p['pos'] for p in tyres]);mid=(v[:,2].min()+v[:,2].max())/2
front=v[(v[:,0]>0)&(v[:,2]>mid)];rear=v[(v[:,0]>0)&(v[:,2]<mid)];fc=(front.min(0)+front.max(0))/2;rc=(rear.min(0)+rear.max(0))/2
scale=257.8/(fc[2]-rc[2]);offset=np.array([0,v[:,1].min(),(fc[2]+rc[2])/2]);p=next(p for p in parts if p['source']=='/jettaa5_carpet-material')
native=(p['pos']-offset)*scale*np.array([-1,1,1]);source_tri=native[p['tri'][:,[0,2,1]]];assert len(source_tri)==87
shutil.copytree(base,out);rows=[];stock_matches=0
for zone in ['chass','fbump','rbump','bonnet','sskirt_l','sskirt_r']:
    folder=base/'assets/cars/jetta_mk5_fsi/mesh';stock=mesh.read((folder/('w260_'+zone+'.d8m')).read_bytes());selected={}
    for i,p in enumerate(stock['parts']):
        if p['material']!='w260_m3':continue
        tri=mesh.triangles(stock,p);t=p['streams'][0,0][tri];centres=t.mean(1);remove=[]
        for target in source_tri:
            distance=np.linalg.norm(centres-target.mean(0),axis=1);candidates=np.flatnonzero(distance<.003)
            for j in candidates:
                d=np.linalg.norm(t[j][:,None,:]-target[None,:,:],axis=2)
                if np.all(d.min(0)<.003) and np.all(d.min(1)<.003):remove.append(int(j));break
        if remove:selected[i]=sorted(set(remove));stock_matches+=len(selected[i])
    if not selected:continue
    for suffix in ['','_ui']:
        f=out/'assets/cars'/('jetta_mk5_fsi'+suffix)/'mesh'
        names=['w260_'+zone]
        if zone!='chass':names+=['w260_sport_'+zone]+['w260_kit'+str(n)+'_'+zone for n in [1,2,3]]
        for name in names:
            path=f/(name+'.d8m')
            if not path.exists():continue
            before=path.read_bytes();a=mesh.read(before);indices=[];data=[];first=0;removed=0
            for i,p in enumerate(a['parts']):
                tri=mesh.triangles(a,p)
                if i in selected:
                    assert p['material']=='w260_m3' and np.array_equal(tri,mesh.triangles(stock,stock['parts'][i]))
                    keep=np.ones(len(tri),dtype=bool);keep[selected[i]]=False;removed+=len(selected[i]);tri=tri[keep]
                h=bytearray(p['header']);struct.pack_into('<3I',h,32,first,tri.size,1);first+=tri.size;indices.append(tri.astype('<u2').reshape(-1));data.append(bytes(h)+bytes(p['verts']))
            idx=np.concatenate(indices);raw=struct.pack('<I',len(idx))+idx.tobytes()+struct.pack('<I',len(a['parts']))+b''.join(data)+a['bounds'];b=mesh.read(raw)
            for p,q in zip(a['parts'],b['parts']):assert p['verts']==q['verts']
            path.write_bytes(raw);rows.append(dict(path=path.relative_to(out).as_posix(),removed_triangles=removed,remaining_vertex_bytes_unchanged=True,before_sha256=hashlib.sha256(before).hexdigest(),after_sha256=hashlib.sha256(raw).hexdigest()))
assert stock_matches==87,('Unmatched source faces',stock_matches)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(out/'SHA256.json').write_text(json.dumps({p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file() and p.name!='SHA256.json'},indent=2)+'\n')
receipt.write_text(json.dumps(dict(source_glb=str(source),source_sha256=sha(source),object='/jettaa5_carpet-material',source_triangles=87,matched_stock_triangles=stock_matches,changes=rows,exterior_vertices_and_other_interior_components_unchanged=True,installed=False),indent=2)+'\n');print('Removed exactly87 source trunk-liner faces across',len(rows),'native variants; exterior retained.')
