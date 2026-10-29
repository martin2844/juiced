import bpy,sys,math,json,re
import numpy as np
from pathlib import Path
from mathutils import Vector
root=Path(sys.argv[sys.argv.index('--')+1])
for car in sys.argv[sys.argv.index('--')+2:]:
 image_cache={};wheel_material_cache={}
 bpy.ops.wm.open_mainfile(filepath=str(root/(car+'-repaired.blend')))
 for o in list(bpy.context.scene.objects):
  if o.type!='MESH':bpy.data.objects.remove(o,do_unlink=True);continue
  # Normalize mesh-space geometry while retaining source texture coordinates.
  m=o.matrix_world.copy();o.parent=None;o.data=o.data.copy()
  for v in o.data.vertices:v.co=m@v.co
  o.matrix_world.identity()
  n=o.name.lower();vs=[v.co for v in o.data.vertices]
  if not vs:continue
  if car=='volvo-v70' and n.startswith('wheels_'):
   # Meshes containing both rear wheels must be split along the centerline.
   for side in [-1,1]:
    indexes=[p.index for p in o.data.polygons if sum(o.data.vertices[i].co.x for i in p.vertices)*side>0]
    if not indexes:continue
    coords=[];faces=[];uvs=[];uv=o.data.uv_layers.active
    for pi in indexes:
     p=o.data.polygons[pi];face=[]
     for li in p.loop_indices:
      face.append(len(coords));coords.append(o.data.vertices[o.data.loops[li].vertex_index].co.copy());uvs.append(uv.data[li].uv.copy() if uv else (0,0))
     faces.append(face)
    mesh=bpy.data.meshes.new(n);mesh.from_pydata(coords,[],faces);mesh.update();lay=mesh.uv_layers.new()
    for i,x in enumerate(uvs):lay.data[i].uv=x
    center=sum(coords,Vector())/len(coords);tag=('F' if center.y<0 else 'R')+('L' if side>0 else 'R');obj=bpy.data.objects.new('wheel'+tag+'_'+n,mesh);bpy.context.collection.objects.link(obj)
    for mat in o.data.materials:mesh.materials.append(mat)
   bpy.data.objects.remove(o,do_unlink=True)
 # Preserve source lens textures while identifying the actual switchable lamp surfaces.
 lamp_materials={}
 def lamp_material(source,kind):
  if kind not in lamp_materials:
   old=bpy.data.materials.get(kind)
   if old:old.name='unused_'+kind
   m=source.copy();m.name=kind;lamp_materials[kind]=m
  return lamp_materials[kind]
 for o in bpy.context.scene.objects:
  if o.type!='MESH':continue
  n=o.name.lower();assembly=o.get('source_assembly','')
  for i,m in enumerate(list(o.data.materials)):
   if not m:continue
   mn=m.name.lower();kind=None
   if car in ['peugeot-406-coupe','jetta-vento-mk5']:
    colour=None
    if 'black' in mn or any(t in mn for t in ['obivka','cloth','speaker','radiator','_pol','buttons','wood','lcd']):colour=(.025,.025,.028,1)
    elif any(t in mn for t in ['rearlowbeam','brakelight']):colour=(.48,.012,.008,1)
    elif 'signal_' in mn:colour=(.75,.19,.008,1)
    if colour:
     m=m.copy();m.use_nodes=True;node=m.node_tree.nodes.get('Principled BSDF')
     for link in list(node.inputs['Base Color'].links):m.node_tree.links.remove(link)
     node.inputs['Base Color'].default_value=colour;m.diffuse_color=colour;o.data.materials[i]=m
   if car=='golf-mk5-r32' and mn in ['gls_window','gls_windscreen']:
    glass=bpy.data.materials.get('oj_glass') or bpy.data.materials.new('oj_glass');glass.diffuse_color=(.07,.10,.13,1);glass.use_nodes=True;glass.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.07,.10,.13,1);o.data.materials[i]=glass
   lens=(car=='golf-mk5-r32' and mn=='gls_clear') or any(t in assembly for t in ['headlightglass','taillightglass','tailightglass'])
   if lens:
    m=m.copy();m.use_nodes=True;node=m.node_tree.nodes.get('Principled BSDF')
    for socket in ['Alpha','Base Color']:
     for link in list(node.inputs[socket].links):m.node_tree.links.remove(link)
    rear=any(t in assembly for t in ['taillightglass','tailightglass'])
    colour=(.48,.012,.008,.65) if rear else (.7,.8,.85,.15)
    node.inputs['Base Color'].default_value=colour;node.inputs['Alpha'].default_value=colour[3];m.diffuse_color=colour;m.surface_render_method='DITHERED';o.data.materials[i]=m

   if car=='volvo-240':
    if 'headlights_texture' in mn:kind='oj_head'
    elif 'tail_lights' in mn:kind='oj_tail'
   elif car=='volvo-v70':
    if n.startswith('headlamp_bulb'):kind='oj_head'
    elif n.startswith('xbrklt_'):kind='oj_tail'
   elif car=='subaru-outback-2002':
    if n.startswith(('pcube4_','pcube30_')):kind='oj_head'
    elif 'taillight' in n:kind='oj_tail'
   elif car=='golf-mk5-r32':
    if mn=='r32golf_lights' and n in ['lbeam','hbeam']:kind='oj_head'
    elif mn=='r32golf_lights_red':kind='oj_tail'
   elif car=='peugeot-406-coupe':
    if 'headlightglass' in assembly:kind='oj_head'
    elif 'tailightglass' in assembly:kind='oj_tail'
   elif car=='jetta-vento-mk5':
    if 'headlightglass' in assembly:kind='oj_head'
    elif 'taillightglass' in assembly:kind='oj_tail'
   if kind:o.data.materials[i]=lamp_material(m,kind)
   if car=='peugeot-405-glx' and '405_light' in mn:
    indexes={}
    for tag in ['oj_head','oj_tail']:
     indexes[tag]=len(o.data.materials);o.data.materials.append(lamp_material(m,tag))
    for poly in o.data.polygons:
     if poly.material_index==i:
      y=sum(o.data.vertices[v].co.y for v in poly.vertices)/len(poly.vertices)
      poly.material_index=indexes['oj_head' if y<0 else 'oj_tail']
  if car=='golf-mk5-r32' and any(m and m.name=='oj_glass' for m in o.data.materials):
   # KN5 windows are inward-facing two-sided sheets; native Car Glass culls backs.
   score=sum(p.normal.dot(p.center-Vector((0,0,.6)))*p.area for p in o.data.polygons)
   if score<0:o.data.flip_normals()
  if o.name.lower().startswith('wheel'):
   for i,m in enumerate(o.data.materials):
    if not m:continue
    key=m.as_pointer()
    if key not in wheel_material_cache:
     q=m.copy();q.name='rubber_'+m.name;wheel_material_cache[key]=q
    o.data.materials[i]=wheel_material_cache[key]
  if car=='subaru-outback-2002' and not o.name.lower().startswith('wheel'):
   uv=o.data.uv_layers.active
   for slot,m in enumerate(list(o.data.materials)):
    if not m or not m.use_nodes or not uv:continue
    image=next((n.image for n in m.node_tree.nodes if n.type=='TEX_IMAGE' and n.image),None)
    if not image:continue
    w,h=image.size
    if image.name not in image_cache:
     cached=np.empty(len(image.pixels),dtype=np.float32);image.pixels.foreach_get(cached);image_cache[image.name]=cached
    pixels=image_cache[image.name];paint_index=len(o.data.materials);o.data.materials.append(bpy.data.materials['oj_paint'])
    for poly in o.data.polygons:
     if poly.material_index!=slot:continue
     color=[0.,0.,0.]
     for li in poly.loop_indices:
      u,v=uv.data[li].uv;offset=4*(int(v*h)%h*w+int(u*w)%w)
      for j in range(3):color[j]+=pixels[offset+j]/len(poly.loop_indices)
     if color[1]>color[0]*1.13 and color[1]>color[2]*1.13 and color[1]>.025:poly.material_index=paint_index
  for p in o.data.polygons:p.use_smooth=True
 bpy.ops.export_scene.gltf(filepath=str(root/(car+'-prepared.glb')),export_format='GLB',export_yup=True)
 bpy.ops.wm.save_as_mainfile(filepath=str(root/(car+'-prepared.blend')))
 print('EXPORTED',car,flush=True)
