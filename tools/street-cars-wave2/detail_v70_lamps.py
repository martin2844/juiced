"""Add low-poly reflector detail behind the V70 source's blank headlamp lenses.
Editable approximation constrained to the existing lamp housings, not an OEM CAD reproduction.
"""
import bpy,math,sys
from pathlib import Path
from mathutils import Vector
root=Path(sys.argv[sys.argv.index('--')+1]);path=root/'volvo-v70-repaired.blend'
bpy.ops.wm.open_mainfile(filepath=str(path))
for o in list(bpy.context.scene.objects):
 if o.name.startswith('headlamp_'):bpy.data.objects.remove(o,do_unlink=True)
def material(name,color,metal=0,alpha=1):
 m=bpy.data.materials.get(name) or bpy.data.materials.new(name);m.use_nodes=True;p=m.node_tree.nodes['Principled BSDF'];p.inputs['Base Color'].default_value=(*color,1);p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=.2 if metal else .45;p.inputs['Alpha'].default_value=alpha;m.diffuse_color=(*color,alpha)
 if alpha<1:m.surface_render_method='DITHERED'
 return m
lens=material('oj_lens',(.55,.65,.7),0,.10);chrome=material('reflector_chrome',(.55,.58,.6),.8);dark=material('lamp_housing',(.025,.025,.03));bulb=material('lamp_bulb',(.8,.8,.7),.25)
for o in list(bpy.context.scene.objects):
 if not o.name.startswith('xlight_'):continue
 points=[o.matrix_world@v.co for v in o.data.vertices];o.data.materials.clear();o.data.materials.append(lens)
 back=o.copy();back.data=o.data.copy();back.name='headlamp_housing';bpy.context.collection.objects.link(back);back.location.y+=.085;back.data.materials.clear();back.data.materials.append(dark)
 for side in [-1,1]:
  vs=[p for p in points if p.x*side>0];lo=Vector([min(p[i] for p in vs) for i in range(3)]);hi=Vector([max(p[i] for p in vs) for i in range(3)])
  for fraction in [.3,.72]:
   x=lo.x+(hi.x-lo.x)*fraction;y=(lo.y+hi.y)/2+.018;z=(lo.z+hi.z)/2;r=min((hi.z-lo.z)*.4,(hi.x-lo.x)*.20)
   vertices=[];faces=[]
   for ring in range(4):
    radius=r*(.12+.88*ring/3);depth=.045*(1-(radius/r)**2)
    for i in range(24):a=2*math.pi*i/24;vertices.append((x+radius*math.cos(a),y+depth,z+radius*math.sin(a)))
   for j in range(3):
    for i in range(24):k=(i+1)%24;faces.append((j*24+i,j*24+k,(j+1)*24+k,(j+1)*24+i))
   mesh=bpy.data.meshes.new('Reflector');mesh.from_pydata(vertices,[],faces);mesh.update();obj=bpy.data.objects.new('headlamp_reflector',mesh);bpy.context.collection.objects.link(obj);obj.data.materials.append(chrome)
   bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=6,radius=r*.19,location=(x,y+.02,z));obj=bpy.context.object;obj.name='headlamp_bulb';obj.data.materials.append(bulb)
bpy.ops.wm.save_as_mainfile(filepath=str(path))
