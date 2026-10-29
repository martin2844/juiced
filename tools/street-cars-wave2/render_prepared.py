import bpy,sys,math,json
from pathlib import Path
from mathutils import Vector
root=Path(sys.argv[sys.argv.index('--')+1])
for car in sys.argv[sys.argv.index('--')+2:]:
 bpy.ops.wm.open_mainfile(filepath=str(root/(car+'-prepared.blend')))
 meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.hide_render];pts=[o.matrix_world@Vector(v) for o in meshes for v in o.bound_box];lo=Vector([min(p[i] for p in pts) for i in range(3)]);hi=Vector([max(p[i] for p in pts) for i in range(3)]);center=(lo+hi)/2;size=max(hi-lo)
 scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=12;scene.render.resolution_x=640;scene.render.resolution_y=480;scene.render.resolution_percentage=100
 scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.2,.2,.2,1)
 for v,p in [((2,-2,3),850),((-2,1,2),650)]:
  bpy.ops.object.light_add(type='AREA',location=center+Vector(v)*size);o=bpy.context.object;o.data.energy=p*size*size;o.data.size=2*size;o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()
 bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam;cam.data.type='ORTHO';cam.data.ortho_scale=size*1.15;cam.data.clip_end=size*20;cam.data.clip_start=size*.0001
 for tag,v in [('front',(1,-1,.4)),('rear',(-1,1,.4))]:
  cam.location=center+Vector(v)*size*2;cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(root/(car+'-prepared-'+tag+'.png'));bpy.ops.render.render(write_still=True)
