import bpy,sys,math,json,bmesh
from pathlib import Path
from mathutils import Vector
root=Path(sys.argv[sys.argv.index('--')+1])
for car in sys.argv[sys.argv.index('--')+2:]:
 if car not in ['peugeot-406-coupe','jetta-vento-mk5']:raise ValueError('This metre-space repair only supports the 406 and Jetta')
 bpy.ops.wm.open_mainfile(filepath=str(root/(car+'-cleaning.blend')))
 for c in list(bpy.data.collections):
  if c.name.startswith('KIT_'):
   for o in list(c.objects):bpy.data.objects.remove(o,do_unlink=True)
   bpy.data.collections.remove(c)
 # Bake source coordinates; these two sources use metres with z=0 at ground.
 for o in bpy.context.scene.objects:
  if o.type=='MESH':
   mat=o.matrix_world.copy();o.data=o.data.copy()
   for v in o.data.vertices:v.co=mat@v.co
   o.matrix_world.identity()
 removed=[];seen=set();duplicates=0
 for o in list(bpy.context.scene.objects):
  if o.type!='MESH':continue
  n=o.name.lower();vs=[v.co for v in o.data.vertices]
  if not vs:continue
  lo=Vector([min(p[i] for p in vs) for i in range(3)]);hi=Vector([max(p[i] for p in vs) for i in range(3)])
  remove=False
  if car=='jetta-vento-mk5':
   remove=(lo.z+hi.z)/2>1.48 or n in ['jettaa5_body-material','jettaa5_body-material.012','jettaa5_body-material.013','jettaa5_body-material.017']
  if car=='peugeot-406-coupe':remove=(lo.z>1.10 and hi.z-lo.z<.15 and len(vs)<1500)
  if remove:removed.append(o.name);bpy.data.objects.remove(o,do_unlink=True);continue
  bm=bmesh.new();bm.from_mesh(o.data);bad=[]
  for f in bm.faces:
   key=tuple(sorted(tuple(round(x,4) for x in v.co) for v in f.verts))
   if key in seen:bad.append(f)
   else:seen.add(key)
  duplicates+=len(bad);bmesh.ops.delete(bm,geom=bad,context='FACES');bm.to_mesh(o.data);bm.free()
 # Replace absent mounted wheels with editable five-spoke road wheels. This is custom geometry,
 # not claimed to reproduce an OEM wheel casting; final review required.
 if car in ['peugeot-406-coupe','jetta-vento-mk5']:
  wb=2.7 if car=='peugeot-406-coupe' else 2.578;track=1.52;radius=.315;rim=.205
  rubber=bpy.data.materials.get('oj_rubber') or bpy.data.materials.new('oj_rubber');rubber.diffuse_color=(.025,.025,.027,1);rubber.use_nodes=True;rubber.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.025,.025,.027,1)
  metal=bpy.data.materials.get('oj_metal') or bpy.data.materials.new('oj_metal');metal.diffuse_color=(.55,.57,.6,1);metal.use_nodes=True;metal.node_tree.nodes['Principled BSDF'].inputs['Metallic'].default_value=.8
  for axle,y in [('F',-1.305 if car=='peugeot-406-coupe' else -1.33),('R',1.395 if car=='peugeot-406-coupe' else -1.33+wb)]:
   for side,x in [('L',track/2),('R',-track/2)]:
    sign=1 if x>0 else -1;tag='wheel'+axle+side
    # Lathed sidewall and tread, with elliptical cross section.
    verts=[];faces=[];profile=[(-.105,rim),(-.105,radius*.85),(-.085,radius),(.085,radius),(.105,radius*.85),(.105,rim)]
    for dx,r in profile:
     for i in range(48):a=2*math.pi*i/48;verts.append((x+dx,y+r*math.sin(a),radius+r*math.cos(a)))
    for j in range(len(profile)-1):
     for i in range(48):k=(i+1)%48;faces.append((j*48+i,j*48+k,(j+1)*48+k,(j+1)*48+i))
    m=bpy.data.meshes.new(tag);m.from_pydata(verts,[],faces);m.update();o=bpy.data.objects.new(tag+'_tire',m);bpy.context.collection.objects.link(o);o.data.materials.append(rubber)
    bpy.ops.mesh.primitive_torus_add(major_segments=48,minor_segments=8,location=(x+sign*.095,y,radius),rotation=(0,math.pi/2,0),major_radius=rim*.94,minor_radius=.014);o=bpy.context.object;o.name=tag+'_rim';o.data.materials.append(metal)
    for i in range(5):
     a=i*2*math.pi/5;start=Vector((x+sign*.095,y,radius));end=Vector((x+sign*.10,y+math.sin(a)*rim*.9,radius+math.cos(a)*rim*.9));mid=(start+end)/2
     bpy.ops.mesh.primitive_cube_add(size=1,location=mid);o=bpy.context.object;o.name=tag+'_spoke';o.dimensions=(.035,.04,(end-start).length);o.rotation_euler=(end-start).to_track_quat('Z','X').to_euler();o.data.materials.append(metal)
     bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
     mod=o.modifiers.new('Spoke cast edge','BEVEL');mod.width=.006;mod.segments=2;bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.ops.mesh.primitive_cylinder_add(vertices=24,radius=.047,depth=.04,location=(x+sign*.10,y,radius),rotation=(0,math.pi/2,0));o=bpy.context.object;o.name=tag+'_hub';o.data.materials.append(metal)
 bpy.ops.wm.save_as_mainfile(filepath=str(root/(car+'-repaired.blend')))
 (root/(car+'-repairs.json')).write_text(json.dumps({'removed':removed,'duplicate_faces_removed':duplicates,'status':'requires final render and native validation'},indent=2))
 print(car,'duplicate faces',duplicates,'removed objects',len(removed),flush=True)
