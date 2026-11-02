from pathlib import Path
import argparse,csv,hashlib,json,os,subprocess
from PIL import Image,ImageDraw
p=argparse.ArgumentParser();p.add_argument('candidate',type=Path);p.add_argument('out',type=Path);p.add_argument('--baseline',action='store_true');p.add_argument('--full',action='store_true');a=p.parse_args();r=Path.cwd();e=a.out.resolve();e.mkdir();work=Path('/run/media/martin/Ediloco/OpenJuice-builds/20261101-peugeot405-bumper-replacement')/e.name;work.mkdir();collection=work/'mods';collection.mkdir()
base=Path.home()/'.local/share/openjuice/mods'
for f in base.iterdir():
 if f.is_dir() and (f/'mod.ini').is_file():(collection/f.name).symlink_to(a.candidate.resolve() if f.name=='street-cars-wave2' else f,target_is_directory=True)
env=os.environ|dict(JUICED_SUPPORT_DIR=str(work/'support'),SDL_VIDEODRIVER='wayland',LD_LIBRARY_PATH='/run/opengl-driver/lib')
def run(args,name):
 p=subprocess.run(list(map(str,args)),env=env,capture_output=True,text=True,timeout=300);(e/(name+'.log')).write_text(p.stdout+p.stderr);p.check_returncode();return p.stdout
stage=Path(run([r/'out/linux/juiced_packages_check',r.parent/'game-data',collection,'--assemble'],'assembly').strip().splitlines()[-1]);assert stage.is_dir()
exe=r/'out/linux/juiced_car_gallery' if a.baseline else r/'out/validation/peugeot405-bumper-replacement-20261101/isolation/slot_isolation'
if a.full:exe=r/'out/validation/peugeot405-photo-kits-20261101/preview/peugeot405_shop'
raw=work/'frames';run([exe,r.parent/'game-data',stage,raw,'--kits','--close','PEUGEOT_405_GLX'],'gallery');log=(e/'gallery.log').read_text();assert 'AMD Radeon RX 7800 XT' in log
assert 'not read' not in log.replace('mesh NONE not read','optional NONE'), 'Missing native resources'
rows=list(csv.DictReader((raw/'captures.csv').open()));styles=list(dict.fromkeys(x['style'] for x in rows));hashes={}
for style in styles:
 ss=[x for x in rows if x['style']==style];sheet=Image.new('RGB',(1600,520*((len(ss)+1)//2)),'#15171a');d=ImageDraw.Draw(sheet)
 for i,x in enumerate(ss):
  with Image.open(raw/x['file']) as im:
   dest=e/Path(x['file']).with_suffix('.png').name;im.save(dest);hashes[dest.name]=hashlib.sha256(dest.read_bytes()).hexdigest();crop=im.convert('RGB').crop((40,220,1240,940));crop.thumbnail((790,480));xx=(i%2)*800;yy=(i//2)*520;sheet.paste(crop,(xx,yy));d.text((xx+10,yy+489),style+' | yaw '+x['yaw_degrees'],fill='white')
 sheet.save(e/(style+'-contact.jpg'),quality=96)
(e/'capture.json').write_text(json.dumps(dict(stage=str(stage),raw=str(raw),frames=len(rows),styles=styles,images=hashes,limits='Actual CarView native renderer; not race driving or Android FPS validation'),indent=2)+'\n');print('Captured',len(rows),'native views:',e,flush=True)
