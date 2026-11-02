"""Exercise the habitual launcher with prepared private profiles, without taking desktop focus.
The support directory must be a private copy, never the actual live support root.
"""
from pathlib import Path
import os,subprocess
from PIL import Image
r=Path.cwd();e=r/'out/validation/peugeot405-fit-finish-20261101';support=e/'launcher-support'
assert support.is_dir() and support.resolve()!=Path.home()/'.local/share/openjuice'
env=os.environ|dict(JUICED_SUPPORT_DIR=str(support),SDL_VIDEODRIVER='wayland',LD_LIBRARY_PATH='/run/opengl-driver/lib')
args=['/home/martin/.local/bin/openjuice','--test','--screen','start','--frames','720','--press','120:RETURN,310:RETURN,450:RETURN','--capture',str(e/'launcher-title-profile-menu.bmp')]
x=subprocess.run(args,env=env,capture_output=True,text=True,timeout=50);log=x.stdout+x.stderr
(e/'launcher-title-profile-menu.log').write_text(log);x.check_returncode()
Image.open(e/'launcher-title-profile-menu.bmp').convert('RGB').save(e/'launcher-title-profile-menu.png')
assert 'frontend: driver Tony loaded:' in log and 'frontend: menu' in log
print('Habitual launcher: title → private Tony profile → menu; hidden-window test flags; actual saves untouched.')
