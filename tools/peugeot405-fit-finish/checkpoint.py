"""Freeze r22 assets-only sources and visual evidence; exclude personal save payloads."""
from pathlib import Path
import datetime,hashlib,json,zipfile
r=Path.cwd();e=r/'out/validation/peugeot405-fit-finish-20261101';files=set()
for folder in [r/'tools/peugeot405-fit-finish',e/'final04-garage',e/'final04-full',e/'gallery',e/'before']:
 for p in folder.rglob('*'):
  if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.blend1':files.add(p)
for name in ['docs/peugeot405-fit-finish-2026-11-01.md','docs/ten-car-acceptance-2026-10-31.md','docs/peugeot405-refinement-2026-11-01.md','tools/peugeot405-photo-kits/build.py','tools/peugeot405-refinement/studio.py','tools/peugeot405-refinement/build_probe.py','mods/street-cars-wave2/mod.ini','mods/street-cars-wave2/README.md','mods/street-cars-wave2/assets/PEUGEOT405-FIT-FINISH.txt']:
 p=r/name;assert p.is_file();files.add(p)
for name in ['preparation.json','preparation.log','linux-delivery.json','linux-install.log','installed-composition.log','geometry04.json','native-check.log','seams04.json','seams04.log','mount-before.json','editable-sources.json','build04.log','studio04.log','final-guard.json','launcher-title-profile-menu.png','launcher-title-profile-menu.log']:
 p=e/name;assert p.is_file(),p;files.add(p)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();parent=r/'out/releases/OpenJuice-checkpoint-20261101-peugeot405-r21.zip'
assert sha(parent)=='9f55af78e3567faa8e3c04a355a98b3ad85a95fb21ffe91dbe6aaff8c6347be5'
paths={str(p.relative_to(r)):sha(p) for p in sorted(files)};runtime=json.loads((e/'preparation.json').read_text())
meta=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='Peugeot405 r22 workshop fit/finish; installed Linux / Wave2 revision20',parent_checkpoint=parent.name,parent_sha256=sha(parent),runtime_archive=runtime['archive'],runtime_sha256=runtime['sha256'],unchanged_elf_sha256=runtime['engine_sha256'],files=paths,private_save_payloads_included=False,limits=['Native slot/presentation verification; artistic acceptance remains with user.','Other cars/assets/tuning and actual personal files preserved.','No Android/macOS build or installation, public push, race/FPS certification.'])
archive=r/'out/releases/OpenJuice-checkpoint-20261101-peugeot405-r22.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in sorted(files):z.write(p,p.relative_to(r))
 z.writestr('CHECKPOINT.json',json.dumps(meta,indent=2)+'\n')
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None and len(z.namelist())==len(paths)+1
 for k,h in paths.items():assert hashlib.sha256(z.read(k)).hexdigest()==h,k
receipt=dict(archive=str(archive.relative_to(r)),bytes=archive.stat().st_size,sha256=sha(archive),payloads=len(paths),all_crc_and_payload_hashes_verified=True,private_save_payloads_included=False)
(e/'checkpoint-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');archive.with_suffix('.zip.sha256').write_text(receipt['sha256']+'  '+archive.name+'\n');print(json.dumps(receipt,indent=2))
