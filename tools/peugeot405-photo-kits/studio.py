"""Build four fully assembled editable Blender scenes from the exported native 405.
Blender --background --python THIS -- REPO CANDIDATE OUTPUT
Uses the native mesh payloads, UVs and DXT1/BGRA textures; the native renderer
remains the authority for Juiced materials. Does not modify runtime payloads.
"""
import bpy,sys,re,struct,json,math
from pathlib import Path
import numpy as np
from mathutils import Vector
repo,base,out=map(Path,sys.argv[sys.argv.index('--')+1:]);out.mkdir(parents=True)
sys.path.insert(0,str(repo/'tools'));from vinyl_pack import mesh
folder=base/'assets/cars/peugeot_405_glx';textures=out/'textures';textures.mkdir()
bpy.ops.wm.read_factory_settings(use_empty=True);materials={}
def image_ddt(path):
 raw=path.read_bytes();fmt,w,h=struct.unpack_from('<3I',raw)
 if fmt==21:
  rgba=np.frombuffer(raw,dtype=np.uint8,count=w*h*4,offset=44).reshape(h,w,4)[:,:,[2,1,0,3]][::-1].astype(float)/255
  im=bpy.data.images.new(path.stem,width=w,height=h,alpha=True);im.pixels[:]=rgba.ravel();im.pack();return im
 assert fmt==0x31545844,(path,hex(fmt))
 # Blender's image loader supports DDS; retain an exact top-mip DXT1 payload.
 b=bytearray(128);b[:4]=b'DDS ';size=max(1,(w+3)//4)*max(1,(h+3)//4)*8
 struct.pack_into('<7I',b,4,124,0x81007,h,w,size,0,1);struct.pack_into('<8I',b,76,32,4,fmt,0,0,0,0,0);struct.pack_into('<I',b,108,0x1000)
 dest=textures/(path.stem+'.dds');dest.write_bytes(bytes(b)+raw[44:44+size]);im=bpy.data.images.load(str(dest));im.pack();return im
images={}
def mat(name):
 if name in materials:return materials[name]
 m=bpy.data.materials.new(name);m.use_nodes=True;n=m.node_tree.nodes.get('Principled BSDF');n.inputs['Roughness'].default_value=.33;colour=(.018,.023,.03,1)
 path=folder/'mesh/mats'/(name+'.pmd');text=path.read_text() if path.exists() else ''
 if name=='peugeot_405_glx_body':colour=(.035,.14,.32,1);n.inputs['Metallic'].default_value=.45;n.inputs['Roughness'].default_value=.23
 elif name=='w257_privacy_glass':colour=(.015,.021,.032,1);n.inputs['Metallic'].default_value=.65;n.inputs['Roughness'].default_value=.14
 else:
  match=re.search(r'^Colour\s*=\s*(\d+)\s+(\d+)\s+(\d+)',text,re.M)
  if match:colour=tuple((int(x)/255)**2.2 for x in match.groups())+(1,)
  match=re.search(r'^(?:Opaque Texture|Texture)\s*=\s*([^\s]+)\.ptd',text,re.M)
  if match:
   tex=folder/'textures'/(match[1]+'.ddt')
   if tex.exists():
    if tex.name not in images:images[tex.name]=image_ddt(tex)
    node=m.node_tree.nodes.new('ShaderNodeTexImage');node.image=images[tex.name];m.node_tree.links.new(node.outputs['Color'],n.inputs['Base Color'])
 if 'metal' in name:n.inputs['Metallic'].default_value=.75;n.inputs['Roughness'].default_value=.23
 n.inputs['Base Color'].default_value=colour;m.diffuse_color=colour;materials[name]=m;return m

def part_object(name,p,a,col,transform=None):
 pos=p['streams'][0,0].copy();normal=p['streams'][3,0].copy();tri=mesh.triangles(a,p)
 if transform is not None:
  pos=pos@transform[:3,:3].T+transform[:3,3];normal=normal@np.linalg.inv(transform[:3,:3]);normal/=np.linalg.norm(normal,axis=1)[:,None]
  if np.linalg.det(transform[:3,:3])<0:tri=tri[:,[0,2,1]]
 coords=np.c_[pos[:,0],-pos[:,2],pos[:,1]]/100;norm=np.c_[normal[:,0],-normal[:,2],normal[:,1]]
 m=bpy.data.meshes.new(name);m.from_pydata(coords.tolist(),[],tri.tolist());m.update();m.materials.append(mat(p['material']));uv=m.uv_layers.new()
 nativeuv=p['streams'].get((5,0),np.zeros((len(pos),2)))
 for loop in m.loops:
  u,v=nativeuv[loop.vertex_index];uv.data[loop.index].uv=(float(u),float(1-v))
 for poly in m.polygons:poly.use_smooth=True
 m.normals_split_custom_set_from_vertices(norm.tolist());o=bpy.data.objects.new(name,m);col.objects.link(o);return o
nodes={n:t for n,_,t in mesh.boh((folder/'models/peugeot_405_glx_stock.boh').read_bytes())};scenes=[]
for level,title in enumerate(['00_FACTORY','01_GUTMANN','02_STRATOS','03_BLUE_SHOWCASE']):
 scene=bpy.data.scenes.new(title);scenes.append(scene);c=bpy.data.collections.new(title+'_complete_native_car');scene.collection.children.link(c)
 names=['w257_chass','w257_bonnet' if level<2 else 'w257_vented_'+('paint' if level==2 else 'carbon')+'_bonnet']
 names += ['w257_'+('kit'+str(level)+'_' if level else '')+z for z in ['fbump','rbump','sskirt_l','sskirt_r']]
 if level:names+=['w257_wing'+str(level)]
 for name in names:
  a=mesh.read((folder/'mesh'/(name+'.d8m')).read_bytes())
  for i,p in enumerate(a['parts']):part_object(name+'_'+str(i),p,a,c)
 a=mesh.read((folder/'mesh/w257_wheel.d8m').read_bytes())
 for wheel in ['WHEEL_FL','WHEEL_FR','WHEEL_RL','WHEEL_RR']:
  t=nodes[wheel].copy()
  # The engine turns the right-hand wheels by180deg about native Y after load.
  # BOH alone contains the translation, so reproduce that actual display pose.
  if wheel.endswith(('FR','RR')):t[:3,:3]=t[:3,:3]@np.diag([-1,1,-1])
  for i,p in enumerate(a['parts']):part_object(wheel+'_'+str(i),p,a,c,t)
 env=bpy.data.collections.new(title+'_studio');scene.collection.children.link(env)
 world=bpy.data.worlds.new(title+'_world');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.08,.09,.11,1);world.node_tree.nodes['Background'].inputs[1].default_value=.35;scene.world=world
 for loc,power,size in [((3,-4,5),1400,4),((-4,-1,3),1100,3),((1,4,4),1800,3)]:
  d=bpy.data.lights.new(title+'_softbox','AREA');d.energy=power;d.shape='DISK';d.size=size;o=bpy.data.objects.new(d.name,d);env.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,.7))-o.location).to_track_quat('-Z','Y').to_euler()
 d=bpy.data.cameras.new(title+'_camera');cam=bpy.data.objects.new(d.name,d);env.objects.link(cam);cam.location=(5,-6,3.0);cam.rotation_euler=(Vector((0,0,.72))-cam.location).to_track_quat('-Z','Y').to_euler();d.type='ORTHO';d.ortho_scale=5.5;scene.camera=cam
 scene.render.engine='CYCLES';scene.cycles.samples=20;scene.render.resolution_x=1400;scene.render.resolution_y=950;scene.view_settings.view_transform='AgX'
 scene['purpose']='Editable assembly of exact exported native meshes; Juiced materials differ from this Blender approximation.'
bpy.context.window.scene=scenes[-1]
for area in bpy.context.screen.areas:
 if area.type=='VIEW_3D':
  area.spaces.active.region_3d.view_distance=6;area.spaces.active.region_3d.view_location=(0,0,.7);area.spaces.active.clip_end=100
note=bpy.data.texts.new('READ_ME');note.write('Peugeot405 custom shop: switch scenes FACTORY / GUTMANN / STRATOS / BLUE_SHOWCASE. Each scene contains the complete native mesh export with separate editable slot/material objects, original texture data packed into this file, all four wheels, and studio lighting. The separate Authoring.blend retains construction modifiers. Native CarView captures are the authority for game materials. Missing source interior is deliberately hidden by opaque reflective glazing. No physics changes.\n')
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Peugeot405-Photo-Kits.blend'))
print('Saved complete 405 Blender scenes with',len(images),'packed native textures.')
