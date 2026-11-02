"""405 r21: separate fixed body from removable bumpers and author complete replacements.
Blender --background --python-exit-code1 --python THIS -- REPO BASE_R20 FRESH_OUT
"""
from pathlib import Path
import sys
repo=Path(sys.argv[sys.argv.index('--')+1]);helper=repo/'tools/peugeot405-photo-kits/build.py'
exec(compile(helper.read_text().split('# Photo-specific silhouettes')[0],str(helper),'exec'))
from collections import defaultdict
CUT=161.45;SEAMS={'fbump':57.0,'rbump':63.0};groups={k:defaultdict(list) for k in ['fixed','fbump','rbump']};persistent=[]
def clip(poly,axis,value,positive):
 result=[]
 for i,b in enumerate(poly):
  aa=poly[i-1];da=(aa[axis]-value)*(1 if positive else -1);db=(b[axis]-value)*(1 if positive else -1)
  if (da>=0)!=(db>=0):result.append(aa+(b-aa)*da/(da-db))
  if db>=0:result.append(b)
 return result
def partition(poly,rules):
 outside=[];inside=poly
 for axis,value,positive in rules:
  q=clip(inside,axis,value,not positive)
  if len(q)>=3:outside.append(q)
  inside=clip(inside,axis,value,positive)
  if len(inside)<3:return [],outside
 return [inside],outside
def emit(role,mat,polys):
 for poly in polys:
  for i in range(1,len(poly)-1):
   t=np.asarray([poly[0],poly[i],poly[i+1]])
   if np.linalg.norm(np.cross(t[1,:3]-t[0,:3],t[2,:3]-t[0,:3]))>1e-8:groups[role][mat].append(t)
def native_part(p,aa):
 return dict(mat=p['material'],pos=p['streams'][0,0].copy(),norm=p['streams'][3,0].copy(),uv=p['streams'][5,0].copy(),tri=mesh.triangles(aa,p))
original_area=0.0;paint_triangles=[]
for zone in ['chass','fbump','rbump']:
 aa=parsed[zone]
 for part in aa['parts']:
  if zone=='chass' and part['material'].startswith('p405_'):
   persistent.append(native_part(part,aa));continue
  for t in mesh.triangles(aa,part):
   attrs=np.c_[part['streams'][0,0][t],part['streams'][3,0][t],part['streams'][5,0][t]].astype(float)
   v=attrs[:,:3];original_area+=float(np.linalg.norm(np.cross(v[1]-v[0],v[2]-v[0]))/2)
   # The textured factory trim also forms the outer corner. Paint alone ends
   # inboard of that corner and would make the replacement stop too early.
   paint_triangles.append(v)
   front,rest=partition(list(attrs),[(1,SEAMS['fbump'],False),(2,CUT,True)])
   emit('fbump',part['material'],front)
   for poly in rest:
    rear,fixed=partition(poly,[(1,SEAMS['rbump'],False),(2,-CUT,False)])
    emit('rbump',part['material'],rear);emit('fixed',part['material'],fixed)
def pack_group(group):
 parts=[]
 for mat,tris in group.items():
  if not tris:continue
  a=np.asarray(tris).reshape(-1,8)
  parts.append(dict(mat=mat,pos=a[:,:3],norm=a[:,3:6],uv=a[:,6:8],tri=np.arange(len(a)).reshape(-1,3)))
 return parts
def area(parts):
 return sum(float(np.linalg.norm(np.cross(p['pos'][p['tri'][:,1]]-p['pos'][p['tri'][:,0]],p['pos'][p['tri'][:,2]]-p['pos'][p['tri'][:,0]]),axis=1).sum()/2) for p in parts)
roles={k:pack_group(g) for k,g in groups.items()};split_area=sum(area(v) for v in roles.values());assert abs(split_area-original_area)<.001,(split_area,original_area)
def output(name,parts):
 data=mesh_bytes(parts)
 for suffix in ['', '_ui']:(candidate/'assets/cars'/('peugeot_405_glx'+suffix)/'mesh'/(name+'.d8m')).write_bytes(data)
 rows.append(dict(name=name,parts=len(parts),triangles=sum(len(p['tri']) for p in parts),sha256=hashlib.sha256(data).hexdigest()))
output('w257_chass',roles['fixed']+persistent)
for zone in ['fbump','rbump']:output('w257_'+zone,roles[zone])

