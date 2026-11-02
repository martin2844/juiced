"""Blender: restore the source Jetta rear lamp assemblies and real brake cells.

-- REPO ORIGINAL_BLEND PREPARED_GLB BASE_PACKAGE FRESH_PACKAGE FRESH_STUDY
Only retained rear-light source faces, brake overlay and flare anchors change.
"""
from pathlib import Path
import collections,hashlib,json,re,shutil,struct,sys
import bpy
import numpy as np
from mathutils import Vector
r,original,prepared,base,out,study=map(Path,sys.argv[sys.argv.index('--')+1:]);assert not out.exists() and not study.exists();study.mkdir(parents=True)
sys.path[:0]=[str(r/'tools'),str(r/'mods/street-cars/tools'),str(r/'mods/ferrari/tools')]
from vinyl_pack import mesh
from gltf import Gltf
from import_ferrari import mesh_bytes,bounds
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
g=Gltf(prepared);ps=g.parts()
def wheel(p):return bool(re.search(r'/wheel(?:fl|fr|rl|rr|_[rl][fr])(?:/|_|$)',p['source'].lower()))
tyres=[p for p in ps if wheel(p) and (any(k in p['source'].lower() for k in ['_tire','_tyre','tire_','tyre_']) or g.j['materials'][p['mi']].get('name','').lower() in ['rubber_oj_rubber','oj_rubber'])]
v=np.concatenate([p['pos'] for p in tyres]);mid=(v[:,2].min()+v[:,2].max())/2;f=v[(v[:,0]>0)&(v[:,2]>mid)];b=v[(v[:,0]>0)&(v[:,2]<mid)];fc=(f.min(0)+f.max(0))/2;rc=(b.min(0)+b.max(0))/2
scale=257.8/(fc[2]-rc[2]);offset=np.array([0,v[:,1].min(),(fc[2]+rc[2])/2]);sign=np.array([-1,1,1])
convert=lambda p:(p-offset)*scale*sign
bpy.ops.wm.open_mainfile(filepath=str(original));stock={root+'_'+side for root in ['jettaa5_trunklightcovers','jettaa5_taillightglass','jettaa5_trunklightsframe','jettaa5_trunklights','jettaa5_taillightframe','jettaa5_taillight','jettaa5_taillight_frame'] for side in ['r','l']}
pieces=[];selected=set();brakes=[];materials={}
oldtags={obj.name:obj.parent.name.lower() for obj in bpy.context.scene.objects if obj.type=='MESH' and obj.parent and (obj.parent.name.lower() in stock or obj.parent.name.lower().startswith('jettaa5_trunklightcovers_black'))}
colours=dict(housing=(.28,.008,.006,1),chrome=(.48,.5,.53,1),trim=(.018,.02,.024,1),red_lens=(.34,.008,.006,.22),clear_lens=(.55,.58,.62,.12),bulb=(.38,.008,.004,1))
for obj in list(bpy.context.scene.objects):
    assembly=obj.parent.name.lower() if obj.parent else ''
    if obj.type!='MESH' or assembly not in stock or not obj.data.polygons:continue
    selected.add(obj.name);obj.data.calc_loop_triangles();verts=[];norm=[];uv=[];layer=obj.data.uv_layers.active;isbrake='brakelight' in obj.name.lower()
    kind='bulb' if isbrake else 'red_lens' if 'taillightglass' in assembly else 'clear_lens' if 'trunklightcovers' in assembly else 'trim' if 'taillight_frame' in assembly or 'trunklightsframe' in assembly else 'housing' if 'taillightframe' in assembly else 'chrome'
    mat='w260_rear_'+kind;materials[mat]=colours[kind];normalmatrix=obj.matrix_world.to_3x3().inverted().transposed()
    for tri in obj.data.loop_triangles:
        for vi,li in zip(tri.vertices,tri.loops):
            q=obj.matrix_world@obj.data.vertices[vi].co
            # glTF Y-up = (Blender X, Blender Z, -Blender Y).
            verts.append((q.x,q.z,-q.y));n=normalmatrix@obj.data.corner_normals[li].vector;n.normalize();norm.append((n.x,n.z,-n.y));uv.append(tuple(layer.data[li].uv) if layer else (0,0))
    pos=convert(np.array(verts));normal=np.array(norm)*sign;tri=np.arange(len(pos)).reshape(-1,3)[:,[0,2,1]]
    part=dict(mat=mat,pos=pos,norm=normal,uv=np.array(uv),tri=tri);pieces.append(part)
    if isbrake:
        on=dict(part);on['mat']='w260_rear_on';on['pos']=pos+normal*.08;brakes.append(on)
    obj['source_assembly']=assembly;obj['native_rear_lamp_material']=mat
    preview=bpy.data.materials.get(mat) or bpy.data.materials.new(mat);preview.use_nodes=True;preview.diffuse_color=colours[kind]
    node=preview.node_tree.nodes.get('Principled BSDF');node.inputs['Base Color'].default_value=colours[kind];node.inputs['Alpha'].default_value=colours[kind][3];node.inputs['Roughness'].default_value=.18
    if colours[kind][3]<1:preview.surface_render_method='DITHERED'
    obj.data.materials.clear();obj.data.materials.append(preview)
    for face in obj.data.polygons:face.material_index=0
