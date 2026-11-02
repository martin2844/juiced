"""Blender study: buried skirt mounts, untouched native vinyl receivers.

Blender --background --python-exit-code 1 --python THIS -- REPO FRESH_OUTPUT
The exported, beveled triangles are checked against all31 original decal meshes
by exact convex intersections in their side-projection plane. No installation.
"""
from pathlib import Path
import hashlib,json,math,sys
import bpy,bmesh
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

repo,out=map(Path,sys.argv[sys.argv.index('--')+1:])
if out.exists():raise RuntimeError('Fresh study required')
out.mkdir(parents=True)
sys.path[:0]=[str(repo/'tools'),str(repo/'mods/street-cars/tools')]
from vinyl_pack import mesh
from gltf import Gltf
folder=repo/'mods/street-cars-wave2/assets/cars/outback_2002/mesh'
cfg=next(c for c in json.loads((Path(__file__).with_name('native_kit_profiles.json')).read_text()) if c['model']=='outback_2002')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sources={};decals=[]
for name,h in cfg['vinyl_source_sha256'].items():
    p=folder/name;assert sha(p)==h;sources[str(p.relative_to(repo))]=h;a=mesh.read(p.read_bytes())
    for part in a['parts']:
        tri=part['streams'][0,0][mesh.triangles(a,part)].astype(float)
        for side in [-1,1]:
            t=tri[(tri[:,:,0].mean(1)*side>0)&(tri[:,:,1].min(1)<43)]
            if len(t):decals.append((side,name,t))

def cross(a,b):return a[0]*b[1]-a[1]*b[0]
def overlap(a,b):
    polygon=list(a);sign=1 if cross(b[1]-b[0],b[2]-b[0])>0 else -1
    for start,end in zip(b,np.roll(b,-1,axis=0)):
        clipped=[];edge=end-start
        for p,q in zip(polygon,polygon[1:]+polygon[:1]):
            d=cross(edge,p-start)*sign;e=cross(edge,q-start)*sign
            if d>=-1e-9:clipped.append(p)
            if (d>0)!=(e>0):clipped.append(p+(q-p)*d/(d-e))
        polygon=clipped
        if len(polygon)<3:return None
    return np.asarray(polygon)

decal_planes={}
for side,name,tri in decals:
    projected=tri[:,:,1:];area=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0])[:,0]
    valid=np.abs(area)>1e-7;tri=tri[valid];projected=projected[valid]
    planes=np.linalg.solve(np.concatenate([projected,np.ones((*projected.shape[:2],1))],axis=2),tri[:,:,0,None])[:,:,0]
    decal_planes[side,name]=(projected,planes,projected.min(1),projected.max(1))

def prove_export(path):
    g=Gltf(path);intersections=0;worst=-1e9;blocked=[]
    for p in g.parts():
        tri=p['pos'][p['tri']]*100;side=1 if tri[:,:,0].mean()>0 else -1
        for t in tri:
            yz=t[:,1:];det=cross(yz[1]-yz[0],yz[2]-yz[0])
            if abs(det)<1e-7:continue
            plane=np.linalg.solve(np.c_[yz,np.ones(3)],t[:,0]);lo=yz.min(0);hi=yz.max(0)
            for (s,name),(projected,planes,lows,highs) in decal_planes.items():
                if s!=side:continue
                ids=np.flatnonzero(np.all(highs>=lo,1)&np.all(lows<=hi,1))
                for i in ids:
                    polygon=overlap(yz,projected[i])
                    if polygon is None:continue
                    area=sum(abs(cross(polygon[k]-polygon[0],polygon[k+1]-polygon[0])) for k in range(1,len(polygon)-1))
                    if area<1e-7:continue
                    delta=plane-planes[i];depth=(polygon@delta[:2]+delta[2])*side
                    maximum=float(depth.max());worst=max(worst,maximum);intersections+=1
                    if maximum>-.03:blocked.append(dict(source=p['source'],style=name,depth_cm=maximum))
    return dict(export_sha256=sha(path),all31_source_meshes=True,convex_triangle_overlaps=intersections,
                greatest_new_surface_x_in_front_of_decal_cm=worst if intersections else None,
                minimum_required_clearance_cm=.03,violations=len(blocked),worst=sorted(blocked,key=lambda x:x['depth_cm'],reverse=True)[:3])

bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC'
materials={}
def mat(name):
    if name not in materials:
        m=bpy.data.materials.new(name);m.diffuse_color=(.08,.24,.5,1) if name=='outback_2002_body' else (.3,.32,.33,1);materials[name]=m
    return materials[name]
def convert(p):return np.c_[p[:,0],-p[:,2],p[:,1]]/100
reference=bpy.data.collections.new('UNCHANGED_STOCK_RECEIVERS');scene.collection.children.link(reference)
stock={}
for zone in ['chass','fbump','rbump','sskirt_l','sskirt_r']:
    path=folder/('w261_'+zone+'.d8m');sources[str(path.relative_to(repo))]=sha(path);a=mesh.read(path.read_bytes());stock[zone]=a
    for index,p in enumerate(a['parts']):
        m=bpy.data.meshes.new(zone);m.from_pydata(convert(p['streams'][0,0]).tolist(),[],mesh.triangles(a,p).tolist());m.update()
        obj=bpy.data.objects.new('STOCK_'+zone+'_'+str(index),m);reference.objects.link(obj);m.materials.append(mat(p['material']))
