"""Rebuild an isolated candidate from prepared scenes and an explicit base overlay."""
import argparse,json,shutil,subprocess,sys
from pathlib import Path

def main():
 repo=Path(__file__).resolve().parents[2]; scripts=Path(__file__).parent
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--work',type=Path,required=True)
 p.add_argument('--game',type=Path,required=True)
 p.add_argument('--base-overlay',type=Path,required=True)
 p.add_argument('--blender',default='blender')
 p.add_argument('--checker',type=Path,default=repo/'out/linux/juiced_wave2_car_check')
 a=p.parse_args();root=a.work.resolve();game=a.game.resolve();base=a.base_overlay.resolve()
 keys=[s['key'] for s in json.loads((scripts/'specifications.json').read_text())['cars']]
 required=[game,base/'scripts/cardefs.txt',base/'scripts/mod_cars.txt',a.checker]
 required += [root/(key+suffix) for key in keys for suffix in ['-prepared.blend','-prepared.glb']]
 for path in required:
  if not path.exists():p.error('Missing prerequisite: '+str(path))
 if not shutil.which(a.blender):p.error('Blender executable not found: '+a.blender)
 commands=[
  [a.blender,'-b','--python-exit-code','1','--python',str(scripts/'build_kit_study.py'),'--',str(root),*keys],
  [sys.executable,str(scripts/'build_probe.py'),'--sources',str(root),'--output',str(root/'native-stock-probe'),'--game',str(game)],
  [sys.executable,str(scripts/'calibrate_probe.py'),str(root/'native-stock-probe')],
  [sys.executable,str(scripts/'stage_probe.py'),str(base),str(root/'native-stock-probe'),str(root/'qa-stage')],
  [str(a.checker.resolve()),str(game),str(root/'qa-stage')],
 ]
 for i,cmd in enumerate(commands):
  with (root/f'candidate-step-{i}.log').open('w') as f:subprocess.run(cmd,cwd=repo,stdout=f,stderr=subprocess.STDOUT,check=True)
  print('PASS candidate step',i,flush=True)
if __name__=='__main__':main()
