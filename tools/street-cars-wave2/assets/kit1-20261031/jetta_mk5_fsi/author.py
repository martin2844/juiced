"""Measured native kit studies with protected vinyl-receiving surfaces.

Blender --background --python-exit-code 1 --python THIS -- REPO FRESH_OUTPUT MODEL [--levels 1 ...]
Original side faces which can receive any stock vinyl remain unchanged. New
skirts/splitters sit below the measured lowest decal; no new engine code.
"""
from pathlib import Path
import argparse,hashlib,json,math,sys
import bpy,bmesh
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('repo',type=Path);parser.add_argument('out',type=Path);parser.add_argument('model')
parser.add_argument('--levels',type=int,nargs='+',choices=[1,2,3],default=[2,3])
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);repo,out,model_text=args.repo,args.out,args.model
levels=sorted(set(args.levels))
config_path=Path(__file__).resolve().with_name('native_kit_profiles.json')
cfg=next(p for p in json.loads(config_path.read_text()) if p['model']==model_text)
model,prefix=cfg['model'],cfg['prefix']
if out.exists():raise RuntimeError('Fresh output required')
out.mkdir(parents=True);sys.path.insert(0,str(repo/'tools'))
from vinyl_pack import mesh
folder=repo/'mods/street-cars-wave2/assets/cars'/model/'mesh'
hierarchy=folder.parent/'models'/(model+'_stock.boh')
nodes={n:x for n,_,x in mesh.boh(hierarchy.read_bytes())}
wp=folder/(prefix+'_wheel.d8m');wheel=mesh.read(wp.read_bytes())
radius=float(np.max(np.abs(np.concatenate([p['streams'][0,0] for p in wheel['parts']])[:,2])))
front,rear=float(nodes['WHEEL_FL'][2,3]),float(nodes['WHEEL_RL'][2,3])
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source={str(p.relative_to(repo)):sha(p) for p in [hierarchy,wp]}
for name,h in cfg['vinyl_source_sha256'].items():assert sha(folder/name)==h,name
floor=cfg['receiver_protection_floor_cm']
def smooth(x):
    x=np.clip(x,0,1);return x*x*(3-2*x)
def convert(p):return np.c_[p[:,0],-p[:,2],p[:,1]]/100
def deform(p,zone,level,bottom):
    q=np.array(p,dtype=float).copy()
    if zone.startswith('sskirt'):return q
    height=cfg['upper_heights_cm'][zone];h=smooth((height-p[:,1])/(height-bottom))
    axle=front if zone=='fbump' else rear
    weight=h*smooth((np.abs(p[:,2]-axle)-radius-5)/18)
    if zone=='rbump':
        for a,b in cfg['exhaust_cutouts_x_cm']:
            weight*=smooth(np.maximum(a-p[:,0],p[:,0]-b)/7)
    drop=min(1.5,cfg['level2_drop_cm'][zone]*.4) if level==1 else cfg['level2_drop_cm'][zone] if level==2 else min(3.,cfg['level2_drop_cm'][zone])
    q[:,1]-=drop*weight
    q[:,2]+=(1 if zone=='fbump' else -1)*(.5 if level==1 else 1.5 if level==2 else 5.)*weight
    q[:,0]+=np.sign(p[:,0])*(.3 if level==1 else .8 if level==2 else 2.)*weight*(np.abs(p[:,0])/90.)**4
    return q

bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC'
ref=bpy.data.collections.new('CURRENT_STOCK_REFERENCE');scene.collection.children.link(ref);materials={}
def material(name):
    if name not in materials:
        m=bpy.data.materials.new(name);m.diffuse_color=(.08,.24,.5,1) if name==model+'_body' else (.10,.10,.11,1) if name.endswith('_kit_trim') else (.3,.32,.33,1);materials[name]=m
    return materials[name]
