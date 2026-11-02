"""405 r20: finished rear fascias, purpose-fitted wings and closed grille cavity.
Blender --background --python-exit-code1 --python THIS -- REPO BASE_R19 FRESH_OUT
"""
from pathlib import Path
import sys
# Reuse the frozen native geometry adapter, not the previous model generator.
repo=Path(sys.argv[sys.argv.index('--')+1]);helper=repo/'tools/peugeot405-photo-kits/build.py'
prelude=helper.read_text().split('# Photo-specific silhouettes')[0]
exec(compile(prelude,str(helper),'exec'))
from mathutils.geometry import delaunay_2d_cdt
reference.name='00_ORIGINAL_R19';collections.pop('00_ORIGINAL_r18');collections[reference.name]=reference
# Import only the constrained silhouette helpers. They call curve/fit_x below.
helpers=helper.read_text();helpers=helpers[helpers.index('def inside('):helpers.index('def front(')]
exec(compile(helpers,str(helper),'exec'))
samples=np.asarray(json.loads((repo/'tools/peugeot405-rework/assets/build-receipt.json').read_text())['bumper_envelope_samples']['rbump'])
current_level=1
def curve(zone,x,y):
 assert zone=='rbump'
 base=float(np.interp(abs(x),samples[:,0],samples[:,1]))
 offset=float(np.interp(y,[20,27,35,45,54,60],[-2.5,-.2,1.0,1.1,-.2,-5.1]))
 if current_level==2:offset+=float(np.interp(y,[20,30,36,44,60],[.4,-.3,-.8,.3,0]))
 if current_level==3:offset+=float(np.interp(y,[20,30,39,49,60],[0,.8,0,.4,0]))
 return -(base+offset)
def fit_x(zone,x,y):
 return x*float(np.interp(y,[20,30,44,60],[.915,.945,.985,1]))
def plate(p,t):
 v=p['streams'][0,0][t]
 return (p['material']=='w257_m6')&(np.abs(v[:,:,2]+227.130).max(1)<.02)&(np.abs(v[:,:,0]).max(1)<28)&(v[:,:,1].min(1)>41)&(v[:,:,1].max(1)<55)
def retained_rear(p,t):
 v=p['streams'][0,0][t];c=v.mean(1);n=np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]);length=np.linalg.norm(n,axis=1)
 if p['material']=='w257_m4':return np.ones(len(t),bool)
 replacing=(c[:,1]<59.9)&(np.abs(c[:,0])<84.5)&(c[:,2]<-208)
 if p['material']=='w257_m5':replacing|=(c[:,1]<60)&(c[:,2]<-170)
 # Unmodified source vinyl receivers on outward-facing corner panels.
 receiver=(p['material']==body)&(np.abs(n[:,0])>.55*length)&(n[:,0]*c[:,0]>0)&(v[:,:,1].max(1)>=29)
 return ~replacing|receiver|plate(p,t)
assert sum(int(plate(p,mesh.triangles(parsed['rbump'],p)).sum()) for p in parsed['rbump']['parts'])==2
raw=encode_retained('rbump',retained_rear)
aa=mesh.read(raw)
for p in aa['parts']:
 if p['material']=='w257_m0':p['header'][:32]=body.encode().ljust(32,b'\0')
raw=mesh.write(aa)
for current_level,label in [(1,'GUTMANN'),(2,'STRATOS'),(3,'SHOWCASE')]:
 current=col('01_REAR_'+label);low=[0,24,22,23][current_level]
 # Exhaust is a genuine notch in the lower silhouette, not a dark rectangle
 # pasted over paint. It follows the actual left-hand pipe centre X=-46cm.
 outline=[(-84,low),(-56,low),(-54,30),(-40,30),(-38,low),(84,low),(85.5,36),(84.5,60),(-84.5,60),(-85.5,36)]
 outline=rounded(outline,.05,4);license=rect(-29.2,29.2,39.7,56.5,.10);holes=[license]
 if current_level==2:holes += [rect(-29,29,25.2,31.8,.14)]
 if current_level==3:holes += [rounded([(-30,24.5),(67,24.5),(62,32),(-27,32)],.15)]
 skin('rbump','Continuous_moulded_rear_shell',outline,holes)
 opening('rbump','Recessed_number_plate_pocket',license,rubber,3.9)
 for i,h in enumerate(holes[1:]):opening('rbump','Lower_valance_insert_'+str(i),h,rubber,2.0)
 # A controlled, thin rolled edge follows each lower section, leaving the
 # exhaust notch open and turning back into the source wheel-arch returns.
 for x0,x1 in [(-83,-56),(-38,83)]:
  sections=[]
  for x in np.linspace(x0,x1,24):
   z=curve('rbump',x,low+1);xx=fit_x('rbump',x,low+1)
   sections.append([(xx,low,z+1.0),(xx,low,z-.7),(xx,low+1.4,z-1.0),(xx,low+1.7,z+.3)])
  loft('Fitted_lower_rolled_edge',sections,body,.12)
 if current_level==1:band('rbump','Period_upper_rub_strip',58,80,1.25,rubber,.15)
 # Original plate remains at its original position/UVs behind the new reveal.
 # Two restrained top lights give the recess visible scale at workshop range.
 for x in [-18,18]:
  z=curve('rbump',x,55.9)+.7
  box('Plate_light_housing',(x-2.0,55.3,z-.25),(x+2.0,56.1,z+.4),trim,.10)
 write(prefix+'_kit'+str(current_level)+'_rbump',list(current.objects),raw)

