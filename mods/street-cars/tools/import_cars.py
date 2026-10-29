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
CONFIGS=[
 dict(key='integra',model='INTEGRA_DB8',id=54,label='INTEGRA DB8 TYPE R',prefix='db8',wb=262,donor='ac_integra',manufacturer='HONDA JAPAN',price=22000,mass=1100,torque=181.4,rpm='1000 2000 3000 4500 6000 7000 7500 8000 8500',curve='.4 .6 .75 .84 .92 .98 1 .967 0',paint='paint',glass='glass',brake='taillights',head='glasslight'),
 dict(key='bmw',model='BMW_E36',id=55,label='BMW M3 E36',prefix='e36',wb=270,donor='350z',manufacturer='NONE EUROPE',price=29000,mass=1441,torque=320,rpm='1000 2000 3000 3800 5000 6000 6800',curve='.55 .75 .95 1 .98 .89 0',paint='CarPaint',glass='Windows',brake='RearLights',head='HeadLights'),
 dict(key='levin',model='LEVIN_AE92',id=56,label='COROLLA LEVIN AE92',prefix='ae92',wb=243,donor='corolla',manufacturer='TOYOTA JAPAN',price=12500,mass=1030,torque=147.1,rpm='1000 2000 3000 4500 6000 7200 7800',curve='.4 .6 .8 .95 1 .93 0',paint='body',glass='WindowGlass',brake='TaillightGlass',head='HeadlightGlass')]

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

def carbon_skin(part, carbon, half_width, zmin, zmax):
 # Clip at a continuous inset outline, preserving paint outside; no centroid sawtooth.
 inside=[];outside=[]
 for tri in part['tri']:
  polygon=[np.r_[part['pos'][i],part['norm'][i],part['uv'][i]] for i in tri]
  for axis,bound,sign in [(0,-half_width,1),(0,half_width,-1),(2,zmin,1),(2,zmax,-1)]:
   yes=[];no=[]
   for a,b in zip(polygon,polygon[1:]+polygon[:1]):
    da=(a[axis]-bound)*sign;db=(b[axis]-bound)*sign
    (yes if da>=0 else no).append(a)
    if (da>=0)!=(db>=0):v=a+(b-a)*da/(da-db);yes.append(v);no.append(v)
   for k in range(1,len(no)-1):outside.extend([no[0],no[k],no[k+1]])
   polygon=yes
   if not polygon:break
  for k in range(1,len(polygon)-1):inside.extend([polygon[0],polygon[k],polygon[k+1]])
 result=[]
 for points,mat in [(inside,carbon),(outside,part['mat'])]:
  if not points:continue
  a=np.array(points);result.append(dict(mat=mat,pos=a[:,:3],norm=a[:,3:6],uv=a[:,[0,2]]/8 if mat==carbon else a[:,6:8],tri=np.arange(len(a)).reshape(-1,3)))
 return result

