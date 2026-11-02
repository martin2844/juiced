"""R23: fit complete replacements around the actual three-dimensional OEM perimeter.
Blender --background --python-exit-code 1 --python THIS -- REPO BASE_R22 FRESH_OUT
"""
from pathlib import Path
import sys
repo=Path(sys.argv[sys.argv.index('--')+1]);helper=repo/'tools/peugeot405-photo-kits/build.py'
exec(compile(helper.read_text().split('# Photo-specific silhouettes')[0],str(helper),'exec'))
old=repo/'tools/peugeot405-fit-finish/build.py';text=old.read_text()
exec(compile(text[text.index('def clip('):text.index('def partition(')],str(old),'exec'))
exec(compile(text[text.index('def old_width('):text.index('native_records=')],str(old),'exec'))
design=repo/'out/validation/peugeot405-bumper-replacement-20261101/r20-installed-rollback/assets/cars/peugeot_405_glx/mesh'
samples=json.loads((repo/'tools/peugeot405-rework/assets/build-receipt.json').read_text())['bumper_envelope_samples']
def original_curve(zone,x,y,level):
 s=np.asarray(samples[zone]);z=np.interp(abs(x),s[:,0],s[:,1])
 if zone=='fbump':return z+np.interp(y,[19,25,34,46,55],[.3,1.1,1,.6,0])
 z+=np.interp(y,[20,27,35,45,54,60],[-2.5,-.2,1,1.1,-.2,-5.1])
 if level==2:z+=np.interp(y,[20,30,36,44,60],[.4,-.3,-.8,.3,0])
 if level==3:z+=np.interp(y,[20,30,39,49,60],[0,.8,0,.4,0])
 return -z
def native(p,m):return dict(mat=p['material'],pos=p['streams'][0,0].copy(),norm=p['streams'][3,0].copy(),uv=p['streams'][5,0].copy(),tri=mesh.triangles(m,p))
def clipped(p,cut,above):
 verts=[]
 for t in p['tri']:
  poly=clip(list(np.c_[p['pos'][t],p['norm'][t],p['uv'][t]]),1,cut,above)
  for i in range(1,len(poly)-1):
   v=np.asarray([poly[0],poly[i],poly[i+1]])
   if np.linalg.norm(np.cross(v[1,:3]-v[0,:3],v[2,:3]-v[0,:3]))>1e-8:verts.extend(v)
 if not verts:return None
 a=np.asarray(verts);return dict(mat=p['mat'],pos=a[:,:3],norm=a[:,3:6],uv=a[:,6:8],tri=np.arange(len(a)).reshape(-1,3))
def smooth(t):t=np.clip(t,0,1);return t*t*(3-2*t)
def write(name,parts):
 data=mesh_bytes(parts)
 for suffix in ['', '_ui']:(candidate/'assets/cars'/('peugeot_405_glx'+suffix)/'mesh'/(name+'.d8m')).write_bytes(data)
 rows.append(dict(name=name,triangles=sum(len(p['tri']) for p in parts),sha256=hashlib.sha256(data).hexdigest()))
