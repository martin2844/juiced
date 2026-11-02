"""Author a 405-only Juiced customization study in Blender and export native D8M.
Blender --background --python-exit-code 1 --python THIS -- REPO BASELINE FRESH_OUT
Native coordinates are centimetres, X across, Y up, +Z forward. Blender uses metres.
Three deliberately different aftermarket kits; no claim of exact Mi16 homologation.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math,re,shutil,struct,sys
import bpy,bmesh
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
p=argparse.ArgumentParser(description=__doc__);p.add_argument('repo',type=Path);p.add_argument('base',type=Path);p.add_argument('out',type=Path)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);repo,base,out=a.repo,a.base,a.out
assert not out.exists(),out
out.mkdir(parents=True);candidate=out/'street-cars-wave2';shutil.copytree(base,candidate)
sys.path[:0]=[str(repo/'tools'),str(repo/'mods/ferrari/tools')]
from vinyl_pack import mesh
from import_ferrari import mesh_bytes,bounds
model='peugeot_405_glx';prefix='w257';body=model+'_body';carbon='w257_carbon';trim='p405_trim';metal='p405_metal';grille='p405_grille';lens='p405_fog';rubber='p405_rubber'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
folder=base/'assets/cars'/model/'mesh';parsed={z:mesh.read((folder/(prefix+'_'+z+'.d8m')).read_bytes()) for z in ['chass','fbump','rbump','bonnet','sskirt_l','sskirt_r']}
source={str(p.relative_to(base)):sha(p) for p in sorted(base.rglob('*')) if p.is_file()}
bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC';materials={};collections={};rows=[]
def col(name):
 c=bpy.data.collections.new(name);scene.collection.children.link(c);collections[name]=c;return c
reference=col('00_ORIGINAL_r17');current=None
colours={body:(.035,.16,.36,1),carbon:(.025,.028,.032,1),trim:(.019,.021,.025,1),metal:(.52,.55,.58,1),grille:(.04,.045,.05,1),lens:(.65,.72,.8,1),rubber:(.009,.01,.012,1)}
def material(name):
 if name not in materials:
  m=bpy.data.materials.new(name);m.diffuse_color=colours.get(name,(.045,.047,.05,1));m.use_nodes=True;n=m.node_tree.nodes.get('Principled BSDF');n.inputs['Base Color'].default_value=m.diffuse_color;n.inputs['Metallic'].default_value=.7 if name==metal else .35 if name==body else .1;n.inputs['Roughness'].default_value=.23 if name==body else .4;materials[name]=m
 return materials[name]
def cv(v):return (float(v[0])/100,-float(v[2])/100,float(v[1])/100)
def obj(name,verts,faces,mat,collection=None,bevel=0):
 m=bpy.data.meshes.new(name);m.from_pydata([cv(v) for v in verts],[],faces);m.update();o=bpy.data.objects.new(name,m);(collection or current).objects.link(o);m.materials.append(material(mat))
 if bevel:
  b=o.modifiers.new('Finished edges','BEVEL');b.width=bevel/100;b.segments=2
  b=o.modifiers.new('Weighted highlights','WEIGHTED_NORMAL');b.keep_sharp=True
 uv=m.uv_layers.new(name='Native planar UV')
 for loop in m.loops:
  v=verts[loop.vertex_index];uv.data[loop.index].uv=((v[0]+v[2])/11.31,(v[2]-v[0])/11.31) if mat==carbon else (v[0]/25,v[1]/25)
 return o
def recalc(o):
 bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
 return o
def box(name,lo,hi,mat,bevel=.25):
 x,y,z=lo;X,Y,Z=hi
 o=obj(name,[(x,y,z),(X,y,z),(X,Y,z),(x,Y,z),(x,y,Z),(X,y,Z),(X,Y,Z),(x,Y,Z)],[(0,3,2,1),(4,5,6,7),(0,1,5,4),(3,7,6,2),(0,4,7,3),(1,2,6,5)],mat,bevel=bevel);return recalc(o)
def loft(name,sections,mat,bevel=.2):
 n=len(sections[0]);verts=[v for s in sections for v in s];faces=[]
 for i in range(len(sections)-1):
  for j in range(n):faces.append((i*n+j,(i+1)*n+j,(i+1)*n+(j+1)%n,i*n+(j+1)%n))
 faces.extend([tuple(range(n-1,-1,-1)),tuple(range(len(verts)-n,len(verts)))])
 return recalc(obj(name,verts,faces,mat,bevel=bevel))
def as_parts(o):
 deps=bpy.context.evaluated_depsgraph_get();e=o.evaluated_get(deps);m=e.to_mesh();m.calc_loop_triangles();verts=[];norm=[];uv=[];tri=[]
 for t in m.loop_triangles:
  start=len(verts)
  for li in t.loops:
   v=o.matrix_world@m.vertices[m.loops[li].vertex_index].co;n=o.matrix_world.to_3x3()@m.corner_normals[li].vector;n.normalize();verts.append((v.x*100,v.z*100,-v.y*100));norm.append((n.x,n.z,-n.y));u=m.uv_layers.active.data[li].uv if m.uv_layers.active else (0,0);uv.append(tuple(u))
  tri.append((start,start+1,start+2))
 e.to_mesh_clear()
 pos=np.asarray(verts);tri=np.asarray(tri);n=np.asarray(norm);v=pos[tri];valid=np.linalg.norm(np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]),axis=1)>1e-7
 # Bevel triangulation may emit microscopic zero-area corner remnants.
 tri=tri[valid];assert len(tri)>0,(o.name,'no non-degenerate surface')
 assert np.isfinite(pos).all() and np.isfinite(n).all()
 return dict(mat=o.data.materials[0].name,pos=pos,norm=n,uv=np.asarray(uv),tri=tri)
def original_object(zone,part,i,collection):
 return obj(zone+'_'+str(i),part['streams'][0,0],mesh.triangles(parsed[zone],part).tolist(),part['material'],collection)
for zone,a0 in parsed.items():
 for i,part in enumerate(a0['parts']):original_object(zone,part,i,reference)
# Measured native exterior and hood projection, not donor scaling.
verts=[];faces=[]
for zone in ['chass','fbump','rbump','bonnet']:
 for part in parsed[zone]['parts']:
  first=len(verts);verts.extend(part['streams'][0,0]);faces.extend((mesh.triangles(parsed[zone],part)+first).tolist())
exterior=BVHTree.FromPolygons(verts,faces,all_triangles=True)
hp=parsed['bonnet']['parts'][0];hoodtree=BVHTree.FromPolygons(hp['streams'][0,0].tolist(),mesh.triangles(parsed['bonnet'],hp).tolist(),all_triangles=True)
def surface(x,z):
 hit=exterior.ray_cast(Vector((x,220,z)),Vector((0,-1,0)),220)[0]
 assert hit is not None and hit.y>70,('missing rear-deck surface',x,z)
 return hit.y
def hood_y(x,z):
 hit=hoodtree.ray_cast(Vector((x,200,z)),Vector((0,-1,0)),200)[0]
 assert hit is not None,('missing hood',x,z)
 return hit.y
# Lossless retained pieces: stock panels, textures and native vertex records.
def encode_retained(zone,mask):
 aa=parsed[zone];chunks=[];indices=[];first=0
 for part in aa['parts']:
  t=mesh.triangles(aa,part);t=t[mask(part,t)]
  if not len(t):continue
  h=bytearray(part['header']);struct.pack_into('<3I',h,32,first,t.size,1);first+=t.size;chunks.append(bytes(h)+bytes(part['verts']));indices.extend(t.ravel())
 if not chunks:return None
 return struct.pack('<I',len(indices))+np.asarray(indices,dtype='<u2').tobytes()+struct.pack('<I',len(chunks))+b''.join(chunks)+aa['bounds']
def combine(raw,extra):
 if not raw:return mesh_bytes(extra)
 aa=mesh.read(raw);bb=mesh.read(mesh_bytes(extra));indices=np.concatenate([aa['indices'],bb['indices']]).astype('<u2');chunks=[bytes(p['header'])+bytes(p['verts']) for p in aa['parts']]
 for p in bb['parts']:
  h=bytearray(p['header']);struct.pack_into('<I',h,32,p['first']+len(aa['indices']));chunks.append(bytes(h)+bytes(p['verts']))
 return struct.pack('<I',len(indices))+indices.tobytes()+struct.pack('<I',len(chunks))+b''.join(chunks)+bounds(np.concatenate([p['streams'][0,0] for p in aa['parts']+bb['parts']]))
def write(name,objects,retained=None):
 parts=[as_parts(o) for o in objects];data=combine(retained,parts);decoded=mesh.read(data)
 assert max(len(p['streams'][0,0]) for p in decoded['parts'])<65536
 for suffix in ['','_ui']:(candidate/'assets/cars'/(model+suffix)/'mesh'/(name+'.d8m')).write_bytes(data)
 rows.append(dict(name=name,objects=[o.name for o in objects],authored_triangles=sum(len(p['tri']) for p in parts),total_triangles=sum(len(mesh.triangles(decoded,p)) for p in decoded['parts']),sha256=hashlib.sha256(data).hexdigest()))
bumper_trees={}
for zone in ['fbump','rbump']:
 vv=[];ff=[]
 for part in parsed[zone]['parts']:
  off=len(vv);vv.extend(part['streams'][0,0].tolist());ff.extend((mesh.triangles(parsed[zone],part)+off).tolist())
 bumper_trees[zone]=BVHTree.FromPolygons(vv,ff,all_triangles=True)
envelopes={};envelope_samples={}
for zone,sgn in [('fbump',1),('rbump',-1)]:
 samples=[]
 for x in np.linspace(0,85,36):
  hits=[]
  for side in [-1,1]:
   for y in np.linspace(27,57,24):
    hit=bumper_trees[zone].ray_cast(Vector((float(x*side),float(y),sgn*300)),Vector((0,0,-sgn)),150)[0]
    if hit is not None:hits.append(sgn*hit.z)
  assert hits,(zone,x)
  samples.append((float(x),max(hits)))
 xx=np.asarray([v[0]/85 for v in samples])**2;zz=np.asarray([v[1] for v in samples]);envelopes[zone]=np.polynomial.polynomial.polyfit(xx,zz,3);envelope_samples[zone]=samples

def front_curve(x,y,level):
 t=float(np.clip((55-y)/(55-[0,23,21,19][level]),0,1));shape=(2+level)*math.sin(math.pi*t)-1.0*t
 return float(np.polynomial.polynomial.polyval((abs(x)/85)**2,envelopes['fbump']))+shape+.5

def rear_curve(x,y,level):
 t=float(np.clip((44-y)/(44-[0,23,21,19][level]),0,1));shape=(1.5+level*.6)*math.sin(math.pi*t)-1.5*t
 return -float(np.polynomial.polynomial.polyval((abs(x)/85)**2,envelopes['rbump']))-shape-.5
def fascia(zone,level):
 front=zone=='fbump';sgn=1 if front else -1;curve=front_curve if front else rear_curve
 top=54.5 if front else 44.0;bottom=[0,23,21,19][level];width=[0,85,85.5,86][level]
 if front:
  hx=[0,44,47,50][level];sx=[0,59,58,60][level];ex=[0,76,78,80][level];low=[0,27,23,21][level];high=[0,40,45,47][level]
  xs=[-width,-ex,-sx,-hx,hx,sx,ex,width];ys=[bottom,low,high,top]
  holes={(1,1),(3,1),(5,1)}
 else:
  hx=[0,42,51,56][level];xs=[-width,-73,-hx,hx,73,width];low=bottom+2;high=[0,27,34,37][level];ys=[bottom,low,high,top];holes={(2,1)}
 def warp(x,y):
  if front and level>=2 and abs(x)<=hx+.01:
   x*=.83+.17*np.clip((y-low)/(high-low),0,1)
  x=float(x)*(1-.025*np.clip((top-y)/(top-bottom),0,1))
  return x,y,curve(x,y,level)
 v=[];faces=[]
 for j in range(len(ys)-1):
  for i in range(len(xs)-1):
   if (i,j) in holes:continue
   # Sample the actual curved bumper instead of spanning a wide curved face
   # with one flat quad; the old centre must not poke through an intake.
   nx=max(2,int((xs[i+1]-xs[i])/7));ny=3
   for jj in range(ny):
    for ii in range(nx):
     x0=xs[i]+(xs[i+1]-xs[i])*ii/nx;x1=xs[i]+(xs[i+1]-xs[i])*(ii+1)/nx
     y0=ys[j]+(ys[j+1]-ys[j])*jj/ny;y1=ys[j]+(ys[j+1]-ys[j])*(jj+1)/ny
     start=len(v);v.extend([warp(x0,y0),warp(x1,y0),warp(x1,y1),warp(x0,y1)]);f=(start,start+1,start+2,start+3);faces.append(f if front else tuple(reversed(f)))
 shell=obj(('FRONT' if front else 'REAR')+'_painted_fascia',v,faces,body)
 # Weld the curved skin for continuous smooth normals, retaining hard intake rims.
 bm=bmesh.new();bm.from_mesh(shell.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001);bm.to_mesh(shell.data);bm.free()
 for poly in shell.data.polygons:poly.use_smooth=True
 # The return surfaces and inset walls provide the shell's visible thickness.
 # Each actual opening gets a bevelled lip, deep walls and a recessed grille.
 for i,j in sorted(holes):
  corners=[(xs[i],ys[j]),(xs[i+1],ys[j]),(xs[i+1],ys[j+1]),(xs[i],ys[j+1])];outer=[]
  for k in range(4):
   ax,ay=corners[k];bx,by=corners[(k+1)%4];steps=max(2,int(math.hypot(bx-ax,by-ay)/6))
   outer.extend(warp(ax+(bx-ax)*t/steps,ay+(by-ay)*t/steps) for t in range(steps))
  n=len(outer);inner=[(x,y,z-sgn*2.2) for x,y,z in outer];ring=obj('INTAKE_bevel_'+str(i),outer+inner,[(k,(k+1)%n,(k+1)%n+n,k+n) for k in range(n)],trim);recalc(ring)
  # Recessed dark grille follows the curved face in both dimensions.
  back=[];bf=[];nx=20;ny=4
  for jj in range(ny+1):
   for ii in range(nx+1):
    x,y,z=warp(xs[i]+(xs[i+1]-xs[i])*ii/nx,ys[j]+(ys[j+1]-ys[j])*jj/ny);back.append((x,y,z-sgn*2.25))
  for jj in range(ny):
   for ii in range(nx):
    k=jj*(nx+1)+ii;f=(k,k+1,k+nx+2,k+nx+1);bf.append(f if front else tuple(reversed(f)))
  obj('RECESSED_mesh_'+str(i),back,bf,grille)
  cx=(xs[i]+xs[i+1])/2;cy=(ys[j]+ys[j+1])/2
  # Horizontal structural slats remain visible through native specular shading.
  for iy in np.arange(ys[j]+2,ys[j+1]-1,3.0):
   x1,x2=xs[i]+1.2,xs[i+1]-1.2
   if front and level>=2 and abs(cx)<1:x1*=.9;x2*=.9
   sections=[]
   for x in np.linspace(x1,x2,9):
    xx,yy,z=warp(float(x),float(iy));z-=sgn*1.65
    sections.append([(xx,yy-.14,z-sgn*.22),(xx,yy-.14,z+sgn*.22),(xx,yy+.14,z+sgn*.22),(xx,yy+.14,z-sgn*.22)])
   loft('GRILLE_slats',sections,trim,0)
  if front and i in [1,5] and level==1:
   cx=(xs[i]+xs[i+1])/2;yy=(low+high)/2;v=[];f=[]
   for y in np.linspace(yy-3.2,yy+3.2,4):
    for x in np.linspace(cx-5.5,cx+5.5,9):v.append((x,y,curve(float(x),float(y),level)-.7))
   for jj in range(3):
    for ii in range(8):
     k=jj*9+ii;f.append((k,k+1,k+10,k+9))
   obj('CURVED_fog_lens',v,f,lens)
   for y in [yy-1.2,yy+1.2]:
    v=[(x,y+dy,curve(float(x),float(y),level)-.60) for dy in [-.09,.09] for x in np.linspace(cx-5.5,cx+5.5,9)]
    obj('FOG_lens_rib',v,[(j,j+1,j+10,j+9) for j in range(8)],metal)
 # Lower shaped splitter/valance follows the complete bumper, with turned ends.
 sections=[]
 for x in np.linspace(-width,width,19):
  z=curve(float(x),bottom,level);depth=3 if level==1 else 5.5 if level==2 else 8
  sections.append([(x,bottom-1,z-sgn*2),(x,bottom-1,z+sgn*depth),(x,bottom+.6,z+sgn*depth),(x,bottom+1,z-sgn*2)])
 loft(('SPLITTER' if front else 'DIFFUSER_lip'),sections,trim if level==1 else carbon,.15)
 if not front:
  for x in [-60,-30,0,30,60] if level==3 else [-32,0,32] if level==2 else []:
   # Exhaust is at X=-46cm, deliberately between diffuser fins.
   z=curve(x,bottom,level);loft('DIFFUSER_vane',[[ (x-.45,bottom,z-2),(x+.45,bottom,z-2),(x+.45,bottom+11,z+1),(x-.45,bottom+11,z+1)],[(x-.45,bottom+4,z+14),(x+.45,bottom+4,z+14),(x+.45,bottom+12,z+14),(x-.45,bottom+12,z+14)]],trim,.12)
 else:
  # Functional-looking braces / two central grille dividers separate the kits.
  if level>=2:
   for x in [-21,21]:
    z=curve(x,(low+high)/2,level);box('INTAKE_divider',(x-.7,low+1,z-3.1),(x+.7,high-1,z-.5),body,.25)
  if level==3:
   for side in [-1,1]:
    x=side*(width-5);z=curve(x,27,level)
    for y in [23,29]:
     loft('CORNER_canard',[[ (x-side*5,y,z-5),(x+side*1,y,z-5),(x+side*1,y+.5,z+3),(x-side*5,y+.5,z+3)],[(x-side*5,y+1,z+10),(x+side*3,y+1,z+10),(x+side*3,y+1.5,z+10),(x-side*5,y+1.5,z+10)]],carbon,.10)
 # Crisp rub strip preserves the car's period horizontal design line.
 if level==1:
  for side in [-1,1]:
   x0,x1=sorted([side*10,side*79]);sections=[]
   for x in np.linspace(x0,x1,12):
    yy=top-3;zz=curve(x,yy,level);sections.append([(x,yy-1,zz),(x,yy-1,zz+sgn*.8),(x,yy+1,zz+sgn*.8),(x,yy+1,zz)])
   loft('PERIOD_rub_strip',sections,rubber,.15)

def retained_bumper(zone,level):
 def keep(part,t):
  if part['material'] in ['w257_m3','w257_m4','w257_m5']:return np.ones(len(t),dtype=bool)
  v=part['streams'][0,0][t];c=v.mean(1);n=np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]);length=np.linalg.norm(n,axis=1)
  facing=np.abs(n[:,2])>.45*length;middle=np.abs(c[:,0])<80
  low=c[:,1]<(54 if zone=='fbump' else 44)
  outside=c[:,2]>(190) if zone=='fbump' else c[:,2]<-210
  receiving=(np.abs(n[:,0])>.35*length)&(n[:,0]*c[:,0]>0)&(v[:,:,1].max(1)>=28.8)
  return ~(facing&middle&low&outside)|receiving
 raw=encode_retained(zone,keep)
 if level>=2:
  aa=mesh.read(raw)
  for p in aa['parts']:
   if p['material']=='w257_m0':p['header'][:32]=body.encode().ljust(32,b'\0')
  raw=mesh.write(aa)
 return raw

for level,name in [(1,'01_STREET'),(2,'02_COMBAT'),(3,'03_TOURING')]:
 for zone in ['fbump','rbump']:
  current=col(name+'_'+zone.upper());fascia(zone,level);write(prefix+'_kit'+str(level)+'_'+zone,list(current.objects),retained_bumper(zone,level))
 for side,zone in [(-1,'sskirt_l'),(1,'sskirt_r')]:
  current=col(name+'_'+zone.upper());sections=[]
  # Below all existing vinyls. Ends taper at the real wheel-arch boundaries.
  for z in np.linspace(-97,95,27):
   taper=min(1,(z+97)/17,(95-z)/17);x0=82.5;x1=84+level*2.2*taper;bottom=16-level*1.1*taper;top=27.4
   sections.append([(side*x0,top,z),(side*x1,top-.7,z),(side*(x1+1),bottom+2,z),(side*x1,bottom,z),(side*80,bottom+1,z),(side*80,top,z)])
  loft('SCULPTED_rocker',sections,body,.25)
  for z0,z1 in [(-78,76)] if level==1 else [(-77,-40),(-34,36),(42,78)]:
   x=side*(84+level*2.2);X=x+side*.65;loft('ROCKER_recess',[[ (x,20.5,z0),(X,20.5,z0),(X,23,z0),(x,23,z0)],[(x,20.5,z1),(X,20.5,z1),(X,23,z1),(x,23,z1)]],trim,.15)
  if level==3:
   for z in [-77,74]:
    x=side*90;loft('ROCKER_end_fence',[[(x,14,z-4),(x+side*.6,14,z-4),(x+side*.6,26,z-4),(x,26,z-4)],[(x,16,z+4),(x+side*.6,16,z+4),(x+side*.6,25,z+4),(x,25,z+4)]],carbon,.1)
  write(prefix+'_kit'+str(level)+'_'+zone,list(current.objects),encode_retained(zone,lambda p,t:np.ones(len(t),dtype=bool)))

# 405-specific rear wings: measured shoes on the flat rear deck, foil chord
# entirely behind the rear window. No donor local-space scaling or warped feet.
mounts=[]
for level in [1,2,3]:
 current=col('04_WING_'+str(level));zfoot=-211.5;feet=[];baseheight=surface(0,zfoot);height=[0,10,20,30][level];halfspan=[0,70,75,79][level];chord=[0,16,22,25][level];centre=-215 if level==1 else -219
 for side in [-1,1]:
  x=side*(52 if level<3 else 48);shoe=[]
  for z in [zfoot-5,zfoot+5]:
   y=surface(x,z);shoe.append([(x-3.5,y-.12,z),(x+3.5,y-.12,z),(x+3.5,y+1,z),(x-3.5,y+1,z)])
   feet.append(dict(x=x,z=z,y=y))
  loft('DECK_mounting_shoe',shoe,rubber,.15)
  y0=surface(x,zfoot);yt=baseheight+height
  loft('FAIRED_pedestal',[[(x-2.3,y0,zfoot-3),(x+2.3,y0,zfoot-3),(x+2.3,y0,zfoot+3),(x-2.3,y0,zfoot+3)],[(x-1.8,yt-1,centre-2),(x+1.8,yt-1,centre-2),(x+1.8,yt-1,centre+3),(x-1.8,yt-1,centre+3)]],body if level<3 else metal,.3)
 # Cambered airfoil: leading edge towards +Z, trailing edge behind the boot.
 sections=[]
 for x in np.linspace(-halfspan,halfspan,23):
  sweep=2.5*(abs(x)/halfspan)**2;yy=baseheight+height+(.8 if level==1 else 2)*(abs(x)/halfspan)**2;zc=centre+sweep
  sections.append([(x,yy,zc+chord*.5),(x,yy+1.8,zc+chord*.12),(x,yy+1.2,zc-chord*.5),(x,yy-.1,zc-chord*.5),(x,yy-.8,zc+chord*.12)])
 loft('CAMBERED_foil',sections,body if level<3 else carbon,.18)
 if level>=2:
  for side in [-1,1]:
   x=side*halfspan;y=baseheight+height;z=centre+2.5
   box('END_plate',(min(x,x+side*.8),y-4,z-chord/2-1),(max(x,x+side*.8),y+5,z+chord/2+1),body if level==2 else carbon,.5)
 write(prefix+'_wing'+str(level),list(current.objects));mounts.append(dict(option=level,feet=feet,foil_height_cm=baseheight+height,chord_cm=chord))

# Vented hoods cut actual holes in the original triangulated source surface.
def split(poly,axis,value,positive):
 out=[]
 for i,b in enumerate(poly):
  a=poly[i-1];ina=(a[axis]>=value) if positive else (a[axis]<=value);inb=(b[axis]>=value) if positive else (b[axis]<=value)
  if ina!=inb:out.append(a+(b-a)*((value-a[axis])/(b[axis]-a[axis])))
  if inb:out.append(b)
 return out
rects=[(-42,-19,132,170),(19,42,132,170)]
def cut_triangles():
 verts=[];tri=[]
 for t in mesh.triangles(parsed['bonnet'],hp):
  polygons=[list(hp['streams'][0,0][t].astype(float))]
  for x0,x1,z0,z1 in rects:
   remaining=[]
   for poly in polygons:
    inside=poly
    for axis,v,positive in [(0,x0,True),(0,x1,False),(2,z0,True),(2,z1,False)]:
     outside=split(inside,axis,v,not positive)
     if len(outside)>=3:remaining.append(outside)
     inside=split(inside,axis,v,positive)
     if len(inside)<3:break
   polygons=remaining
  for poly in polygons:
   for i in range(1,len(poly)-1):
    vv=np.asarray([poly[0],poly[i],poly[i+1]])
    if np.linalg.norm(np.cross(vv[1]-vv[0],vv[2]-vv[0]))<1e-7:continue
    start=len(verts);verts.extend(vv.tolist());tri.append((start,start+1,start+2))
 return verts,tri
for level in [2,3]:
 current=col('05_VENTED_HOOD_'+str(level));v,t=cut_triangles();panel=obj('ORIGINAL_hood_with_cut_ports',v,t,body if level==2 else carbon)
 for x0,x1,z0,z1 in rects:
  # Dark real well lies below the cut-out; louvers sit above it, not on top
  # of an intact hood. Edges follow actual projected native hood curvature.
  well=[(x,hood_y(x,z)-2,z) for x,z in [(x0,z0),(x1,z0),(x1,z1),(x0,z1)]];obj('VENT_dark_well',well,[(3,2,1,0)],trim)
  edge=[(x,hood_y(x,z)+.15,z) for x,z in [(x0,z0),(x1,z0),(x1,z1),(x0,z1)]]
  obj('VENT_inner_walls',edge+well,[(i,(i+1)%4,(i+1)%4+4,i+4) for i in range(4)],trim)
  for z in np.linspace(z0+2,z1-3,7):
   sections=[]
   for x in np.linspace(x0+.5,x1-.5,7):
    y=hood_y(x,float(z));sections.append([(x,y-.4,z-1.1),(x,y+1.5,z+1.2),(x,y+1.8,z+1.2),(x,y-.1,z-1.1)])
   loft('VENT_louver',sections,body if level==2 else carbon,.12)
 write(prefix+'_vented_'+('paint' if level==2 else 'carbon')+'_bonnet',list(current.objects))
# Extra catalogue hood options append; all existing identities retained.
patch=candidate/'patches/scripts/cardefs.txt.patch';lines=patch.read_text().splitlines();text=bytes.fromhex(lines[1][5:]).decode('latin1');needle='CUSTOM BONNET "w257_sport_BONNET" AERO\n';assert text.count(needle)==1
text=text.replace(needle,needle+'CUSTOM BONNET "w257_vented_paint_BONNET" AERO\nCUSTOM BONNET "w257_vented_carbon_BONNET" AERO\n');patch.write_text('append\ntext '+text.encode('latin1').hex()+'\n')
# Purpose-specific native materials; all colours obey the existing body paint.
mattexts={trim:'Material Program = Constant Colour\nColour = 20 22 26\n',rubber:'Material Program = Constant Colour\nColour = 10 11 13\n',metal:'Material Program = Constant Colour\nColour = 160 165 170\n',lens:'Material Program = Constant Colour\nColour = 170 185 195\n',grille:'Material Program = Constant Colour\nColour = 28 31 35\n'}
# The absent source interior must remain hidden at near workshop distances.
# Native Car Glass stayed visibly transparent in the close view; use an opaque
# reflective dark-glazing material here, retaining all glazing geometry.
mattexts['w257_privacy_glass']='Material Program = Car Body\nCar Colour = 17 22 29\nSide Fresnel = 0.35\nFront Fresnel = 0.10\nMetallic Colour = 0 0 0\nPearlescent Colour = 0 0 0\nMetallic Shininess = 20\nPearlescent Shininess = 0\nMetallic = FALSE\nDouble-sided = FALSE\nPaint Work = w257_paint.ptd\nScratch & Reflect = w257_scratch.ptd\n'
for suffix in ['','_ui']:
 for name,text in mattexts.items():(candidate/'assets/cars'/(model+suffix)/'mesh/mats'/(name+'.pmd')).write_text(text)
# A bright atlas-covered undertray looked like a white sheet in workshop
# reflections. Reassign only66 measured low, horizontal source faces to dark
# underbody; preserve their exact original positions, UVs, normals and colours.
def undertray(part,t):
 v=part['streams'][0,0][t];c=v.mean(1);n=np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0])
 return (part['material']=='w257_m6')&(c[:,1]<36)&(np.abs(c[:,0])<70)&(np.abs(n[:,1])>np.linalg.norm(n,axis=1)*.6)
keep=mesh.read(encode_retained('chass',lambda p,t:~undertray(p,t)));dark=mesh.read(encode_retained('chass',undertray));assert sum(len(mesh.triangles(dark,p)) for p in dark['parts'])==66
indices=np.concatenate([keep['indices'],dark['indices']]).astype('<u2');chunks=[bytes(p['header'])+bytes(p['verts']) for p in keep['parts']]
for p in dark['parts']:
 h=bytearray(p['header']);h[:32]=b'p405_underbody'.ljust(32,b'\0');struct.pack_into('<I',h,32,p['first']+len(keep['indices']));chunks.append(bytes(h)+bytes(p['verts']))
data=struct.pack('<I',len(indices))+indices.tobytes()+struct.pack('<I',len(chunks))+b''.join(chunks)+parsed['chass']['bounds']
assert sum(len(mesh.triangles(mesh.read(data),p)) for p in mesh.read(data)['parts'])==sum(len(mesh.triangles(parsed['chass'],p)) for p in parsed['chass']['parts'])
for suffix in ['','_ui']:
 f=candidate/'assets/cars'/(model+suffix)/'mesh';(f/'w257_chass.d8m').write_bytes(data);(f/'mats/p405_underbody.pmd').write_text('Material Program = Constant Colour\nColour = 12 14 17\n')
rows.append(dict(name='w257_chass',method='66 original undertray faces assigned dark material; geometry unchanged',sha256=hashlib.sha256(data).hexdigest()))
# Save all editable collections, reference hidden by default to avoid overlaps.
for c in collections.values():c.hide_render=True;c.hide_viewport=True
collections['00_ORIGINAL_r17'].hide_render=False;collections['00_ORIGINAL_r17'].hide_viewport=False
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Peugeot405-Juiced-Custom-Shop.blend'))
now={str(p.relative_to(candidate)):sha(p) for p in candidate.rglob('*') if p.is_file()}
changed=sorted(k for k in set(source)|set(now) if source.get(k)!=now.get(k))
assert all(k.startswith(('assets/cars/peugeot_405_glx/','assets/cars/peugeot_405_glx_ui/')) or k=='patches/scripts/cardefs.txt.patch' for k in changed),changed
(candidate/'SHA256.json').write_text(json.dumps({k:v for k,v in now.items() if k!='SHA256.json'},indent=2)+'\n')
(out/'build-receipt.json').write_text(json.dumps(dict(author_sha256=sha(Path(__file__)),source=str(base),source_inventory=source,changed=changed,meshes=rows,wing_mounts=mounts,bumper_envelope_samples=envelope_samples,coordinate_contract='native centimetres X across Y up Z forward; Blender metres X across Z up -Y forward',scope='405 only, no handling, production code, personal save or other-car asset changes',styles=['Street: Mi16-inspired period trim, fog lamps and low raised spoiler','Combat: trapezoid intake, split grille, sculpted rocker, endplate spoiler','Touring: deep intake, canards, splitter, diffuser vanes, carbon GT wing'],reference='FIA 5380 Peugeot405 Mi16 homologation for silhouette and period details; aftermarket designs are original interpretations, not exact homologation models',status='PRIVATE CANDIDATE; native visual acceptance pending'),indent=2)+'\n')
print('BUILT',len(rows),'native 405 customization meshes',flush=True)
