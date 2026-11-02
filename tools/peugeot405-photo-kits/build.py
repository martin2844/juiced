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
reference=col('00_ORIGINAL_r18');current=None
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

# Photo-specific silhouettes, not scaled variants of the old three-hole bumper.
# Each 2D outline is in native centimetres, front elevation. Constrained edges
# guarantee the triangulation cannot bridge an opening.
from mathutils.geometry import delaunay_2d_cdt
reference_spec=json.loads((repo/'tools/peugeot405-photo-kits/references.json').read_text())
samples=json.loads((repo/'tools/peugeot405-rework/assets/build-receipt.json').read_text())['bumper_envelope_samples']
def curve(zone,x,y):
 sgn=1 if zone=='fbump' else -1
 points=np.asarray(samples[zone]);z=float(np.interp(abs(x),points[:,0],points[:,1]))
 # Broad, almost planar period bumper bands; no sinusoidal swollen centre.
 offset=float(np.interp(y,[19,25,34,46,55],[.3,1.1,1.0,.6,0]))
 return sgn*(z+offset)
def fit_x(zone,x,y):
 # The stock rear narrows below its waist. Keeping the full upper width at
 # ground level produces boxy hanging corner blocks, unlike the photographs.
 return x*float(np.interp(y,[19,28,44,55],[.945,.967,.994,1])) if zone=='rbump' else x
def inside(p,poly):
 x,y=p;c=False
 for i,(xx,yy) in enumerate(poly):
  ax,ay=poly[i-1]
  if (yy>y)!=(ay>y) and x<(ax-xx)*(y-yy)/(ay-yy)+xx:c=not c
 return c
def rounded(poly,r=.12,steps=4):
 out=[]
 for i,b in enumerate(poly):
  b=np.array(b,float);a=np.array(poly[i-1],float);c=np.array(poly[(i+1)%len(poly)],float)
  u=b+(a-b)*r;v=b+(c-b)*r
  for t in np.linspace(0,1,steps):out.append(tuple((1-t)**2*u+2*t*(1-t)*b+t*t*v))
 return out
def rect(x0,x1,y0,y1,r=.12):return rounded([(x0,y0),(x1,y0),(x1,y1),(x0,y1)],r)
def mirror(poly):return [(-x,y) for x,y in reversed(poly)]
def skin(zone,name,outline,holes,mat=body,depth=0):
 sgn=1 if zone=='fbump' else -1;coords=[];edges=[]
 for loop in [outline]+holes:
  start=len(coords);coords+=loop;edges += [(start+i,start+(i+1)%len(loop)) for i in range(len(loop))]
 xs,ys=zip(*outline)
 for x in np.arange(min(xs)+2,max(xs),4):
  for y in np.arange(min(ys)+1,max(ys),3):
   if inside((x,y),outline) and not any(inside((x,y),h) for h in holes):coords.append((x,y))
 vv,_,ff,*_=delaunay_2d_cdt([Vector(p) for p in coords],edges,[],0,1e-5,False)
 keep=[]
 for f in ff:
  c=sum((vv[i] for i in f),Vector((0,0)))/len(f)
  if inside(c,outline) and not any(inside(c,h) for h in holes):keep.append(tuple(f) if sgn>0 else tuple(reversed(f)))
 positions=[(fit_x(zone,v.x,v.y),v.y,curve(zone,v.x,v.y)-sgn*depth) for v in vv]
 o=obj(name,positions,keep,mat)
 for p in o.data.polygons:p.use_smooth=True
 return o
def opening(zone,name,loop,backmat=rubber,depth=4.5):
 sgn=1 if zone=='fbump' else -1;n=len(loop)
 outside=[(fit_x(zone,x,y),y,curve(zone,x,y)) for x,y in loop]
 cx=sum(x for x,y in loop)/n;cy=sum(y for x,y in loop)/n
 inset=[(cx+(x-cx)*.975,cy+(y-cy)*.92) for x,y in loop]
 bevel=[(fit_x(zone,x,y),y,curve(zone,x,y)-sgn*.65) for x,y in inset]
 inner=[(fit_x(zone,x,y),y,curve(zone,x,y)-sgn*depth) for x,y in inset]
 o=obj(name+'_painted_rolled_rim',outside+bevel,[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)],body)
 recalc(o)
 o=obj(name+'_reveal',bevel+inner,[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)],trim)
 recalc(o)
 skin(zone,name+'_recessed_back',inset,[],backmat,depth+.1)
 if zone=='fbump' and backmat==rubber:
  # Diamond mesh follows the same curved shell behind the real rim. Clip
  # short diagonal strips to the opening so no grille wire crosses paint.
  xs,ys=zip(*inset);v=[];f=[]
  for slope in [-.55,.55]:
   for intercept in np.arange(min(ys)-abs(max(xs))*1.2,max(ys)+abs(max(xs))*1.2,2.2):
    last=None
    for x in np.arange(min(xs),max(xs)+.3,.65):
     y=slope*x+intercept
     if inside((x,y),inset):
      point=(float(x),float(y))
      if last:
       k=len(v)
       for xx,yy in [last,point]:
        z=curve(zone,xx,yy)-sgn*2.4;v.extend([(xx,yy-.065,z),(xx,yy+.065,z)])
       f.append((k,k+2,k+3,k+1))
      last=point
     else:last=None
  if f:obj(name+'_diamond_screen',v,f,grille)
