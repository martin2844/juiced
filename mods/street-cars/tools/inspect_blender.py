import bpy,sys,json
from pathlib import Path
from mathutils import Vector
root=Path(sys.argv[sys.argv.index('--')+1])
for car in ['integra','bmw','levin']:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.gltf(filepath=str(root/(car+'-decoded.glb')))
 rows=[]
 for o in bpy.context.scene.objects:
  if o.type!='MESH':continue
  pts=[o.matrix_world@Vector(v) for v in o.bound_box]
  lo=[min(p[i] for p in pts) for i in range(3)];hi=[max(p[i] for p in pts) for i in range(3)]
  rows.append(dict(name=o.name,lo=lo,hi=hi,verts=len(o.data.vertices),materials=[m.name for m in o.data.materials]))
 (root/(car+'-blender-inspection.json')).write_text(json.dumps(rows,indent=2))
 bpy.ops.wm.save_as_mainfile(filepath=str(root/(car+'-source.blend')))
 print(car,rows[:5])