def add_object(name,pos,tri,mat,col,uv=None):
    m=bpy.data.meshes.new(name);m.from_pydata(convert(pos).tolist(),[],tri.tolist());m.update()
    o=bpy.data.objects.new(name,m);col.objects.link(o);m.materials.append(material(mat))
    if uv is not None:
        layer=m.uv_layers.new()
        for loop in m.loops:layer.data[loop.index].uv=uv[loop.vertex_index]
    return o
def solid(name,sections,mat,col):
    verts=[v for row in sections for v in row];faces=[]
    for i in range(len(sections)-1):
        for j in range(4):faces.append((4*i+j,4*(i+1)+j,4*(i+1)+(j+1)%4,4*i+(j+1)%4))
    faces.extend([(3,2,1,0),tuple(range(len(verts)-4,len(verts)))])
    m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update()
    bm=bmesh.new();bm.from_mesh(m);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    if bm.calc_volume(signed=True)<0:bmesh.ops.reverse_faces(bm,faces=list(bm.faces))
    assert all(e.is_manifold for e in bm.edges) and bm.calc_volume()>1e-8,name
    bm.to_mesh(m);bm.free();o=bpy.data.objects.new(name,m);col.objects.link(o);m.materials.append(material(mat))
    uv=m.uv_layers.new()
    for loop in m.loops:uv.data[loop.index].uv=(.05,.05)
    bevel=o.modifiers.new('3mm edge radius','BEVEL');bevel.width=.003;bevel.segments=2
    return o
def tree_for(parts):
    vertices=[];faces=[]
    for p in parts:
        start=len(vertices);vertices.extend(convert(np.asarray(p['positions'])).tolist());faces.extend((np.asarray(p['triangles'])+start).tolist())
    return BVHTree.FromPolygons(vertices,faces),np.asarray(vertices)

zones=['fbump','rbump','sskirt_l','sskirt_r'];parsed={}
for zone in ['chass','bonnet']+zones:
    path=folder/(prefix+'_'+zone+'.d8m');source[str(path.relative_to(repo))]=sha(path);a=mesh.read(path.read_bytes());parsed[zone]=a
    for i,p in enumerate(a['parts']):add_object('STOCK_'+zone+'_'+str(i),p['streams'][0,0],mesh.triangles(a,p),p['material'],ref,p['streams'].get((5,0)))
report=dict(status='NOT INSTALLED; native visual review required',model=model,prefix=prefix,profile=cfg,profile_sha256=hashlib.sha256(json.dumps(cfg,sort_keys=True).encode()).hexdigest(),profile_file_sha256=sha(config_path),authoring_tool_sha256=sha(Path(__file__)),source_sha256=source,
            method='Protected side receivers; continuous original bumper deformation and fitted lower additions',variants={},additions={})
