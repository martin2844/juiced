"""Reflect the inspected native kit3 material assignments in editable Blender/GLB."""
from pathlib import Path
import bpy,json,sys,shutil,hashlib

study,receipt,out=map(Path,sys.argv[sys.argv.index('--')+1:])
if out.exists():raise RuntimeError('Fresh output required')
out.mkdir(parents=True);data=json.loads(receipt.read_text())
bpy.ops.wm.open_mainfile(filepath=str(study/'outback_2002-morphed-study.blend'))
col=bpy.data.collections['KIT_3'];paint=bpy.data.materials['outback_2002_body']
for row in data['results']:
    if '/outback_2002_ui/' in row['file']:continue
    zone=Path(row['file']).stem.removeprefix('w261_kit3_')
    for part in row['paint_faces']:
        name=zone+'_'+str(part['source_part'])
        obj=next(o for o in col.objects if o.name.split('.')[0]==name)
        assert len(obj.data.materials)==1
        obj.data.materials.append(paint)
        for face in part['triangle_indices']:obj.data.polygons[face].material_index=1
for level in [2,3]:
    c=bpy.data.collections['KIT_'+str(level)];c.hide_viewport=False;c.hide_render=False
    bpy.ops.object.select_all(action='DESELECT')
    for o in c.objects:o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(out/('outback_2002-kit'+str(level)+'.glb')),export_format='GLB',use_selection=True,export_yup=True)
    c.hide_viewport=True;c.hide_render=True
bpy.ops.wm.save_as_mainfile(filepath=str(out/'outback_2002-kits.blend'))
shutil.copy2(study/'native-vertex-study.json',out/'native-vertex-study.json')
shutil.copy2(receipt,out/'coating.json')
(out/'source-receipt.json').write_text(json.dumps({'study':str(study),'coating_receipt':str(receipt),'coating_sha256':hashlib.sha256(receipt.read_bytes()).hexdigest(),'status':'Native inspected kit04 source; local delivery verification required'},indent=2)+'\n')
print('Editable Blender/GLB now match inspected kit04 geometry and material assignments.')
