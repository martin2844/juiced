"""Import preserved sources into Blender and record eight-view baseline evidence."""
import bpy,sys,json,math,os
from pathlib import Path
from mathutils import Vector
root=Path(sys.argv[sys.argv.index('--')+1]); sources=Path(os.environ.get('JUICED_MODEL_SOURCES', str(Path(__file__).resolve().parents[3]/'model-sources/street-cars-2026-10-29')))
sys.path.insert(0,str(root/'kn5-tools'));import kn5_blender_tools;kn5_blender_tools.register()
for car in sys.argv[sys.argv.index('--')+2:]:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 if car=='golf-mk5-r32':
  files=list((root/'golf-original/content/cars/pb_golf_r32_07').glob('*.kn5'));src=max(files,key=lambda x:x.stat().st_size)
  bpy.ops.import_scene.assetto_corsa_kn5(filepath=str(src))
 else:
  src=root/(car+'-decoded.glb') if car=='peugeot-405-glx' else next((sources/car).glob('*.glb'))
  bpy.ops.import_scene.gltf(filepath=str(src))
 meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.hide_render]
 rows=[]
 for o in meshes:
  points=[o.matrix_world@Vector(p) for p in o.bound_box]
  rows.append(dict(name=o.name,verts=len(o.data.vertices),triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name if m else '' for m in o.data.materials],lo=[min(p[i] for p in points) for i in range(3)],hi=[max(p[i] for p in points) for i in range(3)]))
 (root/(car+'-inspection.json')).write_text(json.dumps(rows,indent=2))
 bpy.ops.wm.save_as_mainfile(filepath=str(root/(car+'-source.blend')))
 points=[o.matrix_world@Vector(p) for o in meshes for p in o.bound_box];lo=Vector([min(p[i] for p in points) for i in range(3)]);hi=Vector([max(p[i] for p in points) for i in range(3)]);center=(lo+hi)/2;size=max(hi-lo)
 scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL';scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.background_type='WORLD';scene.world=bpy.data.worlds.new('Inspection');scene.world.color=(.12,.12,.12)
 scene.render.resolution_x=640;scene.render.resolution_y=480;scene.render.resolution_percentage=100
 bpy.ops.object.camera_add();cam=bpy.context.object;cam.data.type='ORTHO';cam.data.ortho_scale=size*1.3;scene.camera=cam
 for i,tag in enumerate(['N','NE','E','SE','S','SW','W','NW']):
  a=i*math.pi/4;cam.location=center+Vector((math.sin(a),math.cos(a),.35))*size*2;cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(root/(car+'-'+tag+'.png'));bpy.ops.render.render(write_still=True)
 print('INSPECTED',car,len(meshes),sum(r['triangles'] for r in rows),flush=True)
