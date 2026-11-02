"""Freeze r23 fitted bumper sources and selected native evidence, excluding private saves."""
from pathlib import Path
import datetime,hashlib,json,zipfile
r=Path.cwd();e=r/'out/validation/peugeot405-oem-fit-20261101';files=set()
for folder in [r/'tools/peugeot405-oem-fit',e/'review08',e/'final08-full',e/'gallery']:
 for p in folder.rglob('*'):
  if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.blend1':files.add(p)
for name in ['docs/peugeot405-oem-fit-2026-11-01.md','docs/ten-car-acceptance-2026-10-31.md','docs/peugeot405-fit-finish-2026-11-01.md','tools/peugeot405-photo-kits/build.py','tools/peugeot405-photo-kits/references.json','tools/peugeot405-refinement/studio.py','src/Mods/Packages.cpp','src/Render/Materials/StandardMaterials.h','mods/street-cars-wave2/mod.ini','mods/street-cars-wave2/README.md','mods/street-cars-wave2/assets/PEUGEOT405-OEM-FIT.txt']:
 p=r/name;assert p.is_file();files.add(p)
for name in ['preparation.json','preparation.log','linux-delivery.json','linux-install.log','installed-composition.log','geometry08.json','stock-comparison.json','native-check.log','editable-sources.json','build08.log','studio08.log','final-guard.json','active-overlay-before.json','active-overlay-after.json','launcher-title-profile-menu.log','launcher-title-profile-menu.png']:
 p=e/name;assert p.is_file(),p;files.add(p)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();parent=r/'out/releases/OpenJuice-checkpoint-20261101-peugeot405-r22.zip'
assert sha(parent)=='fd3480665bcde03c2ef7c9c70ccdc589fd53af1e6724b75505faa1f31f272dd9'
runtime=json.loads((e/'preparation.json').read_text());paths={p.relative_to(r).as_posix():sha(p) for p in sorted(files)}
meta=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='Peugeot405 r23 OEM perimeter bumper fit; installed Linux/Wave2 revision21',parent_checkpoint=parent.name,parent_sha256=sha(parent),runtime_archive=runtime['archive'],runtime_sha256=runtime['sha256'],unchanged_elf_sha256=runtime['engine_sha256'],files=paths,private_save_payloads_included=False,limits=['Native workshop/selector presentation and loading verified; user artistic acceptance remains open.','Existing game kept its old immutable overlay and must be reopened to see revision21.','No Android/macOS installation, public push, race-stability or FPS certification.'])
archive=r/'out/releases/OpenJuice-checkpoint-20261101-peugeot405-r23.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in sorted(files):z.write(p,p.relative_to(r))
 z.writestr('CHECKPOINT.json',json.dumps(meta,indent=2)+'\n')
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None and len(z.namelist())==len(paths)+1
 for path,h in paths.items():assert hashlib.sha256(z.read(path)).hexdigest()==h,path
receipt=dict(archive=str(archive.relative_to(r)),bytes=archive.stat().st_size,sha256=sha(archive),payloads=len(paths),all_crc_and_payload_hashes_verified=True,private_save_payloads_included=False)
(e/'checkpoint-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');archive.with_suffix('.zip.sha256').write_text(receipt['sha256']+'  '+archive.name+'\n');print(json.dumps(receipt,indent=2))