receipts=[]
for zone,sgn,cut in [('fbump',1,49.5),('rbump',-1,58.5)]:
 mount=57.65 if zone=='fbump' else 63.9
 factory=parsed[zone];v=[];f=[];source_normals=[]
 for p in factory['parts']:
  offset=len(v);v.extend(p['streams'][0,0]);source_normals.extend(p['streams'][3,0]);f.extend((mesh.triangles(factory,p)+offset).tolist())
 tree=BVHTree.FromPolygons(v,f,all_triangles=True);v=np.asarray(v);f=np.asarray(f);source_normals=np.asarray(source_normals)
 def hit_at(u,y):
  side=-1 if u<0 else 1;theta=abs(u)*1.42
  ray=Vector((side*np.sin(theta),0,sgn*np.cos(theta)));origin=Vector((0,float(y),sgn*133.45))+ray*350
  hit,n,idx,dist=tree.ray_cast(origin,-ray,350)
  if hit is None:return None
  return np.asarray(hit),idx
 # Full radial sampling sees the return behind the widest point of the
 # bumper. A single-valued Z(X) surface cannot represent that overhang.
 ys=np.unique(np.r_[np.linspace(21.25,mount,79),mount]);us=np.linspace(-1,1,401);table=[];end_angles=[]
 for yy in ys:
  polar=[];ends=[]
  for side in [-1,1]:
   raw=[]
   for q in np.linspace(0,1,321):
    h=hit_at(side*q,float(yy))
    if h is not None and h[0][2]*sgn>=161.44:raw.append((q*1.42,h[0]))
   assert len(raw)>60,(zone,yy,len(raw))
   theta=np.asarray([x[0] for x in raw]);points=np.asarray([x[1] for x in raw]);end=float(theta[-1]);ends.append(end)
   radius=np.linalg.norm(points-np.array([0,float(yy),sgn*133.45]),axis=1)
   for uu in us[us<=0] if side<0 else us[us>0]:
    a0=abs(uu);angle=float(np.interp(a0,[0,.47,.70,.87,.93,1],[0,.46,.68,.94,1.06,end]))
    rr=float(np.interp(angle,theta,radius))
    polar.append([side*rr*np.sin(angle),float(yy),sgn*(133.45+rr*np.cos(angle))])
  # Left samples were emitted in increasing logical U; right continues it.
  table.append(polar);end_angles.append(ends)
 table=np.asarray(table);end_angles=np.asarray(end_angles)
 # The original lower stock mesh contains undertray steps and discontinuous
 # mouldings. Use the actual mounting contour as a clean section and taper
 # it continuously below the joint, rather than reproducing those steps.
 table[:]=table[-1]
 clean=table[-1].copy();points=clean[:,[0,2]]*np.array([1,sgn]);hull=[]
 def cross(a,b):return a[0]*b[1]-a[1]*b[0]
 pp=sorted(set(map(tuple,points.tolist())))
 for seq in [pp,pp[::-1]]:
  chain=[]
  for q in seq:
   while len(chain)>1 and cross(np.subtract(chain[-1],chain[-2]),np.subtract(q,chain[-1]))<=0:chain.pop()
   chain.append(q)
  hull.extend(chain[:-1])
 hull=np.asarray(hull);origin=np.array([0,133.45])
 for j,point in enumerate(points):
  direction=point-origin;direction/=np.linalg.norm(direction);hits=[]
  for k,a0 in enumerate(hull):
   edge=hull[(k+1)%len(hull)]-a0;den=cross(direction,edge)
   if abs(den)<1e-9:continue
   rr=cross(a0-origin,edge)/den;t0=cross(a0-origin,direction)/den
   if rr>0 and -.00001<=t0<=1.00001:hits.append(rr)
  assert hits
  q=origin+max(hits)*direction;clean[j,0]=q[0];clean[j,2]=sgn*q[1]
 for k in [0,2]:clean[:,k]=np.convolve(np.pad(clean[:,k],(5,5),mode='edge'),np.ones(11)/11,mode='valid')
 def profile(u,y):
  y=np.clip(y,ys[0],ys[-1]);i=np.searchsorted(ys,y,side='right')-1;i=np.clip(i,0,len(ys)-2);t=(y-ys[i])/(ys[i+1]-ys[i])
  gu=np.clip((u+1)*.5*(len(us)-1),0,len(us)-1);j=np.minimum(np.floor(gu).astype(int),len(us)-2);q=gu-j
  return (table[i,j]*(1-q[:,None])+table[i,j+1]*q[:,None])*(1-t[:,None])+(table[i+1,j]*(1-q[:,None])+table[i+1,j+1]*q[:,None])*t[:,None]
 upper=[]
 for p in factory['parts']:
  if p['material'] not in [body,'w257_m0','w257_m5']:continue
  p0=clipped(native(p,factory),mount,True)
  if p0 is not None:p0['mat']=body;upper.append(p0)
 # The native plate remains the original two UV-mapped triangles.
 plate_parts=[]
 if zone=='rbump':
  for p in factory['parts']:
   if p['material']!='w257_m6':continue
   p0=native(p,factory);vv=p0['pos'][p0['tri']];mask=(abs(vv[:,:,2]+227.13).max(1)<.02)&(abs(vv[:,:,0]).max(1)<28)&(vv[:,:,1].min(1)>41)&(vv[:,:,1].max(1)<55)
   if mask.any():p0['tri']=p0['tri'][mask];plate_parts.append(p0)
 for level in [1,2,3]:
  raw=mesh.read((design/f'w257_kit{level}_{zone}.d8m').read_bytes());orig=mesh.read((design/f'w257_{zone}.d8m').read_bytes());records={bytes(p['verts']) for p in orig['parts']}
  authored=[native(p,raw) for p in raw['parts'] if bytes(p['verts']) not in records];main=authored[0]
  oldlow=float(main['pos'][:,1].min());newlow=([0,17,15.5,17] if zone=='fbump' else [0,19,17.5,18])[level]
  wy=np.linspace(oldlow+.001,cut,151);width=old_width(main,wy)
  def surface(u,yy):
   p0=profile(u,yy);soft=np.c_[np.interp(u,us,clean[:,0]),yy,np.interp(u,us,clean[:,2])]
   blend=smooth((yy-(mount-15))/15);p0=p0*blend[:,None]+soft*(1-blend[:,None]);p0[:,1]=yy
   t=smooth((yy-newlow)/(mount-newlow));amount=.09 if zone=='rbump' else .03
   taper=1-amount*(1-t)
   p0[:,0]*=taper
   p0[:,2]-=sgn*(3.8 if zone=='rbump' else 1.6)*(1-t)**2
   arch=133.45+np.sqrt(np.maximum(0,33.0**2-(yy-31.665)**2))
   edge=smooth((abs(u)-.94)/.06)
   p0[:,2]+=sgn*np.maximum(0,arch-sgn*p0[:,2])*edge
   return p0
  def mapped(pos):
   x,y,z=pos.T;u=np.clip(x/np.interp(y,wy,width),-1,1)
   # Keep lamps round and preserve intake proportions. Extra height is
   # taken up by the unperforated mounting band, not by stretching details.
   yy=y+(newlow-oldlow)*(1-smooth((y-oldlow)/10))+(mount-cut)*smooth((y-(cut-6))/6)
   p0=surface(u,yy)
   # A mild continuous flare fades to zero at the real OEM mounting band.
   eps=.001;du=(surface(np.clip(u+eps,-1,1),yy)-surface(np.clip(u-eps,-1,1),yy))/(2*eps)
   dy=surface(u,yy+.04)-surface(u,yy-.04)
   normal=np.cross(du,dy)*sgn;normal/=np.linalg.norm(normal,axis=1)[:,None]
   rawx=x/np.interp(y,[20,30,44,60],[.915,.945,.985,1]) if zone=='rbump' else x
   depth=sgn*(z-original_curve(zone,rawx,y,level));flare=[0,.45,1.0,.7][level]*(1-smooth((yy-(mount-12))/12))
   return p0+normal*(depth+flare)[:,None]
  def deform(p):
   pos=p['pos'];pp=mapped(pos);cols=[]
   for axis in [0,1,2]:
    d=np.zeros_like(pos);d[:,axis]=.025;cols.append((mapped(pos+d)-mapped(pos-d))/.05)
   J=np.stack(cols,axis=2)
   # At the return's logical endpoints clamp makes the tangent one-sided;
   # the pseudo-inverse handles the exact boundary without corrupt normals.
   n=(np.linalg.pinv(J.transpose(0,2,1))@p['norm'][...,None])[...,0];n/=np.maximum(np.linalg.norm(n,axis=1)[:,None],1e-9)
   return dict(mat=p['mat'],pos=pp,norm=n,uv=p['uv'],tri=p['tri'])
  parts=[]
  for i,p0 in enumerate(authored):
   if p0['pos'][:,1].min()>=cut-.01:continue
   if len(p0['tri'])>400 and np.ptp(p0['pos'][:,1])<2:continue
   if p0['mat']==metal:continue
   if p0['mat']==lens:
    p0['mat']='w257_m3';lo=p0['pos'][:,:2].min(0);hi=p0['pos'][:,:2].max(0)
    p0['uv']=(p0['pos'][:,:2]-(lo+hi)/2)/np.maximum(hi-lo,.01)*np.array([.5,.18])+np.array([.5,.5])
   p0=clipped(p0,cut,False)
   if p0 is not None:
    result=deform(p0)
    if i==0:
     current=col(f'SKIN_{level}_{zone}');o=obj('Continuous_external_skin',result['pos'],result['tri'],body)
     bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bm.to_mesh(o.data);bm.free()
     for p in o.data.polygons:p.use_smooth=True
     result=as_parts(o)
    parts.append(result)
  parts.extend(upper)
  for p0 in plate_parts:parts.append(deform(p0))
  for p0 in parts:
   if p0['mat'] in [rubber,trim]:p0['mat']='p405_inner_surface'
  current=col(f'INTERIOR_{level}_{zone}');vv=[];ff=[];nx=121;ny=25
  for yy in np.linspace(oldlow+.05,cut-.05,ny):
   ww=np.interp(yy,wy,width)
   for uu in np.linspace(-.999,.999,nx):
    xx=uu*ww;rawx=xx/np.interp(yy,[20,30,44,60],[.915,.945,.985,1]) if zone=='rbump' else xx
    inner=mapped(np.array([[xx,yy,original_curve(zone,rawx,yy,level)]]))[0]
    inner[0]*=.945;inner[2]=sgn*133.45+(inner[2]-sgn*133.45)*.945;vv.append(inner)
  for j in range(ny-1):
   for i in range(nx-1):
    face=(j*nx+i,j*nx+i+1,(j+1)*nx+i+1,(j+1)*nx+i)
    ff.append(face if sgn>0 else tuple(reversed(face)))
  lining=obj('Opaque_inside_of_fascia',vv,ff,'p405_inner_surface')
  for p in lining.data.polygons:p.use_smooth=True
  parts.append(as_parts(lining))
  assert parts
  write(f'w257_kit{level}_{zone}',parts)
  receipts.append(dict(zone=zone,kit=level,stock_mounting_band_starts_cm=mount,bottom_cm=newlow,upper_band_triangles=sum(len(p['tri']) for p in upper),single_outer_surface=True))
  current=col(f'KIT_{level}_{zone}')
  for i,p in enumerate(parts):obj(f'{zone}_{level}_{i}',p['pos'],p['tri'],p['mat'])
