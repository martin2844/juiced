"""Freeze r20/Wave2 revision18 after native art and tuning verification."""
from pathlib import Path
import hashlib,json,shutil,zipfile
r=Path.cwd();e=r/'out/validation/peugeot405-refinement-20261101';w=Path('/run/media/martin/Ediloco/OpenJuice-builds/20261101-peugeot405-refinement');source=w/'study04/street-cars-wave2';base=Path.home()/'.local/share/openjuice';ready=w/'delivery-r20/street-cars-wave2'
assert not ready.exists();shutil.copytree(source,ready)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();inv=lambda d:{p.relative_to(d).as_posix():sha(p) for p in sorted(d.rglob('*')) if p.is_file()}
before=inv(base/'mods/street-cars-wave2');candidate=inv(ready);original=json.loads((e/'before.json').read_text())
assert before==original['wave2'] and inv(base/'ModSaves')==original['personal'] and inv(r/'mods/street-cars-wave2')==before
changed=[k for k in before.keys()|candidate.keys() if before.get(k)!=candidate.get(k)]
allowed={'SHA256.json','patches/scripts/dynamic_peugeot_405_glx.txt.patch'}|{f'assets/cars/peugeot_405_glx{s}/mesh/{n}.d8m' for s in ['', '_ui'] for n in ['w257_chass']+['w257_kit'+str(i)+'_rbump' for i in [1,2,3]]+['w257_wing'+str(i) for i in [1,2,3]]}
assert set(changed)<=allowed,changed
p=ready/'mod.ini';text=p.read_text();assert 'revision=17\n' in text;p.write_text(text.replace('revision=17\n','revision=18\n'))
note='''
## r20 — Peugeot405 rear finish, wings, opaque grille and preparation

Three continuous moulded rear fascias replace the r19 lower add-on look,
retaining the original plate, source corner vinyl receivers and actual left
exhaust clearance. The three405 spoilers are now a low bonded boot lip,
a period painted bridge and a restrained painted GT foil with fitted moulded
feet. The existing upper grille has an opaque radiator backing and inward
facing roof/floor/side returns: its slats no longer look into an empty body.
The approved405-only built-engine/boost preparation leaves stock102BHP intact.
Complete street/sports/competition/prototype sets produce149/218/320/400BHP;
prototype reaches class5. The optional TurboUpgradeScale dynamics parameter
uses unity defaults for every other car. Existing parts and save identities
stay unchanged. Native builds with the r20 engine feature refresh this car's
derived rating on profile load, including previously installed parts.
Older executables safely retain legacy tuning (~216BHP maximum); this asset
ZIP alone does not upgrade an Android/macOS executable. Linux was updated.
Sources: tools/peugeot405-refinement. Earlier model/photo attribution remains.
'''
p=ready/'README.md';p.write_text(p.read_text()+note)
(ready/'assets/PEUGEOT405-REFINEMENT.txt').write_text(note)
shutil.copyfile(e/'performance-receipt.json',ready/'assets/PEUGEOT405-PREPARATION.json')
hashes=inv(ready);(ready/'SHA256.json').write_text(json.dumps({k:v for k,v in hashes.items() if k!='SHA256.json'},indent=2)+'\n');after=inv(ready)
archive=r/'out/releases/OpenJuice-Peugeot405-Refinement-r20.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for rel in after:
  assert not {'.cache','__pycache__','tools','.git'}.intersection(Path(rel).parts)
  z.write(ready/rel,'street-cars-wave2/'+rel)
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 for rel,h in after.items():assert hashlib.sha256(z.read('street-cars-wave2/'+rel)).hexdigest()==h,rel
others={p.name:inv(p) for p in (base/'mods').iterdir() if p.is_dir() and (p/'mod.ini').is_file() and p.name!='street-cars-wave2'};assert others==original['other_packages']
receipt=dict(archive=str(archive.relative_to(r)),bytes=archive.stat().st_size,sha256=sha(archive),candidate=str(ready),before=before,after=after,changed=sorted(k for k in before.keys()|after.keys() if before.get(k)!=after.get(k)),personal=original['personal'],other_packages=others,engine_sha256=sha(r/'out/linux/juiced'),prior_engine_sha256=original['engine_sha256'],all_crc_and_payload_hashes_verified=True)
assert receipt['engine_sha256']!=receipt['prior_engine_sha256']
(e/'preparation.json').write_text(json.dumps(receipt,indent=2)+'\n');archive.with_suffix('.zip.sha256').write_text(receipt['sha256']+'  '+archive.name+'\n');print(json.dumps({k:v for k,v in receipt.items() if k not in ['before','after','changed','personal','other_packages']},indent=2))
