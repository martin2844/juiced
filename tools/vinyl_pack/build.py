"""Retarget the recovered NFS vinyl atlas onto original Juiced paint surfaces.
Produces an isolated package; never modifies retail archives or saves.
"""
from pathlib import Path
import sys,re,json,struct,argparse,collections,shutil
import numpy as np
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'mod_bundle'))
from retail import Retail
from mesh import read,boh,triangles,encode
# UG/MW five-zone template documented by CiPH3R-88's MIT Blender tools.
ZONES=[(.097,.886,.810,.965),(.097,.886,.247,.403),(.086,.920,.464,.749),
       (.307,.618,.037,.120),(.677,.967,.062,.205)]
MODEL_MAP={'SKYLINE':'GTR','FOCUS':'focus','MX5':'MX5'}
def ddt(im,mips=False):
    levels=[];level=im.convert('RGBA')
    while True:
        levels.append(np.array(level)[:,:,[2,1,0,3]].tobytes())
        if not mips or level.size==(1,1):break
        level=level.resize((max(1,level.width//2),max(1,level.height//2)),Image.Resampling.BOX)
    return struct.pack('<9IfI',21,im.width,im.height,len(levels),2,2,2 if mips else 0,3,3,0.,1)+b''.join(levels)

def visible_surface(pos,norm,uv,im):
    """Conservatively discard triangles whose complete UV rectangle is transparent."""
    alpha=np.array(im.getchannel('A'))>2
    sums=np.pad(alpha.astype('int64'),((1,0),(1,0))).cumsum(0).cumsum(1)
    triangles=uv.reshape(-1,3,2)*np.array(im.size)
    lo=np.maximum(np.floor(triangles.min(1)).astype(int)-2,0)
    hi=np.minimum(np.ceil(triangles.max(1)).astype(int)+3,np.array(im.size))
    lo=np.minimum(lo,np.array(im.size));hi=np.maximum(hi,0)
    count=sums[hi[:,1],hi[:,0]]-sums[lo[:,1],hi[:,0]]-sums[hi[:,1],lo[:,0]]+sums[lo[:,1],lo[:,0]]
    keep=np.repeat(count>0,3)
    if not keep.any():raise ValueError('Artwork has no visible surface on this car')
    return pos[keep],norm[keep],uv[keep]

def artwork(im,row):
    """Bake a documented neutral palette for NFS tint-channel maps.

    Full-colour art is left intact. These are adaptation colours, not a claim
    to have recovered a user's NFS in-game colour selections.
    """
    if row.get('prepared'):return im,row
    arr=np.array(im);rgb=arr[:,:,:3].astype(float)
    tint=row['tint_channels']
    if tint:
        palette=np.array([[235,235,240],[165,174,185],[35,45,60]],float)
        # Movie-themed channel palettes; body paint remains the player's choice.
        if row['model']=='SKYLINE' and any(s in row['label'] for s in ('2f2f','2fast')):
            palette=np.array([[30,75,210],[165,174,185],[30,75,210]],float)
        if row['model']=='ECLIPSE' and any(s in row['label'] for s in ('f&f','faf','fnf','fast')):
            palette=np.array([[235,235,240],[165,174,185],[68,40,105]],float)
        arr[:,:,:3]=np.clip(rgb@palette/255,0,255).astype('uint8')
        im=Image.fromarray(arr)
        row=dict(row,baked_palette=palette.astype(int).tolist())
    if row['layout']=='badge':
        # Rectangular sponsor decals are individual logos, not whole-car atlases.
        atlas=Image.new('RGBA',(512,512))
        badge=im.copy();badge.thumbnail((270,55))
        for zone,flip in [(ZONES[0],True),(ZONES[1],False)]:
            u0,u1,v0,v1=zone
            x=int((u0+u1)*256)-badge.width//2
            y=int((2-v0-v1)*256)-badge.height//2
            atlas.paste(badge.transpose(Image.Transpose.ROTATE_180) if flip else badge,(x,y))
        im=atlas
    return im,row

def shell(retail,car,block,ui,ug1=False):
    folder='cars/'+car.lower()+('_ui' if ui else '')
    nodes=list(boh(retail.read(folder+'/models/'+car+'_stock.boh')))
    slots={n.upper():mat for n,m,mat in nodes}
    # The retail MR-S uses MRS_DECAL; CarModel resolves the same suffix alias.
    decal=next(mat for n,m,mat in nodes if n.upper()=='DECAL' or n.upper().endswith('_DECAL'))
    inv=np.linalg.inv(decal)
    names={m:inv@mat for _,m,mat in nodes if m and m.upper()!='NONE'}
    for cat,part in re.findall(r'^\s*STANDARD\s+(\w+)\s+"?([^"\s]+)',block,re.M|re.I):
        if cat in ['FBUMP','RBUMP','BONNET']:names[part]=inv@slots[cat]
        if cat=='SKIRT':
            for side in ['L','R']:
                key=next((k for k in slots if k.endswith('SKIRT_'+side)),None)
                if key:names[part+'_'+side]=inv@slots[key]
    positions=[];normals=[]
    for name in sorted(names):
        try:m=read(retail.read(folder+'/mesh/'+name+'.d8m'))
        except (FileNotFoundError,KeyError):continue
        for p in m['parts']:
            if p['material'].lower()!=car.lower()+'_body':continue
            transform=names[name]
            pos=p['streams'][(0,0)]@transform[:3,:3].T+transform[:3,3]
            norm=p['streams'][(3,0)]@np.linalg.inv(transform[:3,:3])
            norm/=np.maximum(np.linalg.norm(norm,axis=1,keepdims=True),1e-8)
            t=triangles(m,p)
            if not len(t):continue
            avg=norm[t].mean(1)
            keep=avg[:,1]>-.3
            t=t[keep]
            positions.append(pos[t].reshape(-1,3));normals.append(norm[t].reshape(-1,3))
    if not positions:raise ValueError('No painted body for '+car)
    pos=np.concatenate(positions);norm=np.concatenate(normals)
    avg=norm.reshape(-1,3,3).mean(1)
    # Front is +Z in the retail cars; +X and -X are separate atlas sides.
    zones=np.where(avg[:,0]>0,0,1)
    zones[np.abs(avg[:,2])>np.abs(avg[:,0])]=np.where(avg[np.abs(avg[:,2])>np.abs(avg[:,0]),2]>0,3,4)
    zones[avg[:,1]>.55]=2
    zones=np.repeat(zones,3)
    uv=np.zeros((len(pos),2))
    for z,(u0,u1,v0,v1) in enumerate(ZONES):
        if ug1 and z==1:v0+=.05;v1+=.05
        sel=zones==z
        if not sel.any():continue
        p=pos[sel]
        if z in (0,1):a=-p[:,2];b=p[:,1]
        elif z==2:a=p[:,2];b=p[:,0]
        else:a=p[:,0] if z==3 else -p[:,0];b=p[:,1]
        if z==0:b=-b
        if z in (0,1):
            # One physical frame for both sides, even on asymmetric meshes.
            side=pos[np.isin(zones,[0,1])]
            amin=-side[:,2].max();amax=-side[:,2].min()
            bmin=side[:,1].min();bmax=side[:,1].max()
            if z==0:bmin,bmax=-bmax,-bmin
        else:amin,amax=a.min(),a.max();bmin,bmax=b.min(),b.max()
        a=(a-amin)/max(float(amax-amin),1);b=(b-bmin)/max(float(bmax-bmin),1)
        uv[sel,0]=u0+a*(u1-u0);uv[sel,1]=1-(v0+b*(v1-v0))
    return pos+norm*.08,norm,uv

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--game',type=Path,required=True);ap.add_argument('--decoded',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True);ap.add_argument('--only',default='')
    a=ap.parse_args();r=Retail(a.game);report=json.loads((a.decoded/'catalogue.json').read_text())
    text=r.read('scripts/cardefs.txt').decode('latin1')
    blocks={m[0].upper():(m[0],m[1]) for m in re.findall(r'\[BeginCar\]\s+(\S+)(.*?)\[EndCar\]',text,re.S|re.I)}
    out=a.output
    if out.exists() and any(out.iterdir()):raise ValueError('Use a fresh output directory to avoid stale assets/indices')
    assets=out/'assets';assets.mkdir(parents=True,exist_ok=True)
    def write(rel,b):
        p=assets/rel.lower();p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
    records=[];registry=[];cache={}
    for row in report['designs']:
        model=MODEL_MAP.get(row['model'],row['model'])
        if not model or model.upper() not in blocks:continue
        car,block=blocks[model.upper()]
        if a.only and car.upper()!=a.only.upper():continue
        im,row=artwork(Image.open(a.decoded/row['png']).convert('RGBA'),row)
        material='DECAL_NFS_'+row['id']
        meshname=material
        for ui in [False,True]:
            key=(car,ui,row['pack']=='nd4' and row['layout']=='atlas' and not row.get('balanced_sides'))
            if key not in cache:cache[key]=shell(r,car,block,ui,key[2])
            pos,norm,uv=cache[key]
            pos,norm,uv=visible_surface(pos,norm,uv,im)
            write('cars/'+car+('_ui' if ui else '')+'/mesh/'+meshname+'.d8m',encode(pos,norm,uv,material))
        write('cartextures/'+material+'.ddt',ddt(im,mips=True))
        write('carmesh/mats/'+material+'.pmd',('Material Program = Standard Alpha Blend\nZ-Write = FALSE\nAlpha Texture = '+material+'.ptd\n').encode())
        preview=Image.new('RGBA',(128,128),(90,90,90,255))
        small=im.copy();small.thumbnail((128,128));preview.alpha_composite(small)
        write('ui/textures/ui_'+material+'.ddt',ddt(preview))
        title=row['label'] if row['pack']!='aj' else Path(row['texture']).stem.replace(' (nfsu2)','')
        label=(row['pack'].upper()+': '+title[:28]+' '+row['id'][-4:]).replace('"',"'")
        registry.append('Vinyl '+car+' '+meshname+' "'+label+'"')
        records.append(dict(**row,car=car,mesh=meshname))
    write('scripts/mod_vinyls.txt',('\n'.join(registry)+'\n').encode())
    (out/'mod.ini').write_text('name=Underground & F&F Vinyl Collection\nversion=1\nrevision=2\nkind=assets\n')
    (out/'catalogue.json').write_text(json.dumps(records,indent=2))
    print('Built',len(records),'vinyl layers',collections.Counter(x['car'] for x in records))
if __name__=='__main__':main()
