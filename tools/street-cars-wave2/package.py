"""Create an atomic native package from a complete calibrated overlay."""
import argparse,hashlib,json,shutil,sys,tempfile,zipfile
from pathlib import Path
repo=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(repo/'tools/mod_bundle'))
from retail import Retail
from package_overlay import recipe

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['source','output','game','model-sources']:p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--archive',type=Path)
 a=p.parse_args();source=a.source.resolve();dest=a.output.resolve()
 if a.archive:
  archive=a.archive.resolve()
  if archive==dest or archive.is_relative_to(dest) or archive==source or archive.is_relative_to(source):
   p.error('Archive must be outside package and source directories')
 if dest.exists():p.error('Use a fresh package output')
 if dest.is_relative_to(source) or source.is_relative_to(dest):p.error('Output must be separate from source')
 spec=json.loads(Path(__file__).with_name('specifications.json').read_text())['cars']
 donors=dict(zip([s['model'].lower() for s in spec],['peugeot_206','fto','golf','focus','wrx2','mx5','focus']))
 append={'scripts/'+n+'.txt' for n in ['cardefs','mod_cars','mod_unlocks','arcadecollection','arcadeunlock']}
 required=[source/x for x in append]+[source/'reports/handling-candidates.json']
 required += [source/'scripts'/('dynamic_'+m+'.txt') for m in donors]
 for f in required:
  if not f.is_file():p.error('Missing input: '+str(f))
 for model in donors:
  if not (source/'cars'/model/'models'/(model+'_stock.boh')).is_file():p.error('Missing model: '+model)
 provenance={}
 for s in spec:
  path=a.model_sources/s['key']/'source.json'
  if path.exists():
   raw=json.loads(path.read_text());user=raw.get('user',{});lic=raw.get('license',{})
   provenance[s['key']]={'title':raw.get('name'), 'author':user.get('displayName') or user.get('username'), 'source':raw.get('viewerUrl'), 'license':{k:lic.get(k) for k in ['label','slug','url']},'modifications':'Blender cleanup, fitting, material conversion and native customization'}
  elif s['key']=='golf-mk5-r32':
   provenance[s['key']]={'author':'PedroBLR','source':'https://drive.google.com/drive/folders/1qzJ1sOgGRwIWwoyiEDSyaQQw70MXODhi','license':'Not established for redistribution; user-supplied source'}
  else:p.error('Missing provenance: '+str(path))
 retail=Retail(a.game);dest.parent.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='.wave2-',dir=dest.parent) as tmp:
  root=Path(tmp)/dest.name;root.mkdir()
  for src in sorted(source.rglob('*')):
   if not src.is_file():continue
   rel=src.relative_to(source).as_posix()
   if not rel.startswith(('cars/','carmesh/','cartextures/','scripts/')):continue
   data=src.read_bytes()
   if rel in append:target=root/'patches'/(rel+'.patch');payload='append\ntext '+data.hex()+'\n'
   elif rel.startswith('scripts/dynamic_'):
    model=src.stem.removeprefix('dynamic_');base='scripts/dynamic_'+donors[model]+'.txt';target=root/'patches'/(rel+'.patch');payload=recipe(base,retail.read(base),data)
   else:
    target=root/'assets'/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,target);continue
   target.parent.mkdir(parents=True,exist_ok=True);target.write_text(payload)
  (root/'mod.ini').write_text('name=Street Cars Wave 2 - Seven Everyday Cars\nversion=1\nrevision=2\nkind=assets\n')
  (root/'README.md').write_text('''# Street Cars Wave 2 — candidate

Requires a current OpenJuice build supporting64cars, plus Ferrari(ID53) and
Street Cars(IDs54–56). Adds IDs57–63. Stock and three front/rear bumper and skirt
assemblies per car; conservative lips/valances, not OEM accessories.
Rear bodywork is fixed: no synthetic detachable spoiler.
Native donor engine sounds are reused. Power, mass and drivetrain target the
selected factory variants; gearing and torque-curve interpolation include
approximations. Juiced remains arcade physics. Real-device driving/performance
approval is pending. See SOURCE-CREDITS.json for third-party provenance.
No saves are included. Android requires the updated executable, not just this ZIP.
''')
  (root/'SOURCE-CREDITS.json').write_text(json.dumps(provenance,indent=2)+'\n')
  shutil.copy2(Path(__file__).with_name('specifications.json'),root/'SPECIFICATIONS.json')
  shutil.copy2(source/'reports/handling-candidates.json',root/'HANDLING.json')
  hashes={str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(root.rglob('*')) if f.is_file()}
  (root/'SHA256.json').write_text(json.dumps(hashes,indent=2)+'\n')
  root.rename(dest)
 if a.archive:
  a.archive.parent.mkdir(parents=True,exist_ok=True)
  with tempfile.NamedTemporaryFile(prefix='.wave2-',suffix='.zip',dir=a.archive.parent,delete=False) as tmp:temporary=Path(tmp.name)
  try:
   with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for f in sorted(dest.rglob('*')):
     if f.is_file():z.write(f,f.relative_to(dest.parent))
   temporary.replace(a.archive)
  finally:temporary.unlink(missing_ok=True)
 print(dest)
if __name__=='__main__':main()
