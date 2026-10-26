"""Asset conservation and native texture format checks for the refined collection."""
import argparse,json,struct
from pathlib import Path
import numpy as np
from PIL import Image

p=argparse.ArgumentParser();p.add_argument('decoded',type=Path);p.add_argument('refined',type=Path);p.add_argument('package',type=Path)
a=p.parse_args()
original=json.loads((a.decoded/'catalogue.json').read_text())['designs']
rows=json.loads((a.refined/'catalogue.json').read_text())['designs']
source_ids=[layer['id'] for r in rows for layer in r.get('layers',[r])]
assert sorted(source_ids)==sorted(r['id'] for r in original),'Lost or duplicated source artwork'
assert len(set(r['id'] for r in rows))==len(rows),'Duplicate catalogue IDs'
assert not any(r['label']=='2f2fskylinev2' and r['texture']=='2.dds' for r in rows),'Opacity mask exposed as art'
assert all(r['model']=='RX7' for r in rows if r['label']=='rx7sun1-2'),'RX7 file misclassified as S2000'
for r in rows:
    im=Image.open(a.refined/r['png'])
    assert (np.array(im)[:,:,3]>10).any(),f'Empty artwork: {r["id"]}'
    data=(a.package/'assets/cartextures'/('decal_nfs_'+r['id'].lower()+'.ddt')).read_bytes()
    fmt,w,h,levels=struct.unpack_from('<4I',data)
    assert fmt==21 and (w,h)==im.size and levels>1
    n=44
    for _ in range(levels):n+=w*h*4;w=max(1,w//2);h=max(1,h//2)
    assert n==len(data) and w==h==1,'Truncated or malformed mip chain'
print(f'PASS {len(source_ids)} source layers preserved in {len(rows)} options; masks, cars, opacity and mip chains checked')
