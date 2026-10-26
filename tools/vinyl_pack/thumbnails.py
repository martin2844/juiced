"""Use real native-car captures for the workshop selector, instead of flat UV atlases."""
from pathlib import Path
import argparse,json,struct
import numpy as np
from PIL import Image
from build import ddt

p=argparse.ArgumentParser();p.add_argument('package',type=Path);p.add_argument('captures',type=Path)
a=p.parse_args()
rows=json.loads((a.package/'catalogue.json').read_text())
for r in rows:
    raw=(a.package/'assets/cartextures'/('decal_nfs_'+r['id'].lower()+'.ddt')).read_bytes()
    _,w,h=struct.unpack_from('<3I',raw)
    alpha=np.frombuffer(raw,dtype='uint8',offset=44,count=w*h*4).reshape(h,w,4)[:,:,3].astype('int64')
    sides=alpha[:int(h*.25)].sum()+alpha[int(h*.55):int(h*.83)].sum()
    angle=2 if sides<alpha.sum()*.25 else 0
    source=a.captures/f'{r["car"]}_{r["mesh"]}_{angle}.bmp'
    with Image.open(source) as frame:
        crop=frame.convert('RGBA').crop((65,220,1150,555) if angle==0 else (300,180,1040,570))
        crop.thumbnail((128,64),Image.Resampling.LANCZOS)
        thumbnail=Image.new('RGBA',(128,64),(0,0,0,0))
        thumbnail.alpha_composite(crop,((128-crop.width)//2,(64-crop.height)//2))
    path=a.package/'assets/ui/textures'/('ui_'+r['mesh'].lower()+'.ddt')
    path.write_bytes(ddt(thumbnail))
print('Created',len(rows),'native workshop thumbnails')