assert len(pieces)==16 and len(brakes)==2,(len(pieces),len(brakes))

# Prepared GLB object identities are retained from the original source.
# No dependency on an earlier diagnostic's scene copy or JSON report.
targets=[]
for p in ps:
    if p['source'].split('/')[-1] in oldtags:targets.extend(convert(p['pos'])[p['tri'][:,[0,2,1]]])
targets=np.array(targets);assert len(targets)>1000,len(targets)

# Keep an editable source scene containing these exact restored assemblies.
for obj in list(bpy.context.scene.objects):
    if obj.type=='MESH' and obj.name not in selected:bpy.data.objects.remove(obj,do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=str(study/'jetta-rear-lamps.blend'))
shutil.copytree(base,out);folder=out/'assets/cars/jetta_mk5_fsi/mesh';old=mesh.read((folder/'w260_chass.d8m').read_bytes());lookup=collections.defaultdict(list)
for pi,p in enumerate(old['parts']):
    tri=mesh.triangles(old,p);t=p['streams'][0,0][tri]
    for ti,c in enumerate(t.mean(1)):lookup[tuple(np.floor(c/.01).astype(int))].append((pi,ti))
remove=collections.defaultdict(set)
for target in targets:
    cell=np.floor(target.mean(0)/.01).astype(int);found=None
    for dx in [-1,0,1]:
        for dy in [-1,0,1]:
            for dz in [-1,0,1]:
                for pi,ti in lookup.get(tuple(cell+[dx,dy,dz]),[]):
                    if ti in remove[pi]:continue
                    t=old['parts'][pi]['streams'][0,0][mesh.triangles(old,old['parts'][pi])[ti]];d=np.linalg.norm(t[:,None,:]-target[None,:,:],axis=2)
                    if np.all(d.min(0)<.003) and np.all(d.min(1)<.003):found=(pi,ti);break
    if found is not None:remove[found[0]].add(found[1])
matched=sum(map(len,remove.values()));assert matched==len(targets),(matched,len(targets))
added=mesh.read(mesh_bytes(pieces));indices=[];payload=[];first=0
for i,p in enumerate(old['parts']):
    tri=mesh.triangles(old,p);keep=np.ones(len(tri),dtype=bool);keep[list(remove[i])]=False;tri=tri[keep]
    if not len(tri):continue
    h=bytearray(p['header']);struct.pack_into('<3I',h,32,first,tri.size,1);first+=tri.size;indices.append(tri.astype('<u2').reshape(-1));payload.append(bytes(h)+bytes(p['verts']))
