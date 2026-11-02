import bpy,sys
from pathlib import Path
from mathutils import Vector
path,out=map(Path,sys.argv[sys.argv.index('--')+1:]);bpy.ops.wm.open_mainfile(filepath=str(path));out.mkdir(parents=True)
for scene in [s for s in bpy.data.scenes if s.name.startswith(('00_','01_','02_','03_'))]:
 mesh=bpy.data.meshes.new('Studio_floor');mesh.from_pydata([(-10,-10,-.008),(10,-10,-.008),(10,10,-.008),(-10,10,-.008)],[],[(0,1,2,3)]);o=bpy.data.objects.new('Studio floor',mesh);scene.collection.objects.link(o);m=bpy.data.materials.get('StudioFloor')
 if m is None:
  m=bpy.data.materials.new('StudioFloor');m.diffuse_color=(.055,.06,.07,1);m.use_nodes=True;n=m.node_tree.nodes.get('Principled BSDF');n.inputs['Base Color'].default_value=m.diffuse_color;n.inputs['Roughness'].default_value=.32
 mesh.materials.append(m)
# Open in the complete Touring scene with every mesh and original texture packed.
scene=bpy.data.scenes['03_TOURING'];bpy.context.window.scene=scene
bpy.ops.wm.save_as_mainfile(filepath=str(path))
scene.render.resolution_x=1100;scene.render.resolution_y=750;scene.cycles.samples=12;scene.render.filepath=str(out/'Blender-Touring-front.png');bpy.ops.render.render(write_still=True)