# A cross-section's upper convex envelope bridges openings without projecting
# into the car's interior. At the sides it follows the real painted panel, so
# the original fixed vinyl receivers remain close to the new single skin.
paint_triangles=np.asarray(paint_triangles)
def section(y,sgn):
 vv=paint_triangles;points=[]
 for i,j in [(0,1),(1,2),(2,0)]:
  aa=vv[:,i];bb=vv[:,j];den=bb[:,1]-aa[:,1];mask=(np.minimum(aa[:,1],bb[:,1])<=y)&(np.maximum(aa[:,1],bb[:,1])>=y)&(abs(den)>1e-9)
  a0=aa[mask];b0=bb[mask];p=a0+(b0-a0)*((y-a0[:,1])/(b0[:,1]-a0[:,1]))[:,None]
  points.extend((float(x),float(sgn*z)) for x,_,z in p if sgn*z>=CUT-1e-4)
 if len(points)<4:return None
 byx={}
 for x,z in points:byx[round(x,5)]=max(z,byx.get(round(x,5),-1e9))
 hull=[]
 for p in sorted(byx.items()):
  while len(hull)>1:
   aa,bb=hull[-2:];cross=(bb[0]-aa[0])*(p[1]-bb[1])-(bb[1]-aa[1])*(p[0]-bb[0])
   if cross< -1e-8:break
   hull.pop()
  hull.append(p)
 a0=np.asarray(hull)
 if len(a0)<3 or a0[0,0]>-70 or a0[-1,0]<70:return None
 return a0
samples=json.loads((repo/'tools/peugeot405-rework/assets/build-receipt.json').read_text())['bumper_envelope_samples']
def smooth(t):t=np.clip(t,0,1);return t*t*(3-2*t)
tables={};profile_rows={};arch_polynomials={};us=np.linspace(-1,1,241)
for zone,sgn in [('fbump',1),('rbump',-1)]:
 seam=SEAMS[zone];ys=np.linspace(15,seam,161);valid={}
 for i,y in enumerate(ys):
  hull=section(float(y),sgn)
  if hull is not None:valid[i]=hull
 assert len(valid)>20 and len(ys)-1 in valid,(zone,len(valid))
 edges=[];profiles=[]
 first=min(valid);h0=valid[first]
 for i,y in enumerate(ys):
  if i in valid:
   hull=valid[i];left,right=hull[0,0],hull[-1,0];xx=np.where(us<0,us*(-left),us*right);z=np.interp(xx,hull[:,0],hull[:,1])
  elif i<first:
   t=np.clip((y-20)/(ys[first]-20),0,1);left=-(82*(1-t)+(-h0[0,0])*t);right=82*(1-t)+h0[-1,0]*t
   xx=np.where(us<0,us*(-left),us*right);s=np.asarray(samples[zone]);z=np.interp(abs(us)*85,s[:,0],s[:,1])
   edge=CUT+max(0,39**2-(float(y)-31.665)**2)**.5-28
   z+=(edge-z[[0,-1]].mean())*smooth((abs(us)-.82)/.18)
  else:
   j=min(valid,key=lambda j:abs(j-i));hull=valid[j];left,right=hull[0,0],hull[-1,0];xx=np.where(us<0,us*(-left),us*right);z=np.interp(xx,hull[:,0],hull[:,1])
  edges.append((left,right));profiles.append(z)
 edges=np.asarray(edges);profiles=np.asarray(profiles)
 tables[zone]=(ys,edges,profiles);profile_rows[zone]=len(valid)
 ids=sorted(valid);arch_polynomials[zone]=np.polynomial.polynomial.polyfit(ys[ids]-seam,profiles[ids][:,[0,-1]].mean(1),3)

def table_lookup(zone,y,u):
 ys,edges,profiles=tables[zone];f=np.clip((y-ys[0])/(ys[-1]-ys[0])*(len(ys)-1),0,len(ys)-1);i=np.floor(f).astype(int);j=np.minimum(i+1,len(ys)-1);t=f-i
 gu=np.clip((u+1)/2*(len(us)-1),0,len(us)-1);k=np.floor(gu).astype(int);l=np.minimum(k+1,len(us)-1);q=gu-k
 z0=profiles[i,k]*(1-q)+profiles[i,l]*q;z1=profiles[j,k]*(1-q)+profiles[j,l]*q
 left=edges[i,0]*(1-t)+edges[j,0]*t;right=edges[i,1]*(1-t)+edges[j,1]*t
 return np.where(u<0,u*(-left),u*right),z0*(1-t)+z1*t
