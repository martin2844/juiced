"""Blender: fit existing four V70 reflectors/bulbs to their actual lens planes.

Blender --background --python-exit-code 1 --python THIS -- REPO BASELINE FRESH_STUDY FRESH_CANDIDATE
No new components or textures. All non-lamp mesh data is retained. Private only.
"""
from pathlib import Path
import hashlib,json,shutil,struct,sys
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

repo,base,study,out=map(Path,sys.argv[sys.argv.index('--')+1:])
if study.exists() or out.exists():raise RuntimeError('Fresh study and candidate required')
study.mkdir(parents=True);sys.path[:0]=[str(repo/'tools'),str(repo/'mods/ferrari/tools')]
from vinyl_pack import mesh
from import_ferrari import bounds
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
folder=base/'assets/cars/volvo_v70_24/mesh';source=folder/'w263_fbump.d8m';a=mesh.read(source.read_bytes())
stock={p['material']:p for p in a['parts']}
lens=stock['w263_m10'];housing=stock['w263_m11'];reflector=stock['w263_m12'];bulb=stock['w263_m13']
assert reflector['streams'][3,0][:,2].mean()<-.5,'Unexpected/already-corrected source reflectors'
lpos=lens['streams'][0,0];ltri=mesh.triangles(a,lens);tree=BVHTree.FromPolygons(lpos.tolist(),ltri.tolist())
face=np.cross(lpos[ltri[:,1]]-lpos[ltri[:,0]],lpos[ltri[:,2]]-lpos[ltri[:,0]])
verts=[];tri=[];owners=[]
for p in a['parts']:
    if p['material'] in ['w263_m10','w263_m12','w263_m13']:continue
    start=len(verts);verts.extend(p['streams'][0,0].tolist());faces=mesh.triangles(a,p);tri.extend((faces+start).tolist());owners.extend([p['material']]*len(faces))
opaque=BVHTree.FromPolygons(verts,tri)
bp=bulb['streams'][0,0];ordered=np.sort(bp[:,0]);edges=np.r_[ordered[0],ordered[np.flatnonzero(np.diff(ordered)>2)+1],ordered[-1]+.001]
assert len(edges)==5,edges
centres=[]
for lo,hi in zip(edges[:-1],edges[1:]):
    pts=bp[(bp[:,0]>=lo)&(bp[:,0]<hi)];centres.append((pts.min(0)+pts.max(0))/2)
centres=np.array(centres)
def group(points):return abs(points[:,0,None]-centres[None,:,0]).argmin(1)
rp=reflector['streams'][0,0];rg=group(rp);rim=np.array([rp[rg==i,2].max() for i in range(4)])

def sample_lens(points):
    hits=[];gradient=[]
    for x,y,z in points:
        hit,normal,index,distance=tree.ray_cast(Vector((x,y,500)),Vector((0,0,-1)),500)
        if hit is None:raise RuntimeError(f'Point outside original lens opening {x},{y},{z}')
        n=face[index];assert abs(n[2])>1e-5
        hits.append(hit.z);gradient.append((-n[0]/n[2],-n[1]/n[2]))
    return np.array(hits),np.array(gradient)

centre_surface,_=sample_lens(centres);bulb_shift=centre_surface-2.0-centres[:,2]
reflector_scales=np.ones(4)
def transform(p,kind):
    pos=p['streams'][0,0].astype(float);norm=p['streams'][3,0].copy();new=pos.copy();ids=group(pos)
    if kind=='reflector':
        scales=reflector_scales[ids]
        new[:,:2]=centres[ids,:2]+(pos[:,:2]-centres[ids,:2])*scales[:,None]
        surface,gradient=sample_lens(new);new[:,2]=surface-.8+pos[:,2]-rim[ids]
        norm=-norm;norm[:,:2]=norm[:,:2]/scales[:,None]-gradient*norm[:,2,None];norm/=np.linalg.norm(norm,axis=1)[:,None]
    else:new[:,2]+=bulb_shift[ids]
    return new.astype('<f4'),norm.astype('<f4')