for level in levels:
    col=bpy.data.collections.new('KIT_'+str(level));scene.collection.children.link(col);variant={};addon_report=[]
    for zone in zones:
        a=parsed[zone];bottom=float(np.concatenate([p['streams'][0,0] for p in a['parts']])[:,1].min());parts=[]
        protected=[]
        for p in a['parts']:
            pos=p['streams'][0,0];tri=mesh.triangles(a,p);v=pos[tri]
            f=np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0])
            receiving=(np.abs(f[:,0])>.35*np.linalg.norm(f,axis=1))&(f[:,0]*v[:,:,0].mean(1)>0)&(v[:,:,1].max(1)>=floor)
            protected.extend(pos[np.unique(tri[receiving])].tolist())
            # Preserve lamps, chrome, plate frames and other small fitted
            # hardware; their near-collinear faces must not be stretched.
            if p['material']!=model+'_body' and not (model=='outback_2002' and p['material']==prefix+'_m0'):
                protected.extend(pos.tolist())
        # Protect coincident seams across material primitives as well. A smooth
        # 6cm transition replaces the earlier hard fixed/moving boundary.
        kd=KDTree(len(protected)) if protected else None
        if kd:
            for j,v in enumerate(protected):kd.insert(v,j)
            kd.balance()
        strength=1.
        def fitted(points):
            result=deform(points,zone,level,bottom)
            if kd:
                distance=np.asarray([kd.find(v)[2] for v in points])
                result=points+(result-points)*smooth(distance/6)[:,None]
            return points+(result-points)*strength
        # Use one strength for the complete assembly, including coincident
        # material seams. Reject local folds rather than hiding them in export.
        for attempt in range(6):
            safe=True
            for p in a['parts']:
                pos=p['streams'][0,0].astype(float);tri=mesh.triangles(a,p);new=fitted(pos).astype('<f4')
                before=np.cross(pos[tri[:,1]]-pos[tri[:,0]],pos[tri[:,2]]-pos[tri[:,0]])
                after=np.cross(new[tri[:,1]]-new[tri[:,0]],new[tri[:,2]]-new[tri[:,0]])
                valid=np.linalg.norm(before,axis=1)>1e-7
                if np.any(np.einsum('ij,ij->i',before[valid],after[valid])<=0):safe=False;break
            if safe:break
            strength*=.5
        if not safe:raise RuntimeError(f'No safe assembly deformation {model} {zone} {level}')
        for i,p in enumerate(a['parts']):
            pos=p['streams'][0,0].astype(float);tri=mesh.triangles(a,p);norm=p['streams'][3,0].copy()
            v=pos[tri];face=np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]);length=np.linalg.norm(face,axis=1)
            # Same side-facing envelope used to transfer the native vinyls, with
            # a 2mm height margin for their .8mm normal offset. Protect the whole
            # receiving triangle, not just the decal vertices within it.
            receiving=(np.abs(face[:,0])>.35*length)&(face[:,0]*v[:,:,0].mean(1)>0)&(v[:,:,1].max(1)>=floor)
            frozen=np.zeros(len(pos),dtype=bool);frozen[np.unique(tri[receiving])]=True
            if kd:frozen|=np.asarray([kd.find(v)[2]<1e-4 for v in pos])
            new=fitted(pos);new[frozen]=pos[frozen]
            active=np.max(np.abs(new-pos),axis=1)>1e-7
            if active.any():
                jac=np.zeros((int(active.sum()),3,3));h=.001
                for axis in range(3):
                    d=np.zeros(3);d[axis]=h;jac[:,:,axis]=(fitted(pos[active]+d)-fitted(pos[active]-d))/(2*h)
                assert np.all(np.linalg.det(jac)>0),(model,zone)
                n=np.einsum('nij,nj->ni',np.linalg.inv(jac).transpose(0,2,1),norm[active]);n/=np.linalg.norm(n,axis=1)[:,None];norm[active]=n
            new=new.astype('<f4');new[~active]=p['streams'][0,0][~active]
            before=np.cross(pos[tri[:,1]]-pos[tri[:,0]],pos[tri[:,2]]-pos[tri[:,0]])
            after=np.cross(new[tri[:,1]]-new[tri[:,0]],new[tri[:,2]]-new[tri[:,0]])
            valid=np.linalg.norm(before,axis=1)>1e-7
            bad=np.flatnonzero(valid&(np.einsum('ij,ij->i',before,after)<=0))
            if len(bad):
                raise RuntimeError(json.dumps(dict(model=model,zone=zone,level=level,material=p['material'],inverted_faces=bad.tolist(),positions=pos[tri[bad]].tolist(),proposed=new[tri[bad]].tolist(),frozen=frozen[tri[bad]].tolist())))
            assert np.isfinite(new).all() and np.isfinite(norm).all()
            assert new[:,1].min()>=min(9.5,float(pos[:,1].min()))-.001,(model,zone,'new low point')
            assert np.array_equal(new[tri[receiving]],p['streams'][0,0][tri[receiving]])
            assert np.array_equal(norm[tri[receiving]],p['streams'][3,0][tri[receiving]])
            parts.append(dict(material=p['material'],positions=new.tolist(),normals=norm.tolist(),triangles=tri.tolist(),protected_vertices=np.flatnonzero(frozen).tolist(),protected_receiver_faces=int(receiving.sum()),changed_vertices=int(active.sum()),assembly_deformation_strength=strength))
            add_object(zone+'_'+str(i),new,tri,p['material'],col,p['streams'].get((5,0)))
        variant[zone]=parts
    # Append skirts below every existing vinyl. Stock receiver faces do not move.
    for side,zone in [(-1,'sskirt_l'),(1,'sskirt_r')]:
        tree,pts=tree_for(variant[zone]);outer=max(abs(pts[:,0]));start=-front/100+radius/100+.05;end=-rear/100-radius/100-.05
        start=max(start,float(pts[:,1].min())+.02);end=min(end,float(pts[:,1].max())-.02)
        ceiling=(cfg['vinyl_min_height_cm']-(.6 if level==1 else 1))/100;sections=[];hits=[]
        for y in np.linspace(start,end,25):
            hit=None
            for z in np.arange(max(.105,float(pts[:,2].min())-.002),ceiling,.003):
                p=tree.ray_cast(Vector((side*(outer+1),y,z)),Vector((-side,0,0)),2)[0]
                if p is not None and abs(p.x)>.9*outer:hit=p;break
            hits.append(hit)
        usable=[i for i,p in enumerate(hits) if p is not None]
        if len(usable)<18 or any(hits[i] is None for i in range(usable[0],usable[-1]+1)):
            raise RuntimeError(f'Missing continuous lower skirt mount {model} {zone}: {usable}; ceiling={ceiling}; bounds={pts.min(0).tolist()},{pts.max(0).tolist()}')
        # Ends can rise into an arch above the vinyl-safe band. Finish the new
        # lower extension on the last actual contact; never bridge empty space.
        hits=hits[usable[0]:usable[-1]+1];start,end=hits[0].y,hits[-1].y
        for hit in hits:
            x,y,z=hit;shape=max(.15,math.sin(math.pi*(y-start)/(end-start)))
            ext=(.012 if level==1 else .025 if level==2 else .04)*shape;lower=max(.095,z-(.015 if level==1 else .03 if level==2 else .02))
            sections.append([(x-side*.015,y,z+(.004 if level==1 else .008)),(x+side*ext,y,z-.004),(x+side*ext,y,lower),(x-side*.015,y,lower)])
        mat=prefix+'_m0' if model=='outback_2002' and level in [1,2] else model+'_body' if level in [1,2] or model=='outback_2002' else prefix+'_kit_trim'
        solid('ADD_'+zone.upper(),sections,mat,col);addon_report.append(dict(zone=zone,kind='lower skirt',root_count=len(sections),max_height_m=max(v[2] for s in sections for v in s),vinyl_min_height_m=cfg['vinyl_min_height_cm']/100))
    if level in [1,3]:
        # A narrow black splitter; its visible edge is continuous and remains
        # below the lowest source decal, unlike the old wavy sampled strips.
        front_parts=variant['fbump']
        if level==1:
            # OEM lip roots on the actual exterior fascia. The complete front
            # slot also contains undertray/liner geometry, unsuitable as a mount.
            root_material=(prefix+'_m0' if model in ['peugeot_405_glx','outback_2002'] else
                           prefix+'_m3' if model=='jetta_mk5_fsi' else model+'_body')
            front_parts=[p for p in front_parts if p['material']==root_material]
            assert front_parts,(model,root_material)
        tree,pts=tree_for(front_parts);end=float(pts[:,1].min());width=min(abs(pts[:,0].min()),abs(pts[:,0].max()))*.90;samples=[]
        for x in np.linspace(-width,width,33):
            hit=None
            for z in np.arange(max(.11,float(pts[:,2].min())+.005),(cfg['vinyl_min_height_cm']-1)/100,.003):
                p=tree.ray_cast(Vector((x,end-1,z)),Vector((0,1,0)),2)[0]
                if p is not None and -p.y*100>front+radius+2:hit=p;break
            if hit is None and level!=1:raise RuntimeError(f'Missing splitter mount {model} {x}')
            samples.append(hit)
        if level==1:
            usable=[i for i,p in enumerate(samples) if p is not None]
            if len(usable)<25 or any(samples[i] is None for i in range(usable[0],usable[-1]+1)):
                raise RuntimeError(f'No continuous exterior OEM lip mount {model}: {usable}')
            # The fascia can rise above the vinyl-safe band at its ends. Finish
            # at the last genuine contact; do not attach a lip to the undertray.
            samples=samples[usable[0]:usable[-1]+1]
        top=max(.105,min(p.z for p in samples)-.003);bottom=max(.095,top-(.01 if level==1 else .015));sections=[]
        for p in samples:
            x,y,z=p;taper=max(.15,1-(abs(x)/width)**6)
            extension=.012 if level==1 else .035
            sections.append([(x,y+.015,z+.008),(x,y-extension*taper,top),(x,y-extension*taper,bottom),(x,y+.015,bottom)])
        front_mat=(prefix+'_m0' if model=='outback_2002' else prefix+'_kit_trim' if model=='jetta_mk5_fsi' else model+'_body') if level==1 else prefix+'_kit_trim'
        solid('ADD_FBUMP_SPLITTER',sections,front_mat,col);addon_report.append(dict(zone='fbump',kind='OEM lip' if level==1 else 'splitter',root_count=len(samples),bottom_m=bottom,root_materials=[p['material'] for p in front_parts]))
    # A contoured rear lip follows the actual rear bumper, split around every
    # exhaust opening. Unlike the retired flat slab it has a short sealed
    # section at every sample and a curved, bumper-matched visible edge.
    tree,pts=tree_for(variant['rbump']);end=float(pts[:,1].max());width=min(abs(pts[:,0].min()),abs(pts[:,0].max()))*.94
    samples=[]
    for x in np.linspace(-width,width,65):
        hit=None
        if not any(a/100-.01<x<b/100+.01 for a,b in cfg['exhaust_cutouts_x_cm']):
            for z in np.arange(max(.115,float(pts[:,2].min())+.003),(cfg['vinyl_min_height_cm']-.9)/100,.003):
                p=tree.ray_cast(Vector((x,end+1,z)),Vector((0,-1,0)),2)[0]
                if p is not None and p.y*100>-rear+radius+2:hit=p;break
        samples.append(hit)
    groups=[];current=[]
    for hit in samples+[None]:
        if hit is not None:current.append(hit)
        elif current:
            if len(current)>=3:groups.append(current)
            current=[]
    for index,group in enumerate(groups):
        sections=[]
        for p in group:
            x,y,z=p;taper=max(.2,math.sin(math.pi*(x-group[0].x)/(group[-1].x-group[0].x)))
            ext=(.008 if level==1 else .015 if level==2 else .025)*taper;bottom=max(.095,z-(.012 if level==1 else .025 if level==2 else .03))
            sections.append([(x,y-.015,z+.006),(x,y+ext,z-.005),(x,y+ext,bottom),(x,y-.015,bottom)])
        mat=model+'_body' if level==2 else prefix+'_kit_trim'
        if model=='outback_2002' and level in [1,2]:mat=prefix+'_m0'
        solid('ADD_RBUMP_LIP_'+str(index),sections,mat,col)
        addon_report.append(dict(zone='rbump',kind='contoured lip',root_count=len(group),max_height_m=max(v[2] for s in sections for v in s),exhaust_cutouts_x_cm=cfg['exhaust_cutouts_x_cm']))
    report['variants'][str(level)]=variant;report['additions'][str(level)]=addon_report
    bpy.ops.object.select_all(action='DESELECT')
    for o in col.objects:o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(out/(model+'-kit'+str(level)+'.glb')),export_format='GLB',use_selection=True,export_yup=True,export_apply=True)
    col.hide_viewport=True;col.hide_render=True
bpy.ops.wm.save_as_mainfile(filepath=str(out/(model+'-fitted-kits.blend')))
(out/'native-vertex-study.json').write_text(json.dumps(report,indent=2)+'\n')
print('FITTED',model,'native vinyl receivers protected; private study only')
