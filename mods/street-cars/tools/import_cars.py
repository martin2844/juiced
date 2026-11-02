#!/usr/bin/env python3
"""Build native additional cars from licensed prepared GLBs and owner retail data."""
import argparse,sys,re,json,io,copy
from pathlib import Path
import numpy as np
from PIL import Image
from gltf import Gltf
from import_ferrari import mesh_bytes,subset,node,name,pack,box
from retail import Retail
from package_overlay import recipe
from native_decals import transfer as transfer_native_decal
CONFIGS=[
 dict(key='integra',model='INTEGRA_DB8',id=54,label='INTEGRA DB8 TYPE R',prefix='db8',wb=262,donor='ac_integra',manufacturer='HONDA JAPAN',price=22000,mass=1100,torque=181.4,rpm='1000 2000 3000 4500 6000 7000 7500 8000 8500',curve='.4 .6 .75 .84 .92 .98 1 .967 0',paint='paint',glass='glass',brake='taillights',head='glasslight'),
 dict(key='bmw',model='BMW_E36',id=55,label='BMW M3 E36',prefix='e36',wb=270,donor='350z',manufacturer='NONE EUROPE',price=29000,mass=1441,torque=320,rpm='1000 2000 3000 3800 5000 6000 6800',curve='.55 .75 .95 1 .98 .89 0',paint='CarPaint',glass='Windows',brake='RearLights',head='HeadLights'),
 dict(key='levin',model='LEVIN_AE92',id=56,label='COROLLA LEVIN AE92',prefix='ae92',wb=243,donor='corolla',manufacturer='TOYOTA JAPAN',price=12500,mass=1030,torque=147.1,rpm='1000 2000 3000 4500 6000 7200 7800',curve='.4 .6 .8 .95 1 .93 0',paint='body',glass='WindowGlass',brake='TaillightGlass',head='HeadlightGlass')]

# Their rear/high painted surfaces include the boot and hatch surrounds, not
# just a detachable wing. Keep them in the chassis when parts are refitted.
for config in CONFIGS:
 config['fixed_stock_spoiler']=True

# Authoring identities, checked against the retained source assemblies. A hood
# is a complete panel, not a rectangle cut from the car's bounding box.
BONNET_SOURCES={
 'golf-mk5-r32':('/Hood_Sub_',),
 'volvo-v70':('/xhood_0',),
 'peugeot-406-coupe':('/Peugeot_406_color-material.014',),
 'jetta-vento-mk5':('/jettaa5_body-material.010',),
 'subaru-outback-2002':('/Hood_Paint_',),
 'levin':('/Bonnet2_Door.',),
}

# These sources fuse the hood into the body mesh. Explicit source-space panel
# outlines replace centroid selection (which leaves stair-step carbon edges).
BONNET_OUTLINES={
 'peugeot-405-glx':[(-.68,.72),(.68,.72),(.73,1.95),(.64,2.10),(-.64,2.10),(-.73,1.95)],
 # The source hood continues forward past z2.08 over the headlamp/grille lip.
 # Its retained paint mesh ends at z2.283699; the old crop left a20cm blue band.
 'volvo-240':[(-.70,.72),(.70,.72),(.73,2.08),(.73,2.284),(-.73,2.284),(-.73,2.08)],
 'integra':[(-.46,.52),(.46,.52),(.51,1.30),(.44,1.43),(-.44,1.43),(-.51,1.30)],
}

def split_bonnet(part,outline,height):
 inside=[];outside=[]
 outline=np.array(outline)
 area=sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(outline,np.roll(outline,-1,axis=0)))
 direction=1 if area>0 else -1
 for tri in part['tri']:
  polygon=[np.r_[part['pos'][i],part['norm'][i],part['uv'][i]] for i in tri]
  if part['norm'][tri].mean(0)[1]<.3 or part['pos'][tri].mean(0)[1]<height*.4:
   outside.extend(polygon);continue
  for a,b in zip(outline,np.roll(outline,-1,axis=0)):
   yes=[];no=[];edge=b-a
   for p,q in zip(polygon,polygon[1:]+polygon[:1]):
    vp=p[[0,2]]-a;vq=q[[0,2]]-a
    dp=direction*(edge[0]*vp[1]-edge[1]*vp[0]);dq=direction*(edge[0]*vq[1]-edge[1]*vq[0])
    (yes if dp>=0 else no).append(p)
    if (dp>=0)!=(dq>=0):v=p+(q-p)*dp/(dp-dq);yes.append(v);no.append(v)
   for i in range(1,len(no)-1):outside.extend([no[0],no[i],no[i+1]])
   polygon=yes
   if not polygon:break
  for i in range(1,len(polygon)-1):inside.extend([polygon[0],polygon[i],polygon[i+1]])
 result=[]
 for points,panel in [(inside,True),(outside,False)]:
  if not points:continue
  a=np.array(points);q=dict(part);q.update(pos=a[:,:3],norm=a[:,3:6],uv=a[:,6:8],tri=np.arange(len(a)).reshape(-1,3),bonnet=panel);result.append(q)
 return result

