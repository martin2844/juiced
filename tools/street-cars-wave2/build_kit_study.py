"""Fit editable aero studies to the actual stock shell using Blender ray intersections.
Exports are candidates only: visual/native QA is required before packaging.
"""
import bpy,sys,json,math,struct
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path(sys.argv[sys.argv.index('--')+1])
def validate_exported_fins(path):
 # Check the exported artifact, not Blender's evaluated preview: missing applied
 # modifiers previously left each diffuser fin as a one-sided, zero-thickness quad.
 data=path.read_bytes();length=struct.unpack_from('<I',data,12)[0];gltf=json.loads(data[20:20+length]);fins=0
 for node in gltf['nodes']:
  if not node.get('name','').startswith('REAR_DIFFUSER_FIN'):continue
  fins+=1
  for part in gltf['meshes'][node['mesh']]['primitives']:
   position=gltf['accessors'][part['attributes']['POSITION']]
   indices=gltf['accessors'][part['indices']]
   if position['max'][0]-position['min'][0]<.001 or indices['count']<=6:
    raise RuntimeError('Exported diffuser fin lacks physical thickness: '+node['name'])
 if fins!=5:raise RuntimeError('Expected five exported diffuser fins')

def mesh(name,vertices,faces,material,collection):
 m=bpy.data.meshes.new(name);m.from_pydata(vertices,[],faces);m.update();o=bpy.data.objects.new(name,m);collection.objects.link(o);m.materials.append(material)
 if len(faces)==1:
  mod=o.modifiers.new('Physical thickness','SOLIDIFY');mod.thickness=.008
 mod=o.modifiers.new('Edge radius','BEVEL');mod.width=.003;mod.segments=2
 return o

def ribbon(name,sections,material,collection):
 verts=[v for row in sections for v in row];faces=[]
 for i in range(len(sections)-1):
  for j in range(4):faces.append((4*i+j,4*(i+1)+j,4*(i+1)+(j+1)%4,4*i+(j+1)%4))
 faces.extend([(3,2,1,0),tuple(range(len(verts)-4,len(verts)))])
 return mesh(name,verts,faces,material,collection)

for car in sys.argv[sys.argv.index('--')+2:]:
 bpy.ops.wm.open_mainfile(filepath=str(root/(car+'-prepared.blend')))
 objects=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.lower().startswith('wheel')]
 vertices=[];faces=[];paintfaces=[]
 for o in objects:
  start=len(vertices);vertices += [o.matrix_world@v.co for v in o.data.vertices]
  for p in o.data.polygons:
   face=[start+i for i in p.vertices];faces.append(face)
   if o.data.materials and o.data.materials[p.material_index] and o.data.materials[p.material_index].name=='oj_paint':paintfaces.append(face)
 lo=Vector([min(p[i] for p in vertices) for i in range(3)]);hi=Vector([max(p[i] for p in vertices) for i in range(3)])
 tree=BVHTree.FromPolygons(vertices,faces);painttree=BVHTree.FromPolygons(vertices,paintfaces)
 paint=bpy.data.materials['oj_paint'];trim=bpy.data.materials.get('oj_trim') or bpy.data.materials.new('oj_trim');trim.diffuse_color=(.025,.025,.028,1);trim.use_nodes=True;trim.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.025,.025,.028,1)
 # Tyre bounds define the opening which skirts must leave unobstructed.
 wheelpoints=[o.matrix_world@v.co for o in bpy.context.scene.objects if o.type=='MESH' and o.name.lower().startswith('wheel') for v in o.data.vertices]
 front=[p for p in wheelpoints if p.y<0];rear=[p for p in wheelpoints if p.y>0]
 skirt_start=max(p.y for p in front)+.04;skirt_end=min(p.y for p in rear)-.04
 reports=[]
 for level in range(1,4):
  col=bpy.data.collections.new('KIT_'+str(level));bpy.context.scene.collection.children.link(col)
  for tag,sign in [('FRONT',-1),('REAR',1)]:
   end=lo.y if sign<0 else hi.y
   candidates=[p for p in vertices if p.y*sign>abs(end)*.76 and .18<p.z<.5]
   width=min(max(abs(p.x) for p in candidates),max(abs(lo.x),hi.x)*.92)
   samples=[]
   for i in range(33):
    x=-width+2*width*i/32;hit=None
    for step in range(35):
     z=.16+step*.01
     p,normal,index,dist=tree.ray_cast(Vector((x,end+sign,z)),Vector((0,-sign,0)),2)
     if p is not None and p.y*sign>abs(end)*.76:
      hit=p;break
    if hit is None:continue
    samples.append(hit)
   if len(samples)<12:raise RuntimeError('Insufficient bumper contact '+car+' '+tag)
   sections=[]
   for p in samples:
    x,y,z=p;shape=max(0,1-(abs(x)/width)**6)
    extension=[.025,.09,.11][level-1]*shape;drop=[.025,.06,.02][level-1]*shape
    # Root lies inside the original shell; all emitted surfaces share this mount.
    sections.append([(x,y-sign*.025,z+.035),(x,y+sign*extension,z+.01),(x,y+sign*extension,z-drop),(x,y-sign*.025,z-drop)])
   ribbon(tag+'_VALANCE_'+str(level),sections,trim if level==3 else paint,col)
   if level==3 and tag=='REAR':
    for x in [-.42,-.21,0,.21,.42]:
     p=min(samples,key=lambda p:abs(p.x-x));y=p.y;z=p.z
     mesh('REAR_DIFFUSER_FIN',[(x,y-.12,z+.01),(x,y+.035,z+.01),(x,y+.035,z-.07),(x,y-.12,z-.025)],[(0,1,2,3)],trim,col)
   reports.append({'part':tag,'level':level,'mount_samples':len(samples)})
  for side in [-1,1]:
   sections=[]
   for i in range(25):
    y=skirt_start+(skirt_end-skirt_start)*i/24;hit=None
    for step in range(40):
     z=.12+step*.01
     p,normal,index,dist=(tree if car=='subaru-outback-2002' else painttree).ray_cast(Vector((side*(max(abs(lo.x),hi.x)+1),y,z)),Vector((-side,0,0)),2)
     if p is not None and abs(p.x)>.55:
      hit=p;break
    if hit is None:continue
    x,y,z=hit;ext=[.018,.055,.09][level-1]*side;drop=[.02,.045,.025][level-1]
    sections.append([(x-side*.02,y,z+.03),(x+ext,y,z+.005),(x+ext,y,z-drop),(x-side*.02,y,z-drop)])
   if len(sections)<12:raise RuntimeError('Insufficient skirt contact '+car)
   ribbon('SKIRT_'+str(side)+'_'+str(level),sections,trim if level==3 else paint,col)
  bpy.ops.object.select_all(action='DESELECT')
  for o in col.objects:o.select_set(True)
  path=root/(car+'-kit'+str(level)+'.glb')
  bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_apply=True)
  if level==3:validate_exported_fins(path)
  col.hide_render=True;col.hide_viewport=True
 bpy.ops.wm.save_as_mainfile(filepath=str(root/(car+'-kit-study.blend')))
 (root/(car+'-kit-study.json')).write_text(json.dumps({'status':'prototype-exported-not-approved','parts':reports},indent=2))
 print('KIT STUDY',car,flush=True)
