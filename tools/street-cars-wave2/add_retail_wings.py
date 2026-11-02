"""Blender: fit real Juiced wings to the ten imported cars, keeping save IDs.

Blender --background --python-exit-code 1 --python THIS -- REPO WAVE2 FIRSTWAVE FRESH_OUTPUT
Existing body-part lists keep their order. A previously absent spoiler category
gets a stock entry (retained source wing or NONE), then three fitted choices.
"""
from pathlib import Path
import copy,hashlib,json,re,shutil,struct,sys
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

r,wave,first,out=map(Path,sys.argv[sys.argv.index('--')+1:]);assert not out.exists();out.mkdir(parents=True)
sys.path.insert(0,str(r/'tools'));from vinyl_pack import mesh;from mod_bundle.retail import Retail
sys.path.insert(0,str(r/'mods/ferrari/tools'));from import_ferrari import bounds
retail=Retail(r.parent/'game-data');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name,p in [('street-cars-wave2',wave),('street-cars',first)]:shutil.copytree(p,out/name)
studies=out/'editable';studies.mkdir();records=[]
models=[('integra_db8','db8','street-cars',-190,False),('bmw_e36','e36','street-cars',-207,False),('levin_ae92','ae92','street-cars',-185,False),('peugeot_405_glx','w257','street-cars-wave2',-197,False),('peugeot_406_coupe','w258','street-cars-wave2',-193,False),('golf_mk5_r32','w259','street-cars-wave2',-139,True),('jetta_mk5_fsi','w260','street-cars-wave2',-208,False),('outback_2002','w261','street-cars-wave2',-177,True),('volvo_240_turbo','w262','street-cars-wave2',-211,False),('volvo_v70_24','w263','street-cars-wave2',-190,True)]
factory_counts={'integra_db8':{162},'bmw_e36':{60},'levin_ae92':{203,94,82},'golf_mk5_r32':{498},'volvo_v70_24':{106}}

