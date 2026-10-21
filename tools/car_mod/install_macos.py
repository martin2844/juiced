#!/usr/bin/env python3
"""Install a separate local test app using an existing self-contained Juiced bundle."""
import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--base-app',type=Path,required=True)
p.add_argument('--exe',type=Path,required=True)
p.add_argument('--mod',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
if a.output.exists():raise SystemExit('Output exists; choose a new app path.')
# Stage beside the output: failure leaves the base application untouched.
with tempfile.TemporaryDirectory(prefix='.juiced-car-',dir=a.output.parent) as staging:
    app=Path(staging)/a.output.name
    subprocess.run(['ditto',str(a.base_app),str(app)],check=True)
    contents=app/'Contents';exe=contents/'MacOS/Juiced'
    shutil.copy2(a.exe,exe)
    frameworks=contents/'Frameworks'
    for line in subprocess.check_output(['otool','-L',str(exe)],text=True).splitlines()[1:]:
        dep=line.strip().split(' (')[0]
        if dep.startswith(('/usr/lib/','/System/Library/')):continue
        library=Path(dep).name
        if not (frameworks/library).exists():raise RuntimeError('Missing bundled dependency: '+dep)
        subprocess.run(['install_name_tool','-change',dep,'@executable_path/../Frameworks/'+library,str(exe)],check=True)
    cmds=subprocess.check_output(['otool','-l',str(exe)],text=True).splitlines()
    for i,line in enumerate(cmds):
        if line.strip()=='cmd LC_RPATH':
            path=cmds[i+2].strip().split(' (')[0][5:]
            if path.startswith('/'):
                subprocess.run(['install_name_tool','-delete_rpath',path,str(exe)],check=True)
    resources=contents/'Resources';resources.mkdir(exist_ok=True)
    shutil.copytree(a.mod,resources/'CarMod',ignore=shutil.ignore_patterns('Saves','saves'))
    shutil.copy2(Path(__file__).parent/'README.md',resources/'CarMod/README.md')
    launcher=contents/'MacOS/LaunchFerrari'
    launcher.write_text('''#!/bin/sh
set -eu
car_bin_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$car_bin_dir/Juiced" --mod "$car_bin_dir/../Resources/CarMod" --saves "$HOME/Library/Application Support/Juiced Mods/Ferrari458" --screen instant_selcar "$@"
''')
    launcher.chmod(0o755)
    info=contents/'Info.plist';pl=plistlib.loads(info.read_bytes())
    pl.update(CFBundleExecutable='LaunchFerrari',CFBundleIdentifier='org.openjuice.macos.ferrari458',
              CFBundleName='Juiced Ferrari 458',CFBundleDisplayName='Juiced Ferrari 458',
              CFBundleVersion='0.3.3.1',CFBundleShortVersionString='0.3.3')
    info.write_bytes(plistlib.dumps(pl))
    subprocess.run(['codesign','--force','--sign','-',str(exe)],check=True)
    subprocess.run(['codesign','--force','--deep','--sign','-',str(app)],check=True)
    subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True)
    app.rename(a.output)
print(a.output)
