"""Reduce source mesh density for handhelds, retaining editable pre-optimization scenes.
Transfer source corner normals after reduction to keep the original surface shading.
"""
import bpy,sys,json,shutil
from pathlib import Path
root=Path(sys.argv[sys.argv.index('--')+1])
limits={'peugeot-406-coupe':45000,'jetta-vento-mk5':50000,'golf-mk5-r32':55000,'volvo-v70':35000,'volvo-240':25000}
def triangles(o):return sum(len(p.vertices)-2 for p in o.data.polygons)
for car in sys.argv[sys.argv.index('--')+2:]:
 path=root/(car+'-repaired.blend');bpy.ops.wm.open_mainfile(filepath=str(path))
 if bpy.context.scene.get('handheld_optimization_version')==1:continue
 backup=root/(car+'-pre-optimization.blend')
 if not backup.exists():shutil.copy2(path,backup)
 objects=[o for o in bpy.context.scene.objects if o.type=='MESH'];wheel=[o for o in objects if o.name.lower().startswith('wheel')];body=[o for o in objects if o not in wheel]
 before=sum(map(triangles,objects));body_total=sum(map(triangles,body));wheel_total=sum(map(triangles,wheel));bodyratio=min(1,limits.get(car,body_total)/max(body_total,1));wheelratio=min(1,8000/max(wheel_total,1));reduced=[]
 for o in objects:
  count=triangles(o);ratio=wheelratio if o in wheel else bodyratio
  if count<100 or ratio>=.98:continue
  if any(m and 'glass' in m.name.lower() for m in o.data.materials):continue
  reference=o.copy();reference.data=o.data.copy();reference.name='NORMAL_REFERENCE';bpy.context.collection.objects.link(reference)
  bpy.context.view_layer.objects.active=o
  mod=o.modifiers.new('Handheld mesh budget','DECIMATE');mod.ratio=max(ratio,64/count);mod.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=mod.name)
  mod=o.modifiers.new('Retain source surface normals','DATA_TRANSFER');mod.object=reference;mod.use_loop_data=True;mod.data_types_loops={'CUSTOM_NORMAL'};mod.loop_mapping='POLYINTERP_NEAREST';bpy.ops.object.modifier_apply(modifier=mod.name)
  bpy.data.objects.remove(reference,do_unlink=True);reduced.append({'object':o.name,'before':count,'after':triangles(o)})
 bpy.context.scene['handheld_optimization_version']=1
 bpy.ops.wm.save_as_mainfile(filepath=str(path))
 report={'before_triangles':before,'after_triangles':sum(map(triangles,objects)),'objects':reduced,'status':'requires visual and native validation'}
 (root/(car+'-optimization.json')).write_text(json.dumps(report,indent=2));print(car,before,'->',report['after_triangles'],flush=True)
