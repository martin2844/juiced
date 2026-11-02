#!/usr/bin/env python3
"""Read-only audit of factory body references against retail DATs and BOH slots.

Does not certify mesh rendering or visual quality. Uses the independent Python
archive reader, not the runtime's GameFiles or CarModel implementations.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / 'mod_bundle'))
from retail import Retail


def slots(raw):
    offset = 0
    result = {}

    def read(size):
        nonlocal offset
        if offset + size > len(raw):
            raise ValueError('Truncated BOH')
        value = raw[offset:offset + size]
        offset += size
        return value

    def integer():
        return struct.unpack('<I', read(4))[0]

    def name():
        return read(32).split(b'\0', 1)[0].decode('ascii')

    def node(depth):
        if depth > 64:
            raise ValueError('BOH nesting exceeds 64')
        key = name().upper()
        read(64)  # 4x4 transform
        attachment = integer()
        if attachment == 0:
            mesh = name()
        elif attachment == 2:
            read(integer())
            mesh = None
        else:
            raise ValueError(f'Unknown attachment {attachment}')
        result[key] = mesh
        children = integer()
        if children > 4096:
            raise ValueError('Too many BOH children')
        for _ in range(children):
            node(depth + 1)

    while offset < len(raw):
        node(0)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    retail = Retail(args.game)
    definitions = retail.read('scripts/cardefs.txt')
    blocks = re.findall(r'\[BeginCar\]\s+(\S+)(.*?)\[EndCar\]',
                        definitions.decode('latin1'), re.S | re.I)
    rows, models = [], {}
    for car, body in blocks:
        parts = re.findall(r'^\s*STANDARD\s+(FBUMP|RBUMP|BONNET|SPOILER|SKIRT)\s+"?([^"\s]+)',
                           body, re.M | re.I)
        for category, mesh in parts:
            for ui in (False, True):
                folder = f'cars/{car.lower()}{"_ui" if ui else ""}'
                for ending in (['_L', '_R'] if category.upper() == 'SKIRT' else ['']):
                    stem = (mesh + ending).lower()
                    paths = [f'{folder}/mesh/{stem}.d8m', f'carmesh/{stem}.d8m', f'mesh/{stem}.d8m']
                    row = dict(car=car, category=category, ui=ui, name=stem, paths_tried=paths, found=None)
                    for path in paths:
                        try:
                            raw = retail.read(path)
                        except FileNotFoundError:
                            continue
                        row.update(found=path, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())
                        break
                    if row['found'] is None:
                        boh = f'{folder}/models/{car.lower()}_stock.boh'
                        if boh not in models:
                            raw = retail.read(boh)
                            models[boh] = dict(sha256=hashlib.sha256(raw).hexdigest(), slots=slots(raw))
                        slot = 'SSKIRT' + ending if category.upper() == 'SKIRT' else category.upper()
                        tree = models[boh]['slots']
                        row.update(boh_path=boh, slot=slot, boh_slot_exists=slot in tree, boh_mesh=tree.get(slot))
                    rows.append(row)
    missing = [row for row in rows if row['found'] is None]
    summary = dict(car_blocks=len(blocks), references=len(rows), resolved=len(rows) - len(missing),
                   missing=len(missing), missing_slot_meshes=dict(Counter(row['boh_mesh'] for row in missing)))
    report = dict(summary=summary, cardefs_sha256=hashlib.sha256(definitions).hexdigest(), models=models, rows=rows)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