def foot(x,z,width,length):
 # Four footprint corners are individually seated on the measured deck.
 sections=[]
 for zz in [z-length/2,z+length/2]:
  sections.append([(x-width/2,surface(x-width/2,zz)-.06,zz),(x+width/2,surface(x+width/2,zz)-.06,zz),(x+width/2,surface(x+width/2,zz)+.45,zz),(x-width/2,surface(x-width/2,zz)+.45,zz)])
 loft('Deck_fitted_rubber_shoe',sections,rubber,.10)
def foil(name,half,chord,zc,y,mat,sweep=1.5,rise=0):
 sections=[]
 for x in np.linspace(-half,half,43):
  a=abs(x)/half;cc=chord*(1-.20*a**6);centre=zc+sweep*a*a;yy=y+rise*a*a
  profile=[]
  # Thin rounded leading edge and sharp trailing edge; an actual solid foil.
  for upper,ts in [(True,np.linspace(0,1,19)),(False,np.linspace(1,0,19)[1:-1])]:
   for t in ts:
    thick=.95*math.sin(math.pi*t)**.65;camber=.55*math.sin(math.pi*t)
    profile.append((x,yy+camber+(thick if upper else -.60*thick),centre+cc*(.5-t)))
  sections.append(profile)
 o=loft(name,sections,mat,.06)
 for p in o.data.polygons:p.use_smooth=True
 return o

mounts=[]
for level in [1,2,3]:
 current=col('02_WING_'+str(level));seats=[]
 if level==1:
  # Bonded low wraparound lip: its entire underside follows the real boot.
  sections=[]
  for x in np.linspace(-70,70,47):
   taper=.55+.45*(1-(abs(x)/70)**5);front=-209.5;rear=-219.5
   yf=surface(float(x),front);yr=surface(float(x),rear)
   sections.append([(x,yf-.08,front),(x,yf+.45,front),(x,yr+3.6*taper,rear+.6),(x,yr+1.1*taper,rear-1),(x,yr-.08,rear)])
  o=loft('Bonded_boot_lip_with_contoured_underside',sections,body,.12)
  for p in o.data.polygons:p.use_smooth=True
 else:
  target=110.8 if level==2 else 117.8;zc=-216 if level==2 else -218.0;span=72 if level==2 else 75;chord=16 if level==2 else 21
  for side in [-1,1]:
   x=side*(61 if level==2 else 49);z=-213.2;foot(x,z,7,9.5);basey=surface(x,z)+.35;seat=dict(x=x,z=z,y=basey);seats.append(seat)
   sections=[]
   for t in np.linspace(0,1,6):
    centre=z+(zc-z)*t;width=3.1*(1-t)+1.65*t;length=4.0*(1-t)+2.6*t;y=basey+(target-.55-basey)*t
    sections.append([(x-width,y,centre-length),(x+width,y,centre-length),(x+width,y,centre+length),(x-width,y,centre+length)])
   o=loft('Moulded_swept_bodycolour_pedestal',sections,body,.22)
   for p in o.data.polygons:p.use_smooth=True
  foil('Thin_cambered_painted_airfoil',span,chord,zc,target,body,sweep=1.2 if level==2 else 2.5,rise=-1.1 if level==2 else 1.2)
  if level==3:
   # Small shaped endcaps echo the foil; no giant rectangular carbon walls.
   for side in [-1,1]:
    x=side*span;centre=zc+2.5;y=target+1.2
    outline=[(y-1.0,centre+9),(y+2.8,centre+6),(y+2.2,centre-8),(y-.4,centre-10),(y-2.4,centre-6),(y-2.0,centre+6)]
    loft('Shaped_endcap',[[ (x,yy,zz) for yy,zz in outline],[(x+side*.5,yy,zz) for yy,zz in outline]],body,.15)
 write(prefix+'_wing'+str(level),list(current.objects));mounts.append(dict(option=level,seats=seats,construction='Bonded surface-following lip' if level==1 else 'Painted thin foil with individually seated moulded pedestals'))

