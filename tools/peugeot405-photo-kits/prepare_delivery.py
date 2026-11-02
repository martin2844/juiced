"""Freeze the reviewed r19 candidate. Does not install or write profiles."""
from pathlib import Path
import hashlib,json,shutil,zipfile
r=Path.cwd();e=r/'out/validation/peugeot405-photo-kits-20261101';w=Path('/run/media/martin/Ediloco/OpenJuice-builds/20261101-peugeot405-photo-kits');source=w/'study05/street-cars-wave2';base=Path.home()/'.local/share/openjuice';ready=w/'delivery-r19/street-cars-wave2'
assert not ready.exists();shutil.copytree(source,ready)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inv=lambda d:{p.relative_to(d).as_posix():sha(p) for p in sorted(d.rglob('*')) if p.is_file()}
before=inv(base/'mods/street-cars-wave2');candidate=inv(ready)
changed=[k for k in before.keys()|candidate.keys() if before.get(k)!=candidate.get(k)]
assert all(k=='SHA256.json' or k.startswith(('assets/cars/peugeot_405_glx/mesh/','assets/cars/peugeot_405_glx_ui/mesh/')) for k in changed),changed
assert inv(r/'mods/street-cars-wave2')==before
p=ready/'mod.ini';text=p.read_text();assert 'revision=16\n' in text;p.write_text(text.replace('revision=16\n','revision=17\n'))
note='''
## r19 — Peugeot405 photo-based kits, 2026-11-01

Replaces the r18 front/rear bumpers and skirts at the same three indices with
photo-derived Gutmann, Stratos and blue Morette-showcase interpretations.
Painted rolled intake rims, recessed diamond screens, rectangular/round fog
lamps and concave continuous skirts replace the generic inflated fascia.
The factory hood pressing and its original supplied normals are retained for
plain carbon and painted/carbon centre-scoop options. Old paired louvres are
removed. No new parts or save identities; the existing wings stay unchanged.
The third reference does not show its rear: that valance is an original
adaptation. Stock upper bumper seams, lamps, vinyl receivers and left exhaust
are retained rather than claiming exact commercial-kit copies. Source URLs,
observations and adaptations: assets/PEUGEOT405-PHOTO-REFERENCES.json.
Editable source and reproducible Blender pipeline: tools/peugeot405-photo-kits.
Only405 visuals change. Physics, engine, personal saves and other cars do not.
'''
p=ready/'README.md';p.write_text(p.read_text()+note)
shutil.copyfile(r/'tools/peugeot405-photo-kits/references.json',ready/'assets/PEUGEOT405-PHOTO-REFERENCES.json')
(ready/'assets/PEUGEOT405-CUSTOM-SHOP.txt').write_text('r19:405 kit and bonnet revisions authored in Blender by the local OpenJuice port project,2026-11-01. Photo-derived interpretations, not exact commercial-kit replicas. Source405 model/textures retain original attribution in SOURCE-CREDITS.json. New reference provenance and limitations: PEUGEOT405-PHOTO-REFERENCES.json. Existing r18 wings unchanged.\n')
hashes=inv(ready);(ready/'SHA256.json').write_text(json.dumps({k:v for k,v in hashes.items() if k!='SHA256.json'},indent=2)+'\n');after=inv(ready)
archive=r/'out/releases/OpenJuice-Peugeot405-Photo-Kits-r19.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for rel in after:
  assert not {'.cache','__pycache__','tools','.git'}.intersection(Path(rel).parts),rel
  z.write(ready/rel,'street-cars-wave2/'+rel)
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 for rel,h in after.items():assert hashlib.sha256(z.read('street-cars-wave2/'+rel)).hexdigest()==h,rel
receipt=dict(archive=str(archive.relative_to(r)),bytes=archive.stat().st_size,sha256=sha(archive),candidate=str(ready),before=before,after=after,changed=sorted(k for k in before.keys()|after.keys() if before.get(k)!=after.get(k)),personal=inv(base/'ModSaves'),other_packages={p.name:inv(p) for p in (base/'mods').iterdir() if p.is_dir() and (p/'mod.ini').is_file() and p.name!='street-cars-wave2'},engine_sha256=sha(r/'out/linux/juiced'),all_crc_and_payload_hashes_verified=True)
(e/'preparation.json').write_text(json.dumps(receipt,indent=2)+'\n');archive.with_suffix('.zip.sha256').write_text(receipt['sha256']+'  '+archive.name+'\n');print(json.dumps({k:v for k,v in receipt.items() if k not in ['before','after','changed','personal','other_packages']},indent=2))