for c in collections.values():c.hide_render=True;c.hide_viewport=True
reference.hide_render=False;reference.hide_viewport=False
for suffix in ['', '_ui']:
 car=candidate/'assets/cars'/('peugeot_405_glx'+suffix)
 raw=bytearray((car/'textures/w257_paint.ddt').read_bytes());raw[44:48]=bytes([12,12,12,255]);(car/'textures/p405_inner_surface.ddt').write_bytes(raw)
 (car/'mesh/mats/p405_inner_surface.pmd').write_text('Material Program = Specular Opaque\nSpecular Colour = 0 0 0\nSpecular Highlight = 0\nShininess = 1\nDouble-sided = TRUE\nOpaque Texture = p405_inner_surface.ptd\n')
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Peugeot405-OEM-Fit-Authoring.blend'))
now={str(p.relative_to(candidate)):sha(p) for p in candidate.rglob('*') if p.is_file()};changed=sorted(k for k in source.keys()|now.keys() if source.get(k)!=now.get(k))
allowed={f'assets/cars/peugeot_405_glx{s}/mesh/w257_kit{i}_{z}.d8m' for s in ['', '_ui'] for i in [1,2,3] for z in ['fbump','rbump']}|{f'assets/cars/peugeot_405_glx{s}/{p}' for s in ['', '_ui'] for p in ['mesh/mats/p405_inner_surface.pmd','textures/p405_inner_surface.ddt']}
assert set(changed)<=allowed,changed
(candidate/'SHA256.json').write_text(json.dumps({k:v for k,v in now.items() if k!='SHA256.json'},indent=2)+'\n')
(out/'build-receipt.json').write_text(json.dumps(dict(author_sha256=sha(Path(__file__)),source=str(base),design_source=str(design),meshes=rows,changed=changed,kits=receipts,scope='Six complete bumper mesh pairs only. OEM mounting band is the single skin at the top; bespoke fascia is the single skin below it. No factory skin under the fascia. Fixed body, all other assets, engine and physics unchanged.'),indent=2)+'\n')
print('Built OEM-perimeter fitted replacements.',flush=True)