for p in added['parts']:
    tri=mesh.triangles(added,p);h=bytearray(p['header']);struct.pack_into('<3I',h,32,first,tri.size,1);first+=tri.size;indices.append(tri.astype('<u2').reshape(-1));payload.append(bytes(h)+bytes(p['verts']))
index=np.concatenate(indices);chass_data=struct.pack('<I',len(index))+index.tobytes()+struct.pack('<I',len(payload))+b''.join(payload)+bounds(np.concatenate([p['streams'][0,0] for p in old['parts']]+[p['pos'] for p in pieces]))
for suffix in ['','_ui']:
    root=out/'assets/cars'/('jetta_mk5_fsi'+suffix);f=root/'mesh';(f/'w260_chass.d8m').write_bytes(chass_data);(f/'w260_brklight.d8m').write_bytes(mesh_bytes(brakes))
    for mat,colour in materials.items():
        rgba=(np.array(colour)*255).astype(np.uint8);(root/'textures'/(mat+'.ddt')).write_bytes(struct.pack('<9IfI',21,1,1,1,2,2,0,1,1,0.,1)+rgba[[2,1,0,3]].tobytes())
        alpha=colour[3]<1;program='Coloured Spec ABlend' if alpha else 'Specular Opaque';texkey='Texture' if alpha else 'Opaque Texture'
        (f/'mats'/(mat+'.pmd')).write_text('Material Program = '+program+'\nColour = 255 255 255\nSpecular Colour = 110 110 110\nSpecular Highlight = 55\nShininess = 40\nDouble-sided = TRUE\n'+texkey+' = '+mat+'.ptd\n')
    (f/'mats/w260_rear_on.pmd').write_text('Material Program = Constant Colour\nColour = 255 28 10\n')
    # Source-cell centres replace the old generic under-bumper flare anchors.
    cells=np.concatenate([p['pos'] for p in brakes]);positions={side:(cells[cells[:,0]*side>0].min(0)+cells[cells[:,0]*side>0].max(0))/2 for side in [-1,1]}
    boh=root/'models/jetta_mk5_fsi_stock.boh';data=bytearray(boh.read_bytes());at=0;flare_count=0
    def node():
        global at,flare_count
        name=data[at:at+32].split(b'\0')[0].decode();at+=32;matrix=at;at+=64
        if name.startswith('w260_RBRKLIGHTON'):
            side=-1 if name.endswith('-1') else 1;point=positions[side].copy();point[2]-=.8
            for index,value in zip([3,7,11],point):struct.pack_into('<f',data,matrix+index*4,float(value))
            flare_count+=1
        attach=struct.unpack_from('<I',data,at)[0];at+=4
        if attach==0:at+=32
        elif attach==2:size=struct.unpack_from('<I',data,at)[0];at+=4+size
        else:raise ValueError(attach)
        count=struct.unpack_from('<I',data,at)[0];at+=4
        for _ in range(count):node()
    while at<len(data):node()
    assert at==len(data) and flare_count==2
    boh.write_bytes(data)
hashes={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file() and p.name!='SHA256.json'};(out/'SHA256.json').write_text(json.dumps(hashes,indent=2)+'\n')
(study/'rear-lamp-repair.json').write_text(json.dumps(dict(original_source=str(original),original_sha256=sha(original),prepared_sha256=sha(prepared),removed_old_triangles=matched,restored_source_objects=sorted(selected),restored_triangles=sum(len(p['tri']) for p in pieces),brake_cell_triangles=sum(len(p['tri']) for p in brakes),new_brake_overlay='actual source brake cells; no emissive whole outer lens',candidate=str(out),installed=False),indent=2)+'\n')
print('Restored rear lamps:',matched,'old faces replaced with',sum(len(p['tri']) for p in pieces),'source faces; brake overlay uses real cells only.',flush=True)
