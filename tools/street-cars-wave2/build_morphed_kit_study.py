"""Outback pilot: reshape original lower bumpers/rockers, retaining their surfaces.

Blender --background --python THIS -- REPO FRESH_STUDY
Native centimetres Y-up/+Z-forward -> Blender metres Z-up/-Y-forward.
The JSON records the exact native vertex/normal arrays for the byte-preserving
adapter. GLBs and the editable Blender scene show that same geometry.
"""
from pathlib import Path
import hashlib,json,sys
import bpy
import numpy as np

repo,out=map(Path,sys.argv[sys.argv.index('--')+1:])
if out.exists():raise RuntimeError('Fresh study required')
out.mkdir(parents=True);sys.path.insert(0,str(repo/'tools'))
from vinyl_pack import mesh
model,prefix='outback_2002','w261'
folder=repo/'mods/street-cars-wave2/assets/cars'/model/'mesh'
hierarchy=folder.parent/'models'/(model+'_stock.boh')
nodes={name:matrix for name,_,matrix in mesh.boh(hierarchy.read_bytes())}
wheel=mesh.read((folder/(prefix+'_wheel.d8m')).read_bytes())
radius=float(np.max(np.abs(np.concatenate([p['streams'][0,0] for p in wheel['parts']])[:,2])))
front,rear=float(nodes['WHEEL_FL'][2,3]),float(nodes['WHEEL_RL'][2,3])
exhaust=repo/'mods/street-cars-wave2/assets/carmesh'/(prefix+'_exhaust.d8m')
e=mesh.read(exhaust.read_bytes());points=np.concatenate([p['streams'][0,0] for p in e['parts']])+nodes['EXHAUST_S'][:3,3]
tips=points[points[:,2]<points[:,2].min()+12]
notch=[float(tips[:,0].min())-2.5,float(tips[:,0].max())+2.5]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source={str(p.relative_to(repo)):sha(p) for p in [hierarchy,exhaust,folder/(prefix+'_wheel.d8m')]}
def smooth(x):
    x=np.clip(x,0,1);return x*x*(3-2*x)
def convert(p):return np.c_[p[:,0],-p[:,2],p[:,1]]/100
def deform(p,zone,level,bottom):
    p=np.array(p,dtype=np.float64);q=p.copy()
    # Upper surfaces, lamps, plate and wheel arches remain on their source
    # positions. These measured limits apply only to this Outback pilot.
    height=37.;h=smooth((height-p[:,1])/(height-bottom))
    if zone in ('fbump','rbump'):
        axle=front if zone=='fbump' else rear
        w=smooth((np.abs(p[:,2]-axle)-radius-5)/18)
        if zone=='rbump':
            distance=np.maximum(notch[0]-p[:,0],p[:,0]-notch[1])
            w*=smooth(distance/7) # Original continuous surface curves around pipe.
        weight=h*w
        q[:,1]-=(5. if level==2 else 3.)*weight
        q[:,2]+=(1 if zone=='fbump' else -1)*(1.5 if level==2 else 4.)*weight
        q[:,0]+=np.sign(p[:,0])*(.8 if level==2 else 2.)*weight*(np.abs(p[:,0])/90.)**4
    else:
        start,end=rear+radius+5,front-radius-5
        weight=h*smooth((p[:,2]-start)/8)*smooth((end-p[:,2])/8)*smooth((np.abs(p[:,0])-72)/12)
        q[:,1]-=(3.5 if level==2 else 2.)*weight
        q[:,0]+=np.sign(p[:,0])*(2.5 if level==2 else 4.)*weight
    return q

bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC'
reference=bpy.data.collections.new('CURRENT_STOCK_REFERENCE');scene.collection.children.link(reference)
materials={}
def add_object(name,pos,tri,mat,col,uv=None):
    m=bpy.data.meshes.new(name);m.from_pydata(convert(pos).tolist(),[],tri.tolist());m.update()
    obj=bpy.data.objects.new(name,m);col.objects.link(obj)
    if mat not in materials:
        a=bpy.data.materials.new(mat);a.diffuse_color=(.08,.24,.5,1) if mat==model+'_body' else (.25,.27,.28,1);materials[mat]=a
    m.materials.append(materials[mat])
    if uv is not None:
        layer=m.uv_layers.new()
        for loop in m.loops:layer.data[loop.index].uv=uv[loop.vertex_index]
    return obj

zones=['fbump','rbump','sskirt_l','sskirt_r'];parsed={}
for zone in ['chass','bonnet']+zones:
    path=folder/(prefix+'_'+zone+'.d8m');source[str(path.relative_to(repo))]=sha(path)
    a=mesh.read(path.read_bytes());parsed[zone]=a
    for i,p in enumerate(a['parts']):add_object('STOCK_'+zone+'_'+str(i),p['streams'][0,0],mesh.triangles(a,p),p['material'],reference,p['streams'].get((5,0)))
report=dict(status='NOT INSTALLED; native visual review required',source_sha256=source,
            method='Continuous deformation of original lower bumper/rocker surfaces; no appended flat valance plates.',
            front_axle_z_cm=front,rear_axle_z_cm=rear,wheel_radius_cm=radius,exhaust_notch_x_cm=notch,variants={})
for level in [2,3]:
    col=bpy.data.collections.new('KIT_'+str(level));scene.collection.children.link(col);variant={}
    for zone in zones:
        a=parsed[zone];bottom=float(np.concatenate([p['streams'][0,0] for p in a['parts']])[:,1].min());parts=[]
        for i,p in enumerate(a['parts']):
            pos=p['streams'][0,0].astype(np.float64);norm=p['streams'][3,0].copy();new=deform(pos,zone,level,bottom)
            active=np.max(np.abs(new-pos),axis=1)>1e-7
            # Transform the original smooth/hard normals through the deformation
            # Jacobian, retaining untouched normals and all UV seams exactly.
            if active.any():
                jac=np.zeros((int(active.sum()),3,3));h=.001
                for axis in range(3):
                    delta=np.zeros(3);delta[axis]=h
                    jac[:,:,axis]=(deform(pos[active]+delta,zone,level,bottom)-deform(pos[active]-delta,zone,level,bottom))/(2*h)
                assert np.all(np.linalg.det(jac)>0)
                n=np.einsum('nij,nj->ni',np.linalg.inv(jac).transpose(0,2,1),norm[active]);n/=np.linalg.norm(n,axis=1)[:,None];norm[active]=n
            new=new.astype('<f4');tri=mesh.triangles(a,p)
            before_cross=np.cross(pos[tri[:,1]]-pos[tri[:,0]],pos[tri[:,2]]-pos[tri[:,0]])
            after_cross=np.cross(new[tri[:,1]]-new[tri[:,0]],new[tri[:,2]]-new[tri[:,0]])
            valid=np.linalg.norm(before_cross,axis=1)>1e-7
            assert np.all(np.einsum('ij,ij->i',before_cross[valid],after_cross[valid])>0)
            assert np.isfinite(new).all() and np.isfinite(norm).all() and new[:,1].min()>9
            parts.append(dict(material=p['material'],positions=new.tolist(),normals=norm.tolist(),changed_vertices=int(active.sum())))
            add_object(zone+'_'+str(i),new,tri,p['material'],col,p['streams'].get((5,0)))
        variant[zone]=parts
    report['variants'][str(level)]=variant
    bpy.ops.object.select_all(action='DESELECT')
    for o in col.objects:o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(out/(model+'-kit'+str(level)+'.glb')),export_format='GLB',use_selection=True,export_yup=True)
    col.hide_viewport=True;col.hide_render=True
bpy.ops.wm.save_as_mainfile(filepath=str(out/(model+'-morphed-study.blend')))
(out/'native-vertex-study.json').write_text(json.dumps(report,indent=2)+'\n')
print('Exported continuous stock-surface Outback kit study; no installation.')