def band(zone,name,y,width,height,mat,projection=0):
 sec=[];sgn=1 if zone=='fbump' else -1
 for x in np.linspace(-width,width,49):
  z=curve(zone,x,y)+sgn*projection
  xx=fit_x(zone,x,y);sec.append([(xx,y-height/2,z-sgn*.3),(xx,y-height/2,z+sgn*.6),(xx,y+height/2,z+sgn*.6),(xx,y+height/2,z-sgn*.3)])
 return loft(name,sec,mat,.08)
def front(level):
 if level==1:
  # Gutmann: shallow trapezoid, four rectangular lamps and five apron slots.
  centre=rounded([(-38,35),(38,35),(45,46),(-45,46)],.08)
  lamp=rounded([(49,35),(77,36),(77,46),(47,46)],.10)
  lower=[rect(-22,22,24,27.8),rect(25,52,24,27.8),rect(57,74,24.4,28)]
  holes=[centre,lamp,mirror(lamp)]+lower+[mirror(h) for h in lower[1:]]
  low=20.5
 elif level==2:
  centre=rounded([(-30,24),(30,24),(40,46),(-40,46)],.14)
  wing=rounded([(44,24),(76,25),(77,46),(55,46)],.20)
  holes=[centre,wing,mirror(wing)];low=20
 else:
  centre=rounded([(-31,31),(31,31),(34,44),(-34,44)],.18)
  duct=rounded([(59,32),(77,34),(77,44),(60,44)],.18)
  holes=[centre,duct,mirror(duct)];low=21
 outline=rounded([(-85,low),(85,low),(86,34),(85,55),(-85,55),(-86,34)],.035)
 skin('fbump','Photo_fitted_front_skin',outline,holes)
 for i,h in enumerate(holes):opening('fbump','Front_opening_%02d'%i,h)
 if level==1:
  for side in [-1,1]:
   for x0,x1 in [(49.5,61),(63,74.5)]:
    h=rect(x0,x1,37,44,.08);h=h if side>0 else mirror(h)
    skin('fbump','Rectangular_glass_fog_lamp',h,[],lens,.8)
    # Lens fluting is actual raised geometry, not a white rectangle.
    for x in np.arange(x0+1,x1,2):
     x*=side;z=curve('fbump',x,40)-.65
     box('Fluted_fog_lens',(x-.08,37.5,z-.06),(x+.08,43.5,z+.06),metal,0)
  band('fbump','Gutmann_lower_painted_web',30,83,.9,body,.25)
  band('fbump','Gutmann_period_bumper_strip',51.7,81,1.8,rubber,.2)
 elif level==3:
  for side in [-1,1]:
   for x in [41,51]:
    x*=side;h=[(x+3.5*math.cos(t),37.6+3.5*math.sin(t)) for t in np.linspace(0,2*math.pi,24,endpoint=False)]
    opening('fbump','Circular_fog_housing',h,trim,.35);skin('fbump','Circular_glass_fog',h,[],lens,-.10)
  band('fbump','Blue_show_car_double_lower_lip',26,84,.85,body,1.4)
 # A modest continuous painted lip, not carbon planks or canards.
 band('fbump','Integrated_lower_edge',low+.8,84,1.5,body,.8)

