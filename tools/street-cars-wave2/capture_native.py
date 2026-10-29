import os,subprocess,shutil,re,argparse
from pathlib import Path
from PIL import Image
parser=argparse.ArgumentParser();parser.add_argument('--cars',nargs='+',default=['PEUGEOT_405_GLX','PEUGEOT_406_COUPE','GOLF_MK5_R32','JETTA_MK5_FSI','OUTBACK_2002','VOLVO_240_TURBO','VOLVO_V70_24']);parser.add_argument('--levels',nargs='+',type=int,default=[0]);parser.add_argument('--frames',type=int,default=1600);parser.add_argument('--game',type=Path,required=True);parser.add_argument('--work',type=Path,required=True);parser.add_argument('--executable',default='out/linux/juiced');args=parser.parse_args()
base=args.work.resolve();stage=base/'qa-stage';env=dict(os.environ,DISPLAY=os.environ.get('DISPLAY',':0'),JUICED_SUPPORT_DIR=str(base/'qa-support'))
for car in args.cars:
 qa=base/('qa-'+car);qa.mkdir(exist_ok=True)
 for child in stage.iterdir():
  if child.name=='scripts':continue
  link=qa/child.name
  if link.is_symlink():link.unlink()
  if not link.exists():link.symlink_to(child,target_is_directory=child.is_dir())
 (qa/'scripts').mkdir(exist_ok=True)
 for child in (stage/'scripts').iterdir():
  link=qa/'scripts'/child.name
  if child.name=='arcadeunlock.txt':
   if link.is_symlink():link.unlink()
   link.write_text(child.read_text().replace('Car FERRARI_458','Car '+car))
  elif child.name!='arcadecollection.txt':
   if link.is_symlink():link.unlink()
   if not link.exists():link.symlink_to(child)
 for level in args.levels:
  style='stock' if level==0 else 'kit'+str(level)
  collection=qa/'scripts'/'arcadecollection.txt'
  if collection.is_symlink():collection.unlink()
  text=(stage/'scripts'/'arcadecollection.txt').read_text()
  text=re.sub(r'(?i)(BeginCar '+car+r'\b.*?)(EndCar)',lambda m:m[1]+f'BaseColour 25 80 165\nModification FBUMP {level}\nModification RBUMP {level}\nModification SSKIRT {level}\n'+m[2],text,flags=re.S)
  collection.write_text(text)
  bmp=base/(car+'-'+style+'.bmp');png=base/(car+'-'+style+'.png')
  bmp.unlink(missing_ok=True);png.unlink(missing_ok=True)
  command=[args.executable,'--game',str(args.game.resolve()),'--mod',str(qa),'--screen','instant_selcar','--frames',str(args.frames),'--capture',str(base/(car+'-'+style+'.bmp')),'--test']
  if style=='sport':command+=['--press','1200:209']
  with open(base/(car+'-'+style+'.log'),'w') as f:r=subprocess.run(command,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180,check=True)
  print(car,style,r.returncode,flush=True)
  if not bmp.is_file():raise RuntimeError('Successful process produced no screenshot: '+str(bmp))
  temporary=png.with_suffix('.tmp.png')
  with Image.open(bmp) as shot:shot.save(temporary)
  temporary.replace(png)
