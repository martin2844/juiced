#!/usr/bin/env python3
"""Turn a validated overlay into a package without copying unchanged retail assets.

Catalogue recipes copy byte ranges from the owner's game and insert only changes.
Use --wheel-colours for an overlay previously assembled against another car mod.
"""
import argparse
import difflib
from pathlib import Path
import re
import shutil
import sys
from retail import Retail


def fingerprint(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f'{value:016x}'


def recipe(source, before, after):
    lines = [f'base {source} {fingerprint(before)}']
    old, new = before.splitlines(keepends=True), after.splitlines(keepends=True)
    offsets = [0]
    for line in old:
        offsets.append(offsets[-1] + len(line))
    for op, a, b, c, d in difflib.SequenceMatcher(None, old, new, autojunk=False).get_opcodes():
        if op == 'equal':
            lines.append(f'copy {offsets[a]} {offsets[b]-offsets[a]}')
        elif c != d:
            lines.append('text ' + b''.join(new[c:d]).hex())
    return '\n'.join(lines) + '\n'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game', type=Path, required=True)
    p.add_argument('--overlay', type=Path, required=True)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--wheel-colours', action='store_true')
    a = p.parse_args()
    retail = Retail(a.game)
    if (a.package / 'assets').exists() or (a.package / 'patches').exists():
        p.error('Package assets/patches already exist; generate into a fresh package')
    for file in sorted(a.overlay.rglob('*')):
        if not file.is_file() or 'saves' in [s.lower() for s in file.relative_to(a.overlay).parts]:
            continue
        relative = file.relative_to(a.overlay).as_posix()
        data = file.read_bytes()
        source = 'scripts/dynamic_falcon.txt' if relative == 'scripts/dynamic_ferrari_458.txt' else relative
        try:
            base = retail.read(source)
        except (FileNotFoundError, KeyError):
            base = None
        if data == base:
            continue
        if relative == 'scripts/cardefs.txt' and a.wheel_colours:
            pattern = rb'^\s*\[WheelColour\]\s+(\d+)\s+(\d+)\s+(\d+)'
            known = set(re.findall(pattern, base, re.M | re.I))
            extra = [c for c in re.findall(pattern, data, re.M | re.I) if c not in known]
            payload = b'\n' + b''.join(b'[WheelColour] ' + b' '.join(c) + b'\n' for c in extra)
            text = 'append\ntext ' + payload.hex() + '\n'
        elif relative.startswith('scripts/') and base is not None:
            text = recipe(source, base, data)
        else:
            target = a.package / 'assets' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, target)
            continue
        target = a.package / 'patches' / (relative + '.patch')
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)


if __name__ == '__main__':
    main()