def retained(zone):
 def mask(p,t):
  v=p['streams'][0,0][t];c=v.mean(1);n=np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]);length=np.linalg.norm(n,axis=1)
  if p['material'] in ['w257_m3','w257_m4']:return np.ones(len(t),bool)
  front=zone=='fbump';height=55 if front else 43.8
  # Keep the original corner return, upper seam and painted vinyl receivers.
  replace=(c[:,1]<height)&(np.abs(c[:,0])<84)&((c[:,2]>190) if front else (c[:,2]<-210))
  receiver=(p['material']==body)&(np.abs(n[:,0])>.45*length)&(n[:,0]*c[:,0]>0)&(v[:,:,1].max(1)>=29)
  return ~replace|receiver
 raw=encode_retained(zone,mask);aa=mesh.read(raw)
 # Painted aftermarket return surfaces; original chrome above the front seam stays.
 for p in aa['parts']:
  if p['material']=='w257_m0':p['header'][:32]=body.encode().ljust(32,b'\0')
 return mesh.write(aa)

def rear(level):
 low=[0,22,20,21][level]
 # Exhaust cutout at the actual shared left-hand pipe, not the photo's twin pipe.
 notch=rounded([(-53,low-2),(-39,low-2),(-39,29),(-53,29)],.15)
 outline=[(-85,low),(85,low),(86,32),(85,44),(-85,44),(-86,32)]
 # The cutout touches the lower silhouette; constrain both and clip consistently.
 holes=[notch]
 if level==2:
  holes += [rect(-29,-5,23.8,29.5,.2),rect(5,29,23.8,29.5,.2)]
 if level==3:holes += [rect(-27,27,25,29,.15)]
 skin('rbump','Photo_adapted_rear_valance',outline,holes)
 for i,h in enumerate(holes):opening('rbump','Rear_recess_'+str(i),h)
 band('rbump','Rear_painted_rolled_edge',low+1,84,1.6,body,.35)
 if level==1:band('rbump','Rear_period_rub_strip',40.5,81,1.6,rubber,.2)

for level,label in [(1,'GUTMANN'),(2,'STRATOS'),(3,'BLUE_SHOWCASE')]:
 for zone,fn in [('fbump',front),('rbump',rear)]:
  current=col('%02d_%s_%s'%(level,label,zone));fn(level)
  write(prefix+'_kit'+str(level)+'_'+zone,list(current.objects),retained(zone))
 for side,zone in [(-1,'sskirt_l'),(1,'sskirt_r')]:
  current=col('%02d_%s_%s'%(level,label,zone));sections=[]
  # Sculpted concave profiles below the untouched factory vinyl boundary.
  for z in np.linspace(-97,95,33):
   t=max(0,min(1,(z+97)/16,(95-z)/16));w=[0,3.0,3.8,4.8][level]*t
   shape=[(82.7,28),(83.4+w*.35,26.2),(82.6+w*.35,23.4),(84+w,18.5),(84+w,17),(80.5,18),(80.5,27)]
   if level==2:shape=[(82.7,28),(83.8+w*.6,26),(84+w,19),(83.8+w,17),(80.5,18),(80.5,27)]
   if level==3:shape=[(82.7,28),(84+w*.4,26),(83.8+w*.4,22.5),(85+w,19),(85+w,17),(80.5,18),(80.5,27)]
   sections.append([(side*x,y,z) for x,y in shape])
  o=loft('Continuous_photo_rocker_profile',sections,body,.14)
  for p in o.data.polygons:p.use_smooth=True
  write(prefix+'_kit'+str(level)+'_'+zone,list(current.objects),encode_retained(zone,lambda p,t:np.ones(len(t),bool)))

# Hoods: keep factory pressing and its supplied split normals exactly. Clip
# complete attributes at the little centre scoop; never flatten the whole panel.
def clip(poly,axis,value,positive):
 result=[]
 for i,b in enumerate(poly):
  aa=poly[i-1];ina=aa[axis]>=value if positive else aa[axis]<=value;inb=b[axis]>=value if positive else b[axis]<=value
  if ina!=inb:result.append(aa+(b-aa)*((value-aa[axis])/(b[axis]-aa[axis])))
  if inb:result.append(b)
 return result
def hood_part(mat,cut=False):
 vv=[];tt=[]
 for t in mesh.triangles(parsed['bonnet'],hp):
  attrs=np.c_[hp['streams'][0,0][t],hp['streams'][3,0][t],hp['streams'][5,0][t]]
  polys=[list(attrs.astype(float))]
  if cut:
   rest=[]
   for poly in polys:
    for axis,val,pos in [(0,-18,True),(0,18,False),(2,140,True),(2,154,False)]:
     outside=clip(poly,axis,val,not pos)
     if len(outside)>=3:rest.append(outside)
     poly=clip(poly,axis,val,pos)
     if len(poly)<3:break
   polys=rest
  for poly in polys:
   for i in range(1,len(poly)-1):
    v=np.array([poly[0],poly[i],poly[i+1]])
    if np.linalg.norm(np.cross(v[1,:3]-v[0,:3],v[2,:3]-v[0,:3]))<1e-7:continue
    first=len(vv);vv.extend(v);tt.append((first,first+1,first+2))
 vv=np.asarray(vv);norm=vv[:,3:6]
 uv=vv[:,6:8]
 if mat==carbon:uv=np.c_[(vv[:,0]+vv[:,2])/11.31,(vv[:,2]-vv[:,0])/11.31]
 return dict(mat=mat,pos=vv[:,:3],norm=norm,uv=uv,tri=np.asarray(tt))