# The original full circles extend slightly through the lower bumper mask.
# Fit only those existing rings, never enlarge/cut the source lamp aperture.
rt=mesh.triangles(a,reflector)
for i in range(4):
    ids=np.flatnonzero(rg==i);local=np.full(len(rp),-1,dtype=int);local[ids]=np.arange(len(ids))
    faces=rt[np.all(rg[rt]==i,axis=1)];faces=local[faces]
    part={'streams':{(0,0):rp[ids],(3,0):reflector['streams'][3,0][ids]}}
    for scale in [1.,.98,.96,.94,.92,.90,.88,.86]:
        reflector_scales[i]=scale;points,_=transform(part,'reflector');v=points[faces]
        samples=np.concatenate([points,v.mean(1),(v[:,0]+v[:,1])*.5,(v[:,1]+v[:,2])*.5,(v[:,2]+v[:,0])*.5])
        surface,_=sample_lens(samples);safe=bool(np.all(surface-samples[:,2]>.2))
        for x,y,z in samples:
            hit=opaque.ray_cast(Vector((x,y,500)),Vector((0,0,-1)),500)[0]
            if hit is not None and hit.z>z-.1:safe=False;break
        if safe:break
    if not safe:raise RuntimeError(f'Existing reflector cannot fit source aperture {i}')

shutil.copytree(base,out);records=[]
for suffix in ['','_ui']:
    folder=out/'assets/cars'/('volvo_v70_24'+suffix)/'mesh'
    for path in sorted(folder.glob('*.d8m')):
        a=mesh.read(path.read_bytes());changed=[]
        for p in a['parts']:
            name=p['material']
            if name not in ['w263_m12','w263_m13','w263_head']:continue
            oldpos=p['streams'][0,0].copy();oldnorm=p['streams'][3,0].copy()
            new,norm=transform(p,'reflector' if name=='w263_m12' else 'bulb')
            p['streams'][0,0][:]=new;p['streams'][3,0][:]=norm
            if name=='w263_m12':
                assert p['primitive']==1
                idx=a['indices'][p['first']:p['first']+p['count']].reshape(-1,3);idx[:]=idx[:,[0,2,1]]
            p['header'][44:88]=bounds(new)
            changed.append(dict(material=name,vertices=len(new),old_z_min=float(oldpos[:,2].min()),old_z_max=float(oldpos[:,2].max()),new_z_min=float(new[:,2].min()),new_z_max=float(new[:,2].max())))
        if not changed:continue
        a['prefix']=struct.pack('<I',len(a['indices']))+a['indices'].astype('<u2').tobytes()+struct.pack('<I',len(a['parts']))
        a['bounds']=bounds(np.concatenate([p['streams'][0,0] for p in a['parts']]))
        path.write_bytes(mesh.write(a));records.append(dict(path=path.relative_to(out).as_posix(),sha256=sha(path),parts=changed))

# The old on/off overlay contained only tiny bulb spheres. Reuse the fitted
# reflector surfaces as the native emissive area, offset by the same .8mm used
# by the original importer. No extra physical car component is introduced.
for suffix in ['','_ui']:
    folder=out/'assets/cars'/('volvo_v70_24'+suffix)/'mesh'
    body=mesh.read((folder/'w263_fbump.d8m').read_bytes());p=next(p for p in body['parts'] if p['material']=='w263_m12')
    extra=body['indices'][p['first']:p['first']+p['count']].copy()
    path=folder/'w263_lightson.d8m';lights=mesh.read(path.read_bytes());p['streams'][0,0][:]+=p['streams'][3,0]*.08
    p['header'][:32]=b'w263_head'.ljust(32,b'\0');struct.pack_into('<I',p['header'],32,len(lights['indices']));p['header'][44:88]=bounds(p['streams'][0,0])
    indices=np.concatenate([lights['indices'],extra]).astype('<u2')
    payload=struct.pack('<I',len(indices))+indices.tobytes()+struct.pack('<I',len(lights['parts'])+1)
    payload+=b''.join(bytes(q['header'])+bytes(q['verts']) for q in lights['parts'])+bytes(p['header'])+bytes(p['verts'])
    payload+=bounds(np.concatenate([q['streams'][0,0] for q in lights['parts']+[p]]));path.write_bytes(payload)
    record=next(r for r in records if r['path']==path.relative_to(out).as_posix());record['sha256']=sha(path);record['added_emissive_reflector_vertices']=len(p['streams'][0,0]);record['added_emissive_reflector_triangles']=len(extra)//3