boh=folder.parent/'models/outback_2002_stock.boh';sources[str(boh.relative_to(repo))]=sha(boh);nodes={n:m for n,_,m in mesh.boh(boh.read_bytes())}
wheel=folder/'w261_wheel.d8m';sources[str(wheel.relative_to(repo))]=sha(wheel);a=mesh.read(wheel.read_bytes());radius=float(np.max(np.abs(np.concatenate([p['streams'][0,0] for p in a['parts']])[:,2])))
start=float(nodes['WHEEL_RL'][2,3])+radius+5;end=float(nodes['WHEEL_FL'][2,3])-radius-5
mounts={}
for zone,side in [('sskirt_l',-1),('sskirt_r',1)]:
    a=stock[zone];verts=[];faces=[]
    for p in a['parts']:
        i=len(verts);verts.extend(p['streams'][0,0].tolist());faces.extend((mesh.triangles(a,p)+i).tolist())
    pts=np.asarray(verts);tree=BVHTree.FromPolygons(verts,faces);width=abs(pts[:,0]).max();roots=[]
    for z in np.linspace(start,end,29):
        found=None
        for y in np.arange(pts[:,1].min()+.1,pts[:,1].max()+.3,.2):
            hit=tree.ray_cast(Vector((side*(width+100),y,z)),Vector((-side,0,0)),200)[0]
            if hit is not None and abs(hit.x)>.9*width:found=tuple(hit);break
        if found is None:raise RuntimeError(f'No actual skirt root {zone} {z}')
        roots.append(found)
    mounts[zone]=roots

def solid(name,sections,material,col):
    verts=np.asarray([v for row in sections for v in row]);faces=[]
    for i in range(len(sections)-1):
        for j in range(4):faces.append((4*i+j,4*(i+1)+j,4*(i+1)+(j+1)%4,4*i+(j+1)%4))
    faces.extend([(3,2,1,0),tuple(range(len(verts)-4,len(verts)))])
    m=bpy.data.meshes.new(name);m.from_pydata(convert(verts).tolist(),[],faces);m.update();bm=bmesh.new();bm.from_mesh(m)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    if bm.calc_volume(signed=True)<0:bmesh.ops.reverse_faces(bm,faces=list(bm.faces))
    assert all(e.is_manifold for e in bm.edges) and bm.calc_volume()>1e-8;bm.to_mesh(m);bm.free()
    o=bpy.data.objects.new(name,m);col.objects.link(o);m.materials.append(mat(material));uv=m.uv_layers.new()
    # Exported nativeUV must be(.05,.05); Blender/glTF V axes are opposite.
    for loop in m.loops:uv.data[loop.index].uv=(.05,.95 if material=='w261_m0' else .05)
    bevel=o.modifiers.new('3mm fitted edge','BEVEL');bevel.width=.003;bevel.segments=2

report=dict(status='PRIVATE STUDY; not installed or visually approved',source_sha256=sources,author_sha256=sha(Path(__file__)),
            method='Untouched stock receivers; lower closed skirts with buried upper mounts; actual exported triangle intersection proof',mounts_native_cm=mounts,variants={})
for level in [2,3]:
    trials=[]
    for drop in [0.,1.,2.,3.,4.,5.]:
        col=bpy.data.collections.new('KIT_'+str(level));scene.collection.children.link(col)
        for zone,side in [('sskirt_l',-1),('sskirt_r',1)]:
            sections=[];roots=mounts[zone];floor=cfg['vinyl_min_height_cm']-.2
            for x,y,z in roots:
                shape=max(.15,math.sin(math.pi*(z-start)/(end-start)));ext=(2.5 if level==2 else 4)*shape;inside=4.
                top=min(y-.4,floor-.5-max(0,y+.2-floor)*ext/inside-drop);bottom=max(9.5,top-(2.5 if level==2 else 2.))
                sections.append([(x-side*inside,y+.2,z),(x+side*ext,top,z),(x+side*ext,bottom,z),(x-side*inside,bottom,z)])
            solid('ADD_'+zone.upper(),sections,'w261_m0' if level==2 else 'outback_2002_body',col)
        bpy.ops.object.select_all(action='DESELECT')
        for o in col.objects:o.select_set(True)
        path=out/('outback_2002-kit'+str(level)+'.glb')
        bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_yup=True,export_apply=True)
        proof=prove_export(path);trials.append(dict(extra_drop_cm=drop,proof=proof))
        if proof['violations']==0:break
        for o in list(col.objects):bpy.data.objects.remove(o,do_unlink=True)
        bpy.data.collections.remove(col)
    if proof['violations']:raise RuntimeError(json.dumps(proof))
    report['variants'][str(level)]=dict(extra_drop_cm=drop,proof=proof,trials=trials)
    col.hide_viewport=True;col.hide_render=True
bpy.ops.wm.save_as_mainfile(filepath=str(out/'outback-safe-skirts.blend'))
(out/'safe-skirt-study.json').write_text(json.dumps(report,indent=2)+'\n')
print('SAFE SKIRT STUDY',json.dumps(report['variants']))
