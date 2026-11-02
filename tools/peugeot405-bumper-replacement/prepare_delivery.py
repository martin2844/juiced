"""Freeze the reviewed r21 candidate and verify every runtime ZIP payload."""
from pathlib import Path
import datetime,hashlib,json,shutil,zipfile
r=Path.cwd();e=r/'out/validation/peugeot405-bumper-replacement-20261101';w=Path('/run/media/martin/Ediloco/OpenJuice-builds/20261101-peugeot405-bumper-replacement')
base=Path.home()/'.local/share/openjuice';live=base/'mods/street-cars-wave2';source=w/'study03/street-cars-wave2';ready=w/'delivery-r21/street-cars-wave2'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inv=lambda d:{p.relative_to(d).as_posix():sha(p) for p in sorted(d.rglob('*')) if p.is_file()}
before=inv(live);personal=inv(base/'ModSaves');others={p.name:inv(p) for p in (base/'mods').iterdir() if p.is_dir() and (p/'mod.ini').is_file() and p!=live}
assert inv(r/'mods/street-cars-wave2')==before
assert len(personal)==6
assert sha(r/'out/linux/juiced')=='5df97d1c286a8066ff784aa2f01fde658cde860a5dd5e714b00d5041d90de91f'
assert json.loads((e/'geometry.json').read_text())['fixed_body_contains_no_factory_bumper_shell']
for folder,count in [('final-isolation',48),('final-full',64)]:assert json.loads((e/folder/'capture.json').read_text())['frames']==count
assert 'CHECKS 1322 PASSED 1322 FAILED 0' in (e/'native-check.log').read_text()
assert not ready.exists();shutil.copytree(source,ready)
p=ready/'mod.ini';s=p.read_text();assert 'revision=18\n' in s;p.write_text(s.replace('revision=18\n','revision=19\n'))
note='''
## r21 — Peugeot405 complete bumper replacements

The three front/rear choices now replace the complete factory bumper shell.
Factory lights, grille and fixed body panels belong to CHASS rather than being
duplicated in each custom bumper. Factory bumper surfaces remain only in their
stock slots. Exact triangle clipping preserves factory geometry/UVs/normals when
stock is selected. Custom shells discard the inherited factory skins and have
new body-matching mounting collars and continuous wheel-arch returns. The plate
is retained as two original UV-mapped triangles in the new rear recess.
No catalogue IDs, parts, saves, other cars, hoods, skirts, wings, tuning or engine
were changed. The approved r20 102-to-400BHP preparation remains unchanged.
Verified in the Linux native renderer; Android installation is not included.
Sources and editable Blender assembly: tools/peugeot405-bumper-replacement.
Earlier model and photograph attribution remains applicable.
'''
p=ready/'README.md';p.write_text(p.read_text()+note)
(ready/'assets/PEUGEOT405-BUMPER-REPLACEMENT.txt').write_text(note)
hashes=inv(ready);(ready/'SHA256.json').write_text(json.dumps({k:v for k,v in hashes.items() if k!='SHA256.json'},indent=2)+'\n');after=inv(ready)
archive=r/'out/releases/OpenJuice-Peugeot405-Bumper-Replacement-r21.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for rel in after:
  assert not {'.cache','__pycache__','tools','.git'}.intersection(Path(rel).parts)
  z.write(ready/rel,'street-cars-wave2/'+rel)
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 for rel,h in after.items():assert hashlib.sha256(z.read('street-cars-wave2/'+rel)).hexdigest()==h,rel
backup=base/'save-backups'/('before-405-r21-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
shutil.copytree(base/'ModSaves',backup);assert inv(backup)==personal
receipt=dict(archive=str(archive.relative_to(r)),bytes=archive.stat().st_size,sha256=sha(archive),candidate=str(ready),before=before,after=after,personal=personal,save_backup=str(backup),other_packages=others,engine_sha256=sha(r/'out/linux/juiced'),all_crc_and_payload_hashes_verified=True)
(e/'preparation.json').write_text(json.dumps(receipt,indent=2)+'\n');archive.with_suffix('.zip.sha256').write_text(receipt['sha256']+'  '+archive.name+'\n')
print(json.dumps({k:v for k,v in receipt.items() if k not in ['before','after','personal','other_packages']},indent=2))