def build(c,args,r):
 g=Gltf(args.sources/(c['key']+'-prepared.glb'));ps=g.parts();mats=g.j['materials'];prefix=c['prefix'];model=c['model'].lower()
 ps=[p for p in ps if mats[p['mi']].get('name')!='Scene']
 def wheel(p):
  s=p['source'].lower()
  if c['key']=='bmw':return mats[p['mi']].get('name') in ['Tire.001','SeconadryColor.001','Textures.001','MainColor.001']
  return bool(re.search(r'/wheel(?:fl|fr|rl|rr|_[rl][fr])(?:/|_|$)',s))
 wp=[p for p in ps if wheel(p)];assert wp,'No source wheels'
 # Derive centres from actual tyre geometry, not author object pivots.
 tyres=[p for p in wp if any(k in mats[p['mi']].get('name','').lower() for k in ['rubber','tire','tyre'])];assert tyres
 vertices=np.concatenate([p['pos'] for p in tyres]);midz=(vertices[:,2].min()+vertices[:,2].max())/2
 centres={}
 for key,xsign,zsign in [('FL',1,1),('FR',-1,1),('RL',1,-1),('RR',-1,-1)]:
  v=vertices[(vertices[:,0]*xsign>0)&((vertices[:,2]-midz)*zsign>0)];centres[key]=(v.min(0)+v.max(0))/2
 scale=c['wb']/(centres['FL'][2]-centres['RL'][2]);offset=np.array([0,vertices[:,1].min(),(centres['FL'][2]+centres['RL'][2])/2]);sign=np.array([-1,1,1])
 for p in ps:
  p['pos']=(p['pos']-offset)*scale*sign;p['norm']*=sign;p['tri']=p['tri'][:,[0,2,1]];p['mat']=(model+'_body' if mats[p['mi']].get('name')==c['paint'] else model+'_glass' if mats[p['mi']].get('name')==c['glass'] else prefix+'_m'+str(p['mi']))
 centres={k:(v-offset)*scale*sign for k,v in centres.items()}
 body=[p for p in ps if not wheel(p)];v=np.concatenate([p['pos'] for p in body]);lo,hi=v.min(0),v.max(0);height=hi[1];front=centres['FL'][2];rear=centres['RL'][2]
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
   im=g.j['images'][g.j['textures'][ti]['source']];bv=g.j['bufferViews'][im['bufferView']];raw=g.bin[bv.get('byteOffset',0):bv.get('byteOffset',0)+bv['byteLength']];pic=Image.open(io.BytesIO(raw)).convert('RGBA');pic.thumbnail((1024,1024));pixels=np.array(pic).astype(float)*factor;pixels=np.clip(pixels,0,255).astype(np.uint8)
  else:pixels=np.array([[np.array(factor)*255]],dtype=np.uint8)
  if mn==c['glass']:pixels=np.array([[[75,85,95,85]]],dtype=np.uint8)
  tex(n,pixels)
  if mn==c['paint']:
   material=f'''Material Program = Car Body\nCar Colour = 220 220 220\nSide Fresnel = 0.18\nFront Fresnel = 0.02\nMetallic Colour = 20 20 20\nPearlescent Colour = 0 0 0\nMetallic Shininess = 25\nPearlescent Shininess = 0\nMetallic = FALSE\nDouble-sided = FALSE\nPaint Work = {prefix}_paint.ptd\nScratch & Reflect = {prefix}_scratch.ptd\n'''
  elif mn==c['glass']:
   material=f'Material Program = Car Glass\nSide Fresnel = 0.12\nFront Fresnel = 0.02\nBase Opacity = 0.28\nStart Fade = 0\nEnd Fade = 10000\nColour = 110 120 130\nTexture = {n}.ptd\nScratch = {prefix}_scratch.ptd\n'
  else:
   alpha=m.get('alphaMode','OPAQUE')!='OPAQUE'
   material=f'Material Program = Coloured Spec {"ABlend" if alpha else "Opaque"}\nColour = 255 255 255\nSpecular Colour = 100 100 100\nSpecular Highlight = 50\nShininess = 32\nDouble-sided = {str(m.get("doubleSided",False)).upper()}\nTexture = {n}.ptd\n'
   if c.get('source_two_sided') and not alpha:
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
   keys[(z>front*.45)&(z<hi[2]-20)&(y>height*.40)&(norm[:,1]>.45)&(abs(x)<hi[0]*.77)]='BONNET'
   # A geometric rear/high cutoff also catches boot lids and rear doors.
   # Sources without explicitly separated detachable wings keep their stock body intact.
   if not c.get('fixed_stock_spoiler',False):
    keys[(z<rear*.95)&(y>height*.65)]='SPOILER'
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
 carbon=prefix+'_carbon';yy,xx=np.indices((128,128));weave=24+(((xx//4+yy//4)%2)*12);pix=np.empty((128,128,4),dtype=np.uint8);pix[:,:,:3]=weave[:,:,None];pix[:,:,3]=255;tex(carbon,pix)
 cf('mesh/mats/'+carbon+'.pmd',f'Material Program = Coloured Spec Opaque\nColour = 255 255 255\nSpecular Colour = 110 110 110\nSpecular Highlight = 80\nShininess = 48\nTexture = {carbon}.ptd\n')
 for k in ['FBUMP','RBUMP','BONNET','SSKIRT_L','SSKIRT_R','SPOILER']:
  if not zones[k]:continue
  sport=copy.deepcopy(zones[k])
  if k=='BONNET':
   sport=[q for p in sport for q in (carbon_skin(p,carbon,hi[0]*.59,front*.58,hi[2]-36) if p['mat']==model+'_body' else [p])]
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
 # UV projections use the real painted shell, with bonnet and sides independently mapped.
 def skin(mat):
  parts=[]
  for p in paint:
   cent=p['pos'][p['tri']].mean(1);normal=p['norm'][p['tri']].mean(1)
   for side in [-1,1]:
    q=subset(p,(normal[:,0]*side>.55)&(cent[:,1]<height*.68)&(cent[:,1]>height*.18))
    if q is not None:q['mat']=mat;q['uv']=np.c_[(hi[2]-q['pos'][:,2])/(hi[2]-lo[2]),(height*.68-q['pos'][:,1])/(height*.5)];q['pos']+=q['norm']*.12;parts.append(q)
   q=subset(p,(normal[:,1]>.65)&(cent[:,2]>front*.45)&(cent[:,2]<hi[2]-20)&(abs(cent[:,0])<hi[0]*.77))
   if q is not None:q['mat']=mat;q['uv']=np.c_[(q['pos'][:,0]+hi[0]*.77)/(hi[0]*1.54),(hi[2]-20-q['pos'][:,2])/(hi[2]-20-front*.45)];q['pos']+=q['norm']*.12;parts.append(q)
  return parts
 defs=r.read('scripts/cardefs.txt').decode('latin1');focus=re.search(r'\[BeginCar\]\s+focus_2\b(.*?)\[EndCar\]',defs,re.S|re.I).group(1);decals=re.findall(r'^\s*CUSTOM\s+DECAL\s+FOCUS_2_(DECAL_\w+)',focus,re.M|re.I)
 for decal in decals:save(prefix+'_'+decal,skin(decal.lower()))
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
 if c['key']=='levin':changes.update(ForwardGearRatios='3.166 1.904 1.310 0.969 0.815',FinalDrive='4.312')
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