def carbon_texture():
 # A repeating weave without mipmaps aliases into large moving moire bands.
 yy,xx=np.indices((128,128));weave=26+(((xx//4+yy//4)%2)*8)
 pixels=np.empty((128,128,4),dtype=np.uint8);pixels[:,:,:3]=weave[:,:,None];pixels[:,:,3]=255
 levels=[];im=Image.fromarray(pixels)
 while True:
  levels.append(np.asarray(im)[:,:,[2,1,0,3]].tobytes())
  if im.width==1:break
  im=im.resize((im.width//2,im.height//2),Image.Resampling.BOX)
 return pack('9IfI',21,128,128,len(levels),2,2,2,1,1,0.,4)+b''.join(levels)

def component_bonnet(part,height,front,width):
 # The E36 OBJ merges materials but retains disconnected physical panels.
 # Weld duplicate normal/UV vertices only for connectivity, not for export.
 _,inverse=np.unique(np.round(part['pos'],4),axis=0,return_inverse=True)
 parent=list(range(int(inverse.max())+1))
 def root(i):
  while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
  return i
 for tri in inverse[part['tri']]:
  a=root(tri[0])
  for b in tri[1:]:parent[root(b)]=a
 groups={}
 for i,tri in enumerate(inverse[part['tri']]):groups.setdefault(root(tri[0]),[]).append(i)
 selected=np.zeros(len(part['tri']),dtype=bool)
 for ids in groups.values():
  v=part['pos'][part['tri'][ids]].reshape(-1,3);lo,hi=v.min(0),v.max(0)
  if lo[2]>front*.3 and lo[1]>height*.4 and hi[0]-lo[0]>width and lo[0]<0<hi[0]:selected[ids]=True
 if not selected.any():raise ValueError('Source bonnet panel not found')
 return selected

def cage(lo,hi):
 x=min(abs(lo[0]),hi[0])*.92;y0=lo[1]+12;roof=hi[1]*.96;z0=lo[2];z1=hi[2]
 profile=[(z1,y0),(z1,roof*.48),(z1*.42,roof*.62),(z1*.10,roof),(z0*.38,roof),(z0*.70,roof*.62),(z0,roof*.55),(z0,y0)]
 verts=np.array([[side,y,z] for side in (-x,x) for z,y in profile]);faces=[]
 for i in range(1,7):faces.extend([(0,i+1,i),(8,8+i,8+i+1)])
 for i in range(8):k=(i+1)%8;faces.extend([(i,k,k+8),(i,k+8,i+8)])
 out=pack('I',len(verts))+b''.join(pack('4f',*v,1) for v in verts)+pack('I',len(faces));center=verts.mean(0)
 for f in faces:
  a,b,c=verts[list(f)];normal=np.cross(b-a,c-a);normal/=np.linalg.norm(normal)
  if normal@(a-center)<0:f=(f[0],f[2],f[1]);normal=-normal
  out+=pack('4I4f',*f,0,*normal,-float(normal@a))
 out+=pack('I',len(faces))+pack(f'{len(faces)}I',*range(len(faces)))
 return out+pack('I4ff12xiiII',1,*center,1,float(np.linalg.norm(verts-center,axis=1).max()),-1,-1,0,len(faces))


def build(c,args,r):
 g=Gltf(args.sources/(c['key']+'-prepared.glb'));ps=g.parts();mats=g.j['materials'];prefix=c['prefix'];model=c['model'].lower()
 ps=[p for p in ps if mats[p['mi']].get('name')!='Scene']
 def wheel(p):
  s=p['source'].lower()
  # The V70 source calls a rear body seam "whitexgaps". It was previously
  # renamed as a wheel simply because its name starts with w. Including it
  # in the wheel bounds moves the rear axle and changes the entire car scale.
  if 'whitexgaps' in s:return False
  if c['key']=='bmw':return mats[p['mi']].get('name') in ['Tire.001','SeconadryColor.001','Textures.001','MainColor.001']
  return bool(re.search(r'/wheel(?:fl|fr|rl|rr|_[rl][fr])(?:/|_|$)',s))
 wp=[p for p in ps if wheel(p)];assert wp,'No source wheels'
 # Derive centres from actual tyre geometry, not author object pivots.
 tyres=[p for p in wp if any(k in p['source'].lower() for k in ['_tire','_tyre','tire_','tyre_']) or mats[p['mi']].get('name','').lower() in ['rubber_oj_rubber','oj_rubber']]
 if not tyres:tyres=[p for p in wp if any(k in mats[p['mi']].get('name','').lower() for k in ['rubber','tire','tyre'])]
 assert tyres
 vertices=np.concatenate([p['pos'] for p in tyres]);midz=(vertices[:,2].min()+vertices[:,2].max())/2
 centres={}
 for key,xsign,zsign in [('FL',1,1),('FR',-1,1),('RL',1,-1),('RR',-1,-1)]:
  v=vertices[(vertices[:,0]*xsign>0)&((vertices[:,2]-midz)*zsign>0)];centres[key]=(v.min(0)+v.max(0))/2
 scale=c['wb']/(centres['FL'][2]-centres['RL'][2]);offset=np.array([0,vertices[:,1].min(),(centres['FL'][2]+centres['RL'][2])/2]);sign=np.array([-1,1,1])
 for p in ps:
  p['pos']=(p['pos']-offset)*scale*sign;p['norm']*=sign;p['tri']=p['tri'][:,[0,2,1]];p['mat']=(model+'_body' if mats[p['mi']].get('name')==c['paint'] else model+'_glass' if mats[p['mi']].get('name')==c['glass'] else prefix+'_m'+str(p['mi']))
 centres={k:(v-offset)*scale*sign for k,v in centres.items()}
 body=[p for p in ps if not wheel(p)];v=np.concatenate([p['pos'] for p in body]);lo,hi=v.min(0),v.max(0);height=hi[1];front=centres['FL'][2];rear=centres['RL'][2]
 if c['key'] in BONNET_OUTLINES:
  outline=[((-x-offset[0])*scale,(z-offset[2])*scale) for x,z in BONNET_OUTLINES[c['key']]]
  body=[q for p in body for q in (split_bonnet(p,outline,height) if mats[p['mi']].get('name')==c['paint'] else [p])]
 root=args.output
 def write(path,data):
  p=root/path.lower();p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data.encode() if isinstance(data,str) else data)
 def cf(path,data):
  for suffix in ['', '_ui']:write('cars/'+model+suffix+'/'+path,data)
 def tex(n,pixels):
  pixels=np.asarray(pixels,dtype=np.uint8);h,w=pixels.shape[:2]
  fmt=21;payload=pixels[:,:,[2,1,0,3]].tobytes()
  if w>=4 and h>=4 and w%4==0 and h%4==0:
   codec='DXT1' if np.all(pixels[:,:,3]==255) else 'DXT5';stream=io.BytesIO();Image.fromarray(pixels).save(stream,format='DDS',pixel_format=codec);payload=stream.getvalue()[128:];fmt=int.from_bytes(codec.encode(),'little')
  cf('textures/'+n+'.ddt',pack('9IfI',fmt,w,h,1,2,2,0,1,1,0.,1)+payload)
 def solid(n,color):tex(n,np.array([[color]],dtype=np.uint8))
 solid(prefix+'_paint',[255,255,255,0]);solid(prefix+'_scratch',[140,140,140,0])
 for mi,m in enumerate(mats):
  mn=m.get('name','');n=(model+'_body' if mn==c['paint'] else model+'_glass' if mn==c['glass'] else prefix+'_m'+str(mi));pbr=m.get('pbrMetallicRoughness',{});factor=pbr.get('baseColorFactor',[1,1,1,1]);ti=pbr.get('baseColorTexture',{}).get('index')
  dark_kn5=c.get('key')=='golf-mk5-r32' and ('_blk' in mn or mn in ['black','ext_windowmask'])
  if dark_kn5:factor=[factor[0]*.035,factor[1]*.035,factor[2]*.035,factor[3]]
  if ti is not None:
   im=g.j['images'][g.j['textures'][ti]['source']];bv=g.j['bufferViews'][im['bufferView']];raw=g.bin[bv.get('byteOffset',0):bv.get('byteOffset',0)+bv['byteLength']];pic=Image.open(io.BytesIO(raw)).convert('RGBA');limit=2048 if c['key']=='subaru-outback-2002' else 1024;pic.thumbnail((limit,limit));pixels=np.array(pic).astype(float)*factor;pixels=np.clip(pixels,0,255).astype(np.uint8)
  else:pixels=np.array([[np.array(factor)*255]],dtype=np.uint8)
  if mn==c['glass']:pixels=np.array([[[75,85,95,85]]],dtype=np.uint8)
  opaque_lens=c['key']=='volvo-240' and mn==c['head']
  if opaque_lens:pixels[:,:,3]=255
  masked=m.get('alphaMode','OPAQUE')=='MASK' and not opaque_lens
  if masked:
   # Preserve glTF's cutout rather than treating its back-facing double-sided
   # planes as Coloured Spec ABlend, which has no Double-sided parameter.
   cutoff=m.get('alphaCutoff',.5)*255
   pixels[:,:,3]=np.where(pixels[:,:,3]>=cutoff,255,0).astype(np.uint8)
  tex(n,pixels)
  if mn==c['paint']:
   material=f'''Material Program = Car Body\nCar Colour = 220 220 220\nSide Fresnel = 0.18\nFront Fresnel = 0.02\nMetallic Colour = 20 20 20\nPearlescent Colour = 0 0 0\nMetallic Shininess = 25\nPearlescent Shininess = 0\nMetallic = FALSE\nDouble-sided = FALSE\nPaint Work = {prefix}_paint.ptd\nScratch & Reflect = {prefix}_scratch.ptd\n'''
  elif mn==c['glass']:
   material=f'Material Program = Car Glass\nSide Fresnel = 0.12\nFront Fresnel = 0.02\nBase Opacity = 0.28\nStart Fade = 0\nEnd Fade = 10000\nColour = 110 120 130\nTexture = {n}.ptd\nScratch = {prefix}_scratch.ptd\n'
  else:
   alpha=m.get('alphaMode','OPAQUE')!='OPAQUE' and not opaque_lens
   if masked:
    material=f'Material Program = Specular ATest\nSpecular Colour = 100 100 100\nSpecular Highlight = 50\nShininess = 32\nDouble-sided = {str(m.get("doubleSided",False)).upper()}\nOpaque Texture = {n}.ptd\n'
   else:
    material=f'Material Program = Coloured Spec {"ABlend" if alpha else "Opaque"}\nColour = 255 255 255\nSpecular Colour = 100 100 100\nSpecular Highlight = 50\nShininess = 32\nDouble-sided = {str(m.get("doubleSided",False)).upper()}\nTexture = {n}.ptd\n'
   if c.get('source_two_sided') and not alpha and not masked:
    # Coloured Spec ignores Double-sided; use the native material which implements it.
    material=f'Material Program = Specular Opaque\nSpecular Colour = 100 100 100\nSpecular Highlight = 50\nShininess = 32\nDouble-sided = {str(m.get("doubleSided",False)).upper()}\nOpaque Texture = {n}.ptd\n'
   if dark_kn5:material=material.replace('Specular Colour = 100 100 100','Specular Colour = 20 20 20').replace('Specular Highlight = 50','Specular Highlight = 8')
  cf('mesh/mats/'+n+'.pmd',material)
 for suffix,color in [('stop','255 25 12'),('head','225 230 240')]:cf('mesh/mats/'+prefix+'_'+suffix+'.pmd','Material Program = Constant Colour\nColour = '+color+'\n')
 # Compact per-material meshes, preventing hundreds of native draw calls.
 report={}
 def save(n,parts):
  parts=[p for p in parts if p is not None and p['tri'].size];assert parts,'Empty mesh '+n
  merged=[]
  for mat in sorted(set(p['mat'] for p in parts)):
   same=[p for p in parts if p['mat']==mat];verts=[];norm=[];uv=[];tri=[];offset=0
   for p in same:
    if offset+len(p['pos'])>=65000 and verts:
     merged.append(dict(mat=mat,pos=np.concatenate(verts),norm=np.concatenate(norm),uv=np.concatenate(uv),tri=np.concatenate(tri)));verts=[];norm=[];uv=[];tri=[];offset=0
    verts.append(p['pos']);norm.append(p['norm']);uv.append(p['uv']);tri.append(p['tri']+offset);offset+=len(p['pos'])
   merged.append(dict(mat=mat,pos=np.concatenate(verts),norm=np.concatenate(norm),uv=np.concatenate(uv),tri=np.concatenate(tri)))
  cf('mesh/'+n+'.d8m',mesh_bytes(merged));report[n]=sum(p['tri'].size//3 for p in merged)
 zones={k:[] for k in ['CHASS','FBUMP','RBUMP','BONNET','SPOILER','SSKIRT_L','SSKIRT_R','BRKLIGHT','LIGHTSON']}
 exhaustparts=[]
 for p in body:
  if '/exhaust/' in p['source'].lower() or mats[p['mi']].get('name')=='Exhaust' or (c['key']=='levin' and re.search(r'/RearBarHatch1_Door\.00[5679]/|/RearBarHatch1_Door\.010/',p['source'])):
   exhaustparts.append(p);continue
  cent=p['pos'][p['tri']].mean(1);x,y,z=cent.T;norm=p['norm'][p['tri']].mean(1);mn=mats[p['mi']].get('name','');keys=np.full(len(cent),'CHASS',dtype='<U12')
  if mn!=c['glass']:
   keys[(z>front+28)&(y<height*.53)]='FBUMP';keys[(z<rear-28)&(y<height*.53)]='RBUMP'
   keys[(abs(x)>max(abs(lo[0]),hi[0])*.76)&(y<height*.23)&(z>rear)&(z<front)]=np.where(x[(abs(x)>max(abs(lo[0]),hi[0])*.76)&(y<height*.23)&(z>rear)&(z<front)]<0,'SSKIRT_L','SSKIRT_R')
  if mn==c['paint']:
   if c['key']=='bmw':keys[component_bonnet(p,height,front,hi[0])]='BONNET'
   elif c['key'] not in BONNET_SOURCES and c['key'] not in BONNET_OUTLINES:
    keys[(z>front*.45)&(z<hi[2]-20)&(y>height*.40)&(norm[:,1]>.45)&(abs(x)<hi[0]*.77)]='BONNET'
   # A geometric rear/high cutoff also catches boot lids and rear doors.
   # Sources without explicitly separated detachable wings keep their stock body intact.
   if not c.get('fixed_stock_spoiler',False):
    keys[(z<rear*.95)&(y>height*.65)]='SPOILER'
  if p.get('bonnet') or any(token in p['source'] for token in BONNET_SOURCES.get(c['key'],())):
   keys[:]='BONNET'
  if mn.startswith(c['brake']) or mn==c['head']:
   q=copy.deepcopy(p);q['pos']+=q['norm']*.08;q['mat']=prefix+('_stop' if mn.startswith(c['brake']) else '_head');zones['BRKLIGHT' if mn.startswith(c['brake']) else 'LIGHTSON'].append(q)
  for key in np.unique(keys):zones[key].append(subset(p,keys==key))
 # Fixed stock rear bodywork stays in CHASS, including any integral factory wing.
 for k,parts in zones.items():
  if parts:save(prefix+'_'+k,parts)
 # One shared wheel, selected from the actual front left; native engine mirrors right.
 wheelparts=[]
 for p in wp:
  cent=p['pos'][p['tri']].mean(1);q=subset(p,(cent[:,0]<0)&(cent[:,2]>0))
  if q is not None:q['pos']-=centres['FL'];wheelparts.append(q)
 save(prefix+'_wheel',wheelparts);save(prefix+'_wheel_blur',wheelparts)
 # Mild fitted aero options; carbon bonnet retains actual source shape.
 carbon=prefix+'_carbon';cf('textures/'+carbon+'.ddt',carbon_texture())
 cf('mesh/mats/'+carbon+'.pmd',f'Material Program = Coloured Spec Opaque\nColour = 255 255 255\nSpecular Colour = 110 110 110\nSpecular Highlight = 80\nShininess = 48\nTexture = {carbon}.ptd\n')
 for k in ['FBUMP','RBUMP','BONNET','SSKIRT_L','SSKIRT_R','SPOILER']:
  if not zones[k]:continue
  sport=copy.deepcopy(zones[k])
  if k=='BONNET':
   for p in sport:
    if p['mat']==model+'_body':
     p['mat']=carbon;p['uv']=p['pos'][:,[0,2]]/8
  for p in sport:
   if k=='BONNET':continue
   elif k=='SPOILER':
    base=min(q['pos'][:,1].min() for q in zones[k]);top=max(q['pos'][:,1].max() for q in zones[k]);p['pos'][:,1]+=4*np.clip((p['pos'][:,1]-base)/max(top-base,1),0,1)
   else:
    lower=np.clip((height*.42-p['pos'][:,1])/(height*.22),0,1);p['pos'][:,1]-=lower*2
    if k in ['FBUMP','RBUMP']:p['pos'][:,2]+=lower*(2 if k=='FBUMP' else -2)
    elif k.startswith('SSKIRT'):p['pos'][:,0]+=lower*(-1.5 if k.endswith('L') else 1.5)
  save(prefix+'_sport_'+k,sport)
 # Optional Blender-authored extensions use the same source coordinates as the stock GLB.
 # Keep the original assembly in each replacement so fitting a kit cannot remove lamps/grilles.
 kit_count=c.get('kit_count',0)
 if kit_count:
  kit_trim=prefix+'_kit_trim'
  cf('mesh/mats/'+kit_trim+'.pmd','Material Program = Constant Colour\nColour = 25 25 27\n')
  for level in range(1,kit_count+1):
   kg=Gltf(args.sources/(c['key']+'-kit'+str(level)+'.glb'))
   additions={k:[] for k in ['FBUMP','RBUMP','SSKIRT_L','SSKIRT_R']}
   for p in kg.parts():
    source=p['source'].upper()
    if 'FRONT_' in source:key='FBUMP'
    elif 'REAR_' in source:key='RBUMP'
    elif 'SKIRT_1_' in source:key='SSKIRT_L'
    elif 'SKIRT_-1_' in source:key='SSKIRT_R'
    else:raise ValueError('Unclassified kit part '+source)
    p['pos']=(p['pos']-offset)*scale*sign;p['norm']*=sign;p['tri']=p['tri'][:,[0,2,1]]
    p['mat']=model+'_body' if kg.j['materials'][p['mi']].get('name')==c['paint'] else kit_trim
    additions[key].append(p)
   for key,extra in additions.items():
    if not extra:raise ValueError(f'Missing kit {level} {key}')
    save(prefix+'_kit'+str(level)+'_'+key,copy.deepcopy(zones[key])+extra)
 paint=[p for p in body if mats[p['mi']].get('name')==c['paint']]
 decal_shell=paint
 if c['key']=='subaru-outback-2002':
  # The lower doors are factory cladding, not missing bodywork. Vinyl must
  # continue across those exterior surfaces instead of losing its lower half.
  decal_shell=list(paint)
  for p in body:
   if mats[p['mi']].get('name','').startswith('Subaru_Outback'):
    cent=p['pos'][p['tri']].mean(1)
    q=subset(p,(abs(cent[:,0])>max(abs(lo[0]),hi[0])*.75)&(cent[:,1]<height*.75)&(cent[:,1]>height*.15))
    if q is not None:decal_shell.append(q)
 defs=r.read('scripts/cardefs.txt').decode('latin1');focus=re.search(r'\[BeginCar\]\s+focus_2\b(.*?)\[EndCar\]',defs,re.S|re.I).group(1);decals=re.findall(r'^\s*CUSTOM\s+DECAL\s+FOCUS_2_(DECAL_\w+)',focus,re.M|re.I)
 for decal in decals:
  save(prefix+'_'+decal,transfer_native_decal(r,decal,decal_shell,c['wb'],height,decal.lower()))
 # Clip sponsor stickers to small rectangles on actual door surfaces.
 sponsors=[]
 for index in range(9):
  zlo=-40+(index%3)*28;zhi=zlo+26;ylo=height*.29+(index//3)*10;yhi=ylo+9
  material='decal_spons_'+str(index+1)
  cf('mesh/mats/'+material+'.pmd','Material Program = Coloured Spec ABlend\nColour = 255 255 255\nTexture = alpha2.ptd\n')
  for p in paint:
   pos=p['pos'];norm=p['norm'];corners=[]
   for tri in p['tri']:
    if abs(norm[tri].mean(0)[0])<.7:continue
    polygon=[np.r_[pos[i],norm[i]] for i in tri]
    for axis,bound,sign in [(2,zlo,1),(2,zhi,-1),(1,ylo,1),(1,yhi,-1)]:
     result=[]
     for a,b in zip(polygon,polygon[1:]+polygon[:1]):
      da=(a[axis]-bound)*sign;db=(b[axis]-bound)*sign
      if da>=0:result.append(a)
      if (da>=0)!=(db>=0):result.append(a+(b-a)*da/(da-db))
     polygon=result
     if not polygon:break
    for k in range(1,len(polygon)-1):corners.extend([polygon[0],polygon[k],polygon[k+1]])
   if corners:
    a=np.array(corners);v=a[:,:3];n=a[:,3:];sponsors.append(dict(mat=material,pos=v+n*.15,norm=n,uv=np.c_[(zhi-v[:,2])/26,(yhi-v[:,1])/9],tri=np.arange(len(v)).reshape(-1,3)))
 save(prefix+'_sponsors',sponsors)
 # The native shared exhaust slot uses original source geometry when identifiable.
 if exhaustparts:
  ev=np.concatenate([part['pos'] for part in exhaustparts]);origin=np.array([(ev[:,0].min()+ev[:,0].max())/2,25,lo[2]+45]);local=copy.deepcopy(exhaustparts)
  for part in local:part['pos']-=origin
  write('carmesh/'+prefix+'_exhaust.d8m',mesh_bytes(local))
  for mat in set(p['mat'] for p in local):
   write('carmesh/mats/'+mat+'.pmd',(root/'cars'/model/'mesh/mats'/(mat+'.pmd')).read_bytes())
   write('cartextures/'+mat+'.ddt',(root/'cars'/model/'textures'/(mat+'.ddt')).read_bytes())
 children=[node(prefix+'_'+k,prefix+'_'+k) for k in ['BRKLIGHT','LIGHTSON'] if zones[k]]
 children += [node(k) for k in ['FBUMP','RBUMP','BONNET','SPOILER','SSKIRT_L','SSKIRT_R','DECAL']]
 children += [node(prefix+'_SPONS',prefix+'_sponsors')]
 children += [node('DRIVER',pos=(-32,height*.66,front*.05)),node('BONNET_CAM',pos=(0,height*.63,front*.95)),node('EXHAUST_S',pos=origin if exhaustparts else (lo[0]*.45,25,lo[2]+45))]
 for tag,z,y,texture in [('HLIGHTON',hi[2]-12,height*.46,'car_flare01'),('RBRKLIGHTON',lo[2]+5,height*.52,'car_flare_brk')]:
  for side in [-1,1]:
   m=np.eye(4);m[:3,3]=(hi[0]*.72*side,y,z)
   if tag.startswith('R'):m[0,0]=m[2,2]=-1
   props=(f'LIGHTFLARE={texture}\nLIGHTFLARE_SIZE=260\nLIGHTFLARE_BRIGHTNESS=0.6\nLIGHTFLARE_FALLOFF_MIN=0\nLIGHTFLARE_FALLOFF_MAX=60\nLIGHTFLARE_OFFSET=5\n').encode();children.append(name(prefix+'_'+tag+str(side))+pack('16f',*m.ravel())+pack('II',2,len(props))+props+pack('I',0))
 cf('models/'+model+'_stock.boh',node(prefix+'_ROOT',children=[node(prefix+'_CHASS',prefix+'_CHASS',children=children)]+[node('WHEEL_'+k,pos=v) for k,v in centres.items()]))
 for suffix in ['shad','collidee']:cf('mesh/'+model+'_chass_'+suffix+'.ocd',cage(lo,hi))
 donor=re.search(r'\[BeginCar\]\s+'+c['donor']+r'\b(.*?)\[EndCar\]',defs,re.S|re.I).group(1)
 sound=re.search(r'ENGINE_SOUND\s+(\w+)',donor).group(1);exhaust=re.search(r'STANDARDEXHAUST\s+"([^"]+)"',donor).group(1)
 if exhaustparts:exhaust=prefix+'_exhaust'
 block=[f'[BeginCar] {c["model"]} "{c["label"]}"',f'MANUFACTURER {c["manufacturer"]}',f'VALUE {c["price"]}','STATE USED']
 for cat,part in [('FBUMP','FBUMP'),('RBUMP','RBUMP'),('BONNET','BONNET'),('SPOILER','SPOILER'),('SKIRT','SSKIRT')]:
  if cat=='SPOILER' and not zones['SPOILER']:continue
  block += [f'STANDARD {cat} "{prefix}_{part}"']
  if kit_count and cat in ['FBUMP','RBUMP','SKIRT']:
   block += [f'CUSTOM {cat} "{prefix}_kit{level}_{part}" AERO' for level in range(1,kit_count+1)]
  else:block += [f'CUSTOM {cat} "{prefix}_sport_{part}" AERO']
 block += [f'CUSTOM DECAL "{prefix}_{d}" AERO' for d in decals]
 block += ['ALLOW_NONE DECAL']
 if zones['SPOILER']:block += ['ALLOW_NONE SPOILER']
 block += [f'STANDARDWHEELS "{prefix}_wheel"','MATCH_STOCK_WHEELS 1',f'STANDARDEXHAUST "{exhaust}"',f'ENGINE_SOUND {sound}','MAXDROPHEIGHT 4','[EndCar]']
 dynamics=r.read('scripts/dynamic_'+c['donor']+'.txt').decode('latin1');dynamics=re.sub(re.escape(c['donor']),model,dynamics,flags=re.I)
 changes=dict(Name=c['model'],Mass=str(c['mass']),MaxTorque=str(c['torque']),TorqueCurveX=c['rpm'],TorqueCurveY=c['curve'])
 # GT APEX 4A-GE: rated power at 7200 rpm, reverse 3.250 (Toyota catalog).
 # Native automatic shifts above 1.02 * PowerRangeMax. 7000 puts the shift
 # near rated power; 7200 stalls in fourth just below its 7344-rpm trigger.
 if c['key']=='levin':changes.update(ForwardGearRatios='3.166 1.904 1.310 0.969 0.815',ReverseGearRatio='-3.250',FinalDrive='4.312',PowerRangeMax='7000')
 if c['key']=='bmw':changes.update(ForwardGearRatios='4.20 2.49 1.66 1.24 1.00',FinalDrive='3.23',FrontWeightDistribution='0.50')
 radius=(vertices[:,1].max()-vertices[:,1].min())*scale/200;changes['WheelRadius']=str(round(radius,4))
 for k,value in changes.items():dynamics=re.sub(r'^'+k+r'[^\r\n]*',k+' '+value,dynamics,flags=re.M)
 for i,key in enumerate(['FL','FR','RL','RR']):
  x,y,z=centres[key];replacement=f'[BeginLineCollider]\r\nID {i}\r\nPoint {x:.4f} {y+12:.4f} {z:.4f}\r\nEnd {x:.4f} -7 {z:.4f}\r\n[EndLineCollider]';dynamics=re.sub(r'\[BeginLineCollider\]\s*ID '+str(i)+r'\b.*?\[EndLineCollider\]',replacement,dynamics,flags=re.S)
 write('scripts/dynamic_'+model+'.txt',dynamics)
 report.update(wheels={k:v.tolist() for k,v in centres.items()},bounds=[lo.tolist(),hi.tolist()],radius=radius,id=c['id'],audioDonor=sound)
 write('reports/'+model+'.json',json.dumps(report,indent=2));return '\n'.join(block)+'\n'

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--game',type=Path,required=True);p.add_argument('--sources',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);r=Retail(a.game)
 blocks=[build(c,a,r) for c in CONFIGS]
 for path,addition in [('cardefs.txt','\n'.join(blocks)),('mod_cars.txt','\n'.join(f'Car {c["model"]} {c["id"]} 2 {1 if c['key']=='bmw' else 0}' for c in CONFIGS)),('mod_unlocks.txt','\n'.join('Car '+c['model'] for c in CONFIGS)),('arcadeunlock.txt','\n'.join('Car '+c['model'] for c in CONFIGS)),('arcadecollection.txt','\n'.join(f'BeginCar {c["model"]}\nBaseColour 220 220 220\nMetallicColour 0 0 0\nEndCar' for c in CONFIGS))]:
  (a.output/'scripts').mkdir(exist_ok=True);(a.output/'scripts'/path).write_text('\n'+addition+'\n')
 print('Built three car overlays; catalogue fragments require additive packaging with Ferrari ID 53.')
