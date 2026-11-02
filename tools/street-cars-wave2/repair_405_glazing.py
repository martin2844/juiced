"""405-only material assignment on four retained connected window sheets."""
import bpy,sys,json,hashlib
from pathlib import Path
from mathutils import Vector
source,out=map(Path,sys.argv[sys.argv.index('--')+1:]);assert not out.exists();out.mkdir(parents=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();original=sha(source)
bpy.ops.wm.open_mainfile(filepath=str(source));obj=bpy.data.objects.get('1994_405_Plastics_1994_405_Plastic_0');assert obj and len(obj.data.polygons)==2199
mesh=obj.data;before=[tuple(v.co) for v in mesh.vertices];old_slots=[m.name if m else None for m in mesh.materials];assert old_slots==['1994_405_Plastic']
# Weld for component identification only. No source vertex, normal or UV is changed.
keys={};representative=[]
for v in mesh.vertices:
 key=tuple(round(x,5) for x in v.co);representative.append(keys.setdefault(key,len(keys)))
parent=list(range(len(keys)))
def root(i):
 while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
 return i
for p in mesh.polygons:
 a=representative[p.vertices[0]]
 for vi in p.vertices[1:]:parent[root(representative[vi])]=root(a)
groups={}
for p in mesh.polygons:groups.setdefault(root(representative[p.vertices[0]]),[]).append(p)
selected=[];sheets=[]
for polys in groups.values():
 world=[obj.matrix_world@mesh.vertices[i].co for p in polys for i in p.vertices]
 lo=Vector([min(v[i] for v in world) for i in range(3)]);hi=Vector([max(v[i] for v in world) for i in range(3)])
 # Four complete high, broad sheets, unlike detached seals/pillars/mirrors.
 if lo.z>.88 and hi.z>1.38 and max(hi.x-lo.x,hi.y-lo.y)>1.3 and len(polys) in [28,26,112,113]:
  selected.extend(polys);sheets.append(dict(polygons=len(polys),min=list(lo),max=list(hi)))
assert sorted(x['polygons'] for x in sheets)==[26,28,112,113],sheets
m=bpy.data.materials.new('oj_glass');m.use_nodes=True;m.diffuse_color=(.07,.10,.13,.33);node=m.node_tree.nodes.get('Principled BSDF');node.inputs['Base Color'].default_value=(.07,.10,.13,1);node.inputs['Alpha'].default_value=.33;node.inputs['Roughness'].default_value=.1;m.surface_render_method='DITHERED';slot=len(mesh.materials);mesh.materials.append(m)
for p in selected:p.material_index=slot
assert before==[tuple(v.co) for v in mesh.vertices]
bpy.ops.wm.save_as_mainfile(filepath=str(out/'peugeot-405-glx-prepared.blend'))
bpy.ops.export_scene.gltf(filepath=str(out/'peugeot-405-glx-prepared.glb'),export_format='GLB',export_yup=True)
assert sha(source)==original
(out/'material-change.json').write_text(json.dumps(dict(source=str(source),source_sha256=original,window_polygons=len(selected),sheets=sheets,original_vertex_positions_unchanged=True,original_uvs_and_normals_unchanged=True,scope='Material-only 405 glass candidate; no original source write, geometry change or physics change; native acceptance pending.'),indent=2)+'\n')