def original_curve(zone,x,y,level):
 s=np.asarray(samples[zone]);z=np.interp(abs(x),s[:,0],s[:,1])
 if zone=='fbump':return z+np.interp(y,[19,25,34,46,55],[.3,1.1,1,.6,0])
 z+=np.interp(y,[20,27,35,45,54,60],[-2.5,-.2,1,1.1,-.2,-5.1])
 if level==2:z+=np.interp(y,[20,30,36,44,60],[.4,-.3,-.8,.3,0])
 if level==3:z+=np.interp(y,[20,30,39,49,60],[0,.8,0,.4,0])
 return -z
def old_width(main,y):
 v=main['pos'][main['tri']];vals=[]
 for yy in y:
  hits=[]
  for i,j in [(0,1),(1,2),(2,0)]:
   a0=v[:,i];b0=v[:,j];den=b0[:,1]-a0[:,1];mask=(a0[:,1]<=yy)&(b0[:,1]>=yy)|(b0[:,1]<=yy)&(a0[:,1]>=yy);mask &= abs(den)>1e-8
   aa=a0[mask];bb=b0[mask]
   if len(aa):hits.extend(abs(aa[:,0]+(bb[:,0]-aa[:,0])*(yy-aa[:,1])/(bb[:,1]-aa[:,1])))
  vals.append(max(hits) if hits else max(abs(main['pos'][:,0])))
 return np.asarray(vals)
native_records={zone:{bytes(p['verts']) for p in parsed[zone]['parts']} for zone in ['fbump','rbump']}
trim_receipt=[]
for zone,sgn in [('fbump',1),('rbump',-1)]:
 seam=SEAMS[zone];cut=53.0 if zone=='fbump' else 58.0
 for level in [1,2,3]:
  aa=mesh.read((folder/('w257_kit'+str(level)+'_'+zone+'.d8m')).read_bytes());authored=[native_part(p,aa) for p in aa['parts'] if bytes(p['verts']) not in native_records[zone]]
  assert authored and authored[0]['mat']==body
  main=authored[0];widthY=np.linspace(float(main['pos'][:,1].min())+.001,cut,151);width=old_width(main,widthY);width[-1]=old_width(main,np.asarray([cut]))[0]
  def warp(pos):
   x,y,z=pos.T;w=np.interp(y,widthY,width);u=x/w
   topx,top=table_lookup(zone,np.full(len(y),seam),u)
   # A single smooth arch return follows the measured end of the body. Do not
   # reproduce every source cross-section's trim ridges as corrugated skin.
   t=np.clip((y-20)/(seam-20),0,1)
   edge_taper=.985+.015*smooth(t)
   corner=smooth((abs(u)-.72)/.22)
   newx=x*(1-corner)+topx*edge_taper*corner
   _,edge0=table_lookup(zone,np.full(len(y),seam),np.where(u<0,-1.,1.))
   arch=np.maximum(CUT,133.45+np.sqrt(np.maximum(0,39**2-(y-31.665)**2)))
   oem=top+(arch-edge0)*smooth((abs(u)-.65)/.35)
   if zone=='rbump':rawx=x/np.interp(y,[20,30,44,60],[.915,.945,.985,1])
   else:rawx=x
   old=original_curve(zone,rawx,y,level);depth=sgn*(z-old)
   design=sgn*old
   target=top+1.1*(1-u*u)
   blend=smooth((y-(cut-8))/8);centre=design*(1-blend)+target*blend
   surface=centre*(1-corner)+oem*corner
   return np.c_[newx,y,sgn*(surface+depth)]
  def warped(p):
   pos=p['pos'];new=warp(pos);eps=.025;columns=[]
   for axis in [0,1,2]:
    off=np.zeros_like(pos);off[:,axis]=eps;columns.append((warp(pos+off)-warp(pos-off))/(2*eps))
   J=np.stack(columns,axis=2);norm=np.linalg.solve(J.transpose(0,2,1),p['norm'][...,None])[...,0];norm/=np.linalg.norm(norm,axis=1)[:,None]
   return dict(mat=p['mat'],pos=new,norm=norm,uv=p['uv'],tri=p['tri'])
  # Remove the old rounded top: a new continuous collar seats the complete
  # shell directly on the fixed body seam, without any factory bumper skin.
  kept=[]
  for t in main['tri']:
   attrs=np.c_[main['pos'][t],main['norm'][t],main['uv'][t]];poly=clip(list(attrs),1,cut,False)
   for k in range(1,len(poly)-1):
    tri=np.asarray([poly[0],poly[k],poly[k+1]])
    if np.linalg.norm(np.cross(tri[1,:3]-tri[0,:3],tri[2,:3]-tri[0,:3]))>1e-8:kept.extend(tri)
  arr=np.asarray(kept);main_cut=dict(mat=body,pos=arr[:,:3],norm=arr[:,3:6],uv=arr[:,6:8],tri=np.arange(len(arr)).reshape(-1,3));parts=[warped(main_cut)]
  for p in authored[1:]:
   if zone=='rbump' and p['mat']==rubber and p['pos'][:,1].min()>57:continue
   parts.append(warped(p))
  # Order the cut boundary from left to right; include the exact original
  # tessellation intersections so no T-junction floats over another shell.
  boundary=arr[np.abs(arr[:,1]-cut)<1e-6,:3];boundary=np.unique(np.round(boundary,6),axis=0);boundary=boundary[np.argsort(boundary[:,0])]
  low=warp(boundary);u=boundary[:,0]/width[-1];topx,topz=table_lookup(zone,np.full(len(u),seam),u);top=np.c_[topx,np.full(len(u),seam),sgn*topz]
  verts=[];faces=[];steps=5
  for t in np.linspace(0,1,steps):verts.extend(low*(1-t)+top*t)
  n=len(low)
  for j in range(steps-1):
   for i in range(n-1):
    f=(j*n+i,j*n+i+1,(j+1)*n+i+1,(j+1)*n+i);faces.append(f if sgn>0 else tuple(reversed(f)))
  current=col('COLLAR_'+zone+'_'+str(level));o=obj('New_body_matching_mounting_collar',verts,faces,body)
  for poly in o.data.polygons:poly.use_smooth=True
  parts.append(as_parts(o))
  if zone=='rbump':
   plates=[]
   for p in roles['rbump']:
    if p['mat']!='w257_m6':continue
    v=p['pos'][p['tri']];mask=(abs(v[:,:,2]+227.13).max(1)<.02)&(abs(v[:,:,0]).max(1)<28)&(v[:,:,1].min(1)>41)&(v[:,:,1].max(1)<55)
    if mask.any():
     p0={**p,'tri':p['tri'][mask]};parts.append(warped(p0));plates.extend(p0['tri'])
   assert len(plates)==2
  assert not any(p['mat'] in ['w257_m0','w257_m5','w257_m3','w257_m4'] for p in parts)
  output('w257_kit'+str(level)+'_'+zone,parts)
  trim_receipt.append(dict(zone=zone,kit=level,discarded_inherited_source_skin_parts=len(aa['parts'])-len(authored),new_mounting_seam_cm=seam,authored_shell_only=True))

