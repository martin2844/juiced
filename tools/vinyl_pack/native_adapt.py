"""Experimental native-mesh side-motif pilot. Never modifies source assets/saves.

Extracts lateral artwork, not complete NFS liveries. Adapts native UVs into a
measured covered rectangle shared by both sides. Requires visual acceptance.
"""
import argparse,json,re,sys,collections
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFilter
from build import ddt,ZONES,visible_surface,shell
from mesh import read,triangles,encode
from retail import Retail

W,H=256,64

def clip_height(poly, bound, above):
    out=[]
    for a,b in zip(poly,poly[1:]+poly[:1]):
        inside_a=(a[0]>=bound) if above else (a[0]<=bound)
        inside_b=(b[0]>=bound) if above else (b[0]<=bound)
        if inside_a:out.append(a)
        if inside_a!=inside_b:
            t=(bound-a[0])/(b[0]-a[0]);out.append(a+(b-a)*t)
    return out

def coverage(mesh,side,band):
    image=Image.new('L',(W,H));draw=ImageDraw.Draw(image)
    for part in mesh['parts']:
        pos=part['streams'][(0,0)];uv=part['streams'][(5,0)]
        for tri in triangles(mesh,part):
            if (pos[tri,0].mean()>0)!=side:continue
            poly=[np.array([pos[v,1],*uv[v]]) for v in tri]
            poly=clip_height(poly,band[0],True)
            if poly:poly=clip_height(poly,band[1],False)
            if len(poly)>=3:draw.polygon([tuple(v[1:]*[W,H]) for v in poly],fill=255)
    mask=np.array(image)>0
    # One-pixel erosion prevents triangle-edge rounding from certifying a hole.
    safe=np.zeros_like(mask)
    safe[1:-1,1:-1]=np.logical_and.reduce([mask[y:y+H-2,x:x+W-2] for y in range(3) for x in range(3)])
    return safe

def rectangle(mask,ratio):
    sums=np.pad(mask.astype(int),((1,0),(1,0))).cumsum(0).cumsum(1)
    for width in range(W-2,7,-1):
        height=max(3,round(width/ratio))
        if height>=H-2:continue
        counts=sums[height:,width:]-sums[:-height,width:]-sums[height:,:-width]+sums[:-height,:-width]
        yy,xx=np.where(counts==width*height)
        if len(xx):
            # Prefer the centre of the native motif when several rectangles fit.
            k=np.argmin((xx+width/2-W/2)**2+(yy+height/2-H/2)**2)
            return [int(xx[k]),int(yy[k]),int(xx[k]+width),int(yy[k]+height)]
    return None

def main_component(image):
    alpha=np.array(image.getchannel('A'))>20
    grown=np.array(Image.fromarray((alpha*255).astype('uint8')).filter(ImageFilter.MaxFilter(13)))>0
    from scipy import ndimage
    labels,count=ndimage.label(grown)
    if not count:raise ValueError('No source artwork')
    weights=np.bincount(labels.ravel(),weights=alpha.ravel());weights[0]=0
    mask=labels==weights.argmax()
    rgba=np.array(image);rgba[:,:,3]*=mask
    result=Image.fromarray(rgba);box=result.getchannel('A').getbbox()
    return result.crop(box)

