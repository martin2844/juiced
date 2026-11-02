"""Paint only measured grey cladding faces of private Outback kit3.

Usage: python THIS REPO MORPH_CANDIDATE FRESH_CANDIDATE RECEIPT
Uses the existing native body material: no additional texture or GPU allocation.
Source grille, lamps and plate atlas regions remain on their original material.
"""
from pathlib import Path
import io,struct,json,hashlib,shutil,sys
import numpy as np
from PIL import Image

repo,base,out,receipt=map(Path,sys.argv[1:5]);sys.path[:0]=[str(repo/'tools'),str(repo/'mods/ferrari/tools')]
options=sys.argv[5:]
assert options in [[],['--skirts-only']],options
zones=['sskirt_l','sskirt_r'] if options else ['fbump','rbump','sskirt_l','sskirt_r']
from vinyl_pack import mesh
from import_ferrari import bounds
if out.exists():raise RuntimeError('Fresh candidate required')
data=(base/'assets/cars/outback_2002/textures/w261_m0.ddt').read_bytes()
fmt,width,height=struct.unpack_from('<3I',data)
if fmt==21:pic=np.frombuffer(data,dtype=np.uint8,count=width*height*4,offset=44).reshape(height,width,4)[:,:,[2,1,0,3]]
else:
    n=width*height//2 if fmt==int.from_bytes(b'DXT1','little') else width*height
    header=[124,0x81007,height,width,n,0,1]+[0]*11+[32,4,fmt,0,0,0,0,0]+[0x1000,0,0,0,0]
    pic=np.array(Image.open(io.BytesIO(b'DDS '+struct.pack('<31I',*header)+data[44:])).convert('RGBA'))
shutil.copytree(base,out);results=[]
for suffix in ['','_ui']:
    folder=out/'assets/cars'/('outback_2002'+suffix)/'mesh'
    for zone in zones:
        path=folder/('w261_kit3_'+zone+'.d8m');a=mesh.read(path.read_bytes());chunks=[];indices=[];first=0;count=0;paint_faces=[]
        for index,p in enumerate(a['parts']):
            tri=mesh.triangles(a,p);pos=p['streams'][0,0];keep=np.zeros(len(tri),dtype=bool)
            if p['material']=='w261_m0':
                uv=p['streams'][5,0][tri];samples=np.concatenate([uv,uv.mean(1)[:,None,:],(uv+np.roll(uv,1,axis=1))*.5],axis=1)
                x=np.clip((samples[:,:,0]*width).astype(int),0,width-1);y=np.clip((samples[:,:,1]*height).astype(int),0,height-1)
                rgb=pic[y,x,:3].astype(float);grey=(rgb.max(2)-rgb.min(2)<26)&(rgb.mean(2)>65)&(rgb.mean(2)<215)
                # The atlas was inspected: top-right plate and top-centre lamp
                # islands must never acquire the player's body paint.
                centre=samples[:,3,:]
                excluded=((centre[:,0]>.68)&(centre[:,1]<.25))|((centre[:,0]>.23)&(centre[:,0]<.70)&(centre[:,1]<.25))
                # UV border vertices can sample the atlas padding of an otherwise
                # uniform cladding face. Use its interior sample and majority,
                # not a requirement that almost every border pixel is grey.
                ceiling=65 if zone=='fbump' else 90
                keep=grey[:,3]&(grey.mean(1)>=.5)&~excluded&(pos[tri][...,1].mean(1)<ceiling)
            for paint in [False,True]:
                faces=tri[keep if paint else ~keep]
                if not len(faces):continue
                header=bytearray(p['header']);name='outback_2002_body' if paint else p['material']
                header[:32]=name.encode().ljust(32,b'\0');struct.pack_into('<3I',header,32,first,faces.size,1)
                chunks.append(bytes(header)+bytes(p['verts']));indices.extend(faces.ravel().tolist());first+=faces.size
                if paint:count+=len(faces);paint_faces.append(dict(source_part=index,triangle_indices=np.flatnonzero(keep).tolist()))
        assert count>0,zone
        payload=struct.pack('<I',len(indices))+np.array(indices,dtype='<u2').tobytes()+struct.pack('<I',len(chunks))+b''.join(chunks)+a['bounds']
        path.write_bytes(payload);new=mesh.read(payload)
        assert sum(len(mesh.triangles(new,p)) for p in new['parts'])==sum(len(mesh.triangles(a,p)) for p in a['parts'])
        results.append(dict(file=str(path.relative_to(out)),painted_triangles=count,paint_faces=paint_faces,sha256=hashlib.sha256(payload).hexdigest()))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
old={p.relative_to(base).as_posix():sha(p) for p in base.rglob('*') if p.is_file()};now={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file()}
assert set(old)==set(now)
assert {p for p in old if old[p]!=now[p]}=={x['file'] for x in results}
(out/'SHA256.json').write_text(json.dumps({p:h for p,h in now.items() if p!='SHA256.json'},indent=2)+'\n')
receipt.write_text(json.dumps(dict(status='NOT INSTALLED; native visual review required',base=str(base),candidate=str(out),results=results,no_new_textures=True,all_other_files_unchanged=True),indent=2)+'\n')
print([(x['file'],x['painted_triangles']) for x in results])
