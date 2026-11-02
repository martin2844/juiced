"""Finish and render the complete, editable native-mesh study in Blender."""
import bpy,sys
from pathlib import Path
path,out=map(Path,sys.argv[sys.argv.index('--')+1:]);bpy.ops.wm.open_mainfile(filepath=str(path));out.mkdir(parents=True)
for scene in [s for s in bpy.data.scenes if s.name.startswith(('00_','01_','02_','03_'))]:
 m=bpy.data.meshes.new('Studio_floor');m.from_pydata([(-10,-10,-.008),(10,-10,-.008),(10,10,-.008),(-10,10,-.008)],[],[(0,1,2,3)]);o=bpy.data.objects.new('Studio floor',m);scene.collection.objects.link(o)
 mat=bpy.data.materials.get('StudioFloor')
 if mat is None:
  mat=bpy.data.materials.new('StudioFloor');mat.use_nodes=True;n=mat.node_tree.nodes.get('Principled BSDF');n.inputs['Base Color'].default_value=(.045,.05,.06,1);n.inputs['Roughness'].default_value=.4
 m.materials.append(mat)
bpy.context.window.scene=bpy.data.scenes['01_GUTMANN']
bpy.ops.wm.save_as_mainfile(filepath=str(path))
for name in ['01_GUTMANN','02_STRATOS','03_BLUE_SHOWCASE']:
 scene=bpy.data.scenes[name];scene.render.resolution_x=1100;scene.render.resolution_y=750;scene.cycles.samples=12;scene.render.filepath=str(out/(name+'.png'));bpy.ops.render.render(write_still=True,scene=name)
