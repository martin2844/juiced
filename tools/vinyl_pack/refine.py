"""Explicit palettes, multi-layer recipes and balanced placement for the three packs."""
import argparse,copy,hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from build import ZONES,artwork

def panda_trueno(logo):
    """Retarget the Panda trim/hood and original Fujiwara lettering independently.

    NFS's BODY and AD textures do not share a physical anchor on the Juiced car.
    Keeping them in the same raw UV coordinates puts lettering inside black trim.
    """
    canvas=Image.new('RGBA',(1024,1024));draw=ImageDraw.Draw(canvas)
    # Black bonnet at the front of the top-view region.
    u0,u1,v0,v1=ZONES[2]
    draw.rectangle((int(.66*1024),int((1-v1)*1024),int(u1*1024),int((1-v0)*1024)),fill=(22,23,25,255))
    bbox=logo.getchannel('A').point(lambda a:255 if a>20 else 0).getbbox()
    if bbox is None:raise ValueError('Missing Fujiwara lettering')
    lettering=logo.crop(bbox);lettering.thumbnail((270,36),Image.Resampling.LANCZOS)
    for side,zone in enumerate(ZONES[:2]):
        u0,u1,v0,v1=zone;w=round((u1-u0)*1024);h=round((v1-v0)*1024)
        panel=Image.new('RGBA',(w,h));d=ImageDraw.Draw(panel)
        d.rectangle((0,int(h*.78),w,h),fill=(22,23,25,255))
        # Fujiwara's door inscription is on one side, as in the original artwork.
        if side==1:panel.alpha_composite(lettering,((w-lettering.width)//2,int(h*.46)))
        if side==0:panel=panel.transpose(Image.Transpose.ROTATE_180)
        canvas.alpha_composite(panel,(round(u0*1024),round((1-v1)*1024)))
    return canvas

def balanced_sides(im):
    """Fit partial graphics without cropping their artwork or stretching its aspect.

    Whole-body wraps retain their source layout. Each side shares one scale and
    one vertical anchor; transparent padding cannot shift one side independently.
    """
    im=im.resize((1024,1024),Image.Resampling.LANCZOS)
    sides=[]
    for box,rotate in [((0,0,1024,246),True),((0,563,1024,850),False)]:
        part=im.crop(box)
        if rotate:part=part.transpose(Image.Transpose.ROTATE_180)
        alpha=np.array(part.getchannel('A'))
        # Broad wraps include body paint/cut-outs; trimming those changes the design.
        if (alpha>32).mean()>.48:return im,False
        bbox=part.getchannel('A').point(lambda a:255 if a>20 else 0).getbbox()
        sides.append(part.crop(bbox) if bbox else None)
    if not any(s is not None for s in sides):return im,False
    canvas=im.copy()
    canvas.paste((0,0,0,0),(0,0,1024,246))
    canvas.paste((0,0,0,0),(0,563,1024,850))
    width=max(s.width for s in sides if s is not None)
    height=max(s.height for s in sides if s is not None)
    scale=min(690/width,120/height)
    for side,zone,rotate in zip(sides,ZONES[:2],[True,False]):
        if side is None:continue
        side=side.resize((max(1,round(side.width*scale)),max(1,round(side.height*scale))),Image.Resampling.LANCZOS)
        if rotate:side=side.transpose(Image.Transpose.ROTATE_180)
        u0,u1,v0,v1=zone
        x=round((u0+u1)*512-side.width/2);y=round((2-v0-v1)*512-side.height/2)
        canvas.paste(side,(x,y))
    return canvas,True

def main():
    p=argparse.ArgumentParser();p.add_argument('decoded',type=Path);p.add_argument('output',type=Path)
    args=p.parse_args()
    if args.output.exists():raise ValueError('Use a new output directory')
    args.output.mkdir(parents=True)
    original=json.loads((args.decoded/'catalogue.json').read_text())
    rules=json.loads(Path(__file__).with_name('curation.json').read_text())
    rows=copy.deepcopy(original['designs']);images={}
    for row in rows:
        im=Image.open(args.decoded/row['png']).convert('RGBA')
        palette=rules['palettes_by_label'].get(row['label'])
        if palette and row['layout']=='atlas':
            arr=np.array(im);arr[:,:,:3]=np.clip(arr[:,:,:3].astype(float)@np.array(palette)/255,0,255).astype('uint8')
            im=Image.fromarray(arr);row['baked_palette']=palette
        else:
            rgb=np.array(im)[:,:,:3].astype(float);ordered=np.sort(rgb,axis=2)
            colourful=(ordered[:,:,2]-ordered[:,:,0]>50)&(np.array(im.getchannel('A'))>64)
            # BinTex's bundled authoring guide specifies primary HUES, not just
            # saturated RGB bytes: lighter/darker shades are colour channels too.
            if colourful.any() and ((ordered[:,:,1]-ordered[:,:,0])[colourful]<20).mean()>.96:
                row['tint_channels']=True
            im,row2=artwork(im,row);row.update(row2)
        row['tint_channels']=False;row['prepared']=True
        images[row['id']]=im
    combined=[];consumed=set()
    for recipe in rules['composites']:
        candidates=[r for r in rows if r['pack']==recipe['pack'] and r['label']==recipe['label']]
        ordered=[]
        for prefix in recipe['order']:
            matches=[r for r in candidates if r['texture'].startswith(prefix)]
            if len(matches)!=1:raise ValueError(f'Ambiguous/missing composite layer: {recipe}: {prefix}')
            ordered.append(matches[0])
        if len({r['id'] for r in ordered})!=len(ordered):raise ValueError('Duplicate recipe layer')
        canvas=Image.new('RGBA',(1024,1024))
        for row in ordered:canvas.alpha_composite(images[row['id']].resize(canvas.size,Image.Resampling.LANCZOS))
        if recipe.get('layout')=='panda-trueno':canvas=panda_trueno(images[ordered[-1]['id']])
        row=copy.deepcopy(ordered[0]);row['id']='V'+hashlib.sha256(canvas.tobytes()).hexdigest()[:10].upper()
        row['label']=recipe['title'];row['texture']=recipe['title'];row['layers']=ordered
        row['explicit_layout']=recipe.get('layout')
        row['png']=row['id']+'.png';row['layout']='atlas';images[row['id']]=canvas
        combined.append(row);consumed.update(r['id'] for r in ordered)
    output=[r for r in rows if r['id'] not in consumed]+combined
    for row in output:
        im=images[row['id']]
        if row.get('explicit_layout'):row['balanced_sides']=True
        elif row['layout']=='atlas':im,row['balanced_sides']=balanced_sides(im)
        else:row['balanced_sides']=True
        # Placement uses a larger working canvas; do not quadruple runtime VRAM
        # for original 512px artwork. Retain genuine higher-resolution sources.
        size=max(512,min(1024,max(row['width'],row['height'])))
        if im.size!=(size,size):im=im.resize((size,size),Image.Resampling.LANCZOS)
        row['width'],row['height']=im.size
        im.save(args.output/row['png'])
    report=dict(original,designs=output,consumed_layers=sorted(consumed),composite_count=len(combined))
    (args.output/'catalogue.json').write_text(json.dumps(report,indent=2)+'\n')
    print(len(rows),'source options ->',len(output),'selectable designs;',len(combined),'composites;',len(consumed),'layers composed')

if __name__=='__main__':main()
