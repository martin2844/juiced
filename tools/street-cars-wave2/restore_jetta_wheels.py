"""Fit one original source alloy assembly instead of the provisional five bars."""
import bpy,sys
from pathlib import Path
from mathutils import Vector
source,out=map(Path,sys.argv[sys.argv.index('--')+1:])
bpy.ops.wm.open_mainfile(filepath=str(source/'jetta-vento-mk5-source.blend'))
parts=[]
for o in bpy.context.scene.objects:
 if o.type!='MESH' or not o.parent or o.parent.name!='jettaa5_wheel2':continue
 points=[tuple(o.matrix_world@v.co) for v in o.data.vertices]
 parts.append((o.name,points,[tuple(p.vertices) for p in o.data.polygons]))
assert len(parts)==3
bpy.ops.wm.open_mainfile(filepath=str(out/'jetta-vento-mk5-prepared.blend'))
for o in list(bpy.context.scene.objects):
 if o.type=='MESH' and o.name.startswith('wheel') and not o.name.endswith('_tire'):bpy.data.objects.remove(o,do_unlink=True)
def material(name,c):
 m=bpy.data.materials.new(name);m.use_nodes=True;n=m.node_tree.nodes.get('Principled BSDF');n.inputs['Base Color'].default_value=(*c,1);n.inputs['Metallic'].default_value=.7;m.diffuse_color=(*c,1);return m
metal=material('rubber_source_alloy',(.42,.44,.46));black=material('rubber_source_cap',(.025,.027,.03))
for axle,y in [('F',-1.33),('R',1.248)]:
 for side,x in [('L',.76),('R',-.76)]:
  sign=1 if x>0 else -1
  for name,points,faces in parts:
   scale=.205/.2314
   verts=[(x+sign*p[0]*scale,y+p[1]*scale,.315+p[2]*scale) for p in points]
   if sign<0:faces=[tuple(reversed(f)) for f in faces]
   mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
   o=bpy.data.objects.new('wheel'+axle+side+'_'+name,mesh);bpy.context.collection.objects.link(o);o.data.materials.append(metal if 'alu' in name else black)
   for p in mesh.polygons:p.use_smooth=True
bpy.ops.wm.save_as_mainfile(filepath=str(out/'jetta-vento-mk5-prepared.blend'))
bpy.ops.export_scene.gltf(filepath=str(out/'jetta-vento-mk5-prepared.glb'),export_format='GLB',export_yup=True)
