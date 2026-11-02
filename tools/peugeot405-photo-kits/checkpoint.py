"""Save the final source/evidence chain, excluding every personal save byte."""
from pathlib import Path
import datetime,hashlib,json,zipfile
r=Path.cwd();e=r/'out/validation/peugeot405-photo-kits-20261101';files=set()
for folder in [r/'tools/peugeot405-photo-kits',e/'final05',e/'gallery',e/'blender-final05',e/'references']:
 for p in folder.rglob('*'):
  if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ['.blend1']:files.add(p)
for name in ['docs/peugeot405-photo-kits-2026-11-01.md','docs/ten-car-acceptance-2026-10-31.md','mods/street-cars-wave2/mod.ini','mods/street-cars-wave2/README.md','mods/street-cars-wave2/assets/PEUGEOT405-PHOTO-REFERENCES.json']:
 files.add(r/name)
for name in ['preparation.json','linux-delivery.json','linux-install.log','installed-composition.log','final-native-check.log','geometry.json','final-guard.json','editable-sources.json','build05.log','studio05.log','studio-final05-render.log','full-launcher-verified.png','full-launcher-verified.log']:
 p=e/name;assert p.is_file(),p;files.add(p)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();paths={str(p.relative_to(r)):sha(p) for p in sorted(files)}
parent=r/'out/releases/OpenJuice-checkpoint-20261101-peugeot405-r18.zip'
assert sha(parent)=='82f71967698199764882356b2c73379fae23cb4b4713bbfc7ae4f7e646f44449'
runtime=json.loads((e/'preparation.json').read_text())
meta=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='Peugeot405 photo-derived kits r19 / Wave2 revision17; installed in Linux only',parent_checkpoint=parent.name,parent_sha256=sha(parent),runtime_archive=runtime['archive'],runtime_sha256=runtime['sha256'],files=paths,private_save_payloads_included=False,limits=['Photo-derived adaptations, not exact commercial-kit scans; third rear not visible in source photos.','Existing r18 wings and incomplete hidden source interior unchanged.','Native presentation/refit and preserved saves verified; user artistic approval pending; no Android/FPS claims.'])
archive=r/'out/releases/OpenJuice-checkpoint-20261101-peugeot405-r19.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in sorted(files):z.write(p,p.relative_to(r))
 z.writestr('CHECKPOINT.json',json.dumps(meta,indent=2)+'\n')
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None and len(z.namelist())==len(paths)+1
 for k,h in paths.items():assert hashlib.sha256(z.read(k)).hexdigest()==h,k
receipt=dict(archive=str(archive.relative_to(r)),bytes=archive.stat().st_size,sha256=sha(archive),payloads=len(paths),all_crc_and_payload_hashes_verified=True,private_save_payloads_included=False)
(e/'checkpoint-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');archive.with_suffix('.zip.sha256').write_text(receipt['sha256']+'  '+archive.name+'\n');print(json.dumps(receipt,indent=2))