def motif(image,layout):
    sides=[]
    for i,(u0,u1,v0,v1) in enumerate(ZONES[:2]):
        p=image.crop((round(u0*image.width),round((1-v1)*image.height),round(u1*image.width),round((1-v0)*image.height)))
        if i==0:p=p.transpose(Image.Transpose.ROTATE_180)
        box=p.getchannel('A').point(lambda x:255 if x>20 else 0).getbbox()
        if box:sides.append(p.crop(box))
    if not sides:raise ValueError('No lateral motif; needs separate panel adaptation')
    # Pilot uses the larger side; asymmetric complete liveries are not certified.
    art=main_component(max(sides,key=lambda p:np.count_nonzero(np.array(p.getchannel('A'))>20)))
    canvas=Image.new('RGBA',(1024,256))
    scale=min(960/art.width,224/art.height)
    art=art.resize((max(1,round(art.width*scale)),max(1,round(art.height*scale))),Image.Resampling.LANCZOS)
    canvas.alpha_composite(art,((1024-art.width)//2,(256-art.height)//2))
    return canvas

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for arg in ('game','decoded','catalogue','output'):ap.add_argument('--'+arg,type=Path,required=True)
    ap.add_argument('--selection',type=Path)
    ap.add_argument('--surface',choices=['native','body'],default='body')
    ap.add_argument('--overrides',type=Path,default=Path(__file__).with_name('native_placement_overrides.json'))
    ap.add_argument('--placements',type=Path);ap.add_argument('--start',type=int,default=0);ap.add_argument('--limit',type=int,default=90)
    ap.add_argument('--ids',required=True);ap.add_argument('--cars',default='CIVIC_97,ECLIPSE,SUPRA,GTR,GOLF,MUSTANG2')
    args=ap.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    assets=args.output/'assets';assets.mkdir()
    def put(path,raw):
        out=assets/path.lower();out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(raw)
    retail=Retail(args.game);text=retail.read('scripts/cardefs.txt').decode('latin1')
    blocks={car.upper():(car,block) for car,block in re.findall(r'\[BeginCar\]\s+(\S+)(.*?)\[EndCar\]',text,re.S|re.I)}
    donors={};placement=[]
    overrides=json.loads(args.overrides.read_text())
    cached={r['car'].upper():r for r in json.loads(args.placements.read_text())} if args.placements else {}
    car_names=([car for car,block in blocks.values() if re.search(r'^\s*CUSTOM\s+DECAL\s+',block,re.M|re.I)] if args.cars=='ALL' else args.cars.split(','))
    for car in car_names:
        car,block=blocks[car.upper()]
        if car.upper() in cached and car.upper() not in overrides:
            entry=cached[car.upper()];name=entry['donor'];rect=entry['rectangle']
            meshes=[read(retail.read(f'cars/{car.lower()}{"_ui" if ui else ""}/mesh/{name.lower()}.d8m')) for ui in (False,True)]
            donors[car]=(meshes,rect);placement.append(entry);continue
        candidates=re.findall(r'^\s*CUSTOM\s+DECAL\s+(\S+)',block,re.M|re.I)
        proto=read(retail.read(f'cars/{car.lower()}_ui/mesh/{candidates[0].lower()}.d8m'))
        heights=np.concatenate([p['streams'][(0,0)][:,1] for p in proto['parts']])
        lo,hi=float(heights.min()),float(heights.max())
        band=tuple(lo+(hi-lo)*fraction for fraction in overrides.get(car.upper(),{}).get('height_band',[.5,.9]))
        best=None
        for name in candidates:
            meshes=[];common=np.ones((H,W),bool)
            for ui in (False,True):
                mesh=read(retail.read(f'cars/{car.lower()}{"_ui" if ui else ""}/mesh/{name.lower()}.d8m'));meshes.append(mesh)
                common &= coverage(mesh,False,band)&coverage(mesh,True,band)
            rect=rectangle(common,4)
            if rect is not None:
                # Score physical coverage, not texture pixels: different native styles
                # have very different UV scales on the same body.
                part=meshes[1]['parts'][0]
                select=part['streams'][(0,0)][:,0]>0
                coords=part['streams'][(5,0)][select]
                world=part['streams'][(0,0)][select][:,[2,1]]
                transform=np.linalg.lstsq(np.column_stack((coords,np.ones(len(coords)))),world,rcond=None)[0]
                scale=abs(np.linalg.det(transform[:2]))/(W*H)
                score=(rect[2]-rect[0])*(rect[3]-rect[1])*scale
                if best is None or score>best[0]:best=(score,name,meshes,rect)
        if best is None:raise ValueError('No safe native UV rectangle: '+car)
        _,name,meshes,rect=best;donors[car]=(meshes,rect)
        placement.append(dict(car=car,donor=name,rectangle=rect,coverage_pixels=best[0]))
        print(car,name,rect,flush=True)
    records=[];registry=[];body_cache={};frames={}
    rows=json.loads(args.catalogue.read_text())
    selected=(set(json.loads(args.selection.read_text())['selected_ids']) if args.selection else ({r['id'] for r in rows[args.start:args.start+args.limit]} if args.ids=='ALL' else set(args.ids.split(','))))
    assert selected <= {r['id'] for r in rows}
    assert len(selected)<=94,'Current native catalogue capacity: split the research batch'
    exclusions=[];art_hashes={}
    for row in rows:
        if row['id'] not in selected:continue
        assert row.get('prepared'), 'Use the prepared atlas catalogue, including baked badge layouts'
        try:art=motif(Image.open(args.decoded/row['png']).convert('RGBA'),row['layout'])
        except ValueError as exc:
            exclusions.append(dict(id=row['id'],reason=str(exc)));continue
        import hashlib
        art_hash=hashlib.sha256(art.tobytes()).hexdigest()
        if art_hash in art_hashes:
            exclusions.append(dict(id=row['id'],reason='Duplicate extracted lateral motif',same_as=art_hashes[art_hash]));continue
        art_hashes[art_hash]=row['id']
        identifier='DECAL_NFS_N'+row['id'][1:];label='SIDE: '+row['label'][:32]
        put('cartextures/'+identifier+'.ddt',ddt(art,True))
        put('ui/textures/ui_'+identifier+'.ddt',ddt(art.resize((256,64),Image.Resampling.LANCZOS)))
        put('carmesh/mats/'+identifier+'.pmd',('Material Program = Standard Alpha Blend\nZ-Write = FALSE\nDisable Fog = FALSE\nAlpha Texture = '+identifier+'.ptd\n').encode())
        for car,(meshes,rect) in donors.items():
            for ui,mesh in zip((False,True),meshes):
                if args.surface=='body':
                    key=(car,ui)
                    if key not in body_cache:
                        carname,block=blocks[car.upper()]
                        bp,bn,_=shell(retail,carname,block,ui)
                        keep=np.repeat(np.abs(bn.reshape(-1,3,3).mean(1)[:,0])>.55,3)
                        body_cache[key]=(bp[keep],bn[keep])
                        part=mesh['parts'][0];p=part['streams'][(0,0)];v=part['streams'][(5,0)];sel=p[:,0]>0
                        transform=np.linalg.lstsq(np.column_stack((v[sel],np.ones(sel.sum()))),p[sel][:,[2,1]],rcond=None)[0]
                        corners=np.array([[rect[x]/W,rect[y]/H,1] for x in (0,2) for y in (1,3)])@transform
                        low=corners.min(0);high=corners.max(0);center=(low+high)/2
                        width=min(high[0]-low[0],(high[1]-low[1])*4);height=width/4
                        manual=overrides.get(car.upper(),{}).get('body_frame')
                        if manual:
                            center=np.array(manual['center']);width=manual['width'];height=width/4
                        frames[key]=(center,width,height)
                    pos,norm=body_cache[key];center,width,height=frames[key]
                    uv=np.column_stack(((pos[:,2]-center[0])/width+.5,.5-(pos[:,1]-center[1])/height))
                    uv[pos[:,0]<0,0]=1-uv[pos[:,0]<0,0]
                    pos,norm,uv=visible_surface(pos,norm,uv,art)
                    put(f'cars/{car}{"_ui" if ui else ""}/mesh/{identifier}.d8m',encode(pos,norm,uv,identifier))
                    continue
                ps=[];ns=[];uvs=[]
                for part in mesh['parts']:
                    tri=triangles(mesh,part);ps.append(part['streams'][(0,0)][tri].reshape(-1,3));ns.append(part['streams'][(3,0)][tri].reshape(-1,3))
                    uv=part['streams'][(5,0)][tri].reshape(-1,2).copy()
                    uv=(uv*[W,H]-rect[:2])/(np.array(rect[2:])-rect[:2])
                    flatpos=part['streams'][(0,0)][tri].reshape(-1,3)
                    for positive in (False,True):
                        side=(flatpos[:,0]>0)==positive
                        if side.sum()<3:continue
                        corr=np.corrcoef(flatpos[side,2],uv[side,0])[0,1]
                        if (corr>0)!=positive:uv[side,0]=1-uv[side,0]
                    uvs.append(uv)
                pos,norm,uv=visible_surface(np.concatenate(ps),np.concatenate(ns),np.concatenate(uvs),art)
                put(f'cars/{car}{"_ui" if ui else ""}/mesh/{identifier}.d8m',encode(pos+norm*.1,norm,uv,identifier))
            registry.append(f'Vinyl {car} {identifier} "{label}"')
            records.append(dict(source_id=row['id'],source_pack=row['pack'],source_path=row['path'],source_sha256=row['sha256'],car=car,mesh=identifier,label=label,scope='lateral motif only',accepted=False))
        art.save(args.output/(identifier+'.png'))
    put('scripts/mod_vinyls.txt',('\n'.join(registry)+'\n').encode())
    (args.output/'body-frames.json').write_text(json.dumps([dict(car=key[0],ui=key[1],center=frame[0].tolist(),width=frame[1],height=frame[2]) for key,frame in frames.items()],indent=2))
    (args.output/'exclusions.json').write_text(json.dumps(exclusions,indent=2))
    (args.output/'placements.json').write_text(json.dumps(placement,indent=2))
    (args.output/'catalogue.json').write_text(json.dumps(records,indent=2))
    (args.output/'mod.ini').write_text('name=Native vinyl placement pilot - NOT APPROVED\nversion=1\nrevision=1\nkind=assets\n')
    (args.output/'disabled').touch()
    print('Built',len(records),'car/design combinations; NOT approved for release')

if __name__=='__main__':main()
