"""Freeze a reviewed 405 candidate into a runtime ZIP; no installation or saves."""
from pathlib import Path
import hashlib,json,shutil,zipfile
r=Path.cwd();e=r/'out/validation/peugeot405-rework-20261101';w=Path('/run/media/martin/Ediloco/OpenJuice-builds/20261101-peugeot405');source=w/'study08/street-cars-wave2';base=Path.home()/'.local/share/openjuice';ready=w/'delivery-r18/street-cars-wave2'
assert not ready.exists();shutil.copytree(source,ready)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inv=lambda root:{p.relative_to(root).as_posix():sha(p) for p in sorted(root.rglob('*')) if p.is_file()}
before=inv(base/'mods/street-cars-wave2');candidate=inv(ready);changed=sorted(k for k in set(before)|set(candidate) if before.get(k)!=candidate.get(k))
assert all(k.startswith(('assets/cars/peugeot_405_glx/','assets/cars/peugeot_405_glx_ui/')) or k in ['patches/scripts/cardefs.txt.patch','SHA256.json'] for k in changed),changed
# The only catalogue additions are405 hood2/3, in their existing car block.
decode=lambda p:bytes.fromhex(p.read_text().splitlines()[1][5:]).decode('latin1')
old=decode(base/'mods/street-cars-wave2/patches/scripts/cardefs.txt.patch');new=decode(ready/'patches/scripts/cardefs.txt.patch');lines='CUSTOM BONNET "w257_vented_paint_BONNET" AERO\nCUSTOM BONNET "w257_vented_carbon_BONNET" AERO\n';assert new.count(lines)==1 and new.replace(lines,'')==old
p=ready/'mod.ini';t=p.read_text();assert 'revision=15\n' in t;p.write_text(t.replace('revision=15\n','revision=16\n'))
note='''
## r18 — Peugeot405 custom shop, 2026-11-01

The405 alone gets newly authored Street / Combat / Touring front/rear bumpers,
sculpted rockers and three purpose-built rear wings with measured deck shoes.
Two genuinely cut/louvered hoods append at indices2/3; existing carbon index1
and every other car/part identity remain intact. Native materials follow body
paint; original side-facing vinyl receiver faces remain. Opaque reflective
privacy glass hides the incomplete source cabin in close workshop views.
Sixty-six existing undertray faces receive dark material, retaining geometry.
These are original aftermarket designs inspired by the period, not exact Mi16
factory parts. No handling, sound, save format or production engine changes.
Editable Blender assemblies and authoring sources: tools/peugeot405-rework.
Review evidence: out/validation/peugeot405-rework-20261101.
Only the405 was reworked; the other imported cars' existing assets are unchanged.
'''
p=ready/'README.md';p.write_text(p.read_text()+note)
(ready/'assets/PEUGEOT405-CUSTOM-SHOP.txt').write_text('New aftermarket405 kit, louver and rear-wing geometry authored in Blender by the local OpenJuice port project,2026-11-01. Original405 source body, original materials/textures and source credits remain with their credited creators; see SOURCE-CREDITS.json. Mi16 silhouette reference: FIA homologation5380; designs are interpretations, not exact factory or homologation parts. Previous retail-donor wings on other cars are unchanged and remain credited separately in WINGS-ATTRIBUTION.txt.\n')
hashes=inv(ready);(ready/'SHA256.json').write_text(json.dumps({k:v for k,v in hashes.items() if k!='SHA256.json'},indent=2)+'\n')
after=inv(ready)
archive=r/'out/releases/OpenJuice-Peugeot405-Custom-Shop-r18.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for rel in after:
  assert not {'.cache','__pycache__','tools','.git'}.intersection(Path(rel).parts),rel
  z.write(ready/rel,'street-cars-wave2/'+rel)
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None and len(z.namelist())==len(after)
 for rel,h in after.items():assert hashlib.sha256(z.read('street-cars-wave2/'+rel)).hexdigest()==h,rel
receipt=dict(archive=str(archive.relative_to(r)),bytes=archive.stat().st_size,sha256=sha(archive),candidate=str(ready),before=before,after=after,changed=sorted(k for k in set(before)|set(after) if before.get(k)!=after.get(k)),personal=inv(base/'ModSaves'),other_packages={p.name:inv(p) for p in (base/'mods').iterdir() if p.is_dir() and (p/'mod.ini').is_file() and p.name!='street-cars-wave2'},engine_sha256=sha(r/'out/linux/juiced'),all_crc_and_payload_hashes_verified=True)
(e/'preparation.json').write_text(json.dumps(receipt,indent=2)+'\n');archive.with_suffix('.zip.sha256').write_text(receipt['sha256']+'  '+archive.name+'\n');print(json.dumps({k:v for k,v in receipt.items() if k not in ['before','after','changed','personal','other_packages']},indent=2))
