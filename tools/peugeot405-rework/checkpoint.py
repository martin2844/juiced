from pathlib import Path
import datetime,hashlib,json,zipfile
r=Path.cwd();e=r/'out/validation/peugeot405-rework-20261101';files=set()
def add(p):
 assert p.is_file() and p.suffix!='.zip',p
 files.add(p)
for folder in [r/'tools/peugeot405-rework',e/'final08',e/'gallery',e/'blender-final']:
 for p in folder.rglob('*'):
  if p.is_file() and '__pycache__' not in p.parts:add(p)
for name in ['docs/peugeot405-custom-shop-2026-11-01.md','docs/ten-car-acceptance-2026-10-31.md','mods/street-cars-wave2/README.md','mods/street-cars-wave2/mod.ini','mods/street-cars-wave2/assets/PEUGEOT405-CUSTOM-SHOP.txt']:
 add(r/name)
for name in ['preparation.json','linux-delivery.json','linux-install.log','installed-composition.log','final-native-check.log','editable-sources.json','build08.log','studio08.log','studio-final-render.log','baseline-geometry.json']:
 add(e/name)
# Only artifact/input inventories are included; no personal save bytes.
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();paths={str(p.relative_to(r)):sha(p) for p in sorted(files)}
parent=r/'out/releases/OpenJuice-checkpoint-20261031-r17-sources-and-delivery.zip'
assert sha(parent)=='2208bd0d418649c8785755284dece4d7d088bedda2f626e9931ff0085cf53a4e'
meta=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='Peugeot405-only custom shop r18; Linux installed, Android ZIP prepared but device unchanged',parent_checkpoint=parent.name,parent_sha256=sha(parent),runtime_archive='OpenJuice-Peugeot405-Custom-Shop-r18.zip',runtime_sha256='9ee89b4321fc84ec6004b0523320f94e94fb50fccd3e838f7e6d3ce152423de5',files=paths,private_save_payloads_included=False,limits=['Personal saves and other packages unchanged; source inventories are retained as hashes only.','This extends its parent checkpoint; full runtime is in the separately named archive.','Native presentation/refit/1322 checks verified; user aesthetic acceptance and RP5 testing remain open.'])
archive=r/'out/releases/OpenJuice-checkpoint-20261101-peugeot405-r18.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in sorted(files):z.write(p,p.relative_to(r))
 z.writestr('CHECKPOINT.json',json.dumps(meta,indent=2)+'\n')
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None and len(z.namelist())==len(paths)+1
 for k,h in paths.items():assert hashlib.sha256(z.read(k)).hexdigest()==h,k
receipt=dict(archive=str(archive.relative_to(r)),bytes=archive.stat().st_size,sha256=sha(archive),payloads=len(paths),all_crc_and_payload_hashes_verified=True,private_save_payloads_included=False)
(e/'checkpoint-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');archive.with_suffix('.zip.sha256').write_text(receipt['sha256']+'  '+archive.name+'\n');print(json.dumps(receipt,indent=2))
