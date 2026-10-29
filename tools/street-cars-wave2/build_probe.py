"""Native stock loading probe only. Not a releasable mod or final handling calibration."""
import sys,json,argparse
from pathlib import Path
repo=Path(__file__).resolve().parents[2]
for p in [repo/'mods/street-cars/tools',repo/'mods/ferrari/tools',repo/'tools/mod_bundle']:sys.path.insert(0,str(p))
from import_cars import build
from retail import Retail
p=argparse.ArgumentParser();p.add_argument('--sources',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--game',type=Path,required=True);p.add_argument('cars',nargs='*');a=p.parse_args();r=Retail(a.game)
spec=json.loads(Path(__file__).with_name('specifications.json').read_text())['cars']
donors=['peugeot_206','fto','golf','focus','wrx2','mx5','focus'];masses=[1100,1450,1510,1398,1550,1330,1582];torques=[153,267,320,200,225,240,225];power=[102,190.4,246.7,147.5,165,153,168]
labels=['PEUGEOT 405 GLX 1.8','PEUGEOT 406 COUPE V6','GOLF R32 MK5','JETTA 2.0 FSI','SUBARU OUTBACK 2.5','VOLVO 240 TURBO','VOLVO V70 2.4']
manufacturers=['PEUGEOT EUROPE','PEUGEOT EUROPE','VOLKSWAGEN EUROPE','VOLKSWAGEN EUROPE','SUBARU JAPAN','NONE EUROPE','NONE EUROPE']
prices=[4500,16000,29000,14500,11000,7500,14000]
blocks=[];passed=[]
for i,s in enumerate(spec):
 if a.cars and s['key'] not in a.cars:continue
 c=dict(source_two_sided=True,fixed_stock_spoiler=True,kit_count=3,key=s['key'],model=s['model'],id=s['id'],label=labels[i],prefix='w2'+str(s['id']),wb=s['wheelbase_m']*100,donor=donors[i],manufacturer=manufacturers[i],price=prices[i],mass=masses[i],torque=torques[i],rpm='1000 2000 3000 4000 5000 6000 6500 7000',curve='.45 .65 .85 1 .95 .8 .60 0',paint='oj_paint',glass='oj_glass',brake='oj_tail',head='oj_head')
 try:blocks.append(build(c,a,r));passed.append(s);print('NATIVE STOCK PROBE',s['key'],flush=True)
 except Exception as e:print('FAILED',s['key'],type(e).__name__,str(e),flush=True);raise
for path,addition in [('cardefs.txt','\n'.join(blocks)),('mod_cars.txt','\n'.join(f"Car {s['model']} {s['id']} 2 { {'FWD':0,'RWD':1,'AWD':2}[s['drive']]}" for s in passed)),('mod_unlocks.txt','\n'.join('Car '+s['model'] for s in passed)),('arcadeunlock.txt','\n'.join('Car '+s['model'] for s in passed)),('arcadecollection.txt','\n'.join(f"BeginCar {s['model']}\nBaseColour 80 110 150\nEndCar" for s in passed))]:
 (a.output/'scripts').mkdir(exist_ok=True,parents=True);(a.output/'scripts'/path).write_text('\n'+addition+'\n')
(a.output/'NOT_FOR_INSTALL.txt').write_text('Stock geometry probe. Handling and customization are NOT finalized; do not install in a user profile.\n')
