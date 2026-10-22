#!/usr/bin/env python3
"""Package an isolated mod app with hardlinked data and its own save directory."""
import argparse
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile


def link_or_copy(source, target):
    try:
        os.link(source, target)
    except OSError:
        shutil.copy2(source, target)
    return target


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base-app', type=Path, required=True)
    p.add_argument('--exe', type=Path, required=True)
    p.add_argument('--mod', type=Path, help='Legacy single overlay; omit for package-based --creative-mods')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--replace', action='store_true', help='Replace only an existing Mod Garage app after staging and verifying the new build')
    p.add_argument('--scripts', type=Path, default=Path(__file__).resolve().parents[2] / 'mods', help='Runtime Lua mods to bundle as first-launch defaults')
    p.add_argument('--creative-mods', action='store_true', help='Enable the optional creative mod UI in a supporting build')
    a = p.parse_args()
    if a.output.exists():
        existing_info = a.output / 'Contents/Info.plist'
        if not a.replace or not existing_info.is_file() or plistlib.loads(existing_info.read_bytes()).get('CFBundleIdentifier') != 'org.openjuice.macos.modgarage':
            p.error('Output exists; --replace is permitted only for an existing Mod Garage app')
    if not a.creative_mods and not a.mod:
        p.error('Use --creative-mods packages or provide --mod')
    if a.creative_mods and a.mod:
        p.error('--creative-mods loads package assets; do not also supply --mod')
    required_inputs = [a.base_app / 'Contents/Info.plist', a.exe]
    if a.mod:
        required_inputs.append(a.mod / 'scripts/cardefs.txt')
    for required in required_inputs:
        if not required.is_file():
            p.error('Missing input: ' + str(required))
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.juiced-mods-', dir=a.output.parent) as stage:
        app = Path(stage) / a.output.name
        def ignore(path, names):
            return [n for n in names if n in ('CarMod', 'RuntimeMods', '_CodeSignature')]
        shutil.copytree(a.base_app, app, symlinks=True, ignore=ignore)
        contents = app / 'Contents'
        exe = contents / 'MacOS/Juiced'
        shutil.copy2(a.exe, exe)
        for line in subprocess.check_output(['otool', '-L', str(exe)], text=True).splitlines()[1:]:
            dep = line.strip().split(' (')[0]
            if dep.startswith(('/usr/lib/', '/System/Library/')):
                continue
            library = Path(dep).name
            if not (contents / 'Frameworks' / library).exists():
                raise RuntimeError('Missing bundled dependency: ' + dep)
            subprocess.run(['install_name_tool', '-change', dep,
                            '@executable_path/../Frameworks/' + library, str(exe)], check=True)
        commands = subprocess.check_output(['otool', '-l', str(exe)], text=True).splitlines()
        for i, line in enumerate(commands):
            if line.strip() == 'cmd LC_RPATH':
                path = commands[i + 2].strip().split(' (')[0][5:]
                if path.startswith('/'):
                    subprocess.run(['install_name_tool', '-delete_rpath', path, str(exe)], check=True)
        if a.mod:
            assets = contents / 'Resources/CarMod'
            shutil.copytree(a.mod, assets, copy_function=link_or_copy,
                            ignore=shutil.ignore_patterns('Saves', 'saves'))
        launcher = contents / 'MacOS/LaunchModGarage'
        extra = ' --mods "$mod_scripts" --mod-defaults "$mod_bin_dir/../Resources/RuntimeMods"' if a.creative_mods else ' --mod "$mod_bin_dir/../Resources/CarMod"'
        setup = ''
        if a.creative_mods:
            if not a.scripts.is_dir():
                raise RuntimeError('Missing runtime scripts: ' + str(a.scripts))
            shutil.copytree(a.scripts, contents / 'Resources/RuntimeMods', copy_function=link_or_copy,
                            ignore=shutil.ignore_patterns('tools', '.cache', '__pycache__', 'disabled', 'disabled.tmp', '.DS_Store'))
            setup = 'mod_scripts="$HOME/JuicedMods/mods"\n'
        launcher.write_text('''#!/bin/sh
set -eu
mod_bin_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
''' + setup + '''exec "$mod_bin_dir/Juiced" --saves "$HOME/Library/Application Support/Juiced Mods/ModGarage" --screen menu''' + extra + ''' "$@"
''')
        launcher.chmod(0o755)
        info = contents / 'Info.plist'
        pl = plistlib.loads(info.read_bytes())
        pl.update(CFBundleExecutable='LaunchModGarage', CFBundleIdentifier='org.openjuice.macos.modgarage',
                  CFBundleName='Juiced Mod Garage', CFBundleDisplayName='Juiced Mod Garage',
                  CFBundleVersion='0.3.5.0', CFBundleShortVersionString='0.3.5')
        info.write_bytes(plistlib.dumps(pl))
        subprocess.run(['codesign', '--force', '--sign', '-', str(exe)], check=True)
        subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(app)], check=True)
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
        if a.output.exists():
            backup = Path(stage) / 'previous.app'
            a.output.rename(backup)
            try:
                app.rename(a.output)
            except BaseException:
                backup.rename(a.output)
                raise
        else:
            app.rename(a.output)
    print(a.output)


if __name__ == '__main__':
    main()
