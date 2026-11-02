"""Freeze r20 sources, native engine and review evidence; no personal save bytes."""
from pathlib import Path
import datetime,hashlib,json,zipfile
r=Path.cwd();e=r/'out/validation/peugeot405-refinement-20261101';files=set()
for folder in [r/'tools/peugeot405-refinement',e/'final04',e/'gallery',e/'grille-native',e/'blender-final04']:
 for p in folder.rglob('*'):
  if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.blend1':files.add(p)
for name in ['src/Core/Cars/CarPhysics.cpp','src/Core/Physics/Dynamics.cpp','src/Core/Physics/Dynamics.h','src/Game/Frontend/FrontendSaves.cpp','tests/Unit/Physics/TuningPreparationTests.cpp','docs/spec/07-dynamics.md','docs/peugeot405-refinement-2026-11-01.md','docs/ten-car-acceptance-2026-10-31.md','mods/street-cars-wave2/mod.ini','mods/street-cars-wave2/README.md','mods/street-cars-wave2/assets/PEUGEOT405-REFINEMENT.txt','mods/street-cars-wave2/assets/PEUGEOT405-PREPARATION.json','mods/street-cars-wave2/patches/scripts/dynamic_peugeot_405_glx.txt.patch','out/linux/juiced']:
 p=r/name;assert p.is_file();files.add(p)
for name in ['before.json','preparation.json','linux-delivery.json','linux-install.log','installed-composition.log','native-check.log','geometry.json','grille-rays.json','grille-rays.log','grille-native.log','performance-before.log','performance-final.log','performance-receipt.json','final-guard.json','save-backup.json','editable-sources.json','build04.log','studio04.log','studio-final04-render.log','native-build.log','final-build.log','unit-tests.log','controller-test.log','real-profile-copy-menu.png','real-profile-copy-menu.log','full-launcher-verified.png','full-launcher-verified.log','GrillePreview.cpp','build_grille_preview.py','seed-private.log']:
 p=e/name;assert p.is_file(),p;files.add(p)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();parent=r/'out/releases/OpenJuice-checkpoint-20261101-peugeot405-r19.zip'
assert sha(parent)=='85ac0abbbd097d83a06115ccb01759d867561daccac46580de796f63c1d76972'
paths={str(p.relative_to(r)):sha(p) for p in sorted(files)};runtime=json.loads((e/'preparation.json').read_text())
meta=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='Peugeot405 r20 rear/spoiler/grille refinement and explicitly approved400BHP preparation; Linux installed',parent_checkpoint=parent.name,parent_sha256=sha(parent),runtime_archive=runtime['archive'],runtime_sha256=runtime['sha256'],native_elf_sha256=runtime['engine_sha256'],files=paths,private_save_payloads_included=False,limits=['Other cars and shared catalogue unchanged; user artistic acceptance remains open.','Stock102BHP retained;400BHP is an approved game preparation, not a real-car specification.','No Android/macOS build, public push, race-stability or FPS certification.','Native presentation, controlled cached-rating reload, flat-ground power effects,415 unit and1322 native loading checks verified.'])
archive=r/'out/releases/OpenJuice-checkpoint-20261101-peugeot405-r20.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in sorted(files):z.write(p,p.relative_to(r))
 z.writestr('CHECKPOINT.json',json.dumps(meta,indent=2)+'\n')
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None and len(z.namelist())==len(paths)+1
 for k,h in paths.items():assert hashlib.sha256(z.read(k)).hexdigest()==h,k
receipt=dict(archive=str(archive.relative_to(r)),bytes=archive.stat().st_size,sha256=sha(archive),payloads=len(paths),all_crc_and_payload_hashes_verified=True,private_save_payloads_included=False)
(e/'checkpoint-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');archive.with_suffix('.zip.sha256').write_text(receipt['sha256']+'  '+archive.name+'\n');print(json.dumps(receipt,indent=2))
