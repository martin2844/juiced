"""Material-only privacy glazing and a clean Volvo240 registration plate.

python THIS REPO BASELINE_PACKAGE FRESH_CANDIDATE EVIDENCE
Does not install anything or rewrite source geometry, physics or saved games.
"""
from pathlib import Path
import hashlib,json,shutil,struct,sys
import numpy as np
from PIL import Image,ImageDraw,ImageFont

repo,base,out,e=map(Path,sys.argv[1:]);e.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(repo/'tools'));from vinyl_pack import mesh
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert not out.exists();shutil.copytree(base,out);rows=[]

# New vector/text artwork, independent of the downloaded author's plate bitmap.
svg='<svg xmlns="http://www.w3.org/2000/svg" width="512" height="112" viewBox="0 0 512 112"><rect x="2" y="2" width="508" height="108" rx="8" fill="#ededdf" stroke="#17191c" stroke-width="4"/><text x="256" y="87" text-anchor="middle" font-family="DejaVu Sans" font-weight="bold" font-size="82" fill="#17191c">JUICED</text></svg>\n'
(e/'volvo240-plate.svg').write_text(svg)
fontpath=next(Path('/nix/store').glob('*dejavu-fonts-*/share/fonts/truetype/DejaVuSans-Bold.ttf'))
image=Image.new('RGBA',(512,112),'#ededdf');draw=ImageDraw.Draw(image);draw.rounded_rectangle((2,2,509,109),radius=8,outline='#17191c',width=4)
font=ImageFont.truetype(str(fontpath),82);draw.text((256,56),'JUICED',font=font,anchor='mm',fill='#17191c');image.save(e/'volvo240-plate.png')
pixels=np.array(image);ddt=struct.pack('<9IfI',21,512,112,1,2,2,0,1,1,0.,1)+pixels[:,:,[2,1,0,3]].tobytes()
for suffix in ['','_ui']:
    folder=out/'assets/cars'/('volvo_240_turbo'+suffix)
    (folder/'textures/w262_juiced_plate.ddt').write_bytes(ddt)
    (folder/'textures/w262_plate_blank.ddt').write_bytes(struct.pack('<9IfI',21,1,1,1,2,2,0,1,1,0.,1)+bytes([223,237,237,255]))
    background=folder/'mesh/mats/w262_m9.pmd'
    background.write_text(background.read_text().replace('w262_m9.ptd','w262_plate_blank.ptd'))
    (folder/'mesh/mats/w262_juiced_plate.pmd').write_text('Material Program = Specular Opaque\nSpecular Colour = 35 35 35\nSpecular Highlight = 10\nShininess = 16\nDouble-sided = TRUE\nOpaque Texture = w262_juiced_plate.ptd\n')
    for path in sorted((folder/'mesh').glob('*.d8m')):
        data=path.read_bytes();a=mesh.read(data);changed=0
        for p in a['parts']:
            if p['material'] not in ['w262_m10','w262_juiced_plate']:continue
            uv=p['streams'][5,0];lo=uv.min(0);hi=uv.max(0);assert np.all(hi>lo)
            uv[:]=(uv-lo)/(hi-lo)
            # Native V=0 is the bitmap's top. The source letter quad's UVs had
            # V increasing with world height; new text must remain upright.
            y=p['streams'][0,0][:,1];uv[:,1]=(y.max()-y)/(y.max()-y.min())
            p['header'][:32]=b'w262_juiced_plate'.ljust(32,b'\0');changed+=1
        if changed:
            new=mesh.write(a);b=mesh.read(new);old=mesh.read(data)
            assert b['prefix']==old['prefix'] and b['bounds']==old['bounds']
            for p,q in zip(old['parts'],b['parts']):
                assert p['streams'][0,0].tobytes()==q['streams'][0,0].tobytes()
                assert p['streams'][3,0].tobytes()==q['streams'][3,0].tobytes()
                assert p['header'][32:]==q['header'][32:]
            path.write_bytes(new);rows.append(dict(path=path.relative_to(out).as_posix(),plate_parts=changed,positions_normals_indices_unchanged=True))
    folder=out/'assets/cars'/('peugeot_405_glx'+suffix)
    path=folder/'mesh/w257_chass.d8m';old=path.read_bytes();a=mesh.read(old);count=0
    for p in a['parts']:
        if p['material'] in ['peugeot_405_glx_glass','w257_privacy_glass']:p['header'][:32]=b'w257_privacy_glass'.ljust(32,b'\0');count+=1
    assert count==1
    path.write_bytes(mesh.write(a));rows.append(dict(path=path.relative_to(out).as_posix(),privacy_glass_parts=count,geometry_and_vertex_attributes_unchanged=True))
    # A distinct native glass material keeps the privacy floor independent of
    # CarModel::tint's stock zero-opacity override on <model>_glass.
    (folder/'mesh/mats/w257_privacy_glass.pmd').write_text('Material Program = Car Glass\nSide Fresnel = 0.12\nFront Fresnel = 0.02\nBase Opacity = 0.96\nStart Fade = 0\nEnd Fade = 10000\nColour = 28 33 40\nTexture = peugeot_405_glx_glass.ptd\nScratch = w257_scratch.ptd\n')
hashes={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file() and p.name!='SHA256.json'}
(out/'SHA256.json').write_text(json.dumps(hashes,indent=2)+'\n')
(e/'privacy-and-plate-candidate.json').write_text(json.dumps(dict(candidate=str(out),changes=rows,source_font=str(fontpath),new_plate='JUICED; new vector/text artwork',privacy_glass='96% native glass opacity independent of profile tint; retained glazing geometry',installed=False),indent=2)+'\n')
print('Prepared Volvo240 clean plates and405 privacy glazing; native visual check pending.')
