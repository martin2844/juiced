#!/usr/bin/env python3
"""Build an opt-in 458 test mod from the licensed source and the user's retail data.
Run decode.cjs first. No retail assets are modified or distributed by this tool.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import numpy as np
from retail import Retail
from ferrari_audio import build_audio


def pack(fmt, *v):
    return struct.pack('<' + fmt, *v)


def name(s):
    b = s.encode('ascii')
    assert len(b) < 32
    return b.ljust(32, b'\0')


def bounds(p):
    lo, hi = p.min(0), p.max(0)
    c = (lo + hi) / 2
    return pack('11f', *c, 1, float(np.linalg.norm(p-c, axis=1).max()), *lo, *hi)


def mesh_bytes(parts):
    """One index buffer, local indices and an independent vertex buffer per material."""
    parts = [p for p in parts if len(p['tri'])]
    index = b''.join(np.asarray(p['tri'], dtype='<u2').tobytes() for p in parts)
    out = pack('I', len(index)//2) + index + pack('I', len(parts))
    offset = 0
    for p in parts:
        count = p['tri'].size
        assert len(p['pos']) < 65536 and p['tri'].max() < len(p['pos'])
        out += name(p['mat']) + pack('III', offset, count, 1) + bounds(p['pos'])
        out += pack('ffII', 0, 1e12, 2, 5)
        for off, typ, usage in [(0,2,0), (12,2,3), (24,4,10), (28,1,5)]:
            out += pack('HHBBBB',0,off,typ,0,usage,0)
        out += pack('HHBBBB',255,0,17,0,0,0)
        out += pack('II',36,len(p['pos']))
        verts = np.zeros(len(p['pos']), dtype=[('p','<f4',3),('n','<f4',3),('c','<u4'),('uv','<f4',2)])
        verts['p'], verts['n'], verts['uv'] = p['pos'], p['norm'], p['uv']
        # Blue is dent response, alpha is scratch amount (initially undamaged).
        verts['c'] = 0x00ffffff
        out += verts.tobytes()
        offset += count
    out += bounds(np.concatenate([p['pos'] for p in parts]))
    return out


def subset(p, mask):
    tri = p['tri'][mask]
    if not len(tri):
        return None
    used, inverse = np.unique(tri, return_inverse=True)
    return dict(mat=p['mat'], pos=p['pos'][used].copy(), norm=p['norm'][used].copy(),
                uv=p['uv'][used].copy(), tri=inverse.reshape(-1,3))


def box(lo, hi, material):
    """Closed chamfered accessory; bevels catch light instead of razor-box edges."""
    pos=[]; normals=[]; uv=[]; tris=[]
    center=(np.array(lo)+hi)/2;half=(np.array(hi)-lo)/2;b=min(half)*.3
    def face(pts):
        pts=np.array(pts);n=np.cross(pts[1]-pts[0],pts[2]-pts[0]);n/=np.linalg.norm(n)
        if n@pts.mean(0)<0:pts=pts[::-1];n=-n
        base=len(pos);pos.extend(pts+center);normals.extend([n]*len(pts))
        axes=np.argsort(abs(n))[:2];uv.extend((pts+center)[:,axes]/12)
        tris.extend((base,base+i,base+i+1) for i in range(1,len(pts)-1))
    for axis in range(3):
        a,c=[i for i in range(3) if i!=axis]
        for sign in (-1,1):
            pts=[]
            for sa,sc in [(-1,-1),(1,-1),(1,1),(-1,1)]:
                p=np.zeros(3);p[axis]=sign*half[axis];p[a]=sa*(half[a]-b);p[c]=sc*(half[c]-b);pts.append(p)
            face(pts)
    for a,c in [(0,1),(0,2),(1,2)]:
        axis=3-a-c
        for sa in (-1,1):
            for sc in (-1,1):
                pts=[]
                for end,outer in [(-1,a),(-1,c),(1,c),(1,a)]:
                    p=np.zeros(3);p[axis]=end*(half[axis]-b)
                    p[a]=sa*(half[a]-(0 if outer==a else b));p[c]=sc*(half[c]-(0 if outer==c else b));pts.append(p)
                face(pts)
    for sx in (-1,1):
        for sy in (-1,1):
            for sz in (-1,1):
                signs=np.array([sx,sy,sz]);pts=[]
                for axis in range(3):
                    p=half-b;p[axis]+=b;pts.append(p*signs)
                face(pts)
    return dict(mat=material,pos=np.array(pos),norm=np.array(normals),uv=np.array(uv),tri=np.array(tris))


def node(n, mesh='NONE', pos=(0,0,0), children=()):
    m=np.eye(4);m[:3,3]=pos
    return name(n)+pack('16f',*m.ravel())+pack('I',0)+name(mesh)+pack('I',len(children))+b''.join(children)


def transform(n):
    if 'matrix' in n:
        return np.array(n['matrix']).reshape(4,4,order='F')
    x,y,z,w=n.get('rotation',(0,0,0,1));m=np.eye(4)
    m[:3,:3]=np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                      [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                      [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]) @ np.diag(n.get('scale',(1,1,1)))
    m[:3,3]=n.get('translation',(0,0,0))
    return m


def collision_mesh():
    # Convex low-poly collision cage following the bumper, windscreen, roof and rear deck.
    # Native OCD planes use n dot p + w = 0. One leaf holds all faces.
    profile=[(218,23),(218,61),(82,82),(30,119),(-70,119),(-145,84),(-220,72),(-220,23)]
    verts=np.array([[x,y,z] for x in (-89,89) for z,y in profile],dtype=float)
    faces=[]
    for i in range(1,7):faces.extend([(0,i+1,i),(8,8+i,8+i+1)])
    for i in range(8):
        k=(i+1)%8;faces.extend([(i,k,k+8),(i,k+8,i+8)])
    out=pack('I',len(verts))+b''.join(pack('4f',*v,1) for v in verts)+pack('I',len(faces))
    center=verts.mean(0)
    for f in faces:
        a,b,c=verts[list(f)];normal=np.cross(b-a,c-a);normal/=np.linalg.norm(normal)
        if normal @ (a-center)<0:
            f=(f[0],f[2],f[1]);normal=-normal
        out+=pack('4I4f',*f,0,*normal,-float(normal@a))
    out+=pack('I',len(faces))+pack(f'{len(faces)}I',*range(len(faces)))
    out+=pack('I4ff12xiiII',1,*center,1,float(np.linalg.norm(verts-center,axis=1).max()),-1,-1,0,len(faces))
    return out


def build(args):
    j=json.loads(args.decoded.read_text()); r=Retail(args.game)
    root=args.output.resolve(); root.mkdir(parents=True,exist_ok=True)
    def write(path,data):
        p=root/path.lower();p.parent.mkdir(parents=True,exist_ok=True)
        p.write_bytes(data.encode() if isinstance(data,str) else data)
    def carfile(path,data):
        for folder in ('ferrari_458','ferrari_458_ui'):write('cars/'+folder+'/'+path,data)
    parts=[]; worlds={}; wheelparts=[]
    def walk(i,parent,wheel=None):
        n=j['nodes'][i];world=parent@transform(n);worlds[i]=world
        if n['name'].startswith('wheel_'):wheel=n['name']
        if 'mesh' in n:
            for prim in j['meshes'][n['mesh']]['primitives']:
                a=prim['decoded'];pos=np.array(a['POSITION']).reshape(-1,3)
                pos=(np.c_[pos,np.ones(len(pos))]@world.T)[:,:3]*[100,100,-100]
                norm=np.array(a['NORMAL']).reshape(-1,3)@np.linalg.inv(world[:3,:3]);norm*= [1,1,-1]
                norm/=np.maximum(np.linalg.norm(norm,axis=1)[:,None],1e-12)
                uv=pos[:,[0,2]]/100 # deterministic projected scratch coordinates
                material=prim['material'];mat='ferrari_458_body' if material==14 else ('ferrari_458_glass' if material==9 else f'f458_mat{material}')
                if wheel=='wheel_fl' and n['name'] in ('rim_fl','wheel'):mat='f458_wheel'
                p=dict(mat=mat,pos=pos,norm=norm,uv=uv,tri=np.array(a['indices']).reshape(-1,3)[:,[0,2,1]])
                p=subset(p,np.ones(len(p['tri']),dtype=bool))
                if wheel=='wheel_fl':wheelparts.append(p)
                elif wheel is None:parts.append((n['name'],p))
        for c in n.get('children',[]):walk(c,world,wheel)
    walk(0,np.eye(4))
    wheels={j['nodes'][i]['name'].upper():world[:3,3]*[100,100,-100] for i,world in worlds.items() if j['nodes'][i]['name'].startswith('wheel_')}
    for p in wheelparts:p['pos']-=wheels['WHEEL_FL']
    zones={k:[] for k in ['CHASS','FBUMP','RBUMP','BONNET','SSKIRT_L','SSKIRT_R','BRKLIGHT','LIGHTSON','EXHAUST']}
    for source,p in parts:
        if source=='brakes':p['mat']='f458_stop';zones['BRKLIGHT'].append(p);continue
        if source=='leds':p['mat']='f458_head';zones['LIGHTSON'].append(p);continue
        cent=p['pos'][p['tri']].mean(1);x,y,z=cent.T
        keys=np.full(len(cent),'CHASS',dtype='<U10')
        # Include trim in bumpers; preserve cabin, mirror and glass topology.
        exterior=source in ('body','plastic_gray','grills','chrome','interior_dark','interior_light')
        if exterior:
            keys[(z>173)&(y<63)]='FBUMP'
            keys[(z<-183)&(y<75)]='RBUMP'
            keys[(abs(x)>77)&(y<31)&(z>-105)&(z<85)&(x<0)]='SSKIRT_L'
            keys[(abs(x)>77)&(y<31)&(z>-105)&(z<85)&(x>=0)]='SSKIRT_R'
        if source=='body':keys[(z>83)&(z<188)&(y>62)&(abs(x)<54)]='BONNET'
        if source in ('chrome','interior_dark','metal','plastic_gray'):
            keys[(abs(x)<31)&(z<-217)&(y>28)&(y<58)]='EXHAUST'
        for key in np.unique(keys):zones[key].append(subset(p,keys==key))
    report={'car':'Ferrari 458 Spider','model':'FERRARI_458','id':53,'wheels':{k:v.tolist() for k,v in wheels.items()},'meshes':{}}
    def save_mesh(n,ps):
        carfile('mesh/'+n+'.d8m',mesh_bytes(ps));report['meshes'][n]=sum(p['tri'].size//3 for p in ps)
    for k,ps in zones.items():
        if k=='EXHAUST':
            for part in ps:part['pos']-=np.array([0,42,-220])
            write('carmesh/f458_exhaust.d8m',mesh_bytes(ps))
            continue
        assert ps, 'Empty damage/light zone '+k
        save_mesh('f458_'+k,ps)
    save_mesh('f458_wheel',wheelparts)
    # A correct matching wheel is retained at speed; no unrelated donor blur mesh.
    save_mesh('f458_wheel_blur',wheelparts)
    # Stock 458 has an integrated lip, not a standing wing. This slot's small flush lip
    # and the authored sport wing are both specific to this car's rear deck.
    stockspoiler=[box((-65,83,-214),(65,85,-209),'ferrari_458_body')]
    save_mesh('f458_spoiler',stockspoiler)
    kit={
      'FBUMP':[box((-82,20,187),(82,24,223),'f458_mat12')],
      'RBUMP':[box((-69,21,-224),(69,26,-188),'f458_mat12')],
      'BONNET':[box((-30,74,108),(30,77,151),'f458_mat12')],
      'SSKIRT_L':[box((-98,20,-111),(-83,25,88),'f458_mat12')],
      'SSKIRT_R':[box((83,20,-111),(98,25,88),'f458_mat12')],
      'SPOILER':[box((-82,113,-214),(82,117,-194),'f458_mat12'),
                 box((-52,85,-206),(-48,114,-200),'f458_mat12'),box((48,85,-206),(52,114,-200),'f458_mat12')]
    }
    for k,extra in kit.items():
        if k=='BONNET':
            carbon=[]
            for part in zones[k]:
                part=part.copy()
                if part['mat']=='ferrari_458_body':part['mat']='f458_mat12'
                carbon.append(part)
            save_mesh('f458_sport_'+k,carbon)
        else:save_mesh('f458_sport_'+k,(stockspoiler if k=='SPOILER' else zones[k])+extra)
    # Decals are a fitted duplicate of the actual paint surface, not floating quads.
    # Native DECAL/SPONS damage zones deform these together with the body underneath.
    paint=[p for _,p in parts if p['mat']=='ferrari_458_body']
    def skin(material, region, project):
        result=[]
        for p in paint:
            centre=p['pos'][p['tri']].mean(1)
            normal=p['norm'][p['tri']].mean(1)
            q=subset(p,region(centre,normal))
            if q is None:continue
            q['mat']=material
            q['uv']=project(q['pos'])
            q['pos']+=q['norm']*.16
            result.append(q)
        return result
    stripes=skin('decal_f458_stripes',lambda p,n:(n[:,1]>.45)&(p[:,1]>60),
                 lambda p:np.c_[(p[:,0]+110)/220,(p[:,2]+235)/470])
    side=skin('decal_f458_sides',lambda p,n:(abs(n[:,0])>.55)&(p[:,1]<85)&(p[:,1]>30),
              lambda p:np.c_[(p[:,2]+235)/470,(p[:,1]-25)/65])
    save_mesh('DECAL_F458_STRIPES',stripes)
    save_mesh('DECAL_F458_SIDES',side)
    custom=skin('decal_f458_customlogo',lambda p,n:(n[:,1]>.7)&(abs(p[:,0])<43)&(p[:,2]>95)&(p[:,2]<172),
                lambda p:np.c_[np.clip((p[:,0]+43)/86,0,1),np.clip((172-p[:,2])/77,0,1)])
    custom+=skin('decal_f458_customlogo',lambda p,n:(abs(n[:,0])>.7)&(p[:,2]>-55)&(p[:,2]<60)&(p[:,1]>38)&(p[:,1]<78),
                 lambda p:np.c_[np.clip((60-p[:,2])/115,0,1),np.clip((78-p[:,1])/40,0,1)])
    save_mesh('DECAL_F458_CUSTOMLOGO',custom)
    sponsors=[]
    # Three-by-three group on each door: every performance sponsor has its own slot.
    for i in range(9):
        z0=-49+(i%3)*32;y0=41+(i//3)*11
        sponsors+=skin(f'decal_spons_{i+1}',
            lambda p,n,z=z0,y=y0:(abs(n[:,0])>.7)&(p[:,2]>z)&(p[:,2]<z+30)&(p[:,1]>y)&(p[:,1]<y+10),
            lambda p,z=z0,y=y0:np.c_[np.clip((p[:,2]-z)/30,0,1),np.clip(1-(p[:,1]-y)/10,0,1)])
    save_mesh('f458_sponsors',sponsors)
    children=[node('F458_'+k,'f458_'+k) for k in ['BRKLIGHT','LIGHTSON']]
    children += [node(k) for k in ['FBUMP','RBUMP','BONNET','SPOILER','SSKIRT_L','SSKIRT_R','DECAL']]
    children += [node('F458_SPONS','f458_sponsors')]
    # The source includes three exhaust tips; centre upgrade meshes replace their position.
    children += [node('EXHAUST_C',pos=(0,42,-220)),node('DRIVER',pos=(-35,83,9)),node('BONNET_CAM',pos=(0,82,140))]
    for side,x in [('L',-69),('R',69)]:
        for tag,pos,texture in [('HLIGHTON',(x,64,185),'car_flare01'),('RBRKLIGHTON',(x*1.1,83,-213),'car_flare_brk')]:
            m=np.eye(4);m[:3,3]=pos
            if tag=='RBRKLIGHTON':m[0,0]=m[2,2]=-1
            props=(f'LIGHTFLARE={texture}\nLIGHTFLARE_SIZE=380\nLIGHTFLARE_BRIGHTNESS=0.7\n'
                   'LIGHTFLARE_FALLOFF_MIN=0\nLIGHTFLARE_FALLOFF_MAX=60\nLIGHTFLARE_OFFSET=5\n').encode()
            children.append(name('F458_'+tag+'_'+side)+pack('16f',*m.ravel())+pack('II',2,len(props))+props+pack('I',0))
    carfile('models/ferrari_458_stock.boh',node('F458_ROOT',children=[node('F458_CHASS','f458_CHASS',children=children)]+[node(k,pos=v) for k,v in wheels.items()]))
    carfile('mesh/ferrari_458_chass_shad.ocd',collision_mesh())
    carfile('mesh/ferrari_458_chass_collidee.ocd',collision_mesh())
    def tex(n,rgba,size=1):
        pixels=np.tile(np.array(rgba,dtype=np.uint8),(size,size,1))
        if n=='f458_scratch':
            yy,xx=np.indices((size,size));mask=((xx+yy*3)%47<2)|((xx*3-yy)%83==0)
            pixels[:,:,3]=np.where(mask,180,0)
        carfile('textures/'+n+'.ddt',pack('9IfI',21,size,size,1,2,2,0,1,1,0.,1)+pixels[:,:,[2,1,0,3]].tobytes())
    tex('f458_paint',(255,255,255,0));tex('f458_scratch',(150,150,150,0),128)
    tex('f458_rim',(235,235,235,255))
    carfile('mesh/mats/f458_wheel.pmd','''Material Program = Coloured Spec Opaque
Colour = 190 190 190
Specular Colour = 180 180 180
Specular Highlight = 80
Shininess = 64
Z-Bias = 0
Texture = f458_rim.ptd
''')
    def decal_material(material,texture):
        carfile('mesh/mats/'+material+'.pmd',f'''Material Program = Coloured Spec ABlend
Colour = 255 255 255
Specular Colour = 130 130 130
Specular Highlight = 64
Shininess = 32
Additive = FALSE
Texture = {texture}.ptd
''')
    for style in ('stripes','sides'):
        yy,xx=np.indices((512,512));u=(xx+.5)/512;v=(yy+.5)/512
        if style=='stripes':mask=(abs(u-.5)>.018)&(abs(u-.5)<.065)
        else:mask=(abs(v-.35)<.045)|((abs(v-.49)<.018)&(u>.15)&(u<.85))
        pixels=np.full((512,512,4),255,dtype=np.uint8);pixels[:,:,3]=np.where(mask,255,0)
        n='decal_f458_'+style
        carfile('textures/'+n+'.ddt',pack('9IfI',21,512,512,1,2,2,0,1,1,0.,1)+pixels[:,:,[2,1,0,3]].tobytes())
        decal_material(n,n)
    tex('f458_no_logo',(255,255,255,0))
    decal_material('decal_f458_customlogo','f458_no_logo')
    for i in range(1,10):decal_material(f'decal_spons_{i}','f458_no_logo')
    carfile('mesh/mats/ferrari_458_body.pmd', '''Material Program = Car Body
Car Colour = 220 12 8
Side Fresnel = 0.18
Front Fresnel = 0.02
Metallic Colour = 20 20 20
Pearlescent Colour = 0 0 0
Metallic Shininess = 25
Pearlescent Shininess = 0
Metallic = FALSE
Double-sided = FALSE
Z-Bias = 0
Paint Work = f458_paint.ptd
Scratch & Reflect = f458_scratch.ptd
''')
    tex('f458_glass',(60,70,80,85))
    carfile('mesh/mats/ferrari_458_glass.pmd','''Material Program = Car Glass
Side Fresnel = 0.12
Front Fresnel = 0.02
Base Opacity = 0.25
Start Fade = 0
End Fade = 10000
Colour = 100 110 120
Texture = f458_glass.ptd
Scratch = f458_scratch.ptd
''')
    for i,m in enumerate(j['materials']):
        if i in (9,14):continue
        c=m['pbrMetallicRoughness']['baseColorFactor'];rgba=[round(x*255) for x in c[:3]]+[255]
        if i==12:rgba=[35,38,40,255] # carbon: source material lacks its original weave map
        if i==11:rgba=[28,28,30,255]
        if i==12:
            yy,xx=np.indices((128,128));warp=((xx//4+yy//4)%2)==0
            weave=24+np.where(warp,12*np.sin((xx%4+.5)*np.pi/4),12*np.sin((yy%4+.5)*np.pi/4))
            pixels=np.zeros((128,128,4),dtype=np.uint8);pixels[:,:,:3]=weave[:,:,None];pixels[:,:,3]=255
            carfile('textures/f458_mat12.ddt',pack('9IfI',21,128,128,1,2,2,0,1,1,0.,1)+pixels.tobytes())
        else:tex(f'f458_mat{i}',rgba)
        carfile(f'mesh/mats/f458_mat{i}.pmd',f'Material Program = Standard Opaque\nDouble-sided = FALSE\nZ-Bias = 0\nOpaque Texture = f458_mat{i}.ptd\n')
    defs=r.read('scripts/cardefs.txt').decode('latin1')
    block=['[BeginCar] FERRARI_458 "FERRARI 458 SPIDER"','MANUFACTURER NONE EUROPE','VALUE 185000','STATE NEW']
    for category,part in [('FBUMP','FBUMP'),('RBUMP','RBUMP'),('BONNET','BONNET'),('SPOILER','SPOILER'),('SKIRT','SSKIRT')]:
        block += [f'STANDARD {category} "f458_{part}"',f'CUSTOM {category} "f458_sport_{part}" AERO']
    block += ['CUSTOM DECAL "DECAL_F458_STRIPES" AERO','CUSTOM DECAL "DECAL_F458_SIDES" AERO',
              'CUSTOM DECAL "DECAL_F458_CUSTOMLOGO" AERO',
              'ALLOW_NONE DECAL','ALLOW_NONE SPOILER','STANDARDWHEELS "f458_wheel"',
              'STANDARDEXHAUST "f458_exhaust"','ENGINE_SOUND f458','MAXDROPHEIGHT 4','[EndCar]']
    # Skirts use the native _L / _R suffix. The other names are case-insensitive.
    defs += '\n' + '\n'.join(block) + '\n'
    write('scripts/cardefs.txt',defs.encode('latin1'))
    dynamics=r.read('scripts/dynamic_falcon.txt').decode('latin1').replace('falcon', 'ferrari_458')
    changes={'Name':'Ferrari458Spider','Mass':'1535','CentreOfGravity':'0 0.32 -0.25','FrontWeightDistribution':'0.42',
      'TorqueCurveX':'1000 2000 3250 4500 6000 7500 9000 9400','TorqueCurveY':'0.50 0.67 0.82 0.94 1.0 0.98 0.825 0.0',
      'MaxTorque':'540','ForwardGearRatios':'3.077 2.185 1.626 1.286 1.028 0.839 0.693',
      'EngineInertia':'0.8','ClutchSpeed':'9','PowerRangeMin':'6000','PowerRangeMax':'9000','FinalDrive':'5.143',
      'WheelRadius':'0.34','Width':'0.235'}
    for k,v in changes.items():dynamics=re.sub(r'^'+k+r'[^\r\n]*',k+' '+v,dynamics,flags=re.M)
    for i,key in enumerate(['WHEEL_FL','WHEEL_FR','WHEEL_RL','WHEEL_RR']):
        x,y,z=wheels[key]
        replacement=f'[BeginLineCollider]\nID {i}\nPoint {x:.4f} {y+12:.4f} {z:.4f}\nEnd {x:.4f} -6 {z:.4f}\n[EndLineCollider]'
        dynamics=re.sub(r'\[BeginLineCollider\]\s*ID '+str(i)+r'\b.*?\[EndLineCollider\]',replacement,dynamics,flags=re.S)
    dynamics=dynamics.replace('WeightDistribution 0.26','WeightDistribution 0.21').replace('WeightDistribution 0.24','WeightDistribution 0.29')
    write('scripts/dynamic_ferrari_458.txt',dynamics)
    for mat,c in [('f458_stop','255 45 25'),('f458_head','220 235 255')]:
        carfile('mesh/mats/'+mat+'.pmd','Material Program = Constant Colour\nColour = '+c+'\n')
    # The shared exhaust loader searches carmesh/mats and cartextures.
    for mat in ('f458_mat10','f458_mat13','f458_mat3','f458_mat2'):
        write('carmesh/mats/'+mat+'.pmd',(root/'cars/ferrari_458/mesh/mats'/ (mat+'.pmd')).read_bytes())
        write('cartextures/'+mat+'.ddt',(root/'cars/ferrari_458/textures'/ (mat+'.ddt')).read_bytes())
    arcade=r.read('scripts/arcadecollection.txt').decode('latin1')
    arcade += '\nBeginCar ferrari_458\nBaseColour 220 12 8\nMetallicColour 0 0 0\nEndCar\n'
    write('scripts/arcadecollection.txt',arcade.encode('latin1'))
    write('scripts/mod_unlocks.txt','Car FERRARI_458\n')
    write('scripts/mod_cars.txt','; Name, stable ID, dealer respect threshold, model class\nCar FERRARI_458 53 12 1\n')
    groups=r.read('scripts/ArcadeUnlock.txt').decode('latin1')
    groups=groups.replace('Car   viper', 'Car FERRARI_458\nCar   viper')
    write('scripts/arcadeunlock.txt',groups)
    # All arcade/custom-race menus now have a selectable stock instance alongside all retail cars.
    source=Path(__file__).parent/'assets/ferrari.glb';report['source_sha256']=hashlib.sha256(source.read_bytes()).hexdigest()
    write('import-report.json',json.dumps(report,indent=2)+'\n')
    attribution=Path(__file__).parent/'assets/ATTRIBUTION.txt';write('ATTRIBUTION.txt',attribution.read_bytes())
    write('CC-BY-4.0.txt',(attribution.parent/'CC-BY-4.0.txt').read_bytes())
    report['audio']=build_audio(root)
    write('import-report.json',json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game',type=Path,required=True);p.add_argument('--decoded',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    build(p.parse_args())
