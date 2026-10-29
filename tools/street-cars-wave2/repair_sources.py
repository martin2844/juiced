"""Non-destructive source cleanup; originals remain in separate .blend files."""
import bpy,sys,json,math,re
from pathlib import Path
from mathutils import Vector,Matrix
root=Path(sys.argv[sys.argv.index('--')+1])
def mat(name,color,metal=0):
 m=bpy.data.materials.new(name);m.use_nodes=True;m.diffuse_color=(*color,1);p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=.32 if metal else .45;return m
def bounds(o):
 v=[o.matrix_world@Vector(p) for p in o.bound_box];return Vector([min(p[i] for p in v) for i in range(3)]),Vector([max(p[i] for p in v) for i in range(3)])
def replace(o,m):o.data.materials.clear();o.data.materials.append(m)
for car in sys.argv[sys.argv.index('--')+2:]:
 bpy.ops.wm.open_mainfile(filepath=str(root/(car+'-source.blend')))
 paint=mat('oj_paint',(.27,.37,.55),.35);glass=mat('oj_glass',(.07,.1,.13));rubber=mat('oj_rubber',(.025,.025,.027));black=mat('oj_trim',(.035,.035,.04));silver=mat('oj_metal',(.55,.57,.60),.8);red=mat('oj_tail',(.48,.015,.012));head=mat('oj_head',(.65,.7,.72),.5)
 meshes=[o for o in bpy.context.scene.objects if o.type=='MESH'];removed=[];changes=[]
 for o in meshes:
  # Bake parent transforms without losing placement before sorting assemblies.
  assembly=o.parent.name.lower() if o.parent else '';o['source_assembly']=assembly
  world=o.matrix_world.copy();o.parent=None;o.matrix_world=world
  lo,hi=bounds(o);n=o.name.lower();m=' '.join(x.name for x in o.data.materials if x).lower()
  remove=False
  if car=='peugeot-405-glx':
   remove=any(x in m for x in ['collision','shadow'])
   if 'body' in m:replace(o,paint)
   if 'wheel' in n:
    tag=re.search('wheel_([fr][lr])',n).group(1).upper();o.name='wheel'+tag
  elif car=='volvo-240':
   # Preserve the complete Turbo assembly at x≈319cm, exclude the GL variant.
   remove=(lo.x+hi.x)/2<180
   if not remove:
    o.location.x-=319.5433
    if 'body2' in m:replace(o,paint)
    if n.startswith(('cylinder','circle')):
     l,h=bounds(o);tag=('F' if (l.y+h.y)/2<0 else 'R')+('L' if (l.x+h.x)/2>0 else 'R');o.name='wheel'+tag+'_'+o.name
     if n.startswith('cylinder'):replace(o,rubber)
  elif car=='volvo-v70':
   o.matrix_world=Matrix.Rotation(-math.pi/2,4,'Z')@o.matrix_world
   if 'green' in m:replace(o,paint)
   if n.startswith('w'):
    # Rear pair shares a mesh; split disconnected parts later for native wheel grouping.
    o.name='wheels_'+o.name
  elif car=='golf-mk5-r32':
   if 'carpaint' in m:replace(o,paint)
   if n.startswith(('tyre_','rim_','rimbadge_','brakedisc_','caliper_')) or any(x in n for x in ['rim','tyre']):
    l,h=bounds(o);tag=('F' if (l.y+h.y)/2<0 else 'R')+('L' if (l.x+h.x)/2>0 else 'R');o.name='wheel'+tag+'_'+o.name
   if m=='tire':replace(o,rubber)
  elif car=='subaru-outback-2002':
   if any(x in n for x in ['window','windscreen']):replace(o,glass)
   if any(x in n for x in ['hood_paint','trunk_paint']):replace(o,paint)
   if n.startswith(('polysurface56_','polysurface57_','polysurface58_','polysurface59_','pcylinder1_','pcylinder2_','pcylinder3_','pcylinder4_')):
    tag=('F' if (lo.y+hi.y)/2<.1 else 'R')+('L' if (lo.x+hi.x)/2>0 else 'R');o.name='wheel'+tag+'_'+o.name
  elif car=='peugeot-406-coupe':
   remove=lo.z>1.36 or n.startswith('peugeot_406_black-material.0') and n.rsplit('.',1)[-1] in ['022','023','048','049','065'] or n=='peugeot_406_black_003-material' or n=='peugeot_406_potolok-material.004'
   remove |= assembly in ['peugeot_406cop_windshield_dmg','peugeot_406cop_spoilerm','peugeot_406cop_spoiler1','zzr.001','peugeot_406cop_hood1','peugeot_406cop_hood2','peugeot_406cop_hood001','peugeot_406cop_bumper_fz','peugeot_406cop_bumper_f1','peugeot_406cop_fogsz','peugeot_406cop_headlight_rz']
   remove |= bool(re.search(r'_(?:v[1-4]|rv[1-4])(?:_|$)',assembly)) or assembly in ['peugeot_406cop_cluster2','peugeot_406cop_cluster3','peugeot_406cop_cluster4','peugeot_406cop_cluster5','peugeot_406_dash.001','peugeot_406_dash.002']
   if m=='peugeot_406_color':replace(o,paint)
   if 'black' in m or 'obivka' in m or 'cloth' in m:pass # retain source upholstery/trim atlas
   elif 'chrome' in m:pass # retain source metal atlas
  elif car=='jetta-vento-mk5':
   remove=lo.z>1.45 or any(x in n for x in ['taxi','backpack','heineken','floralys','dashcam','cruciulita','cross-material','phone','samsung','sticker','pennant'])
   remove |= any(x in assembly for x in ['_gli','_r36','_race','_wide','_taxi','_sign','_dmg','_leather','_diesel','_adcarrier','_sticker','_pennant','_heineken','_floralys','_dashcam','_cross','_backpack','_phone','samsung','_steelwheel'])
   remove |= '_fl' in assembly and any(t in assembly for t in ['radio','gauge','needle','dash','speedometer','steer'])
   remove |= assembly.endswith(('_p','_us','_us_l','_fl_basic','_fl_d','_a','_int','_black','_red','_red2','_red_l','_red_r'))
   remove |= assembly.startswith(('jettaa5_wheel','jettaa5_hubcap','jettaa5_steelwheel'))
   remove |= assembly in ['jettaa5_engine_i5','jettaa5_intake_i5','jettaa5_turbo_i4','jettaa5_shifter_knob_m5','jettaa5_radio_basic','jettaa5_radio_fl_mid','jettaa5_bumper_r_p','jettaa5_bumper_f_bottomgrille_b','jettaa5_exhaustpipe_double','jettaa5_bumper_r_diffuser_us']
   remove |= bool(re.search(r'(?:headlight|trunklights|taillight).*?(?:2|3|norm|vwlogo)',assembly))
   if 'body-material' in n:replace(o,paint)
   elif any(x in n for x in ['glass2','glass-material']):replace(o,glass)
   elif any(x in n for x in ['red_dark','rearfog','redlight']):replace(o,red)
   elif any(x in n for x in ['lowbeam','highbeam','parkinglight','glass_lights']):replace(o,head)
   elif any(x in m for x in ['chrome','alu','steelwheel']):pass # retain source trim detail
   elif m.startswith('adcarrier'):replace(o,black)
   if n.startswith(('steelwheel','jettaa5_steelwheel','jettaa5_wheel')):remove=True # Unmounted wheel prototype: preserve in source, replace with fitted assembly later.
  if ('glass' in m or any(t in assembly for t in ['windshield','backlight','doorglass'])) and car not in ['subaru-outback-2002'] and not any(t in assembly for t in ['headlight','taillight','tailight']) and not assembly.endswith('_frame'):replace(o,glass)
  if remove:removed.append(o.name);bpy.data.objects.remove(o,do_unlink=True);continue
  # Collapse only dense pieces; preserve body boundaries and source normals.
  if len(o.data.polygons)>2500 and not any(m and m.name in ['oj_paint','oj_glass','oj_head','oj_tail'] for m in o.data.materials):
   bpy.context.view_layer.objects.active=o;o.data=o.data.copy();mod=o.modifiers.new('Mobile interior/mesh budget','DECIMATE');mod.ratio=min(1,2500/len(o.data.polygons));mod.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=mod.name)
 for o in bpy.context.scene.objects:
  if o.type=='MESH':
   o.select_set(True)
   for p in o.data.polygons:p.use_smooth=True
 bpy.ops.wm.save_as_mainfile(filepath=str(root/(car+'-cleaning.blend')))
 (root/(car+'-cleanup.json')).write_text(json.dumps({'removed':removed,'status':'intermediate: wheels, UV paint masks and kits not yet approved'},indent=2))
 print('CLEANING',car,len(removed),'removed',flush=True)