def groups(a,p):
    pos=p['streams'][0,0];tri=mesh.triangles(a,p);_,ids=np.unique(np.round(pos,3),axis=0,return_inverse=True);parent=np.arange(ids.max()+1)
    def root(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    for t in ids[tri]:
        for v in t[1:]:parent[root(v)]=root(t[0])
    result={}
    for i,t in enumerate(tri):result.setdefault(root(ids[t[0]]),[]).append(i)
    return tri,result.values()

def subset(p,tri):
    used,ids=np.unique(np.array(tri).reshape(-1),return_inverse=True);q=copy.deepcopy(p);stride=struct.unpack_from('<I',p['header'],len(p['header'])-8)[0]
    q['verts']=bytearray(np.frombuffer(p['verts'],dtype=np.uint8).reshape(-1,stride)[used].tobytes());struct.pack_into('<I',q['header'],len(q['header'])-4,len(used));q['triangles']=ids.reshape(-1,3).astype('<u2');return q

def clone(p):
    q=copy.deepcopy(p);raw=np.frombuffer(p['verts'],dtype=np.uint8)
    q['streams']={key:np.ndarray(value.shape,dtype=value.dtype,buffer=q['verts'],offset=value.ctypes.data-raw.ctypes.data,strides=value.strides) for key,value in p['streams'].items()}
    return q

def encode(parts):
    index=[];data=[];first=0
    for p in parts:
        tri=p['triangles'].reshape(-1).astype('<u2');h=bytearray(p['header']);struct.pack_into('<3I',h,32,first,len(tri),1);first+=len(tri)
        stride,n=struct.unpack_from('<2I',h,len(h)-8);pos=np.ndarray((n,3),dtype='<f4',buffer=p['verts'],strides=(stride,4));h[44:88]=bounds(pos)
        data.append(bytes(h)+bytes(p['verts']));index.append(tri)
    indices=np.concatenate(index);raw=struct.pack('<I',len(indices))+indices.tobytes()+struct.pack('<I',len(parts))+b''.join(data)
    parsedpos=[]
    for p in parts:
        stride,n=struct.unpack_from('<2I',p['header'],len(p['header'])-8);parsedpos.append(np.ndarray((n,3),dtype='<f4',buffer=p['verts'],strides=(stride,4)))
    return raw+bounds(np.concatenate(parsedpos))

def scene_part(name,p,a,collection,matname):
    coords=p['streams'][0,0];verts=np.c_[coords[:,0],-coords[:,2],coords[:,1]]/100
    m=bpy.data.meshes.new(name);m.from_pydata(verts.tolist(),[],mesh.triangles(a,p).tolist());m.update();obj=bpy.data.objects.new(name,m);collection.objects.link(obj)
    material=bpy.data.materials.get(matname) or bpy.data.materials.new(matname);material.diffuse_color=(.07,.20,.45,1) if matname.endswith('_body') else (.055,.06,.065,1);m.materials.append(material)

for model,prefix,package,targetz,hatch in models:
    folder=out/package/'assets/cars'/model;chass=folder/'mesh'/(prefix+'_chass.d8m');original=mesh.read(chass.read_bytes());stockparts=[];kept=[];factorytri=0
    for p in original['parts']:
        tri=mesh.triangles(original,p);selected=[]
        if p['material']==model+'_body' and model in factory_counts:
            _,components=groups(original,p)
            for g in components:
                v=p['streams'][0,0][tri[g]].reshape(-1,3);lo=v.min(0);hi=v.max(0)
                if len(g) in factory_counts[model] and lo[1]>75 and hi[2]<-140 and hi[1]-lo[1]<30:selected.extend(g)
        mask=np.zeros(len(tri),dtype=bool);mask[selected]=True
        if selected:stockparts.append(subset(p,tri[mask]));factorytri+=int(mask.sum())
        if (~mask).any():kept.append(subset(p,tri[~mask]))
    if model in factory_counts:assert factorytri>0,(model,'factory wing not isolated')
    if stockparts:
        data=encode(kept);stock=encode(stockparts)
        for suffix in ['','_ui']:
            f=out/package/'assets/cars'/(model+suffix)/'mesh';assert (f/(prefix+'_chass.d8m')).read_bytes()==chass.read_bytes() if not suffix else True
            (f/(prefix+'_chass.d8m')).write_bytes(data);(f/(prefix+'_factory_wing.d8m')).write_bytes(stock)
    # Fit against the source painted exterior, excluding the detachable stock wing.
    vertices=[];faces=[]
    for zone in ['chass','fbump','rbump','bonnet']:
        a=mesh.read((folder/'mesh'/(prefix+'_'+zone+'.d8m')).read_bytes())
        for p in a['parts']:
            if p['material']!=model+'_body':continue
            start=len(vertices);vertices.extend(p['streams'][0,0].tolist());faces.extend((mesh.triangles(a,p)+start).tolist())
    bvh=BVHTree.FromPolygons(vertices,faces,all_triangles=True);body=np.array(vertices);width=float(np.ptp(body[:,0]));bpy.ops.wm.read_factory_settings(use_empty=True)
    reference=bpy.data.collections.new('UNCHANGED_NATIVE_BODY');bpy.context.scene.collection.children.link(reference)
    for zone in ['chass','fbump','rbump','bonnet']:
        a=mesh.read((folder/'mesh'/(prefix+'_'+zone+'.d8m')).read_bytes())
        for i,p in enumerate(a['parts']):scene_part(zone+str(i),p,a,reference,p['material'])
    plans=[('golf',3),('golf',4),('golf',5)] if hatch else [('350z',1),('ac_integra',5),('350z',7)]
    choices=[]
    def surface(x,z):
        hit,_,_,_=bvh.ray_cast(Vector((float(x),250,float(z))),Vector((0,-1,0)),250)
        if hit is None or hit.y<65:raise ValueError((model,'wing foot misses exterior',float(x),float(z),None if hit is None else hit.y))
        return hit.y
    for number,(donor,which) in enumerate(plans,1):
        name=donor+'_spoiler_vs'+str(which);raw=retail.read('cars/'+donor+'/mesh/'+name+'.d8m');a=mesh.read(raw)
        active=[p for p in a['parts'] if struct.unpack_from('<f',p['header'],88)[0]==0 and not struct.unpack_from('<I',p['header'],96)[0]&1]
        v=np.concatenate([p['streams'][0,0] for p in active]);lo=v.min(0);hi=v.max(0);sz=.76 if hatch else .88;sy=.8 if hatch else .95
        contact=v[v[:,1]<lo[1]+.75]
        centrex=float((hi[0]+lo[0])/2);centrez=float(np.median(contact[:,2]));basey=surface(0,targetz);parts=[];footcontacts=[]
        # Mirrors are part of the body's bounds, so total width is not a safe
        # mounting width. Measure the actual rear deck/roof across the foot span.
        foot=contact;zs=(foot[:,2]-centrez)*sz+targetz;available=[]
        for z in [zs.min(),targetz,zs.max()]:
            widths=[]
            for side in [-1,1]:
                last=0
                for x in np.arange(0,width*.5,1):
                    hit,_,_,_=bvh.ray_cast(Vector((float(x*side),250,float(z))),Vector((0,-1,0)),250)
                    if hit is None or abs(hit.y-basey)>16:break
                    last=x
                widths.append(last)
            available.append(min(widths))
        assert min(available)>35,(model,name,available)
        sx=min(2*min(available)*.91/(hi[0]-lo[0]),1.23)
        for i,p in enumerate(active):
            q=clone(p);pos=q['streams'][0,0];normal=q['streams'][3,0];before=pos.copy();pos[:,0]=(before[:,0]-centrex)*sx;pos[:,2]=(before[:,2]-centrez)*sz+targetz;pos[:,1]=(before[:,1]-lo[1])*sy+basey
            weight=np.clip(1-(before[:,1]-lo[1])/3,0,1)
            for j in np.flatnonzero(weight>0):
                # Only the true lowest contact vertices must be supported.
                # The foil may overhang the deck, as in the retail assembly.
                try:height=surface(pos[j,0],pos[j,2])
                except ValueError:
                    if before[j,1]<lo[1]+.75:raise
                    height=basey
                pos[j,1]+=(height-basey-.15)*weight[j]
                if before[j,1]<lo[1]+.75:footcontacts.append(dict(x=float(pos[j,0]),z=float(pos[j,2]),y=float(pos[j,1]),surface_y=height))
            normal[:]=normal/np.array([sx,sy,sz]);normal[:]/=np.linalg.norm(normal,axis=1)[:,None]
            material=p['material'];isbody=material.lower().endswith('_body')
            destmat=model+'_body' if isbody else prefix+'_wing'+str(number)+'_m'+str(i)
            if not isbody:
                possible=['cars/'+donor+'/mesh/mats/'+material.lower()+'.pmd','carmesh/mats/'+material.lower()+'.pmd'];mat=None
                for key in possible:
                    try:mat=retail.read(key).decode('latin1');break
                    except FileNotFoundError:pass
                if mat is None:raise FileNotFoundError(possible)
                for k,texture in enumerate(sorted(set(re.findall(r'[^\s=]+\.ptd',mat,re.I)))):
                    texturebase=texture.replace('\\','/').split('/')[-1][:-4];tex=None
                    for key in ['cars/'+donor+'/textures/'+texturebase.lower()+'.ddt','cartextures/'+texturebase.lower()+'.ddt']:
                        try:tex=retail.read(key);break
                        except FileNotFoundError:pass
                    if tex is None:raise FileNotFoundError(texture)
                    texname=destmat+'_t'+str(k);mat=mat.replace(texture,texname+'.ptd')
                    for suffix in ['','_ui']:(out/package/'assets/cars'/(model+suffix)/'textures'/(texname+'.ddt')).write_bytes(tex)
                for suffix in ['','_ui']:(out/package/'assets/cars'/(model+suffix)/'mesh/mats'/(destmat+'.pmd')).write_text(mat)
            q['header'][:32]=destmat.encode().ljust(32,b'\0');q['triangles']=mesh.triangles(a,p);struct.pack_into('<2fI',q['header'],88,0,1e12,0);parts.append(q)
        assert footcontacts
        data=encode(parts);new=mesh.read(data);collection=bpy.data.collections.new('OPTION_'+str(number));bpy.context.scene.collection.children.link(collection)
        for i,p in enumerate(new['parts']):scene_part('wing'+str(number)+'_'+str(i),p,new,collection,p['material'])
        for suffix in ['','_ui']:(out/package/'assets/cars'/(model+suffix)/'mesh'/(prefix+'_wing'+str(number)+'.d8m')).write_bytes(data)
        choices.append(dict(index=number,donor=donor,donor_mesh=name,donor_sha256=hashlib.sha256(raw).hexdigest(),new_mesh=prefix+'_wing'+str(number),triangles=sum(len(mesh.triangles(new,p)) for p in new['parts']),feet=footcontacts))
    patch=out/package/'patches/scripts/cardefs.txt.patch';lines=patch.read_text().splitlines();assert lines[0]=='append' and len(lines)==2 and lines[1].startswith('text ');text=bytes.fromhex(lines[1][5:]).decode('latin1')
    pattern=r'(\[BeginCar\]\s+'+model+r'\b.*?)(\[EndCar\])';match=re.search(pattern,text,re.I|re.S);assert match and 'SPOILER' not in match[1]
    block='STANDARD SPOILER "'+(prefix+'_factory_wing' if stockparts else 'NONE')+'"\n'+''.join('CUSTOM SPOILER "'+prefix+'_wing'+str(i)+'" AERO\n' for i in range(1,4))+'ALLOW_NONE SPOILER\n'
    text=text[:match.start(2)]+block+text[match.start(2):];patch.write_text('append\ntext '+text.encode('latin1').hex()+'\n')
    bpy.ops.wm.save_as_mainfile(filepath=str(studies/(model+'-wings.blend')))
    records.append(dict(model=model,package=package,stock_wing_triangles_detached=factorytri,stock_geometry_retained_in_default_slot=True,target_z_cm=targetz,options=choices));print('FITTED WINGS',model,flush=True)
for package in ['street-cars','street-cars-wave2']:
    root=out/package;(root/'SHA256.json').write_text(json.dumps({p.relative_to(root).as_posix():sha(p) for p in root.rglob('*') if p.is_file() and p.name!='SHA256.json'},indent=2)+'\n')
(out/'wing-fit-receipt.json').write_text(json.dumps(dict(cars=records,installed=False,limits='Native visual review pending; no physics, other body option IDs, saves or engine changes.'),indent=2)+'\n')