for option,name,mat,cut in [(1,'w257_sport_bonnet',carbon,False),(2,'w257_vented_paint_bonnet',body,True),(3,'w257_vented_carbon_bonnet',carbon,True)]:
 current=col('05_HOOD_'+str(option));part=hood_part(mat,cut)
 # Keep the source normals in the editable Blender copy too.
 o=obj('Original_pressed_hood_with_native_normals',part['pos'],part['tri'],mat)
 for poly in o.data.polygons:poly.use_smooth=True
 o.data.normals_split_custom_set_from_vertices([(float(n[0]),float(-n[2]),float(n[1])) for n in part['norm']])
 extra=[]
 if cut:
  # Blue Morette showcase's one centre inlet. Its tapered fairing follows the
  # curved source surface and ends inside both original bonnet crease lines.
  sections=[]
  for z,w,h in [(132,15,0),(136,19.5,2),(145,22.5,4.0),(154,22.5,4.2),(158,22.5,4.0)]:
   yy=hood_y(0,z)
   sections.append([(-w,hood_y(-w,z)+.08,z),(-w*.77,yy+h,z),(w*.77,yy+h,z),(w,hood_y(w,z)+.08,z)])
  v=[p for section in sections for p in section];f=[]
  for j in range(len(sections)-1):
   for i in range(3):f.append((j*4+i,j*4+i+1,(j+1)*4+i+1,(j+1)*4+i))
  scoop=obj('Tapered_central_scoop',v,[tuple(reversed(face)) for face in f],mat,bevel=.22)
  for p in scoop.data.polygons:p.use_smooth=True
  z=158;yy=hood_y(0,z)
  obj('Scoop_inlet_black_recess',[(-21,yy+.15,z-.5),(21,yy+.15,z-.5),(16.5,yy+3.7,z-.5),(-16.5,yy+3.7,z-.5)],[(0,1,2,3)],trim)
  for y in [yy+1,yy+2,yy+3]:box('Scoop_horizontal_mesh',(-16,y-.06,z-.4),(16,y+.06,z-.25),grille,0)
  well=[(x,hood_y(x,z)-1,z) for x,z in [(-18,140),(18,140),(18,154),(-18,154)]]
  obj('Actual_inlet_dark_well',well,[(3,2,1,0)],trim)
  extra=[as_parts(x) for x in current.objects if x!=o]
 data=mesh_bytes([part]+extra)
 for suffix in ['','_ui']:(candidate/'assets/cars'/(model+suffix)/'mesh'/(name+'.d8m')).write_bytes(data)
 rows.append(dict(name=name,sha256=hashlib.sha256(data).hexdigest(),factory_normals_preserved=True,centre_scoop=cut))

for c in collections.values():c.hide_render=True;c.hide_viewport=True
reference.hide_render=False;reference.hide_viewport=False
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Peugeot405-Photo-Kits-Authoring.blend'))
now={str(p.relative_to(candidate)):sha(p) for p in candidate.rglob('*') if p.is_file()}
changed=sorted(k for k in set(source)|set(now) if source.get(k)!=now.get(k))
assert all(k.startswith(('assets/cars/peugeot_405_glx/','assets/cars/peugeot_405_glx_ui/')) for k in changed),changed
assert (candidate/'patches/scripts/cardefs.txt.patch').read_bytes()==(base/'patches/scripts/cardefs.txt.patch').read_bytes()
(candidate/'SHA256.json').write_text(json.dumps({k:v for k,v in now.items() if k!='SHA256.json'},indent=2)+'\n')
(out/'build-receipt.json').write_text(json.dumps(dict(author_sha256=sha(Path(__file__)),source=str(base),changed=changed,meshes=rows,references=reference_spec,scope='405 body kits and hoods only; all part identities, wings, handling, saves and other cars unchanged',status='PRIVATE CANDIDATE: native review pending'),indent=2)+'\n')
print('BUILT',len(rows),'photo-based 405 parts',flush=True)
