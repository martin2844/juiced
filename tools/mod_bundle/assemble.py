#!/usr/bin/env python3
"""Assemble independent Ferrari and wheel overlays without copying large binary assets."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil


def colours(text):
    return [tuple(map(int, m.split())) for m in re.findall(r'^\s*\[WheelColour\]\s+(\d+\s+\d+\s+\d+)', text, re.I | re.M)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ferrari', type=Path, required=True)
    parser.add_argument('--wheels', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output in (args.ferrari.resolve(), args.wheels.resolve()):
        parser.error('Output must be separate from source overlays')
    if output.exists() and any(output.iterdir()):
        parser.error('Output must be absent or empty')
    for root in (args.ferrari, args.wheels):
        if not (root / 'scripts/cardefs.txt').is_file():
            parser.error('Missing overlay: ' + str(root))
    report = json.loads((args.wheels / 'wheel-mod-report.json').read_text())
    if report['missing_dependencies']:
        parser.error('Wheel overlay has unresolved dependencies')
    output.mkdir(parents=True, exist_ok=True)
    linked = copied = 0
    for root in (args.ferrari, args.wheels):
        for source in root.rglob('*'):
            relative = source.relative_to(root)
            if 'saves' in [p.lower() for p in relative.parts] or source.is_dir():
                continue
            if root == args.wheels and relative.as_posix() == 'scripts/cardefs.txt':
                continue
            target = output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                target.unlink()
            # Text is copied: editing a future source catalogue cannot mutate the bundle.
            if source.suffix.lower() in ('.txt', '.json', '.pmd', '.ui', '.ini'):
                shutil.copy2(source, target)
                copied += 1
            else:
                try:
                    os.link(source, target)
                    linked += 1
                except OSError:
                    shutil.copy2(source, target)
                    copied += 1
    cardefs = (args.ferrari / 'scripts/cardefs.txt').read_text()
    known = set(colours(cardefs))
    added = []
    for colour in colours((args.wheels / 'scripts/cardefs.txt').read_text()):
        if colour not in known:
            known.add(colour)
            added.append('[WheelColour] ' + ' '.join(map(str, colour)))
    (output / 'scripts/cardefs.txt').write_text(cardefs.rstrip() + '\n\n; Eagle extra wheel colours\n' + '\n'.join(added) + '\n')
    result = dict(ferrari=str(args.ferrari.resolve()), wheels=str(args.wheels.resolve()),
                  linked_assets=linked, copied_files=copied, extra_wheel_colours=len(added),
                  note='Binary assets are hardlinked; regenerate into a fresh output to update safely.')
    (output / 'mod-bundle-report.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