# Independent visibility measurements against unchanged source housing/body.
old=reflector['streams'][0,0];new,newnorm=transform(reflector,'reflector');surface,_=sample_lens(new)
old_hidden=0;new_hidden=0;gaps=[];hidden=[]
for p,q in zip(old,new):
    oldhit=opaque.ray_cast(Vector((float(p[0]),float(p[1]),500)),Vector((0,0,-1)),500)[0]
    if oldhit is not None:old_hidden+=oldhit.z>float(p[2])+.05
    hit,_,face_index,_=opaque.ray_cast(Vector((float(q[0]),float(q[1]),500)),Vector((0,0,-1)),500)
    if hit is not None:
        new_hidden+=hit.z>float(q[2])+.05;gaps.append(float(q[2])-hit.z)
        if hit.z>float(q[2])+.05:hidden.append(dict(point=q.tolist(),hit=list(hit),material=owners[face_index]))
assert new_hidden==0,('Reflectors still behind housing/body',new_hidden,min(gaps),hidden)
assert np.all(surface-new[:,2]>.5) and np.all(newnorm[:,2]>0)

bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC';materials={}
def add(name,p,tri):
    coords=p['streams'][0,0];m=bpy.data.meshes.new(name);m.from_pydata((np.c_[coords[:,0],-coords[:,2],coords[:,1]]/100).tolist(),[],tri.tolist());m.update()
    o=bpy.data.objects.new(name,m);scene.collection.objects.link(o);key=p['material']
    if key not in materials:
        mat=bpy.data.materials.new(key);mat.diffuse_color=(.55,.58,.60,1) if key=='w263_m12' else (.8,.8,.7,1) if key=='w263_m13' else (.1,.1,.12,.12) if key=='w263_m10' else (.025,.025,.03,1);materials[key]=mat
    m.materials.append(materials[key]);return o
corrected=mesh.read((out/'assets/cars/volvo_v70_24/mesh/w263_fbump.d8m').read_bytes())
for index,p in enumerate(corrected['parts']):
    if p['material'] in ['w263_m10','w263_m11','w263_m12','w263_m13']:add('FITTED_'+p['material'],p,mesh.triangles(corrected,p))
bpy.ops.wm.save_as_mainfile(filepath=str(study/'v70-fitted-lamps.blend'))
oldinv={p.relative_to(base).as_posix():sha(p) for p in base.rglob('*') if p.is_file()};newinv={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file()}
assert oldinv.keys()==newinv.keys() and {p for p in oldinv if oldinv[p]!=newinv[p]}=={r['path'] for r in records}
(out/'SHA256.json').write_text(json.dumps({p:h for p,h in newinv.items() if p!='SHA256.json'},indent=2)+'\n')
report=dict(status='PRIVATE; native review required',baseline=str(base),source_sha256=sha(source),author_sha256=sha(Path(__file__)),method='Existing rings conform to local source lens planes; normals/winding face outward; existing bulbs translated per local lens; no added pieces or textures',centres_native_cm=centres.tolist(),reflector_xy_scales=reflector_scales.tolist(),lens_z_at_centres_cm=centre_surface.tolist(),bulb_forward_shift_cm=bulb_shift.tolist(),original_reflector_vertices_hidden_by_opaque_body=int(old_hidden),corrected_reflector_vertices_hidden_by_opaque_body=int(new_hidden),minimum_corrected_housing_clearance_cm=min(gaps),minimum_lens_clearance_cm=float((surface-new[:,2]).min()),meshes=records)
(study/'lamp-repair.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ['meshes']},indent=2))