# Pack the actual native results as separate editable objects in the study.
for name in ['w257_chass','w257_fbump','w257_rbump']+['w257_kit'+str(i)+'_'+z for i in [1,2,3] for z in ['fbump','rbump']]:
 current=col(name);aa=mesh.read((candidate/'assets/cars/peugeot_405_glx/mesh'/(name+'.d8m')).read_bytes())
 for i,p in enumerate(aa['parts']):obj(name+'_'+str(i),p['streams'][0,0],mesh.triangles(aa,p),p['material'])
for c in collections.values():c.hide_render=True;c.hide_viewport=True
reference.hide_render=False;reference.hide_viewport=False
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Peugeot405-Bumper-Replacement-Authoring.blend'))
now={str(p.relative_to(candidate)):sha(p) for p in candidate.rglob('*') if p.is_file()};changed=sorted(k for k in source.keys()|now.keys() if source.get(k)!=now.get(k))
assert all(k.startswith(('assets/cars/peugeot_405_glx/mesh/','assets/cars/peugeot_405_glx_ui/mesh/')) for k in changed),changed
(candidate/'SHA256.json').write_text(json.dumps({k:v for k,v in now.items() if k!='SHA256.json'},indent=2)+'\n')
(out/'build-receipt.json').write_text(json.dumps(dict(author_sha256=sha(Path(__file__)),helper_sha256=sha(helper),source=str(base),changed=changed,meshes=rows,partition_area_cm2=dict(before=original_area,after=split_area),factory_mounts=SEAMS,painted_section_rows=profile_rows,kits=trim_receipt,scope='Nine405 mesh pairs only: fixed body / stock slots / six complete custom shells; no other assets, parts, physics or saves'),indent=2)+'\n')
print('BUILT complete bumper replacements and physically separate fixed body.',flush=True)