# The stock upper radiator grille was a set of open slats looking into an
# empty source body. Close the cavity behind them for every kit and stock.
current=col('03_CLOSED_RADIATOR_CAVITY');front=[];back=[]
for x in np.linspace(-46,46,25):
 z=193.0;top=hood_y(float(x),z)-.9
 front.append([(x,20,z),(x,top,z)])
 back.append([(x,20,z-5),(x,top-.4,z-5)])
v=[p for sec in front for p in sec]+[p for sec in back for p in sec];n=len(front)*2;f=[]
for i in range(len(front)-1):
 j=i*2;f += [(j,j+2,j+3,j+1),(n+j+1,n+j+3,n+j+2,n+j),(j,n+j,n+j+2,j+2),(j+1,j+3,n+j+3,n+j+1)]
f += [(0,1,n+1,n),(n-2,2*n-2,2*n-1,n-1)]
recalc(obj('Opaque_shaped_radiator_backing_and_returns',v,f,rubber))
mouth=[];inner=[]
for x in np.linspace(-46,46,25):
 hit=exterior.ray_cast(Vector((float(x),220,205)),Vector((0,-1,0)),220)[0]
 assert hit is not None,('missing source grille roof',float(x))
 mouth.append([(x,20,205),(x,hit.y-.5,205)])
 inner.append([(x,20,193),(x,hood_y(float(x),193)-.9,193)])
v=[p for sec in mouth for p in sec]+[p for sec in inner for p in sec];n=len(mouth)*2;f=[]
for i in range(len(mouth)-1):
 j=i*2;f += [(j,n+j,n+j+2,j+2),(j+1,j+3,n+j+3,n+j+1)]
f += [(0,1,n+1,n),(n-2,2*n-2,2*n-1,n-1)]
# These are the inside walls of an open-mouth cavity. Their visible normals
# must face inward; recalculating outward normals would cull them when seen
# through the grille at a diagonal despite a successful two-sided BVH ray.
obj('Sealed_grille_side_roof_and_floor_returns',v,[tuple(reversed(face)) for face in f],rubber)
# A few subdued cooling fins, behind rather than across the existing grille.
for y in np.arange(55,72,2.5):
 v=[]
 for x in np.linspace(-43,43,25):
  if y+.12<hood_y(float(x),193)-1:v += [(x,y-.07,193.10),(x,y+.07,193.10)]
 if len(v)>3:obj('Dark_radiator_fins',v,[(i,i+2,i+3,i+1) for i in range(0,len(v)-2,2)],grille)
write(prefix+'_chass',list(current.objects),encode_retained('chass',lambda p,t:np.ones(len(t),bool)))

for c in collections.values():c.hide_render=True;c.hide_viewport=True
reference.hide_render=False;reference.hide_viewport=False
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Peugeot405-Refinement-Authoring.blend'))
now={str(p.relative_to(candidate)):sha(p) for p in candidate.rglob('*') if p.is_file()};changed=sorted(k for k in source.keys()|now.keys() if source.get(k)!=now.get(k))
assert all(k.startswith(('assets/cars/peugeot_405_glx/mesh/','assets/cars/peugeot_405_glx_ui/mesh/')) for k in changed),changed
(candidate/'SHA256.json').write_text(json.dumps({k:v for k,v in now.items() if k!='SHA256.json'},indent=2)+'\n')
(out/'build-receipt.json').write_text(json.dumps(dict(author_sha256=sha(Path(__file__)),helper_sha256=sha(helper),source=str(base),changed=changed,meshes=rows,wing_mounts=mounts,scope='405 rear bumpers, wings and closed upper grille cavity only. No catalogue, front/hood/skirts, handling or save changes.',status='Private native review pending'),indent=2)+'\n')
print('BUILT',len(rows),'refined native405 meshes',flush=True)
