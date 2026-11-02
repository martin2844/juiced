"""Freeze the reviewed r23 candidate and verify every runtime ZIP payload."""
from pathlib import Path
import datetime,hashlib,json,shutil,zipfile
r=Path.cwd();e=r/'out/validation/peugeot405-oem-fit-20261101';w=Path('/run/media/martin/Ediloco/OpenJuice-builds/20261101-peugeot405-oem-fit')
base=Path.home()/'.local/share/openjuice';live=base/'mods/street-cars-wave2';source=w/'study08/street-cars-wave2';ready=w/'delivery-r23/street-cars-wave2'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inv=lambda d:{p.relative_to(d).as_posix():sha(p) for p in sorted(d.rglob('*')) if p.is_file()}
before=inv(live);personal=inv(base/'ModSaves');others={p.name:inv(p) for p in (base/'mods').iterdir() if p.is_dir() and (p/'mod.ini').is_file() and p!=live}
assert inv(r/'mods/street-cars-wave2')==before
assert len(personal)==6
assert sha(r/'out/linux/juiced')=='5df97d1c286a8066ff784aa2f01fde658cde860a5dd5e714b00d5041d90de91f'
assert json.loads((e/'geometry08.json').read_text())['stock_chassis_and_bumpers_byte_identical']
for folder,count in [('review08',70),('final08-full',64)]:assert json.loads((e/folder/'capture.json').read_text())['frames']==count
assert 'CHECKS 1322 PASSED 1322 FAILED 0' in (e/'native-check.log').read_text()
assert not ready.exists();shutil.copytree(source,ready)
p=ready/'mod.ini';s=p.read_text();assert 'revision=20\n' in s;p.write_text(s.replace('revision=20\n','revision=21\n'))
note='''
## r23 — Peugeot405 kits fitted around the actual OEM perimeter

Six bumper replacements now follow the real U-shaped OEM contour, including
its overhang and the return toward the wheel. A narrow original mounting band
is the single outer skin at the joint; the new fascia is the single skin below
it. Lower sections have controlled convex curvature and taper, with the stock
wheel centres unchanged. Lamp/intake proportions are preserved by taking extra
height in the plain mounting section. Opaque, double-sided textured interior
walls replace Constant Colour reveals whose material class ignores a supplied
Double-sided setting. The original fluted glass texture is reused for fog lamps.
Only six bumper mesh pairs and their local interior material/texture are new.
Fixed body, stock bumpers, other405 assets, other cars, catalogue and physics are
byte-identical to r22. The approved102-to-400BHP preparation remains unchanged.
Reviewed using the real workshop and selector. Linux only installation here.
Sources and editable Blender files: tools/peugeot405-oem-fit.
The earlier source model and photograph attribution remains applicable.
'''
p=ready/'README.md';p.write_text(p.read_text()+note)
(ready/'assets/PEUGEOT405-OEM-FIT.txt').write_text(note)
hashes=inv(ready);(ready/'SHA256.json').write_text(json.dumps({k:v for k,v in hashes.items() if k!='SHA256.json'},indent=2)+'\n');after=inv(ready)
archive=r/'out/releases/OpenJuice-Peugeot405-OEM-Fit-r23.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for rel in after:
  assert not {'.cache','__pycache__','tools','.git'}.intersection(Path(rel).parts)
  z.write(ready/rel,'street-cars-wave2/'+rel)
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 for rel,h in after.items():assert hashlib.sha256(z.read('street-cars-wave2/'+rel)).hexdigest()==h,rel
backup=base/'save-backups'/('before-405-r23-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
shutil.copytree(base/'ModSaves',backup);assert inv(backup)==personal
receipt=dict(archive=str(archive.relative_to(r)),bytes=archive.stat().st_size,sha256=sha(archive),candidate=str(ready),before=before,after=after,personal=personal,save_backup=str(backup),other_packages=others,engine_sha256=sha(r/'out/linux/juiced'),all_crc_and_payload_hashes_verified=True)
(e/'preparation.json').write_text(json.dumps(receipt,indent=2)+'\n');archive.with_suffix('.zip.sha256').write_text(receipt['sha256']+'  '+archive.name+'\n')
print(json.dumps({k:v for k,v in receipt.items() if k not in ['before','after','personal','other_packages']},indent=2))
