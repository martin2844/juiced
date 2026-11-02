#!/usr/bin/env python3
"""Audit BOH references to rejected meshes; not a whole-program reachability proof."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

from mod_bundle.retail import Retail


def mesh_references(raw):
    offset = 0
    refs = []

    def read(size):
        nonlocal offset
        if size > len(raw) - offset:
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
        key = name()
        read(64)
        kind = integer()
        if kind == 0:
            mesh = name()
            if mesh and mesh.lower() != 'none':
                refs.append((key, mesh.lower()))
        elif kind == 2:
            read(integer())
        else:
            raise ValueError(f'Unknown BOH attachment: {kind}')
        children = integer()
        if children > 4096:
            raise ValueError('Too many BOH children')
        for _ in range(children):
            node(depth + 1)

    while offset < len(raw):
        node(0)
    return refs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ('game', 'inventory', 'failures', 'output'):
        parser.add_argument('--' + arg, type=Path, required=True)
    args = parser.parse_args()
    retail = Retail(args.game)
    failures = json.loads(args.failures.read_text())
    rejected = {Path(item['path']).stem: item['path'] for item in failures}
    rows, trees, counts = [], {}, Counter()
    for path in args.inventory.read_text().splitlines():
        file = Path(path)
        if file.suffix != '.boh':
            continue
        raw = retail.read(path)
        trees[path] = hashlib.sha256(raw).hexdigest()
        for slot, mesh in mesh_references(raw):
            counts['all_boh_mesh_references'] += 1
            if mesh not in rejected:
                continue
            row = dict(boh=path, slot=slot, mesh=mesh, rejected=rejected[mesh])
            if file.parts[0] == 'cars':
                # CarModel::mesh stops at the first readable file, even if parsing fails.
                bases = [file.parent.parent / 'mesh', Path('carmesh'), Path('mesh')]
                row['scope'] = 'car_loader_search_order'
            else:
                # Scene3d uses caller-selected folders. Do not invent its context.
                row['scope'] = 'global_boh_context_unresolved'
                rows.append(row)
                continue
            row['selected'] = None
            for base in bases:
                candidate = str(base / (mesh + '.d8m'))
                try:
                    data = retail.read(candidate)
                except FileNotFoundError:
                    continue
                row.update(selected=candidate, sha256=hashlib.sha256(data).hexdigest(),
                           selects_rejected=candidate in rejected.values())
                break
            rows.append(row)
    counts.update(row['scope'] for row in rows)
    counts['car_references_selecting_rejected'] = sum(row.get('selects_rejected', False) for row in rows)
    counts['boh_files'] = len(trees)
    report = dict(summary=dict(counts), boh_sha256=trees, references=rows,
                  limitation='BOH references only; does not cover dynamic part selection, DLH or source callers')
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report['summary'], indent=2))


if __name__ == '__main__':
    main()
