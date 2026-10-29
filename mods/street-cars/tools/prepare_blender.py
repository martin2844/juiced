"""Preserve source UVs/materials; remove BMW presentation floor and optimize Levin."""
import bpy,sys,math,json
from pathlib import Path
from mathutils import Vector
root=Path(sys.argv[sys.argv.index('--')+1])
for car in (sys.argv[sys.argv.index('--')+2:] or ['integra','bmw','levin']):
 bpy.ops.wm.open_mainfile(filepath=str(root/(car+'-source.blend')))
 for o in list(bpy.data.objects):
  if o.type=='MESH' and any(m.name=='Scene' for m in o.data.materials):bpy.data.objects.remove(o,do_unlink=True)
 if car=='levin':
  for o in bpy.context.scene.objects:
   if o.type!='MESH' or len(o.data.polygons)<150:continue
   bpy.context.view_layer.objects.active=o; o.data=o.data.copy()
   mod=o.modifiers.new('RP5 mesh budget','DECIMATE');mod.ratio=.25 if any(m.name.startswith(('int','INT','floor')) for m in o.data.materials) else .75;mod.use_collapse_triangulate=True
   bpy.ops.object.modifier_apply(modifier=mod.name)
   if any(m.name=='body' for m in o.data.materials):
    normal=o.modifiers.new('Body panel normals','WEIGHTED_NORMAL');normal.keep_sharp=True;normal.weight=50;bpy.ops.object.modifier_apply(modifier=normal.name)
 bpy.ops.export_scene.gltf(filepath=str(root/(car+'-prepared.glb')),export_format='GLB',export_yup=True)
 bpy.ops.wm.save_as_mainfile(filepath=str(root/(car+'-prepared.blend')))
 meshes=[o for o in bpy.context.scene.objects if o.type=='MESH'];pts=[o.matrix_world@Vector(v) for o in meshes for v in o.bound_box]
 lo=Vector([min(p[i] for p in pts) for i in range(3)]);hi=Vector([max(p[i] for p in pts) for i in range(3)]);center=(lo+hi)/2;size=max(hi-lo)
 scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=16
 scene.render.resolution_x=960;scene.render.resolution_y=640;scene.render.resolution_percentage=100
 scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.28,.28,.28,1)
 for loc,power,scale in [((2,-3,4),1000,3),((-3,1,3),700,3),((0,3,4),900,2)]:
  bpy.ops.object.light_add(type='AREA',location=center+Vector(loc)*size);light=bpy.context.object;light.data.energy=power*size*size;light.data.shape='DISK';light.data.size=scale*size;light.rotation_euler=(center-light.location).to_track_quat('-Z','Y').to_euler()
 bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam;cam.data.type='ORTHO';cam.data.ortho_scale=size*1.35
 for tag,v in [('front',(1,-1,.5)),('rear',(-1,1,.5)),('side',(1,0,.2)),('top',(0,0,1))]:
  cam.location=center+Vector(v)*size*2;cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(root/(car+'-'+tag+'.png'));bpy.ops.render.render(write_still=True)
