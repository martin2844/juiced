"""Create distributable package: append catalogues, derive dynamics from retail."""
import sys,shutil,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'tools/mod_bundle'))
from retail import Retail
from package_overlay import recipe
from import_cars import CONFIGS
root=Path(__file__).resolve().parents[1];overlay=Path(sys.argv[1]);retail=Retail(Path(sys.argv[2]))
append={'scripts/cardefs.txt','scripts/mod_cars.txt','scripts/mod_unlocks.txt','scripts/arcadecollection.txt','scripts/arcadeunlock.txt'}
for src in overlay.rglob('*'):
 if not src.is_file():continue
 rel=src.relative_to(overlay).as_posix();data=src.read_bytes()
 if rel in append:
  target=root/'patches'/(rel+'.patch');payload='append\ntext '+data.hex()+'\n'
 elif rel.startswith('scripts/dynamic_'):
  c=next(c for c in CONFIGS if rel=='scripts/dynamic_'+c['model'].lower()+'.txt');base='scripts/dynamic_'+c['donor']+'.txt';target=root/'patches'/(rel+'.patch');payload=recipe(base,retail.read(base),data)
 else:
  target=root/'assets'/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,target);continue
 target.parent.mkdir(parents=True,exist_ok=True);target.write_text(payload)
(root/'mod.ini').write_text('name=Street Cars - Integra DB8, BMW E36, Levin AE92\nversion=1\nrevision=1\nkind=assets\n')
