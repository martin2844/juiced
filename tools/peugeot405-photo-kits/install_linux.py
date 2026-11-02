"""Install the frozen 405-only runtime with exact guards and reversible exchanges."""
from pathlib import Path
import ctypes,hashlib,json,os,shutil,subprocess
r=Path.cwd();e=r/'out/validation/peugeot405-photo-kits-20261101';p=json.loads((e/'preparation.json').read_text());base=Path.home()/'.local/share/openjuice';ready=Path(p['candidate']);live=base/'mods/street-cars-wave2';repo=r/'mods/street-cars-wave2'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();inv=lambda d:{p.relative_to(d).as_posix():sha(p) for p in sorted(d.rglob('*')) if p.is_file()}
assert inv(live)==p['before'] and inv(repo)==p['before'],'Installed/repository baseline changed'
assert inv(ready)==p['after'] and inv(base/'ModSaves')==p['personal']
assert sha(r/'out/linux/juiced')==p['engine_sha256']
for name,h in p['other_packages'].items():assert inv(base/'mods'/name)==h,name
stages=[(live,e/'install-live-ready',e/'r18-installed-rollback'),(repo,e/'install-repository-ready',e/'r18-repository-rollback')]
for dest,stage,backup in stages:
 assert not stage.exists() and not backup.exists();shutil.copytree(ready,stage);assert inv(stage)==p['after'];assert os.stat(stage).st_dev==os.stat(dest).st_dev
lib=ctypes.CDLL(None,use_errno=True);rename=lib.renameat2;rename.argtypes=[ctypes.c_int,ctypes.c_char_p,ctypes.c_int,ctypes.c_char_p,ctypes.c_uint];rename.restype=ctypes.c_int
exchanged=[]
for dest,stage,backup in stages:
 code=rename(-100,os.fsencode(stage),-100,os.fsencode(dest),2)
 if code!=0:raise OSError(ctypes.get_errno(),os.strerror(ctypes.get_errno()))
 stage.rename(backup);assert inv(dest)==p['after'] and inv(backup)==p['before'];exchanged.append(str(dest));(e/'install-progress.json').write_text(json.dumps(exchanged,indent=2))
assert inv(base/'ModSaves')==p['personal']
for name,h in p['other_packages'].items():assert inv(base/'mods'/name)==h,name
assert sha(r/'out/linux/juiced')==p['engine_sha256']
env=os.environ|dict(JUICED_SUPPORT_DIR=str(e/'postinstall-support'),LD_LIBRARY_PATH='/run/opengl-driver/lib')
x=subprocess.run([str(r/'out/linux/juiced_packages_check'),str(r.parent/'game-data'),str(base/'mods'),'--assemble'],env=env,capture_output=True,text=True,check=True,timeout=180);(e/'installed-composition.log').write_text(x.stdout+x.stderr);stage=Path(x.stdout.strip().splitlines()[-1]);assert stage.is_dir()
receipt=dict(bundle='Peugeot405 Photo Kits r19',wave2_revision=17,linux_installed=True,repository_matches_installed=True,installed_overlay=str(stage),runtime_sha256=p['sha256'],rollback=[str(x[2]) for x in stages],personal_files_identical=len(p['personal']),other_packages_unchanged=list(p['other_packages']),production_elf_unchanged=True,android_accessed=False,scope='Only405 front/rear/skirts/hood meshes and provenance/metadata. Catalogue, wings, other cars and physics unchanged.')
(e/'linux-delivery.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
