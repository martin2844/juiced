"""Repair explicit source panels/materials in a separate authoring directory.
Never overwrites the original prepared scenes. Native validation is still required.
"""
import bpy,sys,math
from pathlib import Path
from mathutils import Vector
source,out=map(Path,sys.argv[sys.argv.index('--')+1:]);out.mkdir(parents=True,exist_ok=True)
for key in ['volvo-v70','volvo-240','subaru-outback-2002','jetta-vento-mk5','peugeot-406-coupe']:
 bpy.ops.wm.open_mainfile(filepath=str(source/(key+'-prepared.blend')))
 for o in list(bpy.context.scene.objects):
  if o.type!='MESH':continue
  if key=='peugeot-406-coupe' and o.name.startswith(('wheelRL_','wheelRR_')):
   # The mounted replacement rear wheels were 8cm ahead of the source arch.
   # Correct the source fit; export then normalizes the measured axle spacing
   # to the specified 270cm wheelbase, as for the other models.
   for v in o.data.vertices:v.co.y+=.08
  if key=='volvo-v70' and 'whitexgaps' in o.name.lower():o.name=o.name.replace('wheelRR_wheels_','body_').replace('wheelRL_wheels_','body_')
  if key=='volvo-240' and 'headlights texture' in o.name:
   # Source UVs wrap outside the lamp atlas and sample the red rear-light image.
   # Use the actual white front lens island for both headlight housings.
   vs=[v.co for v in o.data.vertices];xmin=min(abs(v.x) for v in vs);xmax=max(abs(v.x) for v in vs);ymin=min(v.z for v in vs);ymax=max(v.z for v in vs)
   uv=o.data.uv_layers.active
   for loop in o.data.loops:
    v=o.data.vertices[loop.vertex_index].co
    u=(abs(v.x)-xmin)/(xmax-xmin);t=(v.z-ymin)/(ymax-ymin)
    uv.data[loop.index].uv=(.30+.375*u,1-(.925-.25*t))
   for m in o.data.materials:
    if m and m.use_nodes:
     node=m.node_tree.nodes.get('Principled BSDF')
     for link in list(node.inputs['Alpha'].links):m.node_tree.links.remove(link)
     node.inputs['Alpha'].default_value=1
     if 'Transmission Weight' in node.inputs:node.inputs['Transmission Weight'].default_value=0
  if key=='subaru-outback-2002' and o.name.startswith(('pCube28_','pCube29_')):
   # These are the two plate assemblies, not painted bodywork. Preserve the
   # original atlas and UVs on every face, including the green Oregon artwork.
   atlas=bpy.data.materials.get('Subaru_Outback')
   assert atlas is not None
   o.data.materials.clear();o.data.materials.append(atlas)
   for p in o.data.polygons:p.material_index=0
  if key in ['jetta-vento-mk5','peugeot-406-coupe']:
   # Retain source topology; mark real hard creases rather than smoothing trim
   # and door edges indiscriminately with neighbouring surfaces.
   import bmesh
   bm=bmesh.new();bm.from_mesh(o.data)
   for e in bm.edges:
    if e.is_manifold:e.smooth=e.calc_face_angle(0)<math.radians(40)
   bm.to_mesh(o.data);bm.free()
 bpy.ops.wm.save_as_mainfile(filepath=str(out/(key+'-prepared.blend')))
 bpy.ops.export_scene.gltf(filepath=str(out/(key+'-prepared.glb')),export_format='GLB',export_yup=True)
 print('REPAIRED',key,flush=True)
